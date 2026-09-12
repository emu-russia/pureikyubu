#include "pch.h"

// DSP ARAM Accelerator

// The accelerator is a streaming engine between the DSP data bus and the ARAM controller.
// Three parameter registers (starting / ending / current address) define a circular window
// in ARAM. The addresses are counted in units of the selected read addressing mode (4, 8 or
// 16 bits), see the audio hardware documentation ("ACCELERATOR", "HARDWARE deADPCM DECODER").
// Both the starting *and* the ending address belong to the window: the wrap to the starting
// address happens after the ending address has been accessed.
//
// On a real system the accelerator prefetches data into three 16-bit data lines (a FIFO
// between ARAM and the DSP). Here the lines are not modelled and ARAM is read directly,
// which is equivalent for a continuous stream; the documented core-halt behaviour (reading
// the lines before the prefetch has completed, or rewriting the parameter registers while
// the lines are not empty) is not reproduced.

using namespace Debug;

namespace DSP
{
	// Accelerator parameter registers are 27 bits wide
	static inline uint32_t AccelAddr(uint32_t value) { return value & 0x07ff'ffff; }

	uint16_t Dsp16::AccelFetch()
	{
		uint16_t val = 0;
		uint32_t addr = AccelAddr(Accel.CurrAddress.addr);

		switch (Accel.Fmt & 3)
		{
			case 0:
			{
				// 4-bit (nibble) addressing. The hardware fetches a 2-byte data line starting at
				// the even byte address ((addr >> 2) << 1) and uses the two low address bits to
				// select the nibble inside that line; taking the byte (addr >> 1) together with
				// its high nibble at even addresses (and the low nibble at odd ones) is the same.
				uint8_t dataByte = aram.mem[(addr >> 1) & (ARAMSIZE - 1)];
				val = (addr & 1) ? (dataByte & 0x0F) : (dataByte >> 4);
				break;
			}

			case 1:
				// 8-bit (byte) addressing: the byte at (bit25..bit1) + 0, i.e. the plain byte address
				val = aram.mem[addr & (ARAMSIZE - 1)];
				break;

			case 2:
				// 16-bit (word) addressing: the word at (bit24..bit0) + 0
				val = _BYTESWAP_UINT16(*(uint16_t*)(aram.mem + ((addr << 1) & (ARAMSIZE - 1))));
				break;

			default:
				// ADM[1:0] = 3 is a reserved addressing mode
				Report(Channel::DSP, "Accelerator: reserved addressing mode (ADM[1:0] = 3)\n");
				break;
		}

		// Advance the window. The direction bit (bit 15 of the high word) is not an address bit,
		// so it is preserved.
		uint32_t direction = Accel.CurrAddress.addr & 0x8000'0000;
		if (addr == AccelAddr(Accel.EndAddress.addr))
			Accel.CurrAddress.addr = direction | AccelAddr(Accel.StartAddress.addr);
		else
			Accel.CurrAddress.addr = direction | (addr + 1);

		return val;
	}

	// Read the accelerator data port and optionally pass the value through the decoder (raw=false)
	uint16_t Dsp16::AccelReadData(bool raw)
	{
		// Bit 15 of the high word of the current address selects the direction of the window
		if ((Accel.CurrAddress.h & 0x8000) != 0)
		{
			Report(Channel::DSP, "Accelerator: read from a write window (ACCAH direction bit is set)\n");
			return 0;
		}

		int opMode = (Accel.Fmt >> 2) & 3;
		int addressMode = Accel.Fmt & 3;

		// In the General IIR mode the decoder is disconnected from the accelerator and takes
		// x(n) from its own register, so reading y(n) does not touch ARAM at all.
		if (!raw && opMode == 1)
		{
			return DecodeAdpcm(Accel.AdpcmXn);
		}

		bool decoderFed = !raw && (opMode == 0 || opMode == 2);

		// A deADPCM frame is 8 bytes (16 nibbles) long: the first nibble of a frame is the 3-bit
		// predictor, the second one the 4-bit scale. That pair is consumed by the decoder, the
		// DSP never receives it as a sample.
		if (!raw && opMode == 0 && addressMode == 0 && (AccelAddr(Accel.CurrAddress.addr) & 0xF) == 0)
		{
			Accel.AdpcmPds = aram.mem[(AccelAddr(Accel.CurrAddress.addr) >> 1) & (ARAMSIZE - 1)];
			AccelFetch();		// predictor nibble
			AccelFetch();		// scale nibble
		}

		uint32_t addr = AccelAddr(Accel.CurrAddress.addr);
		uint16_t val = AccelFetch();

		// Read-side window interrupt. The documentation describes two of them: the
		// accelerator raises ACRS when the read data is at the starting address, and the
		// decoder raises DCRE at the end of its loop window (deADPCM / PCM IIR).
		//
		// JAudio does not survive the ACRS-at-start variant (the title stops dead), so for
		// now only the end-of-window interrupt is generated, as it was before; the exact
		// ACRS semantics still need to be worked out.
		if (addr == AccelAddr(Accel.EndAddress.addr))
		{
			core->AssertInterrupt(decoderFed ? DspInterrupt::Dcre : DspInterrupt::Acrs);
		}

		if (!raw)
		{
			val = DecodeAdpcm(val);
		}

		return val;
	}

	// Write data to ARAM. A write window is always 16-bit wide, no matter which addressing
	// mode is programmed for reads.
	void Dsp16::AccelWriteData(uint16_t data)
	{
		// Bit 15 of the high word of the current address selects the direction of the window
		if ((Accel.CurrAddress.h & 0x8000) == 0)
		{
			Report(Channel::DSP, "Accelerator: write to a read window (ACCAH direction bit is clear)\n");
			return;
		}

		uint32_t addr = AccelAddr(Accel.CurrAddress.addr);
		*(uint16_t*)(aram.mem + ((addr << 1) & (ARAMSIZE - 1))) = _BYTESWAP_UINT16(data);

		// Write-end interrupt: the data written to the ending address has been stored
		if (addr == AccelAddr(Accel.EndAddress.addr))
		{
			core->AssertInterrupt(DspInterrupt::Acwe);
			Accel.CurrAddress.addr = 0x8000'0000 | AccelAddr(Accel.StartAddress.addr);
		}
		else
		{
			Accel.CurrAddress.addr = 0x8000'0000 | (addr + 1);
		}
	}

}

// DSP ADPCM / IIR decoder

// Implements y(n) = a2*y(n-2) + a1*y(n-1) + gain*x(n) with a 34-bit accumulator and a 16-bit
// clamped output, see "HARDWARE deADPCM DECODER" in the audio hardware documentation.
//
//  - deADPCM mode: x(n) is a 4-bit signed sample from the accelerator data lines. Frames are
//    8 bytes long; predictor and scale are the first two nibbles of a frame, they are consumed
//    by the decoder. The hardware rounds the MAC result by adding 0x400.
//  - General IIR mode: x(n) comes from the x(n) register, gain is a 16-bit register, no
//    rounding is applied and the decoder does not touch the accelerator.
//  - PCM IIR mode: x(n) comes from the data lines, scaled according to the read addressing
//    mode, and the 16-bit gain register is used.
//
// Reading y(n) latches the result, shifts the history (y(n) -> y(n-1) -> y(n-2)) and, in the
// modes fed by the accelerator, advances its current address.

namespace DSP
{
	uint16_t Dsp16::DecodeAdpcm(uint16_t in)
	{
		int64_t yn = 0;
		int outputMode = (Accel.Fmt >> 4) & 3;
		int opMode = (Accel.Fmt >> 2) & 3;
		int addressMode = Accel.Fmt & 3;

		int pred = (Accel.AdpcmPds >> 4) & 7;

		switch (opMode)
		{
			case 0:
			{
				// deADPCM: 4-bit signed x(n) from the data lines, gain = 2^scale (scale <= 0xC)
				int scale = Accel.AdpcmPds & 0x0F;
				if (scale > 0x0C)
					scale = 0x0C;
				int32_t gain = 1 << scale;

				int16_t xn = (int16_t)(in << 11);

				yn = (int64_t)(int32_t)(int16_t)Accel.AdpcmYn1 * (int64_t)(int32_t)(int16_t)Accel.AdpcmCoef[2 * pred]
					+ (int64_t)(int32_t)(int16_t)Accel.AdpcmYn2 * (int64_t)(int32_t)(int16_t)Accel.AdpcmCoef[2 * pred + 1]
					+ (int64_t)(int32_t)xn * gain
					+ 0x400;			// rounding, part of the MAC operation

				Accel.AdpcmYn2 = Accel.AdpcmYn1;
				Accel.AdpcmYn1 = (uint16_t)(yn >> 11);
				break;
			}

			case 1:
			{
				// General IIR: x(n) and gain come from registers, no rounding
				int16_t xn = (int16_t)Accel.AdpcmXn;

				yn = (int64_t)(int32_t)(int16_t)Accel.AdpcmYn1 * (int64_t)(int32_t)(int16_t)Accel.AdpcmCoef[2 * pred]
					+ (int64_t)(int32_t)(int16_t)Accel.AdpcmYn2 * (int64_t)(int32_t)(int16_t)Accel.AdpcmCoef[2 * pred + 1]
					+ (int64_t)(int32_t)xn * (int64_t)(int32_t)(int16_t)Accel.AdpcmGan;

				Accel.AdpcmYn2 = Accel.AdpcmYn1;
				Accel.AdpcmYn1 = (uint16_t)(yn >> 11);
				break;
			}

			case 2:
			{
				// PCM IIR: x(n) from the data lines, scaled by the selected addressing mode
				int16_t xn = 0;

				switch (addressMode)
				{
					case 0: xn = (int16_t)(((in & 0x08) ? (in | 0xFFF0) : in) << 12); break;	// 4-bit
					case 1: xn = (int16_t)((in << 8)); break;								// 8-bit
					default: xn = (int16_t)in; break;										// 16-bit
				}

				yn = (int64_t)(int32_t)(int16_t)Accel.AdpcmYn1 * (int64_t)(int32_t)(int16_t)Accel.AdpcmCoef[2 * pred]
					+ (int64_t)(int32_t)(int16_t)Accel.AdpcmYn2 * (int64_t)(int32_t)(int16_t)Accel.AdpcmCoef[2 * pred + 1]
					+ (int64_t)(int32_t)xn * (int64_t)(int32_t)(int16_t)Accel.AdpcmGan;

				Accel.AdpcmYn2 = Accel.AdpcmYn1;
				Accel.AdpcmYn1 = (uint16_t)(yn >> 11);
				break;
			}

			default:
				// ADM[3:2] = 3 is a reserved decoder mode
				Report(Channel::DSP, "Decoder: reserved operation mode (ADM[3:2] = 3)\n");
				return 0;
		}

		// y(n) -> DSP data bus (ADM[5:4] selects the format, the default one is clamped bits 26:11)
		switch (outputMode)
		{
			case 0:
				yn = my_max(-0x8000, my_min(yn >> 11, 0x7FFF));
				break;
			case 1:
				yn = yn & 0xFFFF;					// bits 15:0, no clamping
				break;
			case 2:
				yn = (yn >> 16) & 0xFFFF;			// bits 31:16, no clamping
				break;
			case 3:
				yn = (yn & 0x2'0000'0000) ? 0xFFFF : 0x0000;	// sign extended bits 33:32
				break;
		}

		return (uint16_t)yn;
	}
}

// AR is a separate Flipper module, but actually the SDRAM(ARAM) controller is in the DSP.
// It includes an SDRAM timing and access controller, as well as an "Accelerator" that also does ADPCM decompression.
// Part of the DSPCore registers are mapped to the PI-DSP interface (DMA, interrupts) and the other part to the SDRAM controller

// AR - auxiliary RAM (audio RAM) interface

/* ---------------------------------------------------------------------------
   useful bits from CDCR :
		CDCR_ARDMA         - ARAM dma in progress
		CDCR_ARINTMSK      - mask (blocks PI)
		CDCR_ARINT         - wr:clear, rd:dma int active

   short description of ARAM transfer :

	  AMMAH = (AMMAH & 0x03FF) | (mainmem_addr >> 16);
	  AMMAL = (AMMAL & 0x001F) | (mainmem_addr & 0xFFFF);

	  AMAAH = (AMAAH & 0x03FF) | (aram_addr >> 16);
	  AMAAL = (AMAAL & 0x001F) | (aram_addr & 0xFFFF);

	  AMBLH = (AMBLH & 0x7FFF) | (type << 15);    type - 0:RAM->ARAM, 1:ARAM->RAM
	  AMBLH = (AMBLH & 0x03FF) | (length >> 16);
	  AMBLL = (AMBLL & 0x001F) | (length & 0xFFFF);

   transfer starts, by writing into AMBLL

--------------------------------------------------------------------------- */

namespace DSP
{

	ARControl aram;

// The three internal DSP causes (the DSP->CPU mailbox, the ARAM DMA completion and the AI DMA
// completion) are latched in CDCR and reported to the CPU through ONE aggregate Processor
// Interface line, PI_INTERRUPT_DSP. The line is therefore not "asserted by" any single cause:
// it follows the OR of the ones that are currently latched *and* unmasked, and every place that
// changes a cause has to re-evaluate it.
//
// Evaluating it only where a cause is raised left the line stuck: the guest acknowledges a cause
// by writing the matching bit back to CDCR (write-1-to-clear), and while the line was computed
// from "all three are clear" inside that one write path, clearing one cause while another was
// still latched left the line high with nothing left to clear it. The CPU then re-entered the
// external-interrupt handler on every rfi and never returned to the guest.
void DSPUpdateInt()
{
	uint16_t pending = CDCR & (CDCR_DSPINT | CDCR_ARINT | CDCR_AIINT);
	uint16_t unmasked = CDCR & (CDCR_DSPINTMSK | CDCR_ARINTMSK | CDCR_AIINTMSK);

	if ((pending & unmasked) != 0)
	{
		Flipper::HW->pi->PIAssertInt(PI_INTERRUPT_DSP);
	}
	else
	{
		Flipper::HW->pi->PIClearInt(PI_INTERRUPT_DSP);
	}
}
	static void ARINT()
	{
		CDCR |= CDCR_ARINT;
		if ((CDCR & CDCR_ARINTMSK) && aram.log)
		{
			Report(Channel::AR, "ARINT\n");
		}
		// The ARAM completion is one of the three causes behind the aggregate PI line.
		DSPUpdateInt();
	}

	// Perform the whole ARAM transfer. The hardware streams the block through the ARAM controller
	// and raises CDCR_ARINT when the last byte has been written; the emulator performs it in one go,
	// like the other transfer engines (SI, EXI, DVD).
	//
	// The transfer used to be sliced into 32-byte steps driven by a worker thread. That conflicts
	// with the CPU, which is the only other initiator of an ARAM DMA: a driver that starts the next
	// block before the worker had finished the previous one hit the thread guard (Halt) and had its
	// request dropped, so it waited for a completion interrupt that never came. Metroid Prime hung
	// that way right after its intro movie, while the audio kept streaming.
	static void ARAMDmaRun()
	{
		int type = aram.cnt >> 31;
		uint32_t cnt = aram.cnt & 0x03FF'FFE0;

		// The DSP can mask ARAM-DMA requests, dedicating ARAM to the accelerator. The request is
		// only held back if the DMA has not been started at all (a masked request while a transfer
		// runs would deadlock the driver); the emulator completes the block at once, so the mask
		// can only be seen before the first slice.
		if (aram.masked)
			return;

		// Main memory can be seen only through the DSP window.
		uint8_t* ptr = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForDSP(aram.mmaddr);
		bool beyondAram = aram.araddr >= ARAMSIZE;

		if (beyondAram)
		{
			// No expansion module is installed: a read returns zeros, a write is discarded
			if (type == ARAM_TO_RAM && ptr != nullptr)
				memset(ptr, 0, cnt);
		}
		else if (ptr != nullptr)
		{
			if (type == RAM_TO_ARAM)
				memcpy(&ARAM[aram.araddr], ptr, cnt);
			else
				memcpy(ptr, &ARAM[aram.araddr], cnt);
		}

		aram.araddr += cnt;
		aram.mmaddr += cnt;
		aram.cnt = 0;

		CDCR &= ~CDCR_ARDMA;
		ARINT();                    // invoke aram DMA completion interrupt
	}

	static void ARDMA()
	{
		int type = aram.cnt >> 31;
		int cnt = aram.cnt & 0x03FF'FFE0;
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

		// The AR driver probes for an ARAM expansion module by moving test blocks to and from
		// addresses at and beyond the 16 MB boundary. No expansion is installed on a retail
		// console, so such a transfer reads zeros / discards the written data - but it still has
		// to run to completion and raise the completion interrupt, otherwise a driver that polls
		// the busy flag (AMBL counter) or waits for the interrupt would hang.
		ARAMDmaRun();
	}

	// ---------------------------------------------------------------------------
	// 16-bit ARAM registers

	// RAM pointer

	static void am_write_mmah(uint32_t addr, uint32_t data, void* ctx)
	{
		aram.mmaddr &= 0x0000ffff;
		aram.mmaddr |= ((data & 0x3ff) << 16);
	}
	static void am_read_mmah(uint32_t addr, uint32_t* reg, void* ctx) { *reg = (aram.mmaddr >> 16) & 0x3FF; }

	static void am_write_mmal(uint32_t addr, uint32_t data, void* ctx)
	{
		aram.mmaddr &= 0xffff0000;
		aram.mmaddr |= ((data & ~0x1F) & 0xffff);
	}
	static void am_read_mmal(uint32_t addr, uint32_t* reg, void* ctx) { *reg = (uint16_t)aram.mmaddr & ~0x1F; }

	// ARAM pointer

	static void am_write_amaah(uint32_t addr, uint32_t data, void* ctx)
	{
		aram.araddr &= 0x0000ffff;
		aram.araddr |= ((data & 0x3FF) << 16);
	}
	static void am_read_amaah(uint32_t addr, uint32_t* reg, void* ctx) { *reg = (aram.araddr >> 16) & 0x3FF; }

	static void am_write_amaal(uint32_t addr, uint32_t data, void* ctx)
	{
		aram.araddr &= 0xffff0000;
		aram.araddr |= ((data & ~0x1F) & 0xffff);
	}
	static void am_read_amaal(uint32_t addr, uint32_t* reg, void* ctx) { *reg = (uint16_t)aram.araddr & ~0x1F; }

	//
	// byte count register
	//

	static void am_write_amblh(uint32_t addr, uint32_t data, void* ctx)
	{
		aram.cnt &= 0x0000ffff;
		aram.cnt |= ((data & 0x83FF) << 16);
	}
	static void am_read_amblh(uint32_t addr, uint32_t* reg, void* ctx) { *reg = (aram.cnt >> 16) & 0x83FF; }

	static void am_write_ambll(uint32_t addr, uint32_t data, void* ctx)
	{
		aram.cnt &= 0xffff0000;
		aram.cnt |= ((data & ~0x1F) & 0xffff);
		ARDMA();
	}
	static void am_read_ambll(uint32_t addr, uint32_t* reg, void* ctx) { *reg = (uint16_t)aram.cnt & ~0x1F; }

	//
	// AMCR / AMNF / AMCT
	//

	static void no_read(uint32_t addr, uint32_t* reg, void* ctx) { *reg = 0; }
	static void no_write(uint32_t addr, uint32_t data, void* ctx) {}

	// The AR driver writes the ARAM geometry into AMCR (as the driver calls this register:
	// AR_SIZE) and reads it back. 0x43 means "16 MB internal, no expansion".
	static void am_read_amcr(uint32_t addr, uint32_t* reg, void *ctx) { *reg = aram.amcr; }
	static void am_write_amcr(uint32_t addr, uint32_t data, void* ctx) { aram.amcr = (uint16_t)data; }

	// AMNF - "normal state" flag: the SDRAM controller runs its initialisation after reset and
	// then reports ready. Nothing may be transferred before this bit is set, and the driver
	// waits for it, so the emulated controller is always ready.
	static void am_read_amnf(uint32_t addr, uint32_t* reg, void* ctx) { *reg = 1; }

	// ---------------------------------------------------------------------------
	// init

	void AROpen(Flipper::Flipper* flipper)
	{
		Report(Channel::AR, "Aux. memory (ARAM) driver\n");

		// reallocate ARAM
		ARAM = new uint8_t[ARAMSIZE];

		// clear ARAM data
		memset(ARAM, 0, ARAMSIZE);

		// clear registers
		aram.mmaddr = aram.araddr = aram.cnt = 0;
		aram.amcr = 0x43;			// 16 MB internal ARAM, no expansion
		aram.masked = false;
		aram.log = true;

		// set traps to aram registers
		// (the unit tests initialise the controller without a Flipper instance)
		if (flipper != nullptr)
		{
			flipper->pi->PISetTrap(PI_REGSPACE_DSP | AMMAH, am_read_mmah, am_write_mmah);
			flipper->pi->PISetTrap(PI_REGSPACE_DSP | AMMAL, am_read_mmal, am_write_mmal);
			flipper->pi->PISetTrap(PI_REGSPACE_DSP | AMAAH, am_read_amaah, am_write_amaah);
			flipper->pi->PISetTrap(PI_REGSPACE_DSP | AMAAL, am_read_amaal, am_write_amaal);
			flipper->pi->PISetTrap(PI_REGSPACE_DSP | AMBLH, am_read_amblh, am_write_amblh);
			flipper->pi->PISetTrap(PI_REGSPACE_DSP | AMBLL, am_read_ambll, am_write_ambll);

			// controller configuration registers
			flipper->pi->PISetTrap(PI_REGSPACE_DSP | AMCR, am_read_amcr, am_write_amcr);
			flipper->pi->PISetTrap(PI_REGSPACE_DSP | AMNF, am_read_amnf, no_write);
			flipper->pi->PISetTrap(PI_REGSPACE_DSP | AMCT, no_read, no_write);
		}
	}

	void ARClose()
	{

		// destroy ARAM
		if (ARAM)
		{
			delete[] ARAM;
			ARAM = nullptr;
		}
	}
}