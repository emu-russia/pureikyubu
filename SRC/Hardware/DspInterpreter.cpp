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

	void DspInterpreter::Dispatch(AnalyzeInfo& info)
	{
		DBReport("DspInterpreter::Dispatch\n");
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
