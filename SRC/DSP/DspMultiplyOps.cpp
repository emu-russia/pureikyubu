// DSP multiply instructions
#include "pch.h"

// The PSR flags are set relative to operations on the ALU, not on the multiplier (the multiplier is a separate circuit without flags). Therefore, only the instructions `mvmpy`,` rnmpy` and `admpy` change flags.

using namespace Debug;

namespace DSP
{

	void DspInterpreter::p_mpy(DecoderInfo& info)
	{
		int64_t s1 = 0;
		int64_t s2 = 0;

		FetchMpyParams(info.params[0], info.params[1], s1, s2, true);

		core->regs.prod.bitsPacked = s1 * s2;
		core->UnpackProd(core->regs.prod);
	}

	void DspInterpreter::p_mac(DecoderInfo& info)
	{
		int64_t s1 = 0;
		int64_t s2 = 0;

		FetchMpyParams(info.params[0], info.params[1], s1, s2, false);

		core->PackProd(core->regs.prod);
		core->regs.prod.bitsPacked = s1 * s2 + DspCore::SignExtend40(core->regs.prod.bitsPacked);
		core->UnpackProd(core->regs.prod);
	}

	void DspInterpreter::p_macn(DecoderInfo& info)
	{
		int64_t s1 = 0;
		int64_t s2 = 0;

		FetchMpyParams(info.params[0], info.params[1], s1, s2, false);

		core->PackProd(core->regs.prod);
		core->regs.prod.bitsPacked = -s1 * s2 + DspCore::SignExtend40(core->regs.prod.bitsPacked);
		core->UnpackProd(core->regs.prod);
	}

	void DspInterpreter::p_mvmpy(DecoderInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		int64_t s1 = 0;
		int64_t s2 = 0;

		FetchMpyParams(info.params[1], info.params[2], s1, s2, true);

		core->PackProd(core->regs.prod);
		d = DspCore::SignExtend40(core->regs.prod.bitsPacked);
		r = d + s;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->regs.prod.bitsPacked = s1 * s2;
		core->UnpackProd(core->regs.prod);

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_rnmpy(DecoderInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		int64_t s1 = 0;
		int64_t s2 = 0;

		FetchMpyParams(info.params[1], info.params[2], s1, s2, true);

		core->PackProd(core->regs.prod);
		d = DspCore::SignExtend40(core->regs.prod.bitsPacked);
		s = DspCore::RndFactor(d);

		r = d + s;
		r &= ~0xffff;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->regs.prod.bitsPacked = s1 * s2;
		core->UnpackProd(core->regs.prod);

		core->ModifyFlags(d, s, r, CFlagRules::C7, VFlagRules::V6, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_admpy(DecoderInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		int64_t s1 = 0;
		int64_t s2 = 0;

		FetchMpyParams(info.params[1], info.params[2], s1, s2, true);

		switch (info.params[0])
		{
			case DspParameter::a:
				s = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				s = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		core->PackProd(core->regs.prod);
		d = DspCore::SignExtend40(core->regs.prod.bitsPacked);
		r = d + s;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->regs.prod.bitsPacked = s1 * s2;
		core->UnpackProd(core->regs.prod);

		core->ModifyFlags(d, s, r, CFlagRules::C1, VFlagRules::V1, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

}
