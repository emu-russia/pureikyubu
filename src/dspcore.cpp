#include "pch.h"

// The module handles ALU / multiplier operations and flag setting, as well as other auxiliary operations.

using namespace Debug;

namespace DSP
{

	int64_t DspCore::SignExtend16(int16_t a)
	{
		int64_t res = a;
		if (res & 0x8000)
		{
			res |= 0xffff'ffff'ffff'0000;
		}
		return res;
	}

	int64_t DspCore::SignExtend32(int32_t a)
	{
		int64_t res = a;
		if (res & 0x8000'0000)
		{
			res |= 0xffff'ffff'0000'0000;
		}
		return res;
	}

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

	// The current multiplication result (product) is stored as a set of "temporary" values (ps2, ps1, pc1, ps0).
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

	// Circular addressing logic

	uint16_t DspCore::CircularAddress(uint16_t r, uint16_t l, int16_t m)
	{
		if (m == 0 || l == 0)
		{
			return r;
		}

		if (l == 0xffff)
		{
			return (uint16_t)((int16_t)r + m);
		}
		else
		{
			int16_t abs_m = m > 0 ? m : -m;
			int16_t mm = abs_m % (l + 1);
			uint16_t base = (r / (l + 1)) * (l + 1);
			uint16_t next = 0;
			uint32_t sum = 0;

			if (m > 0)
			{
				sum = (uint32_t)((uint32_t)r + mm);
			}
			else
			{
				sum = (uint32_t)((uint32_t)r + l + 1 - mm);
			}

			next = base + (uint16_t)(sum % (l + 1));

			return next;
		}
	}

	void DspCore::ArAdvance(int r, int16_t step)
	{
		regs.r[r] = CircularAddress(regs.r[r], regs.l[r], step);
	}

#define bit(n,b) (((n) >> (b)) & 1)

	void DspCore::ModifyFlags(uint64_t d, uint64_t s, uint64_t r, CFlagRules cf, VFlagRules vf, ZFlagRules zf, NFlagRules nf, EFlagRules ef, UFlagRules uf)
	{
		// Carry

		switch (cf)
		{
			case CFlagRules::Zero:
				regs.psr.c = 0;
				break;
			case CFlagRules::C1:
				regs.psr.c = (bit(d, 39) & bit(s, 39)) | (~bit(r, 39) & (bit(d, 39) | bit(s, 39)));
				break;
			case CFlagRules::C2:
				regs.psr.c = (bit(d, 39) & ~bit(s, 39)) | (~bit(r, 39) & (bit(d, 39) | ~bit(s, 39)));
				break;
			case CFlagRules::C3:
				regs.psr.c = (bit(d, 39) ^ bit(s, 15)) != 0 ? (bit(d, 39) & bit(s, 15)) | (~bit(r, 39) & (bit(d, 39) | bit(s, 15))) : (bit(d, 39) & ~bit(s, 15)) | (~bit(r, 39) & (bit(d, 39) | ~bit(s, 15)));
				break;
			case CFlagRules::C4:
				regs.psr.c = ~bit(d, 39) & ~bit(r, 39);
				break;
			case CFlagRules::C5:
				regs.psr.c = (bit(d, 39) ^ bit(s, 31)) == 0 ? (bit(d, 39) & bit(s, 39)) | (~bit(r, 39) & (bit(d, 39) | bit(s, 39))) : (bit(d, 39) & ~bit(s, 39)) | (~bit(r, 39) & (bit(d, 39) | ~bit(s, 39)));
				break;
			case CFlagRules::C6:
				regs.psr.c = bit(d, 39) & ~bit(r, 39);
				break;
			case CFlagRules::C7:
				regs.psr.c = bit(d, 39) & ~bit(r, 39);		// Prod
				break;
			case CFlagRules::C8:
				regs.psr.c = (bit(d, 39) & bit(s, 39)) | (~bit(r, 39) & (bit(d, 39) | bit(s, 39)));	// Prod
				break;
		}

		// Overflow

		switch (vf)
		{
			case VFlagRules::Zero:
				regs.psr.v = 0;
				break;
			case VFlagRules::V1:
				regs.psr.v = (bit(d, 39) & bit(s, 39) & ~bit(r, 39)) | (~bit(d, 39) & ~bit(s, 39) & bit(r, 39));
				break;
			case VFlagRules::V2:
				regs.psr.v = (bit(d, 39) & ~bit(s, 39) & ~bit(r, 39)) | (~bit(d, 39) & bit(s, 39) & bit(r, 39));
				break;
			case VFlagRules::V3:
				regs.psr.v = bit(d, 39) & bit(r, 39);
				break;
			case VFlagRules::V4:
				regs.psr.v = ~bit(d, 39) & bit(r, 39);
				break;
			case VFlagRules::V5:
				regs.psr.v = bit(r, 39);
				break;
			case VFlagRules::V6:
				regs.psr.v = ~bit(d, 39) & bit(r, 39);	// Prod
				break;
			case VFlagRules::V7:
				regs.psr.v = (bit(d, 39) & bit(s, 39) & ~bit(r, 39)) | (~bit(d, 39) & ~bit(s, 39) & bit(r, 39));	// Prod
				break;
			case VFlagRules::V8:
				regs.psr.v = bit(d, 39) & ~bit(r, 39);
				break;
		}

		// Sticky overflow

		if (regs.psr.v != 0)
		{
			regs.psr.sv = 1;
		}

		// Zero

		switch (zf)
		{
			case ZFlagRules::Z1:
				regs.psr.z = r == 0;
				break;
			case ZFlagRules::Z2:
				regs.psr.z = (r & 0xffff'0000) == 0;
				break;
			case ZFlagRules::Z3:
				regs.psr.z = (r & 0xff'ffff'ffff) == 0;
				break;
		}

		// Negative

		switch (nf)
		{
			case NFlagRules::N1:
				regs.psr.n = bit(r, 39);
				break;
			case NFlagRules::N2:
				regs.psr.n = bit(r, 31);
				break;
		}

		// Extension (above s32)

		switch (ef)
		{
			case EFlagRules::E1:
			{
				uint64_t ext = (r >> 31) & 0x1ff;
				regs.psr.e = !(ext == 0b0'0000'0000 || ext == 0b1'1111'1111);
				break;
			}
		}

		// Unnormalization

		switch (uf)
		{
			case UFlagRules::U1:
				regs.psr.u = ~(bit(r, 31) ^ bit(r, 30));
				break;
		}
	}

	int64_t DspCore::RndFactor(int64_t d)
	{
		int64_t s;

		uint16_t d0 = d & 0xffff;

		if (d0 < 0x8000)
		{
			s = 0x00'0000'0000;
		}
		else if (d0 > 0x8000)
		{
			s = 0x00'0001'0000;
		}
		else 	// == 0x8000
		{
			if ((d & 0x10000) == 0) 		// lsb of d1
			{
				s = 0x00'0000'0000;
			}
			else
			{
				s = 0x00'0001'0000;
			}
		}

		return s;
	}

}

// Low-level DSP core


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

		for (i; i < (size_t)DspInterrupt::Max; i++)
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
						regs.pcs->clear();
						regs.pss->clear();
						regs.eas->clear();
						regs.lcs->clear();

						regs.pc = Flipper::DSPGetResetModifier() ? IROM_START_ADDRESS : 0;		// IROM start / 0
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
					break;
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
				Halt("DspCore::AssertInterrupt - `Error` counted as non-recoverable\n");
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

	bool DspCore::LoadIrom(std::vector<uint8_t>& iromImage)
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

	bool DspCore::LoadDrom(std::vector<uint8_t>& dromImage)
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
			return _BYTESWAP_UINT16(*(uint16_t*)ptr);
		}

		return 0;
	}

	void DspCore::HardReset()
	{
		Report(Channel::DSP, "DspCore::HardReset\n");

		if (Core != nullptr)
		{
			dsp->savedGekkoTicks = Core->GetTicks();
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
		regs.pc = IROM_START_ADDRESS;
		Report(Channel::DSP, "Hard Reset pc = 0x%04X\n", regs.pc);

		dsp->ResetIfx();

		dsp->Suspend();
	}

	void DspCore::Update()
	{
		uint64_t ticks = Core->GetTicks();

		if (delay_mailbox_reasons) {
			Thread::Sleep(1);
			delay_mailbox_reasons = false;
			return;
		}

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
					Core->Suspend();
					return;
				}

				if (regs.pc == oneShotBreakpoint)
				{
					oneShotBreakpoint = 0xffff;
					dsp->Suspend();
					Core->Suspend();
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

	void DspCore::AddWatch(DspAddress dmemAddress)
	{
		watchesSpinLock.Lock();
		watches.push_back(dmemAddress);
		watchesSpinLock.Unlock();
	}

	void DspCore::RemoveWatch(DspAddress dmemAddress)
	{
		watchesSpinLock.Lock();
		bool found = false;
		for (auto it = watches.begin(); it != watches.end(); ++it)
		{
			if (*it == dmemAddress)
			{
				found = true;
				break;
			}
		}
		if (found)
		{
			watches.remove(dmemAddress);
		}
		watchesSpinLock.Unlock();
	}

	void DspCore::RemoveAllWatches()
	{
		watchesSpinLock.Lock();
		watches.clear();
		watchesSpinLock.Unlock();
	}

	void DspCore::ListWatches(std::list<DspAddress>& watchesOut)
	{
		watchesOut.clear();
		watchesSpinLock.Lock();
		for (auto it = watches.begin(); it != watches.end(); ++it)
		{
			watchesOut.push_back(*it);
		}
		watchesSpinLock.Unlock();
	}

	bool DspCore::TestWatch(DspAddress dmemAddress)
	{
		bool found = false;

		watchesSpinLock.Lock();
		for (auto it = watches.begin(); it != watches.end(); ++it)
		{
			if (*it == dmemAddress)
			{
				found = true;
				break;
			}
		}
		watchesSpinLock.Unlock();

		return found;
	}

	int64_t DspCore::GetInstructionCounter()
	{
		return instructionCounter;
	}

	void DspCore::ResetInstructionCounter()
	{
		resetInstructionCounter = true;
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
				regs.prod.h, regs.prod.m2, regs.prod.m1, regs.prod.l);
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
				Report(Channel::Norm, "l%i: 0x%04X\n", i, regs.l[i]);
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
			default:
				Halt("DspCore::MoveToReg: invalid register index: %i\n", reg);
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
					if (a != (int32_t)a)
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
			default:
				Halt("DspCore::MoveFromReg: invalid register index: %i\n", reg);
				break;
		}
		return val;
	}

#pragma endregion "Register access"

}


// GameCube DSP interpreter

namespace DSP
{
	DspInterpreter::DspInterpreter(DspCore* parent)
	{
		core = parent;
	}

	DspInterpreter::~DspInterpreter()
	{
	}

	void DspInterpreter::FetchMpyParams(DspParameter s1p, DspParameter s2p, int64_t& s1, int64_t& s2, bool checkDp)
	{
		switch (s1p)
		{
			case DspParameter::x0:
				s1 = core->regs.x.l;
				break;
			case DspParameter::x1:
				s1 = core->regs.x.h;
				break;
			case DspParameter::y1:
				s1 = core->regs.y.h;
				break;
			case DspParameter::a1:
				s1 = core->regs.a.m;
				break;
			case DspParameter::b1:
				s1 = core->regs.b.m;
				break;
		}

		switch (s2p)
		{
			case DspParameter::x0:
				s2 = core->regs.x.l;
				break;
			case DspParameter::x1:
				s2 = core->regs.x.h;
				break;
			case DspParameter::y0:
				s2 = core->regs.y.l;
				break;
			case DspParameter::y1:
				s2 = core->regs.y.h;
				break;
		}

		if (core->regs.psr.dp && checkDp)
		{
			if (s1p == DspParameter::x0 && s2p == DspParameter::y1)
			{
				s2 = DspCore::SignExtend16((uint16_t)s2);
			}
			else if (s1p == DspParameter::x1 && s2p == DspParameter::y0)
			{
				s1 = DspCore::SignExtend16((uint16_t)s1);
			}
			else if (s1p == DspParameter::x1 && s2p == DspParameter::y1)
			{
				s1 = DspCore::SignExtend16((uint16_t)s1);
				s2 = DspCore::SignExtend16((uint16_t)s2);
			}
		}

		if (core->regs.psr.im == 0)
		{
			s2 *= 2;
		}
	}

	void DspInterpreter::AdvanceAddress(int r, DspParameter param)
	{
		switch (param)
		{
			case DspParameter::mod_none:
				break;
			case DspParameter::mod_dec:
				core->ArAdvance(r, -1);
				break;
			case DspParameter::mod_inc:
				core->ArAdvance(r, +1);
				break;
			case DspParameter::mod_plus_m0:
				core->ArAdvance(r, core->regs.m[0]);
				break;
			case DspParameter::mod_plus_m1:
				core->ArAdvance(r, core->regs.m[1]);
				break;
			case DspParameter::mod_plus_m2:
				core->ArAdvance(r, core->regs.m[2]);
				break;
			case DspParameter::mod_plus_m3:
				core->ArAdvance(r, core->regs.m[3]);
				break;
			case DspParameter::mod_minus_m:
				core->ArAdvance(r, -core->regs.m[r]);
				break;
			case DspParameter::mod_plus_m:
				core->ArAdvance(r, core->regs.m[r]);
				break;
		}
	}

	bool DspInterpreter::ConditionTrue(ConditionCode cc)
	{
		switch (cc)
		{
			case ConditionCode::ge: return (core->regs.psr.n ^ core->regs.psr.v) == 0;
			case ConditionCode::lt: return (core->regs.psr.n ^ core->regs.psr.v) != 0;
			case ConditionCode::gt: return (core->regs.psr.z | (core->regs.psr.n ^ core->regs.psr.v)) == 0;
			case ConditionCode::le: return (core->regs.psr.z | (core->regs.psr.n ^ core->regs.psr.v)) != 0;
			case ConditionCode::nz: return core->regs.psr.z == 0;
			case ConditionCode::z: return core->regs.psr.z != 0;
			case ConditionCode::nc: return core->regs.psr.c == 0;
			case ConditionCode::c: return core->regs.psr.c != 0;
			case ConditionCode::ne: return core->regs.psr.e == 0;
			case ConditionCode::e: return core->regs.psr.e != 0;
			case ConditionCode::nm: return (core->regs.psr.z | (~core->regs.psr.u & ~core->regs.psr.e)) == 0;
			case ConditionCode::m: return (core->regs.psr.z | (~core->regs.psr.u & ~core->regs.psr.e)) != 0;
			case ConditionCode::nt: return core->regs.psr.tb == 0;
			case ConditionCode::t: return core->regs.psr.tb != 0;
			case ConditionCode::v: return core->regs.psr.v != 0;
			case ConditionCode::always: return true;
		}

		return false;
	}

	void DspInterpreter::Dispatch()
	{
		// A non-flowControl instruction can change the interpreter's internal flag (for example, when trying to access the stack registers with overflow and generating an Error interrupt).

		flowControl = info.flowControl;

		if (!info.parallel)
		{
			switch (info.instr)
			{
				case DspRegularInstruction::jmp: jmp(); break;
				case DspRegularInstruction::call: call(); break;
				case DspRegularInstruction::rets: rets(); break;
				case DspRegularInstruction::reti: reti(); break;
				case DspRegularInstruction::trap: trap(); break;
				case DspRegularInstruction::wait: wait(); break;
				case DspRegularInstruction::exec: exec(); break;
				case DspRegularInstruction::loop: loop(); break;
				case DspRegularInstruction::rep: rep(); break;
				case DspRegularInstruction::pld: pld(); break;
				case DspRegularInstruction::nop:
					break;
				case DspRegularInstruction::mr: mr(); break;
				case DspRegularInstruction::adsi: adsi(); break;
				case DspRegularInstruction::adli: adli(); break;
				case DspRegularInstruction::cmpsi: cmpsi(); break;
				case DspRegularInstruction::cmpli: cmpli(); break;
				case DspRegularInstruction::lsfi: lsfi(); break;
				case DspRegularInstruction::asfi: asfi(); break;
				case DspRegularInstruction::xorli: xorli(); break;
				case DspRegularInstruction::anli: anli(); break;
				case DspRegularInstruction::orli: orli(); break;
				case DspRegularInstruction::norm: norm(); break;
				case DspRegularInstruction::div: div(); break;
				case DspRegularInstruction::addc: addc(); break;
				case DspRegularInstruction::subc: subc(); break;
				case DspRegularInstruction::negc: negc(); break;
				case DspRegularInstruction::max: _max(); break;
				case DspRegularInstruction::lsf: lsf(); break;
				case DspRegularInstruction::asf: asf(); break;
				case DspRegularInstruction::ld: ld(); break;
				case DspRegularInstruction::st: st(); break;
				case DspRegularInstruction::ldsa: ldsa(); break;
				case DspRegularInstruction::stsa: stsa(); break;
				case DspRegularInstruction::ldla: ldla(); break;
				case DspRegularInstruction::stla: stla(); break;
				case DspRegularInstruction::mv: mv(); break;
				case DspRegularInstruction::mvsi: mvsi(); break;
				case DspRegularInstruction::mvli: mvli(); break;
				case DspRegularInstruction::stli: stli(); break;
				case DspRegularInstruction::clr: clr(); break;
				case DspRegularInstruction::set: set(); break;
				case DspRegularInstruction::btstl: btstl(); break;
				case DspRegularInstruction::btsth: btsth(); break;
			}

			core->instructionCounter++;
		}
		else
		{
			switch (info.parallelInstr)
			{
				case DspParallelInstruction::add: p_add(); break;
				case DspParallelInstruction::addl: p_addl(); break;
				case DspParallelInstruction::sub: p_sub(); break;
				case DspParallelInstruction::amv: p_amv(); break;
				case DspParallelInstruction::cmp: p_cmp(); break;
				case DspParallelInstruction::inc: p_inc(); break;
				case DspParallelInstruction::dec: p_dec(); break;
				case DspParallelInstruction::abs: p_abs(); break;
				case DspParallelInstruction::neg: p_neg(); break;
				case DspParallelInstruction::clr: p_clr(); break;
				case DspParallelInstruction::rnd: p_rnd(); break;
				case DspParallelInstruction::rndp: p_rndp(); break;
				case DspParallelInstruction::tst: p_tst(); break;
				case DspParallelInstruction::lsl16: p_lsl16(); break;
				case DspParallelInstruction::lsr16: p_lsr16(); break;
				case DspParallelInstruction::asr16: p_asr16(); break;
				case DspParallelInstruction::addp: p_addp(); break;
				case DspParallelInstruction::nop:
					break;
				case DspParallelInstruction::set: p_set(); break;
				case DspParallelInstruction::mpy: p_mpy(); break;
				case DspParallelInstruction::mac: p_mac(); break;
				case DspParallelInstruction::macn: p_macn(); break;
				case DspParallelInstruction::mvmpy: p_mvmpy(); break;
				case DspParallelInstruction::rnmpy: p_rnmpy(); break;
				case DspParallelInstruction::admpy: p_admpy(); break;
				case DspParallelInstruction::_not: p_not(); break;
				case DspParallelInstruction::_xor: p_xor(); break;
				case DspParallelInstruction::_and: p_and(); break;
				case DspParallelInstruction::_or: p_or(); break;
				case DspParallelInstruction::lsf: p_lsf(); break;
				case DspParallelInstruction::asf: p_asf(); break;
			}

			switch (info.parallelMemInstr)
			{
				case DspParallelMemInstruction::ldd: p_ldd(); break;
				case DspParallelMemInstruction::ls: p_ls(); break;
				case DspParallelMemInstruction::ld: p_ld(); break;
				case DspParallelMemInstruction::st: p_st(); break;
				case DspParallelMemInstruction::mv: p_mv(); break;
				case DspParallelMemInstruction::mr: p_mr(); break;
				case DspParallelMemInstruction::nop:
					break;
			}

			core->instructionCounter += 2;
		}

		if (core->resetInstructionCounter)
		{
			core->resetInstructionCounter = false;
			core->instructionCounter = 0;
		}

		// If there were no control transfers, increase pc by the instruction size

		if (!flowControl)
		{
			// Checking the logic of the `rep` instruction.
			// If the value of the repeat register is not equal to 0, then instead of the usual PC increment, it is not performed.

			if (core->repeatCount)
			{
				core->repeatCount--;
			}

			if (core->repeatCount == 0)
			{
				// Checking the current pc for loop is done only if the eas/lcs stack is not empty

				if (core->regs.pc == core->regs.eas->top() && !core->regs.lcs->empty())
				{
					// If pc is equal to eas then lcs = lcs - 1. 

					uint16_t lc;
					core->regs.lcs->pop(lc);
					core->regs.lcs->push(lc - 1);

					// If after that lcs is not equal to zero, then pc = pcs. Otherwise pop pcs/eas/lcs and pc = pc + 1 (exit the loop)

					if (core->regs.lcs->top() != 0)
					{
						core->regs.pc = core->regs.pcs->top();
					}
					else
					{
						uint16_t dummy;
						core->regs.pcs->pop(dummy);
						core->regs.eas->pop(dummy);
						core->regs.lcs->pop(dummy);

						// The DSP behaves strangely when the last loop instruction is a branch instruction. 
						// The exact work of the DSP in this case is on the verge of unpredictable behavior, so we will not bother and complicate the code. 
						// All the same, microcode developers are adequate people and will never deal with placing branch instructions at the end of a loop.

						core->regs.pc += 1;
					}
				}
				else
				{
					core->regs.pc += (DspAddress)(info.sizeInBytes >> 1);
				}
			}
		}
	}

	void DspInterpreter::ExecuteInstr()
	{
		// Fetch, decode and dispatch instruction at pc addr

		DspAddress imemAddr = core->regs.pc;

		uint8_t* imemPtr = core->TranslateIMem(imemAddr);
		if (imemPtr == nullptr)
		{
			Halt("DSP TranslateIMem failed on dsp addr: 0x%04X\n", imemAddr);
			core->dsp->Suspend();
			return;
		}

		Decoder::Decode(imemPtr, DspCore::MaxInstructionSizeInBytes, info);

		Dispatch();
	}

}


// DSP multiply instructions

// The PSR flags are set relative to operations on the ALU, not on the multiplier (the multiplier is a separate circuit without flags). Therefore, only the instructions `mvmpy`,` rnmpy` and `admpy` change flags.

namespace DSP
{

	void DspInterpreter::p_mpy()
	{
		int64_t s1 = 0;
		int64_t s2 = 0;

		FetchMpyParams(info.params[0], info.params[1], s1, s2, true);

		core->regs.prod.bitsPacked = s1 * s2;
		core->UnpackProd(core->regs.prod);
	}

	void DspInterpreter::p_mac()
	{
		int64_t s1 = 0;
		int64_t s2 = 0;

		FetchMpyParams(info.params[0], info.params[1], s1, s2, false);

		core->PackProd(core->regs.prod);
		core->regs.prod.bitsPacked = s1 * s2 + DspCore::SignExtend40(core->regs.prod.bitsPacked);
		core->UnpackProd(core->regs.prod);
	}

	void DspInterpreter::p_macn()
	{
		int64_t s1 = 0;
		int64_t s2 = 0;

		FetchMpyParams(info.params[0], info.params[1], s1, s2, false);

		core->PackProd(core->regs.prod);
		core->regs.prod.bitsPacked = -s1 * s2 + DspCore::SignExtend40(core->regs.prod.bitsPacked);
		core->UnpackProd(core->regs.prod);
	}

	void DspInterpreter::p_mvmpy()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		int64_t s1 = 0;
		int64_t s2 = 0;

		FetchMpyParams(info.params[1], info.params[2], s1, s2, true);

		core->PackProd(core->regs.prod);
		d = DspCore::SignExtend40(core->regs.prod.bitsPacked);
		r = d + s;

		switch (info.params[0])
		{
		case DspParameter::a:
			core->regs.a.bits = r;
			break;
		case DspParameter::b:
			core->regs.b.bits = r;
			break;
		}

		core->regs.prod.bitsPacked = s1 * s2;
		core->UnpackProd(core->regs.prod);

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_rnmpy()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		int64_t s1 = 0;
		int64_t s2 = 0;

		FetchMpyParams(info.params[1], info.params[2], s1, s2, true);

		core->PackProd(core->regs.prod);
		d = DspCore::SignExtend40(core->regs.prod.bitsPacked);
		s = DspCore::RndFactor(d);

		r = d + s;
		r &= ~0xffff;

		switch (info.params[0])
		{
		case DspParameter::a:
			core->regs.a.bits = r;
			break;
		case DspParameter::b:
			core->regs.b.bits = r;
			break;
		}

		core->regs.prod.bitsPacked = s1 * s2;
		core->UnpackProd(core->regs.prod);

		core->ModifyFlags(d, s, r, CFlagRules::C7, VFlagRules::V6, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_admpy()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		int64_t s1 = 0;
		int64_t s2 = 0;

		FetchMpyParams(info.params[1], info.params[2], s1, s2, true);

		switch (info.params[0])
		{
			case DspParameter::a:
				s = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				s = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		core->PackProd(core->regs.prod);
		d = DspCore::SignExtend40(core->regs.prod.bitsPacked);
		r = d + s;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->regs.prod.bitsPacked = s1 * s2;
		core->UnpackProd(core->regs.prod);

		core->ModifyFlags(d, s, r, CFlagRules::C1, VFlagRules::V1, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

}


// DSP parallel load/store/move instructions

namespace DSP
{

	// ldd d1,rn,mn d2,r3,m3
	void DspInterpreter::p_ldd()
	{
		int r = (int)info.paramsEx[1];
		core->MoveToReg((int)info.paramsEx[0], core->dsp->ReadDMem(core->regs.r[r]));
		AdvanceAddress(r, info.paramsEx[2]);

		r = (int)info.paramsEx[4];
		core->MoveToReg((int)info.paramsEx[3], core->dsp->ReadDMem(core->regs.r[r]));
		AdvanceAddress(r, info.paramsEx[5]);
	}

	// ls d,r,m r,m,s
	void DspInterpreter::p_ls()
	{
		int r = (int)info.paramsEx[1];
		core->MoveToReg((int)info.paramsEx[0], core->dsp->ReadDMem(core->regs.r[r]));
		AdvanceAddress(r, info.paramsEx[2]);

		r = (int)info.paramsEx[3];
		core->dsp->WriteDMem(core->regs.r[r], core->MoveFromReg((int)info.paramsEx[5]));
		AdvanceAddress(r, info.paramsEx[4]);
	}

	// ld d,rn,mn
	void DspInterpreter::p_ld()
	{
		int r = (int)info.paramsEx[1];
		core->MoveToReg((int)info.paramsEx[0], core->dsp->ReadDMem(core->regs.r[r]));
		AdvanceAddress(r, info.paramsEx[2]);
	}

	// st rn,mn,s
	void DspInterpreter::p_st()
	{
		int r = (int)info.paramsEx[0];
		core->dsp->WriteDMem(core->regs.r[r], core->MoveFromReg((int)info.paramsEx[2]));
		AdvanceAddress(r, info.paramsEx[1]);
	}

	// mv d,s
	void DspInterpreter::p_mv()
	{
		core->MoveToReg((int)info.paramsEx[0], core->MoveFromReg((int)info.paramsEx[1]));
	}

	// mr rn,mn
	void DspInterpreter::p_mr()
	{
		int r = (int)info.paramsEx[0];
		AdvanceAddress(r, info.paramsEx[1]);
	}

}


// All parallel instructions except multiplication.

namespace DSP
{

	void DspInterpreter::p_add()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::x0:
				s = DspCore::SignExtend16(core->regs.x.l) << 16;
				break;
			case DspParameter::y0:
				s = DspCore::SignExtend16(core->regs.y.l) << 16;
				break;
			case DspParameter::x1:
				s = DspCore::SignExtend16(core->regs.x.h) << 16;
				break;
			case DspParameter::y1:
				s = DspCore::SignExtend16(core->regs.y.h) << 16;
				break;
			case DspParameter::x:
				s = DspCore::SignExtend32(core->regs.x.bits);
				break;
			case DspParameter::y:
				s = DspCore::SignExtend32(core->regs.y.bits);
				break;
			case DspParameter::a:
				s = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				s = DspCore::SignExtend40(core->regs.b.bits);
				break;
			case DspParameter::prod:
				core->PackProd(core->regs.prod);
				s = DspCore::SignExtend40(core->regs.prod.bitsPacked);
				break;
		}

		r = d + s;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C1, VFlagRules::V1, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_addl()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::x0:
				s = core->regs.x.l;
				break;
			case DspParameter::y0:
				s = core->regs.y.l;
				break;
		}

		r = d + s;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C6, VFlagRules::V4, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_sub()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::x0:
				s = DspCore::SignExtend16(core->regs.x.l) << 16;
				break;
			case DspParameter::y0:
				s = DspCore::SignExtend16(core->regs.y.l) << 16;
				break;
			case DspParameter::x1:
				s = DspCore::SignExtend16(core->regs.x.h) << 16;
				break;
			case DspParameter::y1:
				s = DspCore::SignExtend16(core->regs.y.h) << 16;
				break;
			case DspParameter::x:
				s = DspCore::SignExtend32(core->regs.x.bits);
				break;
			case DspParameter::y:
				s = DspCore::SignExtend32(core->regs.y.bits);
				break;
			case DspParameter::a:
				s = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				s = DspCore::SignExtend40(core->regs.b.bits);
				break;
			case DspParameter::prod:
				core->PackProd(core->regs.prod);
				s = DspCore::SignExtend40(core->regs.prod.bitsPacked);
				break;
		}

		r = d - s;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C2, VFlagRules::V2, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_amv()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[1])
		{
			case DspParameter::x0:
				s = DspCore::SignExtend16(core->regs.x.l) << 16;
				break;
			case DspParameter::y0:
				s = DspCore::SignExtend16(core->regs.y.l) << 16;
				break;
			case DspParameter::x1:
				s = DspCore::SignExtend16(core->regs.x.h) << 16;
				break;
			case DspParameter::y1:
				s = DspCore::SignExtend16(core->regs.y.h) << 16;
				break;
			case DspParameter::x:
				s = DspCore::SignExtend32(core->regs.x.bits);
				break;
			case DspParameter::y:
				s = DspCore::SignExtend32(core->regs.y.bits);
				break;
			case DspParameter::a:
				s = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				s = DspCore::SignExtend40(core->regs.b.bits);
				break;
			case DspParameter::prod:
				core->PackProd(core->regs.prod);
				s = DspCore::SignExtend40(core->regs.prod.bitsPacked);
				break;
		}

		r = d + s;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_cmp()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::x1:
				s = DspCore::SignExtend16(core->regs.x.h) << 16;
				break;
			case DspParameter::y1:
				s = DspCore::SignExtend16(core->regs.y.h) << 16;
				break;
			case DspParameter::a:
				s = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				s = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		r = d - s;

		core->ModifyFlags(d, s, r, CFlagRules::C2, VFlagRules::V2, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_inc()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
			case DspParameter::a1:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
			case DspParameter::b1:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[0])
		{
			case DspParameter::a:
			case DspParameter::b:
				s = 1;
				break;
			case DspParameter::a1:
			case DspParameter::b1:
				s = 0x10000;
				break;
		}

		r = d + s;

		switch (info.params[0])
		{
			case DspParameter::a:
			case DspParameter::a1:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
			case DspParameter::b1:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C6, VFlagRules::V4, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_dec()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
			case DspParameter::a1:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
			case DspParameter::b1:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[0])
		{
			case DspParameter::a:
			case DspParameter::b:
				s = 1;
				break;
			case DspParameter::a1:
			case DspParameter::b1:
				s = 0x10000;
				break;
		}

		r = d - s;

		switch (info.params[0])
		{
			case DspParameter::a:
			case DspParameter::a1:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
			case DspParameter::b1:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C4, VFlagRules::V8, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_abs()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				s = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				s = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		if ((s & 0x80'0000'0000) == 0)
		{
			r = d + s;
		}
		else
		{
			r = d - s;
		}

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::V5, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_neg()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		if (info.numParameters == 2)
		{
			switch (info.params[1])
			{
				case DspParameter::prod:
					core->PackProd(core->regs.prod);
					s = DspCore::SignExtend40(core->regs.prod.bitsPacked);
					break;
			}
		}
		else
		{
			switch (info.params[0])
			{
				case DspParameter::a:
					s = DspCore::SignExtend40(core->regs.a.bits);
					break;
				case DspParameter::b:
					s = DspCore::SignExtend40(core->regs.b.bits);
					break;
			}
		}

		r = d - s;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C4, VFlagRules::V3, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_clr()
	{
		switch (info.params[0])
		{
			case DspParameter::a: core->regs.a.bits = 0; break;
			case DspParameter::b: core->regs.b.bits = 0; break;

			case DspParameter::prod:
				core->regs.prod.l = 0x0000;
				core->regs.prod.m1 = 0xfff0;
				core->regs.prod.h = 0x00ff;
				core->regs.prod.m2 = 0x0010;
				break;

			case DspParameter::psr_im: core->regs.psr.im = 0; break;
			case DspParameter::psr_dp: core->regs.psr.dp = 0; break;
			case DspParameter::psr_xl: core->regs.psr.xl = 0; break;
		}
	}

	void DspInterpreter::p_rnd()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		s = DspCore::RndFactor(d);

		r = d + s;
		r &= ~0xffff;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}
		
		core->ModifyFlags(d, s, r, CFlagRules::C6, VFlagRules::V4, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_rndp()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		core->PackProd(core->regs.prod);
		d = DspCore::SignExtend40(core->regs.prod.bitsPacked);
		s = DspCore::RndFactor(d);

		r = d + s;
		r &= ~0xffff;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C7, VFlagRules::V6, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_tst()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;
		
		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				s = 0;
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				s = 0;
				break;
			case DspParameter::x1:
				d = DspCore::SignExtend16(core->regs.x.h) << 16;
				s = 0;
				break;
			case DspParameter::y1:
				d = DspCore::SignExtend16(core->regs.y.h) << 16;
				s = 0;
				break;
			case DspParameter::prod:
			{
				core->PackProd(core->regs.prod);
				d = ((uint64_t)core->regs.prod.h << 32) | ((uint64_t)core->regs.prod.m1 << 16) | core->regs.prod.l;
				s = ((uint64_t)core->regs.prod.m2 << 16);
				break;
			}
		}

		r = d + s;

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_lsl16()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = core->regs.a.bits & 0x0000'00ff'ffff'ffff;
				break;
			case DspParameter::b:
				d = core->regs.b.bits & 0x0000'00ff'ffff'ffff;
				break;
		}

		r = (uint64_t)d << 16;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}
		
		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_lsr16()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = core->regs.a.bits & 0x0000'00ff'ffff'ffff;
				break;
			case DspParameter::b:
				d = core->regs.b.bits & 0x0000'00ff'ffff'ffff;
				break;
		}

		r = (uint64_t)d >> 16;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_asr16()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		r = d >> 16;		// Arithmetic

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_addp()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[1])
		{
			case DspParameter::x1:
				d = DspCore::SignExtend16(core->regs.x.h) << 16;
				break;
			case DspParameter::y1:
				d = DspCore::SignExtend16(core->regs.y.h) << 16;
				break;
		}

		core->PackProd(core->regs.prod);
		s = DspCore::SignExtend32((uint32_t)core->regs.prod.bitsPacked);

		r = d + s;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C8, VFlagRules::V7, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_set()
	{
		switch (info.params[0])
		{
			case DspParameter::psr_im: core->regs.psr.im = 1; break;
			case DspParameter::psr_dp: core->regs.psr.dp = 1; break;
			case DspParameter::psr_xl: core->regs.psr.xl = 1; break;
		}
	}

	void DspInterpreter::p_not()
	{
		uint16_t d = 0;
		uint16_t s = 0;
		uint16_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a1:
				d = core->regs.a.m;
				break;
			case DspParameter::b1:
				d = core->regs.b.m;
				break;
		}

		r = ~d;

		switch (info.params[0])
		{
			case DspParameter::a1:
				core->regs.a.m = r;
				break;
			case DspParameter::b1:
				core->regs.b.m = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_xor()
	{
		uint16_t d = 0;
		uint16_t s = 0;
		uint16_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a1:
				d = core->regs.a.m;
				break;
			case DspParameter::b1:
				d = core->regs.b.m;
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::a1:
				s = core->regs.a.m;
				break;
			case DspParameter::b1:
				s = core->regs.b.m;
				break;
			case DspParameter::x1:
				s = core->regs.x.h;
				break;
			case DspParameter::y1:
				s = core->regs.y.h;
				break;
		}

		r = d ^ s;

		switch (info.params[0])
		{
			case DspParameter::a1:
				core->regs.a.m = r;
				break;
			case DspParameter::b1:
				core->regs.b.m = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_and()
	{
		uint16_t d = 0;
		uint16_t s = 0;
		uint16_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a1:
				d = core->regs.a.m;
				break;
			case DspParameter::b1:
				d = core->regs.b.m;
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::a1:
				s = core->regs.a.m;
				break;
			case DspParameter::b1:
				s = core->regs.b.m;
				break;
			case DspParameter::x1:
				s = core->regs.x.h;
				break;
			case DspParameter::y1:
				s = core->regs.y.h;
				break;
		}

		r = d & s;

		switch (info.params[0])
		{
			case DspParameter::a1:
				core->regs.a.m = r;
				break;
			case DspParameter::b1:
				core->regs.b.m = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_or()
	{
		uint16_t d = 0;
		uint16_t s = 0;
		uint16_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a1:
				d = core->regs.a.m;
				break;
			case DspParameter::b1:
				d = core->regs.b.m;
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::a1:
				s = core->regs.a.m;
				break;
			case DspParameter::b1:
				s = core->regs.b.m;
				break;
			case DspParameter::x1:
				s = core->regs.x.h;
				break;
			case DspParameter::y1:
				s = core->regs.y.h;
				break;
		}

		r = d | s;

		switch (info.params[0])
		{
			case DspParameter::a1:
				core->regs.a.m = r;
				break;
			case DspParameter::b1:
				core->regs.b.m = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_lsf()
	{
		int64_t d = 0;
		int16_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = core->regs.a.bits & 0x0000'00ff'ffff'ffff;
				break;
			case DspParameter::b:
				d = core->regs.b.bits & 0x0000'00ff'ffff'ffff;
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::x1:
				s = core->regs.x.h;
				break;
			case DspParameter::y1:
				s = core->regs.y.h;
				break;
			case DspParameter::a1:
				s = core->regs.a.m;
				break;
			case DspParameter::b1:
				s = core->regs.b.m;
				break;
		}

		if (s < 0)
		{
			r = (uint64_t)d << (~s + 1);
		}
		else
		{
			r = (uint64_t)d >> s;
		}

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_asf()
	{
		int64_t d = 0;
		int16_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::x1:
				s = core->regs.x.h;
				break;
			case DspParameter::y1:
				s = core->regs.y.h;
				break;
			case DspParameter::a1:
				s = core->regs.a.m;
				break;
			case DspParameter::b1:
				s = core->regs.b.m;
				break;
		}

		if (s < 0)
		{
			r = d << (~s + 1);
		}
		else
		{
			r = d >> s;		// Arithmetic
		}

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

}

// Non-parallel instructions.

namespace DSP
{

	void DspInterpreter::jmp()
	{
		DspAddress address;

		if (info.params[0] == DspParameter::Address)
		{
			address = info.ImmOperand.Address;
		}
		else
		{
			address = core->MoveFromReg((int)info.params[0]);
		}

		if (ConditionTrue(info.cc))
		{
			if (core->dsp->logNonconditionalCallJmp && info.cc == ConditionCode::always)
			{
				Report(Channel::DSP, "0x%04X: jmp 0x%04X\n", core->regs.pc, address);
			}

			core->regs.pc = address;
		}
		else
		{
			core->regs.pc += (DspAddress)(info.sizeInBytes >> 1);
		}
	}

	void DspInterpreter::call()
	{
		DspAddress address;

		if (info.params[0] == DspParameter::Address)
		{
			address = info.ImmOperand.Address;
		}
		else
		{
			address = core->MoveFromReg((int)info.params[0]);
		}

		if (ConditionTrue(info.cc))
		{
			if (core->dsp->logNonconditionalCallJmp && info.cc == ConditionCode::always)
			{
				Report(Channel::DSP, "0x%04X: call 0x%04X\n", core->regs.pc, address);
			}

			if (core->regs.pcs->push((uint16_t)(core->regs.pc + (info.sizeInBytes >> 1))))
			{
				core->regs.pc = address;
			}
			else
			{
				core->AssertInterrupt(DspInterrupt::Error);
			}
		}
		else
		{
			core->regs.pc += (DspAddress)(info.sizeInBytes >> 1);
		}
	}

	void DspInterpreter::rets()
	{
		if (ConditionTrue(info.cc))
		{
			uint16_t pc;

			if (core->regs.pcs->pop(pc))
			{
				core->regs.pc = pc;
			}
			else
			{
				core->AssertInterrupt(DspInterrupt::Error);
			}
		}
		else
		{
			core->regs.pc++;
		}
	}

	void DspInterpreter::reti()
	{
		if (ConditionTrue(info.cc))
		{
			core->ReturnFromInterrupt();
		}
		else
		{
			core->regs.pc++;
		}
	}

	void DspInterpreter::trap()
	{
		core->AssertInterrupt(DspInterrupt::Trap);
	}

	void DspInterpreter::wait()
	{
		// In a real DSP, Clk is disabled and only the interrupt generation circuitry remains active. 
		// In the emulator, due to the fact that the instruction is flowControl, pc changes will not occur and the emulated DSP will "hang" on the execution of the `wait` instruction until an interrupt occurs.
	}

	void DspInterpreter::exec()
	{
		if (ConditionTrue(info.cc))
		{
			core->regs.pc++;
		}
		else
		{
			core->regs.pc += 2;
		}
	}

	void DspInterpreter::loop()
	{
		int lc;
		DspAddress end_addr;

		if (info.params[0] == DspParameter::Byte)
		{
			lc = info.ImmOperand.Byte;
			end_addr = info.ImmOperand2.Address;
		}
		else
		{
			lc = core->MoveFromReg((int)info.params[0]);
			end_addr = info.ImmOperand.Address;
		}

		if (lc != 0)
		{
			if (!core->regs.pcs->push(core->regs.pc + 2))
			{
				core->AssertInterrupt(DspInterrupt::Error);
				return;
			}
			if (!core->regs.lcs->push(lc))
			{
				core->AssertInterrupt(DspInterrupt::Error);
				return;
			}
			if (!core->regs.eas->push(end_addr))
			{
				core->AssertInterrupt(DspInterrupt::Error);
				return;
			}
			core->regs.pc += 2;
		}
		else
		{
			// If the parameter lc = 0, then accordingly no loop occurs, pc = end_address + 1 (the block is skipped)

			core->regs.pc = end_addr + 1;
		}
	}

	void DspInterpreter::rep()
	{
		int rc;

		if (info.params[0] == DspParameter::Byte)
		{
			rc = info.ImmOperand.Byte;
		}
		else
		{
			rc = core->MoveFromReg((int)info.params[0]);
		}

		if (rc != 0)
		{
			core->repeatCount = rc;
			core->regs.pc++;
		}
		else
		{
			core->regs.pc += 2;		// Skip next 1-cycle instruction
		}
	}

	void DspInterpreter::pld()
	{
		int r = (int)info.params[1];
		core->MoveToReg((int)info.params[0], core->ReadIMem(core->regs.r[r]) );
		AdvanceAddress(r, info.params[2]);
	}

	void DspInterpreter::mr()
	{
		int r = (int)info.params[0];
		AdvanceAddress(r, info.params[1]);
	}

	void DspInterpreter::adsi()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		s = DspCore::SignExtend16((int16_t)info.ImmOperand.SignedByte) << 16;

		r = d + s;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C1, VFlagRules::V1, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::adli()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		s = DspCore::SignExtend16(info.ImmOperand.UnsignedShort) << 16;

		r = d + s;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C1, VFlagRules::V1, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::cmpsi()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		s = DspCore::SignExtend16((int16_t)info.ImmOperand.SignedByte) << 16;

		r = d - s;

		core->ModifyFlags(d, s, r, CFlagRules::C2, VFlagRules::V2, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::cmpli()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		s = DspCore::SignExtend16(info.ImmOperand.UnsignedShort) << 16;

		r = d - s;

		core->ModifyFlags(d, s, r, CFlagRules::C2, VFlagRules::V2, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::lsfi()
	{
		int64_t d = 0;
		uint16_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = core->regs.a.bits & 0x0000'00ff'ffff'ffff;
				break;
			case DspParameter::b:
				d = core->regs.b.bits & 0x0000'00ff'ffff'ffff;
				break;
		}

		s = (int16_t)info.ImmOperand.SignedByte;

		if (s & 0x8000)
		{
			r = (uint64_t)d >> (~s + 1);
		}
		else
		{
			r = (uint64_t)d << s;
		}

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::asfi()
	{
		int64_t d = 0;
		uint16_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		s = (int16_t)info.ImmOperand.SignedByte;

		if (s & 0x8000)
		{
			r = d >> (~s + 1);
		}
		else
		{
			r = d << s;		// Arithmetic
		}

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::xorli()
	{
		uint16_t d = 0;
		uint16_t s = info.ImmOperand.UnsignedShort;
		uint16_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a1:
				d = core->regs.a.m;
				break;
			case DspParameter::b1:
				d = core->regs.b.m;
				break;
		}

		r = d ^ s;

		switch (info.params[0])
		{
			case DspParameter::a1:
				core->regs.a.m = r;
				break;
			case DspParameter::b1:
				core->regs.b.m = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::anli()
	{
		uint16_t d = 0;
		uint16_t s = info.ImmOperand.UnsignedShort;
		uint16_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a1:
				d = core->regs.a.m;
				break;
			case DspParameter::b1:
				d = core->regs.b.m;
				break;
		}

		r = d & s;

		switch (info.params[0])
		{
			case DspParameter::a1:
				core->regs.a.m = r;
				break;
			case DspParameter::b1:
				core->regs.b.m = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::orli()
	{
		uint16_t d = 0;
		uint16_t s = info.ImmOperand.UnsignedShort;
		uint16_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a1:
				d = core->regs.a.m;
				break;
			case DspParameter::b1:
				d = core->regs.b.m;
				break;
		}

		r = d | s;

		switch (info.params[0])
		{
			case DspParameter::a1:
				core->regs.a.m = r;
				break;
			case DspParameter::b1:
				core->regs.b.m = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::norm()
	{
		Halt("DspInterpreter::norm\n");
	}

	void DspInterpreter::div()
	{
		Halt("DspInterpreter::div\n");
	}

	void DspInterpreter::addc()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::x:
				s = DspCore::SignExtend32(core->regs.x.bits);
				break;
			case DspParameter::y:
				s = DspCore::SignExtend32(core->regs.y.bits);
				break;
		}

		r = d + s + core->regs.psr.c;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C1, VFlagRules::V1, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::subc()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::x:
				s = DspCore::SignExtend32(core->regs.x.bits);
				break;
			case DspParameter::y:
				s = DspCore::SignExtend32(core->regs.y.bits);
				break;
		}

		r = d + (int64_t)(~s) + core->regs.psr.c;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C2, VFlagRules::V2, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::negc()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				s = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				s = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		r = d + (int64_t)(~s) + core->regs.psr.c;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C4, VFlagRules::V3, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::_max()
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::x0:
				s = DspCore::SignExtend16(core->regs.x.l) << 16;
				break;
			case DspParameter::y0:
				s = DspCore::SignExtend16(core->regs.y.l) << 16;
				break;
			case DspParameter::x1:
				s = DspCore::SignExtend16(core->regs.x.h) << 16;
				break;
			case DspParameter::y1:
				s = DspCore::SignExtend16(core->regs.y.h) << 16;
				break;
		}

		// abs
		if (d < 0) d = -d;
		if (s < 0) s = -s;

		r = d - s;

		core->ModifyFlags(d, s, r, CFlagRules::C5, VFlagRules::Zero, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::lsf()
	{
		int64_t d = 0;
		int16_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = core->regs.a.bits & 0x0000'00ff'ffff'ffff;
				break;
			case DspParameter::b:
				d = core->regs.b.bits & 0x0000'00ff'ffff'ffff;
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::x1:
				s = core->regs.x.h;
				break;
			case DspParameter::y1:
				s = core->regs.y.h;
				break;
			case DspParameter::a1:
				s = core->regs.a.m;
				break;
			case DspParameter::b1:
				s = core->regs.b.m;
				break;
		}

		if (-s < 0)
		{
			r = (uint64_t)d >> s;
		}
		else
		{
			r = (uint64_t)d << (~s + 1);
		}

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::asf()
	{
		int64_t d = 0;
		int16_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::x1:
				s = core->regs.x.h;
				break;
			case DspParameter::y1:
				s = core->regs.y.h;
				break;
			case DspParameter::a1:
				s = core->regs.a.m;
				break;
			case DspParameter::b1:
				s = core->regs.b.m;
				break;
		}

		if (-s < 0)
		{
			r = d >> s;		// Arithmetic
		}
		else
		{
			r = d << (~s + 1);
		}

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::ld()
	{
		int r = (int)info.params[1];
		core->MoveToReg((int)info.params[0], core->dsp->ReadDMem(core->regs.r[r]) );
		AdvanceAddress(r, info.params[2]);
	}

	void DspInterpreter::st()
	{
		int r = (int)info.params[0];
		core->dsp->WriteDMem(core->regs.r[r], core->MoveFromReg((int)info.params[2]) );
		AdvanceAddress(r, info.params[1]);
	}

	void DspInterpreter::ldsa()
	{
		core->MoveToReg((int)info.params[0], 
			core->dsp->ReadDMem( (DspAddress)((core->regs.dpp << 8) | (info.ImmOperand.Address))) );
	}

	void DspInterpreter::stsa()
	{
		uint16_t s;

		switch (info.params[1])
		{
			case DspParameter::a2:
				s = core->regs.a.h & 0xff;
				if (s & 0x80) s |= 0xff00;
				break;
			case DspParameter::b2:
				s = core->regs.b.h & 0xff;
				if (s & 0x80) s |= 0xff00;
				break;
			default:
				s = core->MoveFromReg((int)info.params[1]);
				break;
		}

		core->dsp->WriteDMem((DspAddress)((core->regs.dpp << 8) | (info.ImmOperand.Address)), s);
	}

	void DspInterpreter::ldla()
	{
		core->MoveToReg((int)info.params[0], core->dsp->ReadDMem(info.ImmOperand.Address) );
	}

	void DspInterpreter::stla()
	{
		core->dsp->WriteDMem(info.ImmOperand.Address, core->MoveFromReg((int)info.params[1]) );
	}

	void DspInterpreter::mv()
	{
		core->MoveToReg((int)info.params[0], core->MoveFromReg((int)info.params[1]));
	}

	void DspInterpreter::mvsi()
	{
		core->MoveToReg((int)info.params[0], (int16_t)info.ImmOperand.SignedByte);
	}

	void DspInterpreter::mvli()
	{
		core->MoveToReg((int)info.params[0], info.ImmOperand.UnsignedShort);
	}

	void DspInterpreter::stli()
	{
		core->dsp->WriteDMem(info.ImmOperand.Address, info.ImmOperand2.UnsignedShort);
	}

	void DspInterpreter::clr()
	{
		switch (info.params[0])
		{
			case DspParameter::psr_tb: core->regs.psr.tb = 0; break;
			case DspParameter::psr_sv: core->regs.psr.sv = 0; break;
			case DspParameter::psr_te0: core->regs.psr.te0 = 0; break;
			case DspParameter::psr_te1: core->regs.psr.te1 = 0; break;
			case DspParameter::psr_te2: core->regs.psr.te2 = 0; break;
			case DspParameter::psr_te3: core->regs.psr.te3 = 0; break;
			case DspParameter::psr_et: core->regs.psr.et = 0; break;

			default:
				Halt("DspInterpreter::clr: Invalid parameter\n");
		}
	}

	void DspInterpreter::set()
	{
		switch (info.params[0])
		{
			case DspParameter::psr_tb: core->regs.psr.tb = 1; break;
			case DspParameter::psr_sv: core->regs.psr.sv = 1; break;
			case DspParameter::psr_te0: core->regs.psr.te0 = 1; break;
			case DspParameter::psr_te1: core->regs.psr.te1 = 1; break;
			case DspParameter::psr_te2: core->regs.psr.te2 = 1; break;
			case DspParameter::psr_te3: core->regs.psr.te3 = 1; break;
			case DspParameter::psr_et: core->regs.psr.et = 1; break;

			default:
				Halt("DspInterpreter::set: Invalid parameter\n");
		}
	}

	void DspInterpreter::btstl()
	{
		uint16_t val = 0;

		switch (info.params[0])
		{
			case DspParameter::a1:
				val = core->regs.a.m;
				break;
			case DspParameter::b1:
				val = core->regs.b.m;
				break;

			default:
				Halt("DspInterpreter::btstl: Invalid parameter\n");
		}

		core->regs.psr.tb = (val & info.ImmOperand.UnsignedShort) == 0;
	}
	
	void DspInterpreter::btsth()
	{
		uint16_t val = 0;

		switch (info.params[0])
		{
			case DspParameter::a1:
				val = core->regs.a.m;
				break;
			case DspParameter::b1:
				val = core->regs.b.m;
				break;

			default:
				Halt("DspInterpreter::btsth: Invalid parameter\n");
		}

		core->regs.psr.tb = (val & info.ImmOperand.UnsignedShort) == info.ImmOperand.UnsignedShort;
	}

}


// DSPcore contains a built-in stack implementation.

namespace DSP
{

	DspStack::DspStack(size_t _depth)
	{
		depth = (int)_depth;
		stack = new uint16_t[depth];
	}

	DspStack::~DspStack()
	{
		delete[] stack;
	}

	bool DspStack::push(uint16_t val)
	{
		if (ptr >= depth)
			return false;	// Overflow

		stack[ptr++] = val;
		return true;
	}

	bool DspStack::pop(uint16_t& val)
	{
		if (ptr == 0)
			return false;	// Underflow

		val = stack[--ptr];
		return true;
	}

	uint16_t DspStack::top()
	{
		if (ptr == 0)
			return 0xffff;

		return stack[ptr - 1];
	}

	uint16_t DspStack::at(int pos)
	{
		return stack[pos];
	}

	bool DspStack::empty()
	{
		return ptr == 0;
	}

	int DspStack::size()
	{
		return ptr;
	}

	void DspStack::clear()
	{
		ptr = 0;
	}

}
