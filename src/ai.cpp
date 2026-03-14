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
	AIControl ai;

	static void MixerSetDMASampleRate(AudioSampleRate rate)
	{
		ai.Mixer->SetSampleRate(AxChannel::AudioDma, rate);
		if (ai.log)
		{
			Report(Channel::AI, "DMA sample rate: %i\n", rate == AudioSampleRate::Rate_32000 ? 32000 : 48000);
		}
	}

	static void MixerSetDvdAudioSampleRate(AudioSampleRate rate)
	{
		ai.Mixer->SetSampleRate(AxChannel::DvdAudio, rate);

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
	static void write_cr(uint32_t addr, uint32_t data)
	{
		ai.cr = data & 0x7F;

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
			ai.Mixer->Enable(AxChannel::DvdAudio, true);
			ai.streamFifoPtr = 0;
		}
		else
		{
			if (ai.log)
			{
				Report(Channel::AIS, "stop streaming clock\n");
			}
			DVD::DDU->EnableAudioStreamClock(false);
			ai.Mixer->Enable(AxChannel::DvdAudio, false);
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
			MixerSetDMASampleRate(AudioSampleRate::Rate_32000);
		}
		else
		{
			DSP::DspSetAiDmaSampleRate(48000);
			MixerSetDMASampleRate(AudioSampleRate::Rate_48000);
		}

		// set DVD Audio sample rate
		if (ai.cr & AICR_AFR) MixerSetDvdAudioSampleRate(AudioSampleRate::Rate_48000);
		else MixerSetDvdAudioSampleRate(AudioSampleRate::Rate_32000);
	}
	static void read_cr(uint32_t addr, uint32_t* reg)
	{
		*reg = ai.cr;
	}

	// stream samples counter
	static void read_scnt(uint32_t addr, uint32_t* reg)
	{
		*reg = ai.scnt;
	}

	// interrupt trigger
	static void write_it(uint32_t addr, uint32_t data)
	{
		if (ai.log)
		{
			Report(Channel::AIS, "set trigger to : 0x%08X\n", data);
		}
		ai.it = data;
	}
	static void read_it(uint32_t addr, uint32_t* reg) { *reg = ai.it; }

	// stream volume register
	static void write_vr(uint32_t addr, uint32_t data)
	{
		ai.vr = (uint16_t)data;
	}
	static void read_vr(uint32_t addr, uint32_t* reg)
	{
		*reg = ai.vr;
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
			ai.Mixer->PushBytes(AxChannel::DvdAudio, ai.streamFifo, sizeof(ai.streamFifo));
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
		memset(&ai, 0, sizeof(AIControl));

		DVD::DDU->SetStreamCallback(AIStreamCallback);

		ai.log = config->ai_log;

		PISetTrap(32, AIS_CR, read_cr, write_cr);
		PISetTrap(32, AIS_VR, read_vr, write_vr);
		PISetTrap(32, AIS_SCNT, read_scnt, nullptr);
		PISetTrap(32, AIS_IT, read_it, write_it);

		ai.Mixer = new AudioMixer(config);
	}

	void AIClose()
	{
		DVD::DDU->SetStreamCallback(nullptr);
		delete ai.Mixer;
	}
}