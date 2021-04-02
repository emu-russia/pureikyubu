// Paired Single Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

using namespace Debug;

namespace Gekko
{

	// Paired-Single Floating Point Arithmetic Instructions

	void Interpreter::ps_div(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_div]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = PS0(info.paramBits[1]) / PS0(info.paramBits[2]);
			PS1(info.paramBits[0]) = PS1(info.paramBits[1]) / PS1(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_div_d(AnalyzeInfo& info)
	{
		Halt("ps_div.\n");
	}

	void Interpreter::ps_sub(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_sub]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = PS0(info.paramBits[1]) - PS0(info.paramBits[2]);
			PS1(info.paramBits[0]) = PS1(info.paramBits[1]) - PS1(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_sub_d(AnalyzeInfo& info)
	{
		Halt("ps_sub.\n");
	}

	void Interpreter::ps_add(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_add]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = PS0(info.paramBits[1]) + PS0(info.paramBits[2]);
			PS1(info.paramBits[0]) = PS1(info.paramBits[1]) + PS1(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_add_d(AnalyzeInfo& info)
	{
		Halt("ps_add.\n");
	}

	void Interpreter::ps_sel(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_sel]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = (PS0(info.paramBits[1]) >= 0.0) ? (PS0(info.paramBits[2])) : (PS0(info.paramBits[3]));
			PS1(info.paramBits[0]) = (PS1(info.paramBits[1]) >= 0.0) ? (PS1(info.paramBits[2])) : (PS1(info.paramBits[3]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_sel_d(AnalyzeInfo& info)
	{
		Halt("ps_sel.\n");
	}

	void Interpreter::ps_res(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_res]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = 1.0f / PS0(info.paramBits[1]);
			PS1(info.paramBits[0]) = 1.0f / PS1(info.paramBits[1]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_res_d(AnalyzeInfo& info)
	{
		Halt("ps_res.\n");
	}

	void Interpreter::ps_mul(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_mul]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = PS0(info.paramBits[1]) * PS0(info.paramBits[2]);
			PS1(info.paramBits[0]) = PS1(info.paramBits[1]) * PS1(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_mul_d(AnalyzeInfo& info)
	{
		Halt("ps_mul.\n");
	}

	void Interpreter::ps_rsqrte(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_rsqrte]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = 1.0f / sqrt(PS0(info.paramBits[1]));
			PS1(info.paramBits[0]) = 1.0f / sqrt(PS1(info.paramBits[1]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_rsqrte_d(AnalyzeInfo& info)
	{
		Halt("ps_rsqrte.\n");
	}

	void Interpreter::ps_msub(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_msub]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			double a = PS0(info.paramBits[1]);
			double b = PS0(info.paramBits[3]);
			double c = PS0(info.paramBits[2]);
			PS0(info.paramBits[0]) = (a * c) - b;
			a = PS1(info.paramBits[1]);
			b = PS1(info.paramBits[3]);
			c = PS1(info.paramBits[2]);
			PS1(info.paramBits[0]) = (a * c) - b;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_msub_d(AnalyzeInfo& info)
	{
		Halt("ps_msub.\n");
	}

	void Interpreter::ps_madd(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_madd]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			double a = PS0(info.paramBits[1]);
			double b = PS0(info.paramBits[3]);
			double c = PS0(info.paramBits[2]);
			PS0(info.paramBits[0]) = (a * c) + b;
			a = PS1(info.paramBits[1]);
			b = PS1(info.paramBits[3]);
			c = PS1(info.paramBits[2]);
			PS1(info.paramBits[0]) = (a * c) + b;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_madd_d(AnalyzeInfo& info)
	{
		Halt("ps_madd.\n");
	}

	void Interpreter::ps_nmsub(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_nmsub]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			double a = PS0(info.paramBits[1]);
			double b = PS0(info.paramBits[3]);
			double c = PS0(info.paramBits[2]);
			PS0(info.paramBits[0]) = -((a * c) - b);
			a = PS1(info.paramBits[1]);
			b = PS1(info.paramBits[3]);
			c = PS1(info.paramBits[2]);
			PS1(info.paramBits[0]) = -((a * c) - b);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_nmsub_d(AnalyzeInfo& info)
	{
		Halt("ps_nmsub.\n");
	}

	void Interpreter::ps_nmadd(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_nmadd]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			double a = PS0(info.paramBits[1]);
			double b = PS0(info.paramBits[3]);
			double c = PS0(info.paramBits[2]);
			PS0(info.paramBits[0]) = -((a * c) + b);
			a = PS1(info.paramBits[1]);
			b = PS1(info.paramBits[3]);
			c = PS1(info.paramBits[2]);
			PS1(info.paramBits[0]) = -((a * c) + b);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_nmadd_d(AnalyzeInfo& info)
	{
		Halt("ps_nmadd.\n");
	}

	void Interpreter::ps_neg(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_neg]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = -PS0(info.paramBits[1]);
			PS1(info.paramBits[0]) = -PS1(info.paramBits[1]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_neg_d(AnalyzeInfo& info)
	{
		Halt("ps_neg.\n");
	}

	void Interpreter::ps_mr(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_mr]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			double p0 = PS0(info.paramBits[1]), p1 = PS1(info.paramBits[1]);
			PS0(info.paramBits[0]) = p0, PS1(info.paramBits[0]) = p1;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_mr_d(AnalyzeInfo& info)
	{
		Halt("ps_mr.\n");
	}

	void Interpreter::ps_nabs(AnalyzeInfo& info)
	{
		Halt("ps_nabs\n");
	}

	void Interpreter::ps_nabs_d(AnalyzeInfo& info)
	{
		Halt("ps_nabs.\n");
	}

	void Interpreter::ps_abs(AnalyzeInfo& info)
	{
		Halt("ps_abs\n");
	}

	void Interpreter::ps_abs_d(AnalyzeInfo& info)
	{
		Halt("ps_abs.\n");
	}

	// Miscellaneous Paired-Single Instructions

	void Interpreter::ps_sum0(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_sum0]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			double s0 = PS0(info.paramBits[1]) + PS1(info.paramBits[3]);
			double s1 = PS1(info.paramBits[2]);
			PS0(info.paramBits[0]) = s0;
			PS1(info.paramBits[0]) = s1;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_sum0_d(AnalyzeInfo& info)
	{
		Halt("ps_sum0.\n");
	}

	void Interpreter::ps_sum1(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_sum1]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			double s0 = PS0(info.paramBits[2]);
			double s1 = PS0(info.paramBits[1]) + PS1(info.paramBits[3]);
			PS0(info.paramBits[0]) = s0;
			PS1(info.paramBits[0]) = s1;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_sum1_d(AnalyzeInfo& info)
	{
		Halt("ps_sum1.\n");
	}

	void Interpreter::ps_muls0(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_muls0]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			double m0 = PS0(info.paramBits[1]) * PS0(info.paramBits[2]);
			double m1 = PS1(info.paramBits[1]) * PS0(info.paramBits[2]);
			PS0(info.paramBits[0]) = m0;
			PS1(info.paramBits[0]) = m1;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_muls0_d(AnalyzeInfo& info)
	{
		Halt("ps_muls0.\n");
	}

	void Interpreter::ps_muls1(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_muls1]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			double m0 = PS0(info.paramBits[1]) * PS1(info.paramBits[2]);
			double m1 = PS1(info.paramBits[1]) * PS1(info.paramBits[2]);
			PS0(info.paramBits[0]) = m0;
			PS1(info.paramBits[0]) = m1;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_muls1_d(AnalyzeInfo& info)
	{
		Halt("ps_muls1.\n");
	}

	void Interpreter::ps_madds0(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_madds0]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			double s0 = (PS0(info.paramBits[1]) * PS0(info.paramBits[2])) + PS0(info.paramBits[3]);
			double s1 = (PS1(info.paramBits[1]) * PS0(info.paramBits[2])) + PS1(info.paramBits[3]);
			PS0(info.paramBits[0]) = s0;
			PS1(info.paramBits[0]) = s1;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_madds0_d(AnalyzeInfo& info)
	{
		Halt("ps_madds0.\n");
	}

	void Interpreter::ps_madds1(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_madds1]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			double s0 = (PS0(info.paramBits[1]) * PS1(info.paramBits[2])) + PS0(info.paramBits[3]);
			double s1 = (PS1(info.paramBits[1]) * PS1(info.paramBits[2])) + PS1(info.paramBits[3]);
			PS0(info.paramBits[0]) = s0;
			PS1(info.paramBits[0]) = s1;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_madds1_d(AnalyzeInfo& info)
	{
		Halt("ps_madds1.\n");
	}

	void Interpreter::ps_cmpu0(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_cmpu0]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			int n = info.paramBits[0];
			double a = PS0(info.paramBits[1]), b = PS0(info.paramBits[2]);
			uint64_t da, db;
			uint32_t c;

			da = *(uint64_t*)&a;
			db = *(uint64_t*)&b;

			if (IS_NAN(da) || IS_NAN(db)) c = 1;
			else if (a < b) c = 8;
			else if (a > b) c = 4;
			else c = 2;

			SET_CRF(n, c);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_cmpo0(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_cmpo0]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			int n = info.paramBits[0];
			double a = PS0(info.paramBits[1]), b = PS0(info.paramBits[2]);
			uint64_t da, db;
			uint32_t c;

			da = *(uint64_t*)&a;
			db = *(uint64_t*)&b;

			if (IS_NAN(da) || IS_NAN(db)) c = 1;
			else if (a < b) c = 8;
			else if (a > b) c = 4;
			else c = 2;

			SET_CRF(n, c);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_cmpu1(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_cmpu1]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			int n = info.paramBits[0];
			double a = PS1(info.paramBits[1]), b = PS1(info.paramBits[2]);
			uint64_t da, db;
			uint32_t c;

			da = *(uint64_t*)&a;
			db = *(uint64_t*)&b;

			if (IS_NAN(da) || IS_NAN(db)) c = 1;
			else if (a < b) c = 8;
			else if (a > b) c = 4;
			else c = 2;

			SET_CRF(n, c);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_cmpo1(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_cmpo1]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			int n = info.paramBits[0];
			double a = PS1(info.paramBits[1]), b = PS1(info.paramBits[2]);
			uint64_t da, db;
			uint32_t c;

			da = *(uint64_t*)&a;
			db = *(uint64_t*)&b;

			if (IS_NAN(da) || IS_NAN(db)) c = 1;
			else if (a < b) c = 8;
			else if (a > b) c = 4;
			else c = 2;

			SET_CRF(n, c);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_merge00(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_merge00]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			double a = PS0(info.paramBits[1]);
			double b = PS0(info.paramBits[2]);
			PS0(info.paramBits[0]) = a;
			PS1(info.paramBits[0]) = b;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_merge00_d(AnalyzeInfo& info)
	{
		Halt("ps_merge00.\n");
	}

	void Interpreter::ps_merge01(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_merge01]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			double a = PS0(info.paramBits[1]);
			double b = PS1(info.paramBits[2]);
			PS0(info.paramBits[0]) = a;
			PS1(info.paramBits[0]) = b;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_merge01_d(AnalyzeInfo& info)
	{
		Halt("ps_merge01.\n");
	}

	void Interpreter::ps_merge10(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_merge10]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			double a = PS1(info.paramBits[1]);
			double b = PS0(info.paramBits[2]);
			PS0(info.paramBits[0]) = a;
			PS1(info.paramBits[0]) = b;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_merge10_d(AnalyzeInfo& info)
	{
		Halt("ps_merge10.\n");
	}

	void Interpreter::ps_merge11(AnalyzeInfo& info)
	{
		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)Gekko::Instruction::ps_merge11]++;
		}

		if (core->regs.msr & MSR_FP)
		{
			double a = PS1(info.paramBits[1]);
			double b = PS1(info.paramBits[2]);
			PS0(info.paramBits[0]) = a;
			PS1(info.paramBits[0]) = b;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_merge11_d(AnalyzeInfo& info)
	{
		Halt("ps_merge11.\n");
	}

}
