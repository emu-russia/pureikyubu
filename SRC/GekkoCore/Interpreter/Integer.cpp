// Integer Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

#pragma region "ALU Helpers"

	void Interpreter::ADDXER(uint32_t a, AnalyzeInfo& info)
	{
		uint32_t res;
		uint32_t c = (IS_XER_CA) ? 1 : 0;
		bool carry = false;

		res = AddCarry(a, c);
		carry = CarryBit != 0;

		core->regs.gpr[info.paramBits[0]] = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
	}

	void Interpreter::ADDXERD(uint32_t a, AnalyzeInfo& info)
	{
		uint32_t res;
		uint32_t c = (IS_XER_CA) ? 1 : 0;
		bool carry = false;

		res = AddCarry(a, c);
		carry = CarryBit != 0;

		core->regs.gpr[info.paramBits[0]] = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
		COMPUTE_CR0(res);
	}

	void Interpreter::ADDXER2(uint32_t a, uint32_t b, AnalyzeInfo& info)
	{
		uint32_t res;
		uint32_t c = (IS_XER_CA) ? 1 : 0;
		bool carry = false;

		CarryBit = c;
		res = AddXer2(a, b);
		carry = CarryBit != 0;

		core->regs.gpr[info.paramBits[0]] = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
	}

	void Interpreter::ADDXER2D(uint32_t a, uint32_t b, AnalyzeInfo& info)
	{
		uint32_t res;
		uint32_t c = (IS_XER_CA) ? 1 : 0;
		bool carry = false;

		CarryBit = c;
		res = AddXer2(a, b);
		carry = CarryBit != 0;

		core->regs.gpr[info.paramBits[0]] = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
		COMPUTE_CR0(res);
	}

#pragma endregion "ALU Helpers"

	// rd = ra + rb
	void Interpreter::add(AnalyzeInfo& info)
	{
		core->regs.gpr[info.paramBits[0]] = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
		Gekko->regs.pc += 4;
	}

	// rd = ra + rb, CR0
	void Interpreter::add_d(AnalyzeInfo& info)
	{
		uint32_t res = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
		core->regs.gpr[info.paramBits[0]] = res;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// rd = ra + rb, XER
	void Interpreter::addo(AnalyzeInfo& info)
	{
		uint32_t a = core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]], res;
		bool ovf = false;

		res = AddOverflow(a, b);
		ovf = OverflowBit != 0;

		core->regs.gpr[info.paramBits[0]] = res;
		if (ovf)
		{
			SET_XER_OV;
			SET_XER_SO;
		}
		else RESET_XER_OV;
		Gekko->regs.pc += 4;
	}

	// rd = ra + rb, CR0, XER
	void Interpreter::addo_d(AnalyzeInfo& info)
	{
		uint32_t a = core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]], res;
		bool ovf = false;

		res = AddOverflow(a, b);
		ovf = OverflowBit != 0;

		core->regs.gpr[info.paramBits[0]] = res;
		if (ovf)
		{
			SET_XER_OV;
			SET_XER_SO;
		}
		else RESET_XER_OV;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// rd = ra + rb, XER[CA]
	void Interpreter::addc(AnalyzeInfo& info)
	{
		uint32_t a = core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]], res;
		bool carry = false;

		res = AddCarry(a, b);
		carry = CarryBit != 0;

		core->regs.gpr[info.paramBits[0]] = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
		Gekko->regs.pc += 4;
	}

	// rd = ra + rb, XER[CA], CR0
	void Interpreter::addc_d(AnalyzeInfo& info)
	{
		uint32_t a = core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]], res;
		bool carry = false;

		res = AddCarry(a, b);
		carry = CarryBit != 0;

		core->regs.gpr[info.paramBits[0]] = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// rd = ra + rb, XER[CA], XER[OV]
	void Interpreter::addco(AnalyzeInfo& info)
	{
		uint32_t a = core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]], res;
		bool carry = false, ovf = false;

		res = AddCarryOverflow(a, b);
		carry = CarryBit != 0;
		ovf = OverflowBit != 0;

		core->regs.gpr[info.paramBits[0]] = res;
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
	void Interpreter::addco_d(AnalyzeInfo& info)
	{
		uint32_t a = core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]], res;
		bool carry = false, ovf = false;

		res = AddCarryOverflow(a, b);
		carry = CarryBit != 0;
		ovf = OverflowBit != 0;

		core->regs.gpr[info.paramBits[0]] = res;
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

	// rd = ra + rb + XER[CA], XER
	void Interpreter::adde(AnalyzeInfo& info)
	{
		ADDXER2(core->regs.gpr[info.paramBits[1]], core->regs.gpr[info.paramBits[2]], info);
		Gekko->regs.pc += 4;
	}

	// rd = ra + rb + XER[CA], CR0, XER
	void Interpreter::adde_d(AnalyzeInfo& info)
	{
		ADDXER2D(core->regs.gpr[info.paramBits[1]], core->regs.gpr[info.paramBits[2]], info);
		Gekko->regs.pc += 4;
	}

	void Interpreter::addeo(AnalyzeInfo& info)
	{
		Debug::Halt("addeo\n");
	}

	void Interpreter::addeo_d(AnalyzeInfo& info)
	{
		Debug::Halt("addeo.\n");
	}

	// rd = (ra | 0) + SIMM
	void Interpreter::addi(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->regs.gpr[info.paramBits[0]] = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		else core->regs.gpr[info.paramBits[0]] = (int32_t)info.Imm.Signed;
		Gekko->regs.pc += 4;
	}

	// rd = ra + SIMM, XER
	void Interpreter::addic(AnalyzeInfo& info)
	{
		uint32_t a = core->regs.gpr[info.paramBits[1]], b = (int32_t)info.Imm.Signed, res;
		bool carry = false;

		res = AddCarry(a, b);
		carry = CarryBit != 0;

		core->regs.gpr[info.paramBits[0]] = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
		Gekko->regs.pc += 4;
	}

	// rd = ra + SIMM, CR0, XER
	void Interpreter::addic_d(AnalyzeInfo& info)
	{
		uint32_t a = core->regs.gpr[info.paramBits[1]], b = (int32_t)info.Imm.Signed, res;
		bool carry = false;

		res = AddCarry(a, b);
		carry = CarryBit != 0;

		core->regs.gpr[info.paramBits[0]] = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// rd = (ra | 0) + (SIMM || 0x0000)
	void Interpreter::addis(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->regs.gpr[info.paramBits[0]] = core->regs.gpr[info.paramBits[1]] + ((int32_t)info.Imm.Signed << 16);
		else core->regs.gpr[info.paramBits[0]] = (int32_t)info.Imm.Signed << 16;
		Gekko->regs.pc += 4;
	}

	// rd = ra + XER[CA] - 1 (0xffffffff), XER
	void Interpreter::addme(AnalyzeInfo& info)
	{
		ADDXER(core->regs.gpr[info.paramBits[1]] - 1, info);
		Gekko->regs.pc += 4;
	}

	// rd = ra + XER[CA] - 1 (0xffffffff), CR0, XER
	void Interpreter::addme_d(AnalyzeInfo& info)
	{
		ADDXERD(core->regs.gpr[info.paramBits[1]] - 1, info);
		Gekko->regs.pc += 4;
	}

	void Interpreter::addmeo(AnalyzeInfo& info)
	{
		Debug::Halt("addmeo\n");
	}

	void Interpreter::addmeo_d(AnalyzeInfo& info)
	{
		Debug::Halt("addmeo.\n");
	}

	// rd = ra + XER[CA], XER
	void Interpreter::addze(AnalyzeInfo& info)
	{
		ADDXER(core->regs.gpr[info.paramBits[1]], info);
		Gekko->regs.pc += 4;
	}

	// rd = ra + XER[CA], CR0, XER
	void Interpreter::addze_d(AnalyzeInfo& info)
	{
		ADDXERD(core->regs.gpr[info.paramBits[1]], info);
		Gekko->regs.pc += 4;
	}

	void Interpreter::addzeo(AnalyzeInfo& info)
	{
		Debug::Halt("addzeo\n");
	}

	void Interpreter::addzeo_d(AnalyzeInfo& info)
	{
		Debug::Halt("addzeo.\n");
	}

	// rd = ra / rb (signed)
	void Interpreter::divw(AnalyzeInfo& info)
	{
		int32_t a = core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]];
		if (b) core->regs.gpr[info.paramBits[0]] = a / b;
		Gekko->regs.pc += 4;
	}

	// rd = ra / rb (signed), CR0
	void Interpreter::divw_d(AnalyzeInfo& info)
	{
		int32_t a = core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]], res;
		if (b)
		{
			res = a / b;
			core->regs.gpr[info.paramBits[0]] = res;
			COMPUTE_CR0(res);
		}
		Gekko->regs.pc += 4;
	}

	void Interpreter::divwo(AnalyzeInfo& info)
	{
		Debug::Halt("divwo\n");
	}

	void Interpreter::divwo_d(AnalyzeInfo& info)
	{
		Debug::Halt("divwo.\n");
	}

	// rd = ra / rb (unsigned)
	void Interpreter::divwu(AnalyzeInfo& info)
	{
		uint32_t a = core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]];
		if (b) core->regs.gpr[info.paramBits[0]] = a / b;
		Gekko->regs.pc += 4;
	}

	// rd = ra / rb (unsigned), CR0
	void Interpreter::divwu_d(AnalyzeInfo& info)
	{
		uint32_t a = core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]], res;
		if (b)
		{
			res = a / b;
			core->regs.gpr[info.paramBits[0]] = res;
			COMPUTE_CR0(res);
		}
		Gekko->regs.pc += 4;
	}

	void Interpreter::divwuo(AnalyzeInfo& info)
	{
		Debug::Halt("divwuo\n");
	}

	void Interpreter::divwuo_d(AnalyzeInfo& info)
	{
		Debug::Halt("divwuo.\n");
	}

	// prod[0-63] = ra * rb
	// rd = prod[0-31]
	void Interpreter::mulhw(AnalyzeInfo& info)
	{
		int64_t a = (int32_t)core->regs.gpr[info.paramBits[1]], b = (int32_t)core->regs.gpr[info.paramBits[2]], res = a * b;
		res = (res >> 32);
		core->regs.gpr[info.paramBits[0]] = (int32_t)res;
		Gekko->regs.pc += 4;
	}

	// prod[0-63] = ra * rb
	// rd = prod[0-31]
	// CR0
	void Interpreter::mulhw_d(AnalyzeInfo& info)
	{
		int64_t a = (int32_t)core->regs.gpr[info.paramBits[1]], b = (int32_t)core->regs.gpr[info.paramBits[2]], res = a * b;
		res = (res >> 32);
		core->regs.gpr[info.paramBits[0]] = (int32_t)res;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// prod[0-63] = ra * rb
	// rd = prod[0-31]
	void Interpreter::mulhwu(AnalyzeInfo& info)
	{
		uint64_t a = core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]], res = a * b;
		res = (res >> 32);
		core->regs.gpr[info.paramBits[0]] = (uint32_t)res;
		Gekko->regs.pc += 4;
	}

	// prod[0-63] = ra * rb
	// rd = prod[0-31]
	// CR0
	void Interpreter::mulhwu_d(AnalyzeInfo& info)
	{
		uint64_t a = core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]], res = a * b;
		res = (res >> 32);
		core->regs.gpr[info.paramBits[0]] = (uint32_t)res;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// prod[0-48] = ra * SIMM
	// rd = prod[16-48]
	void Interpreter::mulli(AnalyzeInfo& info)
	{
		core->regs.gpr[info.paramBits[0]] = core->regs.gpr[info.paramBits[1]] * (int32_t)info.Imm.Signed;
		Gekko->regs.pc += 4;
	}

	// prod[0-48] = ra * rb
	// rd = prod[16-48]
	void Interpreter::mullw(AnalyzeInfo& info)
	{
		int32_t a = core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]];
		int64_t res = (int64_t)a * (int64_t)b;
		core->regs.gpr[info.paramBits[0]] = (int32_t)(res & 0x00000000ffffffff);
		Gekko->regs.pc += 4;
	}

	// prod[0-48] = ra * rb
	// rd = prod[16-48]
	// CR0
	void Interpreter::mullw_d(AnalyzeInfo& info)
	{
		int32_t a = core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]];
		int64_t res = (int64_t)a * (int64_t)b;
		core->regs.gpr[info.paramBits[0]] = (int32_t)(res & 0x00000000ffffffff);
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	void Interpreter::mullwo(AnalyzeInfo& info)
	{
		Debug::Halt("mullwo\n");
	}

	void Interpreter::mullwo_d(AnalyzeInfo& info)
	{
		Debug::Halt("mullwo.\n");
	}

	// rd = ~ra + 1
	void Interpreter::neg(AnalyzeInfo& info)
	{
		core->regs.gpr[info.paramBits[0]] = ~core->regs.gpr[info.paramBits[1]] + 1;
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + 1, CR0
	void Interpreter::neg_d(AnalyzeInfo& info)
	{
		uint32_t res = ~core->regs.gpr[info.paramBits[1]] + 1;
		core->regs.gpr[info.paramBits[0]] = res;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	void Interpreter::nego(AnalyzeInfo& info)
	{
		Debug::Halt("nego\n");
	}

	void Interpreter::nego_d(AnalyzeInfo& info)
	{
		Debug::Halt("nego.\n");
	}

	// rd = ~ra + rb + 1
	void Interpreter::subf(AnalyzeInfo& info)
	{
		core->regs.gpr[info.paramBits[0]] = ~core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]] + 1;
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + rb + 1, CR0
	void Interpreter::subf_d(AnalyzeInfo& info)
	{
		uint32_t res = ~core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]] + 1;
		core->regs.gpr[info.paramBits[0]] = res;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + rb + 1, XER
	void Interpreter::subfo(AnalyzeInfo& info)
	{
		Debug::Halt("subfo\n");
	}

	// rd = ~ra + rb + 1, CR0, XER
	void Interpreter::subfo_d(AnalyzeInfo& info)
	{
		Debug::Halt("subfo.\n");
	}

	// rd = ~ra + rb + 1, XER[CA]
	void Interpreter::subfc(AnalyzeInfo& info)
	{
		uint32_t a = ~core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]] + 1, res;
		bool carry = false;

		res = AddCarry(a, b);
		carry = CarryBit != 0;

		if (carry) SET_XER_CA; else RESET_XER_CA;
		core->regs.gpr[info.paramBits[0]] = res;
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + rb + 1, XER[CA], CR0
	void Interpreter::subfc_d(AnalyzeInfo& info)
	{
		uint32_t a = ~core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]] + 1, res;
		bool carry = false;

		res = AddCarry(a, b);
		carry = CarryBit != 0;

		if (carry) SET_XER_CA; else RESET_XER_CA;
		core->regs.gpr[info.paramBits[0]] = res;
		COMPUTE_CR0(res);
		Gekko->regs.pc += 4;
	}

	void Interpreter::subfco(AnalyzeInfo& info)
	{
		Debug::Halt("subfco\n");
	}

	void Interpreter::subfco_d(AnalyzeInfo& info)
	{
		Debug::Halt("subfco.\n");
	}

	// rd = ~ra + rb + XER[CA], XER
	void Interpreter::subfe(AnalyzeInfo& info)
	{
		ADDXER2(~core->regs.gpr[info.paramBits[1]], core->regs.gpr[info.paramBits[2]], info);
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + rb + XER[CA], CR0, XER
	void Interpreter::subfe_d(AnalyzeInfo& info)
	{
		ADDXER2D(~core->regs.gpr[info.paramBits[1]], core->regs.gpr[info.paramBits[2]], info);
		Gekko->regs.pc += 4;
	}

	void Interpreter::subfeo(AnalyzeInfo& info)
	{
		Debug::Halt("subfeo\n");
	}

	void Interpreter::subfeo_d(AnalyzeInfo& info)
	{
		Debug::Halt("subfeo.\n");
	}

	// rd = ~RRA + SIMM + 1, XER
	void Interpreter::subfic(AnalyzeInfo& info)
	{
		uint32_t a = ~core->regs.gpr[info.paramBits[1]], b = (int32_t)info.Imm.Signed + 1, res;
		bool carry = false;

		res = AddCarry(a, b);
		carry = CarryBit != 0;

		core->regs.gpr[info.paramBits[0]] = res;
		if (carry) SET_XER_CA; else RESET_XER_CA;
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + XER[CA] - 1, XER
	void Interpreter::subfme(AnalyzeInfo& info)
	{
		ADDXER(~core->regs.gpr[info.paramBits[1]] - 1, info);
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + XER[CA] - 1, CR0, XER
	void Interpreter::subfme_d(AnalyzeInfo& info)
	{
		ADDXERD(~core->regs.gpr[info.paramBits[1]] - 1, info);
		Gekko->regs.pc += 4;
	}

	void Interpreter::subfmeo(AnalyzeInfo& info)
	{
		Debug::Halt("subfmeo\n");
	}

	void Interpreter::subfmeo_d(AnalyzeInfo& info)
	{
		Debug::Halt("subfmeo.\n");
	}

	// rd = ~ra + XER[CA], XER
	void Interpreter::subfze(AnalyzeInfo& info)
	{
		ADDXER(~core->regs.gpr[info.paramBits[1]], info);
		Gekko->regs.pc += 4;
	}

	// rd = ~ra + XER[CA], CR0, XER
	void Interpreter::subfze_d(AnalyzeInfo& info)
	{
		ADDXERD(~core->regs.gpr[info.paramBits[1]], info);
		Gekko->regs.pc += 4;
	}

	void Interpreter::subfzeo(AnalyzeInfo& info)
	{
		Debug::Halt("subfzeo\n");
	}

	void Interpreter::subfzeo_d(AnalyzeInfo& info)
	{
		Debug::Halt("subfzeo.\n");
	}

}
