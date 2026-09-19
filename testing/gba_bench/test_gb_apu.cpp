// Unit tests for the Game Boy sound controller (src/gba/gb_apu.cpp) - the DMG/CGB APU.
//
// The module had no tests at all until now, which is how a set of timing bugs lived in it: the
// frame sequencer stepped every 512 system clocks instead of every 8192 (so the length, the
// envelope and the sweep all ran 16 times too fast), it clocked the length on step 7 as well (320
// Hz with a jitter instead of 256), the length counters counted the wrong way round, the pulse and
// wave dividers were clocked twice as fast as the hardware (every channel an octave high), the duty
// waveforms had their phases shifted and NR50's master volume was never applied.
//
// Every expectation below is recomputed from the rules rather than read back from the
// implementation:
//
//   * the programming manuals' durations: the sound length is (64 - st) / 256 s (256 for the wave
//     channel), the envelope step is n / 64 s, the sweep step is n / 128 s, and the pulse frequency
//     is 4194304 / (4 * 8 * (2048 - fdat)) Hz - which is the Pan Docs' "the pulse divider is
//     clocked at 1048576 Hz and the waveform has eight samples", i.e. four system clocks per step
//     of (2048 - x);
//   * the wave channel's divider is clocked at 2097152 Hz (two clocks per sample of (2048 - x))
//     and the noise LFSR at 262144 / (divisor * 2^shift) Hz with the divisor 0 meaning 0.5;
//   * the frame sequencer steps at 512 Hz (every 8192 system clocks, DIV's bit 4) and clocks the
//     length on steps 0, 2, 4 and 6, the sweep on steps 2 and 6 and the envelope on step 7;
//   * the duty waveforms as the Pan Docs print them (12.5 % = "00000001" .. 75 % = "01111110");
//   * the mixer: the four digital outputs are summed per side, scaled by NR50 (0 counting as 1 and
//     7 as 8) and put through the high pass filter, and the full scale is the four channels at full
//     volume with NR50 = 7 (the manuals' "each sound is output at 0.75V; 0.75V * 4 = 3V").
//
// The tests drive the APU directly (its Tick takes system clocks, so a test can advance it by
// exactly the number of clocks it wants) and read the channels back with ChannelOutput (the digital
// value the CGB's PCM12/PCM34 registers expose) or through the mixed samples.

#include "gba_test.h"
#include "gb_bus.h"

#include <algorithm>
#include <vector>

namespace
{
	using namespace GBA;

	// The host rate the tests use: 32768 Hz divides the 4.194304 MHz clock exactly, so one sample
	// is 128 system clocks and every expected sample count below is a whole number.
	const int HostRate = 32768;
	const int CyclesPerSample = GbCyclesPerSecond / HostRate;			// 128

	// The frame sequencer's rates, in system clocks and in host samples.
	const int FrameStep = GbCyclesPerSecond / 512;						// 8192: one 512 Hz step
	const int LengthTick = GbCyclesPerSecond / 256;						// 16384: two steps
	const int SweepTick = GbCyclesPerSecond / 128;						// 32768: four steps
	const int EnvelopeTick = GbCyclesPerSecond / 64;					// 65536: eight steps

	const int LengthTickSamples = LengthTick / CyclesPerSample;			// 128
	const int SweepTickSamples = SweepTick / CyclesPerSample;			// 256
	const int EnvelopeTickSamples = EnvelopeTick / CyclesPerSample;		// 512

	// Where the first clock of each kind lands after a trigger that happens at frame sequencer step
	// 0 (which is where Reset leaves it): the length on the next step 0, the sweep on step 2 and
	// the envelope on step 7.
	const int FirstSweepSamples = 3 * FrameStep / CyclesPerSample;		// 192
	const int FirstEnvelopeSamples = 8 * FrameStep / CyclesPerSample;	// 512

	struct Machine
	{
		GbBus bus;

		Machine()
		{
			bus.Reset();
			bus.apu.SetSampleRate(HostRate);
			bus.apu.Reset();
		}

		GbApu& apu() { return bus.apu; }

		void Write(uint16_t address, uint8_t value) { bus.apu.WriteRegister(address, value); }
		uint8_t Read(uint16_t address) const { return bus.apu.ReadRegister(address); }

		// The APU is clocked in slices of exactly one host sample, so the state a test observes is
		// the state at a sample boundary.
		void Tick(int samples = 1)
		{
			for (int i = 0; i < samples; i++)
				apu().Tick(CyclesPerSample);
		}

		std::vector<int16_t> Drain()
		{
			std::vector<int16_t> out;
			int16_t buffer[256 * 2];

			for (;;)
			{
				int frames = apu().ReadSamples(buffer, 256);
				if (frames <= 0)
					break;
				out.insert(out.end(), buffer, buffer + frames * 2);
			}

			return out;
		}

		/// <summary>Clock the APU by `samples` host samples and collect the digital output of one
		/// channel, one value per sample.</summary>
		std::vector<int> Digital(int samples, int channel = 0)
		{
			std::vector<int> out;

			for (int i = 0; i < samples; i++)
			{
				Tick(1);
				out.push_back(apu().ChannelOutput(channel));
			}

			return out;
		}
	};

	// -- register addresses (Pan Docs "Audio Registers") -----------------------------------------

	const uint16_t NR10 = 0xFF10, NR11 = 0xFF11, NR12 = 0xFF12, NR13 = 0xFF13, NR14 = 0xFF14;
	const uint16_t NR21 = 0xFF16, NR22 = 0xFF17, NR23 = 0xFF18, NR24 = 0xFF19;
	const uint16_t NR30 = 0xFF1A, NR31 = 0xFF1B, NR32 = 0xFF1C, NR33 = 0xFF1D, NR34 = 0xFF1E;
	const uint16_t NR41 = 0xFF20, NR42 = 0xFF21, NR43 = 0xFF22, NR44 = 0xFF23;
	const uint16_t NR50 = 0xFF24, NR51 = 0xFF25, NR52 = 0xFF26;
	const uint16_t WaveRam = 0xFF30;

	const uint16_t Pcm12 = 0xFF76, Pcm34 = 0xFF77;

	// -- helpers ---------------------------------------------------------------------------------

	/// <summary>The four duty waveforms as the Pan Docs print them: the level of duty steps 0 to 7.
	/// 12.5 % is one high step out of the eight, 25 % two, 50 % four and 75 % six.</summary>
	const char* const DutyPatterns[4] = { "00000001", "10000001", "10000111", "01111110" };

	bool DutyLevel(int duty, int step)
	{
		return DutyPatterns[duty][step & 7] == '1';
	}

	/// <summary>The mixer's full scale: four channels at volume 15 with NR50's 7 ("8").</summary>
	const double FullScale = 4.0 * 15.0 * 8.0;

	int Scaled(int digitalUnits)
	{
		return (int)(digitalUnits * 32767.0 / FullScale);
	}

	/// <summary>Channel 1 playing a plain pulse: no sweep, no envelope, no length, and the given
	/// frequency, duty and volume. The routing is NR51 = CH1 on both sides with NR50 at maximum, so
	/// the mixed signal is the channel's own output scaled by eight.</summary>
	void PlayPulse1(Machine& m, int frequency, int duty, int volume)
	{
		m.Write(NR52, 0x80);
		m.Write(NR50, 0x77);
		m.Write(NR51, 0x11);
		m.Write(NR10, 0x00);
		m.Write(NR11, (uint8_t)(duty << 6));
		m.Write(NR12, (uint8_t)(volume << 4));
		m.Write(NR13, (uint8_t)(frequency & 0xFF));
		m.Write(NR14, (uint8_t)(0x80 | ((frequency >> 8) & 0x07)));
	}

	int Peak(const std::vector<int16_t>& samples, bool left)
	{
		int peak = 0;
		for (size_t i = left ? 0 : 1; i < samples.size(); i += 2)
			peak = std::max(peak, std::abs((int)samples[i]));
		return peak;
	}

	int PeakToPeak(const std::vector<int16_t>& samples, bool left)
	{
		int low = 32767, high = -32768;
		for (size_t i = left ? 0 : 1; i < samples.size(); i += 2)
		{
			low = std::min(low, (int)samples[i]);
			high = std::max(high, (int)samples[i]);
		}
		return high - low;
	}

	int HighSamples(const std::vector<int>& values)
	{
		int count = 0;
		for (int value : values)
			if (value != 0)
				count++;
		return count;
	}

	/// <summary>The lengths of the runs of equal values, in order: what a duty waveform's shape is
	/// (its ratios) shows up here without depending on where the run starts.</summary>
	std::vector<int> Runs(const std::vector<int>& values)
	{
		std::vector<int> runs;

		for (size_t i = 0; i < values.size(); i++)
		{
			if (i > 0 && values[i] == values[i - 1])
				runs.back()++;
			else
				runs.push_back(1);
		}

		return runs;
	}

	/// <summary>How many host samples a channel stays on after it was triggered (the channel's
	/// status bit of NR52 is what the test watches).</summary>
	int SamplesUntilOff(Machine& m, int maxSamples = 40000)
	{
		for (int i = 0; i < maxSamples; i++)
		{
			if ((m.Read(NR52) & 0x0F) == 0)
				return i;
			m.Tick(1);
		}

		return maxSamples;
	}
}

// ---------------------------------------------------------------------------------------------
// The pulse channels
// ---------------------------------------------------------------------------------------------

GBA_TEST(GbApu, DutyWaveformPhases)
{
	// The four duty settings differ in their phase as well as their ratio (Pan Docs "NR11", and
	// the "waveform duty cycle" picture of the programming manual). One duty step is
	// (2048 - x) * 4 system clocks, so x = 2016 makes a step exactly one host sample long: the
	// eight samples a channel plays after a trigger are then its eight duty steps.
	for (int duty = 0; duty < 4; duty++)
	{
		Machine m;
		PlayPulse1(m, 2016, duty, 15);

		for (int i = 0; i < 8; i++)
		{
			m.Tick(1);

			// The divider reloads on the same clock the sample is taken on, so the first sample
			// after the trigger is duty step 1.
			int step = (i + 1) & 7;
			int level = m.apu().ChannelOutput(0);

			GBA_CHECK_MSG(level == (DutyLevel(duty, step) ? 15 : 0),
				"duty " + std::to_string(duty) + ", step " + std::to_string(step) +
				": " + std::to_string(level));
		}
	}
}

GBA_TEST(GbApu, PulseDividerRunsAtOneMegabyte)
{
	// The pulse divider is clocked at 1048576 Hz (once per four system clocks), so one duty step
	// lasts 4 * (2048 - x) clocks and the tone is 131072 / (2048 - x) Hz. With x = 1792 a step is
	// 1024 clocks = 8 host samples and the tone is 512 Hz: a duty 50 % channel (steps 0, 5, 6 and
	// 7 high) plays 32 samples high and 32 low, i.e. one run of 32 samples per 64 sample period.
	Machine m;
	PlayPulse1(m, 1792, 2, 15);

	// The runs of a 50 % waveform are all 32 samples long; the first and the last one of the window
	// are partial, so only the ones in between are checked (and the first sample's run is short
	// because the trigger lands in the middle of a duty step).
	std::vector<int> runs = Runs(m.Digital(300));
	GBA_CHECK(runs.size() >= 6);
	for (size_t i = 1; i + 1 < runs.size(); i++)
		GBA_CHECK_MSG(runs[i] == 32, "run " + std::to_string(i) + " is " + std::to_string(runs[i]));

	// The same divider with duty 12.5 % (duty 0): one step high out of the eight, so 8 high
	// samples and 56 low ones, and 64 high samples out of every 512.
	Machine narrow;
	PlayPulse1(narrow, 1792, 0, 15);
	std::vector<int> digital = narrow.Digital(512);
	GBA_CHECK_EQ(HighSamples(digital), 64);

	std::vector<int> narrowRuns = Runs(digital);
	for (size_t i = 1; i + 1 < narrowRuns.size(); i++)
		GBA_CHECK(narrowRuns[i] == 8 || narrowRuns[i] == 56);
}

GBA_TEST(GbApu, DutyStepIsNotResetByATrigger)
{
	// "Retriggering a pulse channel causes its duty step timer to reset" but not the step itself
	// (Pan Docs "Audio Details": the step counter can only be reset by turning the APU off), so a
	// channel that is retriggered while it is playing picks up at the step it was on. With x = 1792
	// a step lasts eight samples: 50 samples in, the step is 6, and after the trigger the channel
	// plays step 6 for eight samples and step 7 (the only high step of duty 12.5 %) for the next
	// eight.
	Machine m;
	PlayPulse1(m, 1792, 0, 15);

	m.Tick(50);
	GBA_CHECK_EQ(m.apu().ChannelOutput(0), 0);		// step 6, which is low

	m.Write(NR14, 0x87);							// trigger again
	std::vector<int> digital = m.Digital(16);

	for (int i = 0; i < 7; i++)
		GBA_CHECK_MSG(digital[i] == 0, "sample " + std::to_string(i));
	for (int i = 7; i < 15; i++)
		GBA_CHECK_MSG(digital[i] == 15, "sample " + std::to_string(i));
	GBA_CHECK_EQ(digital[15], 0);
}

// ---------------------------------------------------------------------------------------------
// The length timer
// ---------------------------------------------------------------------------------------------

GBA_TEST(GbApu, LengthTimerLastsTheDocumentedTime)
{
	// "Sound length = (64 - st) / 256 s" (the programming manuals, next to NRx1): a channel whose
	// length is enabled plays (64 - st) 256 Hz ticks and is then switched off. At 32768 Hz that is
	// (64 - st) * 128 samples, give or take the frame sequencer's phase.
	const int values[4] = { 0, 1, 32, 63 };

	for (int i = 0; i < 4; i++)
	{
		int st = values[i];

		Machine m;
		m.Write(NR52, 0x80);
		m.Write(NR50, 0x77);
		m.Write(NR51, 0x11);
		m.Write(NR10, 0x00);
		m.Write(NR11, (uint8_t)(0x80 | st));					// duty 2, length st
		m.Write(NR12, 0xF0);									// volume 15, no envelope
		m.Write(NR13, (uint8_t)(1792 & 0xFF));
		m.Write(NR14, (uint8_t)(0x80 | 0x40 | (1792 >> 8)));	// trigger, length enabled

		int expected = (64 - st) * LengthTickSamples;
		int measured = SamplesUntilOff(m);

		// The counter is clocked by the frame sequencer, whose phase the trigger lands in the
		// middle of: one tick of slack.
		GBA_CHECK_MSG(std::abs(measured - expected) <= LengthTickSamples,
			"st " + std::to_string(st) + ": " + std::to_string(measured) +
			" samples instead of " + std::to_string(expected));

		GBA_CHECK((m.Read(NR52) & 0x01) == 0);
	}
}

GBA_TEST(GbApu, LengthTimerOfEveryChannel)
{
	// The wave channel's length counts to 256, the three others to 64 (Pan Docs "Audio"), and the
	// length is only clocked while the channel's length enable bit is set.
	Machine m;
	m.Write(NR52, 0x80);
	m.Write(NR50, 0x77);
	m.Write(NR51, 0xFF);

	// Channel 2: length 0 with the length enabled lasts the full 64 ticks.
	m.Write(NR21, 0x80);
	m.Write(NR22, 0xF0);
	m.Write(NR23, (uint8_t)(1792 & 0xFF));
	m.Write(NR24, (uint8_t)(0x80 | 0x40 | (1792 >> 8)));
	int squares = SamplesUntilOff(m) / LengthTickSamples;
	GBA_CHECK_MSG(squares == 63 || squares == 64, "channel 2: " + std::to_string(squares) + " ticks");

	// Channel 4: the same.
	m.Write(NR41, 0x00);
	m.Write(NR42, 0xF0);
	m.Write(NR43, 0x00);
	m.Write(NR44, (uint8_t)(0x80 | 0x40));
	int noise = SamplesUntilOff(m) / LengthTickSamples;
	GBA_CHECK_MSG(noise == 63 || noise == 64, "channel 4: " + std::to_string(noise) + " ticks");

	// Channel 3: 256 ticks.
	m.Write(NR30, 0x80);
	m.Write(NR31, 0x00);
	m.Write(NR32, 0x20);
	m.Write(NR33, (uint8_t)(1792 & 0xFF));
	m.Write(NR34, (uint8_t)(0x80 | 0x40 | (1792 >> 8)));
	int wave = SamplesUntilOff(m) / LengthTickSamples;
	GBA_CHECK_MSG(wave == 255 || wave == 256, "channel 3: " + std::to_string(wave) + " ticks");

	// ... and without the length enable bit a channel keeps playing.
	Machine endless;
	endless.Write(NR52, 0x80);
	endless.Write(NR11, 0x80);
	endless.Write(NR12, 0xF0);
	endless.Write(NR13, (uint8_t)(1792 & 0xFF));
	endless.Write(NR14, 0x87);
	endless.Tick(4 * 64 * LengthTickSamples);
	GBA_CHECK((endless.Read(NR52) & 0x01) == 1);
}

GBA_TEST(GbApu, LengthIsOnlyReArmedWhenItHasRunOut)
{
	// "If length timer expired it is reset" (Pan Docs "NR14"): retriggering a channel whose length
	// is still counting keeps the remainder, while a channel that has run out gets the full length
	// again.
	Machine m;
	m.Write(NR52, 0x80);
	m.Write(NR11, 0x80 | 0x20);								// length 32
	m.Write(NR12, 0xF0);
	m.Write(NR13, (uint8_t)(1792 & 0xFF));

	m.Write(NR14, (uint8_t)(0x80 | 0x40 | (1792 >> 8)));	// trigger with the length enabled

	// A quarter of the note (8 of its 32 ticks) passes, then the channel is retriggered: the
	// counter keeps its place, so the note ends after 32 ticks, not after 40.
	m.Tick(8 * LengthTickSamples);
	m.Write(NR14, (uint8_t)(0x80 | 0x40 | (1792 >> 8)));

	int measured = 8 * LengthTickSamples + SamplesUntilOff(m);
	int expected = 32 * LengthTickSamples;
	GBA_CHECK_MSG(std::abs(measured - expected) <= LengthTickSamples,
		std::to_string(measured) + " samples instead of " + std::to_string(expected));

	// Once it has run out, a trigger arms it again and the note lasts its whole length.
	m.Write(NR14, (uint8_t)(0x80 | 0x40 | (1792 >> 8)));
	int armed = SamplesUntilOff(m) / LengthTickSamples;
	GBA_CHECK_MSG(armed == 31 || armed == 32, "re-armed: " + std::to_string(armed) + " ticks");
}

// ---------------------------------------------------------------------------------------------
// The envelope and the sweep
// ---------------------------------------------------------------------------------------------

GBA_TEST(GbApu, EnvelopeStepsAt64Hz)
{
	// The envelope is clocked at 64 Hz and takes one step every `n` of those ticks (the manuals:
	// "the length of 1 step = n / 64 s"). With n = 1 the volume falls from 15 by one every 512
	// host samples, i.e. every 512 / 8 = 64 duty cycles of a one-step-per-sample channel.
	Machine m;
	PlayPulse1(m, 2016, 2, 15);						// one duty step per sample
	m.Write(NR12, 0xF1);							// volume 15, decreasing, step time 1
	m.Write(NR14, 0x80 | (2016 >> 8));				// trigger again

	// The loudest sample of every duty cycle (the channel is silent on the low steps of the 50 %
	// waveform), which is the envelope's volume.
	std::vector<int> volume;

	for (int cycle = 0; cycle < 6 * (EnvelopeTickSamples / 8); cycle++)
	{
		int peak = 0;
		for (int i = 0; i < 8; i++)
		{
			m.Tick(1);
			peak = std::max(peak, m.apu().ChannelOutput(0));
		}
		volume.push_back(peak);
	}

	int step = -1;
	for (size_t i = 1; i < volume.size(); i++)
	{
		if (volume[i] != volume[i - 1])
		{
			GBA_CHECK_EQ(volume[i - 1], 15);
			GBA_CHECK_EQ(volume[i], 14);
			step = (int)i;
			break;
		}
	}

	GBA_CHECK_MSG(step > 0, "the envelope never stepped");
	GBA_CHECK_MSG(std::abs(step * 8 - FirstEnvelopeSamples) <= 8,
		"the first envelope step was at sample " + std::to_string(step * 8));

	// The envelope stops at 0 and never switches the channel off (Pan Docs "NR52": "The envelope
	// reaching a volume of 0 does NOT turn the channel off").
	m.Tick(20 * EnvelopeTickSamples);
	GBA_CHECK_EQ(m.apu().ChannelOutput(0), 0);
	GBA_CHECK((m.Read(NR52) & 0x01) == 1);
}

GBA_TEST(GbApu, SweepStepsAt128HzAndOverflowStopsTheChannel)
{
	// The sweep is clocked at 128 Hz (Pan Docs "Audio Details") and every step replaces the period
	// with x +/- x / 2^shift. Starting from 500 with a shift of 1: 500 -> 750 -> 1125, and the
	// second calculation of the third step (1125 + 562 = 1687 -> 1687 + 843 = 2530) overflows the
	// 11 bits and stops the channel, which the sweep runs straight away (Pan Docs: "frequency
	// calculation and overflow check are run again immediately using this new value").
	Machine m;
	m.Write(NR52, 0x80);
	m.Write(NR51, 0x11);
	m.Write(NR10, 0x11);							// pace 1, addition, shift 1
	m.Write(NR11, 0x80);
	m.Write(NR12, 0xF0);
	m.Write(NR13, (uint8_t)(500 & 0xFF));
	m.Write(NR14, (uint8_t)(0x80 | (500 >> 8)));

	GBA_CHECK((m.Read(NR52) & 0x01) == 1);

	int off = SamplesUntilOff(m);
	int expected = FirstSweepSamples + 2 * SweepTickSamples;		// 704: the third sweep step
	GBA_CHECK_MSG(std::abs(off - expected) <= SweepTickSamples,
		std::to_string(off) + " samples instead of " + std::to_string(expected));

	// A single step at 128 Hz is 256 host samples: 500 -> 750 is +50 %, so the tone rises from
	// 131072 / 1548 = 84.7 Hz to 131072 / 1298 = 101 Hz. The duty step lasts 4 * (2048 - x)
	// clocks, so 32.7 host samples before the step and 26.1 after: the first sweep step has to be
	// visible in the run lengths of the digital output.
	Machine tone;
	tone.Write(NR52, 0x80);
	tone.Write(NR51, 0x11);
	tone.Write(NR12, 0xF0);
	tone.Write(NR10, 0x11);
	tone.Write(NR11, 0x80);							// duty 2: 50 % of the period is high
	tone.Write(NR13, (uint8_t)(500 & 0xFF));
	tone.Write(NR14, (uint8_t)(0x80 | (500 >> 8)));

	std::vector<int> early = tone.Digital(31);		// before the first sweep step
	std::vector<int> late_ = tone.Digital(100);		// after it
	GBA_CHECK(HighSamples(early) > 0);
	GBA_CHECK(HighSamples(late_) > 0);
	GBA_CHECK(Runs(late_).size() >= 2);

	// The trigger itself runs the calculation and the overflow check when the shift is not zero:
	// a period that overflows immediately never plays at all.
	Machine immediate;
	immediate.Write(NR52, 0x80);
	immediate.Write(NR10, 0x11);
	immediate.Write(NR11, 0x80);
	immediate.Write(NR12, 0xF0);
	immediate.Write(NR13, (uint8_t)(1792 & 0xFF));
	immediate.Write(NR14, (uint8_t)(0x80 | (1792 >> 8)));
	GBA_CHECK((immediate.Read(NR52) & 0x01) == 0);

	// The "negate and clear" quirk (Pan Docs "Audio Details"): clearing the direction bit after a
	// subtraction has been calculated switches the channel off.
	Machine quirk;
	quirk.Write(NR52, 0x80);
	quirk.Write(NR10, 0x19);						// pace 1, subtraction, shift 1
	quirk.Write(NR11, 0x80);
	quirk.Write(NR12, 0xF0);
	quirk.Write(NR13, (uint8_t)(1000 & 0xFF));
	quirk.Write(NR14, (uint8_t)(0x80 | (1000 >> 8)));
	GBA_CHECK((quirk.Read(NR52) & 0x01) == 1);

	quirk.Tick(FirstSweepSamples);					// one subtraction happened
	GBA_CHECK((quirk.Read(NR52) & 0x01) == 1);

	quirk.Write(NR10, 0x11);						// clear the direction bit
	GBA_CHECK((quirk.Read(NR52) & 0x01) == 0);
}

// ---------------------------------------------------------------------------------------------
// The wave channel and the noise channel
// ---------------------------------------------------------------------------------------------

GBA_TEST(GbApu, WaveChannelPlaysTheRamFromSampleOne)
{
	// "When CH3 is started, the first sample read is the one at index 1, i.e. the lower nibble of
	// the first byte" (Pan Docs "Wave pattern RAM"), the divider is clocked at 2097152 Hz
	// ((2048 - x) * 2 clocks per sample) and the sample buffer holds the last sample read: with
	// x = 1792 a sample lasts 512 clocks = 4 host samples, so the buffer starts at 0 (it is
	// cleared when the APU is reset), shows the low nibble of the first byte at sample 3 and the
	// high nibble of it at the end of the 32 sample pass.
	Machine m;
	m.Write(NR52, 0x80);
	m.Write(NR51, 0x44);							// channel 3 on both sides
	m.Write(NR50, 0x77);

	m.Write(WaveRam + 0, 0x1A);
	for (int i = 1; i < 16; i++)
		m.Write((uint16_t)(WaveRam + i), 0x00);

	m.Write(NR30, 0x80);							// DAC on
	m.Write(NR32, 0x20);							// 100 %
	m.Write(NR33, (uint8_t)(1792 & 0xFF));
	m.Write(NR34, (uint8_t)(0x80 | (1792 >> 8)));

	std::vector<int> digital = m.Digital(128, 2);
	GBA_CHECK_EQ(digital[0], 0);
	GBA_CHECK_EQ(digital[1], 0);
	GBA_CHECK_EQ(digital[2], 0);
	GBA_CHECK_EQ(digital[3], 0x0A);					// the low nibble of 0x1A
	GBA_CHECK_EQ(digital[7], 0x00);					// the high nibble of the second byte

	// Sample 32 of the waveform (the end of the pass) is the high nibble of the first byte: the
	// channel skips it at the start, so it comes back around here.
	GBA_CHECK_EQ(digital[127], 0x01);

	// The output level shifts the digital value (Pan Docs "NR32"): 50 % is one shift right, 25 %
	// two, and 0 mutes the channel.
	int full = m.apu().ChannelOutput(2);
	GBA_CHECK_EQ(full, 0x01);

	m.Write(NR32, 0x40);
	GBA_CHECK_EQ(m.apu().ChannelOutput(2), full >> 1);

	m.Write(NR32, 0x60);
	GBA_CHECK_EQ(m.apu().ChannelOutput(2), full >> 2);

	m.Write(NR32, 0x00);
	GBA_CHECK_EQ(m.apu().ChannelOutput(2), 0);
}

GBA_TEST(GbApu, NoiseLfsrFollowsTheDocumentedSequence)
{
	// The LFSR is ticked at 262144 / (divisor * 2^shift) Hz; with divisor 0 (0.5) and shift 4 that
	// is one step per host sample, so the sequence can be compared sample by sample. The algorithm
	// is the Pan Docs': the XNOR of bits 0 and 1 is written to bit 14, in 7 bit mode to bit 6 as
	// well, the register shifts right, and the bit that comes out chooses between the volume and
	// silence. (The APU keeps the complement of that state and reads the bit the other way round,
	// which is the same sequence with the opposite polarity - the test recomputes the output from
	// the document's LFSR.)
	Machine m;
	m.Write(NR52, 0x80);
	m.Write(NR51, 0x88);							// channel 4 on both sides
	m.Write(NR50, 0x77);
	m.Write(NR41, 0x00);
	m.Write(NR42, 0xF0);							// volume 15, no envelope
	m.Write(NR43, (uint8_t)((4 << 4) | 0x00));		// shift 4, 15 bits, divisor 0
	m.Write(NR44, 0x80);

	uint16_t lfsr = 0x0000;							// the document's LFSR starts at zero here
	std::vector<int> digital = m.Digital(96, 3);

	for (int i = 0; i < (int)digital.size(); i++)
	{
		// One step per sample, taken before the sample itself (the tick order of Tick()).
		uint16_t feedback = (uint16_t)(((lfsr ^ (lfsr >> 1)) & 1) ^ 1);		// the XNOR
		lfsr = (uint16_t)((lfsr >> 1) | (feedback << 14));
		bool high = (lfsr & 1) != 0;

		GBA_CHECK_MSG(digital[i] == (high ? 15 : 0),
			"sample " + std::to_string(i) + ": " + std::to_string(digital[i]));
	}

	// A larger shift slows the LFSR down: with shift 6 one step lasts 8 << 6 = 512 clocks, i.e.
	// four host samples (the first step lands on the fourth sample, because the trigger's divider
	// has not run out before that). The whole sequence is compared again, this time with the steps
	// four samples apart.
	Machine slow;
	slow.Write(NR52, 0x80);
	slow.Write(NR51, 0x88);
	slow.Write(NR41, 0x00);
	slow.Write(NR42, 0xF0);
	slow.Write(NR43, (uint8_t)((6 << 4) | 0x00));
	slow.Write(NR44, 0x80);

	lfsr = 0x0000;
	std::vector<int> slower = slow.Digital(129, 3);

	for (int i = 0; i < (int)slower.size(); i++)
	{
		if ((i % 4) == 3)
		{
			uint16_t feedback = (uint16_t)(((lfsr ^ (lfsr >> 1)) & 1) ^ 1);
			lfsr = (uint16_t)((lfsr >> 1) | (feedback << 14));
		}

		bool high = (lfsr & 1) != 0;
		GBA_CHECK_MSG(slower[i] == (high ? 15 : 0),
			"shift 6 sample " + std::to_string(i) + ": " + std::to_string(slower[i]));
	}

	// A clock shift of 14 or 15 stops the LFSR (Pan Docs "NR43"): the output never changes.
	Machine stopped;
	stopped.Write(NR52, 0x80);
	stopped.Write(NR51, 0x88);
	stopped.Write(NR41, 0x00);
	stopped.Write(NR42, 0xF0);
	stopped.Write(NR43, (uint8_t)((14 << 4) | 0x00));
	stopped.Write(NR44, 0x80);

	std::vector<int> still = stopped.Digital(64, 3);
	for (size_t i = 1; i < still.size(); i++)
		GBA_CHECK_EQ(still[i], still[0]);
}

// ---------------------------------------------------------------------------------------------
// The mixer
// ---------------------------------------------------------------------------------------------

GBA_TEST(GbApu, MasterVolumeScalesBothSides)
{
	// "A value of 0 is treated as a volume of 1 and 7 as 8" (Pan Docs "NR50"), and the amplifier
	// never mutes a non-silent input. A single channel at volume 15 with NR50 = 0x77 spans
	// 15 * 8 of the mixer's full scale (four channels at volume 15 with NR50 = 7); the amplitudes
	// are compared with each other, which is what the master volume is about.
	Machine full;
	PlayPulse1(full, 1792, 2, 15);
	full.Tick(2000);								// let the high pass filter settle
	full.Drain();
	full.Tick(128);
	std::vector<int16_t> loud = full.Drain();

	int loudLeft = PeakToPeak(loud, true);
	GBA_CHECK_MSG(std::abs(loudLeft - Scaled(15 * 8)) <= Scaled(15 * 8) / 10,
		"the full volume level is " + std::to_string(loudLeft) + " instead of " +
		std::to_string(Scaled(15 * 8)));
	GBA_CHECK(std::abs(PeakToPeak(loud, false) - loudLeft) <= loudLeft / 20);

	// Half the master volume (3 counts as 4) is half the amplitude.
	Machine half;
	PlayPulse1(half, 1792, 2, 15);
	half.Write(NR50, 0x33);
	half.Tick(2000);
	half.Drain();
	half.Tick(128);
	int halfLeft = PeakToPeak(half.Drain(), true);
	GBA_CHECK_MSG(std::abs(halfLeft * 2 - loudLeft) <= loudLeft / 20,
		"the half volume level is " + std::to_string(halfLeft) + " against " +
		std::to_string(loudLeft / 2));

	// A master volume of 0 is a volume of 1: quiet, but not silent.
	Machine minimum;
	PlayPulse1(minimum, 1792, 2, 15);
	minimum.Write(NR50, 0x00);
	minimum.Tick(2000);
	minimum.Drain();
	minimum.Tick(128);
	int faintLeft = PeakToPeak(minimum.Drain(), true);
	GBA_CHECK(faintLeft > 0);
	GBA_CHECK_MSG(std::abs(faintLeft * 8 - loudLeft) <= loudLeft / 10,
		"the minimum volume level is " + std::to_string(faintLeft) + " against " +
		std::to_string(loudLeft / 8));
}

GBA_TEST(GbApu, PanningAndDisabledDacs)
{
	// NR51 routes each channel to either output (Pan Docs "NR51"), and a channel whose DAC is off
	// contributes nothing at all.
	Machine m;
	PlayPulse1(m, 1792, 2, 15);
	m.Tick(600);
	std::vector<int16_t> both = m.Drain();
	GBA_CHECK(Peak(both, true) > 0);
	GBA_CHECK(Peak(both, false) > 0);

	// Unrouting a channel is a DC step, which the high pass filter smooths out over a few
	// milliseconds (the pop the Pan Docs describe): let it settle before measuring.
	m.Write(NR51, 0x01);							// channel 1 to the right only
	m.Tick(2000);
	m.Drain();
	m.Tick(600);
	std::vector<int16_t> right = m.Drain();
	GBA_CHECK_EQ(Peak(right, true), 0);
	GBA_CHECK(Peak(right, false) > 0);

	m.Write(NR51, 0x10);							// ... and to the left only
	m.Tick(2000);
	m.Drain();
	m.Tick(600);
	std::vector<int16_t> left = m.Drain();
	GBA_CHECK(Peak(left, true) > 0);
	GBA_CHECK_EQ(Peak(left, false), 0);

	// NR12's bits 7-3 are the DAC: all zero turns the channel off, and with every DAC off the
	// mixer's output is exactly zero.
	m.Write(NR51, 0x11);
	m.Write(NR12, 0x00);
	GBA_CHECK((m.Read(NR52) & 0x01) == 0);

	m.Tick(600);
	std::vector<int16_t> off = m.Drain();
	GBA_CHECK_EQ(Peak(off, true), 0);
	GBA_CHECK_EQ(Peak(off, false), 0);

	// Volume 0 with a DAC that is on (0x08) is silence, but the channel stays on, which is what
	// "the envelope reaching a volume of 0 does NOT turn the channel off" means in practice.
	m.Write(NR12, 0x08);
	m.Write(NR14, 0x87);
	GBA_CHECK((m.Read(NR52) & 0x01) == 1);
	GBA_CHECK_EQ(m.apu().ChannelOutput(0), 0);
}

GBA_TEST(GbApu, SampleClockIsExactForEveryHostRate)
{
	// The sample clock adds the host rate to an accumulator every system clock, so any rate the
	// frontend asks for produces exactly that many samples a second (the integer division the
	// mixer used to do played 48 kHz 0.44 % sharp).
	const int rates[4] = { 32768, 44100, 48000, 192000 };

	for (int i = 0; i < 4; i++)
	{
		Machine m;
		m.apu().SetSampleRate(rates[i]);
		PlayPulse1(m, 1792, 2, 15);

		int remaining = GbCyclesPerSecond;

		while (remaining > 0)
		{
			int slice = std::min(remaining, 65536);
			m.apu().Tick(slice);
			remaining -= slice;
		}

		int samples = (int)m.Drain().size() / 2;
		GBA_CHECK_MSG(samples == rates[i], "rate " + std::to_string(rates[i]) + ": " +
			std::to_string(samples) + " samples");
	}
}

GBA_TEST(GbApu, PcmRegistersOnCgbOnly)
{
	// PCM12/PCM34 (0xFF76/0xFF77) are the CGB's copy of the generation circuits' outputs (Pan
	// Docs "Audio Details"): the low nibble of PCM12 is channel 1 and the high nibble channel 2.
	Machine m;
	m.bus.SetCgb(true);
	m.apu().Reset();
	PlayPulse1(m, 1792, 2, 15);
	m.Tick(1);

	uint8_t pcm12 = m.bus.ReadByte(Pcm12);
	GBA_CHECK_EQ((int)(pcm12 & 0x0F), m.apu().ChannelOutput(0));
	GBA_CHECK_EQ((int)(pcm12 >> 4), m.apu().ChannelOutput(1));

	// Channel 3 and 4 in PCM34.
	m.Write(NR30, 0x80);
	m.Write(NR32, 0x20);
	m.Write(NR34, 0x80);
	m.Write(NR42, 0xF0);
	m.Write(NR43, 0x00);
	m.Write(NR44, 0x80);
	m.Tick(1);

	uint8_t pcm34 = m.bus.ReadByte(Pcm34);
	GBA_CHECK_EQ((int)(pcm34 & 0x0F), m.apu().ChannelOutput(2));
	GBA_CHECK_EQ((int)(pcm34 >> 4), m.apu().ChannelOutput(3));

	// A monochrome console has no such registers.
	Machine dmg;
	dmg.bus.SetCgb(false);
	GBA_CHECK_EQ((int)dmg.bus.ReadByte(Pcm12), 0xFF);
	GBA_CHECK_EQ((int)dmg.bus.ReadByte(Pcm34), 0xFF);
}

GBA_TEST(GbApu, HighPassFilterIsModelDependent)
{
	// The CGB's high pass filter is more aggressive than the DMG's (Pan Docs "Audio Details":
	// 0.998943 against 0.999958), so a constant output is pulled towards zero faster on a CGB.
	// A wave channel holding a constant 15 is the easiest way to make one.
	auto decaying = [](bool cgb) -> int
	{
		Machine m;
		m.bus.SetCgb(cgb);
		m.apu().Reset();
		m.Write(NR52, 0x80);
		m.Write(NR51, 0x44);
		m.Write(NR50, 0x77);

		for (int i = 0; i < 16; i++)
			m.Write((uint16_t)(WaveRam + i), 0xFF);

		m.Write(NR30, 0x80);
		m.Write(NR32, 0x20);
		m.Write(NR33, (uint8_t)(1792 & 0xFF));
		m.Write(NR34, (uint8_t)(0x80 | (1792 >> 8)));

		m.Tick(1024);
		m.Drain();
		m.Tick(1);

		std::vector<int16_t> samples = m.Drain();
		return samples.empty() ? 0 : samples[0];
	};

	int dmg = decaying(false);
	int cgb = decaying(true);

	GBA_CHECK(dmg > 0);
	GBA_CHECK_MSG(cgb < dmg, "the CGB's filter left " + std::to_string(cgb) +
		" against the DMG's " + std::to_string(dmg));
}

GBA_TEST(GbApu, HighPassFilterCanBeTurnedOff)
{
	// The frontend's settings can turn the filter off (GbApu::SetHighPassFilter), and then the two
	// outputs are the DACs' raw sum: the very same constant output stays where it is instead of
	// being pulled towards zero. One channel at its maximum is the easiest way to make one.
	auto level = [](bool filter) -> int
	{
		Machine m;
		m.apu().SetHighPassFilter(filter);
		m.apu().Reset();
		m.Write(NR52, 0x80);
		m.Write(NR51, 0x44);
		m.Write(NR50, 0x77);

		for (int i = 0; i < 16; i++)
			m.Write((uint16_t)(WaveRam + i), 0xFF);

		m.Write(NR30, 0x80);
		m.Write(NR32, 0x20);
		m.Write(NR33, (uint8_t)(1792 & 0xFF));
		m.Write(NR34, (uint8_t)(0x80 | (1792 >> 8)));

		// 1024 samples is 31 ms, more than five of the DMG filter's 5.7 ms time constants, so a
		// filtered constant has nothing left by then.
		m.Tick(1024);
		m.Drain();
		m.Tick(1);

		std::vector<int16_t> samples = m.Drain();
		return samples.empty() ? 0 : samples[0];
	};

	int filtered = level(true);
	int raw = level(false);

	// The raw sum is the channel's own 15 of 15 through NR50's volume of eight, on the full scale
	// of four channels at 15 ("the output level is set to 0Fh ... 0.75V * 4 = 3V"): 120 / 480.
	const int Expected = (int)((15.0 * 8.0) * (32767.0 / (4.0 * 15.0 * 8.0)));

	GBA_CHECK_EQ(raw, Expected);
	GBA_CHECK_MSG(filtered > 0 && filtered < raw / 40,
		"the filter left " + std::to_string(filtered) + " of the raw " + std::to_string(raw));
}
