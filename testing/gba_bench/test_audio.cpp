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

GBA_TEST(Audio, WantsFrameFollowsTheCushion)
{
	// The frontend runs a frame of the machine while the buffer is behind its cushion, which is
	// what makes the device the clock of the machine: the machine produces a frame only when the
	// device has played one.
	AudioBuffer buffer;
	MakeBuffer(buffer);
	buffer.Prime();

	GBA_CHECK_EQ(buffer.Queued(), buffer.TargetFrames());
	GBA_CHECK(!buffer.WantsFrame());				// exactly at the cushion: nothing is owed yet
	GBA_CHECK(!buffer.Starving());

	// The device plays a callback period: now the machine owes it a frame.
	Play(buffer, CallbackFrames);
	GBA_CHECK_EQ(buffer.Queued(), buffer.TargetFrames() - CallbackFrames);
	GBA_CHECK(buffer.WantsFrame());
	GBA_CHECK(!buffer.Starving());

	// Half a cushion short is where the frontend starts refilling with extra frames.
	Play(buffer, buffer.TargetFrames() / 2);
	GBA_CHECK(buffer.WantsFrame());
	GBA_CHECK(buffer.Starving());
}

GBA_TEST(Audio, ABurstIsHeldBackUntilTheDeviceCatchesUp)
{
	// A burst (a stalled frontend that pushes everything the core accumulated) puts the buffer
	// above its cushion: the machine is not given another frame until the device has played the
	// excess back, so a burst cannot keep turning into drops.
	AudioBuffer buffer;
	MakeBuffer(buffer);
	buffer.Prime();

	int room = buffer.LimitFrames() - buffer.Queued();
	std::vector<int16_t> burst = Ramp(1, room);
	buffer.Push(burst.data(), room);
	GBA_CHECK_EQ(buffer.Queued(), buffer.LimitFrames());

	// The burst is more than a cushion above the mark the machine waits for, and a callback period
	// takes about half a cushion back: several periods pass before a frame may run again.
	int plays = 0;

	while (plays < 8 && !buffer.WantsFrame())
	{
		Play(buffer, CallbackFrames);
		plays++;
	}

	GBA_CHECK_MSG(plays >= 2, "the machine was not held back (" + std::to_string(plays) + " plays)");
	GBA_CHECK(buffer.WantsFrame());

	// A frame is only allowed once the buffer is back below its cushion, and the machine is not
	// starving while that happens (what is being played is the burst, not an empty buffer).
	GBA_CHECK(buffer.Queued() < buffer.TargetFrames());
	GBA_CHECK(!buffer.Starving());
	GBA_CHECK_EQ((int)buffer.Underruns(), 0);
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
	// plays 32768 of them per second. The device's callback is simulated every 512 frames and the
	// frame loop runs a frame whenever the buffer is behind its cushion, exactly as the frontend
	// does. One minute of that:
	//
	//  * the buffer never runs dry (no gap);
	//  * the delay stays at the cushion, not at the core's four second queue;
	//  * the machine is driven at the *device's* rate, not at the display's: the loop here is paced
	//    at 60.00 Hz (the vsync case) and the machine still runs 59.7275 frames a second, i.e. it
	//    skips a frame every few seconds instead of mixing 0.46 % more sound than the device can
	//    play - which is what used to be thrown away sample by sample, and heard as a rattle.
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

	// The machine ran at its own rate (3583.65 frames a minute), not at the display's 3600.
	GBA_CHECK_MSG(machineFrames < DisplayFrames,
		"the machine ran " + std::to_string(machineFrames) + " frames");
	GBA_CHECK_MSG(machineFrames > DisplayFrames * 99 / 100,
		"the machine ran only " + std::to_string(machineFrames) + " frames");

	// Nothing had to be thrown away: the device is the clock, so the two rates agree, and the
	// cushion plus one frame still fits below the mark that drops audio.
	int dropped = produced - taken - buffer.Queued();
	GBA_CHECK_MSG(dropped == 0, std::to_string(dropped) + " frames were dropped");

	// Which keeps the delay at the cushion.
	GBA_CHECK(buffer.LatencyMs() < 100);
	GBA_CHECK_MSG(std::abs(buffer.Queued() - buffer.TargetFrames()) <= 2 * CallbackPeriod,
		"the buffer settled at " + std::to_string(buffer.Queued()) + " of a " +
		std::to_string(buffer.TargetFrames()) + " frame cushion");
}

GBA_TEST(Audio, AStarvedBufferIsRefilledWithExtraFrames)
{
	// A hitch (the host stalled, a frame took far too long) lets the device play on while nothing
	// is mixed: the buffer runs dry and the device hears a gap. A machine in step with the device
	// only makes up one frame's worth of audio per frame, so the frontend runs extra frames until
	// the cushion is back - that is what the catch-up loop does, and this is the rule it uses.
	AudioBuffer buffer;
	MakeBuffer(buffer);
	buffer.Prime();

	// The buffer is emptied (the stall) and its cushion is what a catch-up has to restore.
	Play(buffer, buffer.Queued());
	GBA_CHECK_EQ(buffer.Queued(), 0);
	GBA_CHECK(buffer.Starving());

	// The frontend runs frames back to back until the buffer is out of the starving zone (no
	// callback in between: the catch-up happens inside one iteration of the frame loop). Two
	// frames of 548 samples take it past half a cushion, and from there the ordinary cushion rule
	// (WantsFrame) keeps refilling it one frame at a time.
	int runs = 0;

	while (buffer.Starving() && runs < 4)
	{
		std::vector<int16_t> block = Ramp(runs * 1000, 548);
		buffer.Push(block.data(), 548);
		runs++;
	}

	GBA_CHECK_EQ(runs, 2);
	GBA_CHECK_EQ(buffer.Queued(), 2 * 548);
	GBA_CHECK(!buffer.Starving());
	GBA_CHECK(buffer.WantsFrame());
	GBA_CHECK_EQ((int)buffer.Underruns(), 0);
	GBA_CHECK_EQ((int)buffer.Drops(), 0);
}
