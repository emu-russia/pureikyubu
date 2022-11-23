// Paired Single Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

	// Paired-Single Floating Point Arithmetic Instructions

	void Interpreter::ps_div()
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = PS0(info.paramBits[1]) / PS0(info.paramBits[2]);
			PS1(info.paramBits[0]) = PS1(info.paramBits[1]) / PS1(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_div_d()
	{
		ps_div();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_sub()
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = PS0(info.paramBits[1]) - PS0(info.paramBits[2]);
			PS1(info.paramBits[0]) = PS1(info.paramBits[1]) - PS1(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_sub_d()
	{
		ps_sub();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_add()
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = PS0(info.paramBits[1]) + PS0(info.paramBits[2]);
			PS1(info.paramBits[0]) = PS1(info.paramBits[1]) + PS1(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_add_d()
	{
		ps_add();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_sel()
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = (PS0(info.paramBits[1]) >= 0.0) ? (PS0(info.paramBits[2])) : (PS0(info.paramBits[3]));
			PS1(info.paramBits[0]) = (PS1(info.paramBits[1]) >= 0.0) ? (PS1(info.paramBits[2])) : (PS1(info.paramBits[3]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_sel_d()
	{
		ps_sel();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_res()
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = 1.0f / PS0(info.paramBits[1]);
			PS1(info.paramBits[0]) = 1.0f / PS1(info.paramBits[1]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_res_d()
	{
		ps_res();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_mul()
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = PS0(info.paramBits[1]) * PS0(info.paramBits[2]);
			PS1(info.paramBits[0]) = PS1(info.paramBits[1]) * PS1(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_mul_d()
	{
		ps_mul();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_rsqrte()
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = 1.0f / sqrt(PS0(info.paramBits[1]));
			PS1(info.paramBits[0]) = 1.0f / sqrt(PS1(info.paramBits[1]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_rsqrte_d()
	{
		ps_rsqrte();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_msub()
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
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_msub_d()
	{
		ps_msub();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_madd()
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
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_madd_d()
	{
		ps_madd();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_nmsub()
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
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_nmsub_d()
	{
		ps_nmsub();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_nmadd()
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
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_nmadd_d()
	{
		ps_nmadd();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_neg()
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0(info.paramBits[0]) = -PS0(info.paramBits[1]);
			PS1(info.paramBits[0]) = -PS1(info.paramBits[1]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_neg_d()
	{
		ps_neg();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_mr()
	{
		if (core->regs.msr & MSR_FP)
		{
			double p0 = PS0(info.paramBits[1]), p1 = PS1(info.paramBits[1]);
			PS0(info.paramBits[0]) = p0, PS1(info.paramBits[0]) = p1;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_mr_d()
	{
		ps_mr();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_nabs()
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0U(info.paramBits[0]) = PS0U(info.paramBits[1]) | 0x8000000000000000;
			PS1U(info.paramBits[0]) = PS1U(info.paramBits[1]) | 0x8000000000000000;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_nabs_d()
	{
		ps_nabs();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_abs()
	{
		if (core->regs.msr & MSR_FP)
		{
			PS0U(info.paramBits[0]) = PS0U(info.paramBits[1]) & ~0x8000000000000000;
			PS1U(info.paramBits[0]) = PS1U(info.paramBits[1]) & ~0x8000000000000000;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_abs_d()
	{
		ps_abs();
		if (!core->exception) COMPUTE_CR1();
	}

	// Miscellaneous Paired-Single Instructions

	void Interpreter::ps_sum0()
	{
		if (core->regs.msr & MSR_FP)
		{
			double s0 = PS0(info.paramBits[1]) + PS1(info.paramBits[3]);
			double s1 = PS1(info.paramBits[2]);
			PS0(info.paramBits[0]) = s0;
			PS1(info.paramBits[0]) = s1;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_sum0_d()
	{
		ps_sum0();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_sum1()
	{
		if (core->regs.msr & MSR_FP)
		{
			double s0 = PS0(info.paramBits[2]);
			double s1 = PS0(info.paramBits[1]) + PS1(info.paramBits[3]);
			PS0(info.paramBits[0]) = s0;
			PS1(info.paramBits[0]) = s1;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_sum1_d()
	{
		ps_sum1();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_muls0()
	{
		if (core->regs.msr & MSR_FP)
		{
			double m0 = PS0(info.paramBits[1]) * PS0(info.paramBits[2]);
			double m1 = PS1(info.paramBits[1]) * PS0(info.paramBits[2]);
			PS0(info.paramBits[0]) = m0;
			PS1(info.paramBits[0]) = m1;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_muls0_d()
	{
		ps_muls0();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_muls1()
	{
		if (core->regs.msr & MSR_FP)
		{
			double m0 = PS0(info.paramBits[1]) * PS1(info.paramBits[2]);
			double m1 = PS1(info.paramBits[1]) * PS1(info.paramBits[2]);
			PS0(info.paramBits[0]) = m0;
			PS1(info.paramBits[0]) = m1;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_muls1_d()
	{
		ps_muls1();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_madds0()
	{
		if (core->regs.msr & MSR_FP)
		{
			double s0 = (PS0(info.paramBits[1]) * PS0(info.paramBits[2])) + PS0(info.paramBits[3]);
			double s1 = (PS1(info.paramBits[1]) * PS0(info.paramBits[2])) + PS1(info.paramBits[3]);
			PS0(info.paramBits[0]) = s0;
			PS1(info.paramBits[0]) = s1;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_madds0_d()
	{
		ps_madds0();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_madds1()
	{
		if (core->regs.msr & MSR_FP)
		{
			double s0 = (PS0(info.paramBits[1]) * PS1(info.paramBits[2])) + PS0(info.paramBits[3]);
			double s1 = (PS1(info.paramBits[1]) * PS1(info.paramBits[2])) + PS1(info.paramBits[3]);
			PS0(info.paramBits[0]) = s0;
			PS1(info.paramBits[0]) = s1;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_madds1_d()
	{
		ps_madds1();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_cmpu0()
	{
		if (core->regs.msr & MSR_FP)
		{
			size_t n = info.paramBits[0];
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
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_cmpo0()
	{
		if (core->regs.msr & MSR_FP)
		{
			size_t n = info.paramBits[0];
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
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_cmpu1()
	{
		if (core->regs.msr & MSR_FP)
		{
			size_t n = info.paramBits[0];
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
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_cmpo1()
	{
		if (core->regs.msr & MSR_FP)
		{
			size_t n = info.paramBits[0];
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
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_merge00()
	{
		if (core->regs.msr & MSR_FP)
		{
			double a = PS0(info.paramBits[1]);
			double b = PS0(info.paramBits[2]);
			PS0(info.paramBits[0]) = a;
			PS1(info.paramBits[0]) = b;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_merge00_d()
	{
		ps_merge00();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_merge01()
	{
		if (core->regs.msr & MSR_FP)
		{
			double a = PS0(info.paramBits[1]);
			double b = PS1(info.paramBits[2]);
			PS0(info.paramBits[0]) = a;
			PS1(info.paramBits[0]) = b;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_merge01_d()
	{
		ps_merge01();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_merge10()
	{
		if (core->regs.msr & MSR_FP)
		{
			double a = PS1(info.paramBits[1]);
			double b = PS0(info.paramBits[2]);
			PS0(info.paramBits[0]) = a;
			PS1(info.paramBits[0]) = b;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_merge10_d()
	{
		ps_merge10();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::ps_merge11()
	{
		if (core->regs.msr & MSR_FP)
		{
			double a = PS1(info.paramBits[1]);
			double b = PS1(info.paramBits[2]);
			PS0(info.paramBits[0]) = a;
			PS1(info.paramBits[0]) = b;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::ps_merge11_d()
	{
		ps_merge11();
		if (!core->exception) COMPUTE_CR1();
	}

}
