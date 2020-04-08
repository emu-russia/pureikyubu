// MN102 Disassembler

#pragma once

#include <string>

namespace DVD
{
	class MnDisasm
	{
		static std::string HexToStr(uint8_t value);
		static std::string HexToStr(uint16_t value);
		static std::string HexToStr(uint32_t value);

		static std::string CcToText(MnCond cc);
		static std::string InstrToText(MnInstrInfo* info);
		static std::string OperandToText(uint32_t pc, MnInstrInfo* info, int n);

	public:
		static std::string Disasm(uint32_t pc, MnInstrInfo* info);
	};
}
