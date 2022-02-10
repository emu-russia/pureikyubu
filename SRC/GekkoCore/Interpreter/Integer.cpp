// Integer Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

	// We use macro programming to compress the source code.
	// Now I am not very willing to use such things.

	#define GPR(n) (core->regs.gpr[info.paramBits[(n)]])

	// rd = ra + rb
	void Interpreter::add(DecoderInfo& info)
	{
		GPR(0) = GPR(1) + GPR(2);
		core->regs.pc += 4;
	}

	// rd = ra + rb, CR0
	void Interpreter::add_d(DecoderInfo& info)
	{
		add(info);
		COMPUTE_CR0(GPR(0));
	}

	// rd = ra + rb, XER
	void Interpreter::addo(DecoderInfo& info)
	{
		CarryBit = 0;
		GPR(0) = FullAdder(GPR(1), GPR(2));
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
		core->regs.pc += 4;
	}

	// rd = ra + rb, CR0, XER
	void Interpreter::addo_d(DecoderInfo& info)
	{
		addo(info);
		COMPUTE_CR0(GPR(0));
	}

	// rd = ra + rb, XER[CA]
	void Interpreter::addc(DecoderInfo& info)
	{
		CarryBit = 0;
		GPR(0) = FullAdder(GPR(1), GPR(2));
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ra + rb, XER[CA], CR0
	void Interpreter::addc_d(DecoderInfo& info)
	{
		addc(info);
		COMPUTE_CR0(GPR(0));
	}

	// rd = ra + rb, XER[CA], XER[OV]
	void Interpreter::addco(DecoderInfo& info)
	{
		CarryBit = 0;
		GPR(0) = FullAdder(GPR(1), GPR(2));
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
		core->regs.pc += 4;
	}

	// rd = ra + rb, XER[CA], XER[OV], CR0
	void Interpreter::addco_d(DecoderInfo& info)
	{
		addco(info);
		COMPUTE_CR0(GPR(0));
	}

	// rd = ra + rb + XER[CA], XER
	void Interpreter::adde(DecoderInfo& info)
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(GPR(1), GPR(2));
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ra + rb + XER[CA], CR0, XER
	void Interpreter::adde_d(DecoderInfo& info)
	{
		adde(info);
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::addeo(DecoderInfo& info)
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(GPR(1), GPR(2));
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
		core->regs.pc += 4;
	}

	void Interpreter::addeo_d(DecoderInfo& info)
	{
		addeo(info);
		COMPUTE_CR0(GPR(0));
	}

	// rd = (ra | 0) + SIMM
	void Interpreter::addi(DecoderInfo& info)
	{
		if (info.paramBits[1]) GPR(0) = GPR(1) + (int32_t)info.Imm.Signed;
		else GPR(0) = (int32_t)info.Imm.Signed;
		core->regs.pc += 4;
	}

	// rd = ra + SIMM, XER
	void Interpreter::addic(DecoderInfo& info)
	{
		CarryBit = 0;
		GPR(0) = FullAdder(GPR(1), (int32_t)info.Imm.Signed);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ra + SIMM, CR0, XER
	void Interpreter::addic_d(DecoderInfo& info)
	{
		addic(info);
		COMPUTE_CR0(GPR(0));
	}

	// rd = (ra | 0) + (SIMM || 0x0000)
	void Interpreter::addis(DecoderInfo& info)
	{
		if (info.paramBits[1]) GPR(0) = GPR(1) + ((int32_t)info.Imm.Signed << 16);
		else GPR(0) = (int32_t)info.Imm.Signed << 16;
		core->regs.pc += 4;
	}

	// rd = ra + XER[CA] - 1 (0xffffffff), XER
	void Interpreter::addme(DecoderInfo& info)
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(GPR(1), -1);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ra + XER[CA] - 1 (0xffffffff), CR0, XER
	void Interpreter::addme_d(DecoderInfo& info)
	{
		addme(info);
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::addmeo(DecoderInfo& info)
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(GPR(1), -1);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
		core->regs.pc += 4;
	}

	void Interpreter::addmeo_d(DecoderInfo& info)
	{
		addmeo(info);
		COMPUTE_CR0(GPR(0));
	}

	// rd = ra + XER[CA], XER
	void Interpreter::addze(DecoderInfo& info)
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(GPR(1), 0);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ra + XER[CA], CR0, XER
	void Interpreter::addze_d(DecoderInfo& info)
	{
		addze(info);
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::addzeo(DecoderInfo& info)
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(GPR(1), 0);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
		core->regs.pc += 4;
	}

	void Interpreter::addzeo_d(DecoderInfo& info)
	{
		addzeo(info);
		COMPUTE_CR0(GPR(0));
	}

	// rd = ra / rb (signed)
	void Interpreter::divw(DecoderInfo& info)
	{
		int32_t a = GPR(1), b = GPR(2);
		if (b) GPR(0) = a / b;
		core->regs.pc += 4;
	}

	// rd = ra / rb (signed), CR0
	void Interpreter::divw_d(DecoderInfo& info)
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

	void Interpreter::divwo(DecoderInfo& info)
	{
		Debug::Halt("divwo\n");
	}

	void Interpreter::divwo_d(DecoderInfo& info)
	{
		divwo(info);
		COMPUTE_CR0(GPR(0));
	}

	// rd = ra / rb (unsigned)
	void Interpreter::divwu(DecoderInfo& info)
	{
		uint32_t a = GPR(1), b = GPR(2);
		if (b) GPR(0) = a / b;
		core->regs.pc += 4;
	}

	// rd = ra / rb (unsigned), CR0
	void Interpreter::divwu_d(DecoderInfo& info)
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

	void Interpreter::divwuo(DecoderInfo& info)
	{
		Debug::Halt("divwuo\n");
	}

	void Interpreter::divwuo_d(DecoderInfo& info)
	{
		divwuo(info);
		COMPUTE_CR0(GPR(0));
	}

	// prod[0-63] = ra * rb
	// rd = prod[0-31]
	void Interpreter::mulhw(DecoderInfo& info)
	{
		int64_t a = (int32_t)GPR(1), b = (int32_t)GPR(2), res = a * b;
		res = (res >> 32);
		GPR(0) = (int32_t)res;
		core->regs.pc += 4;
	}

	// prod[0-63] = ra * rb
	// rd = prod[0-31]
	// CR0
	void Interpreter::mulhw_d(DecoderInfo& info)
	{
		mulhw(info);
		COMPUTE_CR0(GPR(0));
	}

	// prod[0-63] = ra * rb
	// rd = prod[0-31]
	void Interpreter::mulhwu(DecoderInfo& info)
	{
		uint64_t a = GPR(1), b = GPR(2), res = a * b;
		res = (res >> 32);
		GPR(0) = (uint32_t)res;
		core->regs.pc += 4;
	}

	// prod[0-63] = ra * rb
	// rd = prod[0-31]
	// CR0
	void Interpreter::mulhwu_d(DecoderInfo& info)
	{
		mulhwu(info);
		COMPUTE_CR0(GPR(0));
	}

	// prod[0-48] = ra * SIMM
	// rd = prod[16-48]
	void Interpreter::mulli(DecoderInfo& info)
	{
		GPR(0) = GPR(1) * (int32_t)info.Imm.Signed;
		core->regs.pc += 4;
	}

	// prod[0-48] = ra * rb
	// rd = prod[16-48]
	void Interpreter::mullw(DecoderInfo& info)
	{
		int32_t a = GPR(1), b = GPR(2);
		int64_t res = (int64_t)a * (int64_t)b;
		GPR(0) = (int32_t)(res & 0x00000000ffffffff);
		core->regs.pc += 4;
	}

	// prod[0-48] = ra * rb
	// rd = prod[16-48]
	// CR0
	void Interpreter::mullw_d(DecoderInfo& info)
	{
		mullw(info);
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::mullwo(DecoderInfo& info)
	{
		Debug::Halt("mullwo\n");
	}

	void Interpreter::mullwo_d(DecoderInfo& info)
	{
		mullwo(info);
		COMPUTE_CR0(GPR(0));
	}

	// rd = ~ra + 1
	void Interpreter::neg(DecoderInfo& info)
	{
		GPR(0) = ~GPR(1) + 1;
		core->regs.pc += 4;
	}

	// rd = ~ra + 1, CR0
	void Interpreter::neg_d(DecoderInfo& info)
	{
		neg(info);
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::nego(DecoderInfo& info)
	{
		CarryBit = 0;
		GPR(0) = FullAdder(~GPR(1), 1);
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
		core->regs.pc += 4;
	}

	void Interpreter::nego_d(DecoderInfo& info)
	{
		nego(info);
		COMPUTE_CR0(GPR(0));
	}

	// rd = ~ra + rb + 1
	void Interpreter::subf(DecoderInfo& info)
	{
		GPR(0) = ~GPR(1) + GPR(2) + 1;
		core->regs.pc += 4;
	}

	// rd = ~ra + rb + 1, CR0
	void Interpreter::subf_d(DecoderInfo& info)
	{
		subf(info);
		COMPUTE_CR0(GPR(0));
	}

	// rd = ~ra + rb + 1, XER
	void Interpreter::subfo(DecoderInfo& info)
	{
		CarryBit = 1;
		GPR(0) = FullAdder(~GPR(1), GPR(2));
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
		core->regs.pc += 4;
	}

	// rd = ~ra + rb + 1, CR0, XER
	void Interpreter::subfo_d(DecoderInfo& info)
	{
		subfo(info);
		COMPUTE_CR0(GPR(0));
	}

	// rd = ~ra + rb + 1, XER[CA]
	void Interpreter::subfc(DecoderInfo& info)
	{
		CarryBit = 1;
		GPR(0) = FullAdder(~GPR(1), GPR(2));
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ~ra + rb + 1, XER[CA], CR0
	void Interpreter::subfc_d(DecoderInfo& info)
	{
		subfc(info);
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::subfco(DecoderInfo& info)
	{
		CarryBit = 1;
		GPR(0) = FullAdder(~GPR(1), GPR(2));
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
		core->regs.pc += 4;
	}

	void Interpreter::subfco_d(DecoderInfo& info)
	{
		subfco(info);
		COMPUTE_CR0(GPR(0));
	}

	// rd = ~ra + rb + XER[CA], XER
	void Interpreter::subfe(DecoderInfo& info)
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(~GPR(1), GPR(2));
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ~ra + rb + XER[CA], CR0, XER
	void Interpreter::subfe_d(DecoderInfo& info)
	{
		subfe(info);
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::subfeo(DecoderInfo& info)
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(~GPR(1), GPR(2));
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
		core->regs.pc += 4;
	}

	void Interpreter::subfeo_d(DecoderInfo& info)
	{
		subfeo(info);
		COMPUTE_CR0(GPR(0));
	}

	// rd = ~RRA + SIMM + 1, XER
	void Interpreter::subfic(DecoderInfo& info)
	{
		CarryBit = 1;
		GPR(0) = FullAdder(~GPR(1), (int32_t)info.Imm.Signed);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ~ra + XER[CA] - 1, XER
	void Interpreter::subfme(DecoderInfo& info)
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(~GPR(1), -1);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ~ra + XER[CA] - 1, CR0, XER
	void Interpreter::subfme_d(DecoderInfo& info)
	{
		subfme(info);
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::subfmeo(DecoderInfo& info)
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(~GPR(1), -1);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
		core->regs.pc += 4;
	}

	void Interpreter::subfmeo_d(DecoderInfo& info)
	{
		subfmeo(info);
		COMPUTE_CR0(GPR(0));
	}

	// rd = ~ra + XER[CA], XER
	void Interpreter::subfze(DecoderInfo& info)
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(~GPR(1), 0);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		core->regs.pc += 4;
	}

	// rd = ~ra + XER[CA], CR0, XER
	void Interpreter::subfze_d(DecoderInfo& info)
	{
		subfze(info);
		COMPUTE_CR0(GPR(0));
	}

	void Interpreter::subfzeo(DecoderInfo& info)
	{
		CarryBit = IS_XER_CA ? 1 : 0;
		GPR(0) = FullAdder(~GPR(1), 0);
		if (CarryBit) SET_XER_CA(); else RESET_XER_CA();
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
		core->regs.pc += 4;
	}

	void Interpreter::subfzeo_d(DecoderInfo& info)
	{
		subfzeo(info);
		COMPUTE_CR0(GPR(0));
	}

}
