// Low-level DSP core

#include "pch.h"

using namespace Debug;

namespace DSP
{

	DspCore::DspCore(Dsp16* parent)
	{
		dsp = parent;
		interp = new DspInterpreter(this);

		// We will not emulate the Dsp feature - the combined stack pointer for eas and lcs.

		regs.pcs = new DspStack(8);
		regs.pss = new DspStack(4);
		regs.eas = new DspStack(4);
		regs.lcs = new DspStack(4);

		HardReset();
	}

	DspCore::~DspCore()
	{
		delete interp;

		delete regs.pcs;
		delete regs.pss;
		delete regs.eas;
		delete regs.lcs;
	}

	void DspCore::CheckInterrupts()
	{
		if (!intr.pendingSomething)
		{
			return;
		}

		size_t i = 0;

		for ( i ; i < (size_t)DspInterrupt::Max; i++)
		{
			if (intr.pending[i])
			{
				if (intr.pendingDelay[i] <= 0)
				{
					if (!regs.pcs->push(regs.pc))
					{
						Halt("CheckInterrupts: pcs overflow\n");
					}
					if (!regs.pss->push(regs.psr.bits))
					{
						Halt("CheckInterrupts: pss overflow\n");
					}
					regs.psr.et = 0;

					if (i == (size_t)DspInterrupt::Reset)
					{
						regs.pc = Flipper::DSPGetResetModifier() ? dsp->IROM_START_ADDRESS : 0;		// IROM start / 0
					}
					else
					{
						regs.pc = (DspAddress)i * 2;
					}

					if (dsp->logDspInterrupts)
					{
						Report(Channel::DSP, "Interrupt Acknowledge: 0x%04X\n", regs.pc);
					}

					intr.pending[i] = false;
					break;
				}
				else
				{
					intr.pendingDelay[i]--;
				}
			}
		}

		if (i == (size_t)DspInterrupt::Max)
		{
			intr.pendingSomething = false;
		}
	}

	void DspCore::AssertInterrupt(DspInterrupt id)
	{
		// Source disabled ?

		switch (id)
		{
			case DspInterrupt::Reset:
				break;
			case DspInterrupt::Error:
				break;
			case DspInterrupt::Trap:
				break;

			case DspInterrupt::Acrs:
			case DspInterrupt::Acwe:
			case DspInterrupt::Dcre:
				// ACRS, ACWE and DCRE interrupts are generated regardless of whether the ET bit is set or cleared (controlled only by the TE1 bit)
				if (regs.psr.te1 == 0)
				{
					return;
				}
				break;

			case DspInterrupt::AiDma:
				if ((regs.psr.te2 & regs.psr.et) == 0)
				{
					return;
				}
				break;

			case DspInterrupt::CpuInt:
				if ((regs.psr.te3 & regs.psr.et) == 0)
				{
					return;
				}
				break;
		}

		// Pending

		if (dsp->logDspInterrupts)
		{
			Report(Channel::DSP, "Interrupt Pending: 0x%04X\n", (DspAddress)id * 2);
		}

		intr.pending[(size_t)id] = true;

		// Pending delay

		int pendingDelay = 0;

		switch (id)
		{
			case DspInterrupt::Reset:
			case DspInterrupt::Error:
			case DspInterrupt::Trap:
				pendingDelay = 0;
				break;

			case DspInterrupt::Acrs:
			case DspInterrupt::Acwe:
			case DspInterrupt::Dcre:
				pendingDelay = 0;
				break;

			case DspInterrupt::AiDma:
				pendingDelay = 0;
				break;

			case DspInterrupt::CpuInt:
				pendingDelay = 2;
				break;
		}

		intr.pendingDelay[(size_t)id] = pendingDelay;
		intr.pendingSomething = true;
	}

	bool DspCore::IsInterruptPending(DspInterrupt id)
	{
		return intr.pending[(size_t)id];
	}

	void DspCore::ReturnFromInterrupt()
	{
		if (!regs.pss->pop(regs.psr.bits))
		{
			AssertInterrupt(DspInterrupt::Error);
			return;
		}
		uint16_t pc;
		if (!regs.pcs->pop(pc))
		{
			AssertInterrupt(DspInterrupt::Error);
			return;
		}
		regs.pc = pc;
	}

	void DspCore::HardReset()
	{
		Report(Channel::DSP, "DspCore::HardReset\n");

		if (Gekko::Gekko != nullptr)
		{
			dsp->savedGekkoTicks = Gekko::Gekko->GetTicks();
		}

		repeatCount = 0;

		regs.pcs->clear();
		regs.pss->clear();
		regs.eas->clear();
		regs.lcs->clear();

		for (int i = 0; i < 4; i++)
		{
			regs.r[i] = 0;
			regs.m[i] = 0;
			regs.l[i] = 0xFFFF;
		}

		regs.a.bits = 0;
		regs.b.bits = 0;
		regs.x.bits = 0;
		regs.y.bits = 0;

		regs.dpp = 0xFF;
		regs.psr.bits = 0;
		regs.prod.h = 0;
		regs.prod.l = 0;
		regs.prod.m1 = 0;
		regs.prod.m2 = 0;
		
		// Clear pending interrupts

		for (size_t i = 0; i < (size_t)DspInterrupt::Max; i++)
		{
			intr.pending[i] = false;
			intr.pendingDelay[i] = 0;
		}

		intr.pendingSomething = false;

		// Hard Reset always from IROM start
		regs.pc = dsp->IROM_START_ADDRESS;
		Report(Channel::DSP, "Hard Reset pc = 0x%04X\n", regs.pc);

		dsp->ResetIfx();

		dsp->Suspend();
	}

	void DspCore::Update()
	{
		uint64_t ticks = Gekko::Gekko->GetTicks();

		if (ticks >= (dsp->savedGekkoTicks + GekkoTicksPerDspInstruction))
		{
			// Test breakpoints and canaries
			if (dsp->IsRunning())
			{
				TestCanary(regs.pc);

				if (TestBreakpoint(regs.pc))
				{
					Halt("DSP: IMEM breakpoint at 0x%04X\n", regs.pc);
					dsp->Suspend();
					Gekko::Gekko->Suspend();
					return;
				}

				if (regs.pc == oneShotBreakpoint)
				{
					oneShotBreakpoint = 0xffff;
					dsp->Suspend();
					Gekko::Gekko->Suspend();
					return;
				}
			}

			// Check pending interrupts
			CheckInterrupts();

			interp->ExecuteInstr();
			dsp->savedGekkoTicks = ticks;
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
		if (dsp->IsRunning())
		{
			Report(Channel::DSP, "It is impossible while running DSP thread.\n");
			_TE();
			return;
		}

		// Check pending interrupts
		CheckInterrupts();

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

		if (regs.dpp != prevState->dpp)
		{
			Report(Channel::Norm, "dpp: 0x%04X\n", regs.dpp);
		}

		if (regs.psr.bits != prevState->psr.bits)
		{
			// TODO: Add bit description
			Report(Channel::Norm, "psr: 0x%04X\n", regs.psr);
		}

		if (regs.a.bits != prevState->a.bits)
		{
			Report(Channel::Norm, "a: 0x%04X_%04X_%04X\n",
				regs.a.h, regs.a.m, regs.a.l);
		}

		if (regs.b.bits != prevState->b.bits)
		{
			Report(Channel::Norm, "b: 0x%04X_%04X_%04X\n",
				regs.b.h, regs.b.m, regs.b.l);
		}

		if (regs.x.bits != prevState->x.bits)
		{
			Report(Channel::Norm, "x: 0x%04X_%04X\n",
				regs.x.h, regs.x.l);
		}

		if (regs.y.bits != prevState->y.bits)
		{
			Report(Channel::Norm, "y: 0x%04X_%04X\n",
				regs.y.h, regs.y.l);
		}

		for (int i = 0; i < 4; i++)
		{
			if (regs.r[i] != prevState->r[i])
			{
				Report(Channel::Norm, "r%i: 0x%04X\n", i, regs.r[i]);
			}
		}

		for (int i = 0; i < 4; i++)
		{
			if (regs.m[i] != prevState->m[i])
			{
				Report(Channel::Norm, "m%i: 0x%04X\n", i, regs.m[i]);
			}
		}

		for (int i = 0; i < 4; i++)
		{
			if (regs.l[i] != prevState->l[i])
			{
				Report(Channel::Norm, "l%i: 0x%04X\n", 8+i, regs.l[i]);
			}
		}
	}

	#pragma endregion "Debug"


	#pragma region "Register access"

	void DspCore::MoveToReg(int reg, uint16_t val)
	{
		switch (reg)
		{
			case (int)DspRegister::r0:
			case (int)DspRegister::r1:
			case (int)DspRegister::r2:
			case (int)DspRegister::r3:
				regs.r[reg] = val;
				break;
			case (int)DspRegister::m0:
			case (int)DspRegister::m1:
			case (int)DspRegister::m2:
			case (int)DspRegister::m3:
				regs.m[reg - (int)DspRegister::m0] = val;
				break;
			case (int)DspRegister::l0:
			case (int)DspRegister::l1:
			case (int)DspRegister::l2:
			case (int)DspRegister::l3:
				regs.l[reg - (int)DspRegister::l0] = val;
				break;
			case (int)DspRegister::pcs:
				if (!regs.pcs->push(val))
				{
					AssertInterrupt(DspInterrupt::Error);
				}
				break;
			case (int)DspRegister::pss:
				if (!regs.pss->push(val))
				{
					AssertInterrupt(DspInterrupt::Error);
				}
				break;
			case (int)DspRegister::eas:
				if (!regs.eas->push(val))
				{
					AssertInterrupt(DspInterrupt::Error);
				}
				break;
			case (int)DspRegister::lcs:
				if (!regs.lcs->push(val))
				{
					AssertInterrupt(DspInterrupt::Error);
				}
				break;
			case (int)DspRegister::a2:
				regs.a.h = val & 0xff;
				break;
			case (int)DspRegister::b2:
				regs.b.h = val & 0xff;
				break;
			case (int)DspRegister::dpp:
				regs.dpp = val & 0xff;
				break;
			case (int)DspRegister::psr:
				regs.psr.bits = val;
				break;
			case (int)DspRegister::ps0:
				regs.prod.l = val;
				break;
			case (int)DspRegister::ps1:
				regs.prod.m1 = val;
				break;
			case (int)DspRegister::ps2:
				regs.prod.h = val & 0xff;
				break;
			case (int)DspRegister::pc1:
				regs.prod.m2 = val;
				break;
			case (int)DspRegister::x0:
				regs.x.l = val;
				break;
			case (int)DspRegister::y0:
				regs.y.l = val;
				break;
			case (int)DspRegister::x1:
				regs.x.h = val;
				break;
			case (int)DspRegister::y1:
				regs.y.h = val;
				break;
			case (int)DspRegister::a0:
				regs.a.l = val;
				break;
			case (int)DspRegister::b0:
				regs.b.l = val;
				break;
			case (int)DspRegister::a1:
				regs.a.m = val;
				if (regs.psr.xl)
				{
					regs.a.h = (val & 0x8000) ? 0xFF : 0;
					regs.a.l = 0;
				}
				break;
			case (int)DspRegister::b1:
				regs.b.m = val;
				if (regs.psr.xl)
				{
					regs.b.h = (val & 0x8000) ? 0xFF : 0;
					regs.b.l = 0;
				}
				break;
		}
	}

	uint16_t DspCore::MoveFromReg(int reg)
	{
		uint16_t val = 0;

		switch (reg)
		{
			case (int)DspRegister::r0:
			case (int)DspRegister::r1:
			case (int)DspRegister::r2:
			case (int)DspRegister::r3:
				val = regs.r[reg];
				break;
			case (int)DspRegister::m0:
			case (int)DspRegister::m1:
			case (int)DspRegister::m2:
			case (int)DspRegister::m3:
				val = regs.m[reg - (int)DspRegister::m0];
				break;
			case (int)DspRegister::l0:
			case (int)DspRegister::l1:
			case (int)DspRegister::l2:
			case (int)DspRegister::l3:
				val = regs.l[reg - (int)DspRegister::l0];
				break;
			case (int)DspRegister::pcs:
				if (!regs.pcs->pop(val))
				{
					AssertInterrupt(DspInterrupt::Error);
				}
				break;
			case (int)DspRegister::pss:
				if (!regs.pss->pop(val))
				{
					AssertInterrupt(DspInterrupt::Error);
				}
				break;
			case (int)DspRegister::eas:
				if (!regs.eas->pop(val))
				{
					AssertInterrupt(DspInterrupt::Error);
				}
				break;
			case (int)DspRegister::lcs:
				if (!regs.lcs->pop(val))
				{
					AssertInterrupt(DspInterrupt::Error);
				}
				break;
			case (int)DspRegister::a2:
				val = regs.a.h & 0xff;
				break;
			case (int)DspRegister::b2:
				val = regs.b.h & 0xff;
				break;
			case (int)DspRegister::dpp:
				val = regs.dpp & 0xff;
				break;
			case (int)DspRegister::psr:
				val = regs.psr.bits;
				break;
			case (int)DspRegister::ps0:
				val = regs.prod.l;
				break;
			case (int)DspRegister::ps1:
				val = regs.prod.m1;
				break;
			case (int)DspRegister::ps2:
				val = regs.prod.h & 0xff;
				break;
			case (int)DspRegister::pc1:
				val = regs.prod.m2;
				break;
			case (int)DspRegister::x0:
				val = regs.x.l;
				break;
			case (int)DspRegister::y0:
				val = regs.y.l;
				break;
			case (int)DspRegister::x1:
				val = regs.x.h;
				break;
			case (int)DspRegister::y1:
				val = regs.y.h;
				break;
			case (int)DspRegister::a0:
				val = regs.a.l;
				break;
			case (int)DspRegister::b0:
				val = regs.b.l;
				break;
			case (int)DspRegister::a1:
				if (regs.psr.xl)
				{
					//int64_t a = SignExtend40(regs.ac[0].sbits) >> 16;
					//val = (uint16_t)(max(-0x8000, min(a, 0x7fff)));

					int64_t a = SignExtend40(regs.a.sbits);
					if (a != (int32_t)a)
					{
						val = a > 0 ? 0x7fff : 0x8000;
					}
					else
					{
						val = regs.a.m;
					}
				}
				else
				{
					val = regs.a.m;
				}
				break;
			case (int)DspRegister::b1:
				if (regs.psr.xl)
				{
					//int64_t a = SignExtend40(regs.ac[1].sbits) >> 16;
					//val = (uint16_t)(max(-0x8000, min(a, 0x7fff)));

					int64_t a = SignExtend40(regs.b.sbits);
					if (a != (int32_t)a )
					{
						val = a > 0 ? 0x7fff : 0x8000;
					}
					else
					{
						val = regs.b.m;
					}
				}
				else
				{
					val = regs.b.m;
				}
				break;
		}
		return val;
	}

	#pragma endregion "Register access"

}
