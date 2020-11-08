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

		if (ppc_bit_to <= ppc_bit_from)
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

		res = (res & ~mask) | (val << (31 - ppc_bit_from));
	}

	void GekkoAssembler::SetBit(uint32_t& res, size_t ppc_bit)
	{
		if (ppc_bit > 31)
		{
			throw "Bit out of range";
		}
		res |= 1 << (31 - ppc_bit);
	}

	void GekkoAssembler::Form_XO(size_t primary, size_t extended, bool oe, bool rc, AnalyzeInfo& info)
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

	void GekkoAssembler::Form_D(size_t primary, AnalyzeInfo& info)
	{
		uint32_t res = 0;

		PackBits(res, 0, 5, primary);

		CheckParam(info, 0, Param::Reg);
		CheckParam(info, 1, Param::Reg);
		CheckParam(info, 2, Param::Simm);

		PackBits(res, 6, 10, info.paramBits[0]);
		PackBits(res, 11, 15, info.paramBits[1]);
		PackBits(res, 16, 31, info.Imm.Signed);

		info.instrBits = res;
	}

	void GekkoAssembler::Assemble(AnalyzeInfo& info)
	{
		// The format of Gekko (PowerPC) instructions is a bit similar to MIPS, only about 350 instructions.
		// Just go through all the instructions from the GUM (google: "IBM Gekko User's Manual pdf")

		switch (info.instr)
		{
			case Instruction::add:		Form_XO(31, 266, false, false, info); break;
			case Instruction::add_d:	Form_XO(31, 266, false, true, info); break;
			case Instruction::addo:		Form_XO(31, 266, true, false, info); break;
			case Instruction::addo_d:	Form_XO(31, 266, true, true, info); break;

			case Instruction::addc:		Form_XO(31, 10, false, false, info); break;
			case Instruction::addc_d:	Form_XO(31, 10, false, true, info); break;
			case Instruction::addco:	Form_XO(31, 10, true, false, info); break;
			case Instruction::addco_d:	Form_XO(31, 10, true, true, info); break;

			case Instruction::adde:		Form_XO(31, 138, false, false, info); break;
			case Instruction::adde_d:	Form_XO(31, 138, false, true, info); break;
			case Instruction::addeo:	Form_XO(31, 138, true, false, info); break;
			case Instruction::addeo_d:	Form_XO(31, 138, true, true, info); break;

			case Instruction::addi:		Form_D(14, info); break;
			case Instruction::addic:	Form_D(12, info); break;
			case Instruction::addic_d:	Form_D(13, info); break;
			case Instruction::addis:	Form_D(15, info); break;
		}
	}
}
