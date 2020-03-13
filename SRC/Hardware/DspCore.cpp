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

		memset(&regs, 0, sizeof(regs));

		regs.pc = 0x8000;			// IROM start
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
			SuspendThread(threadHandle);
			DBReport(_DSP "DspCore::Suspend");
			running = false;
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

		if (regs.sr != prevState->sr)
		{
			// TODO: Add bit description
			DBReport("sr: 0x%04X\n", regs.sr);
		}

		for (int i = 0; i < 2; i++)
		{
			if (regs.ac[i] != prevState->ac[i])
			{
				DBReport("ac%i: 0x%llX\n", i, regs.ac[i]);
			}
		}

		for (int i = 0; i < 2; i++)
		{
			if (regs.ax[i] != prevState->ax[i])
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

		for (int i = 0; i < 4; i++)
		{
			if (regs.st[i] != prevState->st[i])
			{
				DBReport("st%i: 0x%04X\n", i, regs.st[i]);
			}
		}
	}

	#pragma endregion "Debug"


	#pragma region "Register access"

	void DspCore::MoveToReg(int reg, uint16_t val)
	{
		switch (reg)
		{
			case 0:
			case 1:
			case 2:
			case 3:
				regs.ar[reg] = val;
				break;
			case 4:
			case 5:
			case 6:
			case 7:
				regs.ix[reg - 4] = val;
				break;
			case 8:
			case 9:
			case 10:
			case 11:
				regs.gpr[reg - 8] = val;
				break;
			case 12:
			case 13:
			case 14:
			case 15:
				regs.st[reg - 12] = val;
				break;
			case 16:
				break;
			case 17:
				break;
			case 18:
				regs.cr = val;
				break;
			case 19:
				regs.sr = val;
				break;
			case 20:
				break;
			case 21:
				break;
			case 22:
				break;
			case 23:
				break;
			case 24:
				break;
			case 25:
				break;
			case 26:
				break;
			case 27:
				break;
			case 28:
				break;
			case 29:
				break;
			case 30:
				break;
			case 31:
				break;
		}
	}

	uint16_t DspCore::MoveFromReg(int reg)
	{
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
		else if (addr >= 0x8000 && addr < (0x8000 + IROM_SIZE))
		{
			return &irom[(addr - 0x8000) << 1];
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
		else if (addr >= 0x8000 && addr < (0x8000 + DROM_SIZE))
		{
			return &drom[(addr - 0x8000) << 1];
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
		uint8_t* ptr = TranslateDMem(addr);

		if (ptr)
		{
			return MEMSwapHalf(*(uint16_t*)ptr);
		}

		return 0;
	}

	void DspCore::WriteDMem(DspAddress addr, uint16_t value)
	{
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

	}

	bool DspCore::DSPGetResetBit()
	{
		return false;
	}

	void DspCore::DSPSetIntBit(bool val)
	{

	}

	bool DspCore::DSPGetIntBit()
	{
		return false;
	}

	void DspCore::DSPSetHaltBit(bool val)
	{

	}

	bool DspCore::DSPGetHaltBit()
	{
		return false;
	}

	void DspCore::DSPWriteOutMailboxHi(uint16_t value)
	{

	}

	void DspCore::DSPWriteOutMailboxLo(uint16_t value)
	{

	}

	uint16_t DspCore::DSPReadOutMailboxHi()
	{
		return 0;
	}

	uint16_t DspCore::DSPReadOutMailboxLo()
	{
		return 0;
	}

	uint16_t DspCore::DSPReadInMailboxHi()
	{
		return 0;
	}

	uint16_t DspCore::DSPReadInMailboxLo()
	{
		return 0;
	}

	#pragma endregion "Flipper interface"

}
