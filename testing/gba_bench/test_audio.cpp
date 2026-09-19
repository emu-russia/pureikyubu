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

	/// <summary>The same idea, but a sawtooth instead of a ramp: the left sample rises by 64 every
	/// frame and drops back every 512, so a long run stays inside 16 bits while every frame is
	/// still identifiable and its neighbours are a known 64 apart.</summary>
	std::vector<int16_t> Saw(int first, int count)
	{
		std::vector<int16_t> out;
		for (int i = 0; i < count; i++)
		{
			int value = ((first + i) % 512) - 256;
			out.push_back((int16_t)(value * 64));
			out.push_back((int16_t)(-value * 64));
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

GBA_TEST(Audio, ClockCorrectionSteersToTheCushion)
{
	// The machine's clock (the frame loop) and the device's never agree exactly, so the mixer plays
	// the buffer at a slightly different rate: the correction is the integral of the level's error,
	// which drives the level back to the cushion whichever way the two clocks disagree.
	AudioBuffer buffer;
	MakeBuffer(buffer);

	// Prime fills the cushion exactly: nothing to correct yet.
	buffer.Prime();
	GBA_CHECK_EQ(buffer.RatePermille(), 1000);
	GBA_CHECK_EQ(buffer.UpdateClock(), 1000000);

	// More audio than the cushion: the mixer has to play it back faster to drain it.
	buffer.Push(Ramp(1, 2000).data(), 2000);
	int raised = buffer.UpdateClock();
	GBA_CHECK_MSG(raised > 1000000, "the correction did not rise: " + std::to_string(raised));

	// ... and less than the cushion: slower, to let it fill up again.
	AudioBuffer empty;
	MakeBuffer(empty);
	int lowered = empty.UpdateClock();
	GBA_CHECK_MSG(lowered < 1000000, "the correction did not fall: " + std::to_string(lowered));

	// The correction stays inside one percent: a larger disagreement is a broken device rather
	// than two clocks, and the buffer's own drop rule is what handles that.
	AudioBuffer high;
	MakeBuffer(high);
	for (int i = 0; i < 1000; i++)
		high.UpdateClock();
	GBA_CHECK(high.RatePermille() <= 1010);
	GBA_CHECK(high.RatePermille() >= 990);

	AudioBuffer low;
	MakeBuffer(low);
	for (int i = 0; i < 1000; i++)
		low.UpdateClock();
	GBA_CHECK(low.RatePermille() <= 1010);
	GBA_CHECK(low.RatePermille() >= 990);
}

GBA_TEST(Audio, PlaysAtTheCorrectionRate)
{
	// A correction below one plays the buffer slightly slower: with a one percent correction a
	// thousand frames last 1010 output frames, and what falls between two frames is interpolated.
	AudioBuffer buffer;
	MakeBuffer(buffer);

	std::vector<int16_t> pushed = Ramp(0, 1000);
	buffer.Push(pushed.data(), 1000);

	while (buffer.Step() > 990000)
		buffer.UpdateClock();

	GBA_CHECK_EQ(buffer.RatePermille(), 990);

	// A thousand frames at ninety-nine hundredths of a frame each come to 1010.1 output frames,
	// and the last one still starts inside the buffer (at 999.9), so it is played too: 1011 - and
	// asking for exactly those fills the period with the buffer's own samples and no silence.
	std::vector<int16_t> out(1011 * 2, 0x7FFF);
	int produced = buffer.Play(out.data(), 1011);

	GBA_CHECK_EQ(produced, 1011);
	GBA_CHECK_EQ(buffer.Queued(), 0);
	GBA_CHECK_EQ((int)buffer.Underruns(), 0);

	// The output is the buffer read at ninety-nine hundredths of a frame per output frame and
	// interpolated, so output frame n is the buffer at 0.99n: the first two land inside the first
	// frame, frame 9 a little past the start of frame 8, and frame 10 at the start of frame 9.
	GBA_CHECK_EQ((int)out[0], 0);
	GBA_CHECK_EQ((int)out[2], 0);
	GBA_CHECK_EQ((int)out[18], 8);
	GBA_CHECK_EQ((int)out[20], 9);

	// A correction of one for one is an exact copy, which is what every other test relies on.
	AudioBuffer exact;
	MakeBuffer(exact);
	std::vector<int16_t> block = Ramp(7, 32);
	exact.Push(block.data(), 32);
	std::vector<int16_t> copy = Play(exact, 32);
	for (size_t i = 0; i < block.size(); i++)
		GBA_CHECK_EQ((int)copy[i], (int)block[i]);
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
	// The whole sound path, without SDL: a frame of the GBA mixes 548.625 samples, the device plays
	// 32768 of them a second, its callback takes 512 at a time, and the frame loop runs one frame
	// per iteration and steers the mixer's rate from the buffer's level, exactly as the frontend
	// does. The loop here is paced at 60.00 Hz - a display whose refresh is what the frontend ends
	// up following - so the machine produces 60 * 548.625 = 32917.5 samples a second while the
	// device takes 32768: the mixer has to play the buffer 0.46 % fast to keep up. One minute of
	// that has to be, from the device's side:
	//
	//  * continuous: every period is filled from the buffer (no gap), and the samples are the
	//    machine's, all of them, in order, each one played once - nothing repeated and nothing
	//    missed (the rattle was exactly that: the rate swinging every frame and the level then
	//    throwing the excess away a few samples at a time);
	//  * a rate and not noise: the correction may drift, but it must not jerk about - the old
	//    "step += (level - cushion) / 4" swung the full two percent between neighbouring frames;
	//  * settled: the delay at the cushion and the rate at the 0.46 % the two clocks disagree by.
	AudioBuffer buffer;
	MakeBuffer(buffer);
	buffer.Prime();

	const int DisplayFrames = 3600;				// one minute at 60 Hz
	const int CallbackPeriod = CallbackFrames;

	// The machine's own sample clock: 548.625 samples per frame (280896 system cycles at 32768 Hz
	// over 16777216 cycles per second), split into whole samples the way the mixer does it.
	int64_t machineAccum = 0;
	int64_t deviceAccum = 0;

	int pushed = buffer.TargetFrames();			// the primed silence is in the buffer too
	int machineFrames = 0;
	int callbacks = 0;
	int lowest = buffer.Queued();				// the level never goes below this
	int highest = buffer.Queued();
	int worstStep = 0;							// the largest frame to frame change of the rate
	int repeats = 0;							// output frames that played the previous one again
	int lateRepeats = 0;						// ... once the correction had settled
	int skips = 0;								// output frames that missed one
	int previous = 0;
	int previousStep = buffer.Step();

	for (int tick = 0; tick < DisplayFrames; tick++)
	{
		// The device plays a whole period whenever its own clock says one has elapsed.
		deviceAccum += HostRate;

		while (deviceAccum >= 60 * CallbackPeriod)
		{
			deviceAccum -= 60 * CallbackPeriod;

			std::vector<int16_t> out((size_t)CallbackPeriod * 2, 0x7FFF);
			buffer.Play(out.data(), CallbackPeriod);
			callbacks++;

			// The pushed samples are a sawtooth that rises by 64 every frame (and drops back
			// every 512), so a period of it can be read back frame by frame: what the device
			// should hear rises by 64, give or take the correction, and a frame that did not
			// arrive shows up as a jump of 128. A *repeat* is the resampler playing slower than
			// the machine mixed - legitimate while the level is under the cushion - but once the
			// loop has settled the rate is above one for good and there should be none of either.
			if (callbacks > 1)
			{
				for (int i = 0; i < CallbackPeriod; i++)
				{
					int value = out[(size_t)i * 2];
					int difference = value - previous;
					previous = value;

					if (difference == 0)
					{
						repeats++;

						if (tick >= DisplayFrames / 2)
							lateRepeats++;
					}
					else if (difference > 66 && difference < 32000)
					{
						skips++;
					}
				}
			}
			else
			{
				previous = out[0];
			}
		}

		// The frame loop: one frame of the machine, then the clock correction.
		machineAccum += 548625;
		int frames = (int)(machineAccum / 1000);
		machineAccum -= (int64_t)frames * 1000;

		std::vector<int16_t> block = Saw(pushed, frames);
		buffer.Push(block.data(), frames);
		pushed += frames;
		machineFrames++;

		int step = buffer.UpdateClock();

		if (std::abs(step - previousStep) > worstStep)
			worstStep = std::abs(step - previousStep);

		previousStep = step;

		if (buffer.Queued() < lowest)
			lowest = buffer.Queued();

		if (buffer.Queued() > highest)
			highest = buffer.Queued();
	}

	GBA_CHECK(callbacks > 0);
	GBA_CHECK_EQ(machineFrames, DisplayFrames);

	// Continuous: nothing was thrown away, the device was never left with a period to fill from
	// nothing, and the machine's own samples reached it one at a time, in order - no frame of the
	// sawtooth arrived twice after the loop had settled, and none was missed anywhere.
	GBA_CHECK_EQ((int)buffer.Drops(), 0);
	GBA_CHECK_EQ((int)buffer.Underruns(), 0);

	GBA_CHECK_MSG(skips == 0, std::to_string(skips) + " frames were missed");
	GBA_CHECK_MSG(lateRepeats == 0,
		std::to_string(lateRepeats) + " of " + std::to_string(repeats) +
		" repeated frames came after the correction had settled");

	// The machine ran once per iteration: its speed is the loop's, not the device's clock.
	GBA_CHECK_EQ(machineFrames, DisplayFrames);

	// A rate and not noise: the correction moved by at most a thousandth of a percent between two
	// neighbouring frames (242 was the worst of this run, against 20001 for the old rule).
	GBA_CHECK_MSG(worstStep <= 1000,
		"the playback rate jumped " + std::to_string(worstStep) + " millionths in one frame");

	// Settled: the correction found the 0.46 % the two clocks disagree by, the level is back at
	// the cushion, and the delay is the cushion's business, not the core queue's.
	GBA_CHECK_MSG(std::abs(buffer.RatePermille() - 1005) <= 2,
		"the correction settled at " + std::to_string(buffer.RatePermille()) + " permille");

	GBA_CHECK_MSG(std::abs(buffer.Queued() - buffer.TargetFrames()) <= 2 * CallbackPeriod,
		"the buffer settled at " + std::to_string(buffer.Queued()) + " of a " +
		std::to_string(buffer.TargetFrames()) + " frame cushion");

	// Which it never left far enough to run dry or to reach the mark that drops audio.
	GBA_CHECK_MSG(lowest > buffer.TargetFrames() / 2,
		"the level fell to " + std::to_string(lowest));

	GBA_CHECK_MSG(highest < buffer.LimitFrames(),
		"the level reached " + std::to_string(highest));

	GBA_CHECK(buffer.LatencyMs() < 100);
}

GBA_TEST(Audio, AStarvedBufferIsRefilledWithExtraFrames)
{
	// A hitch (the host stalled, a frame took far too long) lets the device play on while nothing
	// is mixed: the buffer runs dry and the device hears a gap. A machine in step with the device
	// only makes up one frame's worth of audio per frame - the mixer's rate correction is a
	// fraction of a percent, not a way to catch up on seconds - so the frontend runs extra frames
	// until the cushion is back. That is what the catch-up loop does, and this is the rule it uses.
	AudioBuffer buffer;
	MakeBuffer(buffer);
	buffer.Prime();

	// The buffer is emptied (the stall) and its cushion is what a catch-up has to restore.
	Play(buffer, buffer.Queued());
	GBA_CHECK_EQ(buffer.Queued(), 0);
	GBA_CHECK(buffer.Starving());

	// The frontend runs frames back to back until the buffer is out of the starving zone (no
	// callback in between: the catch-up happens inside one iteration of the frame loop). Two
	// frames of 548 samples take it past half a cushion (819 frames), and from there the frame
	// loop's ordinary pace makes one frame's worth of audio per frame again.
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
	GBA_CHECK_EQ((int)buffer.Underruns(), 0);
	GBA_CHECK_EQ((int)buffer.Drops(), 0);
}
