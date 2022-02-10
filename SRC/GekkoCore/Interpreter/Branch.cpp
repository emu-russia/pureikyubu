// Branch Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

	// info.Imm.Address is always pre-calculated, there is no need to perform operations on the `pc`, just write a new address there. 

	#define BO(n)       ((bo >> (4-n)) & 1)

	void Interpreter::BranchCheck()
	{
		if (core->intFlag && (core->regs.msr & MSR_EE))
		{
			core->Exception(Gekko::Exception::INTERRUPT);
			core->exception = false;
			return;
		}

		// modify CPU counters (possible CPU_EXCEPTION_DECREMENTER)
		core->Tick();
		if (core->decreq && (core->regs.msr & MSR_EE))
		{
			core->decreq = false;
			core->Exception(Gekko::Exception::DECREMENTER);
		}
	}

	// PC = PC + EXTS(LI || 0b00)
	void Interpreter::b()
	{
		core->regs.pc = info.Imm.Address;
		BranchCheck();
	}

	// PC = EXTS(LI || 0b00)
	void Interpreter::ba()
	{
		core->regs.pc = info.Imm.Address;
		BranchCheck();
	}

	// LR = PC + 4, PC = PC + EXTS(LI || 0b00)
	void Interpreter::bl()
	{
		core->regs.spr[SPR::LR] = core->regs.pc + 4;
		core->regs.pc = info.Imm.Address;
		BranchCheck();
	}

	// LR = PC + 4, PC = EXTS(LI || 0b00)
	void Interpreter::bla()
	{
		core->regs.spr[SPR::LR] = core->regs.pc + 4;
		core->regs.pc = info.Imm.Address;
		BranchCheck();
	}

	// calculation of conditional branch
	bool Interpreter::BcTest()
	{
		bool ctr_ok, cond_ok;
		int bo = info.paramBits[0], bi = info.paramBits[1];

		if (BO(2) == 0)
		{
			core->regs.spr[SPR::CTR]--;

			if (BO(3)) ctr_ok = (core->regs.spr[SPR::CTR] == 0);
			else ctr_ok = (core->regs.spr[SPR::CTR] != 0);
		}
		else ctr_ok = true;

		if (BO(0) == 0)
		{
			if (BO(1)) cond_ok = ((core->regs.cr << bi) & 0x80000000) != 0;
			else cond_ok = ((core->regs.cr << bi) & 0x80000000) == 0;
		}
		else cond_ok = true;

		return (ctr_ok && cond_ok);
	}

	// if ~BO2 then CTR = CTR - 1
	// ctr_ok  = BO2 | ((CTR != 0) ^ BO3)
	// cond_ok = BO0 | (CR[BI] EQV BO1)
	// if ctr_ok & cond_ok then
	//      if LK = 1
	//          LR = PC + 4
	//      if AA = 1
	//          then PC = EXTS(BD || 0b00)
	//          else PC = PC + EXTS(BD || 0b00)
	void Interpreter::bc()
	{
		if (BcTest())
		{
			core->regs.pc = info.Imm.Address;
			BranchCheck();
		}
		else
		{
			core->regs.pc += 4;
		}
	}

	void Interpreter::bca()
	{
		if (BcTest())
		{
			core->regs.pc = info.Imm.Address;
			BranchCheck();
		}
		else
		{
			core->regs.pc += 4;
		}
	}

	void Interpreter::bcl()
	{
		if (BcTest())
		{
			core->regs.spr[SPR::LR] = core->regs.pc + 4; // LK
			core->regs.pc = info.Imm.Address;
			BranchCheck();
		}
		else
		{
			core->regs.pc += 4;
		}
	}

	void Interpreter::bcla()
	{
		if (BcTest())
		{
			core->regs.spr[SPR::LR] = core->regs.pc + 4; // LK
			core->regs.pc = info.Imm.Address;
			BranchCheck();
		}
		else
		{
			core->regs.pc += 4;
		}
	}

	// calculation of conditional to count register branch
	bool Interpreter::BctrTest()
	{
		bool cond_ok;
		int bo = info.paramBits[0], bi = info.paramBits[1];

		if (BO(0) == 0)
		{
			if (BO(1)) cond_ok = ((core->regs.cr << bi) & 0x80000000) != 0;
			else cond_ok = ((core->regs.cr << bi) & 0x80000000) == 0;
		}
		else cond_ok = true;

		return cond_ok;
	}

	// cond_ok = BO0 | (CR[BI] EQV BO1)
	// if cond_ok
	//      then
	//              PC = CTR || 0b00
	void Interpreter::bcctr()
	{
		if (BctrTest())
		{
			core->regs.pc = core->regs.spr[SPR::CTR] & ~3;
			BranchCheck();
		}
		else
		{
			core->regs.pc += 4;
		}
	}

	// cond_ok = BO0 | (CR[BI] EQV BO1)
	// if cond_ok
	//      then
	//              LR = PC + 4
	//              PC = CTR || 0b00
	void Interpreter::bcctrl()
	{
		if (BctrTest())
		{
			core->regs.spr[SPR::LR] = core->regs.pc + 4;
			core->regs.pc = core->regs.spr[SPR::CTR] & ~3;
			BranchCheck();
		}
		else
		{
			core->regs.pc += 4;
		}
	}

	// if ~BO2 then CTR = CTR - 1
	// ctr_ok  = BO2 | ((CTR != 0) ^ BO3)
	// cond_ok = BO0 | (CR[BI] EQV BO1)
	// if ctr_ok & cond_ok then
	//      PC = LR[0-29] || 0b00
	void Interpreter::bclr()
	{
		if (BcTest())
		{
			core->regs.pc = core->regs.spr[SPR::LR] & ~3;
			BranchCheck();
		}
		else
		{
			core->regs.pc += 4;
		}
	}

	// if ~BO2 then CTR = CTR - 1
	// ctr_ok  = BO2 | ((CTR != 0) ^ BO3)
	// cond_ok = BO0 | (CR[BI] EQV BO1)
	// if ctr_ok & cond_ok then
	//      NLR = PC + 4
	//      PC = LR[0-29] || 0b00
	//      LR = NLR
	void Interpreter::bclrl()
	{
		if (BcTest())
		{
			uint32_t lr = core->regs.pc + 4;
			core->regs.pc = core->regs.spr[SPR::LR] & ~3;
			core->regs.spr[SPR::LR] = lr;
			BranchCheck();
		}
		else
		{
			core->regs.pc += 4;
		}
	}

}
