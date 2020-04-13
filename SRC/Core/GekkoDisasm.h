// Gekko Disassembler.

#pragma once

#include <string>

namespace Gekko
{
	class GekkoDisasm
	{
		static std::string HexToString(uint32_t value);
		static std::string SimplifiedInstruction(bool& simple);
		static std::string InstrToString(AnalyzeInfo* info);
		static std::string SprName(int spr);
		static std::string ParamToString(Param * param);

	public:

		static std::string Disasm(uint32_t pc, AnalyzeInfo * info);

	};
}
