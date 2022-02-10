// Gekko Disassembler.

#pragma once

namespace Gekko
{
	class GekkoDisasm
	{
		friend GekkoCore;

		static std::string HexToStr(uint8_t value);
		static std::string HexToStr(uint16_t value);
		static std::string HexToStr(uint32_t value);

		static std::string Bcx(DecoderInfo* info, bool Disp, bool L, bool& simple, bool skipOperand[5]);
		static std::string SimplifiedInstruction(DecoderInfo* info, bool& simple, bool skipOperand[5]);
		static std::string SprName(int spr);
		static std::string TbrName(int tbr);
		static std::string Imm(int val, bool forceHex, bool useSign);

	public:
		
		// Made it public for use in analyzer testing commands

		static std::string InstrToString(DecoderInfo* info);
		static std::string ParamToString(Param param, int paramBits, DecoderInfo* info);
		static std::string ParamName(Param param);

		static std::string Disasm(uint32_t pc, DecoderInfo * info, bool showAddress, bool showInstr);

	};
}
