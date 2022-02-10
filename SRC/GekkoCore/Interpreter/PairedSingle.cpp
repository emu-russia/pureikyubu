// Paired Single Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

using namespace Debug;

namespace Gekko
{

	// Paired-Single Floating Point Arithmetic Instructions

	void Interpreter::ps_div(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = PS0(info.paramBits[1]) / PS0(info.paramBits[2]);
			PS1(info.paramBits[0]) = PS1(info.paramBits[1]) / PS1(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_div_d(DecoderInfo& info)
	{
		ps_div(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_sub(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = PS0(info.paramBits[1]) - PS0(info.paramBits[2]);
			PS1(info.paramBits[0]) = PS1(info.paramBits[1]) - PS1(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_sub_d(DecoderInfo& info)
	{
		ps_sub(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_add(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = PS0(info.paramBits[1]) + PS0(info.paramBits[2]);
			PS1(info.paramBits[0]) = PS1(info.paramBits[1]) + PS1(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_add_d(DecoderInfo& info)
	{
		ps_add(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_sel(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = (PS0(info.paramBits[1]) >= 0.0) ? (PS0(info.paramBits[2])) : (PS0(info.paramBits[3]));
			PS1(info.paramBits[0]) = (PS1(info.paramBits[1]) >= 0.0) ? (PS1(info.paramBits[2])) : (PS1(info.paramBits[3]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_sel_d(DecoderInfo& info)
	{
		ps_sel(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_res(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = 1.0f / PS0(info.paramBits[1]);
			PS1(info.paramBits[0]) = 1.0f / PS1(info.paramBits[1]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_res_d(DecoderInfo& info)
	{
		ps_res(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_mul(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = PS0(info.paramBits[1]) * PS0(info.paramBits[2]);
			PS1(info.paramBits[0]) = PS1(info.paramBits[1]) * PS1(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_mul_d(DecoderInfo& info)
	{
		ps_mul(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_rsqrte(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = 1.0f / sqrt(PS0(info.paramBits[1]));
			PS1(info.paramBits[0]) = 1.0f / sqrt(PS1(info.paramBits[1]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_rsqrte_d(DecoderInfo& info)
	{
		ps_rsqrte(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_msub(DecoderInfo& info)
	{
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

	void Interpreter::ps_msub_d(DecoderInfo& info)
	{
		ps_msub(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_madd(DecoderInfo& info)
	{
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

	void Interpreter::ps_madd_d(DecoderInfo& info)
	{
		ps_madd(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_nmsub(DecoderInfo& info)
	{
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

	void Interpreter::ps_nmsub_d(DecoderInfo& info)
	{
		ps_nmsub(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_nmadd(DecoderInfo& info)
	{
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

	void Interpreter::ps_nmadd_d(DecoderInfo& info)
	{
		ps_nmadd(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_neg(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = -PS0(info.paramBits[1]);
			PS1(info.paramBits[0]) = -PS1(info.paramBits[1]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_neg_d(DecoderInfo& info)
	{
		ps_neg(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_mr(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			double p0 = PS0(info.paramBits[1]), p1 = PS1(info.paramBits[1]);
			PS0(info.paramBits[0]) = p0, PS1(info.paramBits[0]) = p1;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_mr_d(DecoderInfo& info)
	{
		ps_mr(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_nabs(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0U(info.paramBits[0]) = PS0U(info.paramBits[1]) | 0x8000000000000000;
			PS1U(info.paramBits[0]) = PS1U(info.paramBits[1]) | 0x8000000000000000;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_nabs_d(DecoderInfo& info)
	{
		ps_nabs(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_abs(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0U(info.paramBits[0]) = PS0U(info.paramBits[1]) & ~0x8000000000000000;
			PS1U(info.paramBits[0]) = PS1U(info.paramBits[1]) & ~0x8000000000000000;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::ps_abs_d(DecoderInfo& info)
	{
		ps_abs(info);
		if (!core->exception) COMPUTE_CR1();
	}

	// Miscellaneous Paired-Single Instructions

	void Interpreter::ps_sum0(DecoderInfo& info)
	{
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

	void Interpreter::ps_sum0_d(DecoderInfo& info)
	{
		ps_sum0(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_sum1(DecoderInfo& info)
	{
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

	void Interpreter::ps_sum1_d(DecoderInfo& info)
	{
		ps_sum1(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_muls0(DecoderInfo& info)
	{
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

	void Interpreter::ps_muls0_d(DecoderInfo& info)
	{
		ps_muls0(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_muls1(DecoderInfo& info)
	{
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

	void Interpreter::ps_muls1_d(DecoderInfo& info)
	{
		ps_muls1(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_madds0(DecoderInfo& info)
	{
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

	void Interpreter::ps_madds0_d(DecoderInfo& info)
	{
		ps_madds0(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_madds1(DecoderInfo& info)
	{
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

	void Interpreter::ps_madds1_d(DecoderInfo& info)
	{
		ps_madds1(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_cmpu0(DecoderInfo& info)
	{
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

	void Interpreter::ps_cmpo0(DecoderInfo& info)
	{
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

	void Interpreter::ps_cmpu1(DecoderInfo& info)
	{
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

	void Interpreter::ps_cmpo1(DecoderInfo& info)
	{
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

	void Interpreter::ps_merge00(DecoderInfo& info)
	{
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

	void Interpreter::ps_merge00_d(DecoderInfo& info)
	{
		ps_merge00(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_merge01(DecoderInfo& info)
	{
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

	void Interpreter::ps_merge01_d(DecoderInfo& info)
	{
		ps_merge01(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_merge10(DecoderInfo& info)
	{
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

	void Interpreter::ps_merge10_d(DecoderInfo& info)
	{
		ps_merge10(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_merge11(DecoderInfo& info)
	{
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

	void Interpreter::ps_merge11_d(DecoderInfo& info)
	{
		ps_merge11(info);
		if (!core->exception) COMPUTE_CR1();
	}

}
