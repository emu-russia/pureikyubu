// Non-parallel instructions.
#include "pch.h"

using namespace Debug;

namespace DSP
{

	void DspInterpreter::jmp(AnalyzeInfo& info)
	{
		DspAddress address;

		assert(info.numParameters == 1);

		if (info.params[0] == DspParameter::Address)
		{
			address = info.ImmOperand.Address;
		}
		else
		{
			address = core->MoveFromReg((int)info.params[0]);
		}

		if (ConditionTrue(info.cc))
		{
			if (core->dsp->logNonconditionalCallJmp && info.cc == ConditionCode::always)
			{
				Report(Channel::DSP, "0x%04X: jmp 0x%04X\n", core->regs.pc, address);
			}

			core->regs.pc = address;
		}
		else
		{
			core->regs.pc += (DspAddress)(info.sizeInBytes >> 1);
		}
	}

	void DspInterpreter::call(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::call");
	}

	void DspInterpreter::rets(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::rets");
	}

	void DspInterpreter::reti(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::reti");
	}

	void DspInterpreter::trap(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::trap");
	}

	void DspInterpreter::wait(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::wait");
	}

	void DspInterpreter::exec(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::exec");
	}

	void DspInterpreter::loop(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::loop");
	}

	void DspInterpreter::rep(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::rep");
	}

	void DspInterpreter::pld(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::pld");
	}

	void DspInterpreter::mr(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::mr");
	}

	void DspInterpreter::adsi(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::adsi");
	}

	void DspInterpreter::adli(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::adli");
	}

	void DspInterpreter::cmpsi(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::cmpsi");
	}

	void DspInterpreter::cmpli(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::cmpli");
	}

	void DspInterpreter::lsfi(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::lsfi");
	}

	void DspInterpreter::asfi(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::asfi");
	}

	void DspInterpreter::xorli(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::xorli");
	}

	void DspInterpreter::anli(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::anli");
	}

	void DspInterpreter::orli(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::orli");
	}

	void DspInterpreter::norm(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::norm");
	}

	void DspInterpreter::div(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::div");
	}

	void DspInterpreter::addc(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::addc");
	}

	void DspInterpreter::subc(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::subc");
	}

	void DspInterpreter::negc(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::negc");
	}

	void DspInterpreter::_max(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::max");
	}

	void DspInterpreter::lsf(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::lsf");
	}

	void DspInterpreter::asf(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::asf");
	}

	void DspInterpreter::ld(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::ld");
	}

	void DspInterpreter::st(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::st");
	}

	void DspInterpreter::ldsa(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::ldsa");
	}

	void DspInterpreter::stsa(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::stsa");
	}

	void DspInterpreter::ldla(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::ldla");
	}

	void DspInterpreter::stla(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::stla");
	}

	void DspInterpreter::mv(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::mv");
	}

	void DspInterpreter::mvsi(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::mvsi");
	}

	void DspInterpreter::mvli(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::mvli");
	}

	void DspInterpreter::stli(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::stli");
	}

	void DspInterpreter::clr(AnalyzeInfo& info)
	{
		switch (info.params[0])
		{
			case DspParameter::psr_tb: core->regs.psr.tb = 0; break;
			case DspParameter::psr_sv: core->regs.psr.sv = 0; break;
			case DspParameter::psr_te0: core->regs.psr.te0 = 0; break;
			case DspParameter::psr_te1: core->regs.psr.te1 = 0; break;
			case DspParameter::psr_te2: core->regs.psr.te2 = 0; break;
			case DspParameter::psr_te3: core->regs.psr.te3 = 0; break;
			case DspParameter::psr_et: core->regs.psr.et = 0; break;

			default:
				Halt("DspInterpreter::clr: Invalid parameter\n");
		}
	}

	void DspInterpreter::set(AnalyzeInfo& info)
	{
		switch (info.params[0])
		{
			case DspParameter::psr_tb: core->regs.psr.tb = 1; break;
			case DspParameter::psr_sv: core->regs.psr.sv = 1; break;
			case DspParameter::psr_te0: core->regs.psr.te0 = 1; break;
			case DspParameter::psr_te1: core->regs.psr.te1 = 1; break;
			case DspParameter::psr_te2: core->regs.psr.te2 = 1; break;
			case DspParameter::psr_te3: core->regs.psr.te3 = 1; break;
			case DspParameter::psr_et: core->regs.psr.et = 1; break;

			default:
				Halt("DspInterpreter::set: Invalid parameter\n");
		}
	}

	void DspInterpreter::btstl(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::btstl");
	}
	
	void DspInterpreter::btsth(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::btsth");
	}

}
