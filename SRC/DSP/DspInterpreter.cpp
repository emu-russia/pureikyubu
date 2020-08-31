// GameCube DSP interpreter
#include "pch.h"

using namespace Debug;

namespace DSP
{
	DspInterpreter::DspInterpreter(DspCore* parent)
	{
		core = parent;
	}

	DspInterpreter::~DspInterpreter()
	{
	}

	void DspInterpreter::ExecuteInstr()
	{
	}

}
