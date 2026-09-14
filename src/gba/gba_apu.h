// GBA sound: the two "legacy" Game Boy compatible PSG channels plus the wave channel and the
// noise channel (SOUND1CNT .. SOUND4CNT) and the two direct-sound FIFO channels A and B
// (SOUNDCNT_H, FIFO_A at 0x040000A0 and FIFO_B at 0x040000A4).
//
// Implemented from GBATEK "GBA Sound" and the DMG audio chapter of the Pan Docs (the four
// legacy channels are the Game Boy's APU with the GBA's frequency range and without the
// envelope/ sweep differences noted in GBATEK 9.1).
//
// The APU runs at the host's sample rate (32768 Hz by default, the rate the GBA mixes at). The
// FIFO channels are fed by DMA in "special" timing: the APU raises a request when a FIFO holds
// fewer than 16 bytes, and the DMA refills it with four words.

#pragma once

#include "gba_types.h"

namespace GBA
{
	class GbaBus;

	class Apu
	{
	public:
		void Reset();

		/// <summary>Set the host sample rate (the mixer resamples from the channel rates).</summary>
		void SetSampleRate(int hz);
		int SampleRate() const { return sampleRate; }

		/// <summary>Read a sound register (offset relative to 0x04000000, 0x060..0x0A7).</summary>
		u8 Read8(u32 offset, u8 openBus) const;

		/// <summary>Write a sound register (8-bit; GBA sound registers are byte wide).</summary>
		void Write8(u32 offset, u8 value);

		/// <summary>Advance the APU by `cycles` and produce the samples for them.</summary>
		void Tick(GbaBus& bus, int cycles);

		/// <summary>Drain up to `maxFrames` stereo sample frames (interleaved L/R, 16-bit).</summary>
		int ReadSamples(s16* out, int maxFrames);

		/// <summary>True when FIFO `which` (0 = A, 1 = B) needs a DMA refill.</summary>
		bool FifoRequest(int which) const { return fifoRequest[which]; }

		/// <summary>Append the four words the DMA transferred into the FIFO.</summary>
		void FifoDmaDone(int which, const u32* words, int count);

		/// <summary>Clear the DMA request flag (the DMA engine calls this after servicing).</summary>
		void ClearFifoRequest(int which) { fifoRequest[which] = false; }

		/// <summary>The master enable (SOUNDCNT_X bit 7), for the harness.</summary>
		bool Enabled() const { return (soundcntX & 0x80) != 0; }

	private:
		// -- registers ---------------------------------------------------------------------

		u16 sound1cntL = 0, sound1cntH = 0, sound1cntX = 0;
		u16 sound2cntL = 0, sound2cntH = 0;
		u16 sound3cntL = 0, sound3cntH = 0, sound3cntX = 0;
		u16 sound4cntL = 0, sound4cntH = 0;
		u16 soundcntL = 0, soundcntH = 0, soundcntX = 0;
		u16 soundbias = 0x200;
		u8 waveRam[16]{};

		// -- state -------------------------------------------------------------------------

		int sampleRate = 32768;
		int cycleAccum = 0;			// cycles towards the next output sample
		std::vector<s16> pending;	// mixed samples waiting for ReadSamples

		// The four legacy channels.
		struct SquareChannel
		{
			bool enabled = false;
			bool dacEnabled = false;
			int frequency = 0;		// 11-bit NR13/NR14, 131072/(2048-n) Hz
			int shadowFrequency = 0;	// the sweep unit's shadow copy (see TriggerSquare)
			int duty = 0;
			int dutyStep = 0;
			int length = 0;
			bool lengthEnabled = false;
			int volume = 0;
			int envelopeVolume = 0;
			int envelopePeriod = 0;
			int envelopeTimer = 0;
			bool envelopeUp = false;
			int sweepPeriod = 0;
			int sweepShift = 0;
			int sweepTimer = 0;
			bool sweepUp = false;
			bool sweepNegateUsed = false;
			int phaseAccum = 0;
			int sample = 0;
		};

		SquareChannel square[2];
		int noiseSample = 0;

		// Channel 3 (wave)
		bool waveEnabled = false;
		bool waveDacEnabled = false;
		int waveFrequency = 0;
		int waveDimension = 0;		// 0 = 32 samples (two banks), 1 = 64 samples
		int waveBank = 0;
		int waveVolume = 0;			// 0 = mute, 1 = 100%, 2 = 50%, 3 = 25%
		bool waveForceVolume = false;
		int wavePhase = 0;
		int waveSample = 0;
		int wavePosition = 0;		// digit 0..31 (32 samples) or 0..63 (64 samples)
		int waveLength = 0;			// 256 Hz steps left before the channel stops
		bool waveLengthEnabled = false;
		u16 lastTimerValue = 0;

		// Channel 4 (noise)
		bool noiseEnabled = false;
		bool noiseDacEnabled = false;
		int noiseFrequency = 0;
		int noiseDivisor = 0;
		int noiseWidth = 0;
		int noiseShift = 0;
		int noisePhase = 0;
		int noiseEnvelopeVolume = 0;
		int noiseEnvelopePeriod = 0;
		int noiseEnvelopeTimer = 0;
		bool noiseEnvelopeUp = false;
		u16 noiseLfsr = 0x7FFF;
		bool noiseLengthEnabled = false;
		int noiseLength = 0;
		int noiseSampleRate = 0;

		// FIFO channels
		u8 fifo[2][32]{};
		int fifoHead[2]{}, fifoTail[2]{}, fifoCount[2]{};
		bool fifoRequest[2]{};
		bool fifoEnabled[2]{};
		int fifoVolume[2]{};
		bool fifoTimerSelect[2]{};	// 0 = timer 0, 1 = timer 1
		bool fifoTimerRight[2]{};
		bool fifoLeftOnly[2]{}, fifoRightOnly[2]{}, fifoTimerA[2]{};
		int fifoOutput[2]{};
		int fifoAccum[2]{};
		int fifoLatchedSample[2]{};

		int sampleCounter = 0;

		// The frame sequencer (512 Hz, Pan Docs "DIV-APU"):
		int frameSeqStep = 0;		// the index of the next step to execute (0..7)
		int frameSeqClock = 0;		// cycles towards that step

		// -- helpers -----------------------------------------------------------------------

		int OutputSample(GbaBus& bus);
		int MixLegacy(GbaBus& bus);
		int MixFifo(GbaBus& bus);
		int MixSquare(SquareChannel& ch);
		int MixWave(GbaBus& bus);
		int MixNoise(GbaBus& bus);

		void TriggerSquare(int index);
		void TriggerWave();
		void TriggerNoise();
		void ClockFrameSequencer();
		void ClockLengths();
		void ClockEnvelopes();

		void CountLength(SquareChannel& ch);
		static int DutyWaveform(int duty, int step);
	};
}
