// Low-level DSP core

#include "pch.h"

using namespace Debug;

namespace DSP
{

	DspCore::DspCore(Dsp16* parent)
	{
		dsp = parent;
		interp = new DspInterpreter(this);
		HardReset();
	}

	DspCore::~DspCore()
	{
		delete interp;
	}

	void DspCore::CheckInterrupts()
	{
		for (size_t i = 0; i < (size_t)DspInterrupt::Max; i++)
		{
			if (intr.pending[i])
			{
				if (intr.pendingDelay[i] <= 0)
				{
					if (dsp->logDspInterrupts)
					{
						Report(Channel::DSP, "Interrupt Acknowledge: 0x%04X\n", (DspAddress)i * 2);
					}

					regs.st[0].push_back(regs.pc);
					regs.st[1].push_back((DspAddress)regs.psr.bits);
					regs.psr.et = 0;

					if (i == (size_t)DspInterrupt::Reset)
					{
						regs.pc = DSPGetResetModifier() ? dsp->IROM_START_ADDRESS : 0;		// IROM start / 0
					}
					else
					{
						regs.pc = (DspAddress)i * 2;
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
				if ((regs.psr.te1 & regs.psr.et) == 0)
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

		size_t pendingDelay = 0;

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
	}

	bool DspCore::IsInterruptPending(DspInterrupt id)
	{
		return intr.pending[(size_t)id];
	}

	void DspCore::ReturnFromInterrupt()
	{
		regs.psr.bits = (uint16_t)regs.st[1].back();
		regs.st[1].pop_back();
		regs.pc = regs.st[0].back();
		regs.st[0].pop_back();
	}

	void DspCore::HardReset()
	{
		Report(Channel::DSP, "DspCore::HardReset\n");

		if (Gekko::Gekko != nullptr)
		{
			dsp->savedGekkoTicks = Gekko::Gekko->GetTicks();
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
			case (int)DspRegister::dpp:
				regs.dpp = val & 0xFF;
				break;
			case (int)DspRegister::psr:
				regs.psr.bits = val;
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
				if (regs.psr.xl)
				{
					regs.ac[0].h = (val & 0x8000) ? 0xFF : 0;
					regs.ac[0].l = 0;
				}
				break;
			case (int)DspRegister::ac1m:
				regs.ac[1].m = val;
				if (regs.psr.xl)
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
			case (int)DspRegister::dpp:
				val = regs.dpp;
				break;
			case (int)DspRegister::psr:
				val = regs.psr.bits;
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
				if (regs.psr.xl)
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
				if (regs.psr.xl)
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

	// Circular addressing logic

	void DspCore::ArAdvance(int r, int16_t step)
	{
		uint16_t base = regs.ar[r] & ~regs.lm[r];
		regs.ar[r] += step;
		regs.ar[r] = base + (regs.ar[r] & regs.lm[r]);
	}

	#pragma endregion "Multiplier and ALU"

}
