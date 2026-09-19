// Game Boy sound: the four channels of the DMG/CGB sound controller - two pulse channels with
// duty, envelope and length (channel 1 with the sweep unit), the wave channel with its 32 sample
// RAM, and the noise channel with its 15/7 bit LFSR - plus NR50/NR51 mixing and a resampler down
// to the host's sample rate.
//
// Written from the Pan Docs "Audio", "Audio Registers" and "Audio Details" pages, which the
// Game Boy and Game Boy Advance programming manuals agree with (they state the same rules as
// durations: the sound length is (64 - st) / 256 s, the envelope step is n / 64 s, the sweep step
// is n / 128 s, and the pulse frequency is 4194304 / (4 * 8 * (2048 - fdat)) Hz). Modelled:
//
//  * the four channels, each with the period divider the manual describes: the pulse divider is
//    clocked at 1048576 Hz (one 11-bit period of 2048 - x every four system clocks) and its duty
//    waveform has eight steps, the wave divider at 2097152 Hz with 32 samples, and the noise
//    LFSR at 262144 / (divisor * 2^shift) Hz;
//  * the duty waveforms with their phases (12.5 % = 00000001 .. 75 % = 01111110, the first step
//    of the eight being the leftmost);
//  * the 512 Hz frame sequencer: the length at 256 Hz (steps 0, 2, 4 and 6), the sweep at 128 Hz
//    (steps 2 and 6) and the envelope at 64 Hz (step 7). Its rate does not change in double speed;
//  * the length timer: it holds the value written in NRx1 (the length is (64 - st) / 256 s, 256
//    steps for the wave channel) and ticks up until it reaches the channel's maximum, which turns
//    the channel off. A trigger only re-arms a counter that has run out;
//  * the trigger behaviour, the sweep unit (shadow register, the immediate overflow check, the
//    "negate and clear" quirk), the envelope, the LFSR, the wave RAM (including the sample 0 the
//    wave channel skips on a trigger), the DAC enable rules, NR50's master volume, the PCM12 and
//    PCM34 registers of the CGB, and the output high pass filter.
//
// Deliberately not modelled (the Pan Docs describe all of these as hardware quirks that a
// cartridge must not rely on):
//
//  * **Zombie mode** ("Audio Details"): a write to NRx2 while a channel plays can change the live
//    volume. The write is stored but the channel keeps its volume until the next trigger.
//  * **The extra length clock and the 63/255 trigger quirk** ("Audio Details"): a trigger always
//    reloads the length counter from the register instead of the one-off 63/255 value, and a
//    write to NRx4 does not clock the length an extra time.
//  * **The DMG wave RAM corruption on a retrigger** ("Audio Details"): the wave RAM is never
//    corrupted, on either model.
//  * **The "first duty step after the first trigger plays as 0"** behaviour.
//  * **The VIN input** (NR50 bits 3 and 7): there is no external sound hardware in the emulator,
//    and no licensed cartridge used it.
//  * **The noise LFSR lock-up** ("Audio Details"): switching from 15 to 7 bits while the active
//    part of the LFSR is all ones silences the channel on hardware; here it keeps playing.
//  * **The DAC fade** when a DAC is turned off: the Pan Docs note that the fade varies between
//    models, so a disabled DAC contributes silence at once.

#pragma once

#include "gba_types.h"

// The LCD header has the clock constant (the APU and the LCD run on the same 4.194304 MHz clock);
// including it also avoids repeating the constant in two places.
#include "gb_ppu.h"

#include <vector>

namespace GBA
{
	class GbApu
	{
	public:
		void Reset();

		/// <summary>Set the host sample rate; the mixer resamples the channel rates to it.</summary>
		void SetSampleRate(int hz);
		int SampleRate() const { return sampleRate; }

		/// <summary>
		/// Tell the APU which console it is running on: the CGB's high pass filter is more
		/// aggressive than the DMG's (Pan Docs "Audio Details": 0.998943 against 0.999958), and
		/// only a CGB answers the PCM12/PCM34 registers.
		/// </summary>
		void SetCgb(bool value);

		/// <summary>True when the APU models a CGB (the tests and the debugger read this).</summary>
		bool Cgb() const { return cgb; }

		/// <summary>Advance the APU by `cycles` of the 4.194304 MHz system clock.</summary>
		void Tick(int cycles);

		/// <summary>Read a sound register (the full 0xFF10..0xFF3F address).</summary>
		uint8_t ReadRegister(uint16_t address) const;

		/// <summary>Read PCM12 (0xFF76) or PCM34 (0xFF77): the digital outputs of the channels,
		/// which is what a test cartridge checks the generation circuits with.</summary>
		uint8_t ReadPcm(uint16_t address) const;

		/// <summary>Write a sound register (the full 0xFF10..0xFF3F address).</summary>
		void WriteRegister(uint16_t address, uint8_t value);

		/// <summary>Drain up to `maxFrames` stereo sample frames (interleaved left, right).</summary>
		int ReadSamples(int16_t* out, int maxFrames);

		/// <summary>The master enable (NR52 bit 7).</summary>
		bool Enabled() const { return powered; }

		/// <summary>The four channel status bits (NR52 bits 0-3), a channel being "on".</summary>
		uint8_t ChannelStatus() const;

		/// <summary>The digital output of a channel, 0..15 (the CGB's PCM12/PCM34 registers, and
		/// the unit tests, read this).</summary>
		int ChannelOutput(int channel) const;

		/// <summary>How many sample frames are waiting for ReadSamples.</summary>
		int PendingSamples() const { return (int)pending.size() / 2; }

	private:
		// -- the four channels ---------------------------------------------------------------

		struct Pulse
		{
			bool active = false;
			bool dacEnabled = false;
			bool lengthEnabled = false;
			bool sweepEnabled = false;		// the sweep unit's internal enable flag
			int duty = 0;					// NRx1 bits 7-6
			int dutyStep = 0;				// 0..7 into the duty waveform
			int lengthRegister = 0;			// NRx1 bits 5-0, the value a trigger re-arms with
			int lengthCounter = 64;			// ticks up to 64; 64 means "run out" (see Reset)
			int frequency = 0;				// 11 bits
			int timer = 0;					// the sample timer, counting down
			int volume = 0;					// the live volume, 0..15
			int initialVolume = 0;			// NRx2 bits 7-4, reloaded by a trigger
			int envelopePeriod = 0;
			int envelopeTimer = 0;
			bool envelopeIncreasing = false;
			bool envelopeRunning = false;
			int sweepPeriod = 0;			// NR10 bits 6-4
			int sweepShift = 0;				// NR10 bits 2-0
			bool sweepDecreasing = false;	// NR10 bit 3
			int sweepTimer = 0;
			int sweepShadow = 0;
			bool sweepNegateUsed = false;	// the "negate and clear" quirk needs this flag
		};

		struct Wave
		{
			bool active = false;
			bool dacEnabled = false;		// NR30 bit 7
			bool lengthEnabled = false;
			int lengthRegister = 0;			// NR31, the value a trigger re-arms with
			int lengthCounter = 256;		// ticks up to 256; 256 means "run out"
			int frequency = 0;
			int timer = 0;
			int volumeCode = 0;				// NR32 bits 6-5
			int position = 0;				// the sample being played, 0..31
			int sampleBuffer = 0;			// the held sample (Pan Docs: a sample buffer)
		};

		struct Noise
		{
			bool active = false;
			bool dacEnabled = false;
			bool lengthEnabled = false;
			int lengthRegister = 0;			// NR41 bits 5-0
			int lengthCounter = 64;			// ticks up to 64; 64 means "run out"
			int shift = 0;					// NR43 bits 7-4
			int divisorCode = 0;			// NR43 bits 2-0
			bool widthMode = false;			// NR43 bit 3: 1 = the 7 bit LFSR
			int timer = 0;
			int volume = 0;
			int initialVolume = 0;			// NR42 bits 7-4
			int envelopePeriod = 0;
			int envelopeTimer = 0;
			bool envelopeIncreasing = false;
			bool envelopeRunning = false;
			uint16_t lfsr = 0x7FFF;
		};

		Pulse pulse[2];
		Wave wave;
		Noise noise;

		// -- the global registers --------------------------------------------------------------

		uint8_t nr50 = 0x77;			// VIN and the master volume (the post-boot value)
		uint8_t nr51 = 0xF3;			// the channel routing (the post-boot value)
		bool powered = true;
		bool cgb = false;

		uint8_t waveRam[16]{};

		// -- timing ----------------------------------------------------------------------------

		int frameSequencerCycles = 0;	// towards the next 512 Hz frame sequencer step
		int frameStep = 0;				// the step about to be executed, 0..7

		int sampleRate = 48000;

		// The sample clock: every system clock adds `sampleRate` to this and a sample is taken
		// when the sum reaches GbCyclesPerSecond, which is exact for every host rate (32768 Hz
		// divides 4194304 exactly, 44100 and 48000 do not).
		int sampleAccum = 0;

		std::vector<int16_t> pending;		// interleaved stereo samples waiting for ReadSamples

		// The high pass filter of the two outputs (Pan Docs "Audio Details" gives the reference
		// implementation; the capacitor is dragged towards the signal by 0.999958 at the DMG's
		// 4194304 Hz and by 0.998943 on the MGB and CGB, rebased here for an arbitrary host rate).
		double capacitorLeft = 0.0;
		double capacitorRight = 0.0;
		double highPassCharge = 0.999958;

		// -- helpers ---------------------------------------------------------------------------

		void ClockFrameSequencer();
		void ClockLengths();
		void ClockEnvelopes();
		void ClockSweep(int channel);

		void TickPulse(Pulse& channel);
		void TickWaveChannel();
		void TickNoiseChannel();

		void TriggerPulse(int index);
		void TriggerWave();
		void TriggerNoise();

		/// <summary>The pulse divider is clocked at 1048576 Hz, i.e. once per four system clocks,
		/// and the tone spans eight duty steps: (2048 - x) * 4 clocks per step gives the
		/// 131072 / (2048 - x) Hz the manuals specify.</summary>
		void ReloadPulseTimer(Pulse& channel) { channel.timer = (2048 - channel.frequency) * 4; }

		/// <summary>The wave divider is clocked at 2097152 Hz (once per two system clocks) and the
		/// waveform has 32 samples: (2048 - x) * 2 clocks per sample gives 65536 / (2048 - x) Hz
		/// for the tone, half the pulse channel's.</summary>
		void ReloadWaveTimer() { wave.timer = (2048 - wave.frequency) * 2; }

		int NoisePeriod() const;

		int PulseOutput(const Pulse& channel) const;
		int WaveOutput() const;
		int NoiseOutput() const;

		void MixSample();
		void PowerOff();

		/// <summary>One 256 Hz length tick: the counter ticks up from the value NRx1 was written
		/// with and the channel is turned off when it reaches `maximum` (64, or 256 for the wave
		/// channel). A counter that has reached it stays there, which is what a trigger tests.</summary>
		static void ClockLength(bool enabled, int& counter, int maximum, bool& active)
		{
			if (!enabled || counter >= maximum)
				return;

			if (++counter >= maximum)
				active = false;
		}

		/// <summary>A trigger (re)arms a length counter only if it has run out ("If length timer
		/// expired it is reset", Pan Docs "Audio Registers"), so a retrigger keeps the remainder of
		/// a note that is still counting.</summary>
		static void ArmLength(int& counter, int registerValue, int maximum)
		{
			if (counter >= maximum)
				counter = registerValue;
		}

		/// <summary>The duty waveform of a pulse channel (Pan Docs "Audio Registers").</summary>
		static int DutyBit(int duty, int step);
	};
}
