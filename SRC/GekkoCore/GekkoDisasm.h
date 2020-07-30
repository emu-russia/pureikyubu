// Gekko Disassembler.

#pragma once

#include <string>

namespace Gekko
{
	class GekkoDisasm
	{
		static std::string HexToStr(uint8_t value);
		static std::string HexToStr(uint16_t value);
		static std::string HexToStr(uint32_t value);

		static std::string Bcx(AnalyzeInfo* info, bool Disp, bool L, bool& simple, bool skipOperand[5]);
		static std::string SimplifiedInstruction(AnalyzeInfo* info, bool& simple, bool skipOperand[5]);
		static std::string InstrToString(AnalyzeInfo* info);
		static std::string SprName(int spr);
		static std::string TbrName(int tbr);
		static std::string Imm(int val, bool forceHex, bool useSign);
		static std::string ParamToString(Param param, int paramBits, AnalyzeInfo* info);

	public:

		static std::string Disasm(uint32_t pc, AnalyzeInfo * info, bool showAddress, bool showInstr);

	};
}
