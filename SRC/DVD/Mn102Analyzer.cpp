#include "pch.h"

namespace DVD
{
	bool MnAnalyze::Analyze(uint8_t* instrPtr, MnInstrInfo* info)
	{
		assert(instrPtr);
		assert(info);

		info->instr = MnInstruction::Unknown;
		info->instrSize = 0;
		info->numOp = 0;

		return false;
	}

}
