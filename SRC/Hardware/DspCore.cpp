// Low-level DSP core

#include "pch.h"

namespace DSP
{

	DspCore::DspCore(HWConfig* config)
	{
		threadHandle = CreateThread(NULL, 0, DspThreadProc, this, CREATE_SUSPENDED, &threadId);
		assert(threadHandle != INVALID_HANDLE_VALUE);

		HardReset();

		interp = new DspInterpreter(this);
		assert(interp);

		// Load IROM

		uint32_t iromImageSize = 0;
		uint8_t* iromImage = (uint8_t *)FileLoad(config->DspIromFilename, &iromImageSize);

		if (!iromImage || iromImageSize != IROM_SIZE)
		{
			DBReport("Failed to load DSP IROM: %s\n", config->DspIromFilename);
		}
		else
		{
			DBReport2(DbgChannel::DSP, "Loaded DSP IROM: %s\n", config->DspIromFilename);
			memcpy(irom, iromImage, IROM_SIZE);
			free(iromImage);
		}

		// Load DROM

		uint32_t dromImageSize = 0;
		uint8_t* dromImage = (uint8_t*)FileLoad(config->DspDromFilename, &dromImageSize);

		if (!dromImage || dromImageSize != DROM_SIZE)
		{
			DBReport("Failed to load DSP DROM: %s\n", config->DspDromFilename);
		}
		else
		{
			DBReport2(DbgChannel::DSP, "Loaded DSP DROM: %s\n", config->DspDromFilename);
			memcpy(drom, dromImage, DROM_SIZE);
			free(dromImage);
		}

		DBReport2(DbgChannel::DSP, "DSPCore: Ready\n");
	}

	DspCore::~DspCore()
	{
		TerminateThread(threadHandle, 0);
		WaitForSingleObject(threadHandle, 1000);

		delete interp;
	}

	DWORD WINAPI DspCore::DspThreadProc(LPVOID lpParameter)
	{
		DspCore* core = (DspCore*)lpParameter;

		while (true)
		{
			// Do DSP actions
			core->Update();
		}

		return 0;
	}

	void DspCore::Exception(DspException id)
	{
		regs.st[0].push_back(regs.pc);
		regs.st[1].push_back((DspAddress)regs.sr.bits);
		regs.pc = (DspAddress)id * 2;
	}

	void DspCore::ReturnFromException()
	{
		regs.sr.bits = (uint16_t)regs.st[1].back();
		regs.st[1].pop_back();
		regs.pc = regs.st[0].back();
		regs.st[0].pop_back();
	}

	void DspCore::HardReset()
	{
		DBReport2(DbgChannel::DSP, "DspCore::Reset");

		savedGekkoTicks = cpu.tb.uval;

		for (int i = 0; i < _countof(regs.st); i++)
		{
			regs.st[i].clear();
			regs.st[i].reserve(32);		// Should be enough
		}

		for (int i = 0; i < 4; i++)
		{
			regs.ar[i] = 0;
			regs.ix[i] = 0;
			regs.gpr[i] = 0;
		}

		for (int i = 0; i < 2; i++)
		{ 
			regs.ac[i].bits = 0;
			regs.ax[i].bits = 0;
		}

		regs.prod.bitsUnpacked = 0;
		regs.bank = 0xFF;
		regs.sr.bits = 0;

		regs.pc = IROM_START_ADDRESS;		// IROM start

		ResetIfx();
	}

	void DspCore::Run()
	{
		if (!running)
		{
			ResumeThread(threadHandle);
			DBReport2(DbgChannel::DSP, "DspCore::Run");
			savedGekkoTicks = cpu.tb.uval;
			running = true;
		}
	}

	void DspCore::Suspend()
	{
		if (running)
		{
			running = false;
			DBReport2(DbgChannel::DSP, "DspCore::Suspend");
			SuspendThread(threadHandle);
		}
	}

	void DspCore::Update()
	{
		uint64_t ticks = cpu.tb.uval;

		if (ticks >= (savedGekkoTicks + GekkoTicksPerDspInstruction))
		{
			interp->ExecuteInstr();
			savedGekkoTicks = ticks;
		}
	}

	#pragma region "Debug"

	void DspCore::AddBreakpoint(DspAddress imemAddress)
	{
		MySpinLock::Lock(&breakPointsSpinLock);
		breakpoints.push_back(imemAddress);
		MySpinLock::Unlock(&breakPointsSpinLock);
	}

	void DspCore::ListBreakpoints()
	{
		MySpinLock::Lock(&breakPointsSpinLock);
		for (auto it = breakpoints.begin(); it != breakpoints.end(); ++it)
		{
			DBReport("0x%04X\n", *it);
		}
		MySpinLock::Unlock(&breakPointsSpinLock);
	}

	void DspCore::ClearBreakpoints()
	{
		MySpinLock::Lock(&breakPointsSpinLock);
		breakpoints.clear();
		MySpinLock::Unlock(&breakPointsSpinLock);
	}

	bool DspCore::TestBreakpoint(DspAddress imemAddress)
	{
		bool found = false;

		MySpinLock::Lock(&breakPointsSpinLock);
		for (auto it = breakpoints.begin(); it != breakpoints.end(); ++it)
		{
			if (*it == imemAddress)
			{
				found = true;
				break;
			}
		}
		MySpinLock::Unlock(&breakPointsSpinLock);

		return found;
	}

	void DspCore::AddCanary(DspAddress imemAddress, std::string text)
	{
		MySpinLock::Lock(&canariesSpinLock);
		canaries[imemAddress] = text;
		MySpinLock::Unlock(&canariesSpinLock);
	}

	void DspCore::ListCanaries()
	{
		MySpinLock::Lock(&canariesSpinLock);
		for (auto it = canaries.begin(); it != canaries.end(); ++it)
		{
			DBReport("0x%04X: %s\n", it->first, it->second.c_str());
		}
		MySpinLock::Unlock(&canariesSpinLock);
	}

	void DspCore::ClearCanaries()
	{
		MySpinLock::Lock(&canariesSpinLock);
		canaries.clear();
		MySpinLock::Unlock(&canariesSpinLock);
	}

	bool DspCore::TestCanary(DspAddress imemAddress)
	{
		MySpinLock::Lock(&canariesSpinLock);

		auto it = canaries.find(imemAddress);
		if (it != canaries.end())
		{
			DBReport2(DbgChannel::DSP, it->second.c_str());
			MySpinLock::Unlock(&canariesSpinLock);
			return true;
		}

		MySpinLock::Unlock(&canariesSpinLock);
		return false;
	}

	/// Execute single instruction (by interpreter)
	void DspCore::Step()
	{
		if (IsRunning())
		{
			DBReport2(DbgChannel::DSP, "It is impossible while running DSP thread.\n");
			return;
		}

		interp->ExecuteInstr();
	}

	/// Print only registers different from previous ones
	void DspCore::DumpRegs(DspRegs* prevState)
	{
		if (regs.pc != prevState->pc)
		{
			DBReport("pc: 0x%04X\n", regs.pc);
		}

		if (regs.prod.bitsUnpacked != prevState->prod.bitsUnpacked)
		{
			DBReport("prod: 0x%04X_%04X_%04X_%04X\n", 
				regs.prod.h, regs.prod.m2, regs.prod.m1, regs.prod.l );
		}

		if (regs.bank != prevState->bank)
		{
			DBReport("bank: 0x%04X\n", regs.bank);
		}

		if (regs.sr.bits != prevState->sr.bits)
		{
			// TODO: Add bit description
			DBReport("sr: 0x%04X\n", regs.sr);
		}

		for (int i = 0; i < 2; i++)
		{
			if (regs.ac[i].bits != prevState->ac[i].bits)
			{
				DBReport("ac%i: 0x%04X_%04X_%04X\n", i, 
					regs.ac[i].h, regs.ac[i].m, regs.ac[i].l);
			}
		}

		for (int i = 0; i < 2; i++)
		{
			if (regs.ax[i].bits != prevState->ax[i].bits)
			{
				DBReport("ax%i: 0x%04X_%04X\n", i, 
					regs.ax[i].h, regs.ax[i].l);
			}
		}

		for (int i = 0; i < 4; i++)
		{
			if (regs.ar[i] != prevState->ar[i])
			{
				DBReport("ar%i: 0x%04X\n", i, regs.ar[i]);
			}
		}

		for (int i = 0; i < 4; i++)
		{
			if (regs.ix[i] != prevState->ix[i])
			{
				DBReport("ix%i: 0x%04X\n", i, regs.ix[i]);
			}
		}

		for (int i = 0; i < 4; i++)
		{
			if (regs.gpr[i] != prevState->gpr[i])
			{
				DBReport("r%i: 0x%04X\n", 8+i, regs.gpr[i]);
			}
		}
	}

	/// Dump IFX State
	void DspCore::DumpIfx()
	{
		DBReport("Cpu2Dsp Mailbox: Shadow Hi: 0x%04X, Real Hi: 0x%04X, Real Lo: 0x%04X\n",
			CpuToDspMailboxShadow[0], (uint16_t)CpuToDspMailbox[0], (uint16_t)CpuToDspMailbox[1]);
		DBReport("Dsp2Cpu Mailbox: Shadow Hi: 0x%04X, Real Hi: 0x%04X, Real Lo: 0x%04X\n",
			DspToCpuMailboxShadow[0], (uint16_t)DspToCpuMailbox[0], (uint16_t)DspToCpuMailbox[1]);
		DBReport("Dma: MmemAddr: 0x%08X, DspAddr: 0x%04X, Size: 0x%04X, Ctrl: %i\n",
			DmaRegs.mmemAddr.bits, DmaRegs.dspAddr, DmaRegs.blockSize, DmaRegs.control.bits);
	}

	#pragma endregion "Debug"


	#pragma region "Register access"

	void DspCore::MoveToReg(int reg, uint16_t val)
	{
		switch (reg)
		{
			case (int)DspRegister::ar0:
			case (int)DspRegister::ar1:
			case (int)DspRegister::ar2:
			case (int)DspRegister::ar3:
				regs.ar[reg] = val;
				break;
			case (int)DspRegister::ix0:
			case (int)DspRegister::ix1:
			case (int)DspRegister::ix2:
			case (int)DspRegister::ix3:
				regs.ix[reg - (int)DspRegister::indexRegs] = val;
				break;
			case (int)DspRegister::r8:
			case (int)DspRegister::r9:
			case (int)DspRegister::r10:
			case (int)DspRegister::r11:
				regs.gpr[reg - (int)DspRegister::gprs] = val;
				break;
			case (int)DspRegister::st0:
			case (int)DspRegister::st1:
			case (int)DspRegister::st2:
			case (int)DspRegister::st3:
				regs.st[reg - (int)DspRegister::stackRegs].push_back((DspAddress)val);
				break;
			case (int)DspRegister::ac0h:
				regs.ac[0].h = val;
				break;
			case (int)DspRegister::ac1h:
				regs.ac[1].h = val;
				break;
			case (int)DspRegister::bank:
				regs.bank = val;
				break;
			case (int)DspRegister::sr:
				regs.sr.bits = val;
				break;
			case (int)DspRegister::prodl:
				regs.prod.l = val;
				break;
			case (int)DspRegister::prodm1:
				regs.prod.m1 = val;
				break;
			case (int)DspRegister::prodh:
				regs.prod.h = val;
				break;
			case (int)DspRegister::prodm2:
				regs.prod.m2 = val;
				break;
			case (int)DspRegister::ax0l:
				regs.ax[0].l = val;
				break;
			case (int)DspRegister::ax0h:
				regs.ax[0].h = val;
				break;
			case (int)DspRegister::ax1l:
				regs.ax[1].l = val;
				break;
			case (int)DspRegister::ax1h:
				regs.ax[1].h = val;
				break;
			case (int)DspRegister::ac0l:
				regs.ac[0].l = val;
				break;
			case (int)DspRegister::ac1l:
				regs.ac[1].l = val;
				break;
			case (int)DspRegister::ac0m:
				regs.ac[0].m = val;
				if (regs.sr.sxm)
				{
					regs.ac[0].h = (val & 0x8000) ? 0xFFFF : 0;
				}
				break;
			case (int)DspRegister::ac1m:
				regs.ac[1].m = val;
				if (regs.sr.sxm)
				{
					regs.ac[1].h = (val & 0x8000) ? 0xFFFF : 0;
				}
				break;
		}
	}

	uint16_t DspCore::MoveFromReg(int reg)
	{
		switch (reg)
		{
			case (int)DspRegister::ar0:
			case (int)DspRegister::ar1:
			case (int)DspRegister::ar2:
			case (int)DspRegister::ar3:
				return regs.ar[reg];
			case (int)DspRegister::ix0:
			case (int)DspRegister::ix1:
			case (int)DspRegister::ix2:
			case (int)DspRegister::ix3:
				return regs.ix[reg - (int)DspRegister::indexRegs];
			case (int)DspRegister::r8:
			case (int)DspRegister::r9:
			case (int)DspRegister::r10:
			case (int)DspRegister::r11:
				return regs.gpr[reg - (int)DspRegister::gprs];
			case (int)DspRegister::st0:
			case (int)DspRegister::st1:
			case (int)DspRegister::st2:
			case (int)DspRegister::st3:
				return (uint16_t)regs.st[reg - (int)DspRegister::stackRegs].back();
			case (int)DspRegister::ac0h:
				return regs.ac[0].h;
			case (int)DspRegister::ac1h:
				return regs.ac[1].h;
			case (int)DspRegister::bank:
				return regs.bank;
			case (int)DspRegister::sr:
				return regs.sr.bits;
			case (int)DspRegister::prodl:
				return regs.prod.l;
			case (int)DspRegister::prodm1:
				return regs.prod.m1;
			case (int)DspRegister::prodh:
				return regs.prod.h;
			case (int)DspRegister::prodm2:
				return regs.prod.m2;
			case (int)DspRegister::ax0l:
				return regs.ax[0].l;
			case (int)DspRegister::ax0h:
				return regs.ax[0].h;
			case (int)DspRegister::ax1l:
				return regs.ax[1].l;
			case (int)DspRegister::ax1h:
				return regs.ax[1].h;
			case (int)DspRegister::ac0l:
				return regs.ac[0].l;
			case (int)DspRegister::ac1l:
				return regs.ac[1].l;
			case (int)DspRegister::ac0m:
				return regs.ac[0].m;
			case (int)DspRegister::ac1m:
				return regs.ac[1].m;
		}
		return 0;
	}

	int64_t DspCore::PackProd()
	{
		int64_t hi = (int64_t)regs.prod.h << 32;
		int64_t mid = ((int64_t)regs.prod.m1 + (int64_t)regs.prod.m2) << 16;
		int64_t low = regs.prod.l;
		int64_t res = hi + mid + low;
		// Sign extend 40
		if (res & 0x8000000000)
		{
			res |= 0xffffff0000000000;
		}
		return res;
	}

	void DspCore::PackProd(int64_t val)
	{
		DspLongAccumulator acc;
		acc.sbits = val;
		regs.prod.h = acc.h;
		regs.prod.m2 = 0;
		regs.prod.m1 = acc.m;
		regs.prod.l = acc.l;
	}

	#pragma endregion "Register access"


	#pragma region "Memory Engine"

	uint8_t* DspCore::TranslateIMem(DspAddress addr)
	{
		if (addr < IRAM_SIZE)
		{
			return &iram[addr << 1];
		}
		else if (addr >= IROM_START_ADDRESS && addr < (IROM_START_ADDRESS + IROM_SIZE))
		{
			return &irom[(addr - IROM_START_ADDRESS) << 1];
		}
		else
		{
			return nullptr;
		}
	}

	uint8_t* DspCore::TranslateDMem(DspAddress addr)
	{
		if (addr < DRAM_SIZE)
		{
			return &dram[addr << 1];
		}
		else if (addr >= DROM_START_ADDRESS && addr < (DROM_START_ADDRESS + DROM_SIZE))
		{
			return &drom[(addr - DROM_START_ADDRESS) << 1];
		}
		else
		{
			return nullptr;
		}
	}

	uint16_t DspCore::ReadIMem(DspAddress addr)
	{
		uint8_t* ptr = TranslateIMem(addr);

		if (ptr)
		{
			return _byteswap_ushort(*(uint16_t*)ptr);
		}

		return 0;
	}

	uint16_t DspCore::ReadDMem(DspAddress addr)
	{
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
					return CpuToDspReadLo();
				case (DspAddress)DspHardwareRegs::DMBH:
					return DspToCpuReadHi(true);
				case (DspAddress)DspHardwareRegs::DMBL:
					return DspToCpuReadLo();

				case (DspAddress)DspHardwareRegs::DIRQ:
					return 0;

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
					DBHalt("DSP Unknown HW read 0x%04X\n", addr);
					Suspend();
					break;
			}
			return 0;
		}

		uint8_t* ptr = TranslateDMem(addr);

		if (ptr)
		{
			return _byteswap_ushort(*(uint16_t*)ptr);
		}

		DBHalt("DSP Unmapped DMEM read 0x%04X\n", addr);
		Suspend();
		return 0;
	}

	void DspCore::WriteDMem(DspAddress addr, uint16_t value)
	{
		if (addr >= IFX_START_ADDRESS)
		{
			switch (addr)
			{
				case (DspAddress)DspHardwareRegs::DSMAH:
					DmaRegs.mmemAddr.h = value & 0x03ff;
					break;
				case (DspAddress)DspHardwareRegs::DSMAL:
					DmaRegs.mmemAddr.l = value & ~3;
					break;
				case (DspAddress)DspHardwareRegs::DSPA:
					DmaRegs.dspAddr = value & ~1;
					break;
				case (DspAddress)DspHardwareRegs::DSCR:
					DmaRegs.control.bits = value & 3;
					break;
				case (DspAddress)DspHardwareRegs::DSBL:
					DmaRegs.blockSize = value & ~3;
					DoDma();
					break;

				case (DspAddress)DspHardwareRegs::CMBH:
					DBHalt("DSP is not allowed to write processor Mailbox!");
					Suspend();
					break;
				case (DspAddress)DspHardwareRegs::CMBL:
					DBHalt("DSP is not allowed to write processor Mailbox!");
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
						DBReport2(DbgChannel::DSP, "DspHardwareRegs::DIRQ\n");
						DSPAssertInt();
					}
					break;

				case (DspAddress)DspHardwareRegs::ACSAH:
					Accel.StartAddress.h = value;
					break;
				case (DspAddress)DspHardwareRegs::ACSAL:
					Accel.StartAddress.l = value;
					break;
				case (DspAddress)DspHardwareRegs::ACEAH:
					Accel.EndAddress.h = value;
					break;
				case (DspAddress)DspHardwareRegs::ACEAL:
					Accel.EndAddress.l = value;
					break;
				case (DspAddress)DspHardwareRegs::ACCAH:
					Accel.CurrAddress.h = value;
					break;
				case (DspAddress)DspHardwareRegs::ACCAL:
					Accel.CurrAddress.l = value;
					break;

				case (DspAddress)DspHardwareRegs::ACFMT:
					Accel.Fmt = value;
					break;
				case (DspAddress)DspHardwareRegs::ACPDS:
					Accel.AdpcmPds = value;
					break;
				case (DspAddress)DspHardwareRegs::ACYN1:
					Accel.AdpcmYn1 = value;
					break;
				case (DspAddress)DspHardwareRegs::ACYN2:
					Accel.AdpcmYn2 = value;
					break;
				case (DspAddress)DspHardwareRegs::ACGAN:
					Accel.AdpcmGan = value;
					break;

				case (DspAddress)DspHardwareRegs::ACDAT2:
					AccelWriteData(value);
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
					DBReport2(DbgChannel::DSP, "Known unknown HW write 0x%04X = 0x%04X\n", addr, value);
					break;

				default:
					DBHalt("DSP Unknown HW write 0x%04X = 0x%04X\n", addr, value);
					Suspend();
					break;
			}
			return;
		}

		if (addr < DRAM_SIZE)
		{
			uint8_t* ptr = TranslateDMem(addr);

			if (ptr)
			{
				*(uint16_t*)ptr = _byteswap_ushort(value);
				return;
			}
		}

		DBHalt("DSP Unmapped DMEM write 0x%04X = 0x%04X\n", addr, value);
		Suspend();
	}

	#pragma endregion "Memory Engine"


	#pragma region "Flipper interface"

	void DspCore::DSPSetResetBit(bool val)
	{
		if (val)
		{
			DBReport2(DbgChannel::DSP, "Reset\n");
			HardReset();
		}
	}

	bool DspCore::DSPGetResetBit()
	{
		return false;
	}

	void DspCore::DSPSetIntBit(bool val)
	{
		if (val)
		{
			DBReport2(DbgChannel::DSP, "Interrupt\n");
			Exception(DspException::INT);
		}
	}

	bool DspCore::DSPGetIntBit()
	{
		// No meaning?
		return false;
	}

	void DspCore::DSPSetHaltBit(bool val)
	{
		val ? Suspend() : Run();
	}

	bool DspCore::DSPGetHaltBit()
	{
		return !running;
	}

	// CPU->DSP Mailbox

	// Write by processor only.

	void DspCore::CpuToDspWriteHi(uint16_t value)
	{
		DBReport2(DbgChannel::DSP, "DspCore::CpuToDspWriteHi: 0x%04X (Shadowed)\n", value);
		CpuToDspMailboxShadow[0] = value;
	}

	void DspCore::CpuToDspWriteLo(uint16_t value)
	{
		DBReport2(DbgChannel::DSP, "DspCore::CpuToDspWriteLo: 0x%04X\n", value);
		CpuToDspMailbox[1] = value;
		CpuToDspMailbox[0] = CpuToDspMailboxShadow[0] | 0x8000;
	}

	uint16_t DspCore::CpuToDspReadHi(bool ReadByDsp)
	{
		uint16_t value = CpuToDspMailbox[0];

		// If DSP is running and is in a waiting cycle for a message from the CPU, 
		// we put it in the HALT state until the processor sends a message through the Mailbox.

		//if ((value & 0x8000) == 0 && IsRunning() && ReadByDsp)
		//{
		//	DBReport2(DbgChannel::DSP, "Wait CPU Mailbox\n");
		//	Suspend();
		//}

		return value;
	}

	uint16_t DspCore::CpuToDspReadLo()
	{
		uint16_t value = CpuToDspMailbox[1];
		CpuToDspMailbox[0] &= ~0x8000;				// When DSP read
		return value;
	}

	// DSP->CPU Mailbox

	// Write by DSP only.

	void DspCore::DspToCpuWriteHi(uint16_t value)
	{
		DBReport2(DbgChannel::DSP, "DspHardwareRegs::DMBH = 0x%04X (Shadowed)\n", value);
		DspToCpuMailboxShadow[0] = value;
	}

	void DspCore::DspToCpuWriteLo(uint16_t value)
	{
		DBReport2(DbgChannel::DSP, "DspHardwareRegs::DMBL = 0x%04X\n", value);
		DspToCpuMailbox[1] = value;
		DspToCpuMailbox[0] = DspToCpuMailboxShadow[0] | 0x8000;
	}

	uint16_t DspCore::DspToCpuReadHi(bool ReadByDsp)
	{
		return DspToCpuMailbox[0];
	}

	uint16_t DspCore::DspToCpuReadLo()
	{
		uint16_t value = DspToCpuMailbox[1];
		DspToCpuMailbox[0] &= ~0x8000;					// When CPU read
		return value;
	}

	#pragma endregion "Flipper interface"


	#pragma region "IFX"

	void DspCore::ResetIfx()
	{
		DspToCpuMailbox[0] = DspToCpuMailboxShadow[0] = 0;
		DspToCpuMailbox[1] = DspToCpuMailboxShadow[1] = 0;
		CpuToDspMailbox[0] = CpuToDspMailboxShadow[0] = 0;
		CpuToDspMailbox[1] = CpuToDspMailboxShadow[1] = 0;

		memset(&DmaRegs, 0, sizeof(DmaRegs));
		memset(&Accel, 0, sizeof(Accel));
	}

	/// Instant DMA
	void DspCore::DoDma()
	{
		uint8_t* ptr = nullptr;

		//DBReport2(DbgChannel::DSP, "DspCore::Dma: Mmem: 0x%08X, DspAddr: 0x%04X, Size: 0x%04X, Ctrl: %i\n",
		//	DmaRegs.mmemAddr.bits, DmaRegs.dspAddr, DmaRegs.blockSize, DmaRegs.control.bits);

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
			DBHalt("DspCore::DoDma: invalid dsp address: 0x%04X\n", DmaRegs.dspAddr);
			return;
		}

		if (DmaRegs.control.Dsp2Mmem)
		{
			memcpy(&mi.ram[DmaRegs.mmemAddr.bits], ptr, DmaRegs.blockSize);
		}
		else
		{
			memcpy(ptr, &mi.ram[DmaRegs.mmemAddr.bits], DmaRegs.blockSize);
		}

		// Dump ucode
#if 1
		if (DmaRegs.control.Imem && !DmaRegs.control.Dsp2Mmem)
		{
			char filename[0x100] = { 0, };
			sprintf_s(filename, sizeof(filename), "Data\\DspUcode_%04X_%s.bin", DmaRegs.blockSize, ldat.gameID);
			FileSave(filename, ptr, DmaRegs.blockSize);
		}
#endif
	}

	/// Read data by accelerator and optionally decode (raw=false)
	uint16_t DspCore::AccelReadData(bool raw)
	{

		// If the current sample address exceeds the loop-end address, the streaming cache will reset itself to the loop-start position and assert an interrupt.

		return 0;
	}

	// Write RAW data to ARAM
	void DspCore::AccelWriteData(uint16_t data)
	{

	}

	#pragma endregion "IFX"

}
