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

		oe ? SetBit(res, 21) : 0;
		rc ? SetBit(res, 31) : 0;

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

		oe ? SetBit(res, 21) : 0;
		rc ? SetBit(res, 31) : 0;

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

		rc ? SetBit(res, 31) : 0;

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

		aa ? SetBit(res, 30) : 0;
		lk ? SetBit(res, 31) : 0;

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

		aa ? SetBit(res, 30) : 0;
		lk ? SetBit(res, 31) : 0;

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

		lk ? SetBit(res, 31) : 0;

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

		rc ? SetBit(res, 31) : 0;

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

		rc ? SetBit(res, 31) : 0;

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

		rc ? SetBit(res, 31) : 0;

		info.instrBits = res;
	}

	void GekkoAssembler::Assemble(AnalyzeInfo& info)
	{
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

			// Floating-Point Arithmetic Instructions

			// Floating-Point Multiply-Add Instructions

			// Floating-Point Rounding and Conversion Instructions

			// Floating-Point Compare Instructions

			// Floating-Point Status and Control Register Instructions

			// Integer Load Instructions

			// Integer Store Instructions

			// Integer Load and Store with Byte Reverse Instructions

			// Integer Load and Store Multiple Instructions

			// Integer Load and Store String Instructions

			// Memory Synchronization Instructions

			case Instruction::eieio:		Form_Extended(31, 854, info); break;
			case Instruction::isync:		Form_Extended(19, 150, info); break;
			case Instruction::lwarx:		Form_DAB(31, 20, false, false, info); break;
			case Instruction::stwcx_d:		Form_SAB(31, 150, false, true, info); break;
			case Instruction::sync:			Form_Extended(31, 598, info); break;

			// Floating-Point Load Instructions

			// Floating-Point Store Instructions

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
