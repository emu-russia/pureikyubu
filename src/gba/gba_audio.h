// The mixer buffer between the emulated machine and the host's sound device (see gba_apu.h for
// the mixer that fills it).
//
// The machine's mixer *pushes* every sample it produces into this buffer (Push, from the frame
// loop) and the host's sound device *pulls* them back from its own audio thread (Play, called from
// the SDL audio callback). This is the arrangement dmgemu uses (`sound.cpp`: the APU's
// `pop_sample` and the `Mixer` callback SDL calls) and it replaces the "accumulate the samples in
// the core, poll it once per frame and hand them to SDL_QueueAudio" model this frontend started
// with. Two things the polling model got wrong:
//
//  * the core's own queue was only drained when the frontend asked for samples, so any moment in
//    which the machine outran the sound device - a vsync that is not the GBA's 59.7275 Hz, fast
//    forward, a stalled window - left the audio behind by as much as the core's queue is deep
//    (four seconds) and it stayed behind, because nothing ever skips what has already been mixed;
//  * the device queue was allowed to run down to a fraction of one video frame and was refilled
//    once per frame, so a frame that was a little late underran the device and the picture of the
//    sound was a click.
//
// What this buffer does instead:
//
//  * it is kept at a *cushion* of audio (see TargetFrames): the machine is asked to wait once the
//    buffer is a video frame past it (WantsFrame), and audio that does not fit is dropped instead
//    of letting the delay of the sound grow. A block longer than the cushion - a frontend that was
//    stalled and then pushes everything the core accumulated - is trimmed to its newest frames;
//  * Play always fills the whole period the device hands to the callback: the samples that are
//    there, then silence. A late frame is therefore a short gap in the right place and not a
//    repeat of stale samples, and the callback never leaves the device's buffer untouched.
//
// The producer is the frame loop (one thread) and the consumer is the audio callback (one thread),
// so the ring is the single-producer/single-consumer kind: the two indices are free running
// counters, masked into the buffer, and are published with acquire/release ordering. No lock is
// taken on the audio thread.

#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>

namespace GBA
{
	/// <summary>
	/// A ring buffer of interleaved 16-bit stereo frames, pushed by the emulator and played by the
	/// host's audio callback.
	/// </summary>
	class AudioBuffer
	{
	public:
		/// <summary>
		/// Size the buffer for the device: `rate` is its sample rate and `callbackFrames` the
		/// number of frames one call of the callback is given (SDL's `have.samples`), which is how
		/// long the buffer has to cover on its own.
		/// </summary>
		void Reset(int rate, int callbackFrames)
		{
			this->rate = (rate > 0) ? rate : 1;

			// A quarter of a second, rounded up to a power of two so the wrap is a mask. The
			// cushion is placed well inside it.
			capacity = 1;
			while (capacity < this->rate / 4)
				capacity <<= 1;

			// The cushion: three video frames (the GBA's frame is 59.7275 Hz, i.e. 16.74 ms).
			// One frame is being played while the next one is mixed; the other two cover a frame
			// that is late and a callback that is late. Two callback periods is the floor, so a
			// period can always be served from a full cushion.
			target = this->rate * 3 / 60;
			if (callbackFrames * 2 > target)
				target = callbackFrames * 2;
			if (target > capacity / 2)
				target = capacity / 2;

			// One video frame above the cushion is where the machine is asked to wait. That
			// hysteresis is what keeps the rule from being asked again for every single frame,
			// while a machine that is ahead of the device only loses the few samples per frame
			// that the two clocks disagree by.
			limit = target + this->rate / 60;
			if (limit >= capacity)
				limit = capacity - 1;
			if (limit <= target)
				limit = target + 1;

			buffer.assign((size_t)capacity * 2, 0);
			write.store(0, std::memory_order_relaxed);
			read.store(0, std::memory_order_relaxed);
			underruns.store(0, std::memory_order_relaxed);
			drops.store(0, std::memory_order_relaxed);
		}

		/// <summary>
		/// Fill the cushion with silence: call this once before the device is started so that the
		/// first frames of the machine do not underrun it while they are being mixed.
		/// </summary>
		void Prime()
		{
			// The cushion is a few thousand frames at most, so a block of silence is cheaper to
			// build here than to special case the ring.
			std::vector<int16_t> silence((size_t)target * 2, 0);
			Push(silence.data(), target);
		}

		/// <summary>Push `count` interleaved stereo frames (the frame loop, one thread).</summary>
		void Push(const int16_t* frames, int count)
		{
			if (capacity == 0 || frames == nullptr || count <= 0)
				return;

			// A block longer than the cushion (a frontend that was stalled and then pushes
			// everything the core accumulated) is trimmed to its newest frames before anything is
			// copied: the delay cannot be bounded otherwise.
			bool thrown = false;

			if (count > target)
			{
				frames += (size_t)(count - target) * 2;
				count = target;
				thrown = true;
			}

			uint64_t w = write.load(std::memory_order_relaxed);
			uint64_t r = read.load(std::memory_order_acquire);
			int queued = (int)(w - r);

			// The machine is ahead of the device: throw the oldest audio away rather than let the
			// delay grow. Only the part that does not fit is dropped, so the buffer settles at the
			// level the machine was stopped at instead of jumping back to the cushion (which would
			// be an audible skip). `count` is never larger than the cushion here, so the read
			// index cannot pass the write index.
			if (queued + count > limit)
			{
				uint64_t newRead = r + (uint64_t)(queued + count - limit);

				// The callback may have played part of the buffer while this was worked out; the
				// read index is only ever moved forward, and if the device has already passed the
				// frame the drop meant to remove, there is nothing left to drop.
				uint64_t expected = r;

				while (!read.compare_exchange_weak(expected, newRead, std::memory_order_release))
				{
					if (expected >= newRead)
						break;
				}

				thrown = true;
			}

			if (thrown)
			{
				drops.fetch_add(1, std::memory_order_relaxed);
			}

			for (int i = 0; i < count; i++)
			{
				size_t slot = (size_t)((w + (uint64_t)i) & (uint64_t)(capacity - 1)) * 2;
				buffer[slot] = frames[i * 2];
				buffer[slot + 1] = frames[i * 2 + 1];
			}

			write.store(w + (uint64_t)count, std::memory_order_release);
		}

		/// <summary>True while the buffer has room for another frame of the machine.</summary>
		bool WantsFrame() const { return (capacity == 0) || (Queued() < limit); }

		/// <summary>How many frames are waiting for the audio callback.</summary>
		int Queued() const
		{
			uint64_t w = write.load(std::memory_order_acquire);
			uint64_t r = read.load(std::memory_order_acquire);
			return (w > r) ? (int)(w - r) : 0;
		}

		/// <summary>The delay the buffer adds, in milliseconds.</summary>
		int LatencyMs() const { return (rate > 0) ? (int)((int64_t)Queued() * 1000 / rate) : 0; }

		/// <summary>The cushion the buffer is kept at, in frames (the level Prime fills it to and
		/// the level the tests measure against).</summary>
		int TargetFrames() const { return target; }

		/// <summary>The high water mark, in frames: where the machine is asked to wait.</summary>
		int LimitFrames() const { return limit; }

		/// <summary>The rate the buffer was sized for.</summary>
		int Rate() const { return rate; }

		/// <summary>How many callback periods the buffer could not fill (the machine is behind).</summary>
		uint32_t Underruns() const { return underruns.load(std::memory_order_relaxed); }

		/// <summary>
		/// How many pushes had to throw audio away because the machine was ahead of the device.
		/// A display refresh that is not the GBA's 59.7275 Hz makes this happen for every frame,
		/// and what is thrown away is then the few samples the two clocks disagree by; a stalled
		/// frontend makes it happen once, with everything that does not fit the cushion.
		/// </summary>
		uint32_t Drops() const { return drops.load(std::memory_order_relaxed); }

		/// <summary>
		/// Play up to `count` frames into the caller's stream (the audio thread). The whole
		/// `count` is written: the frames the buffer holds, then silence if it did not have
		/// enough. Returns the number of frames that came from the buffer.
		/// </summary>
		int Play(int16_t* out, int count)
		{
			if (out == nullptr || count <= 0)
				return 0;

			if (capacity == 0)
			{
				memset(out, 0, (size_t)count * 2 * sizeof(int16_t));
				return 0;
			}

			uint64_t r = read.load(std::memory_order_relaxed);
			uint64_t w = write.load(std::memory_order_acquire);

			int available = (w > r) ? (int)(w - r) : 0;
			if (available > count)
				available = count;

			for (int i = 0; i < available; i++)
			{
				size_t slot = (size_t)((r + (uint64_t)i) & (uint64_t)(capacity - 1)) * 2;
				out[i * 2] = buffer[slot];
				out[i * 2 + 1] = buffer[slot + 1];
			}

			// The producer may have thrown the oldest samples away while this period was being
			// copied; its index is the newer one and must not be moved back.
			uint64_t expected = r;
			read.compare_exchange_strong(expected, r + (uint64_t)available, std::memory_order_release);

			if (available < count)
			{
				memset(out + (size_t)available * 2, 0,
					(size_t)(count - available) * 2 * sizeof(int16_t));
				underruns.fetch_add(1, std::memory_order_relaxed);
			}

			return available;
		}

	private:
		std::vector<int16_t> buffer;	// `capacity` interleaved stereo frames
		int capacity = 0;
		int rate = 0;
		int target = 0;
		int limit = 0;

		// The producer writes at `write`, the consumer reads at `read`; both are free running
		// frame counters that are masked into the buffer.
		std::atomic<uint64_t> write{ 0 };
		std::atomic<uint64_t> read{ 0 };
		std::atomic<uint32_t> underruns{ 0 };
		std::atomic<uint32_t> drops{ 0 };
	};
}
