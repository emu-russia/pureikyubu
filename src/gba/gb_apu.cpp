// The Game Boy sound controller. See gb_apu.h for the specification pages this is written from
// and for the parts that are deliberately not modelled.
//
// The rate logic is worth reading once: every channel is driven by a down counter reloaded from
// its frequency register, exactly as the hardware does, and the frame sequencer ticks every 512
// system clocks (the DIV-APU clock, 512 Hz, which does not change in double speed mode). The mixer
// then samples the four digital outputs at the host's rate and puts them through the high pass
// filter the Pan Docs' reference implementation describes.

#include "gb_apu.h"

#include <cmath>

namespace GBA
{
	namespace
	{
		// The 512 Hz frame sequencer (Pan Docs "Audio Details": the envelope is clocked every 8
		// ticks, the length every 2 and the sweep every 4, which gives this eight step table).
		// The value is a bit mask: 1 = clock the lengths, 2 = clock the sweep, 4 = the envelope.
		const u8 FrameSequencer[8] =
		{
			1,		// step 0: length
			1,		// step 1: length
			3,		// step 2: length + sweep
			1,		// step 3: length
			1,		// step 4: length
			1,		// step 5: length
			3,		// step 6: length + sweep
			5,		// step 7: length + envelope
		};

		// The duty waveforms (Pan Docs "Audio Registers"): a 1 bit is a high output. The waveform
		// is a bit mask whose bit 0 is the first step of the eight.
		const u8 DutyWaveform[4] = { 0x01, 0x03, 0x0F, 0xFC };

		// The noise divisor table (Pan Docs "Audio Registers"): code 0 means 0.5, so the effective
		// divisors are 0.5, 1, 2, 3, 4, 5, 6, 7.
		const int NoiseDivisors[8] = { 1, 1, 2, 3, 4, 5, 6, 7 };
	}

	int GbApu::DutyBit(int duty, int step)
	{
		return (DutyWaveform[duty & 0x03] >> (step & 0x07)) & 1;
	}

	void GbApu::Reset()
	{
		pulse[0] = Pulse{};
		pulse[1] = Pulse{};
		wave = Wave{};
		noise = Noise{};

		// The post-boot values (Pan Docs "Power Up Sequence": NR50 = 0x77, NR51 = 0xF3).
		nr50 = 0x77;
		nr51 = 0xF3;
		powered = true;

		for (int i = 0; i < 16; i++)
			waveRam[i] = 0x00;		// the boot ROM does not initialise wave RAM

		frameSequencerCycles = 0;
		frameStep = 0;
		sampleCycles = 0;
		capacitorLeft = 0.0;
		capacitorRight = 0.0;
		pending.clear();

		SetSampleRate(sampleRate);
	}

	void GbApu::SetSampleRate(int hz)
	{
		if (hz < 8000)
			hz = 8000;
		if (hz > 192000)
			hz = 192000;
		sampleRate = hz;

		// The sample loop runs on the 4.194304 MHz clock; the host rate picks how many clocks
		// fall between two output samples.
		cyclesPerSample = GbCyclesPerSecond / sampleRate;
		if (cyclesPerSample < 1)
			cyclesPerSample = 1;

		// The Pan Docs' reference high pass filter uses 0.999958 at 4194304 Hz; rebasing it for
		// the host rate (the document's own formula) keeps the cutoff where the hardware has it.
		highPassCharge = std::pow(0.999958, (double)GbCyclesPerSecond / (double)sampleRate);
	}

	// ---------------------------------------------------------------------------------------
	// Running
	// ---------------------------------------------------------------------------------------

	void GbApu::Tick(int cycles)
	{
		if (cycles <= 0)
			return;

		for (int i = 0; i < cycles; i++)
		{
			// The frame sequencer: one step every 512 system clocks, i.e. 512 Hz (Pan Docs
			// "Audio Details"). Its rate does not change in double speed mode (the DIV-APU
			// divider does), which is why it is driven from the system clock here: the machine
			// feeds the APU with the clocks the rest of the system also sees.
			if (++frameSequencerCycles >= 512)
			{
				frameSequencerCycles = 0;
				ClockFrameSequencer();
			}

			TickPulse(pulse[0]);
			TickPulse(pulse[1]);
			TickWaveChannel();
			TickNoiseChannel();

			if (++sampleCycles >= cyclesPerSample)
			{
				sampleCycles = 0;
				MixSample();
			}
		}
	}

	void GbApu::ClockFrameSequencer()
	{
		u8 clocks = FrameSequencer[frameStep];
		frameStep = (frameStep + 1) & 0x07;

		if (clocks & 1)
			ClockLengths();
		if (clocks & 2)
			ClockSweep(0);
		if (clocks & 4)
			ClockEnvelopes();
	}

	// ---------------------------------------------------------------------------------------
	// The channel counters
	// ---------------------------------------------------------------------------------------

	void GbApu::TickPulse(Pulse& channel)
	{
		// A pulse channel's timer is clocked at 1 MHz and the duty waveform has eight steps, so
		// one step takes (2048 - frequency) * 2 system clocks - which is the rate the Pan Docs
		// give as 1048576 / (2048 - x) Hz.
		if (!channel.active || !channel.dacEnabled || --channel.timer > 0)
			return;

		ReloadPulseTimer(channel);
		channel.dutyStep = (channel.dutyStep + 1) & 0x07;
	}

	void GbApu::TickWaveChannel()
	{
		// The wave channel has 32 samples and its timer runs at 2 MHz, so one sample lasts
		// (2048 - frequency) system clocks (Pan Docs: 2097152 / (2048 - x) Hz).
		if (!wave.active || !wave.dacEnabled || --wave.timer > 0)
			return;

		ReloadWaveTimer();
		wave.position = (wave.position + 1) & 0x1F;

		// The wave RAM holds 32 four-bit samples, the high nibble of a byte first (Pan Docs
		// "Audio Registers").
		u8 byte = waveRam[wave.position >> 1];
		wave.sampleBuffer = (wave.position & 1) ? (byte & 0x0F) : (byte >> 4);
	}

	void GbApu::TickNoiseChannel()
	{
		// Pan Docs "Audio Registers": the LFSR is clocked at 262144 / r / 2^(s+1) Hz, where r is
		// the divisor code (code 0 meaning a divisor of 0.5) and s the clock shift. Clock shifts
		// 14 and 15 stop the LFSR altogether.
		if (!noise.active || !noise.dacEnabled || noise.shift >= 14 || --noise.timer > 0)
			return;

		noise.timer = NoisePeriod();

		// The LFSR: bits 0 and 1 are XORed into bit 14, then the register shifts right; in the
		// 7 bit mode the same bit is also written into bit 6 (Pan Docs "Audio Registers").
		u16 xorBit = (u16)((noise.lfsr ^ (noise.lfsr >> 1)) & 1);
		noise.lfsr = (u16)((noise.lfsr >> 1) | (xorBit << 14));
		if (noise.widthMode)
			noise.lfsr = (u16)((noise.lfsr & ~0x0040) | (xorBit << 6));
	}

	int GbApu::NoisePeriod() const
	{
		// The period in system clocks: 4194304 / (262144 / r / 2^(s+1)) = 16 * r * 2^(s+1); with
		// r = 0 meaning 0.5 the period is 8 * 2^(s+1) instead.
		int factor = (noise.divisorCode == 0) ? 8 : 16 * NoiseDivisors[noise.divisorCode & 0x07];
		return factor << noise.shift;
	}

	// ---------------------------------------------------------------------------------------
	// The clocks
	// ---------------------------------------------------------------------------------------

	void GbApu::ClockLengths()
	{
		// The length counters tick at 256 Hz and only while the channel's length enable bit is
		// set; the channel is switched off when the counter reaches its maximum (Pan Docs
		// "Audio Details": 64 for the pulse and noise channels, 256 for the wave channel).
		if (pulse[0].lengthEnabled && pulse[0].lengthCounter > 0 && ++pulse[0].lengthCounter > 64)
		{
			pulse[0].lengthCounter = 64;
			pulse[0].active = false;
		}
		if (pulse[1].lengthEnabled && pulse[1].lengthCounter > 0 && ++pulse[1].lengthCounter > 64)
		{
			pulse[1].lengthCounter = 64;
			pulse[1].active = false;
		}
		if (wave.lengthEnabled && wave.lengthCounter > 0 && ++wave.lengthCounter > 256)
		{
			wave.lengthCounter = 256;
			wave.active = false;
		}
		if (noise.lengthEnabled && noise.lengthCounter > 0 && ++noise.lengthCounter > 64)
		{
			noise.lengthCounter = 64;
			noise.active = false;
		}
	}

	void GbApu::ClockEnvelopes()
	{
		// The envelope ticks at 64 Hz, one step every `period` ticks. A period of zero disables
		// the envelope, and a volume of zero never switches the channel off (Pan Docs "Audio").
		Pulse* squares[2] = { &pulse[0], &pulse[1] };
		for (Pulse* channel : squares)
		{
			if (!channel->envelopeRunning || channel->envelopePeriod == 0)
				continue;
			if (--channel->envelopeTimer > 0)
				continue;

			channel->envelopeTimer = channel->envelopePeriod;
			if (channel->envelopeIncreasing)
			{
				if (channel->volume < 15)
					channel->volume++;
			}
			else if (channel->volume > 0)
			{
				channel->volume--;
			}
		}

		if (noise.envelopeRunning && noise.envelopePeriod != 0)
		{
			if (--noise.envelopeTimer <= 0)
			{
				noise.envelopeTimer = noise.envelopePeriod;
				if (noise.envelopeIncreasing)
				{
					if (noise.volume < 15)
						noise.volume++;
				}
				else if (noise.volume > 0)
				{
					noise.volume--;
				}
			}
		}
	}

	void GbApu::ClockSweep(int channelIndex)
	{
		// Channel 1's sweep unit, clocked at 128 Hz (Pan Docs "Audio Details"): the new period is
		// L +/- (L >> shift); a result above 2047 switches the channel off, and the calculation
		// (including the overflow check) runs a second time without writing anything back.
		Pulse& channel = pulse[channelIndex];
		if (!channel.sweepEnabled || channel.sweepPeriod == 0)
			return;
		if (--channel.sweepTimer > 0)
			return;

		channel.sweepTimer = channel.sweepPeriod;

		int change = channel.sweepShadow >> channel.sweepShift;
		int newFrequency;
		if (channel.sweepDecreasing)
		{
			newFrequency = channel.sweepShadow - change;
			channel.sweepNegateUsed = true;
		}
		else
		{
			newFrequency = channel.sweepShadow + change;
		}

		if (newFrequency > 2047)
		{
			channel.active = false;
			return;
		}

		if (channel.sweepShift != 0)
		{
			channel.sweepShadow = newFrequency;
			channel.frequency = newFrequency;
			ReloadPulseTimer(channel);

			// The hardware repeats the calculation on the value it just wrote and checks it for
			// overflow again; only the check matters, the result is not stored.
			int second = channel.sweepShadow >> channel.sweepShift;
			second = channel.sweepDecreasing ? (channel.sweepShadow - second)
				: (channel.sweepShadow + second);
			if (second > 2047)
				channel.active = false;
		}
	}

	// ---------------------------------------------------------------------------------------
	// Triggering
	// ---------------------------------------------------------------------------------------

	void GbApu::TriggerPulse(int index)
	{
		Pulse& channel = pulse[index];
		if (!channel.dacEnabled)
		{
			// Triggering a channel whose DAC is off does nothing (Pan Docs "Audio").
			channel.active = false;
			return;
		}

		channel.active = true;

		// A trigger reloads the length counter (the Pan Docs' 63-instead-of-64 quirk is not
		// modelled; see the header).
		channel.lengthCounter = 0;
		if (channel.lengthEnabled && channel.lengthCounter >= 64)
			channel.active = false;

		// The envelope reloads its volume from NRx2 and its timer from the period.
		channel.volume = channel.initialVolume;
		channel.envelopeTimer = channel.envelopePeriod;
		channel.envelopeRunning = channel.envelopePeriod != 0;

		ReloadPulseTimer(channel);

		if (index == 0)
		{
			// The sweep unit's shadow register and enable flag (Pan Docs "Audio Details").
			channel.sweepShadow = channel.frequency;
			channel.sweepTimer = channel.sweepPeriod ? channel.sweepPeriod : 8;
			channel.sweepEnabled = (channel.sweepPeriod != 0) || (channel.sweepShift != 0);

			if (channel.sweepShift != 0)
			{
				int change = channel.sweepShadow >> channel.sweepShift;
				int newFrequency = channel.sweepDecreasing
					? (channel.sweepShadow - change)
					: (channel.sweepShadow + change);
				if (newFrequency > 2047)
					channel.active = false;
			}
		}
	}

	void GbApu::TriggerWave()
	{
		if (!wave.dacEnabled)
		{
			wave.active = false;
			return;
		}

		wave.active = true;
		wave.lengthCounter = 0;
		if (wave.lengthEnabled && wave.lengthCounter >= 256)
			wave.active = false;

		ReloadWaveTimer();
		wave.position = 0;

		// Pan Docs "Audio Details": the wave RAM is only read at the next sample, so the channel
		// keeps playing its held sample for one period after a trigger.
	}

	void GbApu::TriggerNoise()
	{
		if (!noise.dacEnabled)
		{
			noise.active = false;
			return;
		}

		noise.active = true;
		noise.lengthCounter = 0;
		if (noise.lengthEnabled && noise.lengthCounter >= 64)
			noise.active = false;

		noise.lfsr = 0x7FFF;		// the LFSR is reset on a trigger
		noise.volume = noise.initialVolume;
		noise.envelopeTimer = noise.envelopePeriod;
		noise.envelopeRunning = noise.envelopePeriod != 0;
		noise.timer = NoisePeriod();
	}

	// ---------------------------------------------------------------------------------------
	// The registers
	// ---------------------------------------------------------------------------------------

	u8 GbApu::ReadRegister(u16 address) const
	{
		switch (address)
		{
		case 0xFF10: return 0x80;			// NR10: write-only, bit 7 reads one
		case 0xFF11: return 0x3F;			// NR11: write-only (the length reload value)
		case 0xFF12: return (u8)((pulse[0].initialVolume << 4) | (pulse[0].envelopeIncreasing ? 0x08 : 0)
			| (pulse[0].envelopePeriod & 0x07));
		case 0xFF13: return 0xFF;			// NR13: write-only
		case 0xFF14: return 0xBF;			// NR14: the length enable reads back, the rest does not
		case 0xFF16: return 0x3F;
		case 0xFF17: return (u8)((pulse[1].initialVolume << 4) | (pulse[1].envelopeIncreasing ? 0x08 : 0)
			| (pulse[1].envelopePeriod & 0x07));
		case 0xFF18: return 0xFF;
		case 0xFF19: return 0xBF;
		case 0xFF1A: return (u8)(wave.dacEnabled ? 0x80 : 0x00);
		case 0xFF1B: return 0xFF;
		case 0xFF1C: return (u8)(0x9F | (wave.volumeCode << 5));
		case 0xFF1D: return 0xFF;
		case 0xFF1E: return 0xBF;
		case 0xFF20: return 0xFF;
		case 0xFF21: return (u8)((noise.initialVolume << 4) | (noise.envelopeIncreasing ? 0x08 : 0)
			| (noise.envelopePeriod & 0x07));
		case 0xFF22: return (u8)((noise.shift << 4) | (noise.widthMode ? 0x08 : 0) | noise.divisorCode);
		case 0xFF23: return 0xBF;
		case 0xFF24: return nr50;
		case 0xFF25: return nr51;
		case 0xFF26:
			// Bit 7 is the master enable, bits 0-3 the read-only channel status, bits 4-6 read
			// as ones (Pan Docs "Audio Registers").
			return (u8)((powered ? 0x80 : 0x00) | 0x70 | ChannelStatus());
		default:
			if (address >= 0xFF30 && address <= 0xFF3F)
				return waveRam[address - 0xFF30];
			return 0xFF;
		}
	}

	void GbApu::WriteRegister(u16 address, u8 value)
	{
		if (address >= 0xFF30 && address <= 0xFF3F)
		{
			// Wave RAM is always readable and writable, even while the APU is off (Pan Docs
			// "Audio Registers"). The DMG's "corruption while playing" quirk is not modelled.
			waveRam[address - 0xFF30] = value;
			return;
		}

		// NR52 is the only register that is writable while the APU is off; everything else in
		// 0xFF10..0xFF25 is ignored (Pan Docs "Audio Registers").
		if (!powered && address != 0xFF26)
			return;

		switch (address)
		{
		case 0xFF10:		// NR10: the sweep
		{
			pulse[0].sweepPeriod = (value >> 4) & 0x07;
			bool decreasing = (value & 0x08) != 0;
			pulse[0].sweepShift = value & 0x07;

			// The "negate and clear" quirk (Pan Docs "Audio Details"): clearing the direction
			// bit after a subtraction calculation since the last trigger switches the channel off.
			if (!decreasing && pulse[0].sweepDecreasing && pulse[0].sweepNegateUsed)
				pulse[0].active = false;
			pulse[0].sweepDecreasing = decreasing;
			break;
		}

		case 0xFF11:		// NR11: duty and the length reload value
			pulse[0].duty = (value >> 6) & 0x03;
			pulse[0].lengthCounter = LengthFromRegister(value & 0x3F, 64);
			break;
		case 0xFF12:		// NR12: the envelope and the DAC enable
			pulse[0].envelopePeriod = value & 0x07;
			pulse[0].envelopeIncreasing = (value & 0x08) != 0;
			pulse[0].initialVolume = (value >> 4) & 0x0F;
			if (!pulse[0].active)
				pulse[0].volume = pulse[0].initialVolume;
			pulse[0].dacEnabled = (value & 0xF8) != 0;
			if (!pulse[0].dacEnabled)
				pulse[0].active = false;
			break;
		case 0xFF13:		// NR13: the low eight bits of the frequency
			pulse[0].frequency = (pulse[0].frequency & 0x700) | value;
			break;
		case 0xFF14:		// NR14: trigger, length enable, the high three frequency bits
			pulse[0].frequency = (pulse[0].frequency & 0xFF) | ((value & 0x07) << 8);
			pulse[0].lengthEnabled = (value & 0x40) != 0;
			if (value & 0x80)
				TriggerPulse(0);
			break;

		case 0xFF16:
			pulse[1].duty = (value >> 6) & 0x03;
			pulse[1].lengthCounter = LengthFromRegister(value & 0x3F, 64);
			break;
		case 0xFF17:
			pulse[1].envelopePeriod = value & 0x07;
			pulse[1].envelopeIncreasing = (value & 0x08) != 0;
			pulse[1].initialVolume = (value >> 4) & 0x0F;
			if (!pulse[1].active)
				pulse[1].volume = pulse[1].initialVolume;
			pulse[1].dacEnabled = (value & 0xF8) != 0;
			if (!pulse[1].dacEnabled)
				pulse[1].active = false;
			break;
		case 0xFF18:
			pulse[1].frequency = (pulse[1].frequency & 0x700) | value;
			break;
		case 0xFF19:
			pulse[1].frequency = (pulse[1].frequency & 0xFF) | ((value & 0x07) << 8);
			pulse[1].lengthEnabled = (value & 0x40) != 0;
			if (value & 0x80)
				TriggerPulse(1);
			break;

		case 0xFF1A:		// NR30: the wave channel's DAC
			wave.dacEnabled = (value & 0x80) != 0;
			if (!wave.dacEnabled)
				wave.active = false;
			break;
		case 0xFF1B:		// NR31: the length reload value (8 bits)
			wave.lengthCounter = LengthFromRegister(value, 256);
			break;
		case 0xFF1C:		// NR32: the output level
			wave.volumeCode = (value >> 5) & 0x03;
			break;
		case 0xFF1D:		// NR33: the low eight bits of the frequency
			wave.frequency = (wave.frequency & 0x700) | value;
			break;
		case 0xFF1E:		// NR34
			wave.frequency = (wave.frequency & 0xFF) | ((value & 0x07) << 8);
			wave.lengthEnabled = (value & 0x40) != 0;
			if (value & 0x80)
				TriggerWave();
			break;

		case 0xFF20:		// NR41: the length reload value (6 bits)
			noise.lengthCounter = LengthFromRegister(value & 0x3F, 64);
			break;
		case 0xFF21:		// NR42: the envelope and the DAC enable
			noise.envelopePeriod = value & 0x07;
			noise.envelopeIncreasing = (value & 0x08) != 0;
			noise.initialVolume = (value >> 4) & 0x0F;
			if (!noise.active)
				noise.volume = noise.initialVolume;
			noise.dacEnabled = (value & 0xF8) != 0;
			if (!noise.dacEnabled)
				noise.active = false;
			break;
		case 0xFF22:		// NR43: the clock shift, the LFSR width and the divisor
			noise.shift = (value >> 4) & 0x0F;
			noise.widthMode = (value & 0x08) != 0;
			noise.divisorCode = value & 0x07;
			break;
		case 0xFF23:		// NR44
			noise.lengthEnabled = (value & 0x40) != 0;
			if (value & 0x80)
				TriggerNoise();
			break;

		case 0xFF24: nr50 = value; break;
		case 0xFF25: nr51 = value; break;

		case 0xFF26:
			// Bit 7 powers the APU down; a write of zero clears every register (Pan Docs
			// "Audio Registers"). Bits 4-6 are unused and the channel status bits are read-only.
			if ((value & 0x80) == 0 && powered)
				PowerOff();
			else if (value & 0x80)
				powered = true;
			break;

		default:
			break;
		}
	}

	void GbApu::PowerOff()
	{
		powered = false;

		pulse[0] = Pulse{};
		pulse[1] = Pulse{};
		wave = Wave{};
		noise = Noise{};
		nr50 = 0x00;
		nr51 = 0x00;

		// "Wave RAM is not affected and can always be read/written" (Pan Docs), and the DIV-APU
		// counter is not reset either, so neither is touched here.
	}

	u8 GbApu::ChannelStatus() const
	{
		u8 status = 0;
		if (pulse[0].active && pulse[0].dacEnabled)
			status |= 0x01;
		if (pulse[1].active && pulse[1].dacEnabled)
			status |= 0x02;
		if (wave.active && wave.dacEnabled)
			status |= 0x04;
		if (noise.active && noise.dacEnabled)
			status |= 0x08;
		return status;
	}

	// ---------------------------------------------------------------------------------------
	// The digital channel outputs
	// ---------------------------------------------------------------------------------------

	int GbApu::PulseOutput(const Pulse& channel) const
	{
		// A channel with its DAC off contributes nothing (Pan Docs "Audio": the channel is only
		// audible when its DAC is enabled); a silent channel outputs the digital zero.
		if (!channel.dacEnabled || !channel.active)
			return 0;
		return DutyBit(channel.duty, channel.dutyStep) ? channel.volume : 0;
	}

	int GbApu::WaveOutput() const
	{
		if (!wave.dacEnabled || !wave.active)
			return 0;

		// The output level shifts the digital sample (Pan Docs "Audio Registers"): 0 mutes the
		// channel, 1 is 100%, 2 is 50% and 3 is 25%.
		switch (wave.volumeCode & 0x03)
		{
		case 0: return 0;
		case 1: return wave.sampleBuffer;
		case 2: return wave.sampleBuffer >> 1;
		default: return wave.sampleBuffer >> 2;
		}
	}

	int GbApu::NoiseOutput() const
	{
		if (!noise.dacEnabled || !noise.active)
			return 0;
		// Bit 0 of the LFSR selects between the envelope's volume and silence.
		return (noise.lfsr & 1) ? 0 : noise.volume;
	}

	int GbApu::ChannelOutput(int channel) const
	{
		switch (channel)
		{
		case 0: return PulseOutput(pulse[0]);
		case 1: return PulseOutput(pulse[1]);
		case 2: return WaveOutput();
		default: return NoiseOutput();
		}
	}

	// ---------------------------------------------------------------------------------------
	// The mixer
	// ---------------------------------------------------------------------------------------

	void GbApu::MixSample()
	{
		// The four DACs are summed per side according to NR51's routing, then scaled by NR50's
		// master volume for that side (Pan Docs "Audio Details": a master volume of 0 counts as
		// 1 and 7 as 8, and the amplifier never fully mutes an input).
		int outputs[4] = { PulseOutput(pulse[0]), PulseOutput(pulse[1]), WaveOutput(), NoiseOutput() };

		bool anyDac = pulse[0].dacEnabled || pulse[1].dacEnabled || wave.dacEnabled || noise.dacEnabled;

		int sumLeft = 0;
		int sumRight = 0;
		for (int i = 0; i < 4; i++)
		{
			int bit = 1 << i;
			if (nr51 & bit)
				sumLeft += outputs[i];
			if (nr51 & (bit << 4))
				sumRight += outputs[i];
		}

		double left = (double)sumLeft;
		double right = (double)sumRight;

		// The high pass filter of the two outputs (Pan Docs "Audio Details" gives this reference
		// implementation). With every DAC off the outputs are disconnected and read exactly zero.
		double filteredLeft = 0.0;
		double filteredRight = 0.0;
		if (anyDac)
		{
			filteredLeft = left - capacitorLeft;
			capacitorLeft = left - filteredLeft * highPassCharge;
			filteredRight = right - capacitorRight;
			capacitorRight = right - filteredRight * highPassCharge;
		}
		else
		{
			capacitorLeft = 0.0;
			capacitorRight = 0.0;
		}

		// Each DAC maps the digital value 0..15 to a swing of one unit (Pan Docs "Audio
		// Details"), so a side carries up to four units and the master volume scales it by up to
		// eight. 64 units is therefore the full scale used to reach a 16-bit host sample.
		const double Scale = 32767.0 / 64.0;

		int leftSample = (int)(filteredLeft * Scale);
		int rightSample = (int)(filteredRight * Scale);
		if (leftSample > 32767) leftSample = 32767;
		if (leftSample < -32768) leftSample = -32768;
		if (rightSample > 32767) rightSample = 32767;
		if (rightSample < -32768) rightSample = -32768;

		pending.push_back((s16)leftSample);
		pending.push_back((s16)rightSample);

		// Drain automatically if the frontend stops calling ReadSamples, so a long run cannot
		// grow the buffer without bound (about four seconds of audio at most).
		size_t limit = (size_t)sampleRate * 2 * 4;
		if (pending.size() > limit)
			pending.erase(pending.begin(), pending.begin() + (pending.size() - limit));
	}

	int GbApu::ReadSamples(s16* out, int maxFrames)
	{
		if (out == nullptr || maxFrames <= 0)
			return 0;

		int frames = (int)(pending.size() / 2);
		if (frames > maxFrames)
			frames = maxFrames;

		for (int i = 0; i < frames * 2; i++)
			out[i] = pending[(size_t)i];

		pending.erase(pending.begin(), pending.begin() + frames * 2);
		return frames;
	}
}
