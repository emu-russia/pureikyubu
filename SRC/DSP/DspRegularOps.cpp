// Non-parallel instructions.
#include "pch.h"

using namespace Debug;

namespace DSP
{

	void DspInterpreter::jmp(AnalyzeInfo& info)
	{
		DspAddress address;

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
		DspAddress address;

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
				Report(Channel::DSP, "0x%04X: call 0x%04X\n", core->regs.pc, address);
			}

			if (core->regs.pcs->push((uint16_t)(core->regs.pc + (info.sizeInBytes >> 1))))
			{
				core->regs.pc = address;
			}
			else
			{
				core->AssertInterrupt(DspInterrupt::Error);
			}
		}
		else
		{
			core->regs.pc += (DspAddress)(info.sizeInBytes >> 1);
		}
	}

	void DspInterpreter::rets(AnalyzeInfo& info)
	{
		if (ConditionTrue(info.cc))
		{
			uint16_t pc;

			if (core->regs.pcs->pop(pc))
			{
				core->regs.pc = pc;
			}
			else
			{
				core->AssertInterrupt(DspInterrupt::Error);
			}
		}
		else
		{
			core->regs.pc++;
		}
	}

	void DspInterpreter::reti(AnalyzeInfo& info)
	{
		if (ConditionTrue(info.cc))
		{
			core->ReturnFromInterrupt();
		}
		else
		{
			core->regs.pc++;
		}
	}

	void DspInterpreter::trap(AnalyzeInfo& info)
	{
		core->AssertInterrupt(DspInterrupt::Trap);
	}

	void DspInterpreter::wait(AnalyzeInfo& info)
	{
		// In a real DSP, Clk is disabled and only the interrupt generation circuitry remains active. 
		// In the emulator, due to the fact that the instruction is flowControl, pc changes will not occur and the emulated DSP will "hang" on the execution of the `wait` instruction until an interrupt occurs.
	}

	void DspInterpreter::exec(AnalyzeInfo& info)
	{
		if (ConditionTrue(info.cc))
		{
			core->regs.pc++;
		}
		else
		{
			core->regs.pc += 2;
		}
	}

	void DspInterpreter::loop(AnalyzeInfo& info)
	{
		int lc;
		DspAddress end_addr;

		if (info.params[0] == DspParameter::Byte)
		{
			lc = info.ImmOperand.Byte;
			end_addr = info.ImmOperand2.Address;
		}
		else
		{
			lc = core->MoveFromReg((int)info.params[0]);
			end_addr = info.ImmOperand.Address;
		}

		if (lc != 0)
		{
			if (!core->regs.pcs->push(core->regs.pc + 2))
			{
				core->AssertInterrupt(DspInterrupt::Error);
				return;
			}
			if (!core->regs.lcs->push(lc))
			{
				core->AssertInterrupt(DspInterrupt::Error);
				return;
			}
			if (!core->regs.eas->push(end_addr))
			{
				core->AssertInterrupt(DspInterrupt::Error);
				return;
			}
			core->regs.pc += 2;
		}
		else
		{
			// If the parameter lc = 0, then accordingly no loop occurs, pc = end_address + 1 (the block is skipped)

			core->regs.pc = end_addr + 1;
		}
	}

	void DspInterpreter::rep(AnalyzeInfo& info)
	{
		int rc;

		if (info.params[0] == DspParameter::Byte)
		{
			rc = info.ImmOperand.Byte;
		}
		else
		{
			rc = core->MoveFromReg((int)info.params[0]);
		}

		if (rc != 0)
		{
			core->repeatCount = rc;
			core->regs.pc++;
		}
		else
		{
			core->regs.pc += 2;		// Skip next 1-cycle instruction
		}
	}

	void DspInterpreter::pld(AnalyzeInfo& info)
	{
		int r = (int)info.params[1];
		core->MoveToReg((int)info.params[0], core->dsp->ReadIMem(core->regs.r[r]) );
		AdvanceAddress(r, info.params[2]);
	}

	void DspInterpreter::mr(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::mr\n");
	}

	void DspInterpreter::adsi(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::adsi\n");
	}

	void DspInterpreter::adli(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::adli\n");
	}

	void DspInterpreter::cmpsi(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::cmpsi\n");
	}

	void DspInterpreter::cmpli(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::cmpli\n");
	}

	void DspInterpreter::lsfi(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::lsfi\n");
	}

	void DspInterpreter::asfi(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::asfi\n");
	}

	void DspInterpreter::xorli(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::xorli\n");
	}

	void DspInterpreter::anli(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::anli\n");
	}

	void DspInterpreter::orli(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::orli\n");
	}

	void DspInterpreter::norm(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::norm\n");
	}

	void DspInterpreter::div(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::div\n");
	}

	void DspInterpreter::addc(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::addc\n");
	}

	void DspInterpreter::subc(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::subc\n");
	}

	void DspInterpreter::negc(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::negc\n");
	}

	void DspInterpreter::_max(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::max\n");
	}

	void DspInterpreter::lsf(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::lsf\n");
	}

	void DspInterpreter::asf(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::asf\n");
	}

	void DspInterpreter::ld(AnalyzeInfo& info)
	{
		int r = (int)info.params[1];
		core->MoveToReg((int)info.params[0], core->dsp->ReadDMem(core->regs.r[r]) );
		AdvanceAddress(r, info.params[2]);
	}

	void DspInterpreter::st(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::st\n");
	}

	void DspInterpreter::ldsa(AnalyzeInfo& info)
	{
		core->MoveToReg((int)info.params[0], 
			core->dsp->ReadDMem( (DspAddress)((core->regs.dpp << 8) | (info.ImmOperand.Address))) );
	}

	void DspInterpreter::stsa(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::stsa\n");
	}

	void DspInterpreter::ldla(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::ldla\n");
	}

	void DspInterpreter::stla(AnalyzeInfo& info)
	{
		Halt("DspInterpreter::stla\n");
	}

	void DspInterpreter::mv(AnalyzeInfo& info)
	{
		core->MoveToReg((int)info.params[0], core->MoveFromReg((int)info.params[1]));
	}

	void DspInterpreter::mvsi(AnalyzeInfo& info)
	{
		core->MoveToReg((int)info.params[0], (int16_t)info.ImmOperand.SignedByte);
	}

	void DspInterpreter::mvli(AnalyzeInfo& info)
	{
		core->MoveToReg((int)info.params[0], info.ImmOperand.UnsignedShort);
	}

	void DspInterpreter::stli(AnalyzeInfo& info)
	{
		core->dsp->WriteDMem(info.ImmOperand.Address, info.ImmOperand2.UnsignedShort);
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
		uint16_t val = 0;

		switch (info.params[0])
		{
			case DspParameter::a1:
				val = core->regs.a.m;
				break;
			case DspParameter::b1:
				val = core->regs.b.m;
				break;

			default:
				Halt("DspInterpreter::btstl: Invalid parameter\n");
		}

		core->regs.psr.tb = (val & info.ImmOperand.UnsignedShort) == 0;
	}
	
	void DspInterpreter::btsth(AnalyzeInfo& info)
	{
		uint16_t val = 0;

		switch (info.params[0])
		{
			case DspParameter::a1:
				val = core->regs.a.m;
				break;
			case DspParameter::b1:
				val = core->regs.b.m;
				break;

			default:
				Halt("DspInterpreter::btsth: Invalid parameter\n");
		}

		core->regs.psr.tb = (val & info.ImmOperand.UnsignedShort) == info.ImmOperand.UnsignedShort;
	}

}
