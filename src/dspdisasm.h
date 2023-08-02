// DSP disassembler.

#pragma once

namespace DSP
{
	class DspDisasm
	{
		// Utilities used by disasm

		static std::string ParameterToString(DspParameter index, DecoderInfo& info);
		static std::string InstrToString(DspRegularInstruction, ConditionCode);
		static std::string ParrallelInstrToString(DspParallelInstruction);
		static std::string ParrallelMemInstrToString(DspParallelMemInstruction);

		static inline std::string ToHexString(uint16_t address)
		{
			char buf[0x100] = { 0, };
			sprintf(buf, "%04X", address);
			return std::string(buf);
		}

		static inline std::string ToHexString(uint8_t Byte)
		{
			char buf[0x100] = { 0, };
			sprintf(buf, "%02X", Byte);
			return std::string(buf);
		}

		static bool IsHardwareReg(DspAddress address);
		static std::string HardwareRegName(DspAddress address);

		static std::string CondCodeToString(ConditionCode cc);

	public:

		static std::string Disasm(DspAddress startAddr, DecoderInfo& info);
	};
}
