// Floating-Point Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

	void Interpreter::fadd(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) + FPRD(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fadd_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) + FPRD(info.paramBits[2]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fadds(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) + FPRD(info.paramBits[2]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fadds_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) + FPRD(info.paramBits[2]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fdiv(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) / FPRD(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fdiv_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) / FPRD(info.paramBits[2]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fdivs(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) / FPRD(info.paramBits[2]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fdivs_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) / FPRD(info.paramBits[2]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmul(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmul_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmuls(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmuls_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fres(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = 1.0 / FPRD(info.paramBits[1]);
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fres_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = 1.0 / FPRD(info.paramBits[1]);
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::frsqrte(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = 1.0 / sqrt(FPRD(info.paramBits[1]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::frsqrte_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = 1.0 / sqrt(FPRD(info.paramBits[1]));
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fsub(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) - FPRD(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fsub_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) - FPRD(info.paramBits[2]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fsubs(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) - FPRD(info.paramBits[2]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fsubs_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) - FPRD(info.paramBits[2]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fsel(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (FPRD(info.paramBits[1]) >= 0.0) ? (FPRD(info.paramBits[2])) : (FPRD(info.paramBits[3]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fsel_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (FPRD(info.paramBits[1]) >= 0.0) ? (FPRD(info.paramBits[2])) : (FPRD(info.paramBits[3]));
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmadd(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) + FPRD(info.paramBits[3]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmadd_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) + FPRD(info.paramBits[3]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmadds(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) + FPRD(info.paramBits[3]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmadds_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) + FPRD(info.paramBits[3]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmsub(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) - FPRD(info.paramBits[3]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmsub_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) - FPRD(info.paramBits[3]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmsubs(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) - FPRD(info.paramBits[3]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmsubs_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) - FPRD(info.paramBits[3]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fnmadd(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = -(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) + FPRD(info.paramBits[3]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fnmadd_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = -(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) + FPRD(info.paramBits[3]));
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fnmadds(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(-(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) + FPRD(info.paramBits[3])));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fnmadds_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(-(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) + FPRD(info.paramBits[3])));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fnmsub(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = -(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) - FPRD(info.paramBits[3]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fnmsub_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = -(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) - FPRD(info.paramBits[3]));
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fnmsubs(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(-(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) - FPRD(info.paramBits[3])));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fnmsubs_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(-(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) - FPRD(info.paramBits[3])));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fctiw(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = (uint64_t)(uint32_t)(int32_t)FPRD(info.paramBits[1]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fctiw_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = (uint64_t)(uint32_t)(int32_t)FPRD(info.paramBits[1]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fctiwz(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = (uint64_t)(uint32_t)(int32_t)FPRD(info.paramBits[1]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fctiwz_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = (uint64_t)(uint32_t)(int32_t)FPRD(info.paramBits[1]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::frsp(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			if (core->regs.spr[SPR::HID2] & HID2_PSE)
			{
				PS0(info.paramBits[0]) = (float)FPRD(info.paramBits[1]);
				PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			}
			else FPRD(info.paramBits[0]) = (float)FPRD(info.paramBits[1]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::frsp_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			if (core->regs.spr[SPR::HID2] & HID2_PSE)
			{
				PS0(info.paramBits[0]) = (float)FPRD(info.paramBits[1]);
				PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			}
			else FPRD(info.paramBits[0]) = (float)FPRD(info.paramBits[1]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fcmpo(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			int32_t n = info.paramBits[0];
			double a = FPRD(info.paramBits[1]), b = FPRD(info.paramBits[2]);
			uint64_t da = FPRU(info.paramBits[1]), db = FPRU(info.paramBits[2]);
			uint32_t c;

			if (IS_NAN(da) || IS_NAN(db)) c = 1;
			else if (a < b) c = 8;
			else if (a > b) c = 4;
			else c = 2;

			core->regs.fpscr = (core->regs.fpscr & 0xffff0fff) | (c << 12);
			core->regs.cr = (core->regs.cr & (~(0xf << ((7 - n) * 4)))) | (c << ((7 - n) * 4));
			if (IS_SNAN(da) || IS_SNAN(db))
			{
				core->regs.fpscr = core->regs.fpscr | 0x01000000;
			}
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fcmpu(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			int32_t n = info.paramBits[0];
			double a = FPRD(info.paramBits[1]), b = FPRD(info.paramBits[2]);
			uint64_t da = FPRU(info.paramBits[1]), db = FPRU(info.paramBits[2]);
			uint32_t c;

			if (IS_NAN(da) || IS_NAN(db)) c = 1;
			else if (a < b) c = 8;
			else if (a > b) c = 4;
			else c = 2;

			core->regs.fpscr = (core->regs.fpscr & 0xffff0fff) | (c << 12);
			core->regs.cr = (core->regs.cr & (~(0xf << ((7 - n) * 4)))) | (c << ((7 - n) * 4));
			if (IS_SNAN(da) || IS_SNAN(db))
			{
				core->regs.fpscr = core->regs.fpscr | 0x01000000;
			}
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fabs(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = FPRU(info.paramBits[1]) & ~0x8000000000000000;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fabs_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = FPRU(info.paramBits[1]) & ~0x8000000000000000;
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// fd = fb
	void Interpreter::fmr(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = FPRU(info.paramBits[1]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmr_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = FPRU(info.paramBits[1]);
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fnabs(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = FPRU(info.paramBits[1]) | 0x8000000000000000;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fnabs_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = FPRU(info.paramBits[1]) | 0x8000000000000000;
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fneg(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = FPRU(info.paramBits[1]) ^ 0x8000000000000000;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fneg_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = FPRU(info.paramBits[1]) ^ 0x8000000000000000;
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// CR[crfD] = FPSCR[crfS]
	void Interpreter::mcrfs(AnalyzeInfo& info)
	{
		uint32_t fp = (core->regs.fpscr >> (28 - info.paramBits[1])) & 0xf;
		core->regs.cr &= ~(0xf0000000 >> info.paramBits[0]);
		core->regs.cr |= fp << (28 - info.paramBits[0]);
		core->regs.pc += 4;
	}

	// fd[32-63] = FPSCR
	void Interpreter::mffs(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = (uint64_t)core->regs.fpscr;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// fd[32-63] = FPSCR, CR1
	void Interpreter::mffs_d(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = (uint64_t)core->regs.fpscr;
			COMPUTE_CR1();
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// FPSCR(crbD) = 0 (clear bit)
	void Interpreter::mtfsb0(AnalyzeInfo& info)
	{
		uint32_t m = 1 << (31 - info.paramBits[0]);
		core->regs.fpscr &= ~m;
		core->regs.pc += 4;
	}

	// FPSCR(crbD) = 0 (clear bit), CR1
	void Interpreter::mtfsb0_d(AnalyzeInfo& info)
	{
		uint32_t m = 1 << (31 - info.paramBits[0]);
		core->regs.fpscr &= ~m;
		COMPUTE_CR1();
		core->regs.pc += 4;
	}

	// FPSCR(crbD) = 1 (set bit)
	void Interpreter::mtfsb1(AnalyzeInfo& info)
	{
		uint32_t m = 1 << (31 - info.paramBits[0]);
		core->regs.fpscr = (core->regs.fpscr & ~m) | m;
		core->regs.pc += 4;
	}

	// FPSCR(crbD) = 1 (set bit), CR1
	void Interpreter::mtfsb1_d(AnalyzeInfo& info)
	{
		uint32_t m = 1 << (31 - info.paramBits[0]);
		core->regs.fpscr = (core->regs.fpscr & ~m) | m;
		COMPUTE_CR1();
		core->regs.pc += 4;
	}

	// mask = (4)FM[0] || (4)FM[1] || ... || (4)FM[7]
	// FPSCR = (fb & mask) | (FPSCR & ~mask)
	void Interpreter::mtfsf(AnalyzeInfo& info)
	{
		uint32_t m = 0, fm = info.paramBits[0];

		for (int i = 7; i >= 0; i--)
		{
			if ((fm >> i) & 1)
			{
				m |= 0xf;
			}
			m <<= 4;
		}

		core->regs.fpscr = ((uint32_t)FPRU(info.paramBits[1]) & m) | (core->regs.fpscr & ~m);
		core->regs.pc += 4;
	}

	// mask = (4)FM[0] || (4)FM[1] || ... || (4)FM[7]
	// FPSCR = (fb & mask) | (FPSCR & ~mask)
	void Interpreter::mtfsf_d(AnalyzeInfo& info)
	{
		uint32_t m = 0, fm = info.paramBits[0];

		for (int i = 7; i >= 0; i--)
		{
			if ((fm >> i) & 1)
			{
				m |= 0xf;
			}
			m <<= 4;
		}

		core->regs.fpscr = ((uint32_t)FPRU(info.paramBits[1]) & m) | (core->regs.fpscr & ~m);
		COMPUTE_CR1();
		core->regs.pc += 4;
	}

	void Interpreter::mtfsfi(AnalyzeInfo& info)
	{
		Debug::Halt("mtfsfi\n");
	}

	void Interpreter::mtfsfi_d(AnalyzeInfo& info)
	{
		Debug::Halt("mtfsfi.\n");
	}

}
