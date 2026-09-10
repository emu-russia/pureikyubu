#include "pch.h"

// TODO: For now, it's a bit ugly, but the module has finally found its place in the GameCube architecture.

using namespace Debug;

namespace DSP
{
	DspAIControl dsp_ai;

	// ---------------------------------------------------------------------------
	// CDCR

	static void write_cdcr(uint32_t addr, uint32_t data, void* ctx)
	{
		if (dsp_ai.log)
		{
			Report(Channel::AI, "CDCR: 0x%04X (RESETMOD:%i, DSPINTMSK:%i, DSPINT:%i, ARINTMSK:%i, ARINT:%i, AIINTMSK:%i, AIINT:%i, HALT:%i, DINT:%i, RES:%i\n",
				data,
				data & CDCR_RESETMOD ? 1 : 0,
				data & CDCR_DSPINTMSK ? 1 : 0,
				data & CDCR_DSPINT ? 1 : 0,
				data & CDCR_ARINTMSK ? 1 : 0,
				data & CDCR_ARINT ? 1 : 0,
				data & CDCR_AIINTMSK ? 1 : 0,
				data & CDCR_AIINT ? 1 : 0,
				data & CDCR_HALT ? 1 : 0,
				data & CDCR_DINT ? 1 : 0,
				data & CDCR_RES ? 1 : 0);
		}

		// set mask
		if (data & CDCR_DSPINTMSK)
		{
			CDCR |= CDCR_DSPINTMSK;
		}
		else
		{
			CDCR &= ~CDCR_DSPINTMSK;
		}
		if (data & CDCR_ARINTMSK)
		{
			CDCR |= CDCR_ARINTMSK;
		}
		else
		{
			CDCR &= ~CDCR_ARINTMSK;
		}
		if (data & CDCR_AIINTMSK)
		{
			CDCR |= CDCR_AIINTMSK;
		}
		else
		{
			CDCR &= ~CDCR_AIINTMSK;
		}

		// clear pending interrupts
		if (data & CDCR_DSPINT)
		{
			CDCR &= ~CDCR_DSPINT;
		}
		if (data & CDCR_ARINT)
		{
			CDCR &= ~CDCR_ARINT;
		}
		if (data & CDCR_AIINT)
		{
			CDCR &= ~CDCR_AIINT;
		}

		if ((CDCR & CDCR_DSPINT) == 0 && (CDCR & CDCR_ARINT) == 0 && (CDCR & CDCR_AIINT) == 0)
		{
			Flipper::HW->pi->PIClearInt(PI_INTERRUPT_DSP);
		}

		// DSP DMA always ready
		CDCR &= ~CDCR_DSPDMA;

		// Reset modifier bit
		if (data & CDCR_RESETMOD)
		{
			CDCR |= CDCR_RESETMOD;
		}
		else
		{
			CDCR &= ~CDCR_RESETMOD;
		}

		// DSP controls
		Flipper::DSP->SetResetBit((data >> 0) & 1);
		Flipper::DSP->SetIntBit((data >> 1) & 1);
		Flipper::DSP->SetHaltBit((data >> 2) & 1);
	}

	static void read_cdcr(uint32_t addr, uint32_t* reg, void* ctx)
	{
		// DSP controls
		CDCR &= ~7;
		CDCR |= Flipper::DSP->GetResetBit() << 0;
		CDCR |= Flipper::DSP->GetIntBit() << 1;
		CDCR |= Flipper::DSP->GetHaltBit() << 2;

		*reg = CDCR;
	}

	// ---------------------------------------------------------------------------
	// DMA

	// dma transfer complete (when AIDCNT == 0)
	void AIDINT()
	{
		CDCR |= CDCR_AIINT;
		if (CDCR & CDCR_AIINTMSK)
		{
			Flipper::HW->pi->PIAssertInt(PI_INTERRUPT_DSP);
			if (dsp_ai.log)
			{
				Report(Channel::AI, "AIDINT\n");
			}
		}
	}

	// how much time AI DMA need to playback "n" bytes in Gekko ticks
	static int64_t AIGetTime(size_t dmaBytes, long rate)
	{
		size_t samples = dmaBytes / 4;    // left+right, 16-bit
		return samples * (dsp_ai.one_second / rate);
	}

	static void AIStartDMA()
	{
		dsp_ai.dcnt = dsp_ai.len & ~AID_EN;
		dsp_ai.dmaTime = Core->GetTicks() + AIGetTime(32, dsp_ai.dmaRate);
		dsp_ai.currentDmaAddr = (dsp_ai.madr_hi << 16) | dsp_ai.madr_lo;
		if (dsp_ai.log)
		{
			Report(Channel::AI, "DMA started: %08X, %i bytes\n", dsp_ai.currentDmaAddr, dsp_ai.dcnt * 32);
		}
		dsp_ai.audioThread->Resume();
	}

	// Simulate AI FIFO
	static void AIFeedMixer()
	{
		int bytes = 32;

		if (dsp_ai.dcnt == 0 || (dsp_ai.len & AID_EN) == 0)
		{
			Flipper::HW->Mixer->PushBytes(Flipper::AxChannel::AudioDma, dsp_ai.zeroes, bytes);
		}
		else
		{
			Flipper::HW->Mixer->PushBytes(Flipper::AxChannel::AudioDma, (uint8_t *)Flipper::HW->mem->MIGetMemoryPointerForDSP(dsp_ai.currentDmaAddr), bytes);
			dsp_ai.currentDmaAddr += bytes;
			dsp_ai.dcnt--;
		}

		dsp_ai.dmaTime = Core->GetTicks() + AIGetTime(bytes, dsp_ai.dmaRate);
	}

	static void AIStopDMA()
	{
		dsp_ai.dmaTime = -1;
		dsp_ai.dcnt = 0;
		if (dsp_ai.log)
		{
			Report(Channel::AI, "DMA stopped\n");
		}
	}

	//
	// dma buffer address
	// 

	static void write_dmah(uint32_t addr, uint32_t data, void* ctx)
	{
		dsp_ai.madr_hi = (uint16_t)data & 0x3FF;
	}

	static void write_dmal(uint32_t addr, uint32_t data, void* ctx)
	{
		dsp_ai.madr_lo = (uint16_t)data & ~0x1F;
	}

	static void read_dmah(uint32_t addr, uint32_t* reg, void* ctx) { *reg = dsp_ai.madr_hi & 0x3FF; }
	static void read_dmal(uint32_t addr, uint32_t* reg, void* ctx) { *reg = dsp_ai.madr_lo & ~0x1F; }

	//
	// dma length / control
	//

	static void write_len(uint32_t addr, uint32_t data, void* ctx)
	{
		dsp_ai.len = (uint16_t)data;

		// start/stop audio dma transfer
		if (dsp_ai.len & AID_EN)
		{
			AIStartDMA();
			if (!Flipper::HW->Mixer->IsEnabled(Flipper::AxChannel::AudioDma))
			{
				Flipper::HW->Mixer->Enable(Flipper::AxChannel::AudioDma, true);
			}
		}
		else
		{
			AIStopDMA();
			if (Flipper::HW->Mixer->IsEnabled(Flipper::AxChannel::AudioDma))
			{
				Flipper::HW->Mixer->Enable(Flipper::AxChannel::AudioDma, false);
			}
		}
	}
	static void read_len(uint32_t addr, uint32_t* reg, void* ctx)
	{
		*reg = dsp_ai.len;
	}

	//
	// read sample block (32b) counter
	//

	static void read_dcnt(uint32_t addr, uint32_t* reg, void* ctx)
	{
		*reg = dsp_ai.dcnt & 0x7FFF;
	}

	// ---------------------------------------------------------------------------
	// DSPCore interface (mailbox and interrupt)

	static void write_out_mbox_h(uint32_t addr, uint32_t data, void *ctx) { Flipper::DSP->CpuToDspWriteHi((uint16_t)data); }
	static void write_out_mbox_l(uint32_t addr, uint32_t data, void* ctx) { Flipper::DSP->CpuToDspWriteLo((uint16_t)data); }
	static void read_out_mbox_h(uint32_t addr, uint32_t* reg, void* ctx) { *reg = Flipper::DSP->CpuToDspReadHi(false); }
	static void read_out_mbox_l(uint32_t addr, uint32_t* reg, void* ctx) { *reg = Flipper::DSP->CpuToDspReadLo(false); }

	static void read_in_mbox_h(uint32_t addr, uint32_t* reg, void* ctx) { *reg = Flipper::DSP->DspToCpuReadHi(false); }
	static void read_in_mbox_l(uint32_t addr, uint32_t* reg, void* ctx) { *reg = Flipper::DSP->DspToCpuReadLo(false); }

	static void write_in_mbox_h(uint32_t addr, uint32_t data, void* ctx) { Halt("Processor is not allowed to write DSP Mailbox!\n"); }
	static void write_in_mbox_l(uint32_t addr, uint32_t data, void* ctx) { Halt("Processor is not allowed to write DSP Mailbox!\n"); }

	void DSPAssertInt()
	{
		if (dsp_ai.log)
		{
			Report(Channel::AI, "DSPAssertInt\n");
		}

		CDCR |= CDCR_DSPINT;
		if (CDCR & CDCR_DSPINTMSK)
		{
			Flipper::HW->pi->PIAssertInt(PI_INTERRUPT_DSP);
		}
	}

	bool DSPGetInterruptStatus()
	{
		return (CDCR & CDCR_DSPINT) != 0;
	}

	bool DSPGetResetModifier()
	{
		return (CDCR & CDCR_RESETMOD) != 0;
	}

	// Update audio DMA thread
	static void AIUpdate(void* Parameter)
	{
		if ((uint64_t)Core->GetTicks() >= dsp_ai.dmaTime)
		{
			if (dsp_ai.dcnt == 0)
			{
				if (dsp_ai.len & AID_EN)
				{
					// Restart Dma and signal AID_INT
					dsp_ai.currentDmaAddr = (dsp_ai.madr_hi << 16) | dsp_ai.madr_lo;
					dsp_ai.dcnt = dsp_ai.len & ~AID_EN;
					AIDINT();
				}
				else
				{
					dsp_ai.audioThread->Suspend();
				}
			}
			else
			{
				if (dsp_ai.len & AID_EN)
				{
					AIFeedMixer();
				}
				else
				{
					dsp_ai.audioThread->Suspend();
				}
			}
		}
	}

	void DspAIOpen(Flipper::Flipper* flipper, HWConfig* config)
	{
		Report(Channel::AI, "DSP AI DMA\n");

		// clear regs
		memset(&dsp_ai, 0, sizeof(DspAIControl));

		dsp_ai.audioThread = EMUCreateThread(AIUpdate, true, nullptr, "AI");

		dsp_ai.one_second = Core->OneSecond();
		dsp_ai.dmaRate = 48000;			// The initial value of the bit 6 in AI CR is zero, which corresponds to 48 kHz
		dsp_ai.dmaTime = Core->GetTicks() + AIGetTime(32, dsp_ai.dmaRate);
		dsp_ai.log = false;
		AIStopDMA();

		// set register traps
		flipper->pi->PISetTrap(PI_REGSPACE_DSP | CDCR_OFF, read_cdcr, write_cdcr);

		flipper->pi->PISetTrap(PI_REGSPACE_DSP | DSP_OUTMBOXH, read_out_mbox_h, write_out_mbox_h);
		flipper->pi->PISetTrap(PI_REGSPACE_DSP | DSP_OUTMBOXL, read_out_mbox_l, write_out_mbox_l);
		flipper->pi->PISetTrap(PI_REGSPACE_DSP | DSP_INMBOXH, read_in_mbox_h, write_in_mbox_h);
		flipper->pi->PISetTrap(PI_REGSPACE_DSP | DSP_INMBOXL, read_in_mbox_l, write_in_mbox_l);

		flipper->pi->PISetTrap(PI_REGSPACE_DSP | AID_MADRH, read_dmah, write_dmah);
		flipper->pi->PISetTrap(PI_REGSPACE_DSP | AID_MADRL, read_dmal, write_dmal);
		flipper->pi->PISetTrap(PI_REGSPACE_DSP | AID_LEN, read_len, write_len);
		flipper->pi->PISetTrap(PI_REGSPACE_DSP | AID_CNT, read_dcnt, nullptr);
	}

	void DspAIClose()
	{
		EMUJoinThread(dsp_ai.audioThread);
		AIStopDMA();
	}

	static void MixerSetDMASampleRate(Flipper::AudioSampleRate rate)
	{
		Flipper::HW->Mixer->SetSampleRate(Flipper::AxChannel::AudioDma, rate);
		if (dsp_ai.log)
		{
			Report(Channel::DSP, "AI DMA sample rate: %i\n", rate == Flipper::AudioSampleRate::Rate_32000 ? 32000 : 48000);
		}
	}

	void DspSetAiDmaSampleRate(int32_t rate)
	{
		dsp_ai.dmaRate = rate;
		MixerSetDMASampleRate(rate == 32000 ? Flipper::AudioSampleRate::Rate_32000 : Flipper::AudioSampleRate::Rate_48000);
	}
}