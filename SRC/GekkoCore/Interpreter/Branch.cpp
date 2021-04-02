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
	void Interpreter::b(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::b]++;
		}

		core->regs.pc = info.Imm.Address;
		BranchCheck();
	}

	// PC = EXTS(LI || 0b00)
	void Interpreter::ba(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ba]++;
		}

		core->regs.pc = info.Imm.Address;
		BranchCheck();
	}

	// LR = PC + 4, PC = PC + EXTS(LI || 0b00)
	void Interpreter::bl(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::bl]++;
		}

		core->regs.spr[(int)SPR::LR] = core->regs.pc + 4;
		core->regs.pc = info.Imm.Address;
		BranchCheck();
	}

	// LR = PC + 4, PC = EXTS(LI || 0b00)
	void Interpreter::bla(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::bla]++;
		}

		core->regs.spr[(int)SPR::LR] = core->regs.pc + 4;
		core->regs.pc = info.Imm.Address;
		BranchCheck();
	}

	// calculation of conditional branch
	bool Interpreter::BcTest(AnalyzeInfo& info)
	{
		bool ctr_ok, cond_ok;
		int bo = info.paramBits[0], bi = info.paramBits[1];

		if (BO(2) == 0)
		{
			core->regs.spr[(int)SPR::CTR]--;

			if (BO(3)) ctr_ok = (core->regs.spr[(int)Gekko::SPR::CTR] == 0);
			else ctr_ok = (core->regs.spr[(int)Gekko::SPR::CTR] != 0);
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
	void Interpreter::bc(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::bc]++;
		}

		if (BcTest(info))
		{
			core->regs.pc = info.Imm.Address;
			BranchCheck();
		}
		else
		{
			core->regs.pc += 4;
		}
	}

	void Interpreter::bca(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::bca]++;
		}

		if (BcTest(info))
		{
			core->regs.pc = info.Imm.Address;
			BranchCheck();
		}
		else
		{
			core->regs.pc += 4;
		}
	}

	void Interpreter::bcl(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::bcl]++;
		}

		if (BcTest(info))
		{
			Gekko->regs.spr[(int)SPR::LR] = core->regs.pc + 4; // LK
			core->regs.pc = info.Imm.Address;
			BranchCheck();
		}
		else
		{
			Gekko->regs.pc += 4;
		}
	}

	void Interpreter::bcla(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::bcla]++;
		}

		if (BcTest(info))
		{
			Gekko->regs.spr[(int)SPR::LR] = core->regs.pc + 4; // LK
			core->regs.pc = info.Imm.Address;
			BranchCheck();
		}
		else
		{
			Gekko->regs.pc += 4;
		}
	}

	// calculation of conditional to count register branch
	bool Interpreter::BctrTest(AnalyzeInfo& info)
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
	void Interpreter::bcctr(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::bcctr]++;
		}

		if (BctrTest(info))
		{
			core->regs.pc = core->regs.spr[(int)SPR::CTR] & ~3;
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
	void Interpreter::bcctrl(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::bcctrl]++;
		}

		if (BctrTest(info))
		{
			core->regs.spr[(int)SPR::LR] = core->regs.pc + 4;
			core->regs.pc = core->regs.spr[(int)SPR::CTR] & ~3;
			BranchCheck();
		}
		else
		{
			Gekko->regs.pc += 4;
		}
	}

	// if ~BO2 then CTR = CTR - 1
	// ctr_ok  = BO2 | ((CTR != 0) ^ BO3)
	// cond_ok = BO0 | (CR[BI] EQV BO1)
	// if ctr_ok & cond_ok then
	//      PC = LR[0-29] || 0b00
	void Interpreter::bclr(AnalyzeInfo& info)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::bclr]++;
		}

		if (BcTest(info))
		{
			core->regs.pc = core->regs.spr[(int)SPR::LR] & ~3;
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
	void Interpreter::bclrl(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::bclrl]++;
		}

		if (BcTest(info))
		{
			uint32_t lr = core->regs.pc + 4;
			core->regs.pc = core->regs.spr[(int)SPR::LR] & ~3;
			core->regs.spr[(int)SPR::LR] = lr;
			BranchCheck();
		}
		else
		{
			core->regs.pc += 4;
		}
	}

}
