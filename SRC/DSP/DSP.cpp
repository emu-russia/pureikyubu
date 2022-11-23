
#include "pch.h"

using namespace Debug;

namespace DSP
{

	Dsp16::Dsp16()
	{
		dspThread = new Thread(DspThreadProc, true, this, "DspCore");

		core = new DspCore(this);

		JDI::Hub.AddNode(DSP_JDI_JSON, dsp_init_handlers);
	}

	Dsp16::~Dsp16()
	{
		delete dspThread;
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
		_TB(Dsp16::Run);
		if (!dspThread->IsRunning())
		{
			dspThread->Resume();
			if (logDspControlBits)
			{
				Report(Channel::DSP, "Run\n");
			}
			savedGekkoTicks = Core->GetTicks();
		}
		_TE();
	}

	void Dsp16::Suspend()
	{
		_TB(Dsp16::Suspend);
		if (dspThread->IsRunning())
		{
			if (logDspControlBits)
			{
				Report(Channel::DSP, "Suspend\n");
			}
			dspThread->Suspend();
		}
		_TE();
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
					Halt("DSP is not allowed to write processor Mailbox!");
					Suspend();
					break;
				case (DspAddress)DspHardwareRegs::CMBL:
					Halt("DSP is not allowed to write processor Mailbox!");
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
