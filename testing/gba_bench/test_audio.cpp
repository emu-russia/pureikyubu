// Unit tests for the mixer buffer between the machine and the host's sound device
// (src/gba/gba_audio.h).
//
// The buffer is the piece the SDL frontend's sound path is built on: the frame loop *pushes* the
// samples the machine mixed and the device's audio callback *plays* them (the arrangement dmgemu
// uses in `sound.cpp`). These tests drive both ends by hand - no SDL, no threads - and check the
// rules the design is built on: nothing is lost or reordered, a period the buffer cannot fill is
// silence and not stale samples, and the delay stays bounded when the machine runs ahead of the
// device.
//
// The expectations are recomputed from those rules rather than read back from the buffer, so a
// change to the cushion or to the high water mark has to be an explicit decision here too.

#include "gba_test.h"
#include "gba_audio.h"

#include <vector>

namespace
{
	using namespace GBA;

	const int HostRate = 32768;			// the GBA's own mixing rate
	const int CallbackFrames = 512;		// what the frontend asks SDL for (16 ms at 32768 Hz)

	/// <summary>Size a buffer the way the frontend sizes it from the device's rate and period: the
	/// buffer itself is not copyable (its indices are atomics), so it is filled in place.</summary>
	void MakeBuffer(AudioBuffer& buffer, int rate = HostRate, int callbackFrames = CallbackFrames)
	{
		buffer.Reset(rate, callbackFrames);
	}

	/// <summary>`count` stereo frames whose left sample is `first + i` and whose right one is its
	/// negative: a ramp that makes every frame of the stream identifiable.</summary>
	std::vector<int16_t> Ramp(int first, int count)
	{
		std::vector<int16_t> out;
		for (int i = 0; i < count; i++)
		{
			out.push_back((int16_t)(first + i));
			out.push_back((int16_t)(-(first + i)));
		}
		return out;
	}

	/// <summary>Play `frames` frames and hand back what the device would have got. The stream is
	/// pre-filled with a value Play has to overwrite, so a buffer that is not written stands out
	/// as that value instead of as silence.</summary>
	std::vector<int16_t> Play(AudioBuffer& buffer, int frames)
	{
		std::vector<int16_t> out((size_t)frames * 2, 0x7FFF);
		buffer.Play(out.data(), frames);
		return out;
	}

	bool Silent(const std::vector<int16_t>& samples)
	{
		for (int16_t sample : samples)
			if (sample != 0)
				return false;
		return true;
	}
}

GBA_TEST(Audio, PushAndPlayRoundTrip)
{
	AudioBuffer buffer;
	MakeBuffer(buffer);
	GBA_CHECK_EQ(buffer.Queued(), 0);
	GBA_CHECK_EQ(buffer.Rate(), HostRate);

	std::vector<int16_t> pushed = Ramp(1, 1000);
	buffer.Push(pushed.data(), 1000);
	GBA_CHECK_EQ(buffer.Queued(), 1000);
	GBA_CHECK_EQ((int)buffer.Drops(), 0);

	std::vector<int16_t> played = Play(buffer, 1000);

	for (size_t i = 0; i < pushed.size(); i++)
		GBA_CHECK_MSG(played[i] == pushed[i], "sample " + std::to_string(i));

	GBA_CHECK_EQ(buffer.Queued(), 0);
	GBA_CHECK_EQ((int)buffer.Underruns(), 0);
	GBA_CHECK_EQ((int)buffer.Drops(), 0);
}

GBA_TEST(Audio, PartialPeriodIsSilence)
{
	// The device hands the callback a whole period and the whole of it is written: the frames the
	// buffer holds come first, the rest of the period is silence. The device must never be left
	// with the previous period's samples in place (which is what a callback that returns early
	// does).
	AudioBuffer buffer;
	MakeBuffer(buffer);

	std::vector<int16_t> pushed = Ramp(100, 40);
	buffer.Push(pushed.data(), 40);

	std::vector<int16_t> played = Play(buffer, 512);

	for (int i = 0; i < 40; i++)
	{
		GBA_CHECK_MSG(played[i * 2] == pushed[i * 2], "left " + std::to_string(i));
		GBA_CHECK_MSG(played[i * 2 + 1] == pushed[i * 2 + 1], "right " + std::to_string(i));
	}

	std::vector<int16_t> tail(played.begin() + 40 * 2, played.end());
	GBA_CHECK(Silent(tail));
	GBA_CHECK_EQ((int)buffer.Underruns(), 1);
	GBA_CHECK_EQ(buffer.Queued(), 0);

	// An empty buffer hands over a period of silence and counts one more gap.
	GBA_CHECK(Silent(Play(buffer, 256)));
	GBA_CHECK_EQ((int)buffer.Underruns(), 2);
}

GBA_TEST(Audio, WrapsAroundTheRing)
{
	// The buffer is a ring: pushing and playing across its end must not lose or reorder anything.
	// Twenty rounds of 1000 frames push 20000 of them through a buffer that holds 8192, so both
	// indices wrap several times, and the level creeps up by 50 frames per round - well inside the
	// cushion, so nothing is ever thrown away.
	AudioBuffer buffer;
	MakeBuffer(buffer);

	const int Chunk = 1000;
	const int Plays = 950;
	const int Rounds = 20;

	int pushed = 0;
	int played = 0;

	for (int round = 0; round < Rounds; round++)
	{
		std::vector<int16_t> block = Ramp(pushed, Chunk);
		buffer.Push(block.data(), Chunk);
		pushed += Chunk;

		std::vector<int16_t> out = Play(buffer, Plays);

		for (int i = 0; i < Plays; i++)
		{
			GBA_CHECK_MSG(out[i * 2] == (int16_t)(played), "left at " + std::to_string(played));
			GBA_CHECK_MSG(out[i * 2 + 1] == (int16_t)(-played), "right at " + std::to_string(played));
			played++;
		}
	}

	GBA_CHECK_EQ((int)buffer.Underruns(), 0);
	GBA_CHECK_EQ((int)buffer.Drops(), 0);
	GBA_CHECK_EQ(buffer.Queued(), pushed - played);
	GBA_CHECK_EQ(pushed, Chunk * Rounds);
	GBA_CHECK(pushed > 2 * buffer.Queued() + buffer.TargetFrames());	// the ring really wrapped
}

GBA_TEST(Audio, PrimeFillsTheCushion)
{
	// Priming puts silence in the buffer so that the device does not underrun while the machine is
	// still mixing its first frames.
	AudioBuffer buffer;
	MakeBuffer(buffer);
	buffer.Prime();

	GBA_CHECK_EQ(buffer.Queued(), buffer.TargetFrames());
	GBA_CHECK(buffer.TargetFrames() > 0);
	GBA_CHECK(buffer.TargetFrames() < buffer.LimitFrames());

	GBA_CHECK(Silent(Play(buffer, buffer.TargetFrames())));
	GBA_CHECK_EQ((int)buffer.Underruns(), 0);
	GBA_CHECK_EQ((int)buffer.Drops(), 0);
	GBA_CHECK_EQ(buffer.Queued(), 0);
}

GBA_TEST(Audio, BlockLongerThanTheCushionKeepsItsNewestFrames)
{
	// A frontend that was stalled hands over everything the core accumulated at once. Only the
	// newest frames of such a block fit the cushion: the delay is bounded by throwing the rest
	// away, and the counter says it happened.
	AudioBuffer buffer;
	MakeBuffer(buffer);

	int burst = buffer.LimitFrames() * 2;
	std::vector<int16_t> pushed = Ramp(1, burst);
	buffer.Push(pushed.data(), burst);

	GBA_CHECK_EQ(buffer.Queued(), buffer.TargetFrames());
	GBA_CHECK_EQ((int)buffer.Drops(), 1);

	std::vector<int16_t> played = Play(buffer, buffer.Queued());
	GBA_CHECK_EQ(buffer.Queued(), 0);
	GBA_CHECK_EQ(played[0], (int16_t)(burst - buffer.TargetFrames() + 1));
	GBA_CHECK_EQ(played[played.size() - 2], (int16_t)burst);
	GBA_CHECK_EQ(played[played.size() - 1], (int16_t)(-burst));
}

GBA_TEST(Audio, SmallCorrectionDropsOnlyTheExcess)
{
	// The machine a fraction of a percent ahead of the device (a display refresh that is not the
	// GBA's 59.7275 Hz): only what does not fit is thrown away, so the buffer settles at the level
	// the machine was stopped at instead of jumping back to the cushion - a correction of a few
	// frames, not a skip.
	AudioBuffer buffer;
	MakeBuffer(buffer);

	int cushion = buffer.TargetFrames();
	std::vector<int16_t> first = Ramp(1, cushion);
	buffer.Push(first.data(), cushion);
	GBA_CHECK_EQ(buffer.Queued(), cushion);
	GBA_CHECK_EQ((int)buffer.Drops(), 0);

	int extra = (buffer.LimitFrames() - cushion) + 100;
	std::vector<int16_t> second = Ramp(10000, extra);
	buffer.Push(second.data(), extra);

	GBA_CHECK_EQ(buffer.Queued(), buffer.LimitFrames());
	GBA_CHECK_EQ((int)buffer.Drops(), 1);

	// The oldest 100 frames of the first block went; the rest of both blocks is in the buffer, in
	// order and with the newest frame at the end.
	std::vector<int16_t> played = Play(buffer, buffer.Queued());
	GBA_CHECK_EQ(played[0], (int16_t)101);
	GBA_CHECK_EQ(played[played.size() - 2], (int16_t)(10000 + extra - 1));
}

GBA_TEST(Audio, WantsFrameFollowsTheHighWaterMark)
{
	// The frontend runs a frame of the machine only while the buffer has room for it. The cushion
	// is the normal level and it is *not* a reason to hold the machine back: the mark is above it.
	AudioBuffer buffer;
	MakeBuffer(buffer);
	buffer.Prime();

	GBA_CHECK(buffer.WantsFrame());

	int room = buffer.LimitFrames() - buffer.Queued();
	std::vector<int16_t> block = Ramp(1, room);
	buffer.Push(block.data(), room);
	GBA_CHECK_EQ(buffer.Queued(), buffer.LimitFrames());
	GBA_CHECK(!buffer.WantsFrame());

	// The device plays a callback period: the machine may run again.
	Play(buffer, CallbackFrames);
	GBA_CHECK(buffer.WantsFrame());
}

GBA_TEST(Audio, LatencyInMilliseconds)
{
	// Below the cushion nothing is thrown away, so the whole 960 frames of a 48 kHz device are
	// still there: 20 ms.
	AudioBuffer buffer;
	MakeBuffer(buffer, 48000, 512);

	std::vector<int16_t> pushed = Ramp(1, 960);
	buffer.Push(pushed.data(), 960);

	GBA_CHECK_EQ(buffer.Queued(), 960);
	GBA_CHECK_EQ(buffer.LatencyMs(), 20);
	GBA_CHECK_EQ((int)buffer.Drops(), 0);
}

GBA_TEST(Audio, KeepsTheDeviceRateAndFormat)
{
	// The frontend sizes the buffer from what SDL gave it (`have.freq` / `have.samples`), which for
	// a device that refused the request may be something else: the cushion has to follow the
	// callback period, so that one period can always be served from a full cushion.
	AudioBuffer small;
	AudioBuffer big;
	MakeBuffer(small, 8000, 512);
	MakeBuffer(big, 192000, 4096);

	GBA_CHECK_EQ(small.Rate(), 8000);
	GBA_CHECK_EQ(big.Rate(), 192000);

	// Two callback periods are the floor of the cushion, whatever the video frame is worth.
	GBA_CHECK(small.TargetFrames() >= 1024);
	GBA_CHECK(big.TargetFrames() >= 8192);

	// ... and the high water mark is above the cushion and inside the buffer.
	GBA_CHECK(small.LimitFrames() > small.TargetFrames());
	GBA_CHECK(big.LimitFrames() > big.TargetFrames());

	// A whole second pushed at once is trimmed, and the buffer is still usable.
	std::vector<int16_t> second = Ramp(1, 8000);
	small.Push(second.data(), 8000);
	GBA_CHECK(small.Queued() <= small.LimitFrames());
	GBA_CHECK_EQ((int)small.Underruns(), 0);
	GBA_CHECK(!Silent(Play(small, 256)));
}

GBA_TEST(Audio, FrameLoopAndCallbackStayInStep)
{
	// The whole sound path, without SDL: a frame of the GBA mixes 548.625 samples and the device
	// plays 32768 of them per second. The frame loop pushes one frame of audio whenever the buffer
	// has room and the device's callback is simulated every 512 frames. One minute of this: the
	// buffer must never run dry (a gap) and the delay must stay at the cushion.
	//
	// The machine is driven at 60 display frames per second here, i.e. 0.46 % faster than the
	// GBA's own 59.7275 Hz - the case of `vsync` on a 60 Hz display, and the one that used to make
	// the delay grow to the core's four second queue. What the buffer does about it is to throw
	// away the handful of samples per frame that do not fit, which is the 0.46 % the two clocks
	// disagree by: no gap, no growing delay, and the machine is not held back (the picture is
	// untouched).
	AudioBuffer buffer;
	MakeBuffer(buffer);
	buffer.Prime();

	const int DisplayFrames = 3600;				// one minute at 60 Hz
	const int CallbackPeriod = CallbackFrames;

	// The machine's own sample clock: 548.625 samples per frame (280896 system cycles at 32768 Hz
	// over 16777216 cycles per second), split into whole samples the way the mixer does it.
	int64_t machineAccum = 0;
	int64_t deviceAccum = 0;

	int produced = buffer.TargetFrames();		// the primed silence is in the buffer too
	int taken = 0;
	int machineFrames = 0;
	int callbacks = 0;

	for (int tick = 0; tick < DisplayFrames; tick++)
	{
		// The device plays a whole period whenever its own clock says one has elapsed.
		deviceAccum += HostRate;

		while (deviceAccum >= 60 * CallbackPeriod)
		{
			deviceAccum -= 60 * CallbackPeriod;

			std::vector<int16_t> out((size_t)CallbackPeriod * 2, 0x7FFF);
			taken += buffer.Play(out.data(), CallbackPeriod);
			callbacks++;
		}

		if (buffer.WantsFrame())
		{
			machineAccum += 548625;
			int frames = (int)(machineAccum / 1000);
			machineAccum -= (int64_t)frames * 1000;

			std::vector<int16_t> block = Ramp(produced, frames);
			buffer.Push(block.data(), frames);
			produced += frames;
			machineFrames++;
		}
	}

	GBA_CHECK(callbacks > 0);
	GBA_CHECK_EQ((int)buffer.Underruns(), 0);
	GBA_CHECK(buffer.Queued() <= buffer.LimitFrames());

	// The display clock did not hold the machine back: it ran every one of the 3600 frames.
	GBA_CHECK_EQ(machineFrames, DisplayFrames);

	// Every frame that was pushed was played, is still queued, or was thrown away - and what was
	// thrown away is the 0.46 % the two clocks disagree by (about nine thousand frames a minute),
	// not a chunk of audio.
	int dropped = produced - taken - buffer.Queued();
	GBA_CHECK_MSG(dropped > 0, "the two clocks disagree, something has to be dropped");
	GBA_CHECK_MSG(dropped > produced / 500,
		"dropped only " + std::to_string(dropped) + " of " + std::to_string(produced));
	GBA_CHECK_MSG(dropped < produced / 100,
		"dropped " + std::to_string(dropped) + " of " + std::to_string(produced));

	// Which keeps the delay at the cushion instead of at the core's four second queue.
	GBA_CHECK(buffer.LatencyMs() < 100);
}

GBA_TEST(Audio, AStalledMachineIsHeldBack)
{
	// The other half of the frame loop's rule: when the buffer is full because the machine ran
	// ahead (a frontend that was stalled, a device that stopped calling the callback), the machine
	// is not given another frame until the device has taken some of it - so a burst cannot turn
	// into a stream of drops.
	AudioBuffer buffer;
	MakeBuffer(buffer);
	buffer.Prime();

	// Fill the buffer to the mark, exactly as pushing a burst does.
	int room = buffer.LimitFrames() - buffer.Queued();
	std::vector<int16_t> burst = Ramp(1, room);
	buffer.Push(burst.data(), room);

	// Four display frames: the first one is refused (the buffer is at the mark), and after that the
	// machine runs once per callback period - it is the device that sets the pace until the buffer
	// is back to its cushion.
	int frames = 0;

	for (int tick = 0; tick < 4; tick++)
	{
		if (buffer.WantsFrame())
			frames++;

		Play(buffer, CallbackFrames);
	}

	GBA_CHECK_EQ(frames, 3);
	GBA_CHECK_EQ((int)buffer.Underruns(), 0);

	// Once drained it is free again.
	for (int tick = 0; tick < 20; tick++)
		Play(buffer, CallbackFrames);

	GBA_CHECK(buffer.WantsFrame());
}
