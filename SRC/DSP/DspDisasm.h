// DSP disassembler.

#pragma once

namespace DSP
{
	class DspDisasm
	{
		// Utilities used by disasm

		template<typename T>
		static std::string ToHexString(T data) { return ""; }
		static std::string ParameterToString(DspParameter index, AnalyzeInfo& info);
		static std::string InstrToString(DspRegularInstruction, ConditionCode);
		static std::string ParrallelInstrToString(DspParallelInstruction);
		static std::string ParrallelMemInstrToString(DspParallelMemInstruction);

		template<typename Dummy>
		static inline std::string ToHexString(uint16_t address)
		{
			char buf[0x100] = { 0, };
			sprintf(buf, "%04X", address);
			return std::string(buf);
		}

		template<typename Dummy>
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

		static std::string Disasm(DspAddress startAddr, AnalyzeInfo& info);
	};
}
