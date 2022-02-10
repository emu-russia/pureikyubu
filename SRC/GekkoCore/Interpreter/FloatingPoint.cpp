// Floating-Point Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

	void Interpreter::fadd(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) + FPRD(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fadd_d(DecoderInfo& info)
	{
		fadd(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fadds(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) + FPRD(info.paramBits[2]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fadds_d(DecoderInfo& info)
	{
		fadds(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fdiv(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) / FPRD(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fdiv_d(DecoderInfo& info)
	{
		fdiv(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fdivs(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) / FPRD(info.paramBits[2]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fdivs_d(DecoderInfo& info)
	{
		fdivs(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fmul(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmul_d(DecoderInfo& info)
	{
		fmul(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fmuls(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmuls_d(DecoderInfo& info)
	{
		fmuls(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fres(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = 1.0 / FPRD(info.paramBits[1]);
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fres_d(DecoderInfo& info)
	{
		fres(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::frsqrte(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = 1.0 / sqrt(FPRD(info.paramBits[1]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::frsqrte_d(DecoderInfo& info)
	{
		frsqrte(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fsub(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) - FPRD(info.paramBits[2]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fsub_d(DecoderInfo& info)
	{
		fsub(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fsubs(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) - FPRD(info.paramBits[2]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fsubs_d(DecoderInfo& info)
	{
		fsubs(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fsel(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (FPRD(info.paramBits[1]) >= 0.0) ? (FPRD(info.paramBits[2])) : (FPRD(info.paramBits[3]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fsel_d(DecoderInfo& info)
	{
		fsel(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fmadd(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) + FPRD(info.paramBits[3]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmadd_d(DecoderInfo& info)
	{
		fmadd(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fmadds(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) + FPRD(info.paramBits[3]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmadds_d(DecoderInfo& info)
	{
		fmadds(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fmsub(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) - FPRD(info.paramBits[3]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmsub_d(DecoderInfo& info)
	{
		fmsub(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fmsubs(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) - FPRD(info.paramBits[3]));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmsubs_d(DecoderInfo& info)
	{
		fmsubs(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fnmadd(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = -(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) + FPRD(info.paramBits[3]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fnmadd_d(DecoderInfo& info)
	{
		fnmadd(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fnmadds(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(-(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) + FPRD(info.paramBits[3])));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fnmadds_d(DecoderInfo& info)
	{
		fnmadds(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fnmsub(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = -(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) - FPRD(info.paramBits[3]));
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fnmsub_d(DecoderInfo& info)
	{
		fnmsub(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fnmsubs(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRD(info.paramBits[0]) = (float)(-(FPRD(info.paramBits[1]) * FPRD(info.paramBits[2]) - FPRD(info.paramBits[3])));
			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS1(info.paramBits[0]) = PS0(info.paramBits[0]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fnmsubs_d(DecoderInfo& info)
	{
		fnmsubs(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fctiw(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = (uint64_t)(uint32_t)(int32_t)FPRD(info.paramBits[1]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fctiw_d(DecoderInfo& info)
	{
		fctiw(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fctiwz(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = (uint64_t)(uint32_t)(int32_t)FPRD(info.paramBits[1]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fctiwz_d(DecoderInfo& info)
	{
		fctiwz(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::frsp(DecoderInfo& info)
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

	void Interpreter::frsp_d(DecoderInfo& info)
	{
		frsp(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fcmpo(DecoderInfo& info)
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

	void Interpreter::fcmpu(DecoderInfo& info)
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

	void Interpreter::fabs(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = FPRU(info.paramBits[1]) & ~0x8000000000000000;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fabs_d(DecoderInfo& info)
	{
		fabs(info);
		if (!core->exception) COMPUTE_CR1();
	}

	// fd = fb
	void Interpreter::fmr(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = FPRU(info.paramBits[1]);
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fmr_d(DecoderInfo& info)
	{
		fmr(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fnabs(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = FPRU(info.paramBits[1]) | 0x8000000000000000;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fnabs_d(DecoderInfo& info)
	{
		fnabs(info);
		if (!core->exception) COMPUTE_CR1();
	}

	void Interpreter::fneg(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = FPRU(info.paramBits[1]) ^ 0x8000000000000000;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::fneg_d(DecoderInfo& info)
	{
		fneg(info);
		if (!core->exception) COMPUTE_CR1();
	}

	// CR[crfD] = FPSCR[crfS]
	void Interpreter::mcrfs(DecoderInfo& info)
	{
		uint32_t fp = (core->regs.fpscr >> (28 - info.paramBits[1])) & 0xf;
		core->regs.cr &= ~(0xf0000000 >> info.paramBits[0]);
		core->regs.cr |= fp << (28 - info.paramBits[0]);
		core->regs.pc += 4;
	}

	// fd[32-63] = FPSCR
	void Interpreter::mffs(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			FPRU(info.paramBits[0]) = (uint64_t)core->regs.fpscr;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// fd[32-63] = FPSCR, CR1
	void Interpreter::mffs_d(DecoderInfo& info)
	{
		mffs(info);
		if (!core->exception) COMPUTE_CR1();
	}

	// FPSCR(crbD) = 0 (clear bit)
	void Interpreter::mtfsb0(DecoderInfo& info)
	{
		uint32_t m = 1 << (31 - info.paramBits[0]);
		core->regs.fpscr &= ~m;
		core->regs.pc += 4;
	}

	// FPSCR(crbD) = 0 (clear bit), CR1
	void Interpreter::mtfsb0_d(DecoderInfo& info)
	{
		mtfsb0(info);
		COMPUTE_CR1();
	}

	// FPSCR(crbD) = 1 (set bit)
	void Interpreter::mtfsb1(DecoderInfo& info)
	{
		uint32_t m = 1 << (31 - info.paramBits[0]);
		core->regs.fpscr = (core->regs.fpscr & ~m) | m;
		core->regs.pc += 4;
	}

	// FPSCR(crbD) = 1 (set bit), CR1
	void Interpreter::mtfsb1_d(DecoderInfo& info)
	{
		mtfsb1(info);
		COMPUTE_CR1();
	}

	// mask = (4)FM[0] || (4)FM[1] || ... || (4)FM[7]
	// FPSCR = (fb & mask) | (FPSCR & ~mask)
	void Interpreter::mtfsf(DecoderInfo& info)
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
	void Interpreter::mtfsf_d(DecoderInfo& info)
	{
		mtfsf(info);
		COMPUTE_CR1();
	}

	// FPSCR[crfD] = IMM.
	void Interpreter::mtfsfi(DecoderInfo& info)
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

	void Interpreter::mtfsfi_d(DecoderInfo& info)
	{
		mtfsfi(info);
		COMPUTE_CR1();
	}

}
