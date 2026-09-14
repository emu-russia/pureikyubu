// GBA sound: the four legacy (DMG derived) PSG channels, the two direct-sound FIFO channels and
// the mixer that turns them into the host's stereo sample stream (see gba_apu.h).
//
// Written from GBATEK ("GBA Sound Controller", "GBA Sound Channel 1 - Tone & Sweep", ".. 2 -
// Tone", ".. 3 - Wave Output", ".. 4 - Noise", ".. A and B - DMA Sound", "GBA Sound Control
// Registers", "GBA Comparison of CGB and GBA Sound") and, for the four legacy channels, from the
// DMG audio chapters of the Pan Docs ("Audio", "Audio Registers", "Audio Details"): the GBA
// keeps the DMG's frame sequencer, length table, envelope and LFSR and only changes the clock
// rate, the sweep/envelope step units and the wave channel (two banks, the forced 75 % volume).
//
// The signal path, in the order the hardware has it:
//
//   channel generator (4 bit level 0..15) -> mixer (NR51/NR50 panning and master volume)
//     -> host scaling
//
// The GBA mixes digitally: each of the four PSGs spans a quarter of the output range and each
// FIFO the full range, and the sum is clamped (GBATEK "Max Output Levels"). The DMG's analog
// DAC and the high pass filter are not modelled; a channel whose DAC is off contributes 0, and
// the levels here are the DC free signed equivalents of the hardware's digital values.
//
// The mixer runs at the host's sample rate (32768 Hz by default, the rate the GBA mixes at): the
// channel generators are stepped once per host sample through their phase accumulators, which is
// the "per sample" resampling the hardware does with its own 32.768 kHz stage.

#include "gba_apu.h"
#include "gba_bus.h"

namespace GBA
{
	namespace
	{
		// -----------------------------------------------------------------------------------
		// Tables and constants
		// -----------------------------------------------------------------------------------

		// The frame sequencer runs at 512 Hz (Pan Docs "DIV-APU": one step per 8 DIV-APU
		// ticks): 16777216 / 512 = 32768 system cycles per step. Length is clocked by the even
		// steps (256 Hz), the sweep by steps 2 and 6 (128 Hz) and the envelope by step 7
		// (64 Hz), which is the classic DMG assignment.
		const int FrameSequencerCycles = CyclesPerSecond / 512;

		// GBATEK "GBA Sound Channel 4 - Noise": Frequency = 524288 Hz / r / 2^(s+1), with
		// r = 0 counting as 0.5. Written as the DMG's divider table (Pan Docs "Audio
		// Registers"), one LFSR step lasts divisor[r] * 2^s DMG clocks, and the GBA's clock is
		// four times as fast, hence the factor 4 below.
		const int NoiseDivisor[8] = { 8, 16, 32, 48, 64, 80, 96, 112 };
		const int NoiseGbaScale = 4;

		// GBATEK "Wave Duty": the four duty patterns as 8 step bits, step 0 being the bit the
		// picture starts with (the high part first: 12.5 % is "-_______" = one high step).
		const u8 DutyPattern[4] = { 0x01, 0x03, 0x0F, 0x3F };

		// The FIFO is 8 x 32 bit = 32 bytes deep and the DMA is asked for four words (16 bytes)
		// as soon as fewer than 16 bytes are left (GBATEK "Sound Channel A and B").
		const int FifoSize = 32;
		const int FifoRequestLevel = 16;

		// The GBA's digital mixer sums four PSGs at +/-0x80 and two FIFOs at +/-0x100, i.e. at
		// most +/-992 in the units used here; MixScale maps that onto the host's 16 bit range
		// (992 * 33 = 32736, about +/-32767 at master volume 100 %).
		const int MixScale = 33;

		// `pending` is capped so a frontend that stops draining cannot make the mixer grow
		// without bound: four seconds of stereo frames.
		const int PendingSeconds = 4;

		// The wave channel's timer driven sample clock remembers the last timer reading; that
		// reading is invalid right after a reset or a trigger, which is what the marker says.
		const u16 TimerNotPrimed = 0xFFFF;

		inline int Clamp16(int value)
		{
			if (value > 32767)
				return 32767;
			if (value < -32768)
				return -32768;
			return value;
		}

		// One step of the sweep unit: X(t) = X(t-1) +/- X(t-1)/2^n (GBATEK "SOUND1CNT_L").
		// The result is checked against the 11 bit frequency range by the caller.
		inline int SweepFrequency(int shadow, int shift, bool increase)
		{
			int delta = shadow >> shift;
			return increase ? shadow + delta : shadow - delta;
		}

		// The wave RAM is played as 4 bit digits, the HIGH nibble of a byte first (GBATEK
		// "WAVE_RAM": "MSBs of 1st byte, followed by LSBs of 1st byte, followed by MSBs of 2nd
		// byte, and so on").
		inline int WaveDigit(const u8* ram, int position)
		{
			u8 byte = ram[(position & 0x1F) >> 1];
			return (position & 1) ? (byte & 0x0F) : (byte >> 4);
		}
	}

	// ---------------------------------------------------------------------------------------
	// Lifecycle
	// ---------------------------------------------------------------------------------------

	void Apu::Reset()
	{
		// The sound registers come up zeroed; SOUNDCNT_X bit 7 is clear, so the PSG half stays
		// in reset until the program writes 0x80 there (GBATEK "SOUNDCNT_X").
		sound1cntL = sound1cntH = sound1cntX = 0;
		sound2cntL = sound2cntH = 0;
		sound3cntL = sound3cntH = sound3cntX = 0;
		sound4cntL = sound4cntH = 0;
		soundcntL = soundcntH = 0;
		soundcntX = 0;

		// SOUNDBIAS comes up at 0200h: a bias level of 100h, 9 bit at 32.768 kHz (GBATEK
		// "SOUNDBIAS").
		soundbias = 0x200;

		for (int i = 0; i < 2; i++)
		{
			square[i] = SquareChannel();
			fifoHead[i] = fifoTail[i] = fifoCount[i] = 0;
			fifoRequest[i] = false;
			fifoEnabled[i] = false;
			fifoVolume[i] = 0;
			fifoTimerSelect[i] = false;
			fifoLeftOnly[i] = fifoRightOnly[i] = false;
			fifoOutput[i] = 0;
			fifoAccum[i] = -1;			// -1: no timer reading has been taken yet
			fifoLatchedSample[i] = 0;
		}
		memset(fifo, 0, sizeof(fifo));

		// The wave RAM is zeroed on power up (GBATEK "WAVE_RAM": "Both banks of Wave Ram are
		// filled with zero upon initialization").
		memset(waveRam, 0, sizeof(waveRam));

		noiseSample = 0;

		waveEnabled = false;
		waveDacEnabled = false;
		waveFrequency = 0;
		waveDimension = 0;
		waveBank = 0;
		waveVolume = 0;
		waveForceVolume = false;
		wavePhase = 0;
		waveSample = 0;
		wavePosition = 0;
		waveLength = 0;
		waveLengthEnabled = false;
		lastTimerValue = TimerNotPrimed;

		noiseEnabled = false;
		noiseDacEnabled = false;
		noiseFrequency = 0;
		noiseDivisor = 0;
		noiseWidth = 0;
		noiseShift = 0;
		noisePhase = 0;
		noiseEnvelopeVolume = 0;
		noiseEnvelopePeriod = 0;
		noiseEnvelopeTimer = 0;
		noiseEnvelopeUp = false;
		noiseLfsr = 0x7FFF;
		noiseLengthEnabled = false;
		noiseLength = 0;
		noiseSampleRate = 0;

		cycleAccum = 0;
		sampleCounter = 0;
		frameSeqStep = 0;
		frameSeqClock = 0;
		pending.clear();

		// `sampleRate` is a host setting rather than hardware state: it survives a reset.
	}

	void Apu::SetSampleRate(int hz)
	{
		// Any host rate works: the sample clock counts system cycles scaled by the rate, so the
		// number of samples per unit of time is exact even for the fractional rates (see Tick).
		if (hz < 8000 || hz > 192000)
		{
			Log(LogLevel::Warn, "apu: host sample rate %i Hz is outside 8000..192000, clamping", hz);
			hz = (hz < 8000) ? 8000 : 192000;
		}

		sampleRate = hz;
		cycleAccum = 0;
	}

	// ---------------------------------------------------------------------------------------
	// Register file
	// ---------------------------------------------------------------------------------------

	u8 Apu::Read8(u32 offset, u8 openBus) const
	{
		switch (offset)
		{
		// -- channel 1: SOUND1CNT_L/H/X (NR10..NR14) ----------------------------------------
		case 0x60: return (u8)(sound1cntL & 0x7F);			// NR10 is R/W
		case 0x61: return 0;								// bits 8-15 are unused
		case 0x62: return (u8)(sound1cntH & 0xC0);			// NR11: only the duty bits are R/W
		case 0x63: return (u8)(sound1cntH >> 8);			// NR12 is R/W
		case 0x64: return openBus;							// NR13 is write only
		case 0x65: return (u8)((sound1cntX >> 8) & 0x40);	// NR14: only the length flag is R/W

		// -- channel 2: SOUND2CNT_L/H (NR21..NR24) -----------------------------------------
		case 0x66: return openBus;
		case 0x68: return (u8)(sound2cntL & 0xC0);			// NR21: only the duty bits are R/W
		case 0x69: return (u8)(sound2cntL >> 8);			// NR22 is R/W
		case 0x6A: return openBus;
		case 0x6C: return openBus;							// NR23 is write only
		case 0x6D: return (u8)((sound2cntH >> 8) & 0x40);	// NR24: only the length flag is R/W

		// -- channel 3: SOUND3CNT_L/H/X (NR30..NR34) ---------------------------------------
		case 0x6E: return openBus;
		case 0x70: return (u8)(sound3cntL & 0xE0);			// NR30 is R/W
		case 0x71: return 0;								// bits 8-15 are unused
		case 0x72: return openBus;							// NR31 is write only
		case 0x73: return (u8)((sound3cntH >> 8) & 0xE0);	// NR32 is R/W
		case 0x74: return openBus;							// NR33 is write only
		case 0x75: return (u8)((sound3cntX >> 8) & 0x40);	// NR34: only the length flag is R/W

		// -- channel 4: SOUND4CNT_L/H (NR41..NR44) -----------------------------------------
		case 0x76: return openBus;
		case 0x78: return openBus;							// NR41 is write only
		case 0x79: return (u8)(sound4cntL >> 8);			// NR42 is R/W
		case 0x7A: return openBus;
		case 0x7C: return (u8)(sound4cntH & 0xFF);			// NR43 is R/W
		case 0x7D: return (u8)((sound4cntH >> 8) & 0x40);	// NR44: only the length flag is R/W

		// -- the control registers ---------------------------------------------------------
		case 0x7E: return openBus;
		case 0x80: return (u8)(soundcntL & 0xFF);			// NR50
		case 0x81: return (u8)(soundcntL >> 8);				// NR51
		case 0x82: return (u8)(soundcntH & 0x0F);			// SOUNDCNT_H: bits 4-7 are unused
		case 0x83: return (u8)((soundcntH >> 8) & 0x77);	// bits 11/15 are the write-only resets
		case 0x84:
		{
			// SOUNDCNT_X bits 0-3 report whether a channel is producing sound. A channel whose
			// DAC is off (or that the length/sweep stopped) reads as 0 (GBATEK "SOUNDCNT_X").
			u8 status = (soundcntX & 0x80) ? 0x80 : 0x00;
			if (square[0].enabled && square[0].dacEnabled)
				status |= 0x01;
			if (square[1].enabled && square[1].dacEnabled)
				status |= 0x02;
			if (waveEnabled && waveDacEnabled)
				status |= 0x04;
			if (noiseEnabled && noiseDacEnabled)
				status |= 0x08;
			return status;
		}
		case 0x85: return 0;								// bits 8-15 are unused
		case 0x86: return openBus;

		// SOUNDBIAS works even while the PSG half is off (GBATEK "SOUNDCNT_X").
		case 0x88: return (u8)(soundbias & 0xFE);			// bit 0 unused, bits 1-7 bias
		case 0x89: return (u8)((soundbias >> 8) & 0xC3);	// bits 8-9 bias, 14-15 resolution
		case 0x8A:
		case 0x8B:
		case 0x8C:
		case 0x8D:
		case 0x8E:
		case 0x8F: return openBus;

		// -- WAVE_RAM and the FIFOs --------------------------------------------------------
		case 0x90: case 0x91: case 0x92: case 0x93:
		case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99: case 0x9A: case 0x9B:
		case 0x9C: case 0x9D: case 0x9E: case 0x9F:
			// The wave RAM is read/write. GBATEK notes that on the real hardware it is a shift
			// register, so the positions move while the channel plays; the array here keeps the
			// bytes where they were written (see the deviation note in MixWave).
			return waveRam[offset - 0x90];

		case 0xA0: case 0xA1: case 0xA2: case 0xA3:
		case 0xA4: case 0xA5: case 0xA6: case 0xA7:
		{
			// On hardware a FIFO read returns the open bus (the FIFOs are write-only); the APU
			// returns the byte that will be played next so the debugger (and the tests) can see
			// what is queued, and the latched sample once the FIFO has run dry.
			int which = (offset >= 0xA4) ? 1 : 0;
			if (fifoCount[which] > 0)
				return fifo[which][fifoHead[which]];
			return (u8)fifoLatchedSample[which];
		}

		default: return openBus;
		}
	}

	void Apu::Write8(u32 offset, u8 value)
	{
		// While SOUNDCNT_X bit 7 is clear, both the PSG and the FIFO sound are disabled and the
		// PSG registers at 0x060..0x081 are held at zero, i.e. writes to them are lost until the
		// master enable is set again (GBATEK "SOUNDCNT_X"). SOUNDCNT_H (0x082), SOUNDBIAS
		// (0x088) and the wave RAM stay writable, and the wave RAM keeps its contents.
		if (offset <= 0x81 && (soundcntX & 0x80) == 0)
			return;

		switch (offset)
		{
		// -- channel 1 ---------------------------------------------------------------------
		case 0x60:		// NR10: sweep shift (0-2), direction (3), time (4-6)
			// Clearing the direction bit after a subtraction sweep has already run disables the
			// channel (Pan Docs "Obscure Behavior": it stops a program from lowering and then
			// raising the frequency without a trigger).
			if ((value & 0x08) == 0 && square[0].sweepNegateUsed)
				square[0].enabled = false;
			sound1cntL = value & 0x7F;
			square[0].sweepShift = ((sound1cntL >> 0) & 7);
			square[0].sweepUp = Bit(sound1cntL, 3) == 0;
			square[0].sweepPeriod = ((sound1cntL >> 4) & 7);
			break;

		case 0x62:		// NR11: duty (6-7), length (0-5, write only)
			// The whole byte is kept in the register shadow (the length bits are needed again when
			// a trigger reloads the counter); Read8 masks the write-only bits out.
			sound1cntH = (sound1cntH & 0xFF00) | value;
			square[0].duty = (value >> 6) & 3;
			// The length counter holds the (64-n) 256 Hz steps of GBATEK "SOUND1CNT_H".
			square[0].length = 64 - (value & 0x3F);
			break;

		case 0x63:		// NR12: envelope step time (0-2), direction (3), volume (4-7)
			sound1cntH = (sound1cntH & 0x00FF) | ((u16)value << 8);
			square[0].envelopePeriod = ((value >> 0) & 7);
			square[0].envelopeUp = Bit(value, 3) != 0;
			square[0].envelopeVolume = ((value >> 4) & 0xF);
			square[0].volume = square[0].envelopeVolume;
			// The DAC is on when bits 3-7 are not zero (Pan Docs "DACs"); turning it off stops
			// the channel at once.
			square[0].dacEnabled = (value & 0xF8) != 0;
			if (!square[0].dacEnabled)
				square[0].enabled = false;
			break;

		case 0x64:		// NR13: frequency, low 8 bits (write only)
			sound1cntX = (sound1cntX & 0xFF00) | value;
			square[0].frequency = sound1cntX & 0x7FF;
			break;

		case 0x65:		// NR14: frequency bits 8-10, length flag (6), trigger (7)
			sound1cntX = (sound1cntX & 0x00FF) | ((u16)(value & 0x47) << 8);
			square[0].frequency = sound1cntX & 0x7FF;
			if (value & 0x80)
			{
				// The trigger reads the length flag from the register and compares it with the
				// state as it was *before* this write, which is what the extra-length quirk
				// (Pan Docs "Obscure Behavior") needs.
				TriggerSquare(0);
			}
			else
			{
				square[0].lengthEnabled = (value & 0x40) != 0;
			}
			break;

		// -- channel 2 ---------------------------------------------------------------------
		case 0x68:		// NR21: duty (6-7), length (0-5)
			sound2cntL = (sound2cntL & 0xFF00) | value;
			square[1].duty = (value >> 6) & 3;
			square[1].length = 64 - (value & 0x3F);
			break;

		case 0x69:		// NR22: envelope and volume
			sound2cntL = (sound2cntL & 0x00FF) | ((u16)value << 8);
			square[1].envelopePeriod = ((value >> 0) & 7);
			square[1].envelopeUp = Bit(value, 3) != 0;
			square[1].envelopeVolume = ((value >> 4) & 0xF);
			square[1].volume = square[1].envelopeVolume;
			square[1].dacEnabled = (value & 0xF8) != 0;
			if (!square[1].dacEnabled)
				square[1].enabled = false;
			break;

		case 0x6C:		// NR23: frequency low
			sound2cntH = (sound2cntH & 0xFF00) | value;
			square[1].frequency = sound2cntH & 0x7FF;
			break;

		case 0x6D:		// NR24: frequency high, length flag, trigger
			sound2cntH = (sound2cntH & 0x00FF) | ((u16)(value & 0x47) << 8);
			square[1].frequency = sound2cntH & 0x7FF;
			if (value & 0x80)
				TriggerSquare(1);
			else
				square[1].lengthEnabled = (value & 0x40) != 0;
			break;

		// -- channel 3 ---------------------------------------------------------------------
		case 0x70:		// NR30: dimension (5), bank (6), playback/DAC (7)
			sound3cntL = value & 0xE0;
			waveDimension = Bit(value, 5);
			waveBank = Bit(value, 6);
			// Channel 3's DAC is bit 7 of NR30 itself (Pan Docs "DACs"), so the same bit both
			// enables the DAC and starts/stops the channel.
			waveDacEnabled = Bit(value, 7) != 0;
			if (!waveDacEnabled)
			{
				waveEnabled = false;
				waveSample = 0;
			}
			break;

		case 0x71:		// SOUND3CNT_L bits 8-15 are unused
			break;

		case 0x72:		// NR31: length (write only), units of (256-n)/256 s
			sound3cntH = (sound3cntH & 0xFF00) | value;
			waveLength = 256 - value;
			break;

		case 0x73:		// NR32: volume (5-6), force volume (7)
			sound3cntH = (sound3cntH & 0x00FF) | ((u16)(value & 0xE0) << 8);
			waveVolume = ((value >> 5) & 3);
			waveForceVolume = Bit(value, 7) != 0;
			break;

		case 0x74:		// NR33: frequency low
			sound3cntX = (sound3cntX & 0xFF00) | value;
			waveFrequency = sound3cntX & 0x7FF;
			break;

		case 0x75:		// NR34: frequency high, length flag (6), trigger (7)
			sound3cntX = (sound3cntX & 0x00FF) | ((u16)(value & 0x47) << 8);
			waveFrequency = sound3cntX & 0x7FF;
			if (value & 0x80)
				TriggerWave();
			else
				waveLengthEnabled = (value & 0x40) != 0;
			break;

		// -- channel 4 ---------------------------------------------------------------------
		case 0x78:		// NR41: length (write only)
			sound4cntL = (sound4cntL & 0xFF00) | (value & 0x3F);
			noiseLength = 64 - (value & 0x3F);
			break;

		case 0x79:		// NR42: envelope and volume
			sound4cntL = (sound4cntL & 0x00FF) | ((u16)value << 8);
			noiseEnvelopePeriod = ((value >> 0) & 7);
			noiseEnvelopeUp = Bit(value, 3) != 0;
			noiseEnvelopeVolume = ((value >> 4) & 0xF);
			noiseDacEnabled = (value & 0xF8) != 0;
			if (!noiseDacEnabled)
				noiseEnabled = false;
			break;

		case 0x7C:		// NR43: divisor (0-2), width (3), clock shift (4-7)
			sound4cntH = (sound4cntH & 0xFF00) | value;
			noiseFrequency = value;
			noiseDivisor = ((value >> 0) & 7);
			noiseWidth = Bit(value, 3);
			noiseShift = ((value >> 4) & 0xF);
			// GBATEK "Sound Channel 4": Frequency = 524288 Hz / r / 2^(s+1). In system cycles
			// one LFSR step is therefore divisor[r] * 2^s * 4.
			noiseSampleRate = NoiseDivisor[noiseDivisor & 7] * NoiseGbaScale * (1 << (noiseShift & 0xF));
			break;

		case 0x7D:		// NR44: length flag (6), trigger (7)
			sound4cntH = (sound4cntH & 0x00FF) | ((u16)(value & 0x40) << 8);
			if (value & 0x80)
				TriggerNoise();
			else
				noiseLengthEnabled = (value & 0x40) != 0;
			break;

		// -- the control registers ---------------------------------------------------------
		case 0x80:		// NR50: master volume right (0-2) and left (4-6)
			soundcntL = (soundcntL & 0xFF00) | value;
			break;

		case 0x81:		// NR51: channel 1-4 panning, bits 8-11 right and 12-15 left
			soundcntL = (soundcntL & 0x00FF) | ((u16)value << 8);
			break;

		case 0x82:		// SOUNDCNT_H: PSG volume (0-1) and the FIFO volumes (2, 3)
			soundcntH = (soundcntH & 0xFF00) | (value & 0x0F);
			fifoVolume[0] = Bit(value, 2);
			fifoVolume[1] = Bit(value, 3);
			break;

		case 0x83:		// SOUNDCNT_H: the FIFO enables, timers and resets
		{
			soundcntH = (soundcntH & 0x00FF) | ((u16)value << 8);

			for (int which = 0; which < 2; which++)
			{
				// A: right = 8, left = 9, timer = 10, reset = 11; B: the same bits + 4.
				int base = which ? 4 : 0;
				bool right = Bit(value, base + 0) != 0;
				bool left = Bit(value, base + 1) != 0;
				bool timer = Bit(value, base + 2) != 0;
				bool reset = Bit(value, base + 3) != 0;

				// A FIFO is audible while it is routed to at least one side (GBATEK
				// "SOUNDCNT_H"); an unrouted FIFO does not consume its data either.
				fifoRightOnly[which] = right;
				fifoLeftOnly[which] = left;
				fifoEnabled[which] = right || left;

				if (fifoTimerSelect[which] != timer)
					fifoAccum[which] = -1;		// the other timer needs a fresh reading
				fifoTimerSelect[which] = timer;

				if (reset)
				{
					// "DMA Sound A/B Reset FIFO (1=Reset)": the FIFO is emptied (GBATEK
					// "SOUNDCNT_H"); the request that the emptied FIFO raises is issued from
					// Tick, which is the only place that has the bus.
					fifoHead[which] = fifoTail[which] = fifoCount[which] = 0;
					fifoLatchedSample[which] = 0;
					fifoOutput[which] = 0;
					fifoAccum[which] = -1;
					fifoRequest[which] = false;
				}
			}
			break;
		}

		case 0x84:		// SOUNDCNT_X: bit 7 is the PSG/FIFO master enable (bits 0-3 are status)
		{
			bool enable = (value & 0x80) != 0;
			if (!enable && (soundcntX & 0x80) != 0)
			{
				// "While Bit 7 is cleared, both PSG and FIFO sounds are disabled, and all PSG
				// registers at 4000060h..4000081h are reset to zero" (GBATEK "SOUNDCNT_X").
				// SOUNDCNT_H, SOUNDBIAS and the wave RAM are kept, and so are the FIFO buffers
				// (they are only stopped, they are not part of the register file).
				sound1cntL = sound1cntH = sound1cntX = 0;
				sound2cntL = sound2cntH = 0;
				sound3cntL = sound3cntH = sound3cntX = 0;
				sound4cntL = sound4cntH = 0;
				soundcntL = 0;

				for (int i = 0; i < 2; i++)
				{
					square[i] = SquareChannel();
					// Turning the APU off resets the duty step counters and the phase of every
					// channel (Pan Docs "Pulse channels": the duty step counter can only be
					// reset by turning the APU off).
					fifoEnabled[i] = false;
					fifoOutput[i] = 0;
					fifoLatchedSample[i] = 0;
					fifoAccum[i] = -1;
					fifoRequest[i] = false;
				}

				waveEnabled = false;
				waveDacEnabled = false;
				waveFrequency = 0;
				waveDimension = 0;
				waveBank = 0;
				waveVolume = 0;
				waveForceVolume = false;
				wavePhase = 0;
				waveSample = 0;
				wavePosition = 0;
				waveLength = 0;
				waveLengthEnabled = false;
				lastTimerValue = TimerNotPrimed;

				noiseEnabled = false;
				noiseDacEnabled = false;
				noiseFrequency = 0;
				noiseDivisor = 0;
				noiseWidth = 0;
				noiseShift = 0;
				noisePhase = 0;
				noiseEnvelopeVolume = 0;
				noiseEnvelopePeriod = 0;
				noiseEnvelopeTimer = 0;
				noiseEnvelopeUp = false;
				noiseLfsr = 0x7FFF;
				noiseLengthEnabled = false;
				noiseLength = 0;
				noiseSampleRate = 0;

				noiseSample = 0;
			}

			// Writing 1 to bit 7 of an already enabled APU keeps the register contents; writing
			// 0 clears the enable (and bit 7 of the shadow follows it).
			soundcntX = enable ? 0x80 : 0x00;
			break;
		}

		case 0x85:		// SOUNDCNT_X bits 8-15 are unused
			break;

		case 0x88:		// SOUNDBIAS low byte: bit 0 unused, bits 1-9 the bias level
			soundbias = (soundbias & 0xFF00) | (value & 0xFE);
			break;

		case 0x89:		// SOUNDBIAS high byte: bits 14-15 the amplitude resolution
			soundbias = (soundbias & 0x00FF) | ((u16)(value & 0xC3) << 8);
			break;

		default:
			if (offset >= 0x90 && offset <= 0x9F)
			{
				// The wave RAM. GBATEK notes that the CPU addresses the bank that is *not*
				// being played; this implementation keeps one 16 byte pattern (see MixWave).
				waveRam[offset - 0x90] = value;
			}
			else if (offset >= 0xA0 && offset <= 0xA7)
			{
				// FIFO_A (0x0A0..0x0A3) / FIFO_B (0x0A4..0x0A7): a byte is pushed into the
				// 32 byte queue. A write while the FIFO is full is lost.
				int which = (offset >= 0xA4) ? 1 : 0;
				if (fifoCount[which] < FifoSize)
				{
					fifo[which][fifoTail[which]] = value;
					fifoTail[which] = (fifoTail[which] + 1) % FifoSize;
					fifoCount[which]++;
				}
				else
				{
					Log(LogLevel::Warn, "apu: FIFO %c overflow, byte %02X dropped", which ? 'B' : 'A', value);
				}
			}
			break;
		}
	}

	// ---------------------------------------------------------------------------------------
	// The DMA side of the FIFOs
	// ---------------------------------------------------------------------------------------

	void Apu::FifoDmaDone(int which, const u32* words, int count)
	{
		// The DMA moved `count` words (four of them for a normal refill) into the FIFO. GBATEK:
		// "Data 0 being located in least significant byte which is replayed first", so the words
		// are pushed little endian, byte 0 first.
		for (int i = 0; i < count; i++)
		{
			u32 word = words[i];
			for (int byte = 0; byte < 4; byte++)
				Write8(0xA0 + which * 4 + byte, (u8)(word >> (byte * 8)));
		}

		// A refill of four words satisfies the request; the DMA engine also calls
		// ClearFifoRequest after servicing it, this only keeps the flag honest if it does not.
		if (fifoCount[which] >= FifoRequestLevel)
			fifoRequest[which] = false;
	}

	// ---------------------------------------------------------------------------------------
	// The clock
	// ---------------------------------------------------------------------------------------

	void Apu::Tick(GbaBus& bus, int cycles)
	{
		if (cycles <= 0)
			return;

		// The sample clock counts system cycles scaled by the host rate: a sample boundary is
		// then always the fixed threshold CyclesPerSecond and the number of samples produced is
		// exact for every host rate, including the fractional ones (48000 Hz divides 16.78 MHz
		// with a remainder, so a plain cycle counter would drift).
		int start = cycleAccum;
		u64 scaled = (u64)start + (u64)cycles * (u64)sampleRate;
		int frames = (int)(scaled / CyclesPerSecond);
		cycleAccum = (int)(scaled % CyclesPerSecond);

		// The sample boundaries sit at k*CyclesPerSecond/sampleRate cycles; the exact integer length
		// of the sample that ends at boundary j is the difference of the two floors (Bresenham), so
		// the channel phases stay in step with the sample clock instead of drifting by the
		// fractional part on every sample. Boundary 0 lies start/sampleRate cycles *before* this
		// slice, so its floor is the ceiling of that offset negated - not simply -1: the caller
		// usually ticks the APU in slices shorter than a sample (the bus uses 64 cycles), so the
		// accumulator is normally a large part of a sample period and the correction is the whole
		// elapsed part of it.
		s64 prevFloor = -(((s64)start + sampleRate - 1) / sampleRate);
		for (int j = 1; j <= frames; j++)
		{
			s64 hi = ((s64)j * CyclesPerSecond - start) / sampleRate;
			s64 lo = (j == 1) ? prevFloor : (((s64)(j - 1) * CyclesPerSecond - start) / sampleRate);
			sampleCounter = (int)(hi - lo);
			OutputSample(bus);
		}

		// The 512 Hz frame sequencer is advanced after the slice's samples have been mixed: a
		// step that falls exactly on a slice boundary then acts on the following slice, so the
		// error is at most one slice (in practice a few cycles, the bus ticks per instruction).
		frameSeqClock += cycles;
		while (frameSeqClock >= FrameSequencerCycles)
		{
			frameSeqClock -= FrameSequencerCycles;
			ClockFrameSequencer();
			frameSeqStep = (frameSeqStep + 1) & 7;
		}

		// GBATEK "DMA-Sound Playback Procedure": "If FIFO contains only 4 x 32bits (16 bytes)
		// then Request more data per DMA". The request is a level (not an edge): a FIFO that is
		// short asks again on every slice, so a DMA channel that the program arms *after* the
		// FIFO has already gone short still gets the request. A DMA that does refill the FIFO
		// raises the level above 16 bytes, which stops the asking until it drops again.
		for (int which = 0; which < 2; which++)
		{
			if (fifoEnabled[which] && fifoCount[which] < FifoRequestLevel)
			{
				fifoRequest[which] = true;
				bus.dma.OnFifoRequest(bus, which);
			}
			else
			{
				fifoRequest[which] = false;
			}
		}
	}

	int Apu::ReadSamples(s16* out, int maxFrames)
	{
		// The queue is the only source of mixed audio, so draining it partially hands the frames
		// over in order and keeps the rest for the next call: nothing is duplicated or lost.
		int available = (int)(pending.size() / 2);
		int count = (maxFrames < available) ? maxFrames : available;
		if (count <= 0)
			return 0;

		memcpy(out, pending.data(), (size_t)count * 2 * sizeof(s16));
		pending.erase(pending.begin(), pending.begin() + (size_t)count * 2);
		return count;
	}

	// ---------------------------------------------------------------------------------------
	// The mixer
	// ---------------------------------------------------------------------------------------

	int Apu::OutputSample(GbaBus& bus)
	{
		// The four legacy channels are stepped by one host sample here, the two FIFOs are
		// clocked by their timers inside MixFifo (it needs the bus for the timer counters).
		// MixLegacy returns the four levels packed as offset bytes because the header's helper
		// returns a single int and every channel has to be routed through its own NR51 bits.
		int packed = MixLegacy(bus);
		int psg[4];
		for (int i = 0; i < 4; i++)
			psg[i] = (int)(((u32)packed >> (i * 8)) & 0xFF) - 128;
		MixFifo(bus);

		// SOUNDCNT_H bits 0-1 scale the four PSGs: 0 = 25 %, 1 = 50 %, 2 = 100 %, 3 is
		// "prohibited" in GBATEK (the hardware effectively behaves like 100 %, which is what
		// this does).
		int psgShift = 2 - (int)(soundcntH & 3);
		if (psgShift < 0)
			psgShift = 0;

		int right = 0;
		int left = 0;

		for (int i = 0; i < 4; i++)
		{
			int level = psg[i];
			if (psgShift == 2)
				level >>= 2;
			else if (psgShift == 1)
				level >>= 1;

			// SOUNDCNT_L bits 8-11 enable channels 1-4 on the right, bits 12-15 on the left.
			if (Bit(soundcntL, 8 + i))
				right += level;
			if (Bit(soundcntL, 12 + i))
				left += level;
		}

		// The FIFOs are routed by SOUNDCNT_H bits 8/9 (A) and 12/13 (B). They span the full
		// output range, twice a PSG channel's quarter (GBATEK "Max Output Levels").
		if (Bit(soundcntH, 8))
			right += fifoOutput[0];
		if (Bit(soundcntH, 9))
			left += fifoOutput[0];
		if (Bit(soundcntH, 12))
			right += fifoOutput[1];
		if (Bit(soundcntH, 13))
			left += fifoOutput[1];

		// SOUNDCNT_L bits 0-2 (right) and 4-6 (left) are the master volume: 0 mutes the side
		// completely and 7 is 100 %, the six other values being 1/8 to 6/8 of full scale.
		int masterRight = soundcntL & 7;
		int masterLeft = (soundcntL >> 4) & 7;
		right = masterRight ? ((right * (masterRight + 1)) >> 3) : 0;
		left = masterLeft ? ((left * (masterLeft + 1)) >> 3) : 0;

		s16 outLeft = (s16)Clamp16(left * MixScale);
		s16 outRight = (s16)Clamp16(right * MixScale);

		// Cap the queue: a frontend that stops draining must not make the mixer grow without
		// bound. The newest frame is dropped once the cap is reached.
		if ((int)pending.size() < sampleRate * 2 * PendingSeconds)
		{
			pending.push_back(outLeft);		// interleaved L/R, as ReadSamples documents
			pending.push_back(outRight);
			return 1;
		}

		return 0;
	}

	int Apu::MixLegacy(GbaBus& bus)
	{
		// The four legacy channels, in the order channel 1, 2, 3, 4. The result travels as four
		// offset bytes (level + 128, levels are in -120..+120) because OutputSample has to apply
		// the NR51 panning of every channel separately.
		int levels[4];
		levels[0] = MixSquare(square[0]);
		levels[1] = MixSquare(square[1]);
		levels[2] = MixWave(bus);
		levels[3] = MixNoise(bus);

		u32 packed = (u32)(levels[0] + 128) | ((u32)(levels[1] + 128) << 8) |
			((u32)(levels[2] + 128) << 16) | ((u32)(levels[3] + 128) << 24);
		return (int)packed;
	}

	int Apu::MixFifo(GbaBus& bus)
	{
		int sum = 0;

		for (int which = 0; which < 2; which++)
		{
			if (!fifoEnabled[which])
			{
				// An unrouted FIFO is silent and does not consume its samples; the timer reading
				// starts over when it is routed again.
				fifoOutput[which] = 0;
				fifoAccum[which] = -1;
				continue;
			}

			// The FIFO moves one byte to the output latch per overflow of timer 0 or timer 1
			// (SOUNDCNT_H bit 10 for A, bit 14 for B). A timer overflow is the counter wrapping
			// around from FFFFh to its reload value, so the previous reading has to be
			// remembered to see it; `fifoAccum` holds that reading (-1 = not taken yet).
			int timer = fifoTimerSelect[which] ? 1 : 0;
			u16 now = bus.timers.Counter(timer);
			if (fifoAccum[which] < 0)
			{
				fifoAccum[which] = now;
			}
			else
			{
				if (now < (u16)fifoAccum[which])
				{
					// Timer overflow: "Move 8bit data from FIFO to sound circuit". An empty FIFO
					// keeps the last sample it played (the latch) rather than going silent.
					if (fifoCount[which] > 0)
					{
						fifoLatchedSample[which] = fifo[which][fifoHead[which]];
						fifoHead[which] = (fifoHead[which] + 1) % FifoSize;
						fifoCount[which]--;
					}
				}
				fifoAccum[which] = now;
			}

			// The sample is a signed 8 bit value (-128..+127, GBATEK "Sound Channel A and B")
			// that spans the full output range, i.e. twice a PSG channel's quarter, and
			// SOUNDCNT_H bit 2/3 selects 50 % or 100 % for it.
			int sample = (int)(s8)fifoLatchedSample[which] << 1;
			if (!fifoVolume[which])
				sample >>= 1;
			fifoOutput[which] = sample;
			sum += sample;
		}

		return sum;
	}

	int Apu::MixSquare(SquareChannel& ch)
	{
		// A channel that is off (or whose DAC is off) outputs the digital zero of the GBA's
		// mixer, which is silence here (Pan Docs: on the GBA a disabled "DAC" behaves like an
		// enabled one receiving 0).
		if (!ch.enabled || !ch.dacEnabled)
		{
			ch.sample = 0;
			return 0;
		}

		// The level the generator holds at the start of this host sample; the clock is advanced
		// afterwards, so the sample that *ends* at a step boundary still belongs to the old step
		// (a zero order hold of the level at the sample's start).
		// The envelope's volume scales the level; the GBA's digital mixer gives a channel at
		// volume v a range of +/-v*8 (GBATEK: each PSG spans a quarter of the output range).
		int high = DutyWaveform(ch.duty, ch.dutyStep);
		ch.sample = (high ? ch.volume : -ch.volume) * 8;

		// The duty step advances at eight times the channel's frequency (Pan Docs "Pulse
		// channels"): the frequency is 131072/(2048-n) Hz, so one duty step lasts
		// 16 * (2048 - n) system cycles and a full 8 step waveform 128 * (2048 - n).
		int period = 16 * (2048 - (ch.frequency & 0x7FF));
		ch.phaseAccum += sampleCounter;
		while (ch.phaseAccum >= period)
		{
			ch.phaseAccum -= period;
			ch.dutyStep = (ch.dutyStep + 1) & 7;
		}

		return ch.sample;
	}

	int Apu::MixWave(GbaBus& bus)
	{
		if (!waveEnabled || !waveDacEnabled)
		{
			waveSample = 0;
			return 0;
		}

		// The level the generator holds at the start of this host sample (the clock is advanced
		// afterwards, so the digit that a step boundary ends on is still the old one).
		// NR32 bits 5-6 shift the digital value: 0 = mute, 1 = 100 %, 2 = 50 %, 3 = 25 %, and
		// bit 7 forces 75 % (GBATEK "SOUND3CNT_H"). The shift is applied to the signed level
		// here (the hardware shifts the digital value, which biases it towards 0).
		int sample = waveSample * 16 - 120;
		if (waveForceVolume)
			sample = (sample * 3) >> 2;
		else if (waveVolume == 0)
			sample = 0;
		else
			sample >>= (waveVolume - 1);

		// The digit position advances either at the NR33 sample rate (2097152/(2048-n) digits
		// per second, i.e. 8*(2048-n) cycles per digit) or, on the GBA, at the overflow of a
		// timer. SOUND3CNT_X bit 14 (the DMG's length flag, which also stays in use) both
		// enables the timer path and selects the timer: timer 1 when it is set, timer 0 when it
		// is clear. The timer path is only taken while that timer is running, so a program that
		// merely enables the length flag keeps the NR33 sample rate.
		int timerIndex = Bit(sound3cntX, 14);
		int advances = 0;

		if (bus.timers.Running(timerIndex))
		{
			u16 now = bus.timers.Counter(timerIndex);
			if (lastTimerValue == TimerNotPrimed)
			{
				lastTimerValue = now;		// nothing to compare against yet
			}
			else
			{
				if (now < lastTimerValue)
					advances = 1;			// the counter wrapped: a timer overflow
				lastTimerValue = now;
			}
		}
		else
		{
			int period = 8 * (2048 - (waveFrequency & 0x7FF));
			wavePhase += sampleCounter;
			while (wavePhase >= period)
			{
				wavePhase -= period;
				advances++;
			}
		}

		for (int i = 0; i < advances; i++)
		{
			// NR30 bit 5 = 0 plays one bank of 32 digits, = 1 plays both banks (64 digits).
			// GBATEK's two 16 byte banks need 32 bytes of RAM; the frozen header only has 16, so
			// the second bank is the same 16 bytes (the bank bit still selects where the 64 digit
			// pass starts, which is what the hardware rule asks for).
			wavePosition = (wavePosition + 1) % (waveDimension ? 64 : 32);
			waveSample = WaveDigit(waveRam, wavePosition);
		}

		return sample;
	}

	int Apu::MixNoise(GbaBus&)
	{
		// The bus is not needed by the noise generator; it is part of the helper's signature.
		if (!noiseEnabled || !noiseDacEnabled)
		{
			noiseSample = 0;
			return 0;
		}

		// The level the generator holds at the start of this host sample; the LFSR is clocked
		// afterwards, so a clock edge that falls on the sample boundary shows up in the next
		// sample.
		int level = noiseSample ? noiseEnvelopeVolume : -noiseEnvelopeVolume;

		// NR43: r selects the divider and s the clock shift, so one LFSR step lasts
		// divisor[r] * 2^s system cycles / 4 (see the table at the top). A clock shift of 14 or
		// 15 stops the LFSR altogether (Pan Docs "Obscure Behavior"). The rate is checked as well
		// so that the loop can never divide by zero, whatever the register bookkeeping did.
		if (noiseShift < 14 && noiseSampleRate > 0)
		{
			noisePhase += sampleCounter;
			while (noisePhase >= noiseSampleRate)
			{
				noisePhase -= noiseSampleRate;

				// GBATEK "Noise Random Generator": X = X SHR 1, if carry (the bit that was
				// shifted out) then Out = HIGH and X = X XOR 60h (7 bit) or 6000h (15 bit),
				// else Out = LOW.
				int bit = noiseLfsr & 1;
				noiseLfsr >>= 1;
				if (bit)
					noiseLfsr ^= noiseWidth ? 0x60 : 0x6000;
				noiseSample = bit ? 1 : 0;
			}
		}

		return level * 8;
	}

	// ---------------------------------------------------------------------------------------
	// Triggering
	// ---------------------------------------------------------------------------------------

	void Apu::TriggerSquare(int index)
	{
		SquareChannel& ch = square[index];
		u16 control = (index == 0) ? sound1cntX : sound2cntH;
		u8 nrx1 = (u8)((index == 0) ? (sound1cntH & 0x3F) : (sound2cntL & 0x3F));

		// A channel is only activated when its DAC is on; otherwise the trigger forces it off
		// (Pan Docs "Audio Details": "A channel is activated by a write to NRx4's MSB, unless
		// its DAC is off, which forces it to be disabled as well").
		if (!ch.dacEnabled)
		{
			ch.enabled = false;
			return;
		}

		// NRx1's length is reloaded on every trigger: the counter runs the (64 - n) 256 Hz
		// steps down to zero and stops the channel when it gets there.
		bool lengthWasEnabled = ch.lengthEnabled;
		ch.length = 64 - nrx1;
		ch.lengthEnabled = Bit(control, 14) != 0;

		// Extra length clocking (Pan Docs "Obscure Behavior"): if this trigger lands right
		// before a frame sequencer step that does not clock the length, the length is clocked
		// once now anyway - provided it was disabled before, which is what makes this the "first
		// step after the trigger does not clock the length" case. It is also what turns the 64
		// tick reload (n = 0) into 63. The decrement cannot stop the channel here because the
		// trigger bit is set.
		if (ch.lengthEnabled && !lengthWasEnabled && (frameSeqStep % 2) != 0)
		{
			if (ch.length > 0)
				ch.length--;
		}

		// NRx2 restarts the envelope from its initial volume, with a step time of 0 counting as
		// 8 (the envelope is disabled by a step time of 0, Pan Docs "Audio Details").
		ch.volume = ch.envelopeVolume;
		ch.envelopeTimer = ch.envelopePeriod ? ch.envelopePeriod : 8;

		// Retriggering resets the duty step *timer* but not the duty step itself: the step
		// counter can only be reset by turning the APU off (Pan Docs "Pulse channels"). The
		// "the low two bits of the frequency timer are not modified" detail of the DMG is not
		// modelled: the accumulator restarts at zero.
		ch.phaseAccum = 0;
		ch.enabled = true;

		if (index == 0)
		{
			// The sweep unit (Pan Docs "Pulse channel with sweep"). On a trigger the live
			// frequency is copied into the shadow register, the sweep timer is reloaded (0 counts
			// as 8) and, if the individual step is non-zero, the frequency calculation and the
			// overflow check run immediately (the result is only checked, not written back).
			ch.shadowFrequency = ch.frequency & 0x7FF;
			ch.sweepTimer = ch.sweepPeriod ? ch.sweepPeriod : 8;
			ch.sweepNegateUsed = false;

			if (ch.sweepShift != 0)
			{
				int calculated = SweepFrequency(ch.shadowFrequency, ch.sweepShift, ch.sweepUp);
				if (calculated < 0 || calculated > 2047)
					ch.enabled = false;
			}
		}
	}

	void Apu::TriggerWave()
	{
		// Channel 3's DAC is NR30 bit 7, so it is already off (and the channel stopped) when
		// waveDacEnabled is clear.
		if (!waveDacEnabled)
		{
			waveEnabled = false;
			return;
		}

		// NR31's length: units of (256 - n)/256 s, with the same extra-length rule as the
		// square channels.
		bool lengthWasEnabled = waveLengthEnabled;
		waveLength = 256 - (sound3cntH & 0xFF);
		waveLengthEnabled = Bit(sound3cntX, 14) != 0;
		if (waveLengthEnabled && !lengthWasEnabled && (frameSeqStep % 2) != 0)
		{
			if (waveLength > 0)
				waveLength--;
		}

		// "On a trigger the sample position resets to 0" and the pattern restarts at the bank
		// NR30 bit 6 selects (GBATEK "SOUND3CNT_L"). Unlike the DMG, the GBA does not corrupt
		// the first wave RAM byte when the channel is retriggered while reading a sample (DMG
		// behavior documented in the Pan Docs "Obscure Behavior", GBA difference in GBATEK
		// "GBA Unpredictable Things" 9.3.3): nothing is written to waveRam here.
		wavePosition = 0;
		waveSample = WaveDigit(waveRam, wavePosition);
		wavePhase = 0;
		lastTimerValue = TimerNotPrimed;
		waveEnabled = true;
	}

	void Apu::TriggerNoise()
	{
		if (!noiseDacEnabled)
		{
			noiseEnabled = false;
			return;
		}

		bool lengthWasEnabled = noiseLengthEnabled;
		noiseLength = 64 - (sound4cntL & 0x3F);
		noiseLengthEnabled = Bit(sound4cntH, 14) != 0;
		if (noiseLengthEnabled && !lengthWasEnabled && (frameSeqStep % 2) != 0)
		{
			if (noiseLength > 0)
				noiseLength--;
		}

		// NR42's initial volume restarts the envelope (a step time of 0 disables it).
		noiseEnvelopeVolume = (sound4cntL >> 12) & 0xF;
		noiseEnvelopeTimer = noiseEnvelopePeriod ? noiseEnvelopePeriod : 8;

		// The LFSR restarts at 40h (7 bit) or 4000h (15 bit) (GBATEK "Noise Random Generator"),
		// and its bit 0 is the level the channel outputs before the first clock edge.
		noiseLfsr = noiseWidth ? 0x40 : 0x4000;
		noiseSample = noiseLfsr & 1;
		noisePhase = 0;

		// The rate the mixer divides by is derived from NR43, so it has to be computed here as
		// well as on the NR43 write: a trigger that follows a write the master enable swallowed
		// (the registers at 0x060-0x081 are held at zero while SOUNDCNT_X bit 7 is clear) would
		// otherwise leave the channel enabled with a divisor of zero, and the mixer would never
		// leave its clocking loop.
		noiseSampleRate = NoiseDivisor[noiseDivisor & 7] * NoiseGbaScale * (1 << (noiseShift & 0xF));
		noiseEnabled = true;
	}

	// ---------------------------------------------------------------------------------------
	// The frame sequencer
	// ---------------------------------------------------------------------------------------

	void Apu::ClockFrameSequencer()
	{
		// Pan Docs "DIV-APU": the sequencer steps at 512 Hz; the length counters are clocked by
		// every second step (256 Hz), the sweep by every fourth (steps 2 and 6, 128 Hz) and the
		// envelopes by the last step (64 Hz).
		switch (frameSeqStep)
		{
		case 0:
		case 4:
			ClockLengths();
			break;

		case 2:
		case 6:
			ClockLengths();

			// Channel 1's frequency sweep (Pan Docs "Pulse channel with sweep"). The sweep is
			// enabled when either the pace or the individual step is non-zero; with a pace of 0
			// the timer counts 8 instead, so the sweep stays off.
			if (square[0].sweepTimer > 0)
				square[0].sweepTimer--;

			if (square[0].sweepTimer == 0)
			{
				square[0].sweepTimer = square[0].sweepPeriod ? square[0].sweepPeriod : 8;

				if (square[0].sweepPeriod != 0 && square[0].sweepShift != 0 && square[0].enabled)
				{
					// The calculation always starts from the shadow register, so a program that
					// pokes NR13/NR14 in between loses those writes at the next sweep step.
					int shadow = square[0].shadowFrequency & 0x7FF;
					if (!square[0].sweepUp)
						square[0].sweepNegateUsed = true;

					int calculated = SweepFrequency(shadow, square[0].sweepShift, square[0].sweepUp);
					if (calculated < 0 || calculated > 2047)
					{
						square[0].enabled = false;
					}
					else
					{
						// Write the new frequency back into the shadow register and into
						// NR13/NR14, then run the calculation and the overflow check once more
						// (that second result is only checked, never written back).
						square[0].shadowFrequency = calculated;
						square[0].frequency = calculated;

						int check = SweepFrequency(calculated, square[0].sweepShift, square[0].sweepUp);
						if (check < 0 || check > 2047)
							square[0].enabled = false;
					}
				}
			}
			break;

		case 7:
			ClockEnvelopes();
			break;

		default:
			break;
		}
	}

	void Apu::ClockLengths()
	{
		CountLength(square[0]);
		CountLength(square[1]);

		// Channel 3's length counts the same 256 Hz steps, up to 256 of them.
		if (waveLengthEnabled)
		{
			if (waveLength > 0)
				waveLength--;
			if (waveLength == 0)
				waveEnabled = false;
		}

		if (noiseLengthEnabled)
		{
			if (noiseLength > 0)
				noiseLength--;
			if (noiseLength == 0)
				noiseEnabled = false;
		}
	}

	void Apu::ClockEnvelopes()
	{
		// 64 Hz: channels 1, 2 and 4 step their volume towards the limit once every
		// `envelopePeriod` steps (a period of 0 disables the envelope).
		for (int i = 0; i < 2; i++)
		{
			SquareChannel& ch = square[i];
			if (ch.envelopePeriod == 0)
				continue;

			if (ch.envelopeTimer > 0)
				ch.envelopeTimer--;
			if (ch.envelopeTimer == 0)
			{
				ch.envelopeTimer = ch.envelopePeriod;
				if (ch.envelopeUp)
				{
					if (ch.volume < 15)
						ch.volume++;
				}
				else
				{
					if (ch.volume > 0)
						ch.volume--;
				}
			}
		}

		if (noiseEnvelopePeriod != 0)
		{
			if (noiseEnvelopeTimer > 0)
				noiseEnvelopeTimer--;
			if (noiseEnvelopeTimer == 0)
			{
				noiseEnvelopeTimer = noiseEnvelopePeriod;
				if (noiseEnvelopeUp)
				{
					if (noiseEnvelopeVolume < 15)
						noiseEnvelopeVolume++;
				}
				else
				{
					if (noiseEnvelopeVolume > 0)
						noiseEnvelopeVolume--;
				}
			}
		}
	}

	void Apu::CountLength(SquareChannel& ch)
	{
		if (!ch.lengthEnabled)
			return;

		if (ch.length > 0)
			ch.length--;
		if (ch.length == 0)
			ch.enabled = false;
	}

	int Apu::DutyWaveform(int duty, int step)
	{
		return (DutyPattern[duty & 3] >> (step & 7)) & 1;
	}
}
