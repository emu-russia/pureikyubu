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
//  * the machine is run by its own clock - the frame loop's pace, i.e. the display's refresh when
//    vsync is on and a wall clock schedule when it is not - and the *mixer* absorbs the difference
//    between that clock and the sound device's: the buffer is played back at a slightly different
//    rate (Play interpolates by Step, which UpdateClock steers from the buffer's level), so a
//    fraction of a percent of clock difference costs neither a growing delay nor audible drops,
//    and the machine's own speed never follows the sound device. The steering has to be a slow,
//    filtered controller and not a per frame nudge - see ClockCorrection for why that distinction
//    is the difference between a steady rate and a rattle;
//  * it is kept at a *cushion* of audio (see TargetFrames), which is what the correction above
//    steers to and what covers a frame or a callback that is late;
//  * a buffer that has run dry is refilled with extra frames in the same iteration (Starving),
//    because one frame only makes up for one frame's worth of audio: without that a stall of the
//    host - the device plays on while nothing is mixed - would leave the device clicking for
//    seconds while the buffer creeps back;
//  * the delay stays bounded: audio that still does not fit (a frontend that pushed a whole queue
//    at once, a device whose clock is wildly different) is dropped, and a block longer than the
//    cushion is trimmed to its newest frames;
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

			// Two video frames above the cushion is where the buffer starts throwing audio away. It
			// is a safety net, not the normal way of keeping the delay down: the correction above
			// leaves the buffer at the cushion, and one more frame on top of it (the frame that is
			// being mixed while the device plays) still fits - so nothing at all is dropped while
			// the machine and the device are in step, and only a burst (a stalled frontend that
			// pushes everything the core accumulated at once) reaches this mark.
			limit = target + this->rate / 30;
			if (limit >= capacity)
				limit = capacity - 1;
			if (limit <= target)
				limit = target + 1;

			buffer.assign((size_t)capacity * 2, 0);
			write.store(0, std::memory_order_relaxed);
			read.store(0, std::memory_order_relaxed);
			underruns.store(0, std::memory_order_relaxed);
			drops.store(0, std::memory_order_relaxed);
			stepMicro.store(Micro, std::memory_order_relaxed);
			playPosition = 0.0;
			correction.Reset(target, callbackFrames);
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

		/// <summary>
		/// True while the buffer is so short that the device is about to run dry: the frontend runs
		/// extra frames in the same iteration to refill the cushion. A machine in step with the
		/// device only produces one frame's worth of audio per frame, so without this a hitch (the
		/// host stalled, a frame took far too long) would be heard as clicks for seconds while the
		/// buffer creeps back a couple of samples at a time.
		/// </summary>
		bool Starving() const { return (capacity != 0) && (Queued() < target / 2); }

		/// <summary>
		/// The frontend's clock correction, called once per frame: the machine is run by the wall
		/// clock (and the display), which never matches the sound device's own clock exactly, so
		/// instead of throwing samples away the mixer plays the buffer at a slightly different rate
		/// (Play interpolates by Step). The correction is a slow PI controller on the buffer's
		/// level: the integral holds the level at the cushion whatever the two clocks disagree by
		/// (a 60.00 Hz display against the GBA's 59.7275 is 0.46 %, i.e. 8 cents), and the
		/// proportional part damps it.
		///
		/// Both parts work on a *filtered* level, and that is the whole point of this class. The
		/// device plays a whole callback period in one go (512 frames here) while the machine
		/// pushes one frame's worth of samples per frame, so the level the frame loop reads saws up
		/// and down by a callback period at the beat of the two clocks - a few hertz. The obvious
		/// one-liner, `step += (level - cushion) / 4` once a frame, is an undamped integrator
		/// answering exactly that sawtooth: its gain is far beyond what sampling once per frame
		/// allows (its period works out at half a frame), so it swings the full +/-1 % between one
		/// frame and the next and its *average* is whatever the sawtooth's phase leaves behind.
		/// That is heard as a rattle - the playback rate is noise rather than a rate - and when the
		/// average lands on the wrong side the level also climbs into the mark that throws audio
		/// away, so the machine's own samples are dropped a few at a time as well. Hence the filter
		/// and hence gains this small: this loop is meant to take seconds to settle, which is all
		/// the fraction of a percent a real clock difference ever needs.
		/// </summary>
		int UpdateClock()
		{
			if (capacity == 0)
				return Micro;

			int step = correction.Update(Queued(), target);
			stepMicro.store(step, std::memory_order_relaxed);
			return step;
		}

		/// <summary>
		/// How the buffer is being played back, in millionths of a frame per frame: 1000000 is one
		/// for one (see UpdateClock).
		/// </summary>
		int Step() const { return stepMicro.load(std::memory_order_relaxed); }

		/// <summary>The same, in thousandths (1000 is one for one): what the window title shows to
		/// say how far the sound device's clock is from the machine's.</summary>
		int RatePermille() const { return stepMicro.load(std::memory_order_relaxed) / 1000; }

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
		/// How many pushes had to throw audio away. Nothing is thrown away while the machine's
		/// clock and the device's agree - what they disagree by is absorbed by the mixer's rate
		/// (see UpdateClock) - so this counts the resyncs a stalled frontend or a long burst
		/// causes.
		/// </summary>
		uint32_t Drops() const { return drops.load(std::memory_order_relaxed); }

		/// <summary>
		/// Play `count` frames into the caller's stream (the audio thread): the frames the buffer
		/// holds, resampled by Step, then silence if it did not have enough. Returns the number of
		/// frames that came from the buffer.
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

			// The position of the next frame to play, as a whole frame plus the fraction that is
			// still owed to the previous one (the resampler's own state, touched by the callback
			// only).
			double step = (double)Step() / (double)Micro;
			double position = playPosition;
			int produced = 0;

			while (produced < count && position < (double)available)
			{
				uint64_t index = (uint64_t)position;
				double fraction = position - (double)index;

				// The frame after this one may not have been pushed yet: at the end of the buffer
				// the last frame is held instead of counting a gap.
				uint64_t next = (index + 1 < (uint64_t)available) ? index + 1 : index;

				size_t slot = (size_t)((r + index) & (uint64_t)(capacity - 1)) * 2;
				size_t slotNext = (size_t)((r + next) & (uint64_t)(capacity - 1)) * 2;

				out[produced * 2] = (int16_t)(buffer[slot] +
					(int)((buffer[slotNext] - buffer[slot]) * fraction));
				out[produced * 2 + 1] = (int16_t)(buffer[slot + 1] +
					(int)((buffer[slotNext + 1] - buffer[slot + 1]) * fraction));

				produced++;
				position += step;
			}

			// Everything the resampler has walked past is the callback's to consume; the fraction
			// stays here for the next period. A whole frame is the most that can be committed, so
			// the interpolation above can never read past what the producer published.
			int consumed = (int)position;
			playPosition = position - (double)consumed;

			if (consumed > available)
				consumed = available;

			// The producer may have thrown the oldest samples away while this period was being
			// copied; its index is the newer one and must not be moved back.
			uint64_t expected = r;
			read.compare_exchange_strong(expected, r + (uint64_t)consumed, std::memory_order_release);

			if (produced < count)
			{
				memset(out + (size_t)produced * 2, 0,
					(size_t)(count - produced) * 2 * sizeof(int16_t));
				underruns.fetch_add(1, std::memory_order_relaxed);
			}

			return produced;
		}

	private:
		// One frame, in the millionths Step and UpdateClock work in.
		static const int Micro = 1000000;

		/// <summary>
		/// The clock correction's controller: a slow, filtered PI on the buffer's level (see
		/// UpdateClock for why it has to be filtered and why it has to be slow).
		/// </summary>
		struct ClockCorrection
		{
			// The gains, per frame of the machine - UpdateClock runs once per frame, about every
			// 16.7 ms. The proportional gain's own time constant is 1/(546 * Proportional) frames,
			// i.e. about a second; the integral's is 1/sqrt(Integral * 546), about ten. Those are
			// the numbers the two clocks' disagreement needs and no faster.
			double Proportional = 0.00003;	// rate per frame, per frame of level error
			double Integral = 0.0000002;	// rate per frame, per frame of level error, per frame
			double Smoothing = 64.0;		// frames of the level's moving average

			// Where the rate is held. A sound device whose clock is this far from the machine's is
			// broken rather than different, and bending the pitch to follow it would be the wrong
			// answer anyway; a *display* that far from the machine's own rate (50 Hz against
			// 59.73) is the frame loop's business, not the mixer's.
			double Maximum = 0.01;

			double level = 0.0;		// the filtered level, in frames
			double rate = 0.0;		// the integral's state: the rate, as a fraction of one

			void Reset(int target, int callbackFrames)
			{
				level = (double)target;
				rate = 0.0;

				// The filter has to be long against the sawtooth it is there to remove, and the
				// sawtooth is a callback period long: eight periods, and never less than the
				// second it takes for the loop to be slow enough for its own sampling.
				int periods = callbackFrames / 8;

				Smoothing = (periods > 64) ? (double)periods : 64.0;
			}

			/// <summary>
			/// Take one frame's reading of the level and return the rate to play at, in millionths
			/// of a frame per frame (Micro is one for one).
			/// </summary>
			int Update(int queued, int target)
			{
				level += ((double)queued - level) / Smoothing;

				double error = level - (double)target;

				rate += Integral * error;

				if (rate > Maximum)
					rate = Maximum;
				else if (rate < -Maximum)
					rate = -Maximum;

				double correction = rate + Proportional * error;

				if (correction > Maximum)
					correction = Maximum;
				else if (correction < -Maximum)
					correction = -Maximum;

				// The value is a millionth of a frame either way, so the rounding is what keeps an
				// exact limit (the +/-1 % clamp, or one for one) from coming back a unit short.
				return (int)((1.0 + correction) * (double)Micro + 0.5);
			}
		};

		// `capacity` interleaved stereo frames
		std::vector<int16_t> buffer;
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

		// How fast the buffer is played back (the frontend's clock correction) and where the
		// resampler stands inside the current frame.
		std::atomic<int> stepMicro{ Micro };
		double playPosition = 0.0;
		ClockCorrection correction;
	};
}
