// All parallel instructions except multiplication.
#include "pch.h"

using namespace Debug;

namespace DSP
{

	void DspInterpreter::p_add(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_add\n");
	}

	void DspInterpreter::p_addl(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_addl\n");
	}

	void DspInterpreter::p_sub(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_sub\n");
	}

	void DspInterpreter::p_amv(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_amv\n");
	}

	void DspInterpreter::p_cmp(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_cmp\n");
	}

	void DspInterpreter::p_inc(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_inc\n");
	}

	void DspInterpreter::p_dec(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_dec\n");
	}

	void DspInterpreter::p_abs(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_abs\n");
	}

	void DspInterpreter::p_neg(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_neg\n");
	}

	void DspInterpreter::p_clr(AnalyzeInfo& info)
	{
		switch (info.params[0])
		{
			case DspParameter::a: core->regs.a.bits = 0; break;
			case DspParameter::b: core->regs.b.bits = 0; break;

			case DspParameter::prod:
				core->regs.prod.l = 0x0000;
				core->regs.prod.m1 = 0xfff0;
				core->regs.prod.h = 0x00ff;
				core->regs.prod.m2 = 0x0010;
				break;

			case DspParameter::psr_im: core->regs.psr.im = 0; break;
			case DspParameter::psr_dp: core->regs.psr.dp = 0; break;
			case DspParameter::psr_xl: core->regs.psr.xl = 0; break;
		}
	}

	void DspInterpreter::p_rnd(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_rnd\n");
	}

	void DspInterpreter::p_rndp(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_rndp\n");
	}

	void DspInterpreter::p_tst(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_tst\n");
	}

	void DspInterpreter::p_lsl16(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_lsl16\n");
	}

	void DspInterpreter::p_lsr16(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_lsr16\n");
	}

	void DspInterpreter::p_asr16(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_asr16\n");
	}

	void DspInterpreter::p_addp(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_addp\n");
	}

	void DspInterpreter::p_set(AnalyzeInfo& info)
	{
		switch (info.params[0])
		{
			case DspParameter::psr_im: core->regs.psr.im = 1; break;
			case DspParameter::psr_dp: core->regs.psr.dp = 1; break;
			case DspParameter::psr_xl: core->regs.psr.xl = 1; break;
		}
	}

	void DspInterpreter::p_not(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_not\n");
	}

	void DspInterpreter::p_xor(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_xor\n");
	}

	void DspInterpreter::p_and(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_and\n");
	}

	void DspInterpreter::p_or(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_or\n");
	}

	void DspInterpreter::p_lsf(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_lsf\n");
	}

	void DspInterpreter::p_asf(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::p_asf\n");
	}

}
