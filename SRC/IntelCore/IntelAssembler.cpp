// Intel instruction code generator.

#include "pch.h"

namespace IntelCore
{

#pragma region "Private"

	void IntelAssembler::Invalid()
	{
		throw "Invalid instruction in specified mode";
	}

	void IntelAssembler::OneByte(AnalyzeInfo& info, uint8_t n)
	{
		if (info.instrSize >= InstrMaxSize)
		{
			throw "IntelAssembler::OneByte overflow!";
		}

		info.instrBytes[info.instrSize] = n;
		info.instrSize++;
	}

#pragma endregion "Private"

#pragma region "Base methods"

	void IntelAssembler::Assemble16(AnalyzeInfo& info)
	{
		switch (info.instr)
		{
			case Instruction::aaa: OneByte(info, 0x37); break;
		}
	}

	void IntelAssembler::Assemble32(AnalyzeInfo& info)
	{
		switch (info.instr)
		{
			case Instruction::aaa: OneByte(info, 0x37); break;
		}
	}

	void IntelAssembler::Assemble64(AnalyzeInfo& info)
	{
		switch (info.instr)
		{
			case Instruction::aaa: Invalid(); break;
		}
	}

#pragma endregion "Base methods"

#pragma region "Quick helpers"

	// Instructions using ModRM / with an immediate operand

	template <> AnalyzeInfo& IntelAssembler::adc<16>()
	{
		AnalyzeInfo info;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::adc<32>()
	{
		AnalyzeInfo info;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::adc<64>()
	{
		AnalyzeInfo info;
		Assemble64(info);
		return info;
	}

	// Simple encoding instructions

	// One or more byte instructions

	template <> AnalyzeInfo& IntelAssembler::aaa<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::aaa;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::aaa<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::aaa;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::aaa<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::aaa;
		Assemble64(info);
		return info;
	}

#pragma endregion "Quick helpers"

}
