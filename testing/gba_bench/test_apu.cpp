// Unit tests for the GBA sound controller (src/gba/gba_apu.cpp).
//
// Every test drives a real Apu through a real GbaBus: the sound registers are written byte wise
// with Apu::Write8 (offsets relative to 0x04000000, exactly as the bus passes them), the clock is
// advanced with Apu::Tick and the mixed audio is read back with Apu::ReadSamples. All expected
// values are derived here from the hardware rules - GBATEK "GBA Sound Controller" and its channel
// chapters, and the Pan Docs audio chapters - the register bit layouts, the 512 Hz frame
// sequencer, the channel clocks, the LFSR and the FIFO DMA protocol are recomputed in the test
// rather than read back from the APU.
//
// The tests advance the clock in slices of one host sample and drain the queue after every slice
// they care about, so the frame sequencer steps (which the APU applies once per Tick) land where
// the hardware rules say they do.

#include "gba_test.h"
#include "gba_bus.h"

#include <algorithm>

namespace
{
	using namespace GBA;

	// -- register offsets relative to 0x04000000 (GBATEK "GBA I/O Map") ----------------------

	const uint32_t RegNR10 = 0x060, RegNR11 = 0x062, RegNR12 = 0x063, RegNR13 = 0x064, RegNR14 = 0x065;
	const uint32_t RegNR21 = 0x068, RegNR22 = 0x069, RegNR23 = 0x06C, RegNR24 = 0x06D;
	const uint32_t RegNR30 = 0x070, RegNR31 = 0x072, RegNR32 = 0x073, RegNR33 = 0x074, RegNR34 = 0x075;
	const uint32_t RegNR41 = 0x078, RegNR42 = 0x079, RegNR43 = 0x07C, RegNR44 = 0x07D;
	const uint32_t RegNR50 = 0x080, RegNR51 = 0x081, RegLow = 0x082, RegHigh = 0x083;
	const uint32_t RegCntX = 0x084, RegBiasLow = 0x088, RegBiasHigh = 0x089;
	const uint32_t RegWaveRam = 0x090, RegFifoA = 0x0A0, RegFifoB = 0x0A4;

	// -- timing constants ---------------------------------------------------------------------

	// The APU's default host rate and the system cycles one host sample covers.
	const int HostRate = 32768;
	const int SampleCycles = CyclesPerSecond / HostRate;			// 512

	// The 512 Hz frame sequencer step: length is clocked by the even steps (256 Hz), the sweep
	// by steps 2 and 6 (128 Hz) and the envelope by step 7 (64 Hz) (Pan Docs "DIV-APU").
	const int FrameStep = CyclesPerSecond / 512;					// 32768
	const int LengthStep = FrameStep * 2;							// 65536: two sequencer steps
	const int EnvelopeStep = FrameStep * 8;							// 262144: eight sequencer steps

	// -- the machine under test ---------------------------------------------------------------

	struct Machine
	{
		GbaBus bus;

		Machine()
		{
			bus.Reset();
			bus.apu.SetSampleRate(HostRate);
		}

		Apu& apu() { return bus.apu; }

		void Write(uint32_t offset, uint8_t value) { bus.apu.Write8(offset, value); }
		uint8_t Read(uint32_t offset) const { return bus.apu.Read8(offset, 0xA5); }

		// The timers run first: the FIFO sample clock and channel 3's timer mode read the timer
		// counter, so a slice has to advance the timer before the APU looks at it.
		void Tick(int cycles)
		{
			bus.timers.Tick(bus, cycles);
			bus.apu.Tick(bus, cycles);
		}

		// Advance in slices of one host sample unless the caller asks for something else. A slice
		// size that is a multiple of the timer period would hide an overflow (the counter would
		// come back to the same value), so the default keeps the reads dense.
		void TickSliced(int cycles, int slice = SampleCycles)
		{
			while (cycles > 0)
			{
				int step = (slice < cycles) ? slice : cycles;
				Tick(step);
				cycles -= step;
			}
		}

		std::vector<int16_t> Drain()
		{
			std::vector<int16_t> out;

			// ReadSamples hands over `maxFrames` *stereo* frames, i.e. two int16_t per frame: the
			// buffer has to hold twice as many samples as the frame count it is asked for.
			int16_t buffer[128 * 2];

			for (;;)
			{
				int frames = bus.apu.ReadSamples(buffer, 128);
				if (frames <= 0)
					break;
				out.insert(out.end(), buffer, buffer + frames * 2);
			}
			return out;
		}

		// Take the samples of the next `cycles` of emulated time, in host-sample slices.
		std::vector<int16_t> Run(int cycles)
		{
			TickSliced(cycles);
			return Drain();
		}
	};

	// -- small helpers ------------------------------------------------------------------------

	void EnablePsg(Machine& m)
	{
		// SOUNDCNT_X bit 7: the master enable; NR50 = 7/7 (master volume 100 % on both sides),
		// NR51 = all four channels on both sides; SOUNDCNT_H bits 0-1 = 2 (the PSGs at 100 %).
		m.Write(RegCntX, 0x80);
		m.Write(RegNR50, 0x77);
		m.Write(RegNR51, 0xFF);
		m.Write(RegLow, 0x02);
	}

	// Channel 1 with no sweep and no envelope, triggered and playing a `duty` square wave.
	void PlayChannel1(Machine& m, int frequency, int duty, int volume)
	{
		m.Write(RegNR10, 0x00);								// sweep off (shift 0, time 0)
		m.Write(RegNR11, (uint8_t)(duty << 6));					// duty, length 0 means 64 steps
		m.Write(RegNR12, (uint8_t)(volume << 4));				// volume, envelope step time 0
		m.Write(RegNR13, (uint8_t)(frequency & 0xFF));
		m.Write(RegNR14, (uint8_t)(0x80 | ((frequency >> 8) & 7)));
	}

	std::vector<int16_t> Left(const std::vector<int16_t>& frames)
	{
		std::vector<int16_t> out;
		for (size_t i = 0; i + 1 < frames.size(); i += 2)
			out.push_back(frames[i]);
		return out;
	}

	std::vector<int16_t> Right(const std::vector<int16_t>& frames)
	{
		std::vector<int16_t> out;
		for (size_t i = 1; i < frames.size(); i += 2)
			out.push_back(frames[i]);
		return out;
	}

	bool Silent(const std::vector<int16_t>& mono)
	{
		for (size_t i = 0; i < mono.size(); i++)
		{
			if (mono[i] != 0)
				return false;
		}
		return true;
	}

	int Peak(const std::vector<int16_t>& mono)
	{
		int peak = 0;
		for (size_t i = 0; i < mono.size(); i++)
		{
			int value = (mono[i] < 0) ? -mono[i] : mono[i];
			peak = (value > peak) ? value : peak;
		}
		return peak;
	}

	// The duty waveform period, measured as the most frequent distance between two consecutive
	// rising edges: the steady part of the signal outvotes a buffer that starts in the middle of
	// a high phase and the one-off distances around a sweep step.
	int MeasurePeriod(const std::vector<int16_t>& mono)
	{
		std::vector<int> distances;
		int previous = -1;

		for (size_t i = 0; i < mono.size(); i++)
		{
			bool rising = (mono[i] > 0) && (i == 0 || mono[i - 1] <= 0);
			if (!rising)
				continue;
			if (previous >= 0)
				distances.push_back((int)i - previous);
			previous = (int)i;
		}

		int best = -1;
		int bestCount = 0;
		for (size_t i = 0; i < distances.size(); i++)
		{
			int count = 0;
			for (size_t j = 0; j < distances.size(); j++)
			{
				if (distances[j] == distances[i])
					count++;
			}
			if (count > bestCount)
			{
				bestCount = count;
				best = distances[i];
			}
		}
		return best;
	}

	// -- independent models of the hardware rules ---------------------------------------------

	// The digits of a 32 digit wave pattern: 0, 1, ... 15, 0, 1, ... 15 (a rising ramp, so the
	// playback order is visible in the mixed output).
	const int RampDigits[32] =
	{
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	};

	// Write 32 digits into the 16 wave RAM bytes. The pattern is played as the MSBs of the first
	// byte, then its LSBs, then the MSBs of the second byte, and so on (GBATEK "WAVE_RAM").
	void WriteWaveRam(Machine& m, const int* digits, int count)
	{
		for (int i = 0; i < 16; i++)
		{
			int high = digits[(i * 2) % count];
			int low = digits[(i * 2 + 1) % count];
			m.Write(RegWaveRam + i, (uint8_t)((high << 4) | low));
		}
	}

	// GBATEK "Noise Random Generator": X = X SHR 1; if carry (the bit shifted out) then the
	// output is HIGH and X = X XOR 6000h (15 bit) or 60h (7 bit), else the output is LOW. The
	// initial value is 4000h (15 bit) or 40h (7 bit) and the level before the first clock is
	// bit 0 of that value.
	//
	// The APU clocks the LFSR once per `period` system cycles and one host sample covers
	// exactly `SampleCycles` cycles, so host sample k shows the level after
	// floor(k * SampleCycles / period) clocks.
	std::vector<int> NoiseLevels(int width, int period, int samples)
	{
		uint16_t x = width ? 0x40 : 0x4000;
		int level = x & 1;
		int clocks = 0;
		std::vector<int> levels;

		for (int k = 0; k < samples; k++)
		{
			int target = (int)(((int64_t)k * SampleCycles) / period);
			while (clocks < target)
			{
				int carry = x & 1;
				x >>= 1;
				if (carry)
					x ^= width ? 0x60 : 0x6000;
				level = carry;
				clocks++;
			}
			levels.push_back(level);
		}
		return levels;
	}
}

// ---------------------------------------------------------------------------------------------
// The register file
// ---------------------------------------------------------------------------------------------

GBA_TEST(Apu, RegisterReadWrite)
{
	Machine m;

	// While SOUNDCNT_X bit 7 is clear the PSG registers are held at zero: a write before the
	// master enable is set must be lost (GBATEK "SOUNDCNT_X").
	m.Write(RegNR12, 0xF0);
	GBA_CHECK_EQ(m.Read(RegNR12), 0x00);

	m.Write(RegCntX, 0x80);
	GBA_CHECK(m.apu().Enabled());

	// The readable bits of every PSG register (GBATEK "GBA Sound Control Registers").
	m.Write(RegNR10, 0xFF);
	GBA_CHECK_EQ(m.Read(RegNR10), 0x7F);			// NR10 bits 0-6 only
	m.Write(RegNR11, 0xFF);
	GBA_CHECK_EQ(m.Read(RegNR11), 0xC0);			// NR11: only the duty bits
	m.Write(RegNR12, 0xAB);
	GBA_CHECK_EQ(m.Read(RegNR12), 0xAB);			// NR12 is fully readable
	m.Write(RegNR13, 0x12);
	GBA_CHECK_EQ(m.Read(RegNR13), 0xA5);			// NR13 is write only: the open bus
	m.Write(RegNR14, 0x47);
	GBA_CHECK_EQ(m.Read(RegNR14), 0x40);			// NR14: only the length flag
	GBA_CHECK_EQ(m.Read(0x066), 0xA5);				// unused

	m.Write(RegNR21, 0xFF);
	GBA_CHECK_EQ(m.Read(RegNR21), 0xC0);
	m.Write(RegNR22, 0x9C);
	GBA_CHECK_EQ(m.Read(RegNR22), 0x9C);
	m.Write(RegNR23, 0x34);
	GBA_CHECK_EQ(m.Read(RegNR23), 0xA5);
	m.Write(RegNR24, 0x47);
	GBA_CHECK_EQ(m.Read(RegNR24), 0x40);

	m.Write(RegNR30, 0xFF);
	GBA_CHECK_EQ(m.Read(RegNR30), 0xE0);
	m.Write(RegNR31, 0x55);
	GBA_CHECK_EQ(m.Read(RegNR31), 0xA5);			// NR31 is write only
	m.Write(RegNR32, 0xFF);
	GBA_CHECK_EQ(m.Read(RegNR32), 0xE0);
	m.Write(RegNR33, 0x77);
	GBA_CHECK_EQ(m.Read(RegNR33), 0xA5);
	m.Write(RegNR34, 0x47);
	GBA_CHECK_EQ(m.Read(RegNR34), 0x40);

	m.Write(RegNR41, 0x3F);
	GBA_CHECK_EQ(m.Read(RegNR41), 0xA5);			// NR41 is write only
	m.Write(RegNR42, 0xF3);
	GBA_CHECK_EQ(m.Read(RegNR42), 0xF3);
	m.Write(RegNR43, 0xDE);
	GBA_CHECK_EQ(m.Read(RegNR43), 0xDE);
	m.Write(RegNR44, 0x47);
	GBA_CHECK_EQ(m.Read(RegNR44), 0x40);

	m.Write(RegNR50, 0xFF);
	GBA_CHECK_EQ(m.Read(RegNR50), 0xFF);
	m.Write(RegNR51, 0xFF);
	GBA_CHECK_EQ(m.Read(RegNR51), 0xFF);
	m.Write(RegLow, 0xFF);
	GBA_CHECK_EQ(m.Read(RegLow), 0x0F);				// SOUNDCNT_H bits 4-7 are unused
	m.Write(RegHigh, 0xFF);
	GBA_CHECK_EQ(m.Read(RegHigh), 0x77);			// bits 11/15 are the write-only FIFO resets

	// SOUNDBIAS: bit 0 unused, bits 1-9 the bias level, bits 10-13 unused, 14-15 the resolution
	// (GBATEK "SOUNDBIAS"); it comes up at 0200h.
	GBA_CHECK_EQ(m.Read(RegBiasHigh), 0x02);
	GBA_CHECK_EQ(m.Read(RegBiasLow), 0x00);
	m.Write(RegBiasLow, 0xFF);
	m.Write(RegBiasHigh, 0xFF);
	GBA_CHECK_EQ(m.Read(RegBiasLow), 0xFE);
	GBA_CHECK_EQ(m.Read(RegBiasHigh), 0xC3);

	// The wave RAM is readable; the FIFOs report the byte they play next (on hardware they are
	// write only, the APU keeps them visible for the debugger).
	for (int i = 0; i < 16; i++)
		m.Write(RegWaveRam + i, (uint8_t)(0x10 + i));
	for (int i = 0; i < 16; i++)
		GBA_CHECK_EQ(m.Read(RegWaveRam + i), (uint8_t)(0x10 + i));

	m.Write(RegFifoA + 0, 0x11);
	m.Write(RegFifoA + 1, 0x22);
	GBA_CHECK_EQ(m.Read(RegFifoA), 0x11);
	m.Write(RegFifoB + 0, 0x33);
	GBA_CHECK_EQ(m.Read(RegFifoB), 0x33);
	GBA_CHECK_EQ(m.Read(0x0A8), 0xA5);				// unused

	// SOUNDCNT_X bits 0-3 report the active channels: nothing is playing here.
	GBA_CHECK_EQ(m.Read(RegCntX), 0x80);
}

GBA_TEST(Apu, BusRegisterAccess)
{
	// The other side of the interface: the same registers through the bus's memory map (the bus
	// decodes 0x060..0x0A7 of the I/O space to the APU, byte wise and as halfwords, GBATEK "GBA
	// I/O Map") and the bus clock reaching the APU.
	GbaBus bus;
	bus.Reset();
	bus.apu.SetSampleRate(HostRate);

	bus.Write8(MemIo + RegCntX, 0x80);
	GBA_CHECK(bus.apu.Enabled());

	// SOUNDCNT_L as one halfword: NR50 in the low byte, NR51 in the high byte.
	bus.Write16(MemIo + RegNR50, 0xFF77);
	GBA_CHECK_EQ(bus.Read16(MemIo + RegNR50), 0xFF77);
	GBA_CHECK_EQ(bus.apu.Read8(RegNR50, 0), 0x77);
	GBA_CHECK_EQ(bus.apu.Read8(RegNR51, 0), 0xFF);

	// A halfword write of NR11/NR12 reaches both bytes (little endian).
	bus.Write16(MemIo + RegNR11, 0xF080);
	GBA_CHECK_EQ(bus.apu.Read8(RegNR12, 0), 0xF0);
	GBA_CHECK_EQ(bus.Read8(MemIo + RegNR12), 0xF0);

	// Trigger channel 1 through the bus and clock it with the bus's own Tick: 64 * 512 cycles is
	// one period of the 1792 frequency, and duty 2 is four high steps out of eight.
	bus.Write8(MemIo + RegNR13, (uint8_t)(1792 & 0xFF));
	bus.Write8(MemIo + RegNR14, 0x87);
	GBA_CHECK_EQ(bus.Read8(MemIo + RegCntX) & 0x01, 0x01);

	for (int i = 0; i < 64; i++)
		bus.Tick(SampleCycles);

	std::vector<int16_t> mono;
	int16_t buffer[128 * 2];		// stereo frames: two int16_t each (see Drain)
	for (;;)
	{
		int frames = bus.apu.ReadSamples(buffer, 128);
		if (frames <= 0)
			break;
		for (int i = 0; i < frames; i++)
			mono.push_back(buffer[i * 2]);
	}

	GBA_CHECK_EQ((int)mono.size(), 64);
	for (int i = 0; i < 64; i++)
		GBA_CHECK_MSG((mono[i] > 0) == (i < 32), "sample " + std::to_string(i));
}

GBA_TEST(Apu, SliceIndependence)
{
	// The mixed stream may not depend on how the caller slices the clock: the bus ticks the APU
	// every 64 cycles, a frontend may tick it once per sample. This is what keeps the channel
	// phase accumulators exactly in step with the sample clock (the sample length is the
	// difference of the two floored sample boundary positions, not a rounded constant).
	auto run = [](int slice) -> std::vector<int16_t>
	{
		Machine m;
		EnablePsg(m);
		PlayChannel1(m, 1792, 2, 15);
		m.TickSliced(SampleCycles * 64, slice);
		return Left(m.Drain());
	};

	std::vector<int16_t> perSample = run(SampleCycles);
	std::vector<int16_t> perSlice = run(64);
	GBA_CHECK_EQ((int)perSlice.size(), (int)perSample.size());

	for (size_t i = 0; i < perSample.size(); i++)
		GBA_CHECK_MSG((int)perSlice[i] == (int)perSample[i], "sample " + std::to_string(i));

	// The audio itself is the expected 50 % duty wave: 32 samples high, 32 low.
	GBA_CHECK_EQ((int)perSample.size(), 64);
	for (int i = 0; i < 64; i++)
		GBA_CHECK((perSample[i] > 0) == (i < 32));
}

GBA_TEST(Apu, PowerGate)
{
	Machine m;
	EnablePsg(m);

	// Leave a recognisable pattern in the wave RAM: the master enable must not clear it.
	m.Write(RegWaveRam + 0, 0x42);
	m.Write(RegWaveRam + 5, 0x99);

	PlayChannel1(m, 1792, 2, 15);
	GBA_CHECK_EQ(m.Read(RegCntX), 0x81);			// channel 1 on
	GBA_CHECK(m.Read(RegCntX) & 0x01);

	// SOUNDCNT_X bit 7 clear: the PSG registers read as zero, the channel stops, the wave RAM,
	// SOUNDCNT_H and SOUNDBIAS stay (GBATEK "SOUNDCNT_X").
	m.Write(RegCntX, 0x00);
	GBA_CHECK(!m.apu().Enabled());
	GBA_CHECK_EQ(m.Read(RegCntX), 0x00);
	GBA_CHECK_EQ(m.Read(RegNR10), 0x00);
	GBA_CHECK_EQ(m.Read(RegNR11), 0x00);
	GBA_CHECK_EQ(m.Read(RegNR12), 0x00);
	GBA_CHECK_EQ(m.Read(RegNR51), 0x00);
	GBA_CHECK_EQ(m.Read(RegNR50), 0x00);
	GBA_CHECK_EQ(m.Read(RegWaveRam + 0), 0x42);
	GBA_CHECK_EQ(m.Read(RegWaveRam + 5), 0x99);
	GBA_CHECK_EQ(m.Read(RegLow), 0x02);				// SOUNDCNT_H is kept
	GBA_CHECK_EQ(m.Read(RegBiasHigh), 0x02);		// SOUNDBIAS is kept

	// A write while the APU is off is lost, and so is a trigger: re-enabling must not start the
	// old channel (GBATEK: the registers "must be re-initialized after re-enabling sound").
	m.Write(RegNR12, 0xF0);
	m.Write(RegNR14, 0x80);
	m.Write(RegCntX, 0x80);
	GBA_CHECK_EQ(m.Read(RegNR12), 0x00);
	GBA_CHECK_EQ(m.Read(RegCntX), 0x80);
	GBA_CHECK(Silent(Left(m.Run(SampleCycles * 32))));

	// A channel whose DAC is off cannot be triggered: NR12 = 0 has DAC bits 3-7 clear, so the
	// channel stays off even though NR14 bit 7 is written (Pan Docs "DACs").
	m.Write(RegNR11, 0x80);
	m.Write(RegNR12, 0x00);
	m.Write(RegNR13, 0x00);
	m.Write(RegNR14, 0x87);
	GBA_CHECK_EQ(m.Read(RegCntX), 0x80);
	GBA_CHECK(Silent(Left(m.Run(SampleCycles * 32))));

	// Turning the DAC off while a channel plays stops it.
	m.Write(RegNR12, 0xF0);
	m.Write(RegNR14, 0x87);
	GBA_CHECK_EQ(m.Read(RegCntX), 0x81);
	m.Write(RegNR12, 0x00);
	GBA_CHECK_EQ(m.Read(RegCntX), 0x80);
}

// ---------------------------------------------------------------------------------------------
// Channel 1: duty and sweep
// ---------------------------------------------------------------------------------------------

GBA_TEST(Apu, DutyWaveform)
{
	// GBATEK's duty pictures, in steps of the 8 step waveform: a step of 12.5 % is one high step
	// at the start, 25 % two, 50 % four and 75 % six.
	const int HighSteps[4] = { 1, 2, 4, 6 };

	// 1792 gives a duty step of 16 * (2048 - 1792) = 4096 cycles = 8 host samples, so one full
	// 8 step waveform is exactly 64 samples long.
	const int Frequency = 1792;

	for (int duty = 0; duty < 4; duty++)
	{
		Machine m;
		EnablePsg(m);
		PlayChannel1(m, Frequency, duty, 15);

		std::vector<int16_t> mono = Left(m.Run(SampleCycles * 128));	// two periods
		GBA_CHECK_EQ((int)mono.size(), 128);

		int expectedHigh = HighSteps[duty] * 8;
		for (int i = 0; i < 64; i++)
		{
			bool high = mono[i] > 0;
			bool expected = (i < expectedHigh);
			GBA_CHECK_MSG(high == expected, std::string("duty ") + std::to_string(duty) +
				", sample " + std::to_string(i));
		}

		// One period is 64 samples, so the second half repeats the first one.
		for (int i = 0; i < 64; i++)
			GBA_CHECK_EQ((int)mono[64 + i], (int)mono[i]);
		GBA_CHECK_EQ(MeasurePeriod(mono), 64);

		// The level is the envelope volume times the quarter range on both phases.
		GBA_CHECK_EQ(Peak(mono), 15 * 8 * 33);
	}
}

GBA_TEST(Apu, SweepChangesFrequency)
{
	// The sweep is clocked by frame sequencer steps 2 and 6, i.e. every four steps = 131072
	// cycles (128 Hz). NR10 = 0x79 is direction = decrease, time 7 (the longest) and shift 1, so
	// the timer counts seven sweep clocks before the first calculation: the frequency 1792
	// becomes 896 at t = 98304 + 6 * 131072 = 884736 cycles (GBATEK "SOUND1CNT_L").
	const int SweepClocks = 7;
	const int Span = FrameStep * 4 * SweepClocks;		// 7 sweep clocks = the reload of time 7

	Machine m;
	EnablePsg(m);
	m.Write(RegNR10, (uint8_t)(0x08 | (7 << 4) | 1));		// decrease, time 7, shift 1
	m.Write(RegNR11, 0x80);
	m.Write(RegNR12, 0xF0);
	m.Write(RegNR13, (uint8_t)(1792 & 0xFF));
	m.Write(RegNR14, (uint8_t)(0x80 | (1792 >> 8)));

	// A write to NR13/NR14 that is not a trigger changes the frequency the channel plays at once,
	// but it does NOT reach the sweep unit: the calculation runs on the shadow register (Pan Docs
	// "Pulse channel with sweep"). So the live frequency is 1024 (a period of 256 samples) from
	// here on, and the sweep step that lands on the shadow's 1792 produces 896, not the 512 that
	// a calculation on the live register would give.
	m.Write(RegNR13, (uint8_t)(1024 & 0xFF));
	m.Write(RegNR14, (uint8_t)((1024 >> 8) & 7));

	std::vector<int16_t> afterPoke = Left(m.Run(Span));
	GBA_CHECK_EQ(MeasurePeriod(afterPoke), 256);		// 16 * (2048 - 1024) = 16384 cycles

	std::vector<int16_t> afterSweep = Left(m.Run(Span));
	GBA_CHECK_EQ(MeasurePeriod(afterSweep), 288);		// 16 * (2048 - 896) = 18432 cycles

	std::vector<int16_t> twice = Left(m.Run(Span));
	GBA_CHECK_EQ(MeasurePeriod(twice), 400);			// 16 * (2048 - 448) = 25600 cycles
}

GBA_TEST(Apu, SweepOverflowDisables)
{
	// (a) The immediate frequency calculation of a trigger (Pan Docs "Pulse channel with
	// sweep"): 1500 + 750 overflows the 11 bit frequency, so the channel never starts.
	{
		Machine m;
		EnablePsg(m);
		m.Write(RegNR10, (uint8_t)((1 << 4) | 1));			// increase, time 1, shift 1
		m.Write(RegNR11, 0x80);
		m.Write(RegNR12, 0xF0);
		m.Write(RegNR13, (uint8_t)(1500 & 0xFF));
		m.Write(RegNR14, (uint8_t)(0x80 | (1500 >> 8)));

		GBA_CHECK_EQ(m.Read(RegCntX) & 0x01, 0x00);
		GBA_CHECK(Silent(Left(m.Run(FrameStep * 4))));
	}

	// (b) 1024 passes the immediate check (1536) and the first sweep step writes 1536 back, but
	// the second calculation that the same step runs - 1536 + 768 - overflows and stops the
	// channel.
	{
		Machine m;
		EnablePsg(m);
		m.Write(RegNR10, (uint8_t)((1 << 4) | 1));
		m.Write(RegNR11, 0x80);
		m.Write(RegNR12, 0xF0);
		m.Write(RegNR13, (uint8_t)(1024 & 0xFF));
		m.Write(RegNR14, (uint8_t)(0x80 | (1024 >> 8)));

		GBA_CHECK_EQ(m.Read(RegCntX) & 0x01, 0x01);
		GBA_CHECK(!Silent(Left(m.Run(FrameStep * 3))));	// the sweep step is at the end of this
		GBA_CHECK_EQ(m.Read(RegCntX) & 0x01, 0x00);
		GBA_CHECK(Silent(Left(m.Run(FrameStep * 2))));
	}
}

GBA_TEST(Apu, SweepNegateQuirk)
{
	// Once a subtraction sweep has run, clearing NR10's direction bit disables the channel
	// (Pan Docs "Obscure Behavior"); a trigger clears that state again.
	Machine m;
	EnablePsg(m);
	m.Write(RegNR10, (uint8_t)(0x08 | (1 << 4) | 1));		// decrease, time 1, shift 1
	m.Write(RegNR11, 0x80);
	m.Write(RegNR12, 0xF0);
	m.Write(RegNR13, (uint8_t)(1792 & 0xFF));
	m.Write(RegNR14, (uint8_t)(0x80 | (1792 >> 8)));

	// Four sequencer steps always contain a sweep step (they sit at 2 and 6), so the
	// subtraction 1792 -> 896 has run by the end of this.
	m.Run(FrameStep * 4);
	GBA_CHECK_EQ(m.Read(RegCntX) & 0x01, 0x01);

	m.Write(RegNR10, (uint8_t)((1 << 4) | 1));				// clear the direction bit
	GBA_CHECK_EQ(m.Read(RegCntX) & 0x01, 0x00);

	m.Write(RegNR10, (uint8_t)(0x08 | (1 << 4) | 1));		// decrease again
	m.Write(RegNR14, (uint8_t)(0x80 | (896 >> 8)));			// trigger
	GBA_CHECK_EQ(m.Read(RegCntX) & 0x01, 0x01);

	// The trigger cleared the flag, so clearing the direction bit now leaves the channel on.
	m.Write(RegNR10, (uint8_t)((1 << 4) | 1));
	GBA_CHECK_EQ(m.Read(RegCntX) & 0x01, 0x01);

	// Once the next sweep step has subtracted again, clearing the direction bit disables it.
	m.Write(RegNR10, (uint8_t)(0x08 | (1 << 4) | 1));
	m.Run(FrameStep * 4);
	m.Write(RegNR10, (uint8_t)((1 << 4) | 1));
	GBA_CHECK_EQ(m.Read(RegCntX) & 0x01, 0x00);
}

GBA_TEST(Apu, Envelope)
{
	// NR12 with a step time of 1 and direction = decrease: the volume falls from 15 to 14 at the
	// first 64 Hz envelope step, i.e. after eight frame sequencer steps (Pan Docs "Volume &
	// envelope").
	Machine m;
	EnablePsg(m);
	m.Write(RegNR11, 0x80);
	m.Write(RegNR12, (uint8_t)(0xF0 | 0x01));
	m.Write(RegNR13, (uint8_t)(1792 & 0xFF));
	m.Write(RegNR14, (uint8_t)(0x80 | (1792 >> 8)));

	int before = Peak(Left(m.Run(EnvelopeStep)));
	GBA_CHECK_EQ(before, 15 * 8 * 33);

	int after = Peak(Left(m.Run(EnvelopeStep)));
	GBA_CHECK_EQ(after, 14 * 8 * 33);
	GBA_CHECK_EQ(after * 15, before * 14);

	// Direction = increase from volume 0: the channel is silent until the first step raises it.
	Machine up;
	EnablePsg(up);
	up.Write(RegNR11, 0x80);
	up.Write(RegNR12, (uint8_t)(0x08 | 0x01));				// volume 0, increase, step time 1
	up.Write(RegNR13, (uint8_t)(1792 & 0xFF));
	up.Write(RegNR14, (uint8_t)(0x80 | (1792 >> 8)));

	GBA_CHECK(Silent(Left(up.Run(EnvelopeStep))));
	std::vector<int16_t> raised = Left(up.Run(EnvelopeStep));
	GBA_CHECK(!Silent(raised));
	GBA_CHECK_EQ(Peak(raised), 1 * 8 * 33);
}

GBA_TEST(Apu, LengthCounter)
{
	// NR11 bits 0-5 hold n and the length lasts (64 - n) 256 Hz steps (GBATEK "SOUND1CNT_H"), so
	// n = 63 is a single step: the channel stops at the first length clock, which is frame
	// sequencer step 0 - 32768 cycles after a trigger that left step 0 next.
	Machine m;
	EnablePsg(m);
	m.Write(RegNR11, (uint8_t)(0x80 | 63));
	m.Write(RegNR12, 0xF0);
	m.Write(RegNR13, (uint8_t)(1792 & 0xFF));
	m.Write(RegNR14, (uint8_t)(0x80 | 0x40 | (1792 >> 8)));	// trigger, length enabled

	GBA_CHECK_EQ(m.Read(RegCntX) & 0x01, 0x01);
	GBA_CHECK(!Silent(Left(m.Run(FrameStep))));
	GBA_CHECK_EQ(m.Read(RegCntX) & 0x01, 0x00);
	GBA_CHECK(Silent(Left(m.Run(FrameStep))));

	// The extra length clocking quirk (Pan Docs "Obscure Behavior"): when the trigger lands just
	// before a step that does not clock the length, the length is clocked once right away - but
	// only if it was disabled before. With n = 0 the reload is 64 steps, so the quirk makes the
	// channel die one 256 Hz step earlier.
	//
	// `run` triggers at the given frame sequencer phase and counts the 65536 cycle slices (two
	// sequencer steps, one of which clocks the length) until the channel goes quiet.
	auto run = [](int preTick) -> int
	{
		Machine m;
		EnablePsg(m);
		m.TickSliced(preTick);							// move the sequencer to the wanted step

		m.Write(RegNR11, 0x80);
		m.Write(RegNR12, 0xF0);
		m.Write(RegNR13, (uint8_t)(1792 & 0xFF));
		m.Write(RegNR14, (uint8_t)(0x80 | 0x40 | (1792 >> 8)));
		m.Drain();

		for (int slices = 1; slices <= 200; slices++)
		{
			std::vector<int16_t> mono = Left(m.Run(LengthStep));
			if (Silent(mono))
				return slices;
		}
		return -1;
	};

	int beforeLengthStep = run(FrameStep);				// the next step (1) does not clock
	int beforeOtherStep = run(0);						// the next step (0) clocks
	GBA_CHECK_EQ(beforeLengthStep, beforeOtherStep - 1);
	GBA_CHECK_EQ(beforeOtherStep, 65);					// 64 length steps plus the slice it dies in
}

GBA_TEST(Apu, Channel2)
{
	// Channel 2 has the same duty/envelope/length machinery; only its registers differ. Duty 1
	// is two high steps of the eight (25 %), so 16 of the 64 samples of a period are high.
	Machine m;
	EnablePsg(m);
	m.Write(RegNR21, (uint8_t)(1 << 6));
	m.Write(RegNR22, 0xF0);
	m.Write(RegNR23, (uint8_t)(1792 & 0xFF));
	m.Write(RegNR24, (uint8_t)(0x80 | (1792 >> 8)));

	std::vector<int16_t> mono = Left(m.Run(SampleCycles * 128));	// two periods
	GBA_CHECK_EQ((int)mono.size(), 128);
	for (int i = 0; i < 64; i++)
		GBA_CHECK_MSG((mono[i] > 0) == (i < 16), "sample " + std::to_string(i));
	for (int i = 0; i < 64; i++)
		GBA_CHECK_EQ((int)mono[64 + i], (int)mono[i]);
	GBA_CHECK_EQ(MeasurePeriod(mono), 64);

	// Channel 1 must not sound while channel 2 plays: routing it off in NR51 leaves the output
	// of channel 2 untouched (both are the same waveform, so this is an exact comparison).
	m.Write(RegNR51, 0x0F);								// channels 1-4 to the right only
	std::vector<int16_t> rightOnly = Right(m.Run(SampleCycles * 64));
	GBA_CHECK(Silent(Left(m.Run(SampleCycles * 4))));
	GBA_CHECK(!Silent(rightOnly));

	// NR22's envelope runs for channel 2 too: an increase from 0 becomes audible after the first
	// 64 Hz step.
	Machine env;
	EnablePsg(env);
	env.Write(RegNR21, (uint8_t)(2 << 6));
	env.Write(RegNR22, 0x08 | 0x01);					// volume 0, increase, step time 1
	env.Write(RegNR23, (uint8_t)(1792 & 0xFF));
	env.Write(RegNR24, (uint8_t)(0x80 | (1792 >> 8)));
	GBA_CHECK(Silent(Left(env.Run(EnvelopeStep))));
	GBA_CHECK(!Silent(Left(env.Run(EnvelopeStep))));
}

// ---------------------------------------------------------------------------------------------
// Channel 3: the wave RAM
// ---------------------------------------------------------------------------------------------

GBA_TEST(Apu, WavePlayback)
{
	// 1984 gives one digit every 8 * (2048 - 1984) = 512 cycles, i.e. exactly one host sample
	// per digit (GBATEK "SOUND3CNT_X": 2097152/(2048-n) digits per second).
	const int Frequency = 1984;

	Machine m;
	EnablePsg(m);
	WriteWaveRam(m, RampDigits, 32);
	m.Write(RegNR32, 0x20);								// volume 100 %
	m.Write(RegNR30, 0x80);								// DAC on, dimension 0, bank 0
	m.Write(RegNR33, (uint8_t)(Frequency & 0xFF));
	m.Write(RegNR34, (uint8_t)(0x80 | (Frequency >> 8)));

	std::vector<int16_t> pass = Left(m.Run(SampleCycles * 32));
	GBA_CHECK_EQ((int)pass.size(), 32);

	// The DAC is linear in the 4 bit digit (GBATEK: each PSG spans a quarter of the output
	// range), so the rising ramp of digits 0..15 gives a constant difference, and digit 16 is
	// digit 0 again.
	int step = pass[1] - pass[0];
	GBA_CHECK(step > 0);
	for (int i = 0; i < 15; i++)
		GBA_CHECK_EQ(pass[i + 1] - pass[i], step);
	GBA_CHECK_EQ((int)pass[16], (int)pass[0]);
	GBA_CHECK_EQ((int)pass[31], (int)pass[15]);
	GBA_CHECK(pass[15] > pass[0]);

	// The 32 digit pattern loops: the next pass is identical.
	std::vector<int16_t> again = Left(m.Run(SampleCycles * 32));
	GBA_CHECK_EQ((int)again.size(), 32);
	for (int i = 0; i < 32; i++)
		GBA_CHECK_EQ((int)again[i], (int)pass[i]);

	// NR30 bit 5 = 1 plays both banks: the loop is 64 digits long and, because the header can
	// only store one bank, the same 32 digits appear twice (see the deviation note in MixWave).
	m.Write(RegNR30, 0xA0);
	m.Write(RegNR34, (uint8_t)(0x80 | (Frequency >> 8)));
	std::vector<int16_t> both = Left(m.Run(SampleCycles * 64));
	GBA_CHECK_EQ((int)both.size(), 64);
	for (int i = 0; i < 64; i++)
		GBA_CHECK_EQ((int)both[i], (int)pass[i % 32]);

	// Bank 1 plays the same 16 bytes: the frozen header has one 16 byte array, so the two banks
	// alias (documented deviation).
	m.Write(RegNR30, 0xC0);
	m.Write(RegNR34, (uint8_t)(0x80 | (Frequency >> 8)));
	std::vector<int16_t> bank1 = Left(m.Run(SampleCycles * 32));
	GbaTest::Note("channel 3 bank 1 aliases bank 0: gba_apu.h stores a single 16 byte wave RAM");
	for (int i = 0; i < 32; i++)
		GBA_CHECK_EQ((int)bank1[i], (int)pass[i]);

	// NR32 bits 5-6 scale the digital value: 50 % and 25 %, and bit 7 forces 75 % (GBATEK
	// "SOUND3CNT_H"). The peak of the pattern is digit 15, so the ratios are exact.
	struct VolumeCase { uint8_t nr32; int numerator; int denominator; };
	const VolumeCase cases[3] = { { 0x40, 1, 2 }, { 0x60, 1, 4 }, { 0x80, 3, 4 } };

	for (int i = 0; i < 3; i++)
	{
		m.Write(RegNR30, 0x80);
		m.Write(RegNR32, cases[i].nr32);
		m.Write(RegNR34, (uint8_t)(0x80 | (Frequency >> 8)));
		int peak = Peak(Left(m.Run(SampleCycles * 32)));
		GBA_CHECK_EQ(peak * cases[i].denominator, Peak(pass) * cases[i].numerator);
	}

	// NR32 = 0 is the mute setting.
	m.Write(RegNR32, 0x00);
	m.Write(RegNR34, (uint8_t)(0x80 | (Frequency >> 8)));
	GBA_CHECK(Silent(Left(m.Run(SampleCycles * 32))));
}

GBA_TEST(Apu, WaveTimerSampling)
{
	// SOUND3CNT_X bit 14 selects the wave channel's clock source on the GBA. It also carries the
	// length flag, so this test sets it and selects timer 1: with TM1CNT_L = F000h the timer
	// overflows every 4096 cycles, i.e. one wave digit every 4096/512 = 8 host samples.
	const int Frequency = 1984;							// the register rate would be 1 sample/digit

	Machine m;
	EnablePsg(m);
	WriteWaveRam(m, RampDigits, 32);
	m.Write(RegNR31, 0x00);								// 256 length steps, far away
	m.Write(RegNR32, 0x20);
	m.Write(RegNR30, 0x80);
	m.Write(RegNR33, (uint8_t)(Frequency & 0xFF));

	m.bus.timers.Write16(m.bus, 0x104, 0xF000);			// TM1CNT_L
	m.bus.timers.Write16(m.bus, 0x106, 0x0080);			// TM1CNT_H: prescaler 1, running

	m.Write(RegNR34, (uint8_t)(0x80 | 0x40 | (Frequency >> 8)));	// trigger + length flag = timer 1

	std::vector<int16_t> mono = Left(m.Run(SampleCycles * 64));
	GBA_CHECK_EQ((int)mono.size(), 64);

	// Every digit lasts exactly eight samples, so the output is a staircase in steps of eight.
	int runs = 0;
	for (int i = 1; i < 64; i++)
	{
		if (mono[i] != mono[i - 1])
		{
			GBA_CHECK_EQ(i % 8, 0);
			runs++;
		}
	}
	GBA_CHECK_EQ(runs, 7);

	// With the bit clear the channel takes timer 0 instead, which is not running here, so the
	// digit clock falls back to NR33 - one digit per host sample.
	m.bus.timers.Write16(m.bus, 0x106, 0x0000);			// stop timer 1
	m.Write(RegNR34, (uint8_t)(0x80 | (Frequency >> 8)));
	std::vector<int16_t> fast = Left(m.Run(SampleCycles * 8));
	GBA_CHECK_EQ((int)fast.size(), 8);
	for (int i = 1; i < 8; i++)
		GBA_CHECK(fast[i] != fast[i - 1]);
}

// ---------------------------------------------------------------------------------------------
// Channel 4: the noise generator
// ---------------------------------------------------------------------------------------------

GBA_TEST(Apu, NoiseLfsr)
{
	// r = 4 and s = 1: the divider is 64, so one LFSR clock lasts 64 * 4 * 2^1 = 512 cycles =
	// exactly one host sample (GBATEK "GBA Sound Channel 4": 524288/r/2^(s+1) Hz).
	const int Period = 512;

	struct Mode { int width; int nr43; };
	const Mode modes[2] = { { 0, (1 << 4) | 4 }, { 1, (1 << 4) | 4 | 0x08 } };

	for (int i = 0; i < 2; i++)
	{
		Machine m;
		EnablePsg(m);
		m.Write(RegNR42, 0xF0);							// volume 15, envelope off
		m.Write(RegNR43, (uint8_t)modes[i].nr43);
		m.Write(RegNR44, 0x80);							// trigger

		std::vector<int16_t> mono = Left(m.Run(SampleCycles * 40));
		GBA_CHECK_EQ((int)mono.size(), 40);

		std::vector<int> expected = NoiseLevels(modes[i].width, Period, 40);
		for (int k = 0; k < 40; k++)
			GBA_CHECK_MSG((mono[k] > 0) == (expected[k] != 0),
				"width " + std::to_string(modes[i].width) + ", sample " + std::to_string(k));

		// Hand derived from the GBATEK rule: the register starts at 4000h (40h in 7 bit mode),
		// whose bit 0 is clear, so the first set carry comes out of the clock that shifts the
		// last set bit off - the 15th clock in 15 bit mode and the 7th in 7 bit mode.
		int firstHigh = modes[i].width ? 7 : 15;
		for (int k = 0; k < firstHigh; k++)
			GBA_CHECK_MSG(expected[k] == 0, "sample " + std::to_string(k));
		GBA_CHECK_EQ(expected[firstHigh], 1);

		// The level is the envelope volume times the quarter range.
		GBA_CHECK_EQ(Peak(mono), 15 * 8 * 33);
	}
}

GBA_TEST(Apu, NoiseDividerTable)
{
	// GBATEK "GBA Sound Channel 4": the divider is 8, 16, 32, 48, 64, 80, 96, 112 for r = 0..7
	// and the clock shift divides by 2^s. With s = 3 one LFSR clock lasts divisor * 4 * 8
	// cycles, which for divisor/16 host samples is 1, 2, 3, 4, 5, 6, 7 and (r = 0) 0.5 samples
	// per clock - the output has to follow the recomputed sequence exactly.
	const int Divider[8] = { 8, 16, 32, 48, 64, 80, 96, 112 };

	for (int r = 0; r < 8; r++)
	{
		int period = Divider[r] * 4 * 8;
		int samples = 48;

		Machine m;
		EnablePsg(m);
		m.Write(RegNR42, 0xF0);
		m.Write(RegNR43, (uint8_t)((3 << 4) | r));
		m.Write(RegNR44, 0x80);

		std::vector<int16_t> mono = Left(m.Run(SampleCycles * samples));
		GBA_CHECK_EQ((int)mono.size(), samples);

		std::vector<int> expected = NoiseLevels(0, period, samples);
		for (int k = 0; k < samples; k++)
			GBA_CHECK_MSG((mono[k] > 0) == (expected[k] != 0),
				"r " + std::to_string(r) + ", sample " + std::to_string(k));
	}

	// A clock shift of 14 or 15 stops the LFSR, so the level never changes (Pan Docs "Obscure
	// Behavior").
	Machine m;
	EnablePsg(m);
	m.Write(RegNR42, 0xF0);
	m.Write(RegNR43, (uint8_t)((14 << 4) | 4));
	m.Write(RegNR44, 0x80);
	std::vector<int16_t> mono = Left(m.Run(SampleCycles * 16));
	for (size_t i = 1; i < mono.size(); i++)
		GBA_CHECK_EQ((int)mono[i], (int)mono[0]);
}

GBA_TEST(Apu, NoiseEnvelope)
{
	// NR42's envelope runs on the same 64 Hz clock as the square channels.
	Machine m;
	EnablePsg(m);
	m.Write(RegNR42, (uint8_t)(0xF0 | 0x01));				// volume 15, decrease, step time 1
	m.Write(RegNR43, (uint8_t)((1 << 4) | 4));
	m.Write(RegNR44, 0x80);

	int before = Peak(Left(m.Run(EnvelopeStep)));
	GBA_CHECK_EQ(before, 15 * 8 * 33);
	int after = Peak(Left(m.Run(EnvelopeStep)));
	GBA_CHECK_EQ(after, 14 * 8 * 33);
}

// ---------------------------------------------------------------------------------------------
// The FIFO channels
// ---------------------------------------------------------------------------------------------

namespace
{
	// SOUNDCNT_H setup for the FIFO tests: `volume` and `sides` belong to FIFO A, the PSG
	// volume stays at 100 % (bits 0-1 = 2).
	void SetupFifoA(Machine& m, int timer, bool right, bool left, int volume)
	{
		m.Write(RegCntX, 0x80);
		m.Write(RegNR50, 0x77);
		m.Write(RegLow, (uint8_t)(0x02 | (volume << 2)));
		m.Write(RegHigh, (uint8_t)((right ? 0x01 : 0) | (left ? 0x02 : 0) | (timer << 2)));
	}

	// TM0CNT_L with prescaler 1: the counter runs from `reload` to FFFFh and wraps, so it
	// overflows every 0x10000 - reload cycles (GBATEK "GBA Timers").
	void StartTimer(Machine& m, int index, uint16_t reload)
	{
		m.bus.timers.Write16(m.bus, 0x100 + index * 4, reload);
		m.bus.timers.Write16(m.bus, 0x102 + index * 4, 0x0080);
	}
}

GBA_TEST(Apu, FifoDmaRefill)
{
	// Timer 0 overflows every 2048 cycles, i.e. every four host samples: one FIFO byte per
	// overflow (GBATEK "DMA-Sound Playback Procedure").
	Machine m;
	SetupFifoA(m, 0, true, true, 1);
	StartTimer(m, 0, 0xF800);

	// The DMA moves four words; byte 0 of the first word is replayed first (GBATEK "FIFO_A_L":
	// "Data 0 being located in least significant byte which is replayed first").
	const uint32_t words[4] = { 0x04030201u, 0x08070605u, 0x0C0B0A09u, 0x100F0E0Du };
	m.apu().FifoDmaDone(0, words, 4);

	// Four words is 16 bytes, which is not "fewer than 16", so no refill is asked for yet.
	GBA_CHECK(!m.apu().FifoRequest(0));
	GBA_CHECK_EQ(m.Read(RegFifoA), 0x01);				// the next byte is the first one pushed

	// One byte is played every 2048 cycles, so after 2048 cycles 15 bytes are left of the 16.
	std::vector<int16_t> mono = Left(m.Run(2048));
	GBA_CHECK_EQ((int)mono.size(), 4);
	GBA_CHECK(m.apu().FifoRequest(0));

	// The audio must replay the bytes in order: 1, 2, 3, 4 here, one per timer overflow and
	// held until the next one. The first byte is already playing in sample 3 (the overflow
	// happens while that sample is mixed), so three more slices add the rest.
	std::vector<int16_t> more = Left(m.Run(2048 * 3));
	mono.insert(mono.end(), more.begin(), more.end());
	GBA_CHECK_EQ((int)mono.size(), 16);
	int first = mono[3];
	GBA_CHECK(first > 0);
	for (int k = 1; k < 4; k++)
		GBA_CHECK_EQ((int)mono[3 + 4 * k], first * (k + 1));

	// The next four words move the queue back above the threshold and clear the request.
	m.apu().FifoDmaDone(0, words, 4);
	GBA_CHECK(!m.apu().FifoRequest(0));
	GBA_CHECK_EQ(m.Read(RegFifoA), 0x05);				// byte 5 is next

	// ClearFifoRequest is what the DMA engine calls after servicing: it must drop the flag, and
	// the request has to come back once the queue is below 16 bytes again (31 - 16 = 15 here).
	m.Run(2048 * 16);
	GBA_CHECK(m.apu().FifoRequest(0));
	m.apu().ClearFifoRequest(0);
	GBA_CHECK(!m.apu().FifoRequest(0));

	// A FIFO write that does not fit is lost rather than wrapped around.
	Machine full;
	SetupFifoA(full, 0, true, true, 1);
	for (int i = 0; i < 40; i++)
		full.Write(RegFifoA, (uint8_t)(i + 1));
	GBA_CHECK_EQ(full.Read(RegFifoA), 0x01);			// the oldest byte is still at the head
}

GBA_TEST(Apu, FifoFasterThanTheHostRate)
{
	// A FIFO byte is moved per timer overflow, and the timer may be faster than the host's own
	// sample rate: TM0CNT_L = FF00h wraps every 256 cycles, i.e. twice per 512 cycle host sample
	// (65536 Hz against 32768). Both bytes have to be consumed - taking one would play the sample
	// at half its rate, an octave down - and the period dividing the slice exactly must not hide
	// the wraps the way a plain comparison of two counter readings does (the counter is back at
	// FF00h after every second wrap).
	Machine m;
	SetupFifoA(m, 0, true, true, 1);
	StartTimer(m, 0, 0xFF00);

	const uint32_t words[4] = { 0x04030201u, 0x08070605u, 0x0C0B0A09u, 0x100F0E0Du };
	m.apu().FifoDmaDone(0, words, 4);

	// The first clock of the FIFO only takes the timer's total (the byte that is playing is the
	// one a previous overflow put there).
	std::vector<int16_t> warmup = Left(m.Run(SampleCycles));
	GBA_CHECK_EQ((int)warmup.size(), 1);
	GBA_CHECK_EQ((int)warmup[0], 0);
	GBA_CHECK_EQ(m.Read(RegFifoA), 0x01);

	// Three samples, two overflows each: the latch walks 2, 4, 6 and byte 7 is the next one.
	std::vector<int16_t> mono = Left(m.Run(SampleCycles * 3));
	GBA_CHECK_EQ((int)mono.size(), 3);
	GBA_CHECK_EQ((int)mono[0], 2 * 2 * 33);
	GBA_CHECK_EQ((int)mono[1], 4 * 2 * 33);
	GBA_CHECK_EQ((int)mono[2], 6 * 2 * 33);
	GBA_CHECK_EQ(m.Read(RegFifoA), 0x07);
}

GBA_TEST(Apu, FifoVolumeAndPanning)
{
	// One byte every 2048 cycles; the byte is +100, so the level is easy to compare.
	const uint32_t words[4] = { 0x64646464u, 0x64646464u, 0x64646464u, 0x64646464u };

	auto run = [&](int volume, bool right, bool left) -> std::vector<int16_t>
	{
		Machine m;
		SetupFifoA(m, 0, right, left, volume);
		StartTimer(m, 0, 0xF800);
		m.apu().FifoDmaDone(0, words, 4);
		m.Run(2048);									// the first byte is played
		return m.Run(2048);
	};

	std::vector<int16_t> full = run(1, true, true);
	GBA_CHECK(!Silent(full));
	int peak100 = Peak(Left(full));
	int peak50 = Peak(Left(run(0, true, true)));
	GBA_CHECK_EQ(peak100, 100 * 2 * 33);
	GBA_CHECK_EQ(peak50 * 2, peak100);

	// SOUNDCNT_H bit 8 routes FIFO A to the right, bit 9 to the left; a FIFO that is routed
	// nowhere is silent and stops consuming its data.
	std::vector<int16_t> rightOnly = run(1, true, false);
	GBA_CHECK(!Silent(Right(rightOnly)));
	GBA_CHECK(Silent(Left(rightOnly)));

	std::vector<int16_t> leftOnly = run(1, false, true);
	GBA_CHECK(!Silent(Left(leftOnly)));
	GBA_CHECK(Silent(Right(leftOnly)));

	std::vector<int16_t> nowhere = run(1, false, false);
	GBA_CHECK(Silent(nowhere));
}

GBA_TEST(Apu, FifoTimerSelect)
{
	// SOUNDCNT_H bit 10 selects the timer: with it set FIFO A is clocked by timer 1. Only timer
	// 0 runs here, so the FIFO must not lose any byte; starting timer 1 gets it going.
	Machine m;
	SetupFifoA(m, 1, true, true, 1);
	StartTimer(m, 0, 0xF800);							// overflows every 2048 cycles
	StartTimer(m, 1, 0xF000);							// overflows every 4096 cycles
	m.bus.timers.Write16(m.bus, 0x106, 0x0000);			// ... but keep timer 1 stopped

	const uint32_t words[4] = { 0x04030201u, 0x08070605u, 0x0C0B0A09u, 0x100F0E0Du };
	m.apu().FifoDmaDone(0, words, 4);

	std::vector<int16_t> idle = m.Run(2048 * 4);
	GBA_CHECK(Silent(idle));
	GBA_CHECK_EQ(m.Read(RegFifoA), 0x01);				// nothing was consumed

	// Start timer 1: now the bytes are played, one per 4096 cycles.
	m.bus.timers.Write16(m.bus, 0x106, 0x0080);
	std::vector<int16_t> playing = m.Run(4096 * 2);
	GBA_CHECK(!Silent(playing));
	GBA_CHECK_EQ(m.Read(RegFifoA), 0x03);
}

// ---------------------------------------------------------------------------------------------
// The mixer
// ---------------------------------------------------------------------------------------------

GBA_TEST(Apu, MasterVolumeAndPanning)
{
	// NR50 bits 0-2 (right) and 4-6 (left) scale the mixed signal; 0 mutes the side and 7 is
	// 100 %, so volume 3 gives half of volume 7 (GBATEK "SOUNDCNT_L").
	auto run = [](uint8_t nr50, uint8_t nr51, uint8_t cntl) -> std::vector<int16_t>
	{
		Machine m;
		m.Write(RegCntX, 0x80);
		m.Write(RegNR50, nr50);
		m.Write(RegNR51, nr51);
		m.Write(RegLow, cntl);
		PlayChannel1(m, 1792, 2, 15);
		return m.Run(SampleCycles * 64);
	};

	std::vector<int16_t> full = run(0x77, 0xFF, 0x02);
	GBA_CHECK_EQ(Peak(Left(full)), 15 * 8 * 33);
	GBA_CHECK_EQ(Peak(Right(full)), 15 * 8 * 33);

	// Master volume 0 is silence on both sides.
	GBA_CHECK(Silent(run(0x00, 0xFF, 0x02)));

	// Half volume: (3 + 1) / 8 of full scale.
	GBA_CHECK_EQ(Peak(Left(run(0x33, 0xFF, 0x02))) * 2, Peak(Left(full)));

	// Separate left and right volumes.
	std::vector<int16_t> split = run(0x07, 0xFF, 0x02);		// left 0 (mute), right 7
	GBA_CHECK(Silent(Left(split)));
	GBA_CHECK(!Silent(Right(split)));

	// NR51: bit 8 routes channel 1 to the right, bit 12 to the left.
	std::vector<int16_t> rightOnly = run(0x77, 0x01, 0x02);
	GBA_CHECK(Silent(Left(rightOnly)));
	GBA_CHECK(!Silent(Right(rightOnly)));

	std::vector<int16_t> leftOnly = run(0x77, 0x10, 0x02);
	GBA_CHECK(!Silent(Left(leftOnly)));
	GBA_CHECK(Silent(Right(leftOnly)));

	GBA_CHECK(Silent(run(0x77, 0x00, 0x02)));			// routed nowhere

	// SOUNDCNT_H bits 0-1 scale the four PSGs: 0 = 25 %, 1 = 50 %, 2 = 100 %.
	GBA_CHECK_EQ(Peak(Left(run(0x77, 0xFF, 0x00))) * 4, Peak(Left(full)));
	GBA_CHECK_EQ(Peak(Left(run(0x77, 0xFF, 0x01))) * 2, Peak(Left(full)));
}

GBA_TEST(Apu, SampleRateCount)
{
	// The mixer has to produce one sample per 1/sampleRate seconds for any host rate: over one
	// second that is exactly `sampleRate` samples, and over a 60th of a second it is within one
	// sample of sampleRate/60.
	const int rates[3] = { 8000, 32768, 48000 };

	for (int i = 0; i < 3; i++)
	{
		Machine m;
		m.apu().SetSampleRate(rates[i]);
		EnablePsg(m);
		PlayChannel1(m, 1792, 2, 15);

		int perSecond = (int)m.Run(CyclesPerSecond).size() / 2;
		GBA_CHECK_MSG(perSecond == rates[i], "rate " + std::to_string(rates[i]) +
			": " + std::to_string(perSecond) + " samples in one second");

		Machine frame;
		frame.apu().SetSampleRate(rates[i]);
		EnablePsg(frame);
		PlayChannel1(frame, 1792, 2, 15);
		int perFrame = (int)frame.Run(CyclesPerSecond / 60).size() / 2;
		int expected = rates[i] / 60;
		int difference = perFrame - expected;
		GBA_CHECK_MSG(difference >= -1 && difference <= 1, "rate " + std::to_string(rates[i]) +
			": " + std::to_string(perFrame) + " samples per frame");
	}
}

GBA_TEST(Apu, ReadSamplesPartial)
{
	// Two identical runs: one drains the queue in a single call, the other one five frames at a
	// time. The concatenation must be the same stream, so nothing is duplicated or lost.
	int16_t whole[512];
	int total = 0;

	{
		Machine m;
		EnablePsg(m);
		PlayChannel1(m, 1792, 2, 15);
		m.TickSliced(SampleCycles * 40);
		total = m.apu().ReadSamples(whole, 256);
		GBA_CHECK_EQ(total, 40);
		GBA_CHECK_EQ(m.apu().ReadSamples(whole, 256), 0);	// nothing left
	}

	Machine m;
	EnablePsg(m);
	PlayChannel1(m, 1792, 2, 15);
	m.TickSliced(SampleCycles * 40);

	int16_t buffer[10];
	int index = 0;

	for (;;)
	{
		int frames = m.apu().ReadSamples(buffer, 5);
		if (frames <= 0)
			break;

		for (int i = 0; i < frames * 2; i++)
		{
			GBA_CHECK(index < total * 2);
			GBA_CHECK_EQ((int)buffer[i], (int)whole[index]);
			index++;
		}
	}

	GBA_CHECK_EQ(index, total * 2);

	// A queue that is drained exactly keeps the rest: the first call returns all of it and the
	// second one nothing (checked above), and a call for more frames than are queued returns
	// only what exists.
	Machine exact;
	EnablePsg(exact);
	PlayChannel1(exact, 1792, 2, 15);
	exact.TickSliced(SampleCycles * 8);
	int16_t big[100 * 2];		// room for 100 stereo frames
	GBA_CHECK_EQ(exact.apu().ReadSamples(big, 100), 8);
}

// ---------------------------------------------------------------------------------------------
// One video frame of sound (the frontend drains the mixer once per frame)
// ---------------------------------------------------------------------------------------------

GBA_TEST(Apu, WholeFrameSampleCount)
{
	// A frame of the GBA is 228 lines of 1232 cycles = 280896 system cycles, which is 548.625 host
	// samples at 32768 Hz. The mixer has to split that into whole samples - 548 and 549 of them -
	// because the frontend hands the device everything the frame produced: an off-by-one here is a
	// click every frame.
	const int FrameCycles = 228 * 1232;
	const int Frames = 8;

	Machine m;
	EnablePsg(m);
	PlayChannel1(m, 1792, 2, 15);			// a plain square: no envelope, no length, no sweep

	int total = 0;
	int shortFrames = 0;

	for (int frame = 0; frame < Frames; frame++)
	{
		m.TickSliced(FrameCycles, 64);		// the bus advances the APU in slices of 64 cycles
		int frames = (int)m.Drain().size() / 2;

		GBA_CHECK_MSG(frames == 548 || frames == 549,
			"frame " + std::to_string(frame) + ": " + std::to_string(frames) + " samples");

		total += frames;

		if (frames == 548)
			shortFrames++;
	}

	// Eight frames are 8 * 280896 / 512 = 4389 samples exactly: five frames of 549 and three of
	// 548, and the fractional part never accumulates into a lost or duplicated sample.
	GBA_CHECK_EQ(total, 4389);
	GBA_CHECK_EQ(shortFrames, 3);
}

GBA_TEST(Apu, PerFrameDrainKeepsTheStreamWhole)
{
	// The frontend drains the mixer after every frame it runs. The samples must come out as one
	// continuous stream across those boundaries: two identical runs, one drained after every frame
	// and one drained only at the end, have to produce exactly the same samples in the same order.
	const int FrameCycles = 228 * 1232;
	const int Frames = 6;

	auto run = [FrameCycles, Frames](bool perFrame) -> std::vector<int16_t>
	{
		Machine m;
		EnablePsg(m);
		PlayChannel1(m, 1792, 2, 15);

		std::vector<int16_t> out;

		for (int frame = 0; frame < Frames; frame++)
		{
			m.TickSliced(FrameCycles, 64);

			if (perFrame)
			{
				std::vector<int16_t> part = m.Drain();
				out.insert(out.end(), part.begin(), part.end());
			}
		}

		if (!perFrame)
			out = m.Drain();

		return out;
	};

	std::vector<int16_t> drainedPerFrame = run(true);
	std::vector<int16_t> drainedAtOnce = run(false);

	// Six frames are 6 * 280896 / 512 = 3291.75, i.e. 3291 samples and not 3292.
	GBA_CHECK_EQ((int)drainedPerFrame.size(), 3291 * 2);
	GBA_CHECK_EQ((int)drainedAtOnce.size(), (int)drainedPerFrame.size());

	for (size_t i = 0; i < drainedPerFrame.size(); i++)
		GBA_CHECK_MSG(drainedPerFrame[i] == drainedAtOnce[i], "sample " + std::to_string(i));
}
