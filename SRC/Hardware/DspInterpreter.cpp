// GameCube DSP interpreter

#include "pch.h"

namespace DSP
{
	DspInterpreter::DspInterpreter(DspCore* parent)
	{
		core = parent;
	}

	DspInterpreter::~DspInterpreter()
	{
	}

	void DspInterpreter::LRI(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramBits[0], info.ImmOperand.UnsignedShort);
	}

	void DspInterpreter::M2(AnalyzeInfo& info)
	{
		// Ignore for now
	}

	void DspInterpreter::M0(AnalyzeInfo& info)
	{
		// Ignore for now
	}

	void DspInterpreter::CLR15(AnalyzeInfo& info)
	{
		// Ignore for now
	}

	void DspInterpreter::SET15(AnalyzeInfo& info)
	{
		// Ignore for now
	}

	void DspInterpreter::CLR40(AnalyzeInfo& info)
	{
		// Ignore for now
	}

	void DspInterpreter::SET40(AnalyzeInfo& info)
	{
		// Ignore for now
	}

	void DspInterpreter::SBSET(AnalyzeInfo& info)
	{
		core->regs.sr |= (1 << info.ImmOperand.Byte);
	}

	void DspInterpreter::Dispatch(AnalyzeInfo& info)
	{
		// Test breakpoints
		if (core->IsRunning())
		{
			if (core->TestBreakpoint(core->regs.pc))
			{
				DBReport(_DSP "IMEM breakpoint at 0x%04X\n", core->regs.pc);
				core->Suspend();
				return;
			}
		}

		switch (info.instr)
		{
			case DspInstruction::LRI: LRI(info); break;

			case DspInstruction::M2: M2(info); break;
			case DspInstruction::M0: M0(info); break;
			case DspInstruction::CLR15: CLR15(info); break;
			case DspInstruction::SET15: SET15(info); break;
			case DspInstruction::CLR40: CLR40(info); break;
			case DspInstruction::SET40: SET40(info); break;

			case DspInstruction::SBSET: SBSET(info); break;

			default:
				DBHalt(_DSP "Unknown instruction at 0x%04X\n", core->regs.pc);
				core->Suspend();
				return;
		}

		// Packed tuple
		if (info.extendedOpcodePresent)
		{
			switch (info.instrEx)
			{
				case DspInstructionEx::NOP2: break;

				default:
					DBHalt(_DSP "Unknown packed instruction at 0x%04X\n", core->regs.pc);
					core->Suspend();
					return;
			}
		}

		if (!info.flowControl)
		{
			core->regs.pc += (DspAddress)(info.sizeInBytes >> 1);
		}
	}

	void DspInterpreter::ExecuteInstr()
	{
		AnalyzeInfo info;

		// Fetch, analyze and dispatch instruction at pc addr

		DspAddress imemAddr = core->regs.pc;

		uint8_t* imemPtr = core->TranslateIMem(imemAddr);
		if (imemPtr == nullptr)
		{
			DBReport(_DSP "TranslateIMem failed on dsp addr: 0x%04X\n", imemAddr);
			return;
		}

		if (!Analyzer::Analyze(imemPtr, DspCore::MaxInstructionSizeInBytes, info))
		{
			DBReport(_DSP "DSP Analyzer failed on dsp addr: 0x%04X\n", imemAddr);
			return;
		}

		Dispatch(info);
	}

}
