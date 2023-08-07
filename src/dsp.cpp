#include "pch.h"

// The real DSP is designed as follows: [DSP PI Interface] <-> [DSPCore] <-> [ARAM Controller/Accelerator]
// The DSPCore includes the actual DSP instruction execution core ("Engine"), as well as the IMEM/DMEM and the register decoder (0xff00...)
// DSP PI I/F deals with PI communication (interrupts, Mailbox) and also contains 3 DMA controllers: DSP IMEM/DMEM DMA, AI DMA, ARAM DMA
// ARAM(SDRAM) Controller controls access to external SDRAM memory (ARAM) and also contains the "Accelerator"

using namespace Debug;

namespace DSP
{

	Dsp16::Dsp16()
	{
		dspThread = EMUCreateThread(DspThreadProc, true, this, "DspCore");

		core = new DspCore(this);

		JDI::Hub.AddNode(DSP_JDI_JSON, dsp_init_handlers);
	}

	Dsp16::~Dsp16()
	{
		EMUJoinThread(dspThread);
		delete core;

		JDI::Hub.RemoveNode(DSP_JDI_JSON);
	}

	bool Dsp16::LoadIrom(std::vector<uint8_t>& iromImage)
	{
		if (iromImage.empty() || iromImage.size() != IROM_SIZE)
		{
			return false;
		}
		else
		{
			memcpy(irom, iromImage.data(), IROM_SIZE);
		}

		return true;
	}

	bool Dsp16::LoadDrom(std::vector<uint8_t>& dromImage)
	{
		if (dromImage.empty() || dromImage.size() != DROM_SIZE)
		{
			return false;
		}
		else
		{
			memcpy(drom, dromImage.data(), DROM_SIZE);
		}

		return true;
	}

	void Dsp16::DspThreadProc(void* Parameter)
	{
		Dsp16* dsp = (Dsp16*)Parameter;

		// Do DSP actions
		dsp->core->Update();
	}

	void Dsp16::Run()
	{
		if (!dspThread->IsRunning())
		{
			dspThread->Resume();
			if (logDspControlBits)
			{
				Report(Channel::DSP, "Run\n");
			}
			savedGekkoTicks = Core->GetTicks();
		}
	}

	void Dsp16::Suspend()
	{
		if (dspThread->IsRunning())
		{
			if (logDspControlBits)
			{
				Report(Channel::DSP, "Suspend\n");
			}
			dspThread->Suspend();
		}
	}

	void Dsp16::ResetIfx()
	{
		DspToCpuMailbox[0] = 0;
		DspToCpuMailbox[1] = 0;
		CpuToDspMailbox[0] = 0;
		CpuToDspMailbox[1] = 0;

		memset(&DmaRegs, 0, sizeof(DmaRegs));
		memset(&Accel, 0, sizeof(Accel));
	}

	// Dump IFX State
	void Dsp16::DumpIfx()
	{
		Report(Channel::Norm, "Cpu2Dsp Mailbox: Hi: 0x%04X, Lo: 0x%04X\n",
			(uint16_t)CpuToDspMailbox[0], (uint16_t)CpuToDspMailbox[1]);
		Report(Channel::Norm, "Dsp2Cpu Mailbox: Hi: 0x%04X, Lo: 0x%04X\n",
			(uint16_t)DspToCpuMailbox[0], (uint16_t)DspToCpuMailbox[1]);
		Report(Channel::Norm, "Dma: MmemAddr: 0x%08X, DspAddr: 0x%04X, Size: 0x%04X, Ctrl: %i\n",
			DmaRegs.mmemAddr.bits, DmaRegs.dspAddr, DmaRegs.blockSize, DmaRegs.control.bits);
		for (int i = 0; i < 16; i++)
		{
			Report(Channel::Norm, "Dsp Coef[%i]: 0x%04X\n", i, Accel.AdpcmCoef[i]);
		}
	}

#pragma region "Memory Engine"

	uint8_t* Dsp16::TranslateIMem(DspAddress addr)
	{
		if (addr < (IRAM_SIZE / 2))
		{
			return &iram[addr << 1];
		}
		else if (addr >= IROM_START_ADDRESS && addr < (IROM_START_ADDRESS + (IROM_SIZE / 2)))
		{
			return &irom[(addr - IROM_START_ADDRESS) << 1];
		}
		else
		{
			return nullptr;
		}
	}

	uint8_t* Dsp16::TranslateDMem(DspAddress addr)
	{
		if (addr < (DRAM_SIZE / 2))
		{
			return &dram[addr << 1];
		}
		else if (addr >= DROM_START_ADDRESS && addr < (DROM_START_ADDRESS + (DROM_SIZE / 2)))
		{
			return &drom[(addr - DROM_START_ADDRESS) << 1];
		}
		else
		{
			return nullptr;
		}
	}

	uint16_t Dsp16::ReadIMem(DspAddress addr)
	{
		uint8_t* ptr = TranslateIMem(addr);

		if (ptr)
		{
			return _BYTESWAP_UINT16(*(uint16_t*)ptr);
		}

		return 0;
	}

	uint16_t Dsp16::ReadDMem(DspAddress addr)
	{
		if (core->TestWatch(addr))
		{
			Report(Channel::DSP, "ReadDMem 0x%04X, pc: 0x%04X\n", addr, core->regs.pc);
		}

		if (addr >= IFX_START_ADDRESS)
		{
			switch (addr)
			{
				case (DspAddress)DspHardwareRegs::DSMAH:
					return DmaRegs.mmemAddr.h;
				case (DspAddress)DspHardwareRegs::DSMAL:
					return DmaRegs.mmemAddr.l;
				case (DspAddress)DspHardwareRegs::DSPA:
					return DmaRegs.dspAddr;
				case (DspAddress)DspHardwareRegs::DSCR:
					return DmaRegs.control.bits;
				case (DspAddress)DspHardwareRegs::DSBL:
					return DmaRegs.blockSize;

				case (DspAddress)DspHardwareRegs::CMBH:
					return CpuToDspReadHi(true);
				case (DspAddress)DspHardwareRegs::CMBL:
					return CpuToDspReadLo(true);
				case (DspAddress)DspHardwareRegs::DMBH:
					return DspToCpuReadHi(true);
				case (DspAddress)DspHardwareRegs::DMBL:
					return DspToCpuReadLo(true);

				case (DspAddress)DspHardwareRegs::DIRQ:
					return Flipper::DSPGetInterruptStatus() ? 1 : 0;

				case (DspAddress)DspHardwareRegs::ACSAH:
					return Accel.StartAddress.h;
				case (DspAddress)DspHardwareRegs::ACSAL:
					return Accel.StartAddress.l;
				case (DspAddress)DspHardwareRegs::ACEAH:
					return Accel.EndAddress.h;
				case (DspAddress)DspHardwareRegs::ACEAL:
					return Accel.EndAddress.l;
				case (DspAddress)DspHardwareRegs::ACCAH:
					return Accel.CurrAddress.h;
				case (DspAddress)DspHardwareRegs::ACCAL:
					return Accel.CurrAddress.l;

				case (DspAddress)DspHardwareRegs::ACFMT:
					return Accel.Fmt;
				case (DspAddress)DspHardwareRegs::ACPDS:
					return Accel.AdpcmPds;
				case (DspAddress)DspHardwareRegs::ACYN1:
					return Accel.AdpcmYn1;
				case (DspAddress)DspHardwareRegs::ACYN2:
					return Accel.AdpcmYn2;
				case (DspAddress)DspHardwareRegs::ACGAN:
					return Accel.AdpcmGan;

				case (DspAddress)DspHardwareRegs::ACDAT2:
					return AccelReadData(true);
				case (DspAddress)DspHardwareRegs::ACDAT:
					return AccelReadData(false);

				default:
					Report(Channel::DSP, "Unknown HW read 0x%04X\n", addr);
					break;
			}

			return 0;
		}

		uint8_t* ptr = TranslateDMem(addr);

		if (ptr)
		{
			return _BYTESWAP_UINT16(*(uint16_t*)ptr);
		}
		else
		{
			if (haltOnUnmappedMemAccess)
			{
				Halt("DSP Unmapped DMEM read 0x%04X\n", addr);
				Suspend();
			}
		}

		return 0xFFFF;
	}

	void Dsp16::WriteDMem(DspAddress addr, uint16_t value)
	{
		if (core->TestWatch(addr))
		{
			Report(Channel::DSP, "WriteDMem 0x%04X = 0x%04X, pc: 0x%04X\n", addr, value, core->regs.pc);
		}

		if (addr >= IFX_START_ADDRESS)
		{
			switch (addr)
			{
				case (DspAddress)DspHardwareRegs::DSMAH:
					DmaRegs.mmemAddr.h = value & 0x03ff;
					if (logDspDma)
					{
						Report(Channel::DSP, "DSMAH: 0x%04X\n", DmaRegs.mmemAddr.h);
					}
					break;
				case (DspAddress)DspHardwareRegs::DSMAL:
					DmaRegs.mmemAddr.l = value & ~3;
					if (logDspDma)
					{
						Report(Channel::DSP, "DSMAL: 0x%04X\n", DmaRegs.mmemAddr.l);
					}
					break;
				case (DspAddress)DspHardwareRegs::DSPA:
					DmaRegs.dspAddr = value & ~1;
					if (logDspDma)
					{
						Report(Channel::DSP, "DSPA: 0x%04X\n", DmaRegs.dspAddr);
					}
					break;
				case (DspAddress)DspHardwareRegs::DSCR:
					DmaRegs.control.bits = value & 3;
					if (logDspDma)
					{
						Report(Channel::DSP, "DSCR: 0x%04X\n", DmaRegs.control.bits);
					}
					break;
				case (DspAddress)DspHardwareRegs::DSBL:
					DmaRegs.blockSize = value & ~3;
					if (logDspDma)
					{
						Report(Channel::DSP, "DSBL: 0x%04X\n", DmaRegs.blockSize);
					}
					DoDma();
					break;

				case (DspAddress)DspHardwareRegs::CMBH:
					Halt("DSP is not allowed to write processor Mailbox!\n");
					Suspend();
					break;
				case (DspAddress)DspHardwareRegs::CMBL:
					Halt("DSP is not allowed to write processor Mailbox!\n");
					Suspend();
					break;
				case (DspAddress)DspHardwareRegs::DMBH:
					DspToCpuWriteHi(value);
					break;
				case (DspAddress)DspHardwareRegs::DMBL:
					DspToCpuWriteLo(value);
					break;

				case (DspAddress)DspHardwareRegs::DIRQ:
					if (value & 1)
					{
						if (logDspInterrupts)
						{
							Report(Channel::DSP, "DspHardwareRegs::DIRQ\n");
						}
						Flipper::DSPAssertInt();
					}
					break;

				case (DspAddress)DspHardwareRegs::ACSAH:
					Accel.StartAddress.h = value;
					if (logAccel)
					{
						Report(Channel::DSP, "ACSAH = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACSAL:
					Accel.StartAddress.l = value;
					if (logAccel)
					{
						Report(Channel::DSP, "ACSAL = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACEAH:
					Accel.EndAddress.h = value;
					if (logAccel)
					{
						Report(Channel::DSP, "ACEAH = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACEAL:
					Accel.EndAddress.l = value;
					if (logAccel)
					{
						Report(Channel::DSP, "ACEAL = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACCAH:
					Accel.CurrAddress.h = value;
					if (logAccel)
					{
						Report(Channel::DSP, "ACCAH = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACCAL:
					Accel.CurrAddress.l = value;
					if (logAccel)
					{
						Report(Channel::DSP, "ACCAL = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACDAT2:
					AccelWriteData(value);
					if (logAccel)
					{
						Report(Channel::DSP, "ACDAT2 = 0x%04X\n", value);
					}
					break;

				case (DspAddress)DspHardwareRegs::ACFMT:
					Accel.Fmt = value;
					if (logAccel || logAdpcm)
					{
						Report(Channel::DSP, "ACFMT = 0x%04X\n", value);
					}
					break;

				case (DspAddress)DspHardwareRegs::ACPDS:
					Accel.AdpcmPds = value;
					if (logAdpcm)
					{
						Report(Channel::DSP, "ACPDS = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACYN1:
					Accel.AdpcmYn1 = value;
					if (logAdpcm)
					{
						Report(Channel::DSP, "ACYN1 = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACYN2:
					Accel.AdpcmYn2 = value;
					if (logAdpcm)
					{
						Report(Channel::DSP, "ACYN2 = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACGAN:
					Accel.AdpcmGan = value;
					if (logAdpcm)
					{
						Report(Channel::DSP, "ACGAN = 0x%04X\n", value);
					}
					break;

				case (DspAddress)DspHardwareRegs::ADPCM_A00:
					Accel.AdpcmCoef[0] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A10:
					Accel.AdpcmCoef[1] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A20:
					Accel.AdpcmCoef[2] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A30:
					Accel.AdpcmCoef[3] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A40:
					Accel.AdpcmCoef[4] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A50:
					Accel.AdpcmCoef[5] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A60:
					Accel.AdpcmCoef[6] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A70:
					Accel.AdpcmCoef[7] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A01:
					Accel.AdpcmCoef[8] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A11:
					Accel.AdpcmCoef[9] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A21:
					Accel.AdpcmCoef[10] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A31:
					Accel.AdpcmCoef[11] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A41:
					Accel.AdpcmCoef[12] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A51:
					Accel.AdpcmCoef[13] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A61:
					Accel.AdpcmCoef[14] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A71:
					Accel.AdpcmCoef[15] = value;
					break;

				case (DspAddress)DspHardwareRegs::UNKNOWN_FFB0:
				case (DspAddress)DspHardwareRegs::UNKNOWN_FFB1:
					Report(Channel::DSP, "Known unknown HW write 0x%04X = 0x%04X\n", addr, value);
					break;

				default:
					Report(Channel::DSP, "Unknown HW write 0x%04X = 0x%04X\n", addr, value);
					break;
			}
			return;
		}

		if (addr < (DRAM_SIZE / 2))
		{
			uint8_t* ptr = TranslateDMem(addr);

			if (ptr)
			{
				*(uint16_t*)ptr = _BYTESWAP_UINT16(value);
				return;
			}
		}

		if (haltOnUnmappedMemAccess)
		{
			Halt("DSP Unmapped DMEM write 0x%04X = 0x%04X\n", addr, value);
			Suspend();
		}
	}

#pragma endregion "Memory Engine"


#pragma region "Flipper interface"

	void Dsp16::SetResetBit(bool val)
	{
		if (val)
		{
			core->AssertInterrupt(DspInterrupt::Reset);
		}
	}

	bool Dsp16::GetResetBit()
	{
		return false;
	}

	void Dsp16::SetIntBit(bool val)
	{
		if (val)
		{
			core->AssertInterrupt(DspInterrupt::CpuInt);
		}
	}

	bool Dsp16::GetIntBit()
	{
		return core->IsInterruptPending(DspInterrupt::CpuInt);
	}

	void Dsp16::SetHaltBit(bool val)
	{
		val ? Suspend() : Run();
	}

	bool Dsp16::GetHaltBit()
	{
		return !dspThread->IsRunning();
	}

#pragma endregion "Flipper interface"

}

// DSP ARAM Accelerator

// Accelerator addresses work in accordance with the selected mode (4-bit, 8-bit, 16-bit). 
// That is, for example, in 4-bit mode - the start, current and end addresses point to nibble in ARAM.

// On a real system, the next piece of data is cached in 3 16-bit registers ("output ports"). 
// Here we do not repeat this mechanism, but refer directly to ARAM.

// The accelerator can work both independently, simply driving data between ARAM and DSP, and in conjunction with an ADPCM decoder.
// In the first case, one register is used (ACDAT, read-write), in the second case, another (ACDAT2, read-only).

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



namespace DSP
{

	// Instant DMA
	void Dsp16::DoDma()
	{
		uint8_t* ptr = nullptr;

		if (logDspDma)
		{
			Report(Channel::DSP, "Dsp16::Dma: Mmem: 0x%08X, DspAddr: 0x%04X, Size: 0x%04X, Ctrl: %i\n",
				DmaRegs.mmemAddr.bits, DmaRegs.dspAddr, DmaRegs.blockSize, DmaRegs.control.bits);
		}

		if (DmaRegs.control.Imem)
		{
			ptr = TranslateIMem(DmaRegs.dspAddr);
		}
		else
		{
			ptr = TranslateDMem(DmaRegs.dspAddr);
		}

		if (ptr == nullptr)
		{
			Halt("Dsp16::DoDma: invalid dsp address: 0x%04X\n", DmaRegs.dspAddr);
			return;
		}

		if (DmaRegs.mmemAddr.bits < mi.ramSize)
		{
			if (DmaRegs.control.Dsp2Mmem)
			{
				memcpy(&mi.ram[DmaRegs.mmemAddr.bits], ptr, DmaRegs.blockSize);
			}
			else
			{
				memcpy(ptr, &mi.ram[DmaRegs.mmemAddr.bits], DmaRegs.blockSize);
			}
		}

		// Dump ucode.
		if (dumpUcode)
		{
			if (DmaRegs.control.Imem && !DmaRegs.control.Dsp2Mmem)
			{
				std::string filename = "Data/DspUcode_" + std::to_string(DmaRegs.blockSize) + ".bin";
				auto buffer = std::vector<uint8_t>(ptr, ptr + DmaRegs.blockSize);

				Util::FileSave(filename, buffer);
				Report(Channel::DSP, "Ucode dumped to %s\n", filename.c_str());
			}
		}

		// Dump PCM samples coming from mixer
#if 0
		if (!DmaRegs.control.Imem && DmaRegs.control.Dsp2Mmem &&
			(0x400 >= DmaRegs.dspAddr && DmaRegs.dspAddr < 0x600) &&
			DmaRegs.blockSize == 0x80)
		{
			memcpy(&dspSamples[dspSampleSize], ptr, DmaRegs.blockSize);
			dspSampleSize += DmaRegs.blockSize;
		}
#endif

	}

	void Dsp16::SpecialAramImemDma(uint8_t* ptr, size_t byteCount)
	{
		memcpy(iram, ptr, byteCount);

		if (logDspDma)
		{
			Report(Channel::DSP, "MMEM -> IRAM transfer %d bytes.\n", byteCount);
		}
	}

}


// DSP Mailbox processing

namespace DSP
{
	// CPU->DSP Mailbox

	// Write by processor only.

	void Dsp16::CpuToDspWriteHi(uint16_t value)
	{
		CpuToDspLock[0].Lock();

		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "CpuToDspWriteHi: 0x%04X\n", value);
		}

		CpuToDspMailbox[0] = value & 0x7FFF;
		CpuToDspLock[0].Unlock();
	}

	void Dsp16::CpuToDspWriteLo(uint16_t value)
	{
		CpuToDspLock[1].Lock();

		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "CpuToDspWriteLo: 0x%04X\n", value);
		}

		CpuToDspMailbox[1] = value;
		CpuToDspMailbox[0] |= 0x8000;

		if (logMailbox)
		{
			Report(Channel::DSP, "CPU Write Message: 0x%04X_%04X\n", CpuToDspMailbox[0], CpuToDspMailbox[1]);
		}
		CpuToDspLock[1].Unlock();
	}

	uint16_t Dsp16::CpuToDspReadHi(bool ReadByDsp)
	{
		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "CpuToDspReadHi\n");
		}

		CpuToDspLock[0].Lock();
		uint16_t value = CpuToDspMailbox[0];
		CpuToDspLock[0].Unlock();
		return value;
	}

	uint16_t Dsp16::CpuToDspReadLo(bool ReadByDsp)
	{
		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "CpuToDspReadLo\n");
		}

		CpuToDspLock[1].Lock();
		uint16_t value = CpuToDspMailbox[1];
		if (ReadByDsp)
		{
			if (logMailbox)
			{
				Report(Channel::DSP, "DSP Read Message: 0x%04X_%04X\n", CpuToDspMailbox[0], CpuToDspMailbox[1]);
			}
			CpuToDspMailbox[0] &= ~0x8000;				// When DSP read
		}
		CpuToDspLock[1].Unlock();
		return value;
	}

	// DSP->CPU Mailbox

	// Write by DSP only.

	void Dsp16::DspToCpuWriteHi(uint16_t value)
	{
		DspToCpuLock[0].Lock();

		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "DspToCpuWriteHi: 0x%04X\n", value);
		}

		DspToCpuMailbox[0] = value & 0x7FFF;
		DspToCpuLock[0].Unlock();
	}

	void Dsp16::DspToCpuWriteLo(uint16_t value)
	{
		DspToCpuLock[1].Lock();

		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "DspToCpuWriteLo: 0x%04X\n", value);
		}

		DspToCpuMailbox[1] = value;
		DspToCpuMailbox[0] |= 0x8000;

		if (logMailbox)
		{
			Report(Channel::DSP, "DSP Write Message: 0x%04X_%04X\n", DspToCpuMailbox[0], DspToCpuMailbox[1]);
		}

		DspToCpuLock[1].Unlock();
	}

	uint16_t Dsp16::DspToCpuReadHi(bool ReadByDsp)
	{
		DspToCpuLock[0].Lock();
		uint16_t value = DspToCpuMailbox[0];
		DspToCpuLock[0].Unlock();
		return value;
	}

	uint16_t Dsp16::DspToCpuReadLo(bool ReadByDsp)
	{
		DspToCpuLock[1].Lock();
		uint16_t value = DspToCpuMailbox[1];
		if (!ReadByDsp)
		{
			if (logMailbox)
			{
				Report(Channel::DSP, "CPU Read Message: 0x%04X_%04X\n", DspToCpuMailbox[0], DspToCpuMailbox[1]);
			}
			DspToCpuMailbox[0] &= ~0x8000;					// When CPU read
		}
		DspToCpuLock[1].Unlock();
		return value;
	}
}



// TODO: We used to think that AR is a separate Flipper module, but actually the SDRAM(ARAM) controller is in the DSP.
// It includes an SDRAM timing and access controller, as well as an "Accelerator" that also does ADPCM decompression.
// Part of the DSPCore registers are mapped to the PI-DSP interface (DMA, interrupts) and the other part to the SDRAM controller
// We need to make this component more beautiful somehow (combine the old code of AR.cpp and Accelerator into a single entity).


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

ARControl aram;

static void ARINT()
{
	Flipper::ai.dcr |= AIDCR_ARINT;
	if (Flipper::ai.dcr & AIDCR_ARINTMSK)
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
	if (type == RAM_TO_ARAM)
	{
		memcpy(&ARAM[aram.araddr], &mi.ram[aram.mmaddr], 32);
	}
	else
	{
		memcpy(&mi.ram[aram.mmaddr], &ARAM[aram.araddr], 32);
	}

	aram.araddr += 32;
	aram.mmaddr += 32;
	cnt -= 32;
	aram.cnt = cnt | (type << 31);

	if ((aram.cnt & ~0x8000'0000) == 0)
	{
		Flipper::ai.dcr &= ~AIDCR_ARDMA;
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

		Flipper::DSP->SpecialAramImemDma(&mi.ram[aram.mmaddr], cnt);

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
			memset(&mi.ram[aram.mmaddr], 0, cnt);

			aram.cnt &= 0x80000000;     // clear dma counter
			ARINT();                    // invoke aram TC interrupt
		}
		return;
	}

	// For other cases - delegate job to thread

	if (aram.dmaThread->IsRunning()) {
		Halt("There is some nonsense going on: the ARAM DMA Thread needs to be started while it is still running.\n");
	}

	Flipper::ai.dcr |= AIDCR_ARDMA;
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

static void ar_write_maddr_h(uint32_t addr, uint32_t data)
{
	aram.mmaddr &= 0x0000ffff;
	aram.mmaddr |= ((data & 0x3ff) << 16);
}
static void ar_read_maddr_h(uint32_t addr, uint32_t* reg) { *reg = (aram.mmaddr >> 16) & 0x3FF; }

static void ar_write_maddr_l(uint32_t addr, uint32_t data)
{
	aram.mmaddr &= 0xffff0000;
	aram.mmaddr |= ((data & ~0x1F) & 0xffff);
}
static void ar_read_maddr_l(uint32_t addr, uint32_t* reg) { *reg = (uint16_t)aram.mmaddr & ~0x1F; }

// ARAM pointer

static void ar_write_araddr_h(uint32_t addr, uint32_t data)
{
	aram.araddr &= 0x0000ffff;
	aram.araddr |= ((data & 0x3FF) << 16);
}
static void ar_read_araddr_h(uint32_t addr, uint32_t* reg) { *reg = (aram.araddr >> 16) & 0x3FF; }

static void ar_write_araddr_l(uint32_t addr, uint32_t data)
{
	aram.araddr &= 0xffff0000;
	aram.araddr |= ((data & ~0x1F) & 0xffff);
}
static void ar_read_araddr_l(uint32_t addr, uint32_t* reg) { *reg = (uint16_t)aram.araddr & ~0x1F; }

//
// byte count register
//

static void ar_write_cnt_h(uint32_t addr, uint32_t data)
{
	aram.cnt &= 0x0000ffff;
	aram.cnt |= ((data & 0x83FF) << 16);
}
static void ar_read_cnt_h(uint32_t addr, uint32_t* reg) { *reg = (aram.cnt >> 16) & 0x83FF; }

static void ar_write_cnt_l(uint32_t addr, uint32_t data)
{
	aram.cnt &= 0xffff0000;
	aram.cnt |= ((data & ~0x1F) & 0xffff);
	ARDMA();
}
static void ar_read_cnt_l(uint32_t addr, uint32_t* reg) { *reg = (uint16_t)aram.cnt & ~0x1F; }

//
// hacks
//

static void no_read(uint32_t addr, uint32_t* reg) { *reg = 0; }
static void no_write(uint32_t addr, uint32_t data) {}

static void ar_hack_size_r(uint32_t addr, uint32_t* reg) { *reg = aram.size; }
static void ar_hack_size_w(uint32_t addr, uint32_t data) { aram.size = (uint16_t)data; }
static void ar_hack_mode(uint32_t addr, uint32_t* reg) { *reg = 1; }

// ---------------------------------------------------------------------------
// 32-bit ARAM registers

static void ar_write_maddr(uint32_t addr, uint32_t data) { aram.mmaddr = data & 0x03FF'FFE0; }
static void ar_read_maddr(uint32_t addr, uint32_t* reg) { *reg = aram.mmaddr; }

static void ar_write_araddr(uint32_t addr, uint32_t data) { aram.araddr = data & 0x03FF'FFE0; }
static void ar_read_araddr(uint32_t addr, uint32_t* reg) { *reg = aram.araddr; }

static void ar_write_cnt(uint32_t addr, uint32_t data)
{
	aram.cnt = data & 0x83FF'FFE0;
	ARDMA();
}
static void ar_read_cnt(uint32_t addr, uint32_t* reg) { *reg = aram.cnt & 0x83FF'FFE0; }

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
	PISetTrap(16, AR_DMA_MMADDR_H, ar_read_maddr_h, ar_write_maddr_h);
	PISetTrap(16, AR_DMA_MMADDR_L, ar_read_maddr_l, ar_write_maddr_l);
	PISetTrap(16, AR_DMA_ARADDR_H, ar_read_araddr_h, ar_write_araddr_h);
	PISetTrap(16, AR_DMA_ARADDR_L, ar_read_araddr_l, ar_write_araddr_l);
	PISetTrap(16, AR_DMA_CNT_H, ar_read_cnt_h, ar_write_cnt_h);
	PISetTrap(16, AR_DMA_CNT_L, ar_read_cnt_l, ar_write_cnt_l);

	PISetTrap(32, AR_DMA_MMADDR, ar_read_maddr, ar_write_maddr);
	PISetTrap(32, AR_DMA_ARADDR, ar_read_araddr, ar_write_araddr);
	PISetTrap(32, AR_DMA_CNT, ar_read_cnt, ar_write_cnt);

	// hacks
	PISetTrap(16, AR_SIZE, ar_hack_size_r, ar_hack_size_w);
	PISetTrap(16, AR_MODE, ar_hack_mode, no_write);
	PISetTrap(16, AR_REFRESH, no_read, no_write);

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
