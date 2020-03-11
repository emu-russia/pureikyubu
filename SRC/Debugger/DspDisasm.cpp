// DSP analyzer & disassembler

#include "dolphin.h"

namespace DSP
{

#pragma region "Analyzer part"

	bool Analyze(uint8_t* instrPtr, size_t instrMaxSize, AnalyzeInfo& info)
	{
		return false;
	}

#pragma endregion "Analyzer part"


#pragma region "Disassembler Part"

	std::string AnalyzeInfo::ParameterToString(DspParameter index)
	{
		switch (index)
		{
			// Registers

			case DspParameter::ar0: return "ar.0";
			case DspParameter::ar1: return "ar.1";
			case DspParameter::ar2: return "ar.2";
			case DspParameter::ar3: return "ar.3";
			case DspParameter::ix0: return "ix.0";
			case DspParameter::ix1: return "ix.1";
			case DspParameter::ix2: return "ix.2";
			case DspParameter::ix3: return "ix.3";
			case DspParameter::r08: return "r08";
			case DspParameter::r09: return "r09";
			case DspParameter::r0a: return "r0a";
			case DspParameter::r0b: return "r0b";
			case DspParameter::st0: return "st0";
			case DspParameter::st1: return "st1";
			case DspParameter::st2: return "st2";
			case DspParameter::st3: return "st3";
			case DspParameter::ac0h: return "ac0.h";
			case DspParameter::ac1h: return "ac1.h";
			case DspParameter::config: return "config";
			case DspParameter::sr: return "sr";
			case DspParameter::prodl: return "prod.l";
			case DspParameter::prodm1: return "prod.m1";
			case DspParameter::prodh: return "prod.h";
			case DspParameter::prodm2: return "prod.m2";
			case DspParameter::ax0l: return "ax0.l";
			case DspParameter::ax0h: return "ax0.h";
			case DspParameter::ax1l: return "ax1.l";
			case DspParameter::ax1h: return "ax1.h";
			case DspParameter::ac0l: return "ac0.l";
			case DspParameter::ac1l: return "ac1.l";
			case DspParameter::ac0m: return "ac0.m";
			case DspParameter::ac1m: return "ac1.m";

			// Immediates

		}

		return "unknown";
	}

	template<>
	static std::string AnalyzeInfo::ToHexString(uint16_t address)
	{
		return std::to_string(address);
	}

	template<>
	static std::string AnalyzeInfo::ToHexString(uint8_t Byte)
	{
		return std::to_string(Byte);
	}

	std::string AnalyzeInfo::InstrToString(DspInstruction instr, ConditionCode cc)
	{
		return "";
	}

	std::string AnalyzeInfo::InstrExToString(DspInstructionEx instrEx)
	{
		return "";
	}

	std::string Disasm(uint16_t startAddr, AnalyzeInfo& info)
	{
		std::string text = "";

		// Address and code bytes

		text += AnalyzeInfo::ToHexString(startAddr);
		text += " ";

		for (int i = 0; i < info.sizeInBytes; i++)
		{
			text += AnalyzeInfo::ToHexString(info.bytes[i]) + " ";
		}

		// Regular instruction

		if (info.instr != DspInstruction::Unknown)
		{
			text += AnalyzeInfo::InstrToString(info.instr, info.cc);
		}
		else
		{
			text += "Unknonwn " + AnalyzeInfo::ToHexString(info.instrBits);
		}

		bool firstParam = true;
		for (int i = 0; i < info.numParameters; i++)
		{
			if (!firstParam)
			{
				text += ", ";
			}
			text += AnalyzeInfo::ParameterToString(info.params[i]);
			firstParam = false;
		}

		// Extended instruction (next line)

		if (info.extendedOpcodePresent)
		{
			text += "\n";

			if (info.instrEx != DspInstructionEx::Unknown)
			{
				text += AnalyzeInfo::InstrExToString(info.instrEx);
			}
			else
			{
				text += "Unknonwn ext " + AnalyzeInfo::ToHexString(info.instrExBits);
			}

			bool firstExtendedParam = true;
			for (int i=0; i<info.numParametersEx; i++)
			{
				if (!firstExtendedParam)
				{
					text += ", ";
				}
				text += AnalyzeInfo::ParameterToString(info.paramsEx[i]);
				firstExtendedParam = false;
			}
		}

		return text;
	}

#pragma endregion "Disassembler Part"

}
