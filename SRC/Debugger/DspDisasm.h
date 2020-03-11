/// DSP disassembler.

#pragma once

#include <string>

namespace DSP
{
	class DspDisasm
	{
		// Utilities used by disasm

		template<typename T>
		static std::string ToHexString(T data);
		static std::string ParameterToString(DspParameter index);
		static std::string InstrToString(DspInstruction, ConditionCode);
		static std::string InstrExToString(DspInstructionEx);

	public:
		static std::string Disasm(uint16_t startAddr, AnalyzeInfo& info);
	};
}
