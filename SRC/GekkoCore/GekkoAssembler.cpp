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
		PackBits(res, 26, 30, extended);

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
			case Instruction::eqv:			Form_ASB(31, 284, false, info); break;		// Typo in GUM (B operand)
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
			// ...

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
			case Instruction::lwzux:		Form_DAB(31, 66, false, false, info); break;
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

			// Trap Instructions

			// Processor Control Instructions

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

			// Lookaside Buffer Management Instructions

			// External Control Instructions

			case Instruction::eciwx:		Form_DAB(31, 310, false, false, info); break;
			case Instruction::ecowx:		Form_SAB(31, 438, false, false, info); break;

			// Paired-Single Load and Store Instructions

			// Paired-Single Floating Point Arithmetic Instructions

			// Miscellaneous Paired-Single Instructions

		}
	}
}
