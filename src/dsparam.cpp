#include "pch.h"

// DSP ARAM Accelerator

// Accelerator addresses work in accordance with the selected mode (4-bit, 8-bit, 16-bit). 
// That is, for example, in 4-bit mode - the start, current and end addresses point to nibble in ARAM.

// On a real system, the next piece of data is cached in 3 16-bit registers ("output ports"). 
// Here we do not repeat this mechanism, but refer directly to ARAM.

// The accelerator can work both independently, simply driving data between ARAM and DSP, and in conjunction with an ADPCM decoder.
// In the first case, one register is used (ACDAT, read-write), in the second case, another (ACDAT2, read-only).

using namespace Debug;

namespace DSP
{
	uint16_t Dsp16::AccelFetch()
	{
		uint16_t val = 0;
		uint8_t tempByte = 0;

		switch (Accel.Fmt & 3)
		{
			case 0:

				// Refresh pred/scale
				if ((Accel.CurrAddress.addr & 0xF) == 0 && ((Accel.Fmt >> 2) & 3) == 0)
				{
					Accel.AdpcmPds = *(uint8_t*)(aram.mem + (Accel.CurrAddress.addr & 0x07ff'ffff) / 2);
					Accel.CurrAddress.addr += 2;
				}

				// TODO: Check currAddr == endAddr after Pred/Scale update.

				tempByte = *(uint8_t*)(aram.mem + (Accel.CurrAddress.addr & 0x07ff'ffff) / 2);
				if ((Accel.CurrAddress.addr & 1) == 0)
				{
					val = tempByte >> 4;		// High nibble
				}
				else
				{
					val = tempByte & 0xf;		// Low nibble
				}
				break;

			case 1:
				val = *(uint8_t*)(aram.mem + (Accel.CurrAddress.addr & 0x07ff'ffff));
				break;

			case 2:
				val = _BYTESWAP_UINT16(*(uint16_t*)(aram.mem + 2 * (uint64_t)(Accel.CurrAddress.addr & 0x07ff'ffff)));
				break;

			default:
				Halt("DSP: Invalid accelerator mode: 0x%04X\n", Accel.Fmt);
				break;
		}

		Accel.CurrAddress.addr++;

		if ((Accel.CurrAddress.addr & 0x07ff'ffff) >= (Accel.EndAddress.addr & 0x07FF'FFFF))
		{
			Accel.CurrAddress.addr = Accel.StartAddress.addr;
			if (logAccel)
			{
				Report(Channel::DSP, "Accelerator Overflow while read\n");
			}

			core->AssertInterrupt(((Accel.Fmt >> 3) & 3) == 0 ? DspInterrupt::Dcre : DspInterrupt::Acrs);
		}

		return val;
	}

	// Read data by accelerator and optionally decode (raw=false)
	uint16_t Dsp16::AccelReadData(bool raw)
	{
		uint16_t val = 0;

		// Check bit15 of ACCAH
		if ((Accel.CurrAddress.h & 0x8000) != 0)
		{
			// This is #UB
			Halt("DSP: Accelerator is not configured to read\n");
		}

		val = AccelFetch();

		// Issue ADPCM Decoder
		if (!raw)
		{
			val = DecodeAdpcm(val);
		}

		return val;
	}

	// Write RAW data to ARAM
	void Dsp16::AccelWriteData(uint16_t data)
	{
		// Check bit15 of ACCAH
		if ((Accel.CurrAddress.h & 0x8000) == 0)
		{
			// This is #UB
			Halt("DSP: Accelerator is not configured to write\n");
		}

		// Write mode is always 16-bit

		*(uint16_t*)(aram.mem + 2 * (uint64_t)(Accel.CurrAddress.addr & 0x07ff'ffff)) = _BYTESWAP_UINT16(data);
		Accel.CurrAddress.addr++;

		if ((Accel.CurrAddress.addr & 0x07ff'ffff) >= (Accel.EndAddress.addr & 0x07FF'FFFF))
		{
			Accel.CurrAddress.addr = Accel.StartAddress.addr;
			Accel.CurrAddress.h |= 0x8000;
			if (logAccel)
			{
				Report(Channel::DSP, "Accelerator Overflow while write\n");
			}

			core->AssertInterrupt(DspInterrupt::Acwe);
		}
	}

}

// DSP PCM/ADPCM Decoder

namespace DSP
{
	uint16_t Dsp16::DecodeAdpcm(uint16_t in)
	{
		int64_t yn = 0;
		int64_t out = 0;
		int outputMode = (Accel.Fmt >> 4) & 3;

		switch ((Accel.Fmt >> 2) & 3)
		{
			case 0:
			{
				int pred = (Accel.AdpcmPds >> 4) & 7;
				int scale = Accel.AdpcmPds & 0xf;
				int16_t gain = 1 << scale;

				if (scale > 0xc)
					scale = 0xc;

				int16_t xn = in << 11;
				if (xn & 0x4000)
					xn |= 0x8000;

				yn = (int64_t)(int32_t)(int16_t)Accel.AdpcmYn1 * (int64_t)(int32_t)(int16_t)Accel.AdpcmCoef[2 * pred]
					+ (int64_t)(int32_t)(int16_t)Accel.AdpcmYn2 * (int64_t)(int32_t)(int16_t)Accel.AdpcmCoef[2 * pred + 1] +
					(int64_t)(int32_t)xn * gain;

				Accel.AdpcmYn2 = Accel.AdpcmYn1;
				Accel.AdpcmYn1 = (uint16_t)(yn >> 11);
				break;
			}

			case 1:
				yn = (int64_t)(int32_t)((int16_t)(in << 8)) * Accel.AdpcmGan;
				break;

			case 2:
				yn = (int64_t)(int32_t)(int16_t)in * Accel.AdpcmGan;
				break;
		}

		switch (outputMode)
		{
			case 0:
				out = yn >> 11;
				out = my_max(-0x8000, my_min(out, 0x7FFF));
				break;
			case 1:
				out = yn & 0xffff;
				break;
			case 2:
				out = (yn >> 16);
				break;
			case 3:
				Halt("DSP: Unsupported Decoder output mode\n");
				break;
		}

		//Report(Channel::DSP, "0x%08X = 0x%04X\n", (Accel.CurrAddress.addr & 0x07FF'FFFF) - 1, (uint16_t)out);

		return (uint16_t)out;
	}
}

// AR is a separate Flipper module, but actually the SDRAM(ARAM) controller is in the DSP.
// It includes an SDRAM timing and access controller, as well as an "Accelerator" that also does ADPCM decompression.
// Part of the DSPCore registers are mapped to the PI-DSP interface (DMA, interrupts) and the other part to the SDRAM controller

// AR - auxiliary RAM (audio RAM) interface

/* ---------------------------------------------------------------------------
   useful bits from AIDCR :
		AIDCR_ARDMA         - ARAM dma in progress
		AIDCR_ARINTMSK      - mask (blocks PI)
		AIDCR_ARINT         - wr:clear, rd:dma int active

   short description of ARAM transfer :

	  AR_DMA_MMADDR_H = (AR_DMA_MMADDR_H & 0x03FF) | (mainmem_addr >> 16);
	  AR_DMA_MMADDR_L = (AR_DMA_MMADDR_L & 0x001F) | (mainmem_addr & 0xFFFF);

	  AR_DMA_ARADDR_H = (AR_DMA_ARADDR_H & 0x03FF) | (aram_addr >> 16);
	  AR_DMA_ARADDR_L = (AR_DMA_ARADDR_L & 0x001F) | (aram_addr & 0xFFFF);

	  AR_DMA_CNT_H = (AR_DMA_CNT_H & 0x7FFF) | (type << 15);    type - 0:RAM->ARAM, 1:ARAM->RAM
	  AR_DMA_CNT_H = (AR_DMA_CNT_H & 0x03FF) | (length >> 16);
	  AR_DMA_CNT_L = (AR_DMA_CNT_L & 0x001F) | (length & 0xFFFF);

   transfer starts, by writing into CNT_L

--------------------------------------------------------------------------- */

namespace DSP
{

	ARControl aram;

	static void ARINT()
	{
		AIDCR |= AIDCR_ARINT;
		if (AIDCR & AIDCR_ARINTMSK)
		{
			if (aram.log)
			{
				Report(Channel::AR, "ARINT\n");
			}
			PIAssertInt(PI_INTERRUPT_DSP);
		}
	}

	static void ARAMDmaThread(void* Parameter)
	{
		if (Core->GetTicks() < aram.gekkoTicks)
			return;
		aram.gekkoTicks = Core->GetTicks() + aram.gekkoTicksPerSlice;

		int type = aram.cnt >> 31;
		uint32_t cnt = aram.cnt & 0x3FF'FFE0;

		// blast data
		uint8_t* ptr = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForDSP(aram.mmaddr);
		if (type == RAM_TO_ARAM)
		{
			memcpy(&ARAM[aram.araddr], ptr, 32);
		}
		else
		{
			memcpy(ptr, &ARAM[aram.araddr], 32);
		}

		aram.araddr += 32;
		aram.mmaddr += 32;
		cnt -= 32;
		aram.cnt = cnt | (type << 31);

		if ((aram.cnt & ~0x8000'0000) == 0)
		{
			AIDCR &= ~AIDCR_ARDMA;
			ARINT();                    // invoke aram TC interrupt
			//if (aram.dspRunningBeforeAramDma)
			//{
			//    Flipper::HW->DSP->Run();
			//}
			if (aram.log) {
				Report(Channel::AR, "Suspending ARAM DMA Thread\n");
			}
			aram.dmaThread->Suspend();
		}
	}

	static void ARDMA()
	{
		int type = aram.cnt >> 31;
		int cnt = aram.cnt & 0x3FF'FFE0;
		bool specialAramDspDma = aram.mmaddr == 0x0100'0000 && aram.araddr == 0;

		// inform developer about aram transfers
		if (aram.log)
		{
			if (type == RAM_TO_ARAM)
			{
				if (!specialAramDspDma)
				{
					Report(Channel::AR, "RAM copy %08X -> %08X (%i)\n", aram.mmaddr, aram.araddr, cnt);
				}
			}
			else Report(Channel::AR, "ARAM copy %08X -> %08X (%i)\n", aram.araddr, aram.mmaddr, cnt);
		}

		// Special ARAM DMA (DSP Init)

		if (specialAramDspDma)
		{
			// Transfer size multiplied by 4
			cnt *= 4;

			// Special ARAM DMA to IRAM

			uint8_t* ptr = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForDSP(aram.mmaddr);
			Flipper::DSP->SpecialAramImemDma(ptr, cnt);

			aram.cnt &= 0x80000000;     // clear dma counter
			ARINT();                    // invoke aram TC interrupt
			return;
		}

		// ARAM driver is trying to check for expansion
		// by reading ARAM on high addresses
		// we are not allowing to read expansion
		if (aram.araddr >= ARAMSIZE)
		{
			if (type == ARAM_TO_RAM)
			{
				uint8_t* ptr = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForDSP(aram.mmaddr);
				memset(ptr, 0, cnt);

				aram.cnt &= 0x80000000;     // clear dma counter
				ARINT();                    // invoke aram TC interrupt
			}
			return;
		}

		// For fast transactions, complete the DMA immediately because interthreading takes longer than the DMA readiness check in ar.a::__ARCheckSize..

		if (cnt <= 32) {

			uint8_t* ptr = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForDSP(aram.mmaddr);
			if (type == RAM_TO_ARAM) {
				memcpy(&ARAM[aram.araddr], ptr, 32);
			}
			else {
				memcpy(ptr, &ARAM[aram.araddr], 32);
			}

			aram.araddr += 32;
			aram.mmaddr += 32;
			aram.cnt = 0;

			AIDCR &= ~AIDCR_ARDMA;
			ARINT();	// invoke aram TC interrupt
			return;
		}

		// For other cases - delegate job to thread

		if (aram.dmaThread->IsRunning()) {
			Halt("There is some nonsense going on: the ARAM DMA Thread needs to be started while it is still running.\n");
		}

		AIDCR |= AIDCR_ARDMA;
		aram.gekkoTicks = Core->GetTicks() + aram.gekkoTicksPerSlice;
		aram.dspRunningBeforeAramDma = Flipper::DSP->IsRunning();
		//if (aram.dspRunningBeforeAramDma)
		//{
		//    Flipper::HW->DSP->Suspend();
		//}
		if (aram.log) {
			Report(Channel::AR, "Resuming ARAM DMA Thread\n");
		}
		aram.dmaThread->Resume();
	}

	// ---------------------------------------------------------------------------
	// 16-bit ARAM registers

	// RAM pointer

	static void ar_write_maddr_h(uint32_t addr, uint32_t data, void* ctx)
	{
		aram.mmaddr &= 0x0000ffff;
		aram.mmaddr |= ((data & 0x3ff) << 16);
	}
	static void ar_read_maddr_h(uint32_t addr, uint32_t* reg, void* ctx) { *reg = (aram.mmaddr >> 16) & 0x3FF; }

	static void ar_write_maddr_l(uint32_t addr, uint32_t data, void* ctx)
	{
		aram.mmaddr &= 0xffff0000;
		aram.mmaddr |= ((data & ~0x1F) & 0xffff);
	}
	static void ar_read_maddr_l(uint32_t addr, uint32_t* reg, void* ctx) { *reg = (uint16_t)aram.mmaddr & ~0x1F; }

	// ARAM pointer

	static void ar_write_araddr_h(uint32_t addr, uint32_t data, void* ctx)
	{
		aram.araddr &= 0x0000ffff;
		aram.araddr |= ((data & 0x3FF) << 16);
	}
	static void ar_read_araddr_h(uint32_t addr, uint32_t* reg, void* ctx) { *reg = (aram.araddr >> 16) & 0x3FF; }

	static void ar_write_araddr_l(uint32_t addr, uint32_t data, void* ctx)
	{
		aram.araddr &= 0xffff0000;
		aram.araddr |= ((data & ~0x1F) & 0xffff);
	}
	static void ar_read_araddr_l(uint32_t addr, uint32_t* reg, void* ctx) { *reg = (uint16_t)aram.araddr & ~0x1F; }

	//
	// byte count register
	//

	static void ar_write_cnt_h(uint32_t addr, uint32_t data, void* ctx)
	{
		aram.cnt &= 0x0000ffff;
		aram.cnt |= ((data & 0x83FF) << 16);
	}
	static void ar_read_cnt_h(uint32_t addr, uint32_t* reg, void* ctx) { *reg = (aram.cnt >> 16) & 0x83FF; }

	static void ar_write_cnt_l(uint32_t addr, uint32_t data, void* ctx)
	{
		aram.cnt &= 0xffff0000;
		aram.cnt |= ((data & ~0x1F) & 0xffff);
		ARDMA();
	}
	static void ar_read_cnt_l(uint32_t addr, uint32_t* reg, void* ctx) { *reg = (uint16_t)aram.cnt & ~0x1F; }

	//
	// hacks
	//

	static void no_read(uint32_t addr, uint32_t* reg, void* ctx) { *reg = 0; }
	static void no_write(uint32_t addr, uint32_t data, void* ctx) {}

	static void ar_hack_size_r(uint32_t addr, uint32_t* reg, void *ctx) { *reg = aram.size; }
	static void ar_hack_size_w(uint32_t addr, uint32_t data, void* ctx) { aram.size = (uint16_t)data; }
	static void ar_hack_mode(uint32_t addr, uint32_t* reg, void* ctx) { *reg = 1; }

	// ---------------------------------------------------------------------------
	// init

	void AROpen()
	{
		Report(Channel::AR, "Aux. memory (ARAM) driver\n");

		// reallocate ARAM
		ARAM = new uint8_t[ARAMSIZE];

		// clear ARAM data
		memset(ARAM, 0, ARAMSIZE);

		// clear registers
		aram.mmaddr = aram.araddr = aram.cnt = 0;
		aram.gekkoTicksPerSlice = 4;
		aram.log = true;

		// set traps to aram registers
		PISetTrap(PI_REGSPACE_DSP | AR_DMA_MMADDR_H, ar_read_maddr_h, ar_write_maddr_h);
		PISetTrap(PI_REGSPACE_DSP | AR_DMA_MMADDR_L, ar_read_maddr_l, ar_write_maddr_l);
		PISetTrap(PI_REGSPACE_DSP | AR_DMA_ARADDR_H, ar_read_araddr_h, ar_write_araddr_h);
		PISetTrap(PI_REGSPACE_DSP | AR_DMA_ARADDR_L, ar_read_araddr_l, ar_write_araddr_l);
		PISetTrap(PI_REGSPACE_DSP | AR_DMA_CNT_H, ar_read_cnt_h, ar_write_cnt_h);
		PISetTrap(PI_REGSPACE_DSP | AR_DMA_CNT_L, ar_read_cnt_l, ar_write_cnt_l);

		// hacks
		PISetTrap(PI_REGSPACE_DSP | AR_SIZE, ar_hack_size_r, ar_hack_size_w);
		PISetTrap(PI_REGSPACE_DSP | AR_MODE, ar_hack_mode, no_write);
		PISetTrap(PI_REGSPACE_DSP | AR_REFRESH, no_read, no_write);

		aram.dmaThread = EMUCreateThread(ARAMDmaThread, true, nullptr, "ARAMDmaThread");
	}

	void ARClose()
	{
		EMUJoinThread(aram.dmaThread);
		aram.dmaThread = nullptr;

		// destroy ARAM
		if (ARAM)
		{
			delete[] ARAM;
			ARAM = nullptr;
		}
	}
}