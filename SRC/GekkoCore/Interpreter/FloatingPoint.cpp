// Floating-Point Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

	void Interpreter::fadd()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) + FPRD(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fadd_d()
	{
		fadd();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fadds()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) + FPRD(info.paramBits[2]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fadds_d()
	{
		fadds();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fdiv()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) / FPRD(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fdiv_d()
	{
		fdiv();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fdivs()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) / FPRD(info.paramBits[2]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fdivs_d()
	{
		fdivs();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fmul()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fmul_d()
	{
		fmul();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fmuls()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fmuls_d()
	{
		fmuls();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fres()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = 1.0 / FPRD(info.paramBits[1]);
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fres_d()
	{
		fres();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::frsqrte()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = 1.0 / sqrt(FPRD(info.paramBits[1]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::frsqrte_d()
	{
		frsqrte();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fsub()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) - FPRD(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fsub_d()
	{
		fsub();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fsubs()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) - FPRD(info.paramBits[2]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fsubs_d()
	{
		fsubs();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fsel()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (FPRD(info.paramBits[1]) >= 0.0) ? (FPRD(info.paramBits[2])) : (FPRD(info.paramBits[3]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fsel_d()
	{
		fsel();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fmadd()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) + FPRD(info.paramBits[3]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fmadd_d()
	{
		fmadd();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fmadds()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) + FPRD(info.paramBits[3]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fmadds_d()
	{
		fmadds();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fmsub()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) - FPRD(info.paramBits[3]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fmsub_d()
	{
		fmsub();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fmsubs()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) - FPRD(info.paramBits[3]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fmsubs_d()
	{
		fmsubs();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fnmadd()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = -(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) + FPRD(info.paramBits[3]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fnmadd_d()
	{
		fnmadd();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fnmadds()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(-(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) + FPRD(info.paramBits[3])));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fnmadds_d()
	{
		fnmadds();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fnmsub()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = -(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) - FPRD(info.paramBits[3]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fnmsub_d()
	{
		fnmsub();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fnmsubs()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(-(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) - FPRD(info.paramBits[3])));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fnmsubs_d()
	{
		fnmsubs();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fctiw()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = (uint64_t)(uint32_t)(int32_t)FPRD(info.paramBits[1]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fctiw_d()
	{
		fctiw();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fctiwz()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = (uint64_t)(uint32_t)(int32_t)FPRD(info.paramBits[1]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fctiwz_d()
	{
		fctiwz();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::frsp()
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
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::frsp_d()
	{
		frsp();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fcmpo()
	{
		if (core->regs.msr & MSR_FP)
		{
			int32_t n = (int32_t)info.paramBits[0];
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
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fcmpu()
	{
		if (core->regs.msr & MSR_FP)
		{
			int32_t n = (int32_t)info.paramBits[0];
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
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fabs()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = FPRU(info.paramBits[1]) & ~0x8000000000000000;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fabs_d()
	{
		fabs();
		if (!core->exception) COMPUTE_CR1();
	}

	// fd = fb
	void Interpreter::fmr()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = FPRU(info.paramBits[1]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fmr_d()
	{
		fmr();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fnabs()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = FPRU(info.paramBits[1]) | 0x8000000000000000;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fnabs_d()
	{
		fnabs();
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fneg()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = FPRU(info.paramBits[1]) ^ 0x8000000000000000;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::fneg_d()
	{
		fneg();
		if (!core->exception) COMPUTE_CR1();
	}

	// CR[crfD] = FPSCR[crfS]
	void Interpreter::mcrfs()
	{
		uint32_t fp = (core->regs.fpscr >> (28 - info.paramBits[1])) & 0xf;
		core->regs.cr &= ~(0xf0000000 >> info.paramBits[0]);
		core->regs.cr |= fp << (28 - info.paramBits[0]);
		core->regs.pc += 4;
	}

	// fd[32-63] = FPSCR
	void Interpreter::mffs()
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = (uint64_t)core->regs.fpscr;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	// fd[32-63] = FPSCR, CR1
	void Interpreter::mffs_d()
	{
		mffs();
		if (!core->exception) COMPUTE_CR1();
	}

	// FPSCR(crbD) = 0 (clear bit)
	void Interpreter::mtfsb0()
	{
		uint32_t m = 1 << (31 - info.paramBits[0]);
		core->regs.fpscr &= ~m;
		core->regs.pc += 4;
	}

	// FPSCR(crbD) = 0 (clear bit), CR1
	void Interpreter::mtfsb0_d()
	{
		mtfsb0();
		COMPUTE_CR1();
	}

	// FPSCR(crbD) = 1 (set bit)
	void Interpreter::mtfsb1()
	{
		uint32_t m = 1 << (31 - info.paramBits[0]);
		core->regs.fpscr = (core->regs.fpscr & ~m) | m;
		core->regs.pc += 4;
	}

	// FPSCR(crbD) = 1 (set bit), CR1
	void Interpreter::mtfsb1_d()
	{
		mtfsb1();
		COMPUTE_CR1();
	}

	// mask = (4)FM[0] || (4)FM[1] || ... || (4)FM[7]
	// FPSCR = (fb & mask) | (FPSCR & ~mask)
	void Interpreter::mtfsf()
	{
		uint32_t m = 0, fm = (uint32_t)info.paramBits[0];

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
	void Interpreter::mtfsf_d()
	{
		mtfsf();
		COMPUTE_CR1();
	}

	// FPSCR[crfD] = IMM.
	void Interpreter::mtfsfi()
	{
		int crf = info.paramBits[0] & 7;
		int imm = info.paramBits[1] & 0xf;

		uint32_t oldFpscr = core->regs.fpscr;

		core->regs.fpscr &= ~(0xf << (7 - crf));

		if (crf == 0)
		{
			// When FPSCR[0–3] is specified, bits 0 (FX) and 3 (OX) are set to the values of IMM[0] and IMM[3]
			core->regs.fpscr |= ((imm & 0b1001) << (7 - crf));
			core->regs.fpscr |= oldFpscr & 0x60000000;
		}
		else
		{
			core->regs.fpscr |= (imm << (7 - crf));
		}

		core->regs.pc += 4;
	}

	void Interpreter::mtfsfi_d()
	{
		mtfsfi();
		COMPUTE_CR1();
	}

}
