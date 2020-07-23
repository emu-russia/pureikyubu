// Low-level DSP core

#include "pch.h"

using namespace Debug;

namespace DSP
{

	DspCore::DspCore(HWConfig* config)
	{
		dspThread = new Thread(DspThreadProc, true, this, "DspCore");

		HardReset();

		interp = new DspInterpreter(this);

		// Load IROM

		if (config != nullptr)
		{
			auto iromImage = Util::FileLoad(config->DspIromFilename);

			if (iromImage.empty() || iromImage.size() != IROM_SIZE)
			{
				Report(Channel::Norm, "Failed to load DSP IROM: %s\n", Util::TcharToString(config->DspIromFilename).c_str());
			}
			else
			{
				Report(Channel::DSP, "Loaded DSP IROM: %s\n", Util::TcharToString(config->DspIromFilename).c_str());
				memcpy(irom, iromImage.data(), IROM_SIZE);
			}

			/* Load DROM. */
			auto dromImage = Util::FileLoad(config->DspDromFilename);

			if (dromImage.empty() || dromImage.size() != DROM_SIZE)
			{
				Report(Channel::Norm, "Failed to load DSP DROM: %s\n", Util::TcharToString(config->DspDromFilename).c_str());
			}
			else
			{
				Report(Channel::DSP, "Loaded DSP DROM: %s\n", Util::TcharToString(config->DspDromFilename).c_str());
				memcpy(drom, dromImage.data(), DROM_SIZE);
			}
		}

		Report(Channel::DSP, "DSPCore: Ready\n");
	}

	DspCore::~DspCore()
	{
		delete dspThread;
		delete interp;
	}

	void DspCore::DspThreadProc(void * Parameter)
	{
		DspCore* core = (DspCore*)Parameter;

		while (true)
		{
			// Do DSP actions
			core->Update();
		}
	}

	void DspCore::Exception(DspException id)
	{
		if (logDspInterrupts)
		{
			Report(Channel::DSP, "Exception: 0x%04X\n", id);
		}

		regs.st[0].push_back(regs.pc);
		regs.st[1].push_back((DspAddress)regs.sr.bits);
		regs.sr.ge = 0;
		regs.pc = (DspAddress)id * 2;
	}

	void DspCore::ReturnFromException()
	{
		regs.sr.bits = (uint16_t)regs.st[1].back();
		regs.st[1].pop_back();
		regs.pc = regs.st[0].back();
		regs.st[0].pop_back();
	}

	void DspCore::SoftReset()
	{
		_TB(DspCore::SoftReset);
		regs.pc = DSPGetResetModifier() ? IROM_START_ADDRESS : 0;		// IROM start / 0
		Report(Channel::DSP, "Soft Reset pc = 0x%04X\n", regs.pc);

		pendingInterrupt = false;
		_TE();
	}

	void DspCore::HardReset()
	{
		_TB(DspCore::HardReset);
		Report(Channel::DSP, "DspCore::HardReset\n");

		if (Gekko::Gekko != nullptr)
		{
			savedGekkoTicks = Gekko::Gekko->GetTicks();
		}

		for (int i = 0; i < _countof(regs.st); i++)
		{
			regs.st[i].clear();
			regs.st[i].reserve(32);		// Should be enough
		}

		for (int i = 0; i < 4; i++)
		{
			regs.ar[i] = 0;
			regs.ix[i] = 0;
			regs.lm[i] = 0xFFFF;
		}

		for (int i = 0; i < 2; i++)
		{
			regs.ac[i].bits = 0;
			regs.ax[i].bits = 0;
		}

		regs.bank = 0xFF;
		regs.sr.bits = 0;
		regs.prod.h = 0;
		regs.prod.l = 0;
		regs.prod.m1 = 0;
		regs.prod.m2 = 0;
		
		// Hard Reset always from IROM start
		regs.pc = IROM_START_ADDRESS;
		Report(Channel::DSP, "Hard Reset pc = 0x%04X\n", regs.pc);

		ResetIfx();

		pendingInterrupt = false;
		pendingSoftReset = false;
		Suspend();
		_TE();
	}

	void DspCore::Run()
	{
		_TB(DspCore::Run);
		if (!dspThread->IsRunning())
		{
			dspThread->Resume();
			if (logDspControlBits)
			{
				Report(Channel::DSP, "DspCore::Run\n");
			}
			savedGekkoTicks = Gekko::Gekko->GetTicks();
		}
		_TE();
	}

	void DspCore::Suspend()
	{
		_TB(DspCore::Suspend);
		if (dspThread->IsRunning())
		{
			if (logDspControlBits)
			{
				Report(Channel::DSP, "DspCore::Suspend\n");
			}
			dspThread->Suspend();
		}
		_TE();
	}

	void DspCore::Update()
	{
		uint64_t ticks = Gekko::Gekko->GetTicks();

		if (ticks >= (savedGekkoTicks + GekkoTicksPerDspInstruction))
		{
			// Test breakpoints and canaries
			if (IsRunning())
			{
				TestCanary(regs.pc);

				if (TestBreakpoint(regs.pc))
				{
					Halt("DSP: IMEM breakpoint at 0x%04X\n", regs.pc);
					Suspend();
					Gekko::Gekko->Suspend();
					return;
				}

				if (regs.pc == oneShotBreakpoint)
				{
					oneShotBreakpoint = 0xffff;
					Suspend();
					Gekko::Gekko->Suspend();
					return;
				}
			}

			if (pendingSoftReset)
			{
				pendingSoftReset = false;
				Report(Channel::DSP, "SoftReset Acknowledge\n");
				SoftReset();
			}
			else if (pendingInterrupt)
			{
				pendingInterruptDelay--;
				if (pendingInterruptDelay == 0)
				{
					Report(Channel::DSP, "Interrupt Acknowledge\n");
					pendingInterrupt = false;
					Exception(DspException::INT);
				}
			}
			else if (Accel.pendingOverflow)
			{
				Accel.pendingOverflow = false;
				Exception(Accel.overflowVector);
			}

			interp->ExecuteInstr();
			savedGekkoTicks = ticks;
		}
	}

	#pragma region "Debug"

	void DspCore::AddBreakpoint(DspAddress imemAddress)
	{
		breakPointsSpinLock.Lock();
		breakpoints.push_back(imemAddress);
		breakPointsSpinLock.Unlock();
	}

	void DspCore::RemoveBreakpoint(DspAddress imemAddress)
	{
		breakPointsSpinLock.Lock();
		bool found = false;
		for (auto it = breakpoints.begin(); it != breakpoints.end(); ++it)
		{
			if (*it == imemAddress)
			{
				found = true;
				break;
			}
		}
		if (found)
		{
			breakpoints.remove(imemAddress);
		}
		breakPointsSpinLock.Unlock();
	}

	void DspCore::ListBreakpoints()
	{
		breakPointsSpinLock.Lock();
		for (auto it = breakpoints.begin(); it != breakpoints.end(); ++it)
		{
			Report(Channel::Norm, "0x%04X\n", *it);
		}
		breakPointsSpinLock.Unlock();
	}

	void DspCore::ClearBreakpoints()
	{
		breakPointsSpinLock.Lock();
		breakpoints.clear();
		breakPointsSpinLock.Unlock();
	}

	bool DspCore::TestBreakpoint(DspAddress imemAddress)
	{
		bool found = false;

		breakPointsSpinLock.Lock();
		for (auto it = breakpoints.begin(); it != breakpoints.end(); ++it)
		{
			if (*it == imemAddress)
			{
				found = true;
				break;
			}
		}
		breakPointsSpinLock.Unlock();

		return found;
	}

	void DspCore::ToggleBreakpoint(DspAddress imemAddress)
	{
		if (TestBreakpoint(imemAddress))
		{
			RemoveBreakpoint(imemAddress);
		}
		else
		{
			AddBreakpoint(imemAddress);
		}
	}

	void DspCore::AddOneShotBreakpoint(DspAddress imemAddress)
	{
		oneShotBreakpoint = imemAddress;
	}

	void DspCore::AddCanary(DspAddress imemAddress, std::string text)
	{
		canariesSpinLock.Lock();
		canaries[imemAddress] = text;
		canariesSpinLock.Unlock();
	}

	void DspCore::ListCanaries()
	{
		canariesSpinLock.Lock();
		for (auto it = canaries.begin(); it != canaries.end(); ++it)
		{
			Report(Channel::Norm, "0x%04X: %s\n", it->first, it->second.c_str());
		}
		canariesSpinLock.Unlock();
	}

	void DspCore::ClearCanaries()
	{
		canariesSpinLock.Lock();
		canaries.clear();
		canariesSpinLock.Unlock();
	}

	bool DspCore::TestCanary(DspAddress imemAddress)
	{
		canariesSpinLock.Lock();

		auto it = canaries.find(imemAddress);
		if (it != canaries.end())
		{
			Report(Channel::DSP, it->second.c_str());
			canariesSpinLock.Unlock();
			return true;
		}

		canariesSpinLock.Unlock();
		return false;
	}

	// Execute single instruction (by interpreter)
	void DspCore::Step()
	{
		if (IsRunning())
		{
			Report(Channel::DSP, "It is impossible while running DSP thread.\n");
			_TE();
			return;
		}

		if (pendingSoftReset)
		{
			pendingSoftReset = false;
			Report(Channel::DSP, "SoftReset Acknowledge\n");
			SoftReset();
		}
		else if (pendingInterrupt)
		{
			pendingInterruptDelay--;
			if (pendingInterruptDelay == 0)
			{
				Report(Channel::DSP, "Interrupt Acknowledge\n");
				pendingInterrupt = false;
				Exception(DspException::INT);
			}
		}
		else if (Accel.pendingOverflow)
		{
			Accel.pendingOverflow = false;
			Exception(Accel.overflowVector);
		}

		interp->ExecuteInstr();
	}

	// Print only registers different from previous ones
	void DspCore::DumpRegs(DspRegs* prevState)
	{
		if (regs.pc != prevState->pc)
		{
			Report(Channel::Norm, "pc: 0x%04X\n", regs.pc);
		}

		if (regs.prod.bitsPacked != prevState->prod.bitsPacked)
		{
			Report(Channel::Norm, "prod: 0x%04X_%04X_%04X_%04X\n",
				regs.prod.h, regs.prod.m2, regs.prod.m1, regs.prod.l );
		}

		if (regs.bank != prevState->bank)
		{
			Report(Channel::Norm, "bank: 0x%04X\n", regs.bank);
		}

		if (regs.sr.bits != prevState->sr.bits)
		{
			// TODO: Add bit description
			Report(Channel::Norm, "sr: 0x%04X\n", regs.sr);
		}

		for (int i = 0; i < 2; i++)
		{
			if (regs.ac[i].bits != prevState->ac[i].bits)
			{
				Report(Channel::Norm, "ac%i: 0x%04X_%04X_%04X\n", i,
					regs.ac[i].h, regs.ac[i].m, regs.ac[i].l);
			}
		}

		for (int i = 0; i < 2; i++)
		{
			if (regs.ax[i].bits != prevState->ax[i].bits)
			{
				Report(Channel::Norm, "ax%i: 0x%04X_%04X\n", i,
					regs.ax[i].h, regs.ax[i].l);
			}
		}

		for (int i = 0; i < 4; i++)
		{
			if (regs.ar[i] != prevState->ar[i])
			{
				Report(Channel::Norm, "ar%i: 0x%04X\n", i, regs.ar[i]);
			}
		}

		for (int i = 0; i < 4; i++)
		{
			if (regs.ix[i] != prevState->ix[i])
			{
				Report(Channel::Norm, "ix%i: 0x%04X\n", i, regs.ix[i]);
			}
		}

		for (int i = 0; i < 4; i++)
		{
			if (regs.lm[i] != prevState->lm[i])
			{
				Report(Channel::Norm, "lm%i: 0x%04X\n", 8+i, regs.lm[i]);
			}
		}
	}

	// Dump IFX State
	void DspCore::DumpIfx()
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
			case (int)DspRegister::lm0:
			case (int)DspRegister::lm1:
			case (int)DspRegister::lm2:
			case (int)DspRegister::lm3:
				regs.lm[reg - (int)DspRegister::limitRegs] = val;
				break;
			case (int)DspRegister::st0:
			case (int)DspRegister::st1:
			case (int)DspRegister::st2:
			case (int)DspRegister::st3:
				regs.st[reg - (int)DspRegister::stackRegs].push_back((DspAddress)val);
				break;
			case (int)DspRegister::ac0h:
				regs.ac[0].h = val & 0xff;
				break;
			case (int)DspRegister::ac1h:
				regs.ac[1].h = val & 0xff;
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
					regs.ac[0].h = (val & 0x8000) ? 0xFF : 0;
					regs.ac[0].l = 0;
				}
				break;
			case (int)DspRegister::ac1m:
				regs.ac[1].m = val;
				if (regs.sr.sxm)
				{
					regs.ac[1].h = (val & 0x8000) ? 0xFF : 0;
					regs.ac[1].l = 0;
				}
				break;
		}
	}

	uint16_t DspCore::MoveFromReg(int reg)
	{
		uint16_t val = 0;

		switch (reg)
		{
			case (int)DspRegister::ar0:
			case (int)DspRegister::ar1:
			case (int)DspRegister::ar2:
			case (int)DspRegister::ar3:
				val = regs.ar[reg];
				break;
			case (int)DspRegister::ix0:
			case (int)DspRegister::ix1:
			case (int)DspRegister::ix2:
			case (int)DspRegister::ix3:
				val = regs.ix[reg - (int)DspRegister::indexRegs];
				break;
			case (int)DspRegister::lm0:
			case (int)DspRegister::lm1:
			case (int)DspRegister::lm2:
			case (int)DspRegister::lm3:
				val = regs.lm[reg - (int)DspRegister::limitRegs];
				break;
			case (int)DspRegister::st0:
			case (int)DspRegister::st1:
			case (int)DspRegister::st2:
			case (int)DspRegister::st3:
				val = (uint16_t)regs.st[reg - (int)DspRegister::stackRegs].back();
				break;
			case (int)DspRegister::ac0h:
				val = regs.ac[0].h & 0xff;
				break;
			case (int)DspRegister::ac1h:
				val = regs.ac[1].h & 0xff;
				break;
			case (int)DspRegister::bank:
				val = regs.bank;
				break;
			case (int)DspRegister::sr:
				val = regs.sr.bits;
				break;
			case (int)DspRegister::prodl:
				val = regs.prod.l;
				break;
			case (int)DspRegister::prodm1:
				val = regs.prod.m1;
				break;
			case (int)DspRegister::prodh:
				val = regs.prod.h;
				break;
			case (int)DspRegister::prodm2:
				val = regs.prod.m2;
				break;
			case (int)DspRegister::ax0l:
				val = regs.ax[0].l;
				break;
			case (int)DspRegister::ax0h:
				val = regs.ax[0].h;
				break;
			case (int)DspRegister::ax1l:
				val = regs.ax[1].l;
				break;
			case (int)DspRegister::ax1h:
				val = regs.ax[1].h;
				break;
			case (int)DspRegister::ac0l:
				val = regs.ac[0].l;
				break;
			case (int)DspRegister::ac1l:
				val = regs.ac[1].l;
				break;
			case (int)DspRegister::ac0m:
				if (regs.sr.sxm)
				{
					//int64_t a = SignExtend40(regs.ac[0].sbits) >> 16;
					//val = (uint16_t)(max(-0x8000, min(a, 0x7fff)));

					int64_t a = SignExtend40(regs.ac[0].sbits);
					if (a != (int32_t)a)
					{
						val = a > 0 ? 0x7fff : 0x8000;
					}
					else
					{
						val = regs.ac[0].m;
					}
				}
				else
				{
					val = regs.ac[0].m;
				}
				break;
			case (int)DspRegister::ac1m:
				if (regs.sr.sxm)
				{
					//int64_t a = SignExtend40(regs.ac[1].sbits) >> 16;
					//val = (uint16_t)(max(-0x8000, min(a, 0x7fff)));

					int64_t a = SignExtend40(regs.ac[1].sbits);
					if (a != (int32_t)a)
					{
						val = a > 0 ? 0x7fff : 0x8000;
					}
					else
					{
						val = regs.ac[1].m;
					}
				}
				else
				{
					val = regs.ac[1].m;
				}
				break;
		}
		return val;
	}

	#pragma endregion "Register access"


	#pragma region "Memory Engine"

	uint8_t* DspCore::TranslateIMem(DspAddress addr)
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

	uint8_t* DspCore::TranslateDMem(DspAddress addr)
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
				return CpuToDspReadLo(true);
			case (DspAddress)DspHardwareRegs::DMBH:
				return DspToCpuReadHi(true);
			case (DspAddress)DspHardwareRegs::DMBL:
				return DspToCpuReadLo(true);

			case (DspAddress)DspHardwareRegs::DIRQ:
				return DSPGetInterruptStatus() ? 1 : 0;

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
				Report(Channel::DSP, "DSP Unknown HW read 0x%04X\n", addr);
				//Suspend();
				break;
			}
			return 0;
		}

		uint8_t* ptr = TranslateDMem(addr);

		if (ptr)
		{
			return _byteswap_ushort(*(uint16_t*)ptr);
		}

		if (haltOnUnmappedMemAccess)
		{
			Halt("DSP Unmapped DMEM read 0x%04X\n", addr);
			Suspend();
		}
		return 0xFFFF;
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
						DSPAssertInt();
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
					ResetAccel();
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
					Report(Channel::DSP, "DSP Unknown HW write 0x%04X = 0x%04X\n", addr, value);
					//Suspend();
					break;
			}
			return;
		}

		if (addr < (DRAM_SIZE / 2))
		{
			uint8_t* ptr = TranslateDMem(addr);

			if (ptr)
			{
				*(uint16_t*)ptr = _byteswap_ushort(value);
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

	void DspCore::DSPSetResetBit(bool val)
	{
		if (val)
		{
			if (logDspControlBits)
			{
				Report(Channel::DSP, "Pending SoftReset\n");
			}
			pendingSoftReset = true;
		}
	}

	bool DspCore::DSPGetResetBit()
	{
		return false;
	}

	void DspCore::DSPSetIntBit(bool val)
	{
		if (val && !pendingSoftReset && regs.sr.eie && regs.sr.ge)
		{
			if (logDspControlBits)
			{
				Report(Channel::DSP, "Pending Interrupt\n");
			}
			pendingInterrupt = true;
			pendingInterruptDelay = 2;
		}
	}

	bool DspCore::DSPGetIntBit()
	{
		return pendingInterrupt;
	}

	void DspCore::DSPSetHaltBit(bool val)
	{
		val ? Suspend() : Run();
	}

	bool DspCore::DSPGetHaltBit()
	{
		return !dspThread->IsRunning();
	}

	#pragma endregion "Flipper interface"


	#pragma region "IFX"

	void DspCore::ResetIfx()
	{
		DspToCpuMailbox[0] = 0;
		DspToCpuMailbox[1] = 0;
		CpuToDspMailbox[0] = 0;
		CpuToDspMailbox[1] = 0;

		memset(&DmaRegs, 0, sizeof(DmaRegs));
		memset(&Accel, 0, sizeof(Accel));

		ResetAccel();
	}

	// Instant DMA
	void DspCore::DoDma()
	{
		_TB(DspCore::DoDma);
		uint8_t* ptr = nullptr;

		if (logDspDma)
		{
			Report(Channel::DSP, "DspCore::Dma: Mmem: 0x%08X, DspAddr: 0x%04X, Size: 0x%04X, Ctrl: %i\n",
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
			Halt("DspCore::DoDma: invalid dsp address: 0x%04X\n", DmaRegs.dspAddr);
			_TE();
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

		/* Dump ucode. */
		if (dumpUcode)
		{
			if (DmaRegs.control.Imem && !DmaRegs.control.Dsp2Mmem)
			{
				auto filename = fmt::format(L"Data\\DspUcode_{:04X}.bin", DmaRegs.blockSize);
				auto buffer = std::vector<uint8_t>(ptr, ptr + DmaRegs.blockSize);
				
				Util::FileSave(filename, buffer);
				Report(Channel::DSP, "DSP Ucode dumped to DspUcode_%04X.bin\n", DmaRegs.blockSize);
			}
		}

		// Dump PCM samples coming from mixer
#if 0
		if (!DmaRegs.control.Imem && DmaRegs.control.Dsp2Mmem && 
			(0x400 >= DmaRegs.dspAddr && DmaRegs.dspAddr < 0x600) && 
			DmaRegs.blockSize == 0x80 )
		{
			memcpy(&dspSamples[dspSampleSize], ptr, DmaRegs.blockSize);
			dspSampleSize += DmaRegs.blockSize;
		}
#endif

		_TE();
	}

	#pragma endregion "IFX"

	#pragma region "Multiplier and ALU"

	int64_t DspCore::SignExtend40(int64_t a)
	{
		if (a & 0x80'0000'0000)
		{
			a |= 0xffff'ff00'0000'0000;
		}
		else
		{
			a &= 0x0000'00ff'ffff'ffff;
		}
		return a;
	}

	int64_t DspCore::SignExtend16(int16_t a)
	{
		int64_t res = a;
		if (res & 0x8000)
		{
			res |= 0xffff'ffff'ffff'0000;
		}
		return res;
	}

	// The current multiplication result (product) is stored as a set of "temporary" values (prod.h, prod.m1, prod.m2, prod.l).
	// The exact algorithm of the multiplier is unknown, but you can guess that these temporary results are collected from partial 
	// sums of multiplications between the upper and lower halves of the 16-bit operands.

	void DspCore::PackProd(DspProduct& prod)
	{
		uint64_t hi = (uint64_t)prod.h << 32;
		uint64_t mid = ((uint64_t)prod.m1 + (uint64_t)prod.m2) << 16;
		uint64_t low = prod.l;
		uint64_t res = hi + mid + low;
		res &= 0xff'ffff'ffff;
		prod.bitsPacked = res;
	}

	void DspCore::UnpackProd(DspProduct& prod)
	{
		prod.h = (prod.bitsPacked >> 32) & 0xff;
		prod.m1 = (prod.bitsPacked >> 16) & 0xffff;
		prod.m2 = 0;
		prod.l = prod.bitsPacked & 0xffff;
	}

	// Treat operands as signed 16-bit numbers and produce signed multiply product.

	DspProduct DspCore::Muls(int16_t a, int16_t b, bool scale)
	{
		DspProduct prod;
#if 0
		// P = A x B= (AH-AL) x (BH-BL) = AH x BH+AH x BL + AL x BH+ AL x BL 

		int32_t u = ((int32_t)(int16_t)(a & 0xff00)) * ((int32_t)(int16_t)(b & 0xff00));
		int32_t c1 = ((int32_t)(int16_t)(a & 0xff00)) * ((int32_t)(b & 0xff));
		int32_t c2 = ((int32_t)(a & 0xff)) * ((int32_t)(int16_t)(b & 0xff00));
		int32_t l = ((int32_t)(a & 0xff)) * ((int32_t)(b & 0xff));

		int32_t m = c1 + c2 + l;

		prod.h = ((a ^ b) & 0x8000) ? 0xff : 0;
		prod.m1 = u >> 16;
		prod.m2 = m >> 16;
		prod.l = m & 0xffff;
#else
		prod.bitsPacked = (int64_t)((int64_t)(int32_t)a * (int64_t)(int32_t)b);
		if (scale)
			prod.bitsPacked <<= 1;
		UnpackProd(prod);
#endif

		return prod;
	}

	// Treat operands as unsigned 16-bit numbers and produce unsigned multiply product.

	DspProduct DspCore::Mulu(uint16_t a, uint16_t b, bool scale)
	{
		DspProduct prod;
#if 0
		// P = A x B = (AH-AL) x (BH-BL) = AHxBH + AHxBL + ALxBH + ALxBL

		uint32_t u = ((uint32_t)a & 0xff00) * ((uint32_t)b & 0xff00);
		uint32_t c1 = ((uint32_t)a & 0xff00) * ((uint32_t)b & 0xff);
		uint32_t c2 = ((uint32_t)a & 0xff) * ((uint32_t)b & 0xff00);
		uint32_t l = ((uint32_t)a & 0xff) * ((uint32_t)b & 0xff);

		uint32_t m = c1 + c2 + l;

		prod.h = 0;
		prod.m1 = u >> 16;
		prod.m2 = m >> 16;
		prod.l = m & 0xffff;
#else
		prod.bitsPacked = (uint64_t)((uint64_t)(uint32_t)a * (uint64_t)(uint32_t)b);
		if (scale)
			prod.bitsPacked <<= 1;
		UnpackProd(prod);
#endif
		return prod;
	}

	// Treat operand `a` as unsigned 16-bit numbers and operand `b` as signed 16-bit number and produce signed multiply product.

	DspProduct DspCore::Mulus(uint16_t a, int16_t b, bool scale)
	{
		DspProduct prod;
		prod.bitsPacked = (int64_t)((int64_t)(int32_t)(uint32_t)a * (int64_t)(int32_t)b);
		if (scale)
			prod.bitsPacked <<= 1;
		UnpackProd(prod);
		return prod;
	}

	void DspCore::ArAdvance(int r, int16_t step)
	{
		uint16_t base = regs.ar[r] & ~regs.lm[r];
		regs.ar[r] += step;
		regs.ar[r] = base + (regs.ar[r] & regs.lm[r]);
	}

	#pragma endregion "Multiplier and ALU"

	void DspCore::InitSubsystem()
	{
		JDI::Hub.AddNode(DSP_JDI_JSON, dsp_init_handlers);
	}

	void DspCore::ShutdownSubsystem()
	{
		JDI::Hub.RemoveNode(DSP_JDI_JSON);
	}

}
