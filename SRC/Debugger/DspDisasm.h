/// DSP disassembler.

#pragma once

#include <string>

namespace DSP
{
	class DspDisasm
	{
		static const size_t MaxInstructionSizeInBytes = 4;		///< max instruction size (for padding)

		// Utilities used by disasm

		template<typename T>
		static std::string ToHexString(T data);
		static std::string ParameterToString(DspParameter index, AnalyzeInfo& info);
		static std::string InstrToString(DspInstruction, ConditionCode);
		static std::string InstrExToString(DspInstructionEx);

		// Kek, allowed only in headers..

		template<>
		static inline std::string ToHexString(uint16_t address)
		{
			char buf[0x100] = { 0, };
			sprintf_s(buf, sizeof(buf), "%04X", address);
			return std::string(buf);
		}

		template<>
		static inline std::string ToHexString(uint8_t Byte)
		{
			char buf[0x100] = { 0, };
			sprintf_s(buf, sizeof(buf), "%02X", Byte);
			return std::string(buf);
		}

		static bool IsHardwareReg(DspAddress address);
		static std::string HardwareRegName(DspAddress address);

		static std::string CondCodeToString(ConditionCode cc);

	public:

		// startAddr in DSP "slots"

		static std::string Disasm(uint16_t startAddr, AnalyzeInfo& info);
	};
}
