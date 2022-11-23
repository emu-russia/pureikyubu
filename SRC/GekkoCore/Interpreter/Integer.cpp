// Integer Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

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
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
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
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
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
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
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
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
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
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
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
		core->Halt("divwo\n");
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
		core->Halt("divwuo\n");
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
		core->Halt("mullwo\n");
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
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
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
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
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
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
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
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
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
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
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
		if (OverflowBit) { SET_XER_OV(); SET_XER_SO(); } else RESET_XER_OV();
		core->regs.pc += 4;
	}

	void Interpreter::subfzeo_d()
	{
		subfzeo();
		COMPUTE_CR0(GPR(0));
	}

}
