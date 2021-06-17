// All assembler errors are thrown as exceptions.

#include "pch.h"

namespace Gekko
{
	void GekkoAssembler::CheckParam(AnalyzeInfo& info, size_t paramNum, Param should)
	{
		if (info.param[paramNum] != should)
		{
			throw "Invalid parameter";
		}
	}

	void GekkoAssembler::PackBits(uint32_t& res, size_t ppc_bit_from, size_t ppc_bit_to, size_t val)
	{
		if (ppc_bit_from > 31 || ppc_bit_to > 31)
		{
			throw "Bit out of range";
		}

		if (ppc_bit_to < ppc_bit_from)
		{
			throw "Wrong bit order";
		}

		size_t bits = ppc_bit_to - ppc_bit_from + 1;
		size_t maxVal = 1ULL << bits;

		if (val >= maxVal)
		{
			throw "Value out of range";
		}

		// Make a mask similar to the `rlwinm` instruction

		uint32_t mask = ((uint32_t)-1 >> ppc_bit_from) ^ ((ppc_bit_to >= 31) ? 0 : ((uint32_t)-1) >> (ppc_bit_to + 1));

		res = (res & ~mask) | (val << (31 - ppc_bit_to));
	}

	void GekkoAssembler::SetBit(uint32_t& res, size_t ppc_bit)
	{
		if (ppc_bit > 31)
		{
			throw "Bit out of range";
		}
		res |= 1 << (31 - ppc_bit);
	}

	void GekkoAssembler::Form_DAB(size_t primary, size_t extended, bool oe, bool rc, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 22, 30, extended);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::Reg);
		CheckParam(info, 2, Param::Reg);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 16, 20, info.paramBits[2]);

		if (oe) SetBit(res, 21);
		if (rc) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_DA(size_t primary, size_t extended, bool oe, bool rc, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 22, 30, extended);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::Reg);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);

		if (oe) SetBit(res, 21);
		if (rc) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_ASB(size_t primary, size_t extended, bool rc, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::Reg);
		CheckParam(info, 2, Param::Reg);

		PackBits(res, 6, 10, info.paramBits[1]);	// Reversed order of parameters
		PackBits(res, 11, 15, info.paramBits[0]);
		PackBits(res, 16, 20, info.paramBits[2]);

		if (rc) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_DASimm(size_t primary, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::Reg);
		CheckParam(info, 2, Param::Simm);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 16, 31, (uint16_t)info.Imm.Signed);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_ASUimm(size_t primary, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::Reg);
		CheckParam(info, 2, Param::Uimm);

		PackBits(res, 6, 10, info.paramBits[1]);	// Reversed order
		PackBits(res, 11, 15, info.paramBits[0]);
		PackBits(res, 16, 31, info.Imm.Unsigned);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_BranchLong(size_t primary, bool aa, bool lk, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);

		CheckParam(info, 0, Param::Address);

		// Calculate LI

		if ((info.pc & 3) != 0)
		{
			throw "Invalid pc alignment";
		}

		if ((info.Imm.Address & 3) != 0)
		{
			throw "Invalid target address alignment";
		}

		int32_t li = 0;

		if (aa)
		{
			// Absolute addressing covers the following address ranges: [0, 0x1FF'FFFC], [0xfe00'0000, 0xFFFF'FFFC]  (canonical address space)
			// The first range is available when msb LI is zero, the second range is available when msb LI is 1.

			if ((info.Imm.Address & 0xFE00'0000) == 0xFE00'0000)
			{
				// This option is obtained when (LI || 0b00) msb is 1 (mask 0x2000'0000).
				// In this case, the target address will be sign-extended and equal to (0xFC00'0000 | (LI || 0b00)).

				li = info.Imm.Address & ~0xFC00'0000;
			}
			else if ((info.Imm.Address & 0xFE00'0000) == 0)
			{
				// This option is obtained when (LI || 0b00) msb is zero.

				li = info.Imm.Address & ~0xFC00'0000;
			}
			else
			{
				throw "Invalid absolute target address";
			}
		}
		else
		{
			li = info.Imm.Address - info.pc;
			if (li >= 0x02000000 || li < -0x02000000)
			{
				throw "Branch out of range";
			}
		}

		PackBits(res, 6, 31, li & ~0xFC00'0003);

		if (aa) SetBit(res, 30);
		if (lk) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_BranchShort(size_t primary, bool aa, bool lk, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);

		CheckParam(info, 0, Param::Num);		// BO
		CheckParam(info, 1, Param::Num);		// BI
		CheckParam(info, 2, Param::Address);

		// Calculate BD

		if ((info.pc & 3) != 0)
		{
			throw "Invalid pc alignment";
		}

		if ((info.Imm.Address & 3) != 0)
		{
			throw "Invalid target address alignment";
		}

		int32_t bd = 0;

		if (aa)
		{
			// Absolute addressing covers the following address ranges: [0, 0x7FFC], [0xffff'8000, 0xFFFF'FFFC]  (canonical address space)
			// The first range is available when msb BD is zero, the second range is available when msb BD is 1.

			if ((info.Imm.Address & 0xFFFF'8000) == 0xFFFF'8000)
			{
				// This option is obtained when (BD || 0b00) msb is 1 (mask 0x8000).
				// In this case, the target address will be sign-extended and equal to (0xFFFF'0000 | (BD || 0b00)).

				bd = info.Imm.Address & 0xFFFF;
			}
			else if ((info.Imm.Address & 0xFFFF'8000) == 0)
			{
				// This option is obtained when (BD || 0b00) msb is zero.

				bd = info.Imm.Address & 0xFFFF;
			}
			else
			{
				throw "Invalid absolute target address";
			}
		}
		else
		{
			bd = info.Imm.Address - info.pc;
			if (bd >= 0x8000 || bd < -0x8000)
			{
				throw "Branch out of range";
			}
		}

		PackBits(res, 6, 10, info.paramBits[0]);		// BO
		PackBits(res, 11, 15, info.paramBits[1]);		// BI

		PackBits(res, 16, 31, bd & 0xFFFC);

		if (aa) SetBit(res, 30);
		if (lk) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_BOBI(size_t primary, size_t extended, bool lk, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Num);		// BO
		CheckParam(info, 1, Param::Num);		// BI

		PackBits(res, 6, 10, info.paramBits[0]);		// BO
		PackBits(res, 11, 15, info.paramBits[1]);		// BI

		if (lk) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_CrfDAB(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Crf);
		CheckParam(info, 1, Param::Reg);
		CheckParam(info, 2, Param::Reg);

		PackBits(res, 6, 8, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 16, 20, info.paramBits[2]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_CrfDCrfS(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Crf);
		CheckParam(info, 1, Param::Crf);

		PackBits(res, 6, 8, info.paramBits[0]);
		PackBits(res, 11, 13, info.paramBits[1]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_CrfDASimm(size_t primary, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);

		CheckParam(info, 0, Param::Crf);
		CheckParam(info, 1, Param::Reg);
		CheckParam(info, 2, Param::Simm);

		PackBits(res, 6, 8, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 16, 31, info.Imm.Signed & 0xffff);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_CrfDAUimm(size_t primary, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);

		CheckParam(info, 0, Param::Crf);
		CheckParam(info, 1, Param::Reg);
		CheckParam(info, 2, Param::Uimm);

		PackBits(res, 6, 8, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 16, 31, info.Imm.Unsigned);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_AS(size_t primary, size_t extended, bool rc, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::Reg);

		PackBits(res, 6, 10, info.paramBits[1]);	// Reversed order of parameters
		PackBits(res, 11, 15, info.paramBits[0]);

		if (rc) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_CrbDAB(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Crb);
		CheckParam(info, 1, Param::Crb);
		CheckParam(info, 2, Param::Crb);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 16, 20, info.paramBits[2]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_AB(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::Reg);

		PackBits(res, 11, 15, info.paramBits[0]);
		PackBits(res, 16, 20, info.paramBits[1]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_SAB(size_t primary, size_t extended, bool oe, bool rc, AnalyzeInfo& info)
	{
		Form_DAB(primary, extended, oe, rc, info);
	}

	void GekkoAssembler::Form_Extended(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_AS_SHMBME(size_t primary, bool rc, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::Reg);
		CheckParam(info, 2, Param::Num);		// SH
		CheckParam(info, 3, Param::Num);		// MB
		CheckParam(info, 4, Param::Num);		// ME

		PackBits(res, 6, 10, info.paramBits[1]);		// Reversed order
		PackBits(res, 11, 15, info.paramBits[0]);
		PackBits(res, 16, 20, info.paramBits[2]);
		PackBits(res, 21, 25, info.paramBits[3]);
		PackBits(res, 26, 30, info.paramBits[4]);

		if (rc) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_ASB_MBME(size_t primary, bool rc, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::Reg);
		CheckParam(info, 2, Param::Reg);
		CheckParam(info, 3, Param::Num);		// MB
		CheckParam(info, 4, Param::Num);		// ME

		PackBits(res, 6, 10, info.paramBits[1]);		// Reversed order
		PackBits(res, 11, 15, info.paramBits[0]);
		PackBits(res, 16, 20, info.paramBits[2]);
		PackBits(res, 21, 25, info.paramBits[3]);
		PackBits(res, 26, 30, info.paramBits[4]);

		if (rc) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_AS_SH(size_t primary, size_t extended, bool rc, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::Reg);
		CheckParam(info, 2, Param::Num);		// SH

		PackBits(res, 6, 10, info.paramBits[1]);		// Reversed order
		PackBits(res, 11, 15, info.paramBits[0]);
		PackBits(res, 16, 20, info.paramBits[2]);

		if (rc) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_FrDAB(size_t primary, size_t extended, bool rc, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 26, 30, extended);

		CheckParam(info, 0, Param::FReg);
		CheckParam(info, 1, Param::FReg);
		CheckParam(info, 2, Param::FReg);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 16, 20, info.paramBits[2]);

		if (rc) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_FrDAC(size_t primary, size_t extended, bool rc, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 26, 30, extended);

		CheckParam(info, 0, Param::FReg);
		CheckParam(info, 1, Param::FReg);
		CheckParam(info, 2, Param::FReg);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 21, 25, info.paramBits[2]);

		if (rc) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_FrDB(size_t primary, size_t extended, bool rc, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);			// The C field is used as part of the extended opcode. 

		CheckParam(info, 0, Param::FReg);
		CheckParam(info, 1, Param::FReg);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 16, 20, info.paramBits[1]);

		if (rc) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_FrDACB(size_t primary, size_t extended, bool rc, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 26, 30, extended);

		CheckParam(info, 0, Param::FReg);
		CheckParam(info, 1, Param::FReg);
		CheckParam(info, 2, Param::FReg);
		CheckParam(info, 3, Param::FReg);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 21, 25, info.paramBits[2]);
		PackBits(res, 16, 20, info.paramBits[3]);

		if (rc) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_FrD(size_t primary, size_t extended, bool rc, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 26, 30, extended);

		CheckParam(info, 0, Param::FReg);

		PackBits(res, 6, 10, info.paramBits[0]);

		if (rc) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_CrfDFrAB(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Crf);
		CheckParam(info, 1, Param::FReg);
		CheckParam(info, 2, Param::FReg);

		PackBits(res, 6, 8, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 16, 20, info.paramBits[2]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_CrbD(size_t primary, size_t extended, bool rc, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 26, 30, extended);

		CheckParam(info, 0, Param::Crb);

		PackBits(res, 6, 10, info.paramBits[0]);

		if (rc) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_Mtfsf(size_t primary, size_t extended, bool rc, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 26, 30, extended);

		CheckParam(info, 0, Param::FM);
		CheckParam(info, 1, Param::FReg);

		PackBits(res, 7, 14, info.paramBits[0]);
		PackBits(res, 16, 20, info.paramBits[1]);

		if (rc) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_Mtfsfi(size_t primary, size_t extended, bool rc, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 26, 30, extended);

		CheckParam(info, 0, Param::Crf);
		CheckParam(info, 1, Param::Num);

		PackBits(res, 6, 8, info.paramBits[0]);
		PackBits(res, 16, 19, info.paramBits[1]);

		if (rc) SetBit(res, 31);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_DA_Offset(size_t primary, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::RegOffset);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 16, 31, info.Imm.Unsigned);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_SA_Offset(size_t primary, AnalyzeInfo& info)
	{
		Form_DA_Offset(primary, info);
	}

	void GekkoAssembler::Form_DA_NB(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::Reg);
		CheckParam(info, 2, Param::Num);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 16, 20, info.paramBits[2]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_SA_NB(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		Form_DA_NB(primary, extended, info);
	}

	void GekkoAssembler::Form_FrDA_Offset(size_t primary, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);

		CheckParam(info, 0, Param::FReg);
		CheckParam(info, 1, Param::RegOffset);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 16, 31, info.Imm.Unsigned);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_FrSA_Offset(size_t primary, AnalyzeInfo& info)
	{
		Form_FrDA_Offset(primary, info);
	}

	void GekkoAssembler::Form_FrDRegAB(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::FReg);
		CheckParam(info, 1, Param::Reg);
		CheckParam(info, 2, Param::Reg);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 16, 20, info.paramBits[2]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_FrSRegAB(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		Form_FrDRegAB(primary, extended, info);
	}

	void GekkoAssembler::Form_Trap(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Num);
		CheckParam(info, 1, Param::Reg);
		CheckParam(info, 2, Param::Reg);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 16, 20, info.paramBits[1]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_TrapImm(size_t primary, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);

		CheckParam(info, 0, Param::Num);
		CheckParam(info, 1, Param::Reg);
		CheckParam(info, 2, Param::Simm);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 16, 31, (uint16_t)info.Imm.Signed);

		info.instrBits = res;
	}

#pragma region "Processor Control Instructions"

	void GekkoAssembler::Form_Mcrxr(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Crf);

		PackBits(res, 6, 8, info.paramBits[0]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_Mfcr(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Reg);

		PackBits(res, 6, 10, info.paramBits[0]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_Mfmsr(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Reg);

		PackBits(res, 6, 10, info.paramBits[0]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_Mfspr(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::Spr);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 20, info.paramBits[1]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_Mftb(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::Tbr);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 20, info.paramBits[1]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_Mtcrf(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::CRM);
		CheckParam(info, 1, Param::Reg);

		PackBits(res, 12, 19, info.paramBits[0]);
		PackBits(res, 6, 10, info.paramBits[1]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_Mtmsr(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Reg);

		PackBits(res, 6, 10, info.paramBits[0]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_Mtspr(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Spr);
		CheckParam(info, 1, Param::Reg);

		PackBits(res, 11, 20, info.paramBits[0]);
		PackBits(res, 6, 10, info.paramBits[1]);

		info.instrBits = res;
	}

#pragma endregion "Processor Control Instructions"

#pragma region "Segment Register Manipulation Instructions"

	void GekkoAssembler::Form_Mfsr(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::Sr);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 12, 15, info.paramBits[1]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_Mfsrin(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::Reg);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 16, 20, info.paramBits[1]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_Mtsr(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Sr);
		CheckParam(info, 1, Param::Reg);

		PackBits(res, 12, 15, info.paramBits[0]);
		PackBits(res, 6, 10, info.paramBits[1]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_Mtsrin(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::Reg);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 16, 20, info.paramBits[1]);

		info.instrBits = res;
	}

#pragma endregion "Segment Register Manipulation Instructions"

	void GekkoAssembler::Form_Tlbie(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 21, 30, extended);

		CheckParam(info, 0, Param::Reg);

		PackBits(res, 16, 20, info.paramBits[0]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_PsqLoadStoreIndexed(size_t primary, size_t extended, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);
		PackBits(res, 25, 30, extended);

		CheckParam(info, 0, Param::FReg);
		CheckParam(info, 1, Param::Reg);
		CheckParam(info, 2, Param::Reg);
		CheckParam(info, 3, Param::Num);
		CheckParam(info, 4, Param::Num);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 16, 20, info.paramBits[2]);
		PackBits(res, 21, 21, info.paramBits[3]);
		PackBits(res, 22, 24, info.paramBits[4]);

		info.instrBits = res;
	}

	void GekkoAssembler::Form_PsqLoadStore(size_t primary, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);

		CheckParam(info, 0, Param::FReg);
		CheckParam(info, 1, Param::RegOffset);
		CheckParam(info, 2, Param::Num);
		CheckParam(info, 3, Param::Num);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 16, 16, info.paramBits[2]);
		PackBits(res, 17, 19, info.paramBits[3]);
		PackBits(res, 20, 31, info.Imm.Signed & 0xfff);

		info.instrBits = res;
	}

	void GekkoAssembler::Assemble(AnalyzeInfo& info)
	{
		// The GUM uses the non-standard DAB form. The extended opcode field for these instructions appears on the OE field for regular DAB format.
		// Therefore, you have to mask msb for the extended opcode value, and set the OE field to true.
		constexpr auto MaskedOeBit = 0b01'1111'1111;

		// The format of Gekko (PowerPC) instructions is a bit similar to MIPS, only about 350 instructions.
		// Just go through all the instructions from the GUM (google: "IBM Gekko User's Manual pdf")

		switch (info.instr)
		{
			// Integer Arithmetic Instructions

			case Instruction::add:			Form_DAB(31, 266, false, false, info); break;
			case Instruction::add_d:		Form_DAB(31, 266, false, true, info); break;
			case Instruction::addo:			Form_DAB(31, 266, true, false, info); break;
			case Instruction::addo_d:		Form_DAB(31, 266, true, true, info); break;
			case Instruction::addc:			Form_DAB(31, 10, false, false, info); break;
			case Instruction::addc_d:		Form_DAB(31, 10, false, true, info); break;
			case Instruction::addco:		Form_DAB(31, 10, true, false, info); break;
			case Instruction::addco_d:		Form_DAB(31, 10, true, true, info); break;
			case Instruction::adde:			Form_DAB(31, 138, false, false, info); break;
			case Instruction::adde_d:		Form_DAB(31, 138, false, true, info); break;
			case Instruction::addeo:		Form_DAB(31, 138, true, false, info); break;
			case Instruction::addeo_d:		Form_DAB(31, 138, true, true, info); break;
			case Instruction::addi:			Form_DASimm(14, info); break;
			case Instruction::addic:		Form_DASimm(12, info); break;
			case Instruction::addic_d:		Form_DASimm(13, info); break;
			case Instruction::addis:		Form_DASimm(15, info); break;
			case Instruction::addme:		Form_DA(31, 234, false, false, info); break;
			case Instruction::addme_d:		Form_DA(31, 234, false, true, info); break;
			case Instruction::addmeo:		Form_DA(31, 234, true, false, info); break;
			case Instruction::addmeo_d:		Form_DA(31, 234, true, true, info); break;
			case Instruction::addze:		Form_DA(31, 202, false, false, info); break;
			case Instruction::addze_d:		Form_DA(31, 202, false, true, info); break;
			case Instruction::addzeo:		Form_DA(31, 202, true, false, info); break;
			case Instruction::addzeo_d:		Form_DA(31, 202, true, true, info); break;
			case Instruction::divw:			Form_DAB(31, 491, false, false, info); break;
			case Instruction::divw_d:		Form_DAB(31, 491, false, true, info); break;
			case Instruction::divwo:		Form_DAB(31, 491, true, false, info); break;
			case Instruction::divwo_d:		Form_DAB(31, 491, true, true, info); break;
			case Instruction::divwu:		Form_DAB(31, 459, false, false, info); break;
			case Instruction::divwu_d:		Form_DAB(31, 459, false, true, info); break;
			case Instruction::divwuo:		Form_DAB(31, 459, true, false, info); break;
			case Instruction::divwuo_d:		Form_DAB(31, 459, true, true, info); break;
			case Instruction::mulhw:		Form_DAB(31, 75, false, false, info); break;
			case Instruction::mulhw_d:		Form_DAB(31, 75, false, true, info); break;
			case Instruction::mulhwu:		Form_DAB(31, 11, false, false, info); break;
			case Instruction::mulhwu_d:		Form_DAB(31, 11, false, true, info); break;
			case Instruction::mulli:		Form_DASimm(7, info); break;
			case Instruction::mullw:		Form_DAB(31, 235, false, false, info); break;
			case Instruction::mullw_d:		Form_DAB(31, 235, false, true, info); break;
			case Instruction::mullwo:		Form_DAB(31, 235, true, false, info); break;
			case Instruction::mullwo_d:		Form_DAB(31, 235, true, true, info); break;
			case Instruction::neg:			Form_DA(31, 104, false, false, info); break;
			case Instruction::neg_d:		Form_DA(31, 104, false, true, info); break;
			case Instruction::nego:			Form_DA(31, 104, true, false, info); break;
			case Instruction::nego_d:		Form_DA(31, 104, true, true, info); break;
			case Instruction::subf:			Form_DAB(31, 40, false, false, info); break;
			case Instruction::subf_d:		Form_DAB(31, 40, false, true, info); break;
			case Instruction::subfo:		Form_DAB(31, 40, true, false, info); break;
			case Instruction::subfo_d:		Form_DAB(31, 40, true, true, info); break;
			case Instruction::subfc:		Form_DAB(31, 8, false, false, info); break;
			case Instruction::subfc_d:		Form_DAB(31, 8, false, true, info); break;
			case Instruction::subfco:		Form_DAB(31, 8, true, false, info); break;
			case Instruction::subfco_d:		Form_DAB(31, 8, true, true, info); break;
			case Instruction::subfe:		Form_DAB(31, 136, false, false, info); break;
			case Instruction::subfe_d:		Form_DAB(31, 136, false, true, info); break;
			case Instruction::subfeo:		Form_DAB(31, 136, true, false, info); break;
			case Instruction::subfeo_d:		Form_DAB(31, 136, true, true, info); break;
			case Instruction::subfic:		Form_DASimm(8, info); break;
			case Instruction::subfme:		Form_DA(31, 232, false, false, info); break;
			case Instruction::subfme_d:		Form_DA(31, 232, false, true, info); break;
			case Instruction::subfmeo:		Form_DA(31, 232, true, false, info); break;
			case Instruction::subfmeo_d:	Form_DA(31, 232, true, true, info); break;
			case Instruction::subfze:		Form_DA(31, 200, false, false, info); break;
			case Instruction::subfze_d:		Form_DA(31, 200, false, true, info); break;
			case Instruction::subfzeo:		Form_DA(31, 200, true, false, info); break;
			case Instruction::subfzeo_d:	Form_DA(31, 200, true, true, info); break;

			// Integer Compare Instructions

			case Instruction::cmp:			Form_CrfDAB(31, 0, info); break;
			case Instruction::cmpi:			Form_CrfDASimm(11, info); break;
			case Instruction::cmpl:			Form_CrfDAB(31, 32, info); break;
			case Instruction::cmpli:		Form_CrfDAUimm(10, info); break;

			// Integer Logical Instructions

			case Instruction::_and:			Form_ASB(31, 28, false, info); break;
			case Instruction::and_d:		Form_ASB(31, 28, true, info); break;
			case Instruction::andc:			Form_ASB(31, 60, false, info); break;
			case Instruction::andc_d:		Form_ASB(31, 60, true, info); break;
			case Instruction::andi_d:		Form_ASUimm(28, info); break;
			case Instruction::andis_d:		Form_ASUimm(29, info); break;
			case Instruction::cntlzw:		Form_AS(31, 26, false, info); break;
			case Instruction::cntlzw_d:		Form_AS(31, 26, true, info); break;
			case Instruction::eqv:			Form_ASB(31, 284, false, info); break;
			case Instruction::eqv_d:		Form_ASB(31, 284, true, info); break;
			case Instruction::extsb:		Form_AS(31, 954, false, info); break;
			case Instruction::extsb_d:		Form_AS(31, 954, true, info); break;
			case Instruction::extsh:		Form_AS(31, 922, false, info); break;
			case Instruction::extsh_d:		Form_AS(31, 922, true, info); break;
			case Instruction::nand:			Form_ASB(31, 476, false, info); break;
			case Instruction::nand_d:		Form_ASB(31, 476, true, info); break;
			case Instruction::nor:			Form_ASB(31, 124, false, info); break;
			case Instruction::nor_d:		Form_ASB(31, 124, true, info); break;
			case Instruction::_or:			Form_ASB(31, 444, false, info); break;
			case Instruction::or_d:			Form_ASB(31, 444, true, info); break;
			case Instruction::orc:			Form_ASB(31, 412, false, info); break;
			case Instruction::orc_d:		Form_ASB(31, 412, true, info); break;
			case Instruction::ori:			Form_ASUimm(24, info); break;
			case Instruction::oris:			Form_ASUimm(25, info); break;
			case Instruction::_xor:			Form_ASB(31, 316, false, info); break;
			case Instruction::xor_d:		Form_ASB(31, 316, true, info); break;
			case Instruction::xori:			Form_ASUimm(26, info); break;
			case Instruction::xoris:		Form_ASUimm(27, info); break;

			// Integer Rotate Instructions

			case Instruction::rlwimi:		Form_AS_SHMBME(20, false, info); break;
			case Instruction::rlwimi_d:		Form_AS_SHMBME(20, true, info); break;
			case Instruction::rlwinm:		Form_AS_SHMBME(21, false, info); break;
			case Instruction::rlwinm_d:		Form_AS_SHMBME(21, true, info); break;
			case Instruction::rlwnm:		Form_ASB_MBME(23, false, info); break;
			case Instruction::rlwnm_d:		Form_ASB_MBME(23, true, info); break;

			// Integer Shift Instructions

			case Instruction::slw:			Form_ASB(31, 24, false, info); break;
			case Instruction::slw_d:		Form_ASB(31, 24, true, info); break;
			case Instruction::sraw:			Form_ASB(31, 792, false, info); break;
			case Instruction::sraw_d:		Form_ASB(31, 792, true, info); break;
			case Instruction::srawi:		Form_AS_SH(31, 824, false, info); break;
			case Instruction::srawi_d:		Form_AS_SH(31, 824, true, info); break;
			case Instruction::srw:			Form_ASB(31, 536, false, info); break;
			case Instruction::srw_d:		Form_ASB(31, 536, true, info); break;

			// Floating-Point Arithmetic Instructions

			case Instruction::fadd:			Form_FrDAB(63, 21, false, info); break;
			case Instruction::fadd_d:		Form_FrDAB(63, 21, true, info); break;
			case Instruction::fadds:		Form_FrDAB(59, 21, false, info); break;
			case Instruction::fadds_d:		Form_FrDAB(59, 21, true, info); break;
			case Instruction::fdiv:			Form_FrDAB(63, 18, false, info); break;
			case Instruction::fdiv_d:		Form_FrDAB(63, 18, true, info); break;
			case Instruction::fdivs:		Form_FrDAB(59, 18, false, info); break;
			case Instruction::fdivs_d:		Form_FrDAB(59, 18, true, info); break;
			case Instruction::fmul:			Form_FrDAC(63, 25, false, info); break;
			case Instruction::fmul_d:		Form_FrDAC(63, 25, true, info); break;
			case Instruction::fmuls:		Form_FrDAC(59, 25, false, info); break;
			case Instruction::fmuls_d:		Form_FrDAC(59, 25, true, info); break;
			case Instruction::fres:			Form_FrDB(59, 24, false, info); break;
			case Instruction::fres_d:		Form_FrDB(59, 24, true, info); break;
			case Instruction::frsqrte:		Form_FrDB(63, 26, false, info); break;
			case Instruction::frsqrte_d:	Form_FrDB(63, 26, true, info); break;
			case Instruction::fsub:			Form_FrDAB(63, 20, false, info); break;
			case Instruction::fsub_d:		Form_FrDAB(63, 20, true, info); break;
			case Instruction::fsubs:		Form_FrDAB(59, 20, false, info); break;
			case Instruction::fsubs_d:		Form_FrDAB(59, 20, true, info); break;
			case Instruction::fsel:			Form_FrDACB(63, 23, false, info); break;
			case Instruction::fsel_d:		Form_FrDACB(63, 23, true, info); break;

			// Floating-Point Multiply-Add Instructions

			case Instruction::fmadd:		Form_FrDACB(63, 29, false, info); break;
			case Instruction::fmadd_d:		Form_FrDACB(63, 29, true, info); break;
			case Instruction::fmadds:		Form_FrDACB(59, 29, false, info); break;
			case Instruction::fmadds_d:		Form_FrDACB(59, 29, true, info); break;
			case Instruction::fmsub:		Form_FrDACB(63, 28, false, info); break;
			case Instruction::fmsub_d:		Form_FrDACB(63, 28, true, info); break;
			case Instruction::fmsubs:		Form_FrDACB(59, 28, false, info); break;
			case Instruction::fmsubs_d:		Form_FrDACB(59, 28, true, info); break;
			case Instruction::fnmadd:		Form_FrDACB(63, 31, false, info); break;
			case Instruction::fnmadd_d:		Form_FrDACB(63, 31, true, info); break;
			case Instruction::fnmadds:		Form_FrDACB(59, 31, false, info); break;
			case Instruction::fnmadds_d:	Form_FrDACB(59, 31, true, info); break;
			case Instruction::fnmsub:		Form_FrDACB(63, 30, false, info); break;
			case Instruction::fnmsub_d:		Form_FrDACB(63, 30, true, info); break;
			case Instruction::fnmsubs:		Form_FrDACB(59, 30, false, info); break;
			case Instruction::fnmsubs_d:	Form_FrDACB(59, 30, true, info); break;

			// Floating-Point Rounding and Conversion Instructions

			case Instruction::fctiw:		Form_FrDB(63, 14, false, info); break;
			case Instruction::fctiw_d:		Form_FrDB(63, 14, true, info); break;
			case Instruction::fctiwz:		Form_FrDB(63, 15, false, info); break;
			case Instruction::fctiwz_d:		Form_FrDB(63, 15, true, info); break;
			case Instruction::frsp:			Form_FrDB(63, 12, false, info); break;
			case Instruction::frsp_d:		Form_FrDB(63, 12, true, info); break;

			// Floating-Point Compare Instructions

			case Instruction::fcmpo:		Form_CrfDFrAB(63, 32, info); break;
			case Instruction::fcmpu:		Form_CrfDFrAB(63, 0, info); break;

			// Floating-Point Status and Control Register Instructions
			
			case Instruction::mcrfs:		Form_CrfDCrfS(63, 64, info); break;
			case Instruction::mffs:			Form_FrD(63, 583, false, info); break;
			case Instruction::mffs_d:		Form_FrD(63, 583, true, info); break;
			case Instruction::mtfsb0:		Form_CrbD(63, 70, false, info); break;
			case Instruction::mtfsb0_d:		Form_CrbD(63, 70, true, info); break;
			case Instruction::mtfsb1:		Form_CrbD(63, 38, false, info); break;
			case Instruction::mtfsb1_d:		Form_CrbD(63, 38, true, info); break;
			case Instruction::mtfsf:		Form_Mtfsf(63, 711, false, info); break;
			case Instruction::mtfsf_d:		Form_Mtfsf(63, 711, true, info); break;
			case Instruction::mtfsfi:		Form_Mtfsfi(63, 134, false, info); break;
			case Instruction::mtfsfi_d:		Form_Mtfsfi(63, 134, true, info); break;

			// Integer Load Instructions

			case Instruction::lbz:			Form_DA_Offset(34, info); break;
			case Instruction::lbzu:			Form_DA_Offset(35, info); break;
			case Instruction::lbzux:		Form_DAB(31, 119, false, false, info); break;
			case Instruction::lbzx:			Form_DAB(31, 87, false, false, info); break;
			case Instruction::lha:			Form_DA_Offset(42, info); break;
			case Instruction::lhau:			Form_DA_Offset(43, info); break;
			case Instruction::lhaux:		Form_DAB(31, 375, false, false, info); break;
			case Instruction::lhax:			Form_DAB(31, 343, false, false, info); break;
			case Instruction::lhz:			Form_DA_Offset(40, info); break;
			case Instruction::lhzu:			Form_DA_Offset(41, info); break;
			case Instruction::lhzux:		Form_DAB(31, 311, false, false, info); break;
			case Instruction::lhzx:			Form_DAB(31, 279, false, false, info); break;
			case Instruction::lwz:			Form_DA_Offset(32, info); break;
			case Instruction::lwzu:			Form_DA_Offset(33, info); break;
			case Instruction::lwzux:		Form_DAB(31, 55, false, false, info); break;
			case Instruction::lwzx:			Form_DAB(31, 23, false, false, info); break;

			// Integer Store Instructions

			case Instruction::stb:			Form_SA_Offset(38, info); break;
			case Instruction::stbu:			Form_SA_Offset(39, info); break;
			case Instruction::stbux:		Form_SAB(31, 247, false, false, info); break;
			case Instruction::stbx:			Form_SAB(31, 215, false, false, info); break;
			case Instruction::sth:			Form_SA_Offset(44, info); break;
			case Instruction::sthu:			Form_SA_Offset(45, info); break;
			case Instruction::sthux:		Form_SAB(31, 439, false, false, info); break;
			case Instruction::sthx:			Form_SAB(31, 407, false, false, info); break;
			case Instruction::stw:			Form_SA_Offset(36, info); break;
			case Instruction::stwu:			Form_SA_Offset(37, info); break;
			case Instruction::stwux:		Form_SAB(31, 183, false, false, info); break;
			case Instruction::stwx:			Form_SAB(31, 151, false, false, info); break;

			// Integer Load and Store with Byte Reverse Instructions

			case Instruction::lhbrx:		Form_DAB(31, 790 & MaskedOeBit, true, false, info); break;
			case Instruction::lwbrx:		Form_DAB(31, 534 & MaskedOeBit, true, false, info); break;
			case Instruction::sthbrx:		Form_SAB(31, 918 & MaskedOeBit, true, false, info); break;
			case Instruction::stwbrx:		Form_SAB(31, 662 & MaskedOeBit, true, false, info); break;

			// Integer Load and Store Multiple Instructions

			case Instruction::lmw:			Form_DA_Offset(46, info); break;
			case Instruction::stmw:			Form_SA_Offset(47, info); break;

			// Integer Load and Store String Instructions

			case Instruction::lswi:			Form_DA_NB(31, 597, info); break;
			case Instruction::lswx:			Form_DAB(31, 533 & MaskedOeBit, true, false, info); break;
			case Instruction::stswi:		Form_SA_NB(31, 725, info); break;
			case Instruction::stswx:		Form_SAB(31, 661 & MaskedOeBit, true, false, info); break;

			// Memory Synchronization Instructions

			case Instruction::eieio:		Form_Extended(31, 854, info); break;
			case Instruction::isync:		Form_Extended(19, 150, info); break;
			case Instruction::lwarx:		Form_DAB(31, 20, false, false, info); break;
			case Instruction::stwcx_d:		Form_SAB(31, 150, false, true, info); break;
			case Instruction::sync:			Form_Extended(31, 598, info); break;

			// Floating-Point Load Instructions

			case Instruction::lfd:			Form_FrDA_Offset(50, info); break;
			case Instruction::lfdu:			Form_FrDA_Offset(51, info); break;
			case Instruction::lfdux:		Form_FrDRegAB(31, 631, info); break;
			case Instruction::lfdx:			Form_FrDRegAB(31, 599, info); break;
			case Instruction::lfs:			Form_FrDA_Offset(48, info); break;
			case Instruction::lfsu:			Form_FrDA_Offset(49, info); break;
			case Instruction::lfsux:		Form_FrDRegAB(31, 567, info); break;
			case Instruction::lfsx:			Form_FrDRegAB(31, 535, info); break;

			// Floating-Point Store Instructions

			case Instruction::stfd:			Form_FrSA_Offset(54, info); break;
			case Instruction::stfdu:		Form_FrSA_Offset(55, info); break;
			case Instruction::stfdux:		Form_FrSRegAB(31, 759, info); break;
			case Instruction::stfdx:		Form_FrSRegAB(31, 727, info); break;
			case Instruction::stfiwx:		Form_FrSRegAB(31, 983, info); break;
			case Instruction::stfs:			Form_FrSA_Offset(52, info); break;
			case Instruction::stfsu:		Form_FrSA_Offset(53, info); break;
			case Instruction::stfsux:		Form_FrSRegAB(31, 695, info); break;
			case Instruction::stfsx:		Form_FrSRegAB(31, 663, info); break;

			// Floating-Point Move Instructions

			case Instruction::fabs:			Form_FrDB(63, 264, false, info); break;
			case Instruction::fabs_d:		Form_FrDB(63, 264, true, info); break;
			case Instruction::fmr:			Form_FrDB(63, 72, false, info); break;
			case Instruction::fmr_d:		Form_FrDB(63, 72, true, info); break;
			case Instruction::fnabs:		Form_FrDB(63, 136, false, info); break;
			case Instruction::fnabs_d:		Form_FrDB(63, 136, true, info); break;
			case Instruction::fneg:			Form_FrDB(63, 40, false, info); break;
			case Instruction::fneg_d:		Form_FrDB(63, 40, true, info); break;

			// Branch Instructions

			case Instruction::b:			Form_BranchLong(18, false, false, info); break;
			case Instruction::ba:			Form_BranchLong(18, true, false, info); break;
			case Instruction::bl:			Form_BranchLong(18, false, true, info); break;
			case Instruction::bla:			Form_BranchLong(18, true, true, info); break;
			case Instruction::bc:			Form_BranchShort(16, false, false, info); break;
			case Instruction::bca:			Form_BranchShort(16, true, false, info); break;
			case Instruction::bcl:			Form_BranchShort(16, false, true, info); break;
			case Instruction::bcla:			Form_BranchShort(16, true, true, info); break;
			case Instruction::bcctr:		Form_BOBI(19, 528, false, info); break;
			case Instruction::bcctrl:		Form_BOBI(19, 528, true, info); break;
			case Instruction::bclr:			Form_BOBI(19, 16, false, info); break;
			case Instruction::bclrl:		Form_BOBI(19, 16, true, info); break;

			// Condition Register Logical Instructions

			case Instruction::crand:		Form_CrbDAB(19, 257, info); break;
			case Instruction::crandc:		Form_CrbDAB(19, 129, info); break;
			case Instruction::creqv:		Form_CrbDAB(19, 289, info); break;
			case Instruction::crnand:		Form_CrbDAB(19, 225, info); break;
			case Instruction::crnor:		Form_CrbDAB(19, 33, info); break;
			case Instruction::cror:			Form_CrbDAB(19, 449, info); break;
			case Instruction::crorc:		Form_CrbDAB(19, 417, info); break;
			case Instruction::crxor:		Form_CrbDAB(19, 193, info); break;
			case Instruction::mcrf:			Form_CrfDCrfS(19, 0, info); break;

			// System Linkage Instructions

			case Instruction::rfi:			Form_Extended(19, 50, info); break;
			case Instruction::sc:			Form_Extended(17, 1, info); break;

			// Trap Instructions

			case Instruction::tw:			Form_Trap(31, 4, info); break;
			case Instruction::twi:			Form_TrapImm(3, info); break;

			// Processor Control Instructions

			case Instruction::mcrxr:		Form_Mcrxr(31, 512, info); break;
			case Instruction::mfcr:			Form_Mfcr(31, 19, info); break;
			case Instruction::mfmsr:		Form_Mfmsr(31, 83, info); break;
			case Instruction::mfspr:		Form_Mfspr(31, 339, info); break;
			case Instruction::mftb:			Form_Mftb(31, 371, info); break;
			case Instruction::mtcrf:		Form_Mtcrf(31, 144, info); break;
			case Instruction::mtmsr:		Form_Mtmsr(31, 146, info); break;
			case Instruction::mtspr:		Form_Mtspr(31, 467, info); break;

			// Cache Management Instructions

			case Instruction::dcbf:			Form_AB(31, 86, info); break;
			case Instruction::dcbi:			Form_AB(31, 470, info); break;
			case Instruction::dcbst:		Form_AB(31, 54, info); break;
			case Instruction::dcbt:			Form_AB(31, 278, info); break;
			case Instruction::dcbtst:		Form_AB(31, 246, info); break;
			case Instruction::dcbz:			Form_AB(31, 1014, info); break;
			case Instruction::dcbz_l:		Form_AB(4, 1014, info); break;
			case Instruction::icbi:			Form_AB(31, 982, info); break;

			// Segment Register Manipulation Instructions

			case Instruction::mfsr:			Form_Mfsr(31, 595, info); break;
			case Instruction::mfsrin:		Form_Mfsrin(31, 659, info); break;
			case Instruction::mtsr:			Form_Mtsr(31, 210, info); break;
			case Instruction::mtsrin:		Form_Mtsrin(31, 242, info); break;

			// Lookaside Buffer Management Instructions

			case Instruction::tlbie:		Form_Tlbie(31, 306, info); break;
			case Instruction::tlbsync:		Form_Extended(31, 566, info); break;

			// External Control Instructions

			case Instruction::eciwx:		Form_DAB(31, 310, false, false, info); break;
			case Instruction::ecowx:		Form_SAB(31, 438, false, false, info); break;

			// Paired-Single Load and Store Instructions

			case Instruction::psq_lx:		Form_PsqLoadStoreIndexed(4, 6, info); break;
			case Instruction::psq_stx:		Form_PsqLoadStoreIndexed(4, 7, info); break;
			case Instruction::psq_lux:		Form_PsqLoadStoreIndexed(4, 38, info); break;
			case Instruction::psq_stux:		Form_PsqLoadStoreIndexed(4, 39, info); break;
			case Instruction::psq_l:		Form_PsqLoadStore(56, info); break;
			case Instruction::psq_lu:		Form_PsqLoadStore(57, info); break;
			case Instruction::psq_st:		Form_PsqLoadStore(60, info); break;
			case Instruction::psq_stu:		Form_PsqLoadStore(61, info); break;

			// Paired-Single Floating Point Arithmetic Instructions

			case Instruction::ps_div:		Form_FrDAB(4, 18, false, info); break;
			case Instruction::ps_div_d:		Form_FrDAB(4, 18, true, info); break;
			case Instruction::ps_sub:		Form_FrDAB(4, 20, false, info); break;
			case Instruction::ps_sub_d:		Form_FrDAB(4, 20, true, info); break;
			case Instruction::ps_add:		Form_FrDAB(4, 21, false, info); break;
			case Instruction::ps_add_d:		Form_FrDAB(4, 21, true, info); break;
			case Instruction::ps_sel:		Form_FrDACB(4, 23, false, info); break;
			case Instruction::ps_sel_d:		Form_FrDACB(4, 23, true, info); break;
			case Instruction::ps_res:		Form_FrDB(4, 24, false, info); break;
			case Instruction::ps_res_d:		Form_FrDB(4, 24, true, info); break;
			case Instruction::ps_mul:		Form_FrDAC(4, 25, false, info); break;
			case Instruction::ps_mul_d:		Form_FrDAC(4, 25, true, info); break;
			case Instruction::ps_rsqrte:	Form_FrDB(4, 26, false, info); break;
			case Instruction::ps_rsqrte_d:	Form_FrDB(4, 26, true, info); break;
			case Instruction::ps_msub:		Form_FrDACB(4, 28, false, info); break;
			case Instruction::ps_msub_d:	Form_FrDACB(4, 28, true, info); break;
			case Instruction::ps_madd:		Form_FrDACB(4, 29, false, info); break;
			case Instruction::ps_madd_d:	Form_FrDACB(4, 29, true, info); break;
			case Instruction::ps_nmsub:		Form_FrDACB(4, 30, false, info); break;
			case Instruction::ps_nmsub_d:	Form_FrDACB(4, 30, true, info); break;
			case Instruction::ps_nmadd:		Form_FrDACB(4, 31, false, info); break;
			case Instruction::ps_nmadd_d:	Form_FrDACB(4, 31, true, info); break;
			case Instruction::ps_neg:		Form_FrDB(4, 40, false, info); break;
			case Instruction::ps_neg_d:		Form_FrDB(4, 40, true, info); break;
			case Instruction::ps_mr:		Form_FrDB(4, 72, false, info); break;
			case Instruction::ps_mr_d:		Form_FrDB(4, 72, true, info); break;
			case Instruction::ps_nabs:		Form_FrDB(4, 136, false, info); break;
			case Instruction::ps_nabs_d:	Form_FrDB(4, 136, true, info); break;
			case Instruction::ps_abs:		Form_FrDB(4, 264, false, info); break;
			case Instruction::ps_abs_d:		Form_FrDB(4, 264, true, info); break;

			// Miscellaneous Paired-Single Instructions

			case Instruction::ps_sum0:		Form_FrDACB(4, 10, false, info); break;
			case Instruction::ps_sum0_d:	Form_FrDACB(4, 10, true, info); break;
			case Instruction::ps_sum1:		Form_FrDACB(4, 11, false, info); break;
			case Instruction::ps_sum1_d:	Form_FrDACB(4, 11, true, info); break;
			case Instruction::ps_muls0:		Form_FrDAC(4, 12, false, info); break;
			case Instruction::ps_muls0_d:	Form_FrDAC(4, 12, true, info); break;
			case Instruction::ps_muls1:		Form_FrDAC(4, 13, false, info); break;
			case Instruction::ps_muls1_d:	Form_FrDAC(4, 13, true, info); break;
			case Instruction::ps_madds0:	Form_FrDACB(4, 14, false, info); break;
			case Instruction::ps_madds0_d:	Form_FrDACB(4, 14, true, info); break;
			case Instruction::ps_madds1:	Form_FrDACB(4, 15, false, info); break;
			case Instruction::ps_madds1_d:	Form_FrDACB(4, 15, true, info); break;
			case Instruction::ps_cmpu0:		Form_CrfDFrAB(4, 0, info); break;
			case Instruction::ps_cmpo0:		Form_CrfDFrAB(4, 32, info); break;
			case Instruction::ps_cmpu1:		Form_CrfDFrAB(4, 64, info); break;
			case Instruction::ps_cmpo1:		Form_CrfDFrAB(4, 96, info); break;
			case Instruction::ps_merge00:	Form_FrDAB(4, 528, false, info); break;
			case Instruction::ps_merge00_d:	Form_FrDAB(4, 528, true, info); break;
			case Instruction::ps_merge01:	Form_FrDAB(4, 560, false, info); break;
			case Instruction::ps_merge01_d:	Form_FrDAB(4, 560, true, info); break;
			case Instruction::ps_merge10:	Form_FrDAB(4, 592, false, info); break;
			case Instruction::ps_merge10_d:	Form_FrDAB(4, 592, true, info); break;
			case Instruction::ps_merge11:	Form_FrDAB(4, 624, false, info); break;
			case Instruction::ps_merge11_d:	Form_FrDAB(4, 624, true, info); break;

			default:
				throw "Unhandled instruction";
		}
	}

#pragma region "Quick helpers"

	uint32_t GekkoAssembler::add(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::add;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::add_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::add_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addo(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addo;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addo_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addo_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addc(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addc;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addc_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addc_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addco(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addco;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addco_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addco_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::adde(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::adde;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::adde_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::adde_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addeo(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addeo;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addeo_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addeo_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addi(size_t rd, size_t ra, int16_t simm)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addi;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Simm;
		info.Imm.Signed = simm;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addic(size_t rd, size_t ra, int16_t simm)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addic;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Simm;
		info.Imm.Signed = simm;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addic_d(size_t rd, size_t ra, int16_t simm)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addic_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Simm;
		info.Imm.Signed = simm;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addis(size_t rd, size_t ra, int16_t simm)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addis;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Simm;
		info.Imm.Signed = simm;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addme(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addme;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addme_d(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addme_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addmeo(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addmeo;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addmeo_d(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addmeo_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addze(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addze;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addze_d(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addze_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addzeo(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addzeo;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::addzeo_d(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::addzeo_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::_and(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::_and;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::and_d(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::and_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::andc(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::andc;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::andc_d(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::andc_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::andi_d(size_t ra, size_t rs, uint16_t uimm)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::andi_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Uimm;
		info.Imm.Unsigned = uimm;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::andis_d(size_t ra, size_t rs, uint16_t uimm)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::andis_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Uimm;
		info.Imm.Unsigned = uimm;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::b(uint32_t pc, uint32_t ta)
	{
		AnalyzeInfo info = { 0 };
		info.pc = pc;
		info.instr = Instruction::b;
		info.param[0] = Param::Address;
		info.Imm.Address = ta;
		info.numParam = 1;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ba(uint32_t pc, uint32_t ta)
	{
		AnalyzeInfo info = { 0 };
		info.pc = pc;
		info.instr = Instruction::ba;
		info.param[0] = Param::Address;
		info.Imm.Address = ta;
		info.numParam = 1;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::bl(uint32_t pc, uint32_t ta)
	{
		AnalyzeInfo info = { 0 };
		info.pc = pc;
		info.instr = Instruction::bl;
		info.param[0] = Param::Address;
		info.Imm.Address = ta;
		info.numParam = 1;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::bla(uint32_t pc, uint32_t ta)
	{
		AnalyzeInfo info = { 0 };
		info.pc = pc;
		info.instr = Instruction::bla;
		info.param[0] = Param::Address;
		info.Imm.Address = ta;
		info.numParam = 1;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::bc(uint32_t pc, size_t bo, size_t bi, uint32_t ta)
	{
		AnalyzeInfo info = { 0 };
		info.pc = pc;
		info.instr = Instruction::bc;
		info.param[0] = Param::Num;
		info.paramBits[0] = bo;
		info.param[1] = Param::Num;
		info.paramBits[1] = bi;
		info.param[2] = Param::Address;
		info.Imm.Address = ta;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::bca(uint32_t pc, size_t bo, size_t bi, uint32_t ta)
	{
		AnalyzeInfo info = { 0 };
		info.pc = pc;
		info.instr = Instruction::bca;
		info.param[0] = Param::Num;
		info.paramBits[0] = bo;
		info.param[1] = Param::Num;
		info.paramBits[1] = bi;
		info.param[2] = Param::Address;
		info.Imm.Address = ta;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::bcl(uint32_t pc, size_t bo, size_t bi, uint32_t ta)
	{
		AnalyzeInfo info = { 0 };
		info.pc = pc;
		info.instr = Instruction::bcl;
		info.param[0] = Param::Num;
		info.paramBits[0] = bo;
		info.param[1] = Param::Num;
		info.paramBits[1] = bi;
		info.param[2] = Param::Address;
		info.Imm.Address = ta;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::bcla(uint32_t pc, size_t bo, size_t bi, uint32_t ta)
	{
		AnalyzeInfo info = { 0 };
		info.pc = pc;
		info.instr = Instruction::bcla;
		info.param[0] = Param::Num;
		info.paramBits[0] = bo;
		info.param[1] = Param::Num;
		info.paramBits[1] = bi;
		info.param[2] = Param::Address;
		info.Imm.Address = ta;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::bcctr(size_t bo, size_t bi)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bcctr;
		info.param[0] = Param::Num;
		info.paramBits[0] = bo;
		info.param[1] = Param::Num;
		info.paramBits[1] = bi;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::bcctrl(size_t bo, size_t bi)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bcctrl;
		info.param[0] = Param::Num;
		info.paramBits[0] = bo;
		info.param[1] = Param::Num;
		info.paramBits[1] = bi;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::bclr(size_t bo, size_t bi)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bclr;
		info.param[0] = Param::Num;
		info.paramBits[0] = bo;
		info.param[1] = Param::Num;
		info.paramBits[1] = bi;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::bclrl(size_t bo, size_t bi)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bclrl;
		info.param[0] = Param::Num;
		info.paramBits[0] = bo;
		info.param[1] = Param::Num;
		info.paramBits[1] = bi;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::cmp(size_t crfd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cmp;
		info.param[0] = Param::Crf;
		info.paramBits[0] = crfd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::cmpi(size_t crfd, size_t ra, int16_t simm)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cmpi;
		info.param[0] = Param::Crf;
		info.paramBits[0] = crfd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Simm;
		info.Imm.Signed = simm;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::cmpl(size_t crfd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cmpl;
		info.param[0] = Param::Crf;
		info.paramBits[0] = crfd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::cmpli(size_t crfd, size_t ra, uint16_t uimm)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cmpli;
		info.param[0] = Param::Crf;
		info.paramBits[0] = crfd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Uimm;
		info.Imm.Signed = uimm;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::cntlzw(size_t ra, size_t rs)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cntlzw;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::cntlzw_d(size_t ra, size_t rs)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cntlzw_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::crand(size_t d, size_t a, size_t b)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::crand;
		info.param[0] = Param::Crb;
		info.paramBits[0] = d;
		info.param[1] = Param::Crb;
		info.paramBits[1] = a;
		info.param[2] = Param::Crb;
		info.paramBits[2] = b;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::crandc(size_t d, size_t a, size_t b)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::crandc;
		info.param[0] = Param::Crb;
		info.paramBits[0] = d;
		info.param[1] = Param::Crb;
		info.paramBits[1] = a;
		info.param[2] = Param::Crb;
		info.paramBits[2] = b;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::creqv(size_t d, size_t a, size_t b)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::creqv;
		info.param[0] = Param::Crb;
		info.paramBits[0] = d;
		info.param[1] = Param::Crb;
		info.paramBits[1] = a;
		info.param[2] = Param::Crb;
		info.paramBits[2] = b;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::crnand(size_t d, size_t a, size_t b)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::crnand;
		info.param[0] = Param::Crb;
		info.paramBits[0] = d;
		info.param[1] = Param::Crb;
		info.paramBits[1] = a;
		info.param[2] = Param::Crb;
		info.paramBits[2] = b;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::crnor(size_t d, size_t a, size_t b)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::crnor;
		info.param[0] = Param::Crb;
		info.paramBits[0] = d;
		info.param[1] = Param::Crb;
		info.paramBits[1] = a;
		info.param[2] = Param::Crb;
		info.paramBits[2] = b;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::cror(size_t d, size_t a, size_t b)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cror;
		info.param[0] = Param::Crb;
		info.paramBits[0] = d;
		info.param[1] = Param::Crb;
		info.paramBits[1] = a;
		info.param[2] = Param::Crb;
		info.paramBits[2] = b;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::crorc(size_t d, size_t a, size_t b)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::crorc;
		info.param[0] = Param::Crb;
		info.paramBits[0] = d;
		info.param[1] = Param::Crb;
		info.paramBits[1] = a;
		info.param[2] = Param::Crb;
		info.paramBits[2] = b;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::crxor(size_t d, size_t a, size_t b)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::crxor;
		info.param[0] = Param::Crb;
		info.paramBits[0] = d;
		info.param[1] = Param::Crb;
		info.paramBits[1] = a;
		info.param[2] = Param::Crb;
		info.paramBits[2] = b;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::dcbf(size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::dcbf;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::dcbi(size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::dcbi;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::dcbst(size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::dcbst;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::dcbt(size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::dcbt;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::dcbtst(size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::dcbtst;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::dcbz(size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::dcbz;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::dcbz_l(size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::dcbz_l;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::divw(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::divw;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::divw_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::divw_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::divwo(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::divwo;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::divwo_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::divwo_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::divwu(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::divwu;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::divwu_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::divwu_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::divwuo(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::divwuo;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::divwuo_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::divwuo_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::eciwx(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::eciwx;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ecowx(size_t rs, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ecowx;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::eieio()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::eieio;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::eqv(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::eqv;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::eqv_d(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::eqv_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::extsb(size_t ra, size_t rs)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::extsb;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::extsb_d(size_t ra, size_t rs)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::extsb_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::extsh(size_t ra, size_t rs)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::extsh;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::extsh_d(size_t ra, size_t rs)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::extsh_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fabs(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fabs;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fabs_d(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fabs_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fadd(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fadd;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fadd_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fadd_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fadds(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fadds;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fadds_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fadds_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fcmpo(size_t crfD, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmpo;
		info.param[0] = Param::Crf;
		info.paramBits[0] = crfD;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fcmpu(size_t crfD, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmpu;
		info.param[0] = Param::Crf;
		info.paramBits[0] = crfD;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fctiw(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fctiw;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fctiw_d(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fctiw_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fctiwz(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fctiwz;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fctiwz_d(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fctiwz_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fdiv(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdiv;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fdiv_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdiv_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fdivs(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdivs;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fdivs_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdivs_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fmadd(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmadd;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fmadd_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmadd_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fmadds(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmadds;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fmadds_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmadds_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fmr(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmr;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fmr_d(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmr_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fmsub(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmsub;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fmsub_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmsub_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fmsubs(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmsubs;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fmsubs_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmsubs_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fmul(size_t rd, size_t ra, size_t rc)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmul;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fmul_d(size_t rd, size_t ra, size_t rc)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmul_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fmuls(size_t rd, size_t ra, size_t rc)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmuls;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fmuls_d(size_t rd, size_t ra, size_t rc)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmuls_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fnabs(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnabs;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fnabs_d(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnabs_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fneg(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fneg;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fneg_d(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fneg_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fnmadd(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnmadd;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fnmadd_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnmadd_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fnmadds(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnmadds;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fnmadds_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnmadds_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fnmsub(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnmsub;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fnmsub_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnmsub_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fnmsubs(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnmsubs;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fnmsubs_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnmsubs_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fres(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fres;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fres_d(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fres_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::frsp(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::frsp;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::frsp_d(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::frsp_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::frsqrte(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::frsqrte;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::frsqrte_d(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::frsqrte_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fsel(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsel;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fsel_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsel_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fsub(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsub;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fsub_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsub_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fsubs(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsubs;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::fsubs_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsubs_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::icbi(size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::icbi;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::isync()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::isync;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lbz(size_t rd, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lbz;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lbzu(size_t rd, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lbzu;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lbzux(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lbzux;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lbzx(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lbzx;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lfd(size_t rd, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lfd;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lfdu(size_t rd, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lfdu;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lfdux(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lfdux;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lfdx(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lfdx;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lfs(size_t rd, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lfs;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lfsu(size_t rd, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lfsu;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lfsux(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lfsux;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lfsx(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lfsx;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lha(size_t rd, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lha;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lhau(size_t rd, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lhau;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lhaux(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lhaux;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lhax(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lhax;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lhbrx(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lhbrx;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lhz(size_t rd, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lhz;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lhzu(size_t rd, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lhzu;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lhzux(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lhzux;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lhzx(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lhzx;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lmw(size_t rd, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lmw;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lswi(size_t rd, size_t ra, size_t nb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lswi;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Num;
		info.paramBits[2] = nb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lswx(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lswx;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lwarx(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lwarx;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lwbrx(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lwbrx;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lwz(size_t rd, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lwz;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lwzu(size_t rd, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lwzu;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lwzux(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lwzux;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::lwzx(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lwzx;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mcrf(size_t d, size_t s)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mcrf;
		info.param[0] = Param::Crf;
		info.paramBits[0] = d;
		info.param[1] = Param::Crf;
		info.paramBits[1] = s;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mcrfs(size_t d, size_t s)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mcrfs;
		info.param[0] = Param::Crf;
		info.paramBits[0] = d;
		info.param[1] = Param::Crf;
		info.paramBits[1] = s;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mcrxr(size_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mcrxr;
		info.param[0] = Param::Crf;
		info.paramBits[0] = d;
		info.numParam = 1;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mfcr(size_t rd)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mfcr;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.numParam = 1;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mffs(size_t rd)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mffs;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.numParam = 1;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mffs_d(size_t rd)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mffs_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.numParam = 1;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mfmsr(size_t rd)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mfmsr;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.numParam = 1;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mfspr(size_t rd, size_t spr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mfspr;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Num;
		info.paramBits[1] = spr;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mfsr(size_t rd, size_t sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mfsr;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Num;
		info.paramBits[1] = sr;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mfsrin(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mfsrin;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mftb(size_t rd, size_t tbr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mftb;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Num;
		info.paramBits[1] = tbr;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mtcrf(size_t crm, size_t rs)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mtcrf;
		info.param[0] = Param::CRM;
		info.paramBits[0] = crm;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mtfsb0(size_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mtfsb0;
		info.param[0] = Param::Crb;
		info.paramBits[0] = d;
		info.numParam = 1;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mtfsb0_d(size_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mtfsb0_d;
		info.param[0] = Param::Crb;
		info.paramBits[0] = d;
		info.numParam = 1;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mtfsb1(size_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mtfsb1;
		info.param[0] = Param::Crb;
		info.paramBits[0] = d;
		info.numParam = 1;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mtfsb1_d(size_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mtfsb1_d;
		info.param[0] = Param::Crb;
		info.paramBits[0] = d;
		info.numParam = 1;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mtfsf(size_t fm, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mtfsf;
		info.param[0] = Param::FM;
		info.paramBits[0] = fm;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mtfsf_d(size_t fm, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mtfsf_d;
		info.param[0] = Param::FM;
		info.paramBits[0] = fm;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mtfsfi(size_t d, size_t imm)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mtfsfi;
		info.param[0] = Param::Crf;
		info.paramBits[0] = d;
		info.param[1] = Param::Num;
		info.paramBits[1] = imm;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mtfsfi_d(size_t d, size_t imm)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mtfsfi_d;
		info.param[0] = Param::Crf;
		info.paramBits[0] = d;
		info.param[1] = Param::Num;
		info.paramBits[1] = imm;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mtmsr(size_t rs)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mtmsr;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.numParam = 1;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mtspr(size_t spr, size_t rs)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mtspr;
		info.param[0] = Param::Num;
		info.paramBits[0] = spr;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mtsr(size_t sr, size_t rs)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mtsr;
		info.param[0] = Param::Num;
		info.paramBits[0] = sr;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mtsrin(size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mtsrin;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mulhw(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mulhw;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mulhw_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mulhw_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mulhwu(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mulhwu;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mulhwu_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mulhwu_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mulli(size_t rd, size_t ra, int16_t simm)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mulli;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Simm;
		info.Imm.Signed = simm;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mullw(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mullw;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mullw_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mullw_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mullwo(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mullwo;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::mullwo_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::mullwo_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::nand(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::nand;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::nand_d(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::nand_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::neg(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::neg;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::neg_d(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::neg_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::nego(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::nego;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::nego_d(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::nego_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::nor(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::nor;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::nor_d(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::nor_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::_or(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::_or;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::or_d(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::or_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::orc(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::orc;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::orc_d(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::orc_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ori(size_t ra, size_t rs, uint16_t uimm)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ori;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Uimm;
		info.Imm.Unsigned = uimm;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::oris(size_t ra, size_t rs, uint16_t uimm)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::oris;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Uimm;
		info.Imm.Unsigned = uimm;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::psq_l(size_t rd, size_t ra, uint16_t d, size_t w, size_t i)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::psq_l;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.param[2] = Param::Num;
		info.paramBits[2] = w;
		info.param[3] = Param::Num;
		info.paramBits[3] = i;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::psq_lu(size_t rd, size_t ra, uint16_t d, size_t w, size_t i)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::psq_lu;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.param[2] = Param::Num;
		info.paramBits[2] = w;
		info.param[3] = Param::Num;
		info.paramBits[3] = i;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::psq_lux(size_t rd, size_t ra, size_t rb, size_t w, size_t i)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::psq_lux;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.param[3] = Param::Num;
		info.paramBits[3] = w;
		info.param[4] = Param::Num;
		info.paramBits[4] = i;
		info.numParam = 5;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::psq_lx(size_t rd, size_t ra, size_t rb, size_t w, size_t i)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::psq_lx;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.param[3] = Param::Num;
		info.paramBits[3] = w;
		info.param[4] = Param::Num;
		info.paramBits[4] = i;
		info.numParam = 5;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::psq_st(size_t rs, size_t ra, uint16_t d, size_t w, size_t i)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::psq_st;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rs;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.param[2] = Param::Num;
		info.paramBits[2] = w;
		info.param[3] = Param::Num;
		info.paramBits[3] = i;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::psq_stu(size_t rs, size_t ra, uint16_t d, size_t w, size_t i)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::psq_stu;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rs;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.param[2] = Param::Num;
		info.paramBits[2] = w;
		info.param[3] = Param::Num;
		info.paramBits[3] = i;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::psq_stux(size_t rs, size_t ra, size_t rb, size_t w, size_t i)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::psq_stux;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.param[3] = Param::Num;
		info.paramBits[3] = w;
		info.param[4] = Param::Num;
		info.paramBits[4] = i;
		info.numParam = 5;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::psq_stx(size_t rs, size_t ra, size_t rb, size_t w, size_t i)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::psq_stx;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.param[3] = Param::Num;
		info.paramBits[3] = w;
		info.param[4] = Param::Num;
		info.paramBits[4] = i;
		info.numParam = 5;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_abs(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_abs;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_abs_d(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_abs_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_add(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_add;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_add_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_add_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_cmpo0(size_t crfd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_cmpo0;
		info.param[0] = Param::Crf;
		info.paramBits[0] = crfd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_cmpo1(size_t crfd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_cmpo1;
		info.param[0] = Param::Crf;
		info.paramBits[0] = crfd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_cmpu0(size_t crfd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_cmpu0;
		info.param[0] = Param::Crf;
		info.paramBits[0] = crfd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_cmpu1(size_t crfd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_cmpu1;
		info.param[0] = Param::Crf;
		info.paramBits[0] = crfd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_div(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_div;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_div_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_div_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_madd(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_madd;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_madd_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_madd_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_madds0(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_madds0;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_madds0_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_madds0_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_madds1(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_madds1;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_madds1_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_madds1_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_merge00(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_merge00;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_merge00_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_merge00_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_merge01(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_merge01;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_merge01_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_merge01_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_merge10(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_merge10;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_merge10_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_merge10_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_merge11(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_merge11;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_merge11_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_merge11_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_mr(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_mr;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_mr_d(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_mr_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_msub(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_msub;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_msub_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_msub_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_mul(size_t rd, size_t ra, size_t rc)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_mul;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_mul_d(size_t rd, size_t ra, size_t rc)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_mul_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_muls0(size_t rd, size_t ra, size_t rc)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_muls0;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_muls0_d(size_t rd, size_t ra, size_t rc)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_muls0_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_muls1(size_t rd, size_t ra, size_t rc)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_muls1;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_muls1_d(size_t rd, size_t ra, size_t rc)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_muls1_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_nabs(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_nabs;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_nabs_d(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_nabs_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_neg(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_neg;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_neg_d(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_neg_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_nmadd(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_nmadd;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_nmadd_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_nmadd_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_nmsub(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_nmsub;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_nmsub_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_nmsub_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_res(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_res;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_res_d(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_res_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_rsqrte(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_rsqrte;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_rsqrte_d(size_t rd, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_rsqrte_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = rb;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_sel(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_sel;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_sel_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_sel_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_sub(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_sub;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_sub_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_sub_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_sum0(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_sum0;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_sum0_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_sum0_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_sum1(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_sum1;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::ps_sum1_d(size_t rd, size_t ra, size_t rc, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ps_sum1_d;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rd;
		info.param[1] = Param::FReg;
		info.paramBits[1] = ra;
		info.param[2] = Param::FReg;
		info.paramBits[2] = rc;
		info.param[3] = Param::FReg;
		info.paramBits[3] = rb;
		info.numParam = 4;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::rfi()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rfi;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::rlwimi(size_t ra, size_t rs, size_t sh, size_t mb, size_t me)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rlwimi;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Num;
		info.paramBits[2] = sh;
		info.param[3] = Param::Num;
		info.paramBits[3] = mb;
		info.param[4] = Param::Num;
		info.paramBits[4] = me;
		info.numParam = 5;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::rlwimi_d(size_t ra, size_t rs, size_t sh, size_t mb, size_t me)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rlwimi_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Num;
		info.paramBits[2] = sh;
		info.param[3] = Param::Num;
		info.paramBits[3] = mb;
		info.param[4] = Param::Num;
		info.paramBits[4] = me;
		info.numParam = 5;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::rlwinm(size_t ra, size_t rs, size_t sh, size_t mb, size_t me)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rlwinm;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Num;
		info.paramBits[2] = sh;
		info.param[3] = Param::Num;
		info.paramBits[3] = mb;
		info.param[4] = Param::Num;
		info.paramBits[4] = me;
		info.numParam = 5;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::rlwinm_d(size_t ra, size_t rs, size_t sh, size_t mb, size_t me)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rlwinm_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Num;
		info.paramBits[2] = sh;
		info.param[3] = Param::Num;
		info.paramBits[3] = mb;
		info.param[4] = Param::Num;
		info.paramBits[4] = me;
		info.numParam = 5;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::rlwnm(size_t ra, size_t rs, size_t rb, size_t mb, size_t me)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rlwnm;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.param[3] = Param::Num;
		info.paramBits[3] = mb;
		info.param[4] = Param::Num;
		info.paramBits[4] = me;
		info.numParam = 5;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::rlwnm_d(size_t ra, size_t rs, size_t rb, size_t mb, size_t me)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rlwnm_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.param[3] = Param::Num;
		info.paramBits[3] = mb;
		info.param[4] = Param::Num;
		info.paramBits[4] = me;
		info.numParam = 5;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::sc()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sc;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::slw(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::slw;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::slw_d(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::slw_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::sraw(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sraw;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::sraw_d(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sraw_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::srawi(size_t ra, size_t rs, size_t sh)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::srawi;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Num;
		info.paramBits[2] = sh;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::srawi_d(size_t ra, size_t rs, size_t sh)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::srawi_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Num;
		info.paramBits[2] = sh;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::srw(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::srw;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::srw_d(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::srw_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stb(size_t rs, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stb;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stbu(size_t rs, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stbu;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stbux(size_t rs, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stbux;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stbx(size_t rs, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stbx;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stfd(size_t rs, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stfd;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rs;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stfdu(size_t rs, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stfdu;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rs;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stfdux(size_t rs, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stfdux;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stfdx(size_t rs, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stfdx;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stfiwx(size_t rs, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stfiwx;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stfs(size_t rs, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stfs;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rs;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stfsu(size_t rs, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stfsu;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rs;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stfsux(size_t rs, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stfsux;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stfsx(size_t rs, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stfsx;
		info.param[0] = Param::FReg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::sth(size_t rs, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sth;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::sthbrx(size_t rs, size_t ra, uint16_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sthbrx;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::sthu(size_t rs, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sthu;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::sthux(size_t rs, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sthux;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::sthx(size_t rs, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sthx;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stmw(size_t rs, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stmw;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stswi(size_t rs, size_t ra, uint16_t nb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stswi;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Num;
		info.paramBits[2] = nb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stswx(size_t rs, size_t ra, uint16_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stswx;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stw(size_t rs, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stw;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stwbrx(size_t rs, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stwbrx;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stwcx_d(size_t rs, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stwcx_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stwu(size_t rs, size_t ra, uint16_t d)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stwu;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::RegOffset;
		info.paramBits[1] = ra;
		info.Imm.Signed = d;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stwux(size_t rs, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stwux;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::stwx(size_t rs, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stwx;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rs;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subf(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subf;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subf_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subf_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfo(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfo;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfo_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfo_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfc(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfc;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfc_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfc_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfco(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfco;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfco_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfco_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfe(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfe;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfe_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfe_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfeo(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfeo;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfeo_d(size_t rd, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfeo_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfic(size_t rd, size_t ra, int16_t simm)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfic;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Simm;
		info.Imm.Signed = simm;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfme(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfme;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfme_d(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfme_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfmeo(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfmeo;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfmeo_d(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfmeo_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfze(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfze;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfze_d(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfze_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfzeo(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfzeo;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::subfzeo_d(size_t rd, size_t ra)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::subfzeo_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rd;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.numParam = 2;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::sync()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sync;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::tlbie(size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::tlbie;
		info.param[0] = Param::Reg;
		info.paramBits[0] = rb;
		info.numParam = 1;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::tlbsync()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::tlbsync;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::tw(size_t to, size_t ra, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::tw;
		info.param[0] = Param::Num;
		info.paramBits[0] = to;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::twi(size_t to, size_t ra, int16_t simm)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::twi;
		info.param[0] = Param::Num;
		info.paramBits[0] = to;
		info.param[1] = Param::Reg;
		info.paramBits[1] = ra;
		info.param[2] = Param::Simm;
		info.Imm.Signed = simm;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::_xor(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::_xor;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::xor_d(size_t ra, size_t rs, size_t rb)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::xor_d;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Reg;
		info.paramBits[2] = rb;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::xori(size_t ra, size_t rs, uint16_t uimm)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::xori;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Uimm;
		info.Imm.Unsigned = uimm;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

	uint32_t GekkoAssembler::xoris(size_t ra, size_t rs, uint16_t uimm)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::xoris;
		info.param[0] = Param::Reg;
		info.paramBits[0] = ra;
		info.param[1] = Param::Reg;
		info.paramBits[1] = rs;
		info.param[2] = Param::Uimm;
		info.Imm.Unsigned = uimm;
		info.numParam = 3;
		Assemble(info);
		return info.instrBits;
	}

#pragma endregion "Quick helpers"

}
