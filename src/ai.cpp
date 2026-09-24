// AI - audio interface
#include "pch.h"

// all AI timers update is based on TBR.

// AI is a very simple device. It polls the DSP with L/R samples and also gets samples from the DVD (AIS). Then it does FIR and SRC and outputs the sound to the outside.
// But of course from the outside (register interface) everything seems very confusing. Who knew that the AI DMA controller is actually in the DSP.

using namespace Debug;

namespace Flipper
{
	void AudioInterface::MixerSetDvdAudioSampleRate(AudioSampleRate rate)
	{
		HW->Mixer->SetSampleRate(AxChannel::DvdAudio, rate);

		if (rate == AudioSampleRate::Rate_48000)
		{
			DVD::DDU->SetDvdAudioSampleRate(DVD::DvdAudioSampleRate::Rate_48000);
		}
		else
		{
			DVD::DDU->SetDvdAudioSampleRate(DVD::DvdAudioSampleRate::Rate_32000);
		}

		if (ai.log)
		{
			Report(Channel::AIS, "DVD Audio sample rate: %i\n", rate == AudioSampleRate::Rate_32000 ? 32000 : 48000);
		}
	}

	// ---------------------------------------------------------------------------
	// streaming

	// streaming trigger and counter coincidence
	void AudioInterface::AISINT()
	{
		// only if AIINT is validated
		if ((ai.cr & AICR_AIINTVLD) == 0)
		{
			ai.cr |= AICR_AIINT;
			if (ai.cr & AICR_AIINTMSK)
			{
				HW->pi->PIAssertInt(PI_INTERRUPT_AI);
				if (ai.log)
				{
					Report(Channel::AIS, "AISINT\n");
				}
			}
		}
	}

	// AI control register
	void AudioInterface::AIControl()
	{
		// clear stream interrupt
		if (ai.cr & AICR_AIINT)
		{
			ai.cr &= ~AICR_AIINT;
			HW->pi->PIClearInt(PI_INTERRUPT_AI);
		}

		// enable sample counter
		if (ai.cr & AICR_PSTAT)
		{
			if (ai.log)
			{
				Report(Channel::AIS, "start streaming clock\n");
			}
			DVD::DDU->EnableAudioStreamClock(true);
			HW->Mixer->Enable(AxChannel::DvdAudio, true);
			ai.streamFifoPtr = 0;
		}
		else
		{
			if (ai.log)
			{
				Report(Channel::AIS, "stop streaming clock\n");
			}
			DVD::DDU->EnableAudioStreamClock(false);
			HW->Mixer->Enable(AxChannel::DvdAudio, false);
		}

		// reset sample counter
		if (ai.cr & AICR_SCRESET)
		{
			if (ai.log)
			{
				Report(Channel::AIS, "reset sample counter\n");
			}
			ai.scnt = 0;
			ai.cr &= ~AICR_SCRESET;
		}

		// set DMA sample rate
		if (ai.cr & AICR_DFR)
		{
			DSP::DspSetAiDmaSampleRate(32000);
		}
		else
		{
			DSP::DspSetAiDmaSampleRate(48000);
		}

		// set DVD Audio sample rate
		if (ai.cr & AICR_AFR) MixerSetDvdAudioSampleRate(AudioSampleRate::Rate_48000);
		else MixerSetDvdAudioSampleRate(AudioSampleRate::Rate_32000);
	}

	void AudioInterface::AIReadReg(uint32_t addr, uint32_t* reg, void *ctx)
	{
		AudioInterface* ai = (AudioInterface*)ctx;

		switch (addr & 0xFF) {

			case AIS_CR+2:
				*reg = (uint16_t)ai->ai.cr;
				break;
			case AIS_VR+2:
				*reg = (uint16_t)ai->ai.vr;
				break;
			case AIS_SCNT:
				*reg = ai->ai.scnt >> 16;
				break;
			case AIS_SCNT+2:
				*reg = (uint16_t)ai->ai.scnt;
				break;
			case AIS_IT:
				*reg = ai->ai.it >> 16;
				break;
			case AIS_IT+2:
				*reg = (uint16_t)ai->ai.it;
				break;

			default:
				*reg = 0;
				break;
		}
	}

	void AudioInterface::AIWriteReg(uint32_t addr, uint32_t data, void *ctx)
	{
		AudioInterface* ai = (AudioInterface*)ctx;

		switch (addr & 0xFF) {

			case AIS_CR+2:
				ai->ai.cr = data & 0x7F;
				ai->AIControl();
				break;
			case AIS_VR+2:
				ai->ai.vr = (uint16_t)data;
				break;
			case AIS_IT:
				ai->ai.it &= 0x0000ffff;
				ai->ai.it |= data << 16;
				break;
			case AIS_IT+2:
				ai->ai.it &= 0xffff0000;
				ai->ai.it |= data;
				if (ai->ai.log) {
					Report(Channel::AIS, "set trigger to: 0x%08X\n", ai->ai.it);
				}
				break;

			default:
				break;
		}
	}

	// ---------------------------------------------------------------------------

	// AI DMA and DVD Audio are played uncompetitively from different streams.
	// All work on Sample Rate Conversion and sound mixing for convenience is done in Mixer (audiosdl.cpp).

	// The streaming (auxiliary) channel passes through the volume stage before it is added to the DSP
	// output: an 8-bit multiplier built by the SRC from AIVR scales the sample by volume/256, where
	// 0x00 mutes the stream and 0xFF is full scale (audio-interface.md sections 3.3 and 8.2). The
	// samples are signed, so the scaling is done on the signed value.

	uint16_t AudioInterface::AdjustVolume(uint16_t sampleValue, int volume)
	{
		int32_t scaled = ((int32_t)(int16_t)sampleValue * (volume & 0xFF)) >> 8;
		return (uint16_t)scaled;
	}

	// Called from DDU Core when DVD Audio decodes the next sample
	void AudioInterface::AIStreamCallback(uint16_t l, uint16_t r, void *ctx)
	{
		AudioInterface* ai = (AudioInterface*)ctx;

		// Check FIFO overflow
		if (ai->ai.streamFifoPtr >= sizeof(ai->ai.streamFifo))
		{
			ai->ai.streamFifoPtr = 0;
			// Feed mixer
			HW->Mixer->PushBytes(AxChannel::DvdAudio, ai->ai.streamFifo, sizeof(ai->ai.streamFifo));

			// The DVD audio stream is the mixer's second input (issue #394); the samples come from
			// the disc, not from main memory, so this is mixer traffic only.
			HwProfile::Count(HwProfile::Counter::AudioMixer, sizeof(ai->ai.streamFifo));
		}

		// Adjust volume and swap endianess
		int leftVolume = (uint8_t)ai->ai.vr;
		int rightVolume = (uint8_t)(ai->ai.vr >> 8);
		l = _BYTESWAP_UINT16(l);
		r = _BYTESWAP_UINT16(r);
		l = ai->AdjustVolume(l, leftVolume);
		r = ai->AdjustVolume(r, rightVolume);

		// Put sample in FIFO
		uint16_t* ptr = (uint16_t*)&ai->ai.streamFifo[ai->ai.streamFifoPtr];

		ptr[0] = l;
		ptr[1] = r;

		ai->ai.streamFifoPtr += 4;

		// update stream sample counter
		if (ai->ai.cr & AICR_PSTAT)
		{
			ai->ai.scnt++;
			if (ai->ai.scnt >= ai->ai.it)
			{
				ai->AISINT();
			}
		}
	}

	AudioInterface::AudioInterface(Flipper* flipper, HWConfig* config)
	{
		Report(Channel::AI, "Audio interface (DSP AI/DVD Audio mixer)\n");

		// clear regs
		memset(&ai, 0, sizeof(ai));

		DVD::DDU->SetStreamCallback(AIStreamCallback, this);

		ai.log = config->ai_log;

		for (uint32_t i = 0; i < AIS_REG_MAX; i+=2) {
			flipper->pi->PISetTrap(PI_REGSPACE_AI + i, AIReadReg, AIWriteReg, this);
		}
	}

	AudioInterface::~AudioInterface()
	{
		DVD::DDU->SetStreamCallback(nullptr, nullptr);
	}

	// ---------------------------------------------------------------------------
	// save states

	void AudioInterface::SaveState(SaveStates::StateWriter& writer) const
	{
		writer.Fields(ai.cr, ai.vr, ai.scnt, ai.it);

		// The streaming FIFO is a half-filled buffer rather than a register: the drive's decoder
		// appends one sample pair to it per decoded sample (AIStreamCallback) and the mixer takes
		// the whole 32 bytes at once when it is full. Nothing about that is visible in the four
		// registers, so a state that left it out would drop (or repeat) the fraction of a buffer
		// the drive had already decoded, and the first sound after a load would be wrong.
		writer.Array(ai.streamFifo);
		writer.U64((uint64_t)ai.streamFifoPtr);
	}

	void AudioInterface::LoadState(SaveStates::StateReader& reader)
	{
		reader.Fields(ai.cr, ai.vr, ai.scnt, ai.it);

		reader.Array(ai.streamFifo);
		ai.streamFifoPtr = (size_t)reader.U64();

		if (reader.Failed())
		{
			return;
		}

		// The write pointer only ever moves in whole sample pairs, four bytes at a time, and it
		// is reset to zero when the mixer takes the buffer - so a position that is not inside the
		// buffer or that splits a pair is not one this emulator could have written. (A write
		// pointer left *equal* to the size is the moment the buffer is full, which is a place the
		// callback really does leave it in for an instant, so it is accepted.)
		if (ai.streamFifoPtr > sizeof(ai.streamFifo) || (ai.streamFifoPtr & 3) != 0)
		{
			reader.Fail("the audio streaming FIFO of the save state is not where its writer could have left it");
			return;
		}

		// What the control register implies for the hardware *around* this block is not a register
		// of it and is not in the state: PSTAT starts the drive's audio streaming clock and the
		// mixer's DVD audio channel (and stopping the channel with the stream still running is
		// worse than not having a state at all - the machine would stay silent), AIINT drops the
		// streaming interrupt the register write clears, and DFR and AFR re-select the sample rate
		// of the DSP's AI DMA and of the DVD audio mixer input. AIControl() is exactly that
		// refresh, so it is called here rather than left to the caller - nothing outside this
		// method has to know that a load needs it.
		//
		// The catch is that AIControl() is written for a *register write*, where the guest has
		// just programmed the streaming state and starting it from the beginning is right: with
		// PSTAT set it resets the write pointer to the start of the FIFO, and with SCRESET set it
		// zeroes the sample counter. Neither of those is what a load wants - the state has just
		// brought back both, and the drive will keep feeding the FIFO from where it was rather
		// than from the start (nothing restarts the streaming burst), so resetting the pointer
		// would overwrite the first sample pair of the buffer and repeat the samples at the end of
		// the stream. The three values are therefore taken out of the way, the refresh is run, and
		// they are put back, with only the spent SCRESET bit left cleared the way the register
		// write leaves it.
		uint8_t streamFifo[sizeof(ai.streamFifo)];
		size_t streamFifoPtr = ai.streamFifoPtr;
		uint32_t scnt = ai.scnt;

		memcpy(streamFifo, ai.streamFifo, sizeof(streamFifo));

		AIControl();

		memcpy(ai.streamFifo, streamFifo, sizeof(streamFifo));
		ai.streamFifoPtr = streamFifoPtr;
		ai.scnt = scnt;
		ai.cr &= ~AICR_SCRESET;
	}
}