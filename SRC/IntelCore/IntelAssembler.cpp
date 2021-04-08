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
			throw "Code stream overflow";
		}

		info.instrBytes[info.instrSize] = n;
		info.instrSize++;
	}

	void IntelAssembler::OneByteImm8(AnalyzeInfo& info, uint8_t n)
	{
		OneByte(info, n);
		OneByte(info, info.Imm.uimm8);
	}

	void IntelAssembler::AddImmParam(AnalyzeInfo& info, uint8_t n)
	{
		if (info.numParams >= ParamsMax)
		{
			throw "Parameter overflow";
		}

		info.params[info.numParams] = Param::imm8;
		info.Imm.uimm8 = n;
	}

#pragma endregion "Private"

#pragma region "Base methods"

	void IntelAssembler::Assemble16(AnalyzeInfo& info)
	{
		switch (info.instr)
		{
			case Instruction::aaa: OneByte(info, 0x37); break;
			case Instruction::aad: OneByteImm8(info, 0xd5); break;
		}
	}

	void IntelAssembler::Assemble32(AnalyzeInfo& info)
	{
		switch (info.instr)
		{
			case Instruction::aaa: OneByte(info, 0x37); break;
			case Instruction::aad: OneByteImm8(info, 0xd5); break;
		}
	}

	void IntelAssembler::Assemble64(AnalyzeInfo& info)
	{
		switch (info.instr)
		{
			case Instruction::aaa: Invalid(); break;
			case Instruction::aad: Invalid(); break;
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

	template <> AnalyzeInfo& IntelAssembler::aad<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::aad;
		AddImmParam(info, 10);
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::aad<16>(uint8_t v)
	{
		AnalyzeInfo info;
		info.instr = Instruction::aad;
		AddImmParam(info, v);
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::aad<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::aad;
		AddImmParam(info, 10);
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::aad<32>(uint8_t v)
	{
		AnalyzeInfo info;
		info.instr = Instruction::aad;
		AddImmParam(info, v);
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::aad<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::aad;
		AddImmParam(info, 10);
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::aad<64>(uint8_t v)
	{
		AnalyzeInfo info;
		info.instr = Instruction::aad;
		AddImmParam(info, v);
		Assemble64(info);
		return info;
	}

#pragma endregion "Quick helpers"

}
