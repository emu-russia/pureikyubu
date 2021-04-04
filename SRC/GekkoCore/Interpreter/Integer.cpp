// Integer Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

	// rd = (ra | 0) + SIMM
	OP(ADDI)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::addi]++;
		}

		if (RA) RRD = RRA + SIMM;
		else RRD = SIMM;
		Gekko->regs.pc += 4;
	}

	// rd = (ra | 0) + (SIMM || 0x0000)
	OP(ADDIS)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::addis]++;
		}

		if (RA) RRD = RRA + (SIMM << 16);
		else RRD = SIMM << 16;
		Gekko->regs.pc += 4;
	}

	// rd = ra + rb
	OP(ADD)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::add]++;
		}

		RRD = RRA + RRB;
		Gekko->regs.pc += 4;
	}

	// rd = ra + rb, CR0
	OP(ADDD)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::add_d]++;
		}

		uint32_t res = RRA + RRB;
		RRD = res;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// rd = ra + rb, XER
	OP(ADDO)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::addo]++;
		}

		uint32_t a = RRA, b = RRB, res;
		bool ovf = false;

		res = AddOverflow(a, b);
		ovf = OverflowBit != 0;

		RRD = res;
		if (ovf)
		{
			SET_XER_OV;
			SET_XER_SO;
		}
		else RESET_XER_OV;
		Gekko->regs.pc += 4;
	}

	// rd = ra + rb, CR0, XER
	OP(ADDOD)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::addo_d]++;
		}

		uint32_t a = RRA, b = RRB, res;
		bool ovf = false;

		res = AddOverflow(a, b);
		ovf = OverflowBit != 0;

		RRD = res;
		if (ovf)
		{
			SET_XER_OV;
			SET_XER_SO;
		}
		else RESET_XER_OV;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + rb + 1
	OP(SUBF)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::subf]++;
		}

		RRD = ~RRA + RRB + 1;
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + rb + 1, CR0
	OP(SUBFD)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::subf_d]++;
		}

		uint32_t res = ~RRA + RRB + 1;
		RRD = res;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + rb + 1, XER
	OP(SUBFO)
	{
		Debug::Halt("SUBFO\n");
	}

	// rd = ~ra + rb + 1, CR0, XER
	OP(SUBFOD)
	{
		Debug::Halt("SUBFOD\n");
	}

	// rd = ra + SIMM, XER
	OP(ADDIC)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::addic]++;
		}

		uint32_t a = RRA, b = SIMM, res;
		bool carry = false;

		res = AddCarry(a, b);
		carry = CarryBit != 0;

		RRD = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
		Gekko->regs.pc += 4;
	}

	// rd = ra + SIMM, CR0, XER
	OP(ADDICD)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::addic_d]++;
		}

		uint32_t a = RRA, b = SIMM, res;
		bool carry = false;

		res = AddCarry(a, b);
		carry = CarryBit != 0;

		RRD = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// rd = ~RRA + SIMM + 1, XER
	OP(SUBFIC)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::subfic]++;
		}

		uint32_t a = ~RRA, b = SIMM + 1, res;
		bool carry = false;

		res = AddCarry(a, b);
		carry = CarryBit != 0;

		RRD = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
		Gekko->regs.pc += 4;
	}

	// rd = ra + rb, XER[CA]
	OP(ADDC)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::addc]++;
		}

		uint32_t a = RRA, b = RRB, res;
		bool carry = false;

		res = AddCarry(a, b);
		carry = CarryBit != 0;

		RRD = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
		Gekko->regs.pc += 4;
	}

	// rd = ra + rb, XER[CA], CR0
	OP(ADDCD)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::addc_d]++;
		}

		uint32_t a = RRA, b = RRB, res;
		bool carry = false;

		res = AddCarry(a, b);
		carry = CarryBit != 0;

		RRD = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// rd = ra + rb, XER[CA], XER[OV]
	OP(ADDCO)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::addco]++;
		}

		uint32_t a = RRA, b = RRB, res;
		bool carry = false, ovf = false;

		res = AddCarryOverflow(a, b);
		carry = CarryBit != 0;
		ovf = OverflowBit != 0;

		RRD = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
		if (ovf)
		{
			SET_XER_OV;
			SET_XER_SO;
		}
		else RESET_XER_OV;
		Gekko->regs.pc += 4;
	}

	// rd = ra + rb, XER[CA], XER[OV], CR0
	OP(ADDCOD)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::addco_d]++;
		}

		uint32_t a = RRA, b = RRB, res;
		bool carry = false, ovf = false;

		res = AddCarryOverflow(a, b);
		carry = CarryBit != 0;
		ovf = OverflowBit != 0;

		RRD = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
		if (ovf)
		{
			SET_XER_OV;
			SET_XER_SO;
		}
		else RESET_XER_OV;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + rb + 1, XER[CA]
	OP(SUBFC)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::subfc]++;
		}

		uint32_t a = ~RRA, b = RRB + 1, res;
		bool carry = false;

		res = AddCarry(a, b);
		carry = CarryBit != 0;

		if (carry) SET_XER_CA; else RESET_XER_CA;
		RRD = res;
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + rb + 1, XER[CA], CR0
	OP(SUBFCD)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::subfc_d]++;
		}

		uint32_t a = ~RRA, b = RRB + 1, res;
		bool carry = false;

		res = AddCarry(a, b);
		carry = CarryBit != 0;

		if (carry) SET_XER_CA; else RESET_XER_CA;
		RRD = res;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// ---------------------------------------------------------------------------

	static void ADDXER(uint32_t a, uint32_t op)
	{
		uint32_t res;
		uint32_t c = (IS_XER_CA) ? 1 : 0;
		bool carry = false;

		res = AddCarry(a, c);
		carry = CarryBit != 0;

		RRD = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
	}

	static void ADDXERD(uint32_t a, uint32_t op)
	{
		uint32_t res;
		uint32_t c = (IS_XER_CA) ? 1 : 0;
		bool carry = false;

		res = AddCarry(a, c);
		carry = CarryBit != 0;

		RRD = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
		COMPUTE_CR0(res);
	}

	static void ADDXER2(uint32_t a, uint32_t b, uint32_t op)
	{
		uint32_t res;
		uint32_t c = (IS_XER_CA) ? 1 : 0;
		bool carry = false;

		CarryBit = c;
		res = AddXer2(a, b);
		carry = CarryBit != 0;

		RRD = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
	}

	static void ADDXER2D(uint32_t a, uint32_t b, uint32_t op)
	{
		uint32_t res;
		uint32_t c = (IS_XER_CA) ? 1 : 0;
		bool carry = false;

		CarryBit = c;
		res = AddXer2(a, b);
		carry = CarryBit != 0;

		RRD = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
		COMPUTE_CR0(res);
	}

	// rd = ra + rb + XER[CA], XER
	OP(ADDE)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::adde]++;
		}

		ADDXER2(RRA, RRB, op);
		Gekko->regs.pc += 4;
	}

	// rd = ra + rb + XER[CA], CR0, XER
	OP(ADDED)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::adde_d]++;
		}

		ADDXER2D(RRA, RRB, op);
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + rb + XER[CA], XER
	OP(SUBFE)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::subfe]++;
		}

		ADDXER2(~RRA, RRB, op);
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + rb + XER[CA], CR0, XER
	OP(SUBFED)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::subfe_d]++;
		}

		ADDXER2D(~RRA, RRB, op);
		Gekko->regs.pc += 4;
	}

	// rd = ra + XER[CA] - 1 (0xffffffff), XER
	OP(ADDME)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::addme]++;
		}

		ADDXER(RRA - 1, op);
		Gekko->regs.pc += 4;
	}

	// rd = ra + XER[CA] - 1 (0xffffffff), CR0, XER
	OP(ADDMED)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::addme_d]++;
		}

		ADDXERD(RRA - 1, op);
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + XER[CA] - 1, XER
	OP(SUBFME)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::subfme]++;
		}

		ADDXER(~RRA - 1, op);
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + XER[CA] - 1, CR0, XER
	OP(SUBFMED)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::subfme_d]++;
		}

		ADDXERD(~RRA - 1, op);
		Gekko->regs.pc += 4;
	}

	// rd = ra + XER[CA], XER
	OP(ADDZE)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::addze]++;
		}

		ADDXER(RRA, op);
		Gekko->regs.pc += 4;
	}

	// rd = ra + XER[CA], CR0, XER
	OP(ADDZED)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::addze_d]++;
		}

		ADDXERD(RRA, op);
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + XER[CA], XER
	OP(SUBFZE)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::subfze]++;
		}

		ADDXER(~RRA, op);
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + XER[CA], CR0, XER
	OP(SUBFZED)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::subfze_d]++;
		}

		ADDXERD(~RRA, op);
		Gekko->regs.pc += 4;
	}

	// ---------------------------------------------------------------------------

	// rd = ~ra + 1
	OP(NEG)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::neg]++;
		}

		RRD = ~RRA + 1;
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + 1, CR0
	OP(NEGD)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::neg_d]++;
		}

		uint32_t res = ~RRA + 1;
		RRD = res;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// ---------------------------------------------------------------------------

	// prod[0-48] = ra * SIMM
	// rd = prod[16-48]
	OP(MULLI)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::mulli]++;
		}

		RRD = RRA * SIMM;
		Gekko->regs.pc += 4;
	}

	// prod[0-48] = ra * rb
	// rd = prod[16-48]
	OP(MULLW)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::mullw]++;
		}

		int32_t a = RRA, b = RRB;
		int64_t res = (int64_t)a * (int64_t)b;
		RRD = (int32_t)(res & 0x00000000ffffffff);
		Gekko->regs.pc += 4;
	}

	// prod[0-48] = ra * rb
	// rd = prod[16-48]
	// CR0
	OP(MULLWD)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::mullw_d]++;
		}

		int32_t a = RRA, b = RRB;
		int64_t res = (int64_t)a * (int64_t)b;
		RRD = (int32_t)(res & 0x00000000ffffffff);
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// prod[0-63] = ra * rb
	// rd = prod[0-31]
	OP(MULHW)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::mulhw]++;
		}

		int64_t a = (int32_t)RRA, b = (int32_t)RRB, res = a * b;
		res = (res >> 32);
		RRD = (int32_t)res;
		Gekko->regs.pc += 4;
	}

	// prod[0-63] = ra * rb
	// rd = prod[0-31]
	// CR0
	OP(MULHWD)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::mulhw_d]++;
		}

		int64_t a = (int32_t)RRA, b = (int32_t)RRB, res = a * b;
		res = (res >> 32);
		RRD = (int32_t)res;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// prod[0-63] = ra * rb
	// rd = prod[0-31]
	OP(MULHWU)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::mulhwu]++;
		}

		uint64_t a = RRA, b = RRB, res = a * b;
		res = (res >> 32);
		RRD = (uint32_t)res;
		Gekko->regs.pc += 4;
	}

	// prod[0-63] = ra * rb
	// rd = prod[0-31]
	// CR0
	OP(MULHWUD)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::mulhwu_d]++;
		}

		uint64_t a = RRA, b = RRB, res = a * b;
		res = (res >> 32);
		RRD = (uint32_t)res;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// rd = ra / rb (signed)
	OP(DIVW)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::divw]++;
		}

		int32_t a = RRA, b = RRB;
		if (b) RRD = a / b;
		Gekko->regs.pc += 4;
	}

	// rd = ra / rb (signed), CR0
	OP(DIVWD)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::divw_d]++;
		}

		int32_t a = RRA, b = RRB, res;
		if (b)
		{
			res = a / b;
			RRD = res;
			COMPUTE_CR0(res);
		}
		Gekko->regs.pc += 4;
	}

	// rd = ra / rb (unsigned)
	OP(DIVWU)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::divwu]++;
		}

		uint32_t a = RRA, b = RRB;
		if (b) RRD = a / b;
		Gekko->regs.pc += 4;
	}

	// rd = ra / rb (unsigned), CR0
	OP(DIVWUD)
	{
		if (Gekko->opcodeStatsEnabled)
		{
			Gekko->opcodeStats[(size_t)Gekko::Instruction::divwu_d]++;
		}

		uint32_t a = RRA, b = RRB, res;
		if (b)
		{
			res = a / b;
			RRD = res;
			COMPUTE_CR0(res);
		}
		Gekko->regs.pc += 4;
	}

}
