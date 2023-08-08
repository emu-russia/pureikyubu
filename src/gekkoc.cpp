#include "pch.h"

using namespace Debug;

// Branch Instructions

namespace Gekko
{
	// info.Imm.Address is always pre-calculated, there is no need to perform operations on the `pc`, just write a new address there.

	#define BO(n)       ((bo >> (4-n)) & 1)

	void Interpreter::BranchCheck()
	{
		if (core->intFlag && (core->regs.msr & MSR_EE))
		{
			core->Exception(Gekko::Exception::EXCEPTION_EXTERNAL_INTERRUPT);
			core->exception = false;
			return;
		}

		// modify CPU counters (possible CPU_EXCEPTION_DECREMENTER)
		core->Tick();
		if (core->decreq && (core->regs.msr & MSR_EE))
		{
			core->decreq = false;
			core->Exception(Gekko::Exception::EXCEPTION_DECREMENTER);
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
		size_t bo = info.paramBits[0], bi = info.paramBits[1];

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
		size_t bo = info.paramBits[0], bi = info.paramBits[1];

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


// Integer Compare Instructions

namespace Gekko
{

#define SET_CR_LT(n)    (core->regs.cr |=  (1 << (3 + 4 * (7 - n))))
#define SET_CR_GT(n)    (core->regs.cr |=  (1 << (2 + 4 * (7 - n))))
#define SET_CR_EQ(n)    (core->regs.cr |=  (1 << (1 + 4 * (7 - n))))
#define SET_CR_SO(n)    (core->regs.cr |=  (1 << (    4 * (7 - n))))
#define RESET_CR_LT(n)  (core->regs.cr &= ~(1 << (3 + 4 * (7 - n))))
#define RESET_CR_GT(n)  (core->regs.cr &= ~(1 << (2 + 4 * (7 - n))))
#define RESET_CR_EQ(n)  (core->regs.cr &= ~(1 << (1 + 4 * (7 - n))))
#define RESET_CR_SO(n)  (core->regs.cr &= ~(1 << (    4 * (7 - n))))

	// if a < b
	//      then c = 0b100
	//      else if a > b
	//          then c = 0b010
	//          else c = 0b001
	// CR[4*crf..4*crf+3] = c || XER[SO]
	template <typename T>
	inline void Interpreter::CmpCommon(size_t crfd, T a, T b)
	{
		// TODO: Optimize
		if (a < b) SET_CR_LT(crfd); else RESET_CR_LT(crfd);
		if (a > b) SET_CR_GT(crfd); else RESET_CR_GT(crfd);
		if (a == b) SET_CR_EQ(crfd); else RESET_CR_EQ(crfd);
		if (IS_XER_SO) SET_CR_SO(crfd); else RESET_CR_SO(crfd);
		core->regs.pc += 4;
	}

	// a = ra (signed)
	// b = SIMM
	void Interpreter::cmpi()
	{
		int32_t a = core->regs.gpr[info.paramBits[1]];
		int32_t b = (int32_t)info.Imm.Signed;
		CmpCommon<int32_t>(info.paramBits[0], a, b);
	}

	// a = ra (signed)
	// b = rb (signed)
	void Interpreter::cmp()
	{
		int32_t a = core->regs.gpr[info.paramBits[1]];
		int32_t b = core->regs.gpr[info.paramBits[2]];
		CmpCommon<int32_t>(info.paramBits[0], a, b);
	}

	// a = ra (unsigned)
	// b = 0x0000 || UIMM
	void Interpreter::cmpli()
	{
		uint32_t a = core->regs.gpr[info.paramBits[1]];
		uint32_t b = info.Imm.Unsigned;
		CmpCommon<uint32_t>(info.paramBits[0], a, b);
	}

	// a = ra (unsigned)
	// b = rb (unsigned)
	void Interpreter::cmpl()
	{
		uint32_t a = core->regs.gpr[info.paramBits[1]];
		uint32_t b = core->regs.gpr[info.paramBits[2]];
		CmpCommon<uint32_t>(info.paramBits[0], a, b);
	}

}


// Condition Register Logical Instructions

namespace Gekko
{

	// CR[crbd] = CR[crba] & CR[crbb]
	void Interpreter::crand()
	{
		size_t crbd = info.paramBits[0], crba = info.paramBits[1], crbb = info.paramBits[2];

		uint32_t a = (core->regs.cr >> (31 - crba)) & 1;
		uint32_t b = (core->regs.cr >> (31 - crbb)) & 1;
		uint32_t d = (a & b) << (31 - crbd);     // <- crop is here
		uint32_t m = ~(1 << (31 - crbd));
		core->regs.cr = (core->regs.cr & m) | d;
		core->regs.pc += 4;
	}

	// CR[crbd] = CR[crba] & ~CR[crbb]
	void Interpreter::crandc()
	{
		size_t crbd = info.paramBits[0], crba = info.paramBits[1], crbb = info.paramBits[2];

		uint32_t a = (core->regs.cr >> (31 - crba)) & 1;
		uint32_t b = (core->regs.cr >> (31 - crbb)) & 1;
		uint32_t d = (a & (~b)) << (31 - crbd);     // <- crop is here
		uint32_t m = ~(1 << (31 - crbd));
		core->regs.cr = (core->regs.cr & m) | d;
		core->regs.pc += 4;
	}

	// CR[crbd] = CR[crba] EQV CR[crbb]
	void Interpreter::creqv()
	{
		size_t crbd = info.paramBits[0], crba = info.paramBits[1], crbb = info.paramBits[2];

		uint32_t a = (core->regs.cr >> (31 - crba)) & 1;
		uint32_t b = (core->regs.cr >> (31 - crbb)) & 1;
		uint32_t d = (!(a ^ b)) << (31 - crbd);     // <- crop is here
		uint32_t m = ~(1 << (31 - crbd));
		core->regs.cr = (core->regs.cr & m) | d;
		core->regs.pc += 4;
	}

	// CR[crbd] = !(CR[crba] & CR[crbb])
	void Interpreter::crnand()
	{
		size_t crbd = info.paramBits[0], crba = info.paramBits[1], crbb = info.paramBits[2];

		uint32_t a = (core->regs.cr >> (31 - crba)) & 1;
		uint32_t b = (core->regs.cr >> (31 - crbb)) & 1;
		uint32_t d = (!(a & b)) << (31 - crbd);     // <- crop is here
		uint32_t m = ~(1 << (31 - crbd));
		core->regs.cr = (core->regs.cr & m) | d;
		core->regs.pc += 4;
	}

	// CR[crbd] = !(CR[crba] | CR[crbb])
	void Interpreter::crnor()
	{
		size_t crbd = info.paramBits[0], crba = info.paramBits[1], crbb = info.paramBits[2];

		uint32_t a = (core->regs.cr >> (31 - crba)) & 1;
		uint32_t b = (core->regs.cr >> (31 - crbb)) & 1;
		uint32_t d = (!(a | b)) << (31 - crbd);     // <- crop is here
		uint32_t m = ~(1 << (31 - crbd));
		core->regs.cr = (core->regs.cr & m) | d;
		core->regs.pc += 4;
	}

	// CR[crbd] = CR[crba] | CR[crbb]
	void Interpreter::cror()
	{
		size_t crbd = info.paramBits[0], crba = info.paramBits[1], crbb = info.paramBits[2];

		uint32_t a = (core->regs.cr >> (31 - crba)) & 1;
		uint32_t b = (core->regs.cr >> (31 - crbb)) & 1;
		uint32_t d = (a | b) << (31 - crbd);     // <- crop is here
		uint32_t m = ~(1 << (31 - crbd));
		core->regs.cr = (core->regs.cr & m) | d;
		core->regs.pc += 4;
	}

	// CR[crbd] = CR[crba] | ~CR[crbb]
	void Interpreter::crorc()
	{
		size_t crbd = info.paramBits[0], crba = info.paramBits[1], crbb = info.paramBits[2];

		uint32_t a = (core->regs.cr >> (31 - crba)) & 1;
		uint32_t b = (core->regs.cr >> (31 - crbb)) & 1;
		uint32_t d = (a | (~b)) << (31 - crbd);     // <- crop is here
		uint32_t m = ~(1 << (31 - crbd));
		core->regs.cr = (core->regs.cr & m) | d;
		core->regs.pc += 4;
	}

	// CR[crbd] = CR[crba] ^ CR[crbb]
	void Interpreter::crxor()
	{
		size_t crbd = info.paramBits[0], crba = info.paramBits[1], crbb = info.paramBits[2];

		uint32_t a = (core->regs.cr >> (31 - crba)) & 1;
		uint32_t b = (core->regs.cr >> (31 - crbb)) & 1;
		uint32_t d = (a ^ b) << (31 - crbd);     // <- crop is here
		uint32_t m = ~(1 << (31 - crbd));
		core->regs.cr = (core->regs.cr & m) | d;
		core->regs.pc += 4;
	}

	// CR[4*crfd .. 4*crfd + 3] = CR[4*crfs .. 4*crfs + 3]
	void Interpreter::mcrf()
	{
		size_t crfd = 4 * (7 - info.paramBits[0]), crfs = 4 * (7 - info.paramBits[1]);
		core->regs.cr = (core->regs.cr & (~(0xf << crfd))) | (((core->regs.cr >> crfs) & 0xf) << crfd);
		core->regs.pc += 4;
	}

}


// Floating-Point Instructions

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

// Floating-Point Load and Store Instructions

namespace Gekko
{

	// ea = (ra | 0) + SIMM
	// fd = MEM(ea, 8)
	void Interpreter::lfd()
	{
		if (core->regs.msr & MSR_FP)
		{
			if (info.paramBits[1]) core->ReadDouble(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, &FPRU(info.paramBits[0]));
			else core->ReadDouble((int32_t)info.Imm.Signed, &FPRU(info.paramBits[0]));
			if (core->exception) return;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	// ea = ra + SIMM
	// fd = MEM(ea, 8)
	// ra = ea
	void Interpreter::lfdu()
	{
		if (core->regs.msr & MSR_FP)
		{
			uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
			core->ReadDouble(ea, &FPRU(info.paramBits[0]));
			if (core->exception) return;
			core->regs.gpr[info.paramBits[1]] = ea;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	// ea = ra + rb
	// fd = MEM(ea, 8)
	// ra = ea
	void Interpreter::lfdux()
	{
		if (core->regs.msr & MSR_FP)
		{
			uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
			core->ReadDouble(ea, &FPRU(info.paramBits[0]));
			if (core->exception) return;
			core->regs.gpr[info.paramBits[1]] = ea;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	// ea = (ra | 0) + rb
	// fd = MEM(ea, 8)
	void Interpreter::lfdx()
	{
		if (core->regs.msr & MSR_FP)
		{
			if (info.paramBits[1]) core->ReadDouble(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], &FPRU(info.paramBits[0]));
			else core->ReadDouble(core->regs.gpr[info.paramBits[2]], &FPRU(info.paramBits[0]));
			if (core->exception) return;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	// ea = (ra | 0) + SIMM
	// if HID2[PSE] = 0
	//      then fd = DOUBLE(MEM(ea, 4))
	//      else fd(ps0) = SINGLE(MEM(ea, 4))
	//           fd(ps1) = SINGLE(MEM(ea, 4))
	void Interpreter::lfs()
	{
		if (core->regs.msr & MSR_FP)
		{
			float res;

			if (info.paramBits[1]) core->ReadWord(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, (uint32_t*)&res);
			else core->ReadWord((int32_t)info.Imm.Signed, (uint32_t*)&res);

			if (core->exception) return;

			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS0(info.paramBits[0]) = PS1(info.paramBits[0]) = (double)res;
			else FPRD(info.paramBits[0]) = (double)res;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	// ea = ra + SIMM
	// if HID2[PSE] = 0
	//      then fd = DOUBLE(MEM(ea, 4))
	//      else fd(ps0) = SINGLE(MEM(ea, 4))
	//           fd(ps1) = SINGLE(MEM(ea, 4))
	// ra = ea
	void Interpreter::lfsu()
	{
		if (core->regs.msr & MSR_FP)
		{
			uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
			float res;

			core->ReadWord(ea, (uint32_t*)&res);

			if (core->exception) return;

			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS0(info.paramBits[0]) = PS1(info.paramBits[0]) = (double)res;
			else FPRD(info.paramBits[0]) = (double)res;

			core->regs.gpr[info.paramBits[1]] = ea;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	// ea = ra + rb
	// if HID2[PSE] = 0
	//      then fd = DOUBLE(MEM(ea, 4))
	//      else fd(ps0) = SINGLE(MEM(ea, 4))
	//           fd(ps1) = SINGLE(MEM(ea, 4))
	// ra = ea
	void Interpreter::lfsux()
	{
		if (core->regs.msr & MSR_FP)
		{
			uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
			float res;

			core->ReadWord(ea, (uint32_t*)&res);

			if (core->exception) return;

			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS0(info.paramBits[0]) = PS1(info.paramBits[0]) = (double)res;
			else FPRD(info.paramBits[0]) = (double)res;

			core->regs.gpr[info.paramBits[1]] = ea;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	// ea = (ra | 0) + rb
	// if HID2[PSE] = 0
	//      then fd = DOUBLE(MEM(ea, 4))
	//      else fd(ps0) = SINGLE(MEM(ea, 4))
	//           fd(ps1) = SINGLE(MEM(ea, 4))
	void Interpreter::lfsx()
	{
		if (core->regs.msr & MSR_FP)
		{
			float res;

			if (info.paramBits[1]) core->ReadWord(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], (uint32_t*)&res);
			else core->ReadWord(core->regs.gpr[info.paramBits[2]], (uint32_t*)&res);

			if (core->exception) return;

			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS0(info.paramBits[0]) = PS1(info.paramBits[0]) = (double)res;
			else FPRD(info.paramBits[0]) = (double)res;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	// ea = (ra | 0) + SIMM
	// MEM(ea, 8) = fs
	void Interpreter::stfd()
	{
		if (core->regs.msr & MSR_FP)
		{
			if (info.paramBits[1]) core->WriteDouble(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, &FPRU(info.paramBits[0]));
			else core->WriteDouble((int32_t)info.Imm.Signed, &FPRU(info.paramBits[0]));
			if (core->exception) return;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	// ea = ra + SIMM
	// MEM(ea, 8) = fs
	// ra = ea
	void Interpreter::stfdu()
	{
		if (core->regs.msr & MSR_FP)
		{
			uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
			core->WriteDouble(ea, &FPRU(info.paramBits[0]));
			if (core->exception) return;
			core->regs.gpr[info.paramBits[1]] = ea;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	// ea = ra + rb
	// MEM(ea, 8) = fs
	// ra = ea
	void Interpreter::stfdux()
	{
		if (core->regs.msr & MSR_FP)
		{
			uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
			core->WriteDouble(ea, &FPRU(info.paramBits[0]));
			if (core->exception) return;
			core->regs.gpr[info.paramBits[1]] = ea;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	// ea = (ra | 0) + rb
	// MEM(ea, 8) = fs
	void Interpreter::stfdx()
	{
		if (core->regs.msr & MSR_FP)
		{
			if (info.paramBits[1]) core->WriteDouble(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], &FPRU(info.paramBits[0]));
			else core->WriteDouble(core->regs.gpr[info.paramBits[2]], &FPRU(info.paramBits[0]));
			if (core->exception) return;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	// ea = (ra | 0) + rb
	// MEM(ea, 4) = fs[32-63]
	void Interpreter::stfiwx()
	{
		if (core->regs.msr & MSR_FP)
		{
			uint32_t val = (uint32_t)(FPRU(info.paramBits[0]) & 0x00000000ffffffff);
			if (info.paramBits[1]) core->WriteWord(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], val);
			else core->WriteWord(core->regs.gpr[info.paramBits[2]], val);
			if (core->exception) return;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	// ea = (ra | 0) + SIMM
	// MEM(ea, 4) = SINGLE(fs)
	void Interpreter::stfs()
	{
		if (core->regs.msr & MSR_FP)
		{
			float data = (float)FPRD(info.paramBits[0]);
			if (info.paramBits[1]) core->WriteWord(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, *(uint32_t*)&data);
			else core->WriteWord((int32_t)info.Imm.Signed, *(uint32_t*)&data);
			if (core->exception) return;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	// ea = ra + SIMM
	// MEM(ea, 4) = SINGLE(fs)
	// ra = ea
	void Interpreter::stfsu()
	{
		if (core->regs.msr & MSR_FP)
		{
			float data = (float)FPRD(info.paramBits[0]);
			uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
			core->WriteWord(ea, *(uint32_t*)&data);
			if (core->exception) return;
			core->regs.gpr[info.paramBits[1]] = ea;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	// ea = ra + rb
	// MEM(ea, 4) = SINGLE(fs)
	// ra = ea
	void Interpreter::stfsux()
	{
		if (core->regs.msr & MSR_FP)
		{
			float data = (float)FPRD(info.paramBits[0]);
			uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
			core->WriteWord(ea, *(uint32_t*)&data);
			if (core->exception) return;
			core->regs.gpr[info.paramBits[1]] = ea;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	// ea = (ra | 0) + rb
	// MEM(ea, 4) = SINGLE(fs)
	void Interpreter::stfsx()
	{
		if (core->regs.msr & MSR_FP)
		{
			float data = (float)FPRD(info.paramBits[0]);
			if (info.paramBits[1]) core->WriteWord(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], *(uint32_t*)&data);
			else core->WriteWord(core->regs.gpr[info.paramBits[2]], *(uint32_t*)&data);
			if (core->exception) return;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

}

// Integer Instructions

namespace Gekko
{
	// We use macro programming to compress the source code.
	// Now I am not very willing to use such things.
	#define GPR(n) (core->regs.gpr[info.paramBits[(n)]])

	// rd = ra + rb
	void Interpreter::add()
	{
		GPR(0) = GPR(1) + GPR(2);
		core->regs.pc += 4;
	}

	// rd = ra + rb, CR0
	void Interpreter::add_d()
	{
		add();
		COMPUTE_CR0(GPR(0));
	}

	// rd = ra + rb, XER
	void Interpreter::addo()
	{
		CarryBit = 0;
		GPR(0) = FullAdder(GPR(1), GPR(2));
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); }
		else RESET_XER_OV();
		core->regs.pc += 4;
	}

	// rd = ra + rb, CR0, XER
	void Interpreter::addo_d()
	{
		addo();
		COMPUTE_CR0(GPR(0));
	}

	// rd = ra + rb, XER[CA]
	void Interpreter::addc()
	{
		CarryBit = 0;
		GPR(0) = FullAdder(GPR(1), GPR(2));
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ra + rb, XER[CA], CR0
	void Interpreter::addc_d()
	{
		addc();
		COMPUTE_CR0(GPR(0));
	}

	// rd = ra + rb, XER[CA], XER[OV]
	void Interpreter::addco()
	{
		CarryBit = 0;
		GPR(0) = FullAdder(GPR(1), GPR(2));
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); }
		else RESET_XER_OV();
		core->regs.pc += 4;
	}

	// rd = ra + rb, XER[CA], XER[OV], CR0
	void Interpreter::addco_d()
	{
		addco();
		COMPUTE_CR0(GPR(0));
	}

	// rd = ra + rb + XER[CA], XER
	void Interpreter::adde()
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(GPR(1), GPR(2));
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ra + rb + XER[CA], CR0, XER
	void Interpreter::adde_d()
	{
		adde();
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::addeo()
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(GPR(1), GPR(2));
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); }
		else RESET_XER_OV();
		core->regs.pc += 4;
	}

	void Interpreter::addeo_d()
	{
		addeo();
		COMPUTE_CR0(GPR(0));
	}

	// rd = (ra | 0) + SIMM
	void Interpreter::addi()
	{
		if (info.paramBits[1]) GPR(0) = GPR(1) + (int32_t)info.Imm.Signed;
		else GPR(0) = (int32_t)info.Imm.Signed;
		core->regs.pc += 4;
	}

	// rd = ra + SIMM, XER
	void Interpreter::addic()
	{
		CarryBit = 0;
		GPR(0) = FullAdder(GPR(1), (int32_t)info.Imm.Signed);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ra + SIMM, CR0, XER
	void Interpreter::addic_d()
	{
		addic();
		COMPUTE_CR0(GPR(0));
	}

	// rd = (ra | 0) + (SIMM || 0x0000)
	void Interpreter::addis()
	{
		if (info.paramBits[1]) GPR(0) = GPR(1) + ((int32_t)info.Imm.Signed << 16);
		else GPR(0) = (int32_t)info.Imm.Signed << 16;
		core->regs.pc += 4;
	}

	// rd = ra + XER[CA] - 1 (0xffffffff), XER
	void Interpreter::addme()
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(GPR(1), -1);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ra + XER[CA] - 1 (0xffffffff), CR0, XER
	void Interpreter::addme_d()
	{
		addme();
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::addmeo()
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(GPR(1), -1);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); }
		else RESET_XER_OV();
		core->regs.pc += 4;
	}

	void Interpreter::addmeo_d()
	{
		addmeo();
		COMPUTE_CR0(GPR(0));
	}

	// rd = ra + XER[CA], XER
	void Interpreter::addze()
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(GPR(1), 0);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ra + XER[CA], CR0, XER
	void Interpreter::addze_d()
	{
		addze();
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::addzeo()
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(GPR(1), 0);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); }
		else RESET_XER_OV();
		core->regs.pc += 4;
	}

	void Interpreter::addzeo_d()
	{
		addzeo();
		COMPUTE_CR0(GPR(0));
	}

	// rd = ra / rb (signed)
	void Interpreter::divw()
	{
		int32_t a = GPR(1), b = GPR(2);
		if (b) GPR(0) = a / b;
		core->regs.pc += 4;
	}

	// rd = ra / rb (signed), CR0
	void Interpreter::divw_d()
	{
		int32_t a = GPR(1), b = GPR(2), res;
		if (b)
		{
			res = a / b;
			GPR(0) = res;
			COMPUTE_CR0(res);
		}
		core->regs.pc += 4;
	}

	void Interpreter::divwo()
	{
		Halt("divwo\n");
	}

	void Interpreter::divwo_d()
	{
		divwo();
		COMPUTE_CR0(GPR(0));
	}

	// rd = ra / rb (unsigned)
	void Interpreter::divwu()
	{
		uint32_t a = GPR(1), b = GPR(2);
		if (b) GPR(0) = a / b;
		core->regs.pc += 4;
	}

	// rd = ra / rb (unsigned), CR0
	void Interpreter::divwu_d()
	{
		uint32_t a = GPR(1), b = GPR(2), res;
		if (b)
		{
			res = a / b;
			GPR(0) = res;
			COMPUTE_CR0(res);
		}
		core->regs.pc += 4;
	}

	void Interpreter::divwuo()
	{
		Halt("divwuo\n");
	}

	void Interpreter::divwuo_d()
	{
		divwuo();
		COMPUTE_CR0(GPR(0));
	}

	// prod[0-63] = ra * rb
	// rd = prod[0-31]
	void Interpreter::mulhw()
	{
		int64_t a = (int32_t)GPR(1), b = (int32_t)GPR(2), res = a * b;
		res = (res >> 32);
		GPR(0) = (int32_t)res;
		core->regs.pc += 4;
	}

	// prod[0-63] = ra * rb
	// rd = prod[0-31]
	// CR0
	void Interpreter::mulhw_d()
	{
		mulhw();
		COMPUTE_CR0(GPR(0));
	}

	// prod[0-63] = ra * rb
	// rd = prod[0-31]
	void Interpreter::mulhwu()
	{
		uint64_t a = GPR(1), b = GPR(2), res = a * b;
		res = (res >> 32);
		GPR(0) = (uint32_t)res;
		core->regs.pc += 4;
	}

	// prod[0-63] = ra * rb
	// rd = prod[0-31]
	// CR0
	void Interpreter::mulhwu_d()
	{
		mulhwu();
		COMPUTE_CR0(GPR(0));
	}

	// prod[0-48] = ra * SIMM
	// rd = prod[16-48]
	void Interpreter::mulli()
	{
		GPR(0) = GPR(1) * (int32_t)info.Imm.Signed;
		core->regs.pc += 4;
	}

	// prod[0-48] = ra * rb
	// rd = prod[16-48]
	void Interpreter::mullw()
	{
		int32_t a = GPR(1), b = GPR(2);
		int64_t res = (int64_t)a * (int64_t)b;
		GPR(0) = (int32_t)(res & 0x00000000ffffffff);
		core->regs.pc += 4;
	}

	// prod[0-48] = ra * rb
	// rd = prod[16-48]
	// CR0
	void Interpreter::mullw_d()
	{
		mullw();
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::mullwo()
	{
		Halt("mullwo\n");
	}

	void Interpreter::mullwo_d()
	{
		mullwo();
		COMPUTE_CR0(GPR(0));
	}

	// rd = ~ra + 1
	void Interpreter::neg()
	{
		GPR(0) = ~GPR(1) + 1;
		core->regs.pc += 4;
	}

	// rd = ~ra + 1, CR0
	void Interpreter::neg_d()
	{
		neg();
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::nego()
	{
		CarryBit = 0;
		GPR(0) = FullAdder(~GPR(1), 1);
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); }
		else RESET_XER_OV();
		core->regs.pc += 4;
	}

	void Interpreter::nego_d()
	{
		nego();
		COMPUTE_CR0(GPR(0));
	}

	// rd = ~ra + rb + 1
	void Interpreter::subf()
	{
		GPR(0) = ~GPR(1) + GPR(2) + 1;
		core->regs.pc += 4;
	}

	// rd = ~ra + rb + 1, CR0
	void Interpreter::subf_d()
	{
		subf();
		COMPUTE_CR0(GPR(0));
	}

	// rd = ~ra + rb + 1, XER
	void Interpreter::subfo()
	{
		CarryBit = 1;
		GPR(0) = FullAdder(~GPR(1), GPR(2));
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); }
		else RESET_XER_OV();
		core->regs.pc += 4;
	}

	// rd = ~ra + rb + 1, CR0, XER
	void Interpreter::subfo_d()
	{
		subfo();
		COMPUTE_CR0(GPR(0));
	}

	// rd = ~ra + rb + 1, XER[CA]
	void Interpreter::subfc()
	{
		CarryBit = 1;
		GPR(0) = FullAdder(~GPR(1), GPR(2));
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ~ra + rb + 1, XER[CA], CR0
	void Interpreter::subfc_d()
	{
		subfc();
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::subfco()
	{
		CarryBit = 1;
		GPR(0) = FullAdder(~GPR(1), GPR(2));
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); }
		else RESET_XER_OV();
		core->regs.pc += 4;
	}

	void Interpreter::subfco_d()
	{
		subfco();
		COMPUTE_CR0(GPR(0));
	}

	// rd = ~ra + rb + XER[CA], XER
	void Interpreter::subfe()
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(~GPR(1), GPR(2));
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ~ra + rb + XER[CA], CR0, XER
	void Interpreter::subfe_d()
	{
		subfe();
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::subfeo()
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(~GPR(1), GPR(2));
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); }
		else RESET_XER_OV();
		core->regs.pc += 4;
	}

	void Interpreter::subfeo_d()
	{
		subfeo();
		COMPUTE_CR0(GPR(0));
	}

	// rd = ~RRA + SIMM + 1, XER
	void Interpreter::subfic()
	{
		CarryBit = 1;
		GPR(0) = FullAdder(~GPR(1), (int32_t)info.Imm.Signed);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ~ra + XER[CA] - 1, XER
	void Interpreter::subfme()
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(~GPR(1), -1);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ~ra + XER[CA] - 1, CR0, XER
	void Interpreter::subfme_d()
	{
		subfme();
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::subfmeo()
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(~GPR(1), -1);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); }
		else RESET_XER_OV();
		core->regs.pc += 4;
	}

	void Interpreter::subfmeo_d()
	{
		subfmeo();
		COMPUTE_CR0(GPR(0));
	}

	// rd = ~ra + XER[CA], XER
	void Interpreter::subfze()
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(~GPR(1), 0);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ~ra + XER[CA], CR0, XER
	void Interpreter::subfze_d()
	{
		subfze();
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::subfzeo()
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(~GPR(1), 0);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); }
		else RESET_XER_OV();
		core->regs.pc += 4;
	}

	void Interpreter::subfzeo_d()
	{
		subfzeo();
		COMPUTE_CR0(GPR(0));
	}

}


// Integer Load and Store Instructions

namespace Gekko
{

	// ea = (ra | 0) + SIMM
	// rd = 0x000000 || MEM(ea, 1)
	void Interpreter::lbz()
	{
		if (info.paramBits[1]) core->ReadByte(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, &core->regs.gpr[info.paramBits[0]]);
		else core->ReadByte((int32_t)info.Imm.Signed, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = ra + SIMM
	// rd = 0x000000 || MEM(ea, 1)
	// ra = ea
	void Interpreter::lbzu()
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		core->ReadByte(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = ra + rb
	// rd = 0x000000 || MEM(ea, 1)
	// ra = ea
	void Interpreter::lbzux()
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
		core->ReadByte(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// rd = 0x000000 || MEM(ea, 1)
	void Interpreter::lbzx()
	{
		if (info.paramBits[1]) core->ReadByte(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], &core->regs.gpr[info.paramBits[0]]);
		else core->ReadByte(core->regs.gpr[info.paramBits[2]], &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + SIMM
	// rd = (signed)MEM(ea, 2)
	void Interpreter::lha()
	{
		if (info.paramBits[1]) core->ReadHalf(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, &core->regs.gpr[info.paramBits[0]]);
		else core->ReadHalf((int32_t)info.Imm.Signed, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		if (core->regs.gpr[info.paramBits[0]] & 0x8000) core->regs.gpr[info.paramBits[0]] |= 0xffff0000;
		core->regs.pc += 4;
	}

	// ea = ra + SIMM
	// rd = (signed)MEM(ea, 2)
	// ra = ea
	void Interpreter::lhau()
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		core->ReadHalf(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		if (core->regs.gpr[info.paramBits[0]] & 0x8000) core->regs.gpr[info.paramBits[0]] |= 0xffff0000;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = ra + rb
	// rd = (signed)MEM(ea, 2)
	// ra = ea
	void Interpreter::lhaux()
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
		core->ReadHalf(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		if (core->regs.gpr[info.paramBits[0]] & 0x8000) core->regs.gpr[info.paramBits[0]] |= 0xffff0000;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// rd = (signed)MEM(ea, 2)
	void Interpreter::lhax()
	{
		if (info.paramBits[1]) core->ReadHalf(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], &core->regs.gpr[info.paramBits[0]]);
		else core->ReadHalf(core->regs.gpr[info.paramBits[2]], &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		if (core->regs.gpr[info.paramBits[0]] & 0x8000) core->regs.gpr[info.paramBits[0]] |= 0xffff0000;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + SIMM
	// rd = 0x0000 || MEM(ea, 2)
	void Interpreter::lhz()
	{
		if (info.paramBits[1]) core->ReadHalf(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, &core->regs.gpr[info.paramBits[0]]);
		else core->ReadHalf((int32_t)info.Imm.Signed, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = ra + SIMM
	// rd = 0x0000 || MEM(ea, 2)
	// ra = ea
	void Interpreter::lhzu()
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		core->ReadHalf(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = ra + rb
	// rd = 0x0000 || MEM(ea, 2)
	// ra = ea
	void Interpreter::lhzux()
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
		core->ReadHalf(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// rd = 0x0000 || MEM(ea, 2)
	void Interpreter::lhzx()
	{
		if (info.paramBits[1]) core->ReadHalf(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], &core->regs.gpr[info.paramBits[0]]);
		else core->ReadHalf(core->regs.gpr[info.paramBits[2]], &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + SIMM
	// rd = MEM(ea, 4)
	void Interpreter::lwz()
	{
		if (info.paramBits[1]) core->ReadWord(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, &core->regs.gpr[info.paramBits[0]]);
		else core->ReadWord((int32_t)info.Imm.Signed, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = ra + SIMM
	// rd = MEM(ea, 4)
	// ra = ea
	void Interpreter::lwzu()
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		core->ReadWord(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = ra + rb
	// rd = MEM(ea, 4)
	// ra = ea
	void Interpreter::lwzux()
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
		core->ReadWord(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// rd = MEM(ea, 4)
	void Interpreter::lwzx()
	{
		if (info.paramBits[1]) core->ReadWord(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], &core->regs.gpr[info.paramBits[0]]);
		else core->ReadWord(core->regs.gpr[info.paramBits[2]], &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + SIMM
	// MEM(ea, 1) = rs[24-31]
	void Interpreter::stb()
	{
		if (info.paramBits[1]) core->WriteByte(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, core->regs.gpr[info.paramBits[0]]);
		else core->WriteByte((int32_t)info.Imm.Signed, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = ra + SIMM
	// MEM(ea, 1) = rs[24-31]
	// ra = ea
	void Interpreter::stbu()
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		core->WriteByte(ea, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = ra + rb
	// MEM(ea, 1) = rs[24-31]
	// ra = ea
	void Interpreter::stbux()
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
		core->WriteByte(ea, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// MEM(ea, 1) = rs[24-31]
	void Interpreter::stbx()
	{
		if (info.paramBits[1]) core->WriteByte(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], core->regs.gpr[info.paramBits[0]]);
		else core->WriteByte(core->regs.gpr[info.paramBits[2]], core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + SIMM
	// MEM(ea, 2) = rs[16-31]
	void Interpreter::sth()
	{
		if (info.paramBits[1]) core->WriteHalf(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, core->regs.gpr[info.paramBits[0]]);
		else core->WriteHalf((int32_t)info.Imm.Signed, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = ra + SIMM
	// MEM(ea, 2) = rs[16-31]
	// ra = ea
	void Interpreter::sthu()
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		core->WriteHalf(ea, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = ra + rb
	// MEM(ea, 2) = rs[16-31]
	// ra = ea
	void Interpreter::sthux()
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
		core->WriteHalf(ea, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// MEM(ea, 2) = rs[16-31]
	void Interpreter::sthx()
	{
		if (info.paramBits[1]) core->WriteHalf(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], core->regs.gpr[info.paramBits[0]]);
		else core->WriteHalf(core->regs.gpr[info.paramBits[2]], core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + SIMM
	// MEM(ea, 4) = rs
	void Interpreter::stw()
	{
		if (info.paramBits[1]) core->WriteWord(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, core->regs.gpr[info.paramBits[0]]);
		else core->WriteWord((int32_t)info.Imm.Signed, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = ra + SIMM
	// MEM(ea, 4) = rs
	// ra = ea
	void Interpreter::stwu()
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		core->WriteWord(ea, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = ra + rb
	// MEM(ea, 4) = rs
	// ra = ea
	void Interpreter::stwux()
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
		core->WriteWord(ea, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// MEM(ea, 4) = rs
	void Interpreter::stwx()
	{
		if (info.paramBits[1]) core->WriteWord(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], core->regs.gpr[info.paramBits[0]]);
		else core->WriteWord(core->regs.gpr[info.paramBits[2]], core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// rd = 0x0000 || MEM(ea+1, 1) || MEM(EA, 1)
	void Interpreter::lhbrx()
	{
		uint32_t val;
		if (info.paramBits[1]) core->ReadHalf(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], &val);
		else core->ReadHalf(core->regs.gpr[info.paramBits[2]], &val);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[0]] = _BYTESWAP_UINT16((uint16_t)val);
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// rd = MEM(ea+3, 1) || MEM(ea+2, 1) || MEM(ea+1, 1) || MEM(ea, 1)
	void Interpreter::lwbrx()
	{
		uint32_t val;
		if (info.paramBits[1]) core->ReadWord(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], &val);
		else core->ReadWord(core->regs.gpr[info.paramBits[2]], &val);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[0]] = _BYTESWAP_UINT32(val);
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// MEM(ea, 2) = rs[24-31] || rs[16-23]
	void Interpreter::sthbrx()
	{
		if (info.paramBits[1]) core->WriteHalf(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], _BYTESWAP_UINT16((uint16_t)core->regs.gpr[info.paramBits[0]]));
		else core->WriteHalf(core->regs.gpr[info.paramBits[2]], _BYTESWAP_UINT16((uint16_t)core->regs.gpr[info.paramBits[0]]));
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// MEM(ea, 4) = rs[24-31] || rs[16-23] || rs[8-15] || rs[0-7]
	void Interpreter::stwbrx()
	{
		if (info.paramBits[1]) core->WriteWord(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], _BYTESWAP_UINT32(core->regs.gpr[info.paramBits[0]]));
		else core->WriteWord(core->regs.gpr[info.paramBits[2]], _BYTESWAP_UINT32(core->regs.gpr[info.paramBits[0]]));
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + SIMM
	// r = rd
	// while r <= 31
	//      GPR(r) = MEM(ea, 4)
	//      r = r + 1
	//      ea = ea + 4
	void Interpreter::lmw()
	{
		uint32_t ea;
		if (info.paramBits[1]) ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		else ea = (int32_t)info.Imm.Signed;

		for (size_t r = info.paramBits[0]; r < 32; r++, ea += 4)
		{
			core->ReadWord(ea, &core->regs.gpr[r]);
			if (core->exception) return;
		}
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + SIMM
	// r = rs
	// while r <= 31
	//      MEM(ea, 4) = GPR(r)
	//      r = r + 1
	//      ea = ea + 4
	void Interpreter::stmw()
	{
		uint32_t ea;
		if (info.paramBits[1]) ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		else ea = (int32_t)info.Imm.Signed;

		for (size_t r = info.paramBits[0]; r < 32; r++, ea += 4)
		{
			core->WriteWord(ea, core->regs.gpr[r]);
			if (core->exception) return;
		}
		core->regs.pc += 4;
	}

	// ea = (ra | 0)
	// n = NB ? NB : 32
	// r = rd - 1
	// i = 0
	// while n > 0
	//      if i = 0 then
	//          r = (r + 1) % 32
	//          GPR(r) = 0
	//      GPR(r)[i...i+7] = MEM(ea, 1)
	//      i = i + 8
	//      if i = 32 then i = 0
	//      ea = ea + 1
	//      n = n -1
	void Interpreter::lswi()
	{
		int32_t rd = info.paramBits[0], n = (info.paramBits[2]) ? (info.paramBits[2]) : 32, i = 4;
		uint32_t ea = (info.paramBits[1]) ? (core->regs.gpr[info.paramBits[1]]) : 0;
		uint32_t r = 0, val;

		while (n > 0)
		{
			if (i == 0)
			{
				i = 4;
				core->regs.gpr[rd] = r;
				rd++;
				rd %= 32;
				r = 0;
			}
			core->ReadByte(ea, &val);
			if (core->exception) return;
			r <<= 8;
			r |= (uint8_t)val;
			ea++;
			i--;
			n--;
		}

		if (i != 0)
		{
			while (i)
			{
				r <<= 8;
				i--;
			}
			core->regs.gpr[rd] = r;
		}

		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// n = XER[25-31]
	// r = rd - 1
	// i = 0
	// while n > 0
	//      if i = 0 then
	//          r = (r + 1) % 32
	//          GPR(r) = 0
	//      GPR(r)[i...i+7] = MEM(ea, 1)
	//      i = i + 8
	//      if i = 32 then i = 0
	//      ea = ea + 1
	//      n = n -1
	void Interpreter::lswx()
	{
		int32_t rd = info.paramBits[0], n = core->regs.spr[SPR::XER] & 0x7f, i = 4;
		uint32_t ea = ((info.paramBits[1]) ? (core->regs.gpr[info.paramBits[1]]) : 0) + core->regs.gpr[info.paramBits[2]];
		uint32_t r = 0, val;

		while (n > 0)
		{
			if (i == 0)
			{
				i = 4;
				core->regs.gpr[rd] = r;
				rd++;
				rd %= 32;
				r = 0;
			}
			core->ReadByte(ea, &val);
			if (core->exception) return;
			r <<= 8;
			r |= (uint8_t)val;
			ea++;
			i--;
			n--;
		}

		if (i != 0)
		{
			while (i)
			{
				r <<= 8;
				i--;
			}
			core->regs.gpr[rd] = r;
		}

		core->regs.pc += 4;
	}

	// ea = (ra | 0)
	// n = NB ? NB : 32
	// r = rs - 1
	// i = 0
	// while n > 0
	//      if i = 0 then r = (r + 1) % 32
	//      MEM(ea, 1) = GPR(r)[i...i+7]
	//      i = i + 8
	//      if i = 32 then i = 0;
	//      ea = ea + 1
	//      n = n -1
	void Interpreter::stswi()
	{
		int32_t rs = info.paramBits[0], n = (info.paramBits[2]) ? (info.paramBits[2]) : 32, i = 0;
		uint32_t ea = (info.paramBits[1]) ? (core->regs.gpr[info.paramBits[1]]) : 0;
		uint32_t r = 0;

		while (n > 0)
		{
			if (i == 0)
			{
				r = core->regs.gpr[rs];
				rs++;
				rs %= 32;
				i = 4;
			}
			core->WriteByte(ea, r >> 24);
			if (core->exception) return;
			r <<= 8;
			ea++;
			i--;
			n--;
		}
		core->regs.pc += 4;
	}

	// ea = (ra | 0)
	// n = XER[25-31]
	// r = rs - 1
	// i = 0
	// while n > 0
	//      if i = 0 then r = (r + 1) % 32
	//      MEM(ea, 1) = GPR(r)[i...i+7]
	//      i = i + 8
	//      if i = 32 then i = 0;
	//      ea = ea + 1
	//      n = n -1
	void Interpreter::stswx()
	{
		int32_t rs = info.paramBits[0], n = core->regs.spr[SPR::XER] & 0x7f, i = 0;
		uint32_t ea = ((info.paramBits[1]) ? (core->regs.gpr[info.paramBits[1]]) : 0) + core->regs.gpr[info.paramBits[2]];
		uint32_t r = 0;

		while (n > 0)
		{
			if (i == 0)
			{
				r = core->regs.gpr[rs];
				rs++;
				rs %= 32;
				i = 4;
			}
			core->WriteByte(ea, r >> 24);
			if (core->exception) return;
			r <<= 8;
			ea++;
			i--;
			n--;
		}
		core->regs.pc += 4;
	}

}



// Logical Instructions

namespace Gekko
{
	// ra = rs & rb
	void Interpreter::_and()
	{
		GPR(0) = GPR(1) & GPR(2);
		core->regs.pc += 4;
	}

	// ra = rs & rb, CR0
	void Interpreter::and_d()
	{
		_and();
		COMPUTE_CR0(GPR(0));
	}

	// ra = rs & ~rb
	void Interpreter::andc()
	{
		GPR(0) = GPR(1) & (~GPR(2));
		core->regs.pc += 4;
	}

	// ra = rs & ~rb, CR0
	void Interpreter::andc_d()
	{
		andc();
		COMPUTE_CR0(GPR(0));
	}

	// ra = rs & UIMM, CR0
	void Interpreter::andi_d()
	{
		uint32_t res = GPR(1) & (uint32_t)info.Imm.Unsigned;
		GPR(0) = res;
		COMPUTE_CR0(res);
		core->regs.pc += 4;
	}

	// ra = rs & (UIMM || 0x0000), CR0
	void Interpreter::andis_d()
	{
		uint32_t res = GPR(1) & ((uint32_t)info.Imm.Unsigned << 16);
		GPR(0) = res;
		COMPUTE_CR0(res);
		core->regs.pc += 4;
	}

	// n = 0
	// while n < 32
	//      if rs[n] = 1 then leave
	//      n = n + 1
	// ra = n
	void Interpreter::cntlzw()
	{
		uint32_t n, m, rs = GPR(1);
		for (n = 0, m = 1 << 31; n < 32; n++, m >>= 1)
		{
			if (rs & m) break;
		}

		GPR(0) = n;
		core->regs.pc += 4;
	}

	// n = 0
	// while n < 32
	//      if rs[n] = 1 then leave
	//      n = n + 1
	// ra = n
	// CR0
	void Interpreter::cntlzw_d()
	{
		cntlzw();
		COMPUTE_CR0(GPR(0));
	}

	// ra = rs EQV rb
	void Interpreter::eqv()
	{
		GPR(0) = ~(GPR(1) ^ GPR(2));
		core->regs.pc += 4;
	}

	// ra = rs EQV rb, CR0
	void Interpreter::eqv_d()
	{
		eqv();
		COMPUTE_CR0(GPR(0));
	}

	// sign = rs[24]
	// ra[24-31] = rs[24-31]
	// ra[0-23] = (24)sign
	void Interpreter::extsb()
	{
		GPR(0) = (uint32_t)(int32_t)(int8_t)(uint8_t)GPR(1);
		core->regs.pc += 4;
	}

	// sign = rs[24]
	// ra[24-31] = rs[24-31]
	// ra[0-23] = (24)sign
	// CR0
	void Interpreter::extsb_d()
	{
		extsb();
		COMPUTE_CR0(GPR(0));
	}

	// sign = rs[16]
	// ra[16-31] = rs[16-31]
	// ra[0-15] = (16)sign
	void Interpreter::extsh()
	{
		GPR(0) = (uint32_t)(int32_t)(int16_t)(uint16_t)GPR(1);
		core->regs.pc += 4;
	}

	// sign = rs[16]
	// ra[16-31] = rs[16-31]
	// ra[0-15] = (16)sign
	// CR0
	void Interpreter::extsh_d()
	{
		extsh();
		COMPUTE_CR0(GPR(0));
	}

	// ra = ~(rs & rb)
	void Interpreter::nand()
	{
		GPR(0) = ~(GPR(1) & GPR(2));
		core->regs.pc += 4;
	}

	// ra = ~(rs & rb), CR0
	void Interpreter::nand_d()
	{
		nand();
		COMPUTE_CR0(GPR(0));
	}

	// ra = ~(rs | rb)
	void Interpreter::nor()
	{
		GPR(0) = ~(GPR(1) | GPR(2));
		core->regs.pc += 4;
	}

	// ra = ~(rs | rb), CR0
	void Interpreter::nor_d()
	{
		nor();
		COMPUTE_CR0(GPR(0));
	}

	// ra = rs | rb
	void Interpreter::_or()
	{
		GPR(0) = GPR(1) | GPR(2);
		core->regs.pc += 4;
	}

	// ra = rs | rb, CR0
	void Interpreter::or_d()
	{
		_or();
		COMPUTE_CR0(GPR(0));
	}

	// ra = rs | ~rb
	void Interpreter::orc()
	{
		GPR(0) = GPR(1) | (~GPR(2));
		core->regs.pc += 4;
	}

	// ra = rs | ~rb, CR0
	void Interpreter::orc_d()
	{
		orc();
		COMPUTE_CR0(GPR(0));
	}

	// ra = rs | (0x0000 || UIMM)
	void Interpreter::ori()
	{
		GPR(0) = GPR(1) | (uint32_t)info.Imm.Unsigned;
		core->regs.pc += 4;
	}

	// ra = rs | (UIMM || 0x0000)
	void Interpreter::oris()
	{
		GPR(0) = GPR(1) | ((uint32_t)info.Imm.Unsigned << 16);
		core->regs.pc += 4;
	}

	// ra = rs ^ rb
	void Interpreter::_xor()
	{
		GPR(0) = GPR(1) ^ GPR(2);
		core->regs.pc += 4;
	}

	// ra = rs ^ rb, CR0
	void Interpreter::xor_d()
	{
		_xor();
		COMPUTE_CR0(GPR(0));
	}

	// ra = rs ^ (0x0000 || UIMM)
	void Interpreter::xori()
	{
		GPR(0) = GPR(1) ^ (uint32_t)info.Imm.Unsigned;
		core->regs.pc += 4;
	}

	// ra = rs ^ (UIMM || 0x0000)
	void Interpreter::xoris()
	{
		GPR(0) = GPR(1) ^ ((uint32_t)info.Imm.Unsigned << 16);
		core->regs.pc += 4;
	}

}


// Paired Single Instructions

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

// Paired Single Load and Store Instructions
// used for fast type casting and matrix transfers.

namespace Gekko
{
#define LD_SCALE(n) ((core->regs.spr[SPR::GQRs + n] >> 24) & 0x3f)
#define LD_TYPE(n)  (GEKKO_QUANT_TYPE)((core->regs.spr[SPR::GQRs + n] >> 16) & 7)
#define ST_SCALE(n) ((core->regs.spr[SPR::GQRs + n] >>  8) & 0x3f)
#define ST_TYPE(n)  (GEKKO_QUANT_TYPE)((core->regs.spr[SPR::GQRs + n]      ) & 7)

	// INT -> float (F = I * 2 ** -S)
	float Interpreter::dequantize(uint32_t data, GEKKO_QUANT_TYPE type, uint8_t scale)
	{
		float flt;

		switch (type)
		{
		case GEKKO_QUANT_TYPE::U8: flt = (float)(uint8_t)data; break;
		case GEKKO_QUANT_TYPE::U16: flt = (float)(uint16_t)data; break;
		case GEKKO_QUANT_TYPE::S8:
			if (data & 0x80) data |= 0xffffff00;
			flt = (float)(int8_t)data; break;
		case GEKKO_QUANT_TYPE::S16:
			if (data & 0x8000) data |= 0xffff0000;
			flt = (float)(int16_t)data; break;
		case GEKKO_QUANT_TYPE::SINGLE_FLOAT:
		default: flt = *((float*)&data); break;
		}

		return flt * core->interp->ldScale[scale];
	}

	// float -> INT (I = ROUND(F * 2 ** S))
	uint32_t Interpreter::quantize(float data, GEKKO_QUANT_TYPE type, uint8_t scale)
	{
		uint32_t uval;

		data *= core->interp->stScale[scale];

		switch (type)
		{
		case GEKKO_QUANT_TYPE::U8:
			if (data < 0) data = 0;
			if (data > 255) data = 255;
			uval = (uint8_t)(uint32_t)data; break;
		case GEKKO_QUANT_TYPE::U16:
			if (data < 0) data = 0;
			if (data > 65535) data = 65535;
			uval = (uint16_t)(uint32_t)data; break;
		case GEKKO_QUANT_TYPE::S8:
			if (data < -128) data = -128;
			if (data > 127) data = 127;
			uval = (int8_t)(uint8_t)(int32_t)(uint32_t)data; break;
		case GEKKO_QUANT_TYPE::S16:
			if (data < -32768) data = -32768;
			if (data > 32767) data = 32767;
			uval = (int16_t)(uint16_t)(int32_t)(uint32_t)data; break;
		case GEKKO_QUANT_TYPE::SINGLE_FLOAT:
		default: *((float*)&uval) = data; break;
		}

		return uval;
	}

	void Interpreter::psq_lx()
	{
		if ((core->regs.spr[SPR::HID2] & HID2_PSE) == 0 ||
			(core->regs.spr[SPR::HID2] & HID2_LSQE) == 0)
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::EXCEPTION_PROGRAM);
			return;
		}

		if (core->regs.msr & MSR_FP)
		{
			size_t i = info.paramBits[4];
			uint32_t EA = core->regs.gpr[info.paramBits[2]], data0, data1;
			int32_t d = info.paramBits[0];
			uint8_t scale = (uint8_t)LD_SCALE(i);
			GEKKO_QUANT_TYPE type = LD_TYPE(i);

			if (info.paramBits[1]) EA += core->regs.gpr[info.paramBits[1]];

			if (info.paramBits[3] /* W */)
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA, &data0);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA, &data0);
				else core->ReadWord(EA, &data0);

				if (core->exception) return;

				PS0(d) = (double)dequantize(data0, type, scale);
				PS1(d) = 1.0f;
			}
			else
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA, &data0);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA, &data0);
				else core->ReadWord(EA, &data0);

				if (core->exception) return;

				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA + 1, &data1);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA + 2, &data1);
				else core->ReadWord(EA + 4, &data1);

				if (core->exception) return;

				PS0(d) = (double)dequantize(data0, type, scale);
				PS1(d) = (double)dequantize(data1, type, scale);
			}

			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::psq_stx()
	{
		if ((core->regs.spr[SPR::HID2] & HID2_PSE) == 0 ||
			(core->regs.spr[SPR::HID2] & HID2_LSQE) == 0)
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::EXCEPTION_PROGRAM);
			return;
		}

		if (core->regs.msr & MSR_FP)
		{
			size_t i = info.paramBits[4];
			uint32_t EA = core->regs.gpr[info.paramBits[2]];
			int32_t d = info.paramBits[0];
			uint8_t scale = (uint8_t)ST_SCALE(i);
			GEKKO_QUANT_TYPE type = ST_TYPE(i);

			if (info.paramBits[1]) EA += core->regs.gpr[info.paramBits[1]];

			if (info.paramBits[3] /* W */)
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA, quantize((float)PS0(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA, quantize((float)PS0(d), type, scale));
				else core->WriteWord(EA, quantize((float)PS0(d), type, scale));
			}
			else
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA, quantize((float)PS0(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA, quantize((float)PS0(d), type, scale));
				else core->WriteWord(EA, quantize((float)PS0(d), type, scale));

				if (core->exception) return;

				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA + 1, quantize((float)PS1(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA + 2, quantize((float)PS1(d), type, scale));
				else core->WriteWord(EA + 4, quantize((float)PS1(d), type, scale));
			}

			if (core->exception) return;

			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::psq_lux()
	{
		if ((core->regs.spr[SPR::HID2] & HID2_PSE) == 0 ||
			(core->regs.spr[SPR::HID2] & HID2_LSQE) == 0 ||
			info.paramBits[1] == 0)
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::EXCEPTION_PROGRAM);
			return;
		}

		if (core->regs.msr & MSR_FP)
		{
			size_t i = info.paramBits[4];
			uint32_t EA = core->regs.gpr[info.paramBits[2]], data0, data1;
			int32_t d = info.paramBits[0];
			uint8_t scale = (uint8_t)LD_SCALE(i);
			GEKKO_QUANT_TYPE type = LD_TYPE(i);

			EA += core->regs.gpr[info.paramBits[1]];

			if (info.paramBits[3] /* W */)
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA, &data0);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA, &data0);
				else core->ReadWord(EA, &data0);

				if (core->exception) return;

				PS0(d) = (double)dequantize(data0, type, scale);
				PS1(d) = 1.0f;
			}
			else
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA, &data0);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA, &data0);
				else core->ReadWord(EA, &data0);

				if (core->exception) return;

				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA + 1, &data1);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA + 2, &data1);
				else core->ReadWord(EA + 4, &data1);

				if (core->exception) return;

				PS0(d) = (double)dequantize(data0, type, scale);
				PS1(d) = (double)dequantize(data1, type, scale);
			}

			core->regs.gpr[info.paramBits[1]] = EA;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::psq_stux()
	{
		if ((core->regs.spr[SPR::HID2] & HID2_PSE) == 0 ||
			(core->regs.spr[SPR::HID2] & HID2_LSQE) == 0 ||
			info.paramBits[1] == 0)
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::EXCEPTION_PROGRAM);
			return;
		}

		if (core->regs.msr & MSR_FP)
		{
			size_t i = info.paramBits[4];
			uint32_t EA = core->regs.gpr[info.paramBits[2]];
			int32_t d = info.paramBits[0];
			uint8_t scale = (uint8_t)ST_SCALE(i);
			GEKKO_QUANT_TYPE type = ST_TYPE(i);

			EA += core->regs.gpr[info.paramBits[1]];

			if (info.paramBits[3] /* W */)
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA, quantize((float)PS0(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA, quantize((float)PS0(d), type, scale));
				else core->WriteWord(EA, quantize((float)PS0(d), type, scale));
			}
			else
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA, quantize((float)PS0(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA, quantize((float)PS0(d), type, scale));
				else core->WriteWord(EA, quantize((float)PS0(d), type, scale));

				if (core->exception) return;

				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA + 1, quantize((float)PS1(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA + 2, quantize((float)PS1(d), type, scale));
				else core->WriteWord(EA + 4, quantize((float)PS1(d), type, scale));
			}

			if (core->exception) return;

			core->regs.gpr[info.paramBits[1]] = EA;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::psq_l()
	{
		if ((core->regs.spr[SPR::HID2] & HID2_PSE) == 0 ||
			(core->regs.spr[SPR::HID2] & HID2_LSQE) == 0)
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::EXCEPTION_PROGRAM);
			return;
		}

		if (core->regs.msr & MSR_FP)
		{
			uint32_t EA = info.Imm.Signed & 0xfff, data0, data1;
			int32_t d = info.paramBits[0];
			uint8_t scale = (uint8_t)LD_SCALE(info.paramBits[3]);
			GEKKO_QUANT_TYPE type = LD_TYPE(info.paramBits[3]);

			if (EA & 0x800) EA |= 0xfffff000;
			if (info.paramBits[1]) EA += core->regs.gpr[info.paramBits[1]];

			if (info.paramBits[2])
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA, &data0);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA, &data0);
				else core->ReadWord(EA, &data0);

				if (core->exception) return;

				PS0(d) = (double)dequantize(data0, type, scale);
				PS1(d) = 1.0f;
			}
			else
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA, &data0);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA, &data0);
				else core->ReadWord(EA, &data0);

				if (core->exception) return;

				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA + 1, &data1);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA + 2, &data1);
				else core->ReadWord(EA + 4, &data1);

				if (core->exception) return;

				PS0(d) = (double)dequantize(data0, type, scale);
				PS1(d) = (double)dequantize(data1, type, scale);
			}

			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::psq_lu()
	{
		if ((core->regs.spr[SPR::HID2] & HID2_PSE) == 0 ||
			(core->regs.spr[SPR::HID2] & HID2_LSQE) == 0 ||
			info.paramBits[1] == 0)
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::EXCEPTION_PROGRAM);
			return;
		}

		if (core->regs.msr & MSR_FP)
		{
			uint32_t EA = info.Imm.Signed & 0xfff, data0, data1;
			int32_t d = info.paramBits[0];
			uint8_t scale = (uint8_t)LD_SCALE(info.paramBits[3]);
			GEKKO_QUANT_TYPE type = LD_TYPE(info.paramBits[3]);

			if (EA & 0x800) EA |= 0xfffff000;
			EA += core->regs.gpr[info.paramBits[1]];

			if (info.paramBits[2])
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA, &data0);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA, &data0);
				else core->ReadWord(EA, &data0);

				if (core->exception) return;

				PS0(d) = (double)dequantize(data0, type, scale);
				PS1(d) = 1.0f;
			}
			else
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA, &data0);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA, &data0);
				else core->ReadWord(EA, &data0);

				if (core->exception) return;

				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA + 1, &data1);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA + 2, &data1);
				else core->ReadWord(EA + 4, &data1);

				if (core->exception) return;

				PS0(d) = (double)dequantize(data0, type, scale);
				PS1(d) = (double)dequantize(data1, type, scale);
			}

			core->regs.gpr[info.paramBits[1]] = EA;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::psq_st()
	{
		if ((core->regs.spr[SPR::HID2] & HID2_PSE) == 0 ||
			(core->regs.spr[SPR::HID2] & HID2_LSQE) == 0)
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::EXCEPTION_PROGRAM);
			return;
		}

		if (core->regs.msr & MSR_FP)
		{
			uint32_t EA = info.Imm.Signed & 0xfff;
			int32_t d = info.paramBits[0];
			uint8_t scale = (uint8_t)ST_SCALE(info.paramBits[3]);
			GEKKO_QUANT_TYPE type = ST_TYPE(info.paramBits[3]);

			if (EA & 0x800) EA |= 0xfffff000;
			if (info.paramBits[1]) EA += core->regs.gpr[info.paramBits[1]];

			if (info.paramBits[2])
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA, quantize((float)PS0(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA, quantize((float)PS0(d), type, scale));
				else core->WriteWord(EA, quantize((float)PS0(d), type, scale));
			}
			else
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA, quantize((float)PS0(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA, quantize((float)PS0(d), type, scale));
				else core->WriteWord(EA, quantize((float)PS0(d), type, scale));

				if (core->exception) return;

				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA + 1, quantize((float)PS1(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA + 2, quantize((float)PS1(d), type, scale));
				else core->WriteWord(EA + 4, quantize((float)PS1(d), type, scale));
			}

			if (core->exception) return;

			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

	void Interpreter::psq_stu()
	{
		if ((core->regs.spr[SPR::HID2] & HID2_PSE) == 0 ||
			(core->regs.spr[SPR::HID2] & HID2_LSQE) == 0 ||
			info.paramBits[1] == 0)
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::EXCEPTION_PROGRAM);
			return;
		}

		if (core->regs.msr & MSR_FP)
		{
			uint32_t EA = info.Imm.Signed & 0xfff;
			int32_t d = info.paramBits[0];
			uint8_t scale = (uint8_t)ST_SCALE(info.paramBits[3]);
			GEKKO_QUANT_TYPE type = ST_TYPE(info.paramBits[3]);

			if (EA & 0x800) EA |= 0xfffff000;
			EA += core->regs.gpr[info.paramBits[1]];

			if (info.paramBits[2])
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA, quantize((float)PS0(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA, quantize((float)PS0(d), type, scale));
				else core->WriteWord(EA, quantize((float)PS0(d), type, scale));
			}
			else
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA, quantize((float)PS0(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA, quantize((float)PS0(d), type, scale));
				else core->WriteWord(EA, quantize((float)PS0(d), type, scale));

				if (core->exception) return;

				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA + 1, quantize((float)PS1(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA + 2, quantize((float)PS1(d), type, scale));
				else core->WriteWord(EA + 4, quantize((float)PS1(d), type, scale));
			}

			if (core->exception) return;

			core->regs.gpr[info.paramBits[1]] = EA;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::EXCEPTION_FP_UNAVAILABLE);
	}

}

// Integer Rotate Instructions

namespace Gekko
{
	// n = SH
	// r = ROTL(rs, n)
	// m = MASK(mb, me)
	// ra = (r & m) | (ra & ~m)
	// CR0 (if .)
	void Interpreter::rlwimi()
	{
		uint32_t m = rotmask[info.paramBits[3]][info.paramBits[4]];
		uint32_t r = Rotl32(info.paramBits[2], GPR(1));
		GPR(0) = (r & m) | (core->regs.gpr[info.paramBits[0]] & ~m);
		core->regs.pc += 4;
	}

	void Interpreter::rlwimi_d()
	{
		rlwimi();
		COMPUTE_CR0(GPR(0));
	}

	// n = SH
	// r = ROTL(rs, n)
	// m = MASK(MB, ME)
	// ra = r & m
	// CR0 (if .)
	void Interpreter::rlwinm()
	{
		uint32_t m = rotmask[info.paramBits[3]][info.paramBits[4]];
		uint32_t r = Rotl32(info.paramBits[2], GPR(1));
		GPR(0) = r & m;
		core->regs.pc += 4;
	}

	void Interpreter::rlwinm_d()
	{
		rlwinm();
		COMPUTE_CR0(GPR(0));
	}

	// n = rb[27-31]
	// r = ROTL(rs, n)
	// m = MASK(MB, ME)
	// ra = r & m
	void Interpreter::rlwnm()
	{
		uint32_t m = rotmask[info.paramBits[3]][info.paramBits[4]];
		uint32_t r = Rotl32(GPR(2) & 0x1f, GPR(1));
		GPR(0) = r & m;
		core->regs.pc += 4;
	}

	void Interpreter::rlwnm_d()
	{
		rlwnm();
		COMPUTE_CR0(GPR(0));
	}

}

// Integer Shift Instructions

namespace Gekko
{
	// n = rb[27-31]
	// r = ROTL(rs, n)
	// if rb[26] = 0
	// then m = MASK(0, 31-n)
	// else m = (32)0
	// ra = r & m
	// (simply : ra = rs << rb, or ra = 0, if rb[26] = 1)
	void Interpreter::slw()
	{
		uint32_t n = GPR(2);

		uint32_t res;

		if (n & 0x20) res = 0;
		else res = GPR(1) << (n & 0x1f);

		GPR(0) = res;
		core->regs.pc += 4;
	}

	void Interpreter::slw_d()
	{
		slw();
		COMPUTE_CR0(GPR(0));
	}

	// n = rb[27-31]
	// r = ROTL(rs, 32-n)
	// if rb[26] = 0
	// then m = MASK(n, 31)
	// else m = (32)0
	// S = rs(0)
	// ra = r & m | (32)S & ~m
	// XER[CA] = S & (r & ~m[0-31] != 0)
	void Interpreter::sraw()
	{
		uint32_t n = GPR(2);
		int32_t res;
		int32_t src = GPR(1);

		if (n == 0)
		{
			res = src;
			RESET_XER_CA();
		}
		else if (n & 0x20)
		{
			if (src < 0)
			{
				res = 0xffffffff;
				if (src & 0x7fffffff) SET_XER_CA(); else RESET_XER_CA();
			}
			else
			{
				res = 0;
				RESET_XER_CA();
			}
		}
		else
		{
			n = n & 0x1f;
			res = (int32_t)src >> n;
			if (src < 0 && (src << (32 - n)) != 0) SET_XER_CA(); else RESET_XER_CA();
		}

		GPR(0) = res;
		core->regs.pc += 4;
	}

	void Interpreter::sraw_d()
	{
		sraw();
		COMPUTE_CR0(GPR(0));
	}

	// n = SH
	// r = ROTL(rs, 32 - n)
	// m = MASK(n, 31)
	// sign = rs[0]
	// ra = r & m | (32)sign & ~m
	// XER[CA] = sign(0) & ((r & ~m) != 0)
	void Interpreter::srawi()
	{
		uint32_t n = (uint32_t)info.paramBits[2];
		int32_t res;
		int32_t src = GPR(1);

		if (n == 0)
		{
			res = src;
			RESET_XER_CA();
		}
		else
		{
			res = src >> n;
			if (src < 0 && (src << (32 - n)) != 0) SET_XER_CA(); else RESET_XER_CA();
		}

		GPR(0) = res;
		core->regs.pc += 4;
	}

	void Interpreter::srawi_d()
	{
		srawi();
		COMPUTE_CR0(GPR(0));
	}

	// n = rb[27-31]
	// r = ROTL(rs, 32-n)
	// if rb[26] = 0
	// then m = MASK(n, 31)
	// else m = (32)0
	// ra = r & m
	// (simply : ra = rs >> rb, or ra = 0, if rb[26] = 1)
	void Interpreter::srw()
	{
		uint32_t n = GPR(2);

		uint32_t res;

		if (n & 0x20) res = 0;
		else res = GPR(1) >> (n & 0x1f);

		GPR(0) = res;
		core->regs.pc += 4;
	}

	void Interpreter::srw_d()
	{
		srw();
		COMPUTE_CR0(GPR(0));
	}

}

// System Instructions

namespace Gekko
{

	void Interpreter::eieio()
	{
		core->regs.pc += 4;
	}

	// instruction synchronize.
	void Interpreter::isync()
	{
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// RESERVE = 1
	// RESERVE_ADDR = physical(ea)
	// rd = MEM(ea, 4)
	void Interpreter::lwarx()
	{
		int WIMG;
		uint32_t ea = core->regs.gpr[info.paramBits[2]];
		if (info.paramBits[1]) ea += core->regs.gpr[info.paramBits[1]];
		core->RESERVE = true;
		core->RESERVE_ADDR = core->EffectiveToPhysical(ea, Gekko::MmuAccess::Read, WIMG);
		core->ReadWord(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// if RESERVE
	//      then
	//          MEM(ea, 4) = rs
	//          CR0 = 0b00 || 0b1 || XER[SO]
	//          RESERVE = 0
	//      else
	//          CR0 = 0b00 || 0b0 || XER[SO]
	void Interpreter::stwcx_d()
	{
		uint32_t ea = core->regs.gpr[info.paramBits[2]];
		if (info.paramBits[1]) ea += core->regs.gpr[info.paramBits[1]];

		core->regs.cr &= 0x0fffffff;

		if (core->RESERVE)
		{
			core->WriteWord(ea, core->regs.gpr[info.paramBits[0]]);
			if (core->exception) return;
			core->regs.cr |= GEKKO_CR0_EQ;
			core->RESERVE = false;
		}

		if (IS_XER_SO) core->regs.cr |= GEKKO_CR0_SO;
		core->regs.pc += 4;
	}

	void Interpreter::sync()
	{
		core->regs.pc += 4;
	}

	// return from exception
	void Interpreter::rfi()
	{
		core->regs.msr &= ~(0x87C0FF73 | 0x00040000);
		core->regs.msr |= core->regs.spr[SPR::SRR1] & 0x87C0FF73;
		core->regs.pc = core->regs.spr[SPR::SRR0] & ~3;
	}

	// syscall
	void Interpreter::sc()
	{
		// pseudo-branch (to resume from next instruction after 'rfi')
		core->regs.pc += 4;
		core->Exception(Gekko::Exception::EXCEPTION_SYSTEM_CALL);
	}

	void Interpreter::tw()
	{
		int32_t a = core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]];
		int32_t to = info.paramBits[0];

		if (((a < b) && (to & 0x10)) ||
			((a > b) && (to & 0x08)) ||
			((a == b) && (to & 0x04)) ||
			(((uint32_t)a < (uint32_t)b) && (to & 0x02)) ||
			(((uint32_t)a > (uint32_t)b) && (to & 0x01)))
		{
			// pseudo-branch (to resume from next instruction after 'rfi')
			core->regs.pc += 4;
			core->PrCause = PrivilegedCause::Trap;
			core->Exception(Gekko::Exception::EXCEPTION_PROGRAM);
		}
		else
		{
			core->regs.pc += 4;
		}
	}

	void Interpreter::twi()
	{
		int32_t a = core->regs.gpr[info.paramBits[1]], b = (int32_t)info.Imm.Signed;
		int32_t to = info.paramBits[0];

		if (((a < b) && (to & 0x10)) ||
			((a > b) && (to & 0x08)) ||
			((a == b) && (to & 0x04)) ||
			(((uint32_t)a < (uint32_t)b) && (to & 0x02)) ||
			(((uint32_t)a > (uint32_t)b) && (to & 0x01)))
		{
			// pseudo-branch (to resume from next instruction after 'rfi')
			core->regs.pc += 4;
			core->PrCause = PrivilegedCause::Trap;
			core->Exception(Gekko::Exception::EXCEPTION_PROGRAM);
		}
		else
		{
			core->regs.pc += 4;
		}
	}

	// CR[4 * crfD .. 4 * crfd + 3] = XER[0-3]
	// XER[0..3] = 0b0000
	void Interpreter::mcrxr()
	{
		uint32_t mask = 0xf0000000 >> (4 * info.paramBits[0]);
		core->regs.cr &= ~mask;
		core->regs.cr |= (core->regs.spr[SPR::XER] & 0xf0000000) >> (4 * info.paramBits[0]);
		core->regs.spr[SPR::XER] &= ~0xf0000000;
		core->regs.pc += 4;
	}

	// rd = cr
	void Interpreter::mfcr()
	{
		core->regs.gpr[info.paramBits[0]] = core->regs.cr;
		core->regs.pc += 4;
	}

	// rd = msr
	void Interpreter::mfmsr()
	{
		if (core->regs.msr & MSR_PR)
		{
			core->PrCause = PrivilegedCause::Privileged;
			core->Exception(Exception::EXCEPTION_PROGRAM);
			return;
		}

		core->regs.gpr[info.paramBits[0]] = core->regs.msr;
		core->regs.pc += 4;
	}

	// We do not support access rights to SPRs, since all applications on the emulated system are executed with OEA rights.
	// A detailed study of all SPRs in all modes is in Docs\HW\SPR.txt. If necessary, it will be possible to wind the rights properly.

	// rd = spr
	void Interpreter::mfspr()
	{
		int spr = info.paramBits[1];
		uint32_t value;

		switch (spr)
		{
			case SPR::WPAR:
				value = (core->regs.spr[spr] & ~0x1f) | (core->gatherBuffer->NotEmpty() ? 1 : 0);
				break;

			case SPR::HID1:
				// Gekko PLL_CFG = 0b1000
				value = 0x8000'0000;
				break;

			default:
				value = core->regs.spr[spr];
				break;
		}

		core->regs.gpr[info.paramBits[0]] = value;
		core->regs.pc += 4;
	}

	// rd = tbr
	void Interpreter::mftb()
	{
		int tbr = info.paramBits[1];

		if (tbr == (int)TBR::TBL)
		{
			core->regs.gpr[info.paramBits[0]] = core->regs.tb.Part.l;
		}
		else if (tbr == (int)TBR::TBU)
		{
			core->regs.gpr[info.paramBits[0]] = core->regs.tb.Part.u;
		}

		core->regs.pc += 4;
	}

	// mask = (4)CRM[0] || (4)CRM[1] || ... || (4)CRM[7]
	// CR = (rs & mask) | (CR & ~mask)
	void Interpreter::mtcrf()
	{
		uint32_t m, crm = info.paramBits[0], a, d = core->regs.gpr[info.paramBits[1]];

		for (int i = 0; i < 8; i++)
		{
			if ((crm >> i) & 1)
			{
				a = (d >> (i << 2)) & 0xf;
				m = (0xf << (i << 2));
				core->regs.cr = (core->regs.cr & ~m) | (a << (i << 2));
			}
		}
		core->regs.pc += 4;
	}

	// msr = rs
	void Interpreter::mtmsr()
	{
		if (core->regs.msr & MSR_PR)
		{
			core->PrCause = PrivilegedCause::Privileged;
			core->Exception(Exception::EXCEPTION_PROGRAM);
			return;
		}

		uint32_t oldMsr = core->regs.msr;
		core->regs.msr = core->regs.gpr[info.paramBits[0]];

		if ((oldMsr & MSR_IR) != (core->regs.msr & MSR_IR))
		{
			core->itlb.InvalidateAll();
		}

		if ((oldMsr & MSR_DR) != (core->regs.msr & MSR_DR))
		{
			core->dtlb.InvalidateAll();
		}

		core->regs.pc += 4;
	}

	// spr = rs
	void Interpreter::mtspr()
	{
		int spr = info.paramBits[0];

		// Diagnostic output when the BAT registers are changed.

		if (spr >= SPR::IBAT0U && spr <= SPR::DBAT3L)
		{
			static const char* bat[] = {
				"IBAT0U", "IBAT0L", "IBAT1U", "IBAT1L",
				"IBAT2U", "IBAT2L", "IBAT3U", "IBAT3L",
				"DBAT0U", "DBAT0L", "DBAT1U", "DBAT1L",
				"DBAT2U", "DBAT2L", "DBAT3U", "DBAT3L"
			};

			bool msr_ir = (core->regs.msr & MSR_IR) ? true : false;
			bool msr_dr = (core->regs.msr & MSR_DR) ? true : false;

			Report(Channel::CPU, "%s <- %08X (IR:%i DR:%i pc:%08X)\n",
				bat[spr - SPR::IBAT0U], core->regs.gpr[info.paramBits[1]], msr_ir, msr_dr, core->regs.pc);
		}

		switch (spr)
		{
			// decrementer
			case SPR::DEC:
				//DBReport2(DbgChannel::CPU, "set decrementer (OS alarm) to %08X\n", RRS);
				break;

				// page table base
			case SPR::SDR1:
			{
				bool msr_ir = (core->regs.msr & MSR_IR) ? true : false;
				bool msr_dr = (core->regs.msr & MSR_DR) ? true : false;

				Report(Channel::CPU, "SDR <- %08X (IR:%i DR:%i pc:%08X)\n",
					core->regs.gpr[info.paramBits[1]], msr_ir, msr_dr, core->regs.pc);

				core->dtlb.InvalidateAll();
				core->itlb.InvalidateAll();
			}
			break;

			case SPR::TBL:
				core->regs.tb.Part.l = core->regs.gpr[info.paramBits[1]];
				Report(Channel::CPU, "Set TBL: 0x%08X\n", core->regs.tb.Part.l);
				break;
			case SPR::TBU:
				core->regs.tb.Part.u = core->regs.gpr[info.paramBits[1]];
				Report(Channel::CPU, "Set TBU: 0x%08X\n", core->regs.tb.Part.u);
				break;

				// write gathering buffer
			case SPR::WPAR:
				// A mtspr to WPAR invalidates the data.
				core->gatherBuffer->Reset();
				break;

			case SPR::HID0:
			{
				uint32_t bits = core->regs.gpr[info.paramBits[1]];
				core->cache->Enable((bits & HID0_DCE) ? true : false);
				core->icache->Enable((bits & HID0_ICE) ? true : false);
				core->cache->Freeze((bits & HID0_DLOCK) ? true : false);
				if (bits & HID0_DCFI)
				{
					bits &= ~HID0_DCFI;
					Report(Channel::CPU, "Data Cache Flash Invalidate\n");
					core->cache->FlashInvalidate();
				}
				if (bits & HID0_ICFI)
				{
					bits &= ~HID0_ICFI;
					Report(Channel::CPU, "Instruction Cache Flash Invalidate\n");
					core->icache->FlashInvalidate();
				}

				core->regs.spr[spr] = bits;
				core->regs.pc += 4;
				return;
			}
			break;

			case SPR::HID1:
				// Read only
				core->regs.pc += 4;
				return;

			case SPR::HID2:
			{
				uint32_t bits = core->regs.gpr[info.paramBits[1]];
				core->cache->LockedEnable((bits & HID2_LCE) ? true : false);
			}
			break;

			// Locked cache DMA

			case SPR::DMAU:
				//DBReport2(DbgChannel::CPU, "DMAU: 0x%08X\n", RRS);
				break;
			case SPR::DMAL:
			{
				core->regs.spr[spr] = core->regs.gpr[info.paramBits[1]];
				//DBReport2(DbgChannel::CPU, "DMAL: 0x%08X\n", RRS);
				if (core->regs.spr[SPR::DMAL] & GEKKO_DMAL_DMA_T)
				{
					uint32_t maddr = core->regs.spr[SPR::DMAU] & GEKKO_DMAU_MEM_ADDR;
					uint32_t lcaddr = core->regs.spr[SPR::DMAL] & GEKKO_DMAL_LC_ADDR;
					size_t length = ((core->regs.spr[SPR::DMAU] & GEKKO_DMAU_DMA_LEN_U) << GEKKO_DMA_LEN_SHIFT) |
						((core->regs.spr[SPR::DMAL] >> GEKKO_DMA_LEN_SHIFT) & GEKKO_DMAL_DMA_LEN_L);
					if (length == 0) length = 128;
					if (core->cache->IsLockedEnable())
					{
						core->cache->LockedCacheDma(
							(core->regs.spr[SPR::DMAL] & GEKKO_DMAL_DMA_LD) ? true : false,
							maddr,
							lcaddr,
							length);
					}
				}

				// It makes no sense to implement such a small Queue. We make all transactions instant.

				core->regs.spr[spr] &= ~(GEKKO_DMAL_DMA_T | GEKKO_DMAL_DMA_F);
				core->regs.pc += 4;
				return;
			}
			break;

			case SPR::GQR0:
			case SPR::GQR1:
			case SPR::GQR2:
			case SPR::GQR3:
			case SPR::GQR4:
			case SPR::GQR5:
			case SPR::GQR6:
			case SPR::GQR7:
				// In the sense of Dolphin OS, registers GQR1-7 are constantly reloaded when switching threads via `OSLoadContext`.
				// GQR0 is always 0 and is not reloaded when switching threads.
				break;

			case SPR::IBAT0U:
			case SPR::IBAT0L:
			case SPR::IBAT1U:
			case SPR::IBAT1L:
			case SPR::IBAT2U:
			case SPR::IBAT2L:
			case SPR::IBAT3U:
			case SPR::IBAT3L:
				core->itlb.InvalidateAll();
				break;

			case SPR::DBAT0U:
			case SPR::DBAT0L:
			case SPR::DBAT1U:
			case SPR::DBAT1L:
			case SPR::DBAT2U:
			case SPR::DBAT2L:
			case SPR::DBAT3U:
			case SPR::DBAT3L:
				core->dtlb.InvalidateAll();
				break;
		}

		// default
		core->regs.spr[spr] = core->regs.gpr[info.paramBits[1]];
		core->regs.pc += 4;
	}

	void Interpreter::dcbf()
	{
		int WIMG;
		uint32_t ea = info.paramBits[0] ? core->regs.gpr[info.paramBits[0]] + core->regs.gpr[info.paramBits[1]] : core->regs.gpr[info.paramBits[1]];

		uint32_t pa = core->EffectiveToPhysical(ea, MmuAccess::Read, WIMG);
		if (pa != Gekko::BadAddress)
		{
			core->cache->Flush(pa);
		}
		else
		{
			core->regs.spr[Gekko::SPR::DAR] = ea;
			core->Exception(Exception::EXCEPTION_DSI);
			return;
		}
		core->regs.pc += 4;
	}

	void Interpreter::dcbi()
	{
		int WIMG;
		uint32_t ea = info.paramBits[0] ? core->regs.gpr[info.paramBits[0]] + core->regs.gpr[info.paramBits[1]] : core->regs.gpr[info.paramBits[1]];

		if (core->regs.msr & MSR_PR)
		{
			core->PrCause = PrivilegedCause::Privileged;
			core->Exception(Exception::EXCEPTION_PROGRAM);
			return;
		}

		uint32_t pa = core->EffectiveToPhysical(ea, MmuAccess::Write, WIMG);
		if (pa != Gekko::BadAddress)
		{
			core->cache->Invalidate(pa);
		}
		else
		{
			core->regs.spr[Gekko::SPR::DAR] = ea;
			core->Exception(Exception::EXCEPTION_DSI);
			return;
		}
		core->regs.pc += 4;
	}

	void Interpreter::dcbst()
	{
		int WIMG;
		uint32_t ea = info.paramBits[0] ? core->regs.gpr[info.paramBits[0]] + core->regs.gpr[info.paramBits[1]] : core->regs.gpr[info.paramBits[1]];

		uint32_t pa = core->EffectiveToPhysical(ea, MmuAccess::Read, WIMG);
		if (pa != Gekko::BadAddress)
		{
			core->cache->Store(pa);
		}
		else
		{
			core->regs.spr[Gekko::SPR::DAR] = ea;
			core->Exception(Exception::EXCEPTION_DSI);
			return;
		}
		core->regs.pc += 4;
	}

	void Interpreter::dcbt()
	{
		int WIMG;

		if (core->regs.spr[Gekko::SPR::HID0] & HID0_NOOPTI)
		{
			core->regs.pc += 4;
			return;
		}

		uint32_t ea = info.paramBits[0] ? core->regs.gpr[info.paramBits[0]] + core->regs.gpr[info.paramBits[1]] : core->regs.gpr[info.paramBits[1]];

		uint32_t pa = core->EffectiveToPhysical(ea, MmuAccess::Read, WIMG);
		if (pa != Gekko::BadAddress)
		{
			core->cache->Touch(pa);
		}
		core->regs.pc += 4;
	}

	void Interpreter::dcbtst()
	{
		int WIMG;

		if (core->regs.spr[Gekko::SPR::HID0] & HID0_NOOPTI)
		{
			core->regs.pc += 4;
			return;
		}

		uint32_t ea = info.paramBits[0] ? core->regs.gpr[info.paramBits[0]] + core->regs.gpr[info.paramBits[1]] : core->regs.gpr[info.paramBits[1]];

		// TouchForStore is also made architecturally as a Read operation so that the MMU does not set the "Changed" bit for PTE.

		uint32_t pa = core->EffectiveToPhysical(ea, MmuAccess::Read, WIMG);
		if (pa != Gekko::BadAddress)
		{
			core->cache->TouchForStore(pa);
		}
		core->regs.pc += 4;
	}

	void Interpreter::dcbz()
	{
		int WIMG;
		uint32_t ea = info.paramBits[0] ? core->regs.gpr[info.paramBits[0]] + core->regs.gpr[info.paramBits[1]] : core->regs.gpr[info.paramBits[1]];

		uint32_t pa = core->EffectiveToPhysical(ea, MmuAccess::Write, WIMG);
		if (pa != Gekko::BadAddress)
		{
			core->cache->Zero(pa);
		}
		else
		{
			core->regs.spr[Gekko::SPR::DAR] = ea;
			core->Exception(Exception::EXCEPTION_DSI);
			return;
		}
		core->regs.pc += 4;
	}

	// DCBZ_L is used for the alien Locked Cache address mapping mechanism.
	// For example, calling dcbz_l 0xE0000000 will make this address be associated with Locked Cache for subsequent Load/Store operations.
	// Locked Cache is saved in RAM by another alien mechanism (DMA).

	void Interpreter::dcbz_l()
	{
		int WIMG;

		if (!core->cache->IsLockedEnable())
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::EXCEPTION_PROGRAM);
			return;
		}

		uint32_t ea = info.paramBits[0] ? core->regs.gpr[info.paramBits[0]] + core->regs.gpr[info.paramBits[1]] : core->regs.gpr[info.paramBits[1]];

		uint32_t pa = core->EffectiveToPhysical(ea, MmuAccess::Write, WIMG);
		if (pa != Gekko::BadAddress)
		{
			core->cache->ZeroLocked(pa);
		}
		else
		{
			core->regs.spr[Gekko::SPR::DAR] = ea;
			core->Exception(Exception::EXCEPTION_DSI);
			return;
		}
		core->regs.pc += 4;
	}

	void Interpreter::icbi()
	{
		int WIMG;
		uint32_t address = info.paramBits[0] ? core->regs.gpr[info.paramBits[0]] + core->regs.gpr[info.paramBits[1]] : core->regs.gpr[info.paramBits[1]];
		address &= ~0x1f;

		if (core->regs.msr & MSR_PR)
		{
			core->PrCause = PrivilegedCause::Privileged;
			core->Exception(Exception::EXCEPTION_PROGRAM);
			return;
		}

		uint32_t pa = core->EffectiveToPhysical(address, MmuAccess::Read, WIMG);
		if (pa != Gekko::BadAddress)
		{
			core->icache->Invalidate(pa);
		}
		else
		{
			core->Exception(Exception::EXCEPTION_ISI);
			return;
		}
		core->regs.pc += 4;
	}

	// rd = sr[a]
	void Interpreter::mfsr()
	{
		if (core->regs.msr & MSR_PR)
		{
			core->PrCause = PrivilegedCause::Privileged;
			core->Exception(Exception::EXCEPTION_PROGRAM);
			return;
		}

		core->regs.gpr[info.paramBits[0]] = core->regs.sr[info.paramBits[1]];
		core->regs.pc += 4;
	}

	// rd = sr[rb]
	void Interpreter::mfsrin()
	{
		if (core->regs.msr & MSR_PR)
		{
			core->PrCause = PrivilegedCause::Privileged;
			core->Exception(Exception::EXCEPTION_PROGRAM);
			return;
		}

		core->regs.gpr[info.paramBits[0]] = core->regs.sr[core->regs.gpr[info.paramBits[1]] >> 28];
		core->regs.pc += 4;
	}

	// sr[a] = rs
	void Interpreter::mtsr()
	{
		if (core->regs.msr & MSR_PR)
		{
			core->PrCause = PrivilegedCause::Privileged;
			core->Exception(Exception::EXCEPTION_PROGRAM);
			return;
		}

		core->regs.sr[info.paramBits[0]] = core->regs.gpr[info.paramBits[1]];
		core->regs.pc += 4;
	}

	// sr[rb] = rs
	void Interpreter::mtsrin()
	{
		if (core->regs.msr & MSR_PR)
		{
			core->PrCause = PrivilegedCause::Privileged;
			core->Exception(Exception::EXCEPTION_PROGRAM);
			return;
		}

		core->regs.sr[core->regs.gpr[info.paramBits[1]] >> 28] = core->regs.gpr[info.paramBits[0]];
		core->regs.pc += 4;
	}

	void Interpreter::tlbie()
	{
		core->dtlb.Invalidate(core->regs.gpr[info.paramBits[0]]);
		core->itlb.Invalidate(core->regs.gpr[info.paramBits[0]]);
		core->regs.pc += 4;
	}

	void Interpreter::tlbsync()
	{
		core->regs.pc += 4;
	}

	void Interpreter::eciwx()
	{
		Halt("eciwx\n");
	}

	void Interpreter::ecowx()
	{
		Halt("ecowx\n");
	}

}


// Gekko interpreter

namespace Gekko
{
	Interpreter::Interpreter(GekkoCore* _core)
	{
		core = _core;

		// build rotate mask table
		for (int mb = 0; mb < 32; mb++)
		{
			for (int me = 0; me < 32; me++)
			{
				uint32_t mask = ((uint32_t)-1 >> mb) ^ ((me >= 31) ? 0 : ((uint32_t)-1) >> (me + 1));
				rotmask[mb][me] = (mb > me) ? (~mask) : (mask);
			}
		}

		// build paired-single load scale
		for (uint8_t scale = 0; scale < 64; scale++)
		{
			int factor;
			if (scale & 0x20)    // -32 ... -1
			{
				factor = -32 + (scale & 0x1f);
			}
			else                // 0 ... 31
			{
				factor = 0 + (scale & 0x1f);
			}
			ldScale[scale] = powf(2, -1.0f * (float)factor);
		}

		// build paired-single store scale
		for (uint8_t scale = 0; scale < 64; scale++)
		{
			int factor;
			if (scale & 0x20)    // -32 ... -1
			{
				factor = -32 + (scale & 0x1f);
			}
			else                // 0 ... 31
			{
				factor = 0 + (scale & 0x1f);
			}
			stScale[scale] = powf(2, +1.0f * (float)factor);
		}
	}

	/// <summary>
	/// Result = a + b + CarryIn. Return carry flag in CarryBit. Return overflow flag in OverflowBit.
	/// </summary>
	/// <param name="a"></param>
	/// <param name="b"></param>
	/// <returns></returns>
	uint32_t Interpreter::FullAdder(uint32_t a, uint32_t b)
	{
		uint64_t res = (uint64_t)a + (uint64_t)b + (uint64_t)(CarryBit != 0 ? 1 : 0);

		//A human need only remember that, when doing signed math, adding
		//two numbers of the same sign must produce a result of the same sign,
		//otherwise overflow happened.
		bool msb = (res & 0x8000'0000) != 0 ? true : false;
		bool aMsb = (a & 0x8000'0000) != 0 ? true : false;
		bool bMsb = (b & 0x8000'0000) != 0 ? true : false;
		OverflowBit = 0;
		if (aMsb == bMsb)
		{
			OverflowBit = (aMsb != msb) ? 1 : 0;
		}

		CarryBit = ((res & 0xffffffff'00000000) != 0) ? 1 : 0;

		return (uint32_t)res;
	}

	/// <summary>
	/// Rotate 32 bit left
	/// </summary>
	/// <param name="sa">Rotate bits amount</param>
	/// <param name="data">Source</param>
	/// <returns>Result</returns>
	uint32_t Interpreter::Rotl32(size_t sa, uint32_t data)
	{
		return (data << sa) | (data >> ((32 - sa) & 31));
	}


	// parse and execute single opcode
	void Interpreter::ExecuteOpcode()
	{
		uint32_t instr = 0;

		// Fetch instruction

		core->Fetch(core->regs.pc, &instr);
		// ISI
		if (core->exception)
		{
			core->exception = false;
			return;
		}

		// Decode instruction and dispatch

		Decoder::DecodeFast(core->regs.pc, instr, &info);
		Dispatch();
		core->ops++;

		if (core->resetInstructionCounter)
		{
			core->resetInstructionCounter = false;
			core->ops = 0;
		}
		// DSI, ALIGN, PROGRAM, FPUNA, SC
		if (core->exception)
		{
			core->exception = false;
			return;
		}

		core->Tick();

		core->exception = false;
	}

	void Interpreter::Dispatch()
	{
		switch (info.instr)
		{
			case Instruction::b: b(); break;
			case Instruction::ba: ba(); break;
			case Instruction::bl: bl(); break;
			case Instruction::bla: bla(); break;
			case Instruction::bc: bc(); break;
			case Instruction::bca: bca(); break;
			case Instruction::bcl: bcl(); break;
			case Instruction::bcla: bcla(); break;
			case Instruction::bcctr: bcctr(); break;
			case Instruction::bcctrl: bcctrl(); break;
			case Instruction::bclr: bclr(); break;
			case Instruction::bclrl: bclrl(); break;

			case Instruction::cmpi: cmpi(); break;
			case Instruction::cmp: cmp(); break;
			case Instruction::cmpli: cmpli(); break;
			case Instruction::cmpl: cmpl(); break;

			case Instruction::crand: crand(); break;
			case Instruction::crandc: crandc(); break;
			case Instruction::creqv: creqv(); break;
			case Instruction::crnand: crnand(); break;
			case Instruction::crnor: crnor(); break;
			case Instruction::cror: cror(); break;
			case Instruction::crorc: crorc(); break;
			case Instruction::crxor: crxor(); break;
			case Instruction::mcrf: mcrf(); break;

			case Instruction::fadd: fadd(); break;
			case Instruction::fadd_d: fadd_d(); break;
			case Instruction::fadds: fadds(); break;
			case Instruction::fadds_d: fadds_d(); break;
			case Instruction::fdiv: fdiv(); break;
			case Instruction::fdiv_d: fdiv_d(); break;
			case Instruction::fdivs: fdivs(); break;
			case Instruction::fdivs_d: fdivs_d(); break;
			case Instruction::fmul: fmul(); break;
			case Instruction::fmul_d: fmul_d(); break;
			case Instruction::fmuls: fmuls(); break;
			case Instruction::fmuls_d: fmuls_d(); break;
			case Instruction::fres: fres(); break;
			case Instruction::fres_d: fres_d(); break;
			case Instruction::frsqrte: frsqrte(); break;
			case Instruction::frsqrte_d: frsqrte_d(); break;
			case Instruction::fsub: fsub(); break;
			case Instruction::fsub_d: fsub_d(); break;
			case Instruction::fsubs: fsubs(); break;
			case Instruction::fsubs_d: fsubs_d(); break;
			case Instruction::fsel: fsel(); break;
			case Instruction::fsel_d: fsel_d(); break;
			case Instruction::fmadd: fmadd(); break;
			case Instruction::fmadd_d: fmadd_d(); break;
			case Instruction::fmadds: fmadds(); break;
			case Instruction::fmadds_d: fmadds_d(); break;
			case Instruction::fmsub: fmsub(); break;
			case Instruction::fmsub_d: fmsub_d(); break;
			case Instruction::fmsubs: fmsubs(); break;
			case Instruction::fmsubs_d: fmsubs_d(); break;
			case Instruction::fnmadd: fnmadd(); break;
			case Instruction::fnmadd_d: fnmadd_d(); break;
			case Instruction::fnmadds: fnmadds(); break;
			case Instruction::fnmadds_d: fnmadds_d(); break;
			case Instruction::fnmsub: fnmsub(); break;
			case Instruction::fnmsub_d: fnmsub_d(); break;
			case Instruction::fnmsubs: fnmsubs(); break;
			case Instruction::fnmsubs_d: fnmsubs_d(); break;
			case Instruction::fctiw: fctiw(); break;
			case Instruction::fctiw_d: fctiw_d(); break;
			case Instruction::fctiwz: fctiwz(); break;
			case Instruction::fctiwz_d: fctiwz_d(); break;
			case Instruction::frsp: frsp(); break;
			case Instruction::frsp_d: frsp_d(); break;
			case Instruction::fcmpo: fcmpo(); break;
			case Instruction::fcmpu: fcmpu(); break;
			case Instruction::fabs: fabs(); break;
			case Instruction::fabs_d: fabs_d(); break;
			case Instruction::fmr: fmr(); break;
			case Instruction::fmr_d: fmr_d(); break;
			case Instruction::fnabs: fnabs(); break;
			case Instruction::fnabs_d: fnabs_d(); break;
			case Instruction::fneg: fneg(); break;
			case Instruction::fneg_d: fneg_d(); break;

			case Instruction::mcrfs: mcrfs(); break;
			case Instruction::mffs: mffs(); break;
			case Instruction::mffs_d: mffs_d(); break;
			case Instruction::mtfsb0: mtfsb0(); break;
			case Instruction::mtfsb0_d: mtfsb0_d(); break;
			case Instruction::mtfsb1: mtfsb1(); break;
			case Instruction::mtfsb1_d: mtfsb1_d(); break;
			case Instruction::mtfsf: mtfsf(); break;
			case Instruction::mtfsf_d: mtfsf_d(); break;
			case Instruction::mtfsfi: mtfsfi(); break;
			case Instruction::mtfsfi_d: mtfsfi_d(); break;

			case Instruction::lfd: lfd(); break;
			case Instruction::lfdu: lfdu(); break;
			case Instruction::lfdux: lfdux(); break;
			case Instruction::lfdx: lfdx(); break;
			case Instruction::lfs: lfs(); break;
			case Instruction::lfsu: lfsu(); break;
			case Instruction::lfsux: lfsux(); break;
			case Instruction::lfsx: lfsx(); break;
			case Instruction::stfd: stfd(); break;
			case Instruction::stfdu: stfdu(); break;
			case Instruction::stfdux: stfdux(); break;
			case Instruction::stfdx: stfdx(); break;
			case Instruction::stfiwx: stfiwx(); break;
			case Instruction::stfs: stfs(); break;
			case Instruction::stfsu: stfsu(); break;
			case Instruction::stfsux: stfsux(); break;
			case Instruction::stfsx: stfsx(); break;

			case Instruction::add: add(); break;
			case Instruction::add_d: add_d(); break;
			case Instruction::addo: addo(); break;
			case Instruction::addo_d: addo_d(); break;
			case Instruction::addc: addc(); break;
			case Instruction::addc_d: addc_d(); break;
			case Instruction::addco: addco(); break;
			case Instruction::addco_d: addco_d(); break;
			case Instruction::adde: adde(); break;
			case Instruction::adde_d: adde_d(); break;
			case Instruction::addeo: addeo(); break;
			case Instruction::addeo_d: addeo_d(); break;
			case Instruction::addi: addi(); break;
			case Instruction::addic: addic(); break;
			case Instruction::addic_d: addic_d(); break;
			case Instruction::addis: addis(); break;
			case Instruction::addme: addme(); break;
			case Instruction::addme_d: addme_d(); break;
			case Instruction::addmeo: addmeo(); break;
			case Instruction::addmeo_d: addmeo_d(); break;
			case Instruction::addze: addze(); break;
			case Instruction::addze_d: addze_d(); break;
			case Instruction::addzeo: addzeo(); break;
			case Instruction::addzeo_d: addzeo_d(); break;
			case Instruction::divw: divw(); break;
			case Instruction::divw_d: divw_d(); break;
			case Instruction::divwo: divwo(); break;
			case Instruction::divwo_d: divwo_d(); break;
			case Instruction::divwu: divwu(); break;
			case Instruction::divwu_d: divwu_d(); break;
			case Instruction::divwuo: divwuo(); break;
			case Instruction::divwuo_d: divwuo_d(); break;
			case Instruction::mulhw: mulhw(); break;
			case Instruction::mulhw_d: mulhw_d(); break;
			case Instruction::mulhwu: mulhwu(); break;
			case Instruction::mulhwu_d: mulhwu_d(); break;
			case Instruction::mulli: mulli(); break;
			case Instruction::mullw: mullw(); break;
			case Instruction::mullw_d: mullw_d(); break;
			case Instruction::mullwo: mullwo(); break;
			case Instruction::mullwo_d: mullwo_d(); break;
			case Instruction::neg: neg(); break;
			case Instruction::neg_d: neg_d(); break;
			case Instruction::nego: nego(); break;
			case Instruction::nego_d: nego_d(); break;
			case Instruction::subf: subf(); break;
			case Instruction::subf_d: subf_d(); break;
			case Instruction::subfo: subfo(); break;
			case Instruction::subfo_d: subfo_d(); break;
			case Instruction::subfc: subfc(); break;
			case Instruction::subfc_d: subfc_d(); break;
			case Instruction::subfco: subfco(); break;
			case Instruction::subfco_d: subfco_d(); break;
			case Instruction::subfe: subfe(); break;
			case Instruction::subfe_d: subfe_d(); break;
			case Instruction::subfeo: subfeo(); break;
			case Instruction::subfeo_d: subfeo_d(); break;
			case Instruction::subfic: subfic(); break;
			case Instruction::subfme: subfme(); break;
			case Instruction::subfme_d: subfme_d(); break;
			case Instruction::subfmeo: subfmeo(); break;
			case Instruction::subfmeo_d: subfmeo_d(); break;
			case Instruction::subfze: subfze(); break;
			case Instruction::subfze_d: subfze_d(); break;
			case Instruction::subfzeo: subfzeo(); break;
			case Instruction::subfzeo_d: subfzeo_d(); break;

			case Instruction::lbz: lbz(); break;
			case Instruction::lbzu: lbzu(); break;
			case Instruction::lbzux: lbzux(); break;
			case Instruction::lbzx: lbzx(); break;
			case Instruction::lha: lha(); break;
			case Instruction::lhau: lhau(); break;
			case Instruction::lhaux: lhaux(); break;
			case Instruction::lhax: lhax(); break;
			case Instruction::lhz: lhz(); break;
			case Instruction::lhzu: lhzu(); break;
			case Instruction::lhzux: lhzux(); break;
			case Instruction::lhzx: lhzx(); break;
			case Instruction::lwz: lwz(); break;
			case Instruction::lwzu: lwzu(); break;
			case Instruction::lwzux: lwzux(); break;
			case Instruction::lwzx: lwzx(); break;
			case Instruction::stb: stb(); break;
			case Instruction::stbu: stbu(); break;
			case Instruction::stbux: stbux(); break;
			case Instruction::stbx: stbx(); break;
			case Instruction::sth: sth(); break;
			case Instruction::sthu: sthu(); break;
			case Instruction::sthux: sthux(); break;
			case Instruction::sthx: sthx(); break;
			case Instruction::stw: stw(); break;
			case Instruction::stwu: stwu(); break;
			case Instruction::stwux: stwux(); break;
			case Instruction::stwx: stwx(); break;
			case Instruction::lhbrx: lhbrx(); break;
			case Instruction::lwbrx: lwbrx(); break;
			case Instruction::sthbrx: sthbrx(); break;
			case Instruction::stwbrx: stwbrx(); break;
			case Instruction::lmw: lmw(); break;
			case Instruction::stmw: stmw(); break;
			case Instruction::lswi: lswi(); break;
			case Instruction::lswx: lswx(); break;
			case Instruction::stswi: stswi(); break;
			case Instruction::stswx: stswx(); break;

			case Instruction::_and: _and(); break;
			case Instruction::and_d: and_d(); break;
			case Instruction::andc: andc(); break;
			case Instruction::andc_d: andc_d(); break;
			case Instruction::andi_d: andi_d(); break;
			case Instruction::andis_d: andis_d(); break;
			case Instruction::cntlzw: cntlzw(); break;
			case Instruction::cntlzw_d: cntlzw_d(); break;
			case Instruction::eqv: eqv(); break;
			case Instruction::eqv_d: eqv_d(); break;
			case Instruction::extsb: extsb(); break;
			case Instruction::extsb_d: extsb_d(); break;
			case Instruction::extsh: extsh(); break;
			case Instruction::extsh_d: extsh_d(); break;
			case Instruction::nand: nand(); break;
			case Instruction::nand_d: nand_d(); break;
			case Instruction::nor: nor(); break;
			case Instruction::nor_d: nor_d(); break;
			case Instruction::_or: _or(); break;
			case Instruction::or_d: or_d(); break;
			case Instruction::orc: orc(); break;
			case Instruction::orc_d: orc_d(); break;
			case Instruction::ori: ori(); break;
			case Instruction::oris: oris(); break;
			case Instruction::_xor: _xor(); break;
			case Instruction::xor_d: xor_d(); break;
			case Instruction::xori: xori(); break;
			case Instruction::xoris: xoris(); break;

			case Instruction::ps_div: ps_div(); break;
			case Instruction::ps_div_d: ps_div_d(); break;
			case Instruction::ps_sub: ps_sub(); break;
			case Instruction::ps_sub_d: ps_sub_d(); break;
			case Instruction::ps_add: ps_add(); break;
			case Instruction::ps_add_d: ps_add_d(); break;
			case Instruction::ps_sel: ps_sel(); break;
			case Instruction::ps_sel_d: ps_sel_d(); break;
			case Instruction::ps_res: ps_res(); break;
			case Instruction::ps_res_d: ps_res_d(); break;
			case Instruction::ps_mul: ps_mul(); break;
			case Instruction::ps_mul_d: ps_mul_d(); break;
			case Instruction::ps_rsqrte: ps_rsqrte(); break;
			case Instruction::ps_rsqrte_d: ps_rsqrte_d(); break;
			case Instruction::ps_msub: ps_msub(); break;
			case Instruction::ps_msub_d: ps_msub_d(); break;
			case Instruction::ps_madd: ps_madd(); break;
			case Instruction::ps_madd_d: ps_madd_d(); break;
			case Instruction::ps_nmsub: ps_nmsub(); break;
			case Instruction::ps_nmsub_d: ps_nmsub_d(); break;
			case Instruction::ps_nmadd: ps_nmadd(); break;
			case Instruction::ps_nmadd_d: ps_nmadd_d(); break;
			case Instruction::ps_neg: ps_neg(); break;
			case Instruction::ps_neg_d: ps_neg_d(); break;
			case Instruction::ps_mr: ps_mr(); break;
			case Instruction::ps_mr_d: ps_mr_d(); break;
			case Instruction::ps_nabs: ps_nabs(); break;
			case Instruction::ps_nabs_d: ps_nabs_d(); break;
			case Instruction::ps_abs: ps_abs(); break;
			case Instruction::ps_abs_d: ps_abs_d(); break;

			case Instruction::ps_sum0: ps_sum0(); break;
			case Instruction::ps_sum0_d: ps_sum0_d(); break;
			case Instruction::ps_sum1: ps_sum1(); break;
			case Instruction::ps_sum1_d: ps_sum1_d(); break;
			case Instruction::ps_muls0: ps_muls0(); break;
			case Instruction::ps_muls0_d: ps_muls0_d(); break;
			case Instruction::ps_muls1: ps_muls1(); break;
			case Instruction::ps_muls1_d: ps_muls1_d(); break;
			case Instruction::ps_madds0: ps_madds0(); break;
			case Instruction::ps_madds0_d: ps_madds0_d(); break;
			case Instruction::ps_madds1: ps_madds1(); break;
			case Instruction::ps_madds1_d: ps_madds1_d(); break;
			case Instruction::ps_cmpu0: ps_cmpu0(); break;
			case Instruction::ps_cmpo0: ps_cmpo0(); break;
			case Instruction::ps_cmpu1: ps_cmpu1(); break;
			case Instruction::ps_cmpo1: ps_cmpo1(); break;
			case Instruction::ps_merge00: ps_merge00(); break;
			case Instruction::ps_merge00_d: ps_merge00_d(); break;
			case Instruction::ps_merge01: ps_merge01(); break;
			case Instruction::ps_merge01_d: ps_merge01_d(); break;
			case Instruction::ps_merge10: ps_merge10(); break;
			case Instruction::ps_merge10_d: ps_merge10_d(); break;
			case Instruction::ps_merge11: ps_merge11(); break;
			case Instruction::ps_merge11_d: ps_merge11_d(); break;

			case Instruction::psq_lx: psq_lx(); break;
			case Instruction::psq_stx: psq_stx(); break;
			case Instruction::psq_lux: psq_lux(); break;
			case Instruction::psq_stux: psq_stux(); break;
			case Instruction::psq_l: psq_l(); break;
			case Instruction::psq_lu: psq_lu(); break;
			case Instruction::psq_st: psq_st(); break;
			case Instruction::psq_stu: psq_stu(); break;

			case Instruction::rlwimi: rlwimi(); break;
			case Instruction::rlwimi_d: rlwimi_d(); break;
			case Instruction::rlwinm: rlwinm(); break;
			case Instruction::rlwinm_d: rlwinm_d(); break;
			case Instruction::rlwnm: rlwnm(); break;
			case Instruction::rlwnm_d: rlwnm_d(); break;

			case Instruction::slw: slw(); break;
			case Instruction::slw_d: slw_d(); break;
			case Instruction::sraw: sraw(); break;
			case Instruction::sraw_d: sraw_d(); break;
			case Instruction::srawi: srawi(); break;
			case Instruction::srawi_d: srawi_d(); break;
			case Instruction::srw: srw(); break;
			case Instruction::srw_d: srw_d(); break;

			case Instruction::eieio: eieio(); break;
			case Instruction::isync: isync(); break;
			case Instruction::lwarx: lwarx(); break;
			case Instruction::stwcx_d: stwcx_d(); break;
			case Instruction::sync: sync(); break;
			case Instruction::rfi: rfi(); break;
			case Instruction::sc: sc(); break;
			case Instruction::tw: tw(); break;
			case Instruction::twi: twi(); break;
			case Instruction::mcrxr: mcrxr(); break;
			case Instruction::mfcr: mfcr(); break;
			case Instruction::mfmsr: mfmsr(); break;
			case Instruction::mfspr: mfspr(); break;
			case Instruction::mftb: mftb(); break;
			case Instruction::mtcrf: mtcrf(); break;
			case Instruction::mtmsr: mtmsr(); break;
			case Instruction::mtspr: mtspr(); break;
			case Instruction::dcbf: dcbf(); break;
			case Instruction::dcbi: dcbi(); break;
			case Instruction::dcbst: dcbst(); break;
			case Instruction::dcbt: dcbt(); break;
			case Instruction::dcbtst: dcbtst(); break;
			case Instruction::dcbz: dcbz(); break;
			case Instruction::dcbz_l: dcbz_l(); break;
			case Instruction::icbi: icbi(); break;
			case Instruction::mfsr: mfsr(); break;
			case Instruction::mfsrin: mfsrin(); break;
			case Instruction::mtsr: mtsr(); break;
			case Instruction::mtsrin: mtsrin(); break;
			case Instruction::tlbie: tlbie(); break;
			case Instruction::tlbsync: tlbsync(); break;
			case Instruction::eciwx: eciwx(); break;
			case Instruction::ecowx: ecowx(); break;

				// TODO: CallVM opcode.

			default:
				Halt("** CPU ERROR **\n"
					"unimplemented opcode : %08X\n", core->regs.pc);

				core->PrCause = PrivilegedCause::IllegalInstruction;
				core->Exception(Exception::EXCEPTION_PROGRAM);
				return;
		}

		if (core->opcodeStatsEnabled)
		{
			core->opcodeStats[(size_t)info.instr]++;
		}
	}

	uint32_t Interpreter::GetRotMask(int mb, int me)
	{
		return rotmask[mb][me];
	}

	// high level call
	void Interpreter::callvm()
	{
		// module base should be specified as 0x400000 in project properties
		//void (*pcall)() = (void (*)())((void*)(uint64_t)op);

		//if (op == 0)
		//{
		//	Halt(
		//		"Something goes wrong in interpreter, \n"
		//		"program is trying to execute NULL opcode.\n\n"
		//		"pc:%08X", core->regs.pc);
		//	return;
		//}

		//pcall();

		Halt("callvm: Temporary not implemented!\n");
	}
}
