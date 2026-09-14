// Game Boy sound: the four channels of the DMG/CGB sound controller - two pulse channels with
// duty, envelope and length (channel 1 with the sweep unit), the wave channel with its 32 sample
// RAM, and the noise channel with its 15/7 bit LFSR - plus NR50/NR51 mixing and a resampler down
// to the host's sample rate.
//
// Written from the Pan Docs "Audio", "Audio Registers" and "Audio Details" pages: the frequency
// formulas, the duty waveforms, the 512 Hz frame sequencer, the length/envelope/sweep clocks, the
// DAC rules and the high pass filter at the output.
//
// Modelled: the four channels, the 512 Hz frame sequencer (length at 256 Hz, sweep at 128 Hz,
// envelope at 64 Hz), the frequency formulas, the trigger behaviour, the LFSR, the wave RAM, the
// DAC enable rules, NR50/NR51 mixing and the output high pass filter.
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

		/// <summary>Advance the APU by `cycles` of the 4.194304 MHz system clock.</summary>
		void Tick(int cycles);

		/// <summary>Read a sound register (the full 0xFF10..0xFF3F address).</summary>
		u8 ReadRegister(u16 address) const;

		/// <summary>Write a sound register (the full 0xFF10..0xFF3F address).</summary>
		void WriteRegister(u16 address, u8 value);

		/// <summary>Drain up to `maxFrames` stereo sample frames (interleaved left, right).</summary>
		int ReadSamples(s16* out, int maxFrames);

		/// <summary>The master enable (NR52 bit 7).</summary>
		bool Enabled() const { return powered; }

		/// <summary>The four channel status bits (NR52 bits 0-3), a channel being "on".</summary>
		u8 ChannelStatus() const;

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
			int lengthCounter = 0;			// counts up to 64
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
			int lengthCounter = 0;			// counts up to 256
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
			int lengthCounter = 0;
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
			u16 lfsr = 0x7FFF;
		};

		Pulse pulse[2];
		Wave wave;
		Noise noise;

		// -- the global registers --------------------------------------------------------------

		u8 nr50 = 0x77;			// VIN and the master volume (the post-boot value)
		u8 nr51 = 0xF3;			// the channel routing (the post-boot value)
		bool powered = true;

		u8 waveRam[16]{};

		// -- timing ----------------------------------------------------------------------------

		int frameSequencerCycles = 0;	// towards the next 512 Hz frame sequencer step
		int frameStep = 0;				// the step about to be executed, 0..7
		int sampleCycles = 0;			// towards the next host sample

		int sampleRate = 48000;
		int cyclesPerSample = GbCyclesPerSecond / 48000;

		std::vector<s16> pending;		// interleaved stereo samples waiting for ReadSamples

		// The high pass filter of the two outputs (Pan Docs "Audio Details" gives the reference
		// implementation; the capacitor is dragged towards the signal by 0.999958 at the DMG's
		// 4194304 Hz, rebased here for an arbitrary host rate).
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

		void ReloadPulseTimer(Pulse& channel) { channel.timer = (2048 - channel.frequency) * 2; }
		void ReloadWaveTimer() { wave.timer = (2048 - wave.frequency); }

		int NoisePeriod() const;

		int PulseOutput(const Pulse& channel) const;
		int WaveOutput() const;
		int NoiseOutput() const;

		void MixSample();
		void PowerOff();

		/// <summary>The length a trigger loads: 64 minus the register for the pulse and noise
		/// channels, 256 minus it for the wave channel (Pan Docs "Audio Registers").</summary>
		static int LengthFromRegister(int value, int maximum) { return maximum - value; }

		/// <summary>The duty waveform of a pulse channel (Pan Docs "Audio Registers").</summary>
		static int DutyBit(int duty, int step);
	};
}
