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

	// ---------------------------------------------------------------------------
	// AIDCR

	static void write_aidcr(uint32_t addr, uint32_t data)
	{
		if (ai.log)
		{
			Report(Channel::AI, "AIDCR: 0x%04X (RESETMOD:%i, DSPINTMSK:%i, DSPINT:%i, ARINTMSK:%i, ARINT:%i, AIINTMSK:%i, AIINT:%i, HALT:%i, DINT:%i, RES:%i\n",
				data,
				data & AIDCR_RESETMOD ? 1 : 0,
				data & AIDCR_DSPINTMSK ? 1 : 0,
				data & AIDCR_DSPINT ? 1 : 0,
				data & AIDCR_ARINTMSK ? 1 : 0,
				data & AIDCR_ARINT ? 1 : 0,
				data & AIDCR_AIINTMSK ? 1 : 0,
				data & AIDCR_AIINT ? 1 : 0,
				data & AIDCR_HALT ? 1 : 0,
				data & AIDCR_DINT ? 1 : 0,
				data & AIDCR_RES ? 1 : 0);
		}

		// set mask
		if (data & AIDCR_DSPINTMSK)
		{
			AIDCR |= AIDCR_DSPINTMSK;
		}
		else
		{
			AIDCR &= ~AIDCR_DSPINTMSK;
		}
		if (data & AIDCR_ARINTMSK)
		{
			AIDCR |= AIDCR_ARINTMSK;
		}
		else
		{
			AIDCR &= ~AIDCR_ARINTMSK;
		}
		if (data & AIDCR_AIINTMSK)
		{
			AIDCR |= AIDCR_AIINTMSK;
		}
		else
		{
			AIDCR &= ~AIDCR_AIINTMSK;
		}

		// clear pending interrupts
		if (data & AIDCR_DSPINT)
		{
			AIDCR &= ~AIDCR_DSPINT;
		}
		if (data & AIDCR_ARINT)
		{
			AIDCR &= ~AIDCR_ARINT;
		}
		if (data & AIDCR_AIINT)
		{
			AIDCR &= ~AIDCR_AIINT;
		}

		if ((AIDCR & AIDCR_DSPINT) == 0 && (AIDCR & AIDCR_ARINT) == 0 && (AIDCR & AIDCR_AIINT) == 0)
		{
			PIClearInt(PI_INTERRUPT_DSP);
		}

		// DSP DMA always ready
		AIDCR &= ~AIDCR_DSPDMA;

		// Reset modifier bit
		if (data & AIDCR_RESETMOD)
		{
			AIDCR |= AIDCR_RESETMOD;
		}
		else
		{
			AIDCR &= ~AIDCR_RESETMOD;
		}

		// DSP controls
		DSP->SetResetBit((data >> 0) & 1);
		DSP->SetIntBit((data >> 1) & 1);
		DSP->SetHaltBit((data >> 2) & 1);
	}

	static void read_aidcr(uint32_t addr, uint32_t* reg)
	{
		// DSP controls
		AIDCR &= ~7;
		AIDCR |= DSP->GetResetBit() << 0;
		AIDCR |= DSP->GetIntBit() << 1;
		AIDCR |= DSP->GetHaltBit() << 2;

		*reg = AIDCR;
	}

	// ---------------------------------------------------------------------------
	// DMA

	// dma transfer complete (when AIDCNT == 0)
	void AIDINT()
	{
		AIDCR |= AIDCR_AIINT;
		if (AIDCR & AIDCR_AIINTMSK)
		{
			PIAssertInt(PI_INTERRUPT_DSP);
			if (ai.log)
			{
				Report(Channel::AI, "AIDINT");
			}
		}
	}

	// how much time AI DMA need to playback "n" bytes in Gekko ticks
	static int64_t AIGetTime(size_t dmaBytes, long rate)
	{
		size_t samples = dmaBytes / 4;    // left+right, 16-bit
		return samples * (ai.one_second / rate);
	}

	static void AIStartDMA()
	{
		ai.dcnt = ai.len & ~AID_EN;
		ai.dmaTime = Core->GetTicks() + AIGetTime(32, ai.dmaRate);
		ai.currentDmaAddr = (ai.madr_hi << 16) | ai.madr_lo;
		if (ai.log)
		{
			Report(Channel::AI, "DMA started: %08X, %i bytes\n", ai.currentDmaAddr, ai.dcnt * 32);
		}
		ai.audioThread->Resume();
	}

	// Simulate AI FIFO
	static void AIFeedMixer()
	{
		int bytes = 32;

		if (ai.dcnt == 0 || (ai.len & AID_EN) == 0)
		{
			ai.Mixer->PushBytes(AxChannel::AudioDma, ai.zeroes, bytes);
		}
		else
		{
			ai.Mixer->PushBytes(AxChannel::AudioDma, &mi.ram[ai.currentDmaAddr & RAMMASK], bytes);
			ai.currentDmaAddr += bytes;
			ai.dcnt--;
		}

		ai.dmaTime = Core->GetTicks() + AIGetTime(bytes, ai.dmaRate);
	}

	static void AIStopDMA()
	{
		ai.dmaTime = -1;
		ai.dcnt = 0;
		if (ai.log)
		{
			Report(Channel::AI, "DMA stopped\n");
		}
	}

	static void AISetDMASampleRate(AudioSampleRate rate)
	{
		ai.Mixer->SetSampleRate(AxChannel::AudioDma, rate);
		if (ai.log)
		{
			Report(Channel::AI, "DMA sample rate: %i\n", rate == AudioSampleRate::Rate_32000 ? 32000 : 48000);
		}
	}

	static void AISetDvdAudioSampleRate(AudioSampleRate rate)
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

	//
	// dma buffer address
	// 

	static void write_dmah(uint32_t addr, uint32_t data)
	{
		ai.madr_hi = (uint16_t)data & 0x3FF;
	}

	static void write_dmal(uint32_t addr, uint32_t data)
	{
		ai.madr_lo = (uint16_t)data & ~0x1F;
	}

	static void read_dmah(uint32_t addr, uint32_t* reg) { *reg = ai.madr_hi & 0x3FF; }
	static void read_dmal(uint32_t addr, uint32_t* reg) { *reg = ai.madr_lo & ~0x1F; }

	//
	// dma length / control
	//

	static void write_len(uint32_t addr, uint32_t data)
	{
		ai.len = (uint16_t)data;

		// start/stop audio dma transfer
		if (ai.len & AID_EN)
		{
			AIStartDMA();
			if (!ai.Mixer->IsEnabled(AxChannel::AudioDma))
			{
				ai.Mixer->Enable(AxChannel::AudioDma, true);
			}
		}
		else
		{
			AIStopDMA();
			if (ai.Mixer->IsEnabled(AxChannel::AudioDma))
			{
				ai.Mixer->Enable(AxChannel::AudioDma, false);
			}
		}
	}
	static void read_len(uint32_t addr, uint32_t* reg)
	{
		*reg = ai.len;
	}

	//
	// read sample block (32b) counter
	//

	static void read_dcnt(uint32_t addr, uint32_t* reg)
	{
		*reg = ai.dcnt & 0x7FFF;
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
			ai.dmaRate = 32000;
			AISetDMASampleRate(AudioSampleRate::Rate_32000);
		}
		else
		{
			ai.dmaRate = 48000;
			AISetDMASampleRate(AudioSampleRate::Rate_48000);
		}

		// set DVD Audio sample rate
		if (ai.cr & AICR_AFR) AISetDvdAudioSampleRate(AudioSampleRate::Rate_48000);
		else AISetDvdAudioSampleRate(AudioSampleRate::Rate_32000);
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
	// DSPCore interface (mailbox and interrupt)

	static void write_out_mbox_h(uint32_t addr, uint32_t data) { DSP->CpuToDspWriteHi((uint16_t)data); }
	static void write_out_mbox_l(uint32_t addr, uint32_t data) { DSP->CpuToDspWriteLo((uint16_t)data); }
	static void read_out_mbox_h(uint32_t addr, uint32_t* reg) { *reg = DSP->CpuToDspReadHi(false); }
	static void read_out_mbox_l(uint32_t addr, uint32_t* reg) { *reg = DSP->CpuToDspReadLo(false); }

	static void read_in_mbox_h(uint32_t addr, uint32_t* reg) { *reg = DSP->DspToCpuReadHi(false); }
	static void read_in_mbox_l(uint32_t addr, uint32_t* reg) { *reg = DSP->DspToCpuReadLo(false); }

	static void write_in_mbox_h(uint32_t addr, uint32_t data) { Halt("Processor is not allowed to write DSP Mailbox!"); }
	static void write_in_mbox_l(uint32_t addr, uint32_t data) { Halt("Processor is not allowed to write DSP Mailbox!"); }

	void DSPAssertInt()
	{
		if (ai.log)
		{
			Report(Channel::AI, "DSPAssertInt\n");
		}

		AIDCR |= AIDCR_DSPINT;
		if (AIDCR & AIDCR_DSPINTMSK)
		{
			PIAssertInt(PI_INTERRUPT_DSP);
		}
	}

	bool DSPGetInterruptStatus()
	{
		return (AIDCR & AIDCR_DSPINT) != 0;
	}

	bool DSPGetResetModifier()
	{
		return (AIDCR & AIDCR_RESETMOD) != 0;
	}

	// ---------------------------------------------------------------------------

	// AI DMA and DVD Audio are played uncompetitively from different streams.
	// All work on Sample Rate Conversion and sound mixing for convenience is done in Mixer (AX.cpp).

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

	// Update audio DMA thread
	static void AIUpdate(void* Parameter)
	{
		if ((uint64_t)Core->GetTicks() >= ai.dmaTime)
		{
			if (ai.dcnt == 0)
			{
				if (ai.len & AID_EN)
				{
					// Restart Dma and signal AID_INT
					ai.currentDmaAddr = (ai.madr_hi << 16) | ai.madr_lo;
					ai.dcnt = ai.len & ~AID_EN;
					AIDINT();
				}
				else
				{
					ai.audioThread->Suspend();
				}
			}
			else
			{
				if (ai.len & AID_EN)
				{
					AIFeedMixer();
				}
				else
				{
					ai.audioThread->Suspend();
				}
			}
		}
	}

	void AIOpen(HWConfig* config)
	{
		Report(Channel::AI, "Audio interface (DMA, DVD Streaming and DSP)\n");

		// clear regs
		memset(&ai, 0, sizeof(AIControl));

		DVD::DDU->SetStreamCallback(AIStreamCallback);

		ai.audioThread = EMUCreateThread(AIUpdate, true, nullptr, "AI");

		ai.one_second = Core->OneSecond();
		ai.dmaRate = ai.cr & AICR_DFR ? 32000 : 48000;
		ai.dmaTime = Core->GetTicks() + AIGetTime(32, ai.dmaRate);
		ai.log = false;
		AIStopDMA();

		// set register traps
		PISetTrap(16, AI_DCR, read_aidcr, write_aidcr);

		PISetTrap(16, DSP_OUTMBOXH, read_out_mbox_h, write_out_mbox_h);
		PISetTrap(16, DSP_OUTMBOXL, read_out_mbox_l, write_out_mbox_l);
		PISetTrap(16, DSP_INMBOXH, read_in_mbox_h, write_in_mbox_h);
		PISetTrap(16, DSP_INMBOXL, read_in_mbox_l, write_in_mbox_l);

		PISetTrap(16, AID_MADRH, read_dmah, write_dmah);
		PISetTrap(16, AID_MADRL, read_dmal, write_dmal);
		PISetTrap(16, AID_LEN, read_len, write_len);
		PISetTrap(16, AID_CNT, read_dcnt, nullptr);

		PISetTrap(32, AIS_CR, read_cr, write_cr);
		PISetTrap(32, AIS_VR, read_vr, write_vr);
		PISetTrap(32, AIS_SCNT, read_scnt, nullptr);
		PISetTrap(32, AIS_IT, read_it, write_it);

		ai.Mixer = new AudioMixer(config);
	}

	void AIClose()
	{
		AIStopDMA();
		EMUJoinThread(ai.audioThread);
		DVD::DDU->SetStreamCallback(nullptr);
		delete ai.Mixer;
	}

}
