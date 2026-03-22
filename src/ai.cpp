// AI - audio interface
#include "pch.h"

// all AI timers update is based on TBR.

/*/
	AI Interrupts
	-------------

	The Audio Interface API is responsible for managing two interrupts to the
	host CPU:
		The streamed sample counter interrupt (AIINT).
		The AI DMA interrupt (AIDINT).
	Note that the streamed sample counter interrupt is generated directly by
	the AI hardware. The AI DMA interrupt, however, comes from the memory
	controller of the audio subsystem.

	Audio streaming sample counter interrupt (AIINT)
	The Audio Interface provides facilities for counting the number of streamed
	(left/right) samples played and asserting an interrupt at some programmable
	trigger value. Note that only audio samples streamed from the optical disc
	are counted. Note also that samples are counted after the sample rate
	conversion stage-thus, the stream will always be at a 48KHz sample rate.

	AI DMA interrupt (AIDINT)
	The Audio Interface API provides control over the AI DMA. The actual DMA
	controller resides within the audio subsystem of the Graphics Processor ASIC.
	The AI DMA feeds data from main memory to the AI FIFO, which is 32 bytes in
	length (the size of a single DMA block). The AI FIFO consumes data at a rate
	of either 48,000 or 32,000 stereo samples per second. The sample rate of the
	AI FIFO DMA may be controlled through AISetDSPSampleRate().
/*/

// AI is a very simple device. It polls the DSP with L/R samples and also gets samples from the DVD (AIS). Then it does FIR and SRC and outputs the sound to the outside.
// But of course from the outside (register interface) everything seems very confusing. Who knew that the AI DMA controller is actually in the DSP.

using namespace Debug;

namespace Flipper
{

	// AI state (registers and other data)
	AIState ai;

	static void MixerSetDvdAudioSampleRate(AudioSampleRate rate)
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
	void AISINT()
	{
		// only if AIINT is validated
		if ((ai.cr & AICR_AIINTVLD) == 0)
		{
			ai.cr |= AICR_AIINT;
			if (ai.cr & AICR_AIINTMSK)
			{
				PIAssertInt(PI_INTERRUPT_AI);
				if (ai.log)
				{
					Report(Channel::AIS, "AISINT\n");
				}
			}
		}
	}

	// AI control register
	static void AIControl()
	{
		// clear stream interrupt
		if (ai.cr & AICR_AIINT)
		{
			ai.cr &= ~AICR_AIINT;
			PIClearInt(PI_INTERRUPT_AI);
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

	static void AIReadReg(uint32_t addr, uint32_t* reg, void *ctx)
	{
		switch (addr & 0xFF) {

			case AIS_CR+2:
				*reg = (uint16_t)ai.cr;
				break;
			case AIS_VR+2:
				*reg = (uint16_t)ai.vr;
				break;
			case AIS_SCNT:
				*reg = ai.scnt >> 16;
				break;
			case AIS_SCNT+2:
				*reg = (uint16_t)ai.scnt;
				break;
			case AIS_IT:
				*reg = ai.it >> 16;
				break;
			case AIS_IT+2:
				*reg = (uint16_t)ai.it;
				break;

			default:
				*reg = 0;
				break;
		}
	}

	static void AIWriteReg(uint32_t addr, uint32_t data, void *ctx)
	{
		switch (addr & 0xFF) {

			case AIS_CR+2:
				ai.cr = data & 0x7F;
				AIControl();
				break;
			case AIS_VR+2:
				ai.vr = (uint16_t)data;
				break;
			case AIS_IT:
				ai.it &= 0x0000ffff;
				ai.it |= data << 16;
				break;
			case AIS_IT+2:
				ai.it &= 0xffff0000;
				ai.it |= data;
				if (ai.log) {
					Report(Channel::AIS, "set trigger to: 0x%08X\n", ai.it);
				}
				break;

			default:
				break;
		}
	}

	// ---------------------------------------------------------------------------

	// AI DMA and DVD Audio are played uncompetitively from different streams.
	// All work on Sample Rate Conversion and sound mixing for convenience is done in Mixer (audio.cpp).

	static uint16_t AdjustVolume(uint16_t sampleValue, int volume)
	{
		// Let's try how this conversion will behave on a float, if it slows down, then translate it to ints.
		// In theory, on modern processors, float is fast.
		float value = (float)sampleValue / (float)0xFFFF;
		float volumeF = (float)volume / (float)0xFF;
		float adjusted = value * volumeF;
		return (uint16_t)(adjusted * (float)0xFFFF);
	}

	// Called from DDU Core when DVD Audio decodes the next sample
	static void AIStreamCallback(uint16_t l, uint16_t r)
	{
		// Check FIFO overflow
		if (ai.streamFifoPtr >= sizeof(ai.streamFifo))
		{
			ai.streamFifoPtr = 0;
			// Feed mixer
			HW->Mixer->PushBytes(AxChannel::DvdAudio, ai.streamFifo, sizeof(ai.streamFifo));
		}

		// Adjust volume and swap endianess
		int leftVolume = (uint8_t)ai.vr;
		int rightVolume = (uint8_t)(ai.vr >> 8);
		l = _BYTESWAP_UINT16(l);
		r = _BYTESWAP_UINT16(r);
		//l = AdjustVolume(l, leftVolume);
		//r = AdjustVolume(r, rightVolume);

		// Put sample in FIFO
		uint16_t* ptr = (uint16_t*)&ai.streamFifo[ai.streamFifoPtr];

		ptr[0] = l;
		ptr[1] = r;

		ai.streamFifoPtr += 4;

		// update stream sample counter
		if (ai.cr & AICR_PSTAT)
		{
			ai.scnt++;
			if (ai.scnt >= ai.it)
			{
				AISINT();
			}
		}
	}

	void AIOpen(HWConfig* config)
	{
		Report(Channel::AI, "Audio interface (DSP AI/DVD Audio mixer)\n");

		// clear regs
		memset(&ai, 0, sizeof(ai));

		DVD::DDU->SetStreamCallback(AIStreamCallback);

		ai.log = config->ai_log;

		for (uint32_t i = 0; i < AIS_REG_MAX; i+=2) {
			PISetTrap(PI_REGSPACE_AI + i, AIReadReg, AIWriteReg);
		}
	}

	void AIClose()
	{
		DVD::DDU->SetStreamCallback(nullptr);
	}
}