// Low-level DSP core

#include "pch.h"

namespace DSP
{

	DspCore::DspCore(HWConfig* config)
	{
		threadHandle = CreateThread(NULL, 0, DspThreadProc, this, CREATE_SUSPENDED, &threadId);
		assert(threadHandle != INVALID_HANDLE_VALUE);

		Reset();

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
			DBReport(YEL "Loaded DSP IROM: %s\n", config->DspIromFilename);
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
			DBReport(YEL "Loaded DSP DROM: %s\n", config->DspDromFilename);
			memcpy(drom, dromImage, DROM_SIZE);
			free(dromImage);
		}

		DBReport(CYAN "DSPCore: Ready\n");
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

			Sleep(1);
		}

		return 0;
	}

	void DspCore::Reset()
	{
		DBReport(_DSP "DspCore::Reset");

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

		regs.prod = 0;
		regs.cr = 0;
		regs.sr.bits = 0;

		regs.pc = IROM_START_ADDRESS;			// IROM start

		ResetIfx();
	}

	void DspCore::Run()
	{
		if (!running)
		{
			ResumeThread(threadHandle);
			DBReport(_DSP "DspCore::Run");
			savedGekkoTicks = cpu.tb.uval;
			running = true;
		}
	}

	void DspCore::Suspend()
	{
		if (running)
		{
			running = false;
			DBReport(_DSP "DspCore::Suspend");
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

	/// Execute single instruction (by interpreter)
	void DspCore::Step()
	{
		if (IsRunning())
		{
			DBReport(_DSP "It is impossible while running DSP thread.\n");
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

		if (regs.prod != prevState->prod)
		{
			DBReport("prod: 0x%llX\n", regs.prod);
		}

		if (regs.cr != prevState->cr)
		{
			DBReport("cr: 0x%04X\n", regs.cr);
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
				DBReport("ac%i: 0x%llX\n", i, regs.ac[i]);
			}
		}

		for (int i = 0; i < 2; i++)
		{
			if (regs.ax[i].bits != prevState->ax[i].bits)
			{
				DBReport("ax%i: 0x%llX\n", i, regs.ax[i]);
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
			case (int)DspRegister::config:
				regs.cr = val;
				break;
			case (int)DspRegister::sr:
				regs.sr.bits = val;
				break;
			case (int)DspRegister::prodl:
				// Read only
				break;
			case (int)DspRegister::prodm1:
				// Read only
				break;
			case (int)DspRegister::prodh:
				// Read only
				break;
			case (int)DspRegister::prodm2:
				// Read only
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
				break;
			case (int)DspRegister::ac1m:
				regs.ac[1].m = val;
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
			case (int)DspRegister::config:
				return regs.cr;
			case (int)DspRegister::sr:
				return regs.sr.bits;
			case (int)DspRegister::prodl:
				// TODO: Prod
				return 0;
			case (int)DspRegister::prodm1:
				// TODO: Prod
				return 0;
			case (int)DspRegister::prodh:
				// TODO: Prod
				return 0;
			case (int)DspRegister::prodm2:
				// TODO: Prod
				return 0;
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
			return MEMSwapHalf(*(uint16_t*)ptr);
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
					return CpuToDspReadHi();
				case (DspAddress)DspHardwareRegs::CMBL:
					return CpuToDspReadLo();
				case (DspAddress)DspHardwareRegs::DMBH:
					return DspToCpuReadHi();
				case (DspAddress)DspHardwareRegs::DMBL:
					return DspToCpuReadLo();
				default:
					DBHalt(_DSP "Unknown HW read 0x%04X\n", addr);
					break;
			}
			return 0;
		}

		uint8_t* ptr = TranslateDMem(addr);

		if (ptr)
		{
			return MEMSwapHalf(*(uint16_t*)ptr);
		}

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
					DolwinError(__FILE__, "DSP is not allowed to write processor Mailbox!");
					break;
				case (DspAddress)DspHardwareRegs::CMBL:
					DolwinError(__FILE__, "DSP is not allowed to write processor Mailbox!");
					break;
				case (DspAddress)DspHardwareRegs::DMBH:
					DspToCpuWriteHi(value);
					break;
				case (DspAddress)DspHardwareRegs::DMBL:
					DspToCpuWriteLo(value);
					break;
				default:
					DBHalt(_DSP "Unknown HW write 0x%04X = 0x%04X\n", addr, value);
					break;
			}
			return;
		}

		if (addr < DRAM_SIZE)
		{
			uint8_t* ptr = TranslateDMem(addr);

			if (ptr)
			{
				*(uint16_t*)ptr = MEMSwapHalf(value);
			}
		}
	}

	#pragma endregion "Memory Engine"


	#pragma region "Flipper interface"

	void DspCore::DSPSetResetBit(bool val)
	{
		if (val)
		{
			Reset();
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
			DBHalt (_DSP BRED "DspCore::DSPSetIntBit (not implemented!)\n");
			Suspend();
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
		DBReport("DspCore::CpuToDspWriteHi: 0x%04X (Shadowed)\n", value);
		CpuToDspMailboxShadow[0] = value;
	}

	void DspCore::CpuToDspWriteLo(uint16_t value)
	{
		DBReport("DspCore::CpuToDspWriteLo: 0x%04X\n", value);
		CpuToDspMailbox[1] = value;
		CpuToDspMailbox[0] = CpuToDspMailboxShadow[0] | 0x8000;
	}

	uint16_t DspCore::CpuToDspReadHi()
	{
		return CpuToDspMailbox[0];
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
		DBReport("DspHardwareRegs::DMBH = 0x%04X (Shadowed)\n", value);
		DspToCpuMailboxShadow[0] = value;
	}

	void DspCore::DspToCpuWriteLo(uint16_t value)
	{
		DBReport("DspHardwareRegs::DMBL = 0x%04X\n", value);
		DspToCpuMailbox[1] = value;
		DspToCpuMailbox[0] = DspToCpuMailboxShadow[0] | 0x8000;
	}

	uint16_t DspCore::DspToCpuReadHi()
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
	}

	/// Instant DMA
	void DspCore::DoDma()
	{
		uint8_t* ptr = nullptr;

		DBReport("DspCore::Dma: Mmem: 0x%08X, DspAddr: 0x%04X, Size: 0x%04X, Ctrl: %i\n",
			DmaRegs.mmemAddr.bits, DmaRegs.dspAddr, DmaRegs.blockSize, DmaRegs.control.bits);

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

	#pragma endregion "IFX"

}
