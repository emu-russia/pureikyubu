// Low-level DSP core

#include "pch.h"

namespace DSP
{

	DspCore::DspCore(HWConfig* config)
	{
		threadHandle = CreateThread(NULL, 0, DspThreadProc, this, CREATE_SUSPENDED, &threadId);

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

		regs.pc = IROM_START_ADDRESS;			// IROM start

		DspToCpuMailbox[0] = DspToCpuMailbox[1] = 0;
		CpuToDspMailbox[0] = CpuToDspMailbox[1] = 0;
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
				case (DspAddress)DspHardwareRegs::CMBH:
					return DSPReadOutMailboxHi();
				case (DspAddress)DspHardwareRegs::CMBL:
					return DSPReadOutMailboxLo();
				case (DspAddress)DspHardwareRegs::DMBH:
					return DSPReadInMailboxHi();
				case (DspAddress)DspHardwareRegs::DMBL:
					return DSPReadInMailboxLo();
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
				case (DspAddress)DspHardwareRegs::DMBH:
					DSPWriteInMailboxHi(value);
					break;
				case (DspAddress)DspHardwareRegs::DMBL:
					DSPWriteInMailboxLo(value);
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
			DBReport(_DSP "DspCore::DSPSetIntBit\n");
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

	void DspCore::DSPWriteOutMailboxHi(uint16_t value)
	{
		DBReport("DspCore::DSPWriteOutMailboxHi: 0x%04X\n", value);

		CpuToDspMailbox[0] = value;

		if (value & 0x8000)
		{
			CpuToDspMailbox[0] &= ~0x8000;
		}
	}

	void DspCore::DSPWriteOutMailboxLo(uint16_t value)
	{
		DBReport("DspCore::DSPWriteOutMailboxLo: 0x%04X\n", value);

		CpuToDspMailbox[1] = value;

		CpuToDspMailbox[0] |= 0x8000;
	}

	uint16_t DspCore::DSPReadOutMailboxHi()
	{
		return CpuToDspMailbox[0];
	}

	uint16_t DspCore::DSPReadOutMailboxLo()
	{
		return CpuToDspMailbox[1];
	}

	// DSP->CPU Mailbox

	// Write by DSP only.

	void DspCore::DSPWriteInMailboxHi(uint16_t value)
	{
		DBReport(_DSP "DspHardwareRegs::DMBH = 0x%04X\n", value);
		DspToCpuMailbox[0] = value;

		if (value & 0x8000)
		{
			DspToCpuMailbox[0] &= ~0x8000;
		}
	}

	void DspCore::DSPWriteInMailboxLo(uint16_t value)
	{
		DBReport(_DSP "DspHardwareRegs::DMBL = 0x%04X\n", value);
		DspToCpuMailbox[1] = value;

		DspToCpuMailbox[0] |= 0x8000;
	}

	uint16_t DspCore::DSPReadInMailboxHi()
	{
		return DspToCpuMailbox[0];
	}

	uint16_t DspCore::DSPReadInMailboxLo()
	{
		return DspToCpuMailbox[1];
	}

	#pragma endregion "Flipper interface"

}
