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

	void IntelAssembler::TwoByte(AnalyzeInfo& info, uint8_t b1, uint8_t b2)
	{
		OneByte(info, b1);
		OneByte(info, b2);
	}

	void IntelAssembler::OneByteImm8(AnalyzeInfo& info, uint8_t n)
	{
		if (info.numParams != 1)
		{
			throw "Invalid number of parameters for instruction with imm8.";
		}

		if (info.params[0] != Param::imm8)
		{
			throw "Invalid parameter type for instruction with imm8.";
		}

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
		info.numParams++;
	}

#pragma endregion "Private"

#pragma region "Base methods"

	void IntelAssembler::Assemble16(AnalyzeInfo& info)
	{
		switch (info.instr)
		{
			case Instruction::aaa: OneByte(info, 0x37); break;
			case Instruction::aad: OneByteImm8(info, 0xd5); break;
			case Instruction::aam: OneByteImm8(info, 0xd4); break;
			case Instruction::aas: OneByte(info, 0x3f); break;

			case Instruction::cbw: OneByte(info, 0x98); break;
			case Instruction::cwde: TwoByte(info, 0x66, 0x98); break;
			case Instruction::cdqe: Invalid(); break;

			case Instruction::cwd: OneByte(info, 0x99); break;
			case Instruction::cdq: TwoByte(info, 0x66, 0x99); break;
			case Instruction::cqo: Invalid(); break;
		}
	}

	void IntelAssembler::Assemble32(AnalyzeInfo& info)
	{
		switch (info.instr)
		{
			case Instruction::aaa: OneByte(info, 0x37); break;
			case Instruction::aad: OneByteImm8(info, 0xd5); break;
			case Instruction::aam: OneByteImm8(info, 0xd4); break;
			case Instruction::aas: OneByte(info, 0x3f); break;

			case Instruction::cbw: TwoByte(info, 0x66, 0x98); break;
			case Instruction::cwde: OneByte(info, 0x98); break;
			case Instruction::cdqe: Invalid(); break;

			case Instruction::cwd: TwoByte(info, 0x66, 0x99); break;
			case Instruction::cdq: OneByte(info, 0x99); break;
			case Instruction::cqo: Invalid(); break;
		}
	}

	void IntelAssembler::Assemble64(AnalyzeInfo& info)
	{
		switch (info.instr)
		{
			case Instruction::aaa: Invalid(); break;
			case Instruction::aad: Invalid(); break;
			case Instruction::aam: Invalid(); break;
			case Instruction::aas: Invalid(); break;

			case Instruction::cbw: TwoByte(info, 0x66, 0x98); break;
			case Instruction::cwde: OneByte(info, 0x98); break;
			case Instruction::cdqe: TwoByte(info, 0x48, 0x98); break;

			case Instruction::cwd: TwoByte(info, 0x66, 0x99); break;
			case Instruction::cdq: OneByte(info, 0x99); break;
			case Instruction::cqo: TwoByte(info, 0x48, 0x99); break;
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

	template <> AnalyzeInfo& IntelAssembler::aam<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::aam;
		AddImmParam(info, 10);
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::aam<16>(uint8_t v)
	{
		AnalyzeInfo info;
		info.instr = Instruction::aam;
		AddImmParam(info, v);
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::aam<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::aam;
		AddImmParam(info, 10);
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::aam<32>(uint8_t v)
	{
		AnalyzeInfo info;
		info.instr = Instruction::aam;
		AddImmParam(info, v);
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::aam<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::aam;
		AddImmParam(info, 10);
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::aam<64>(uint8_t v)
	{
		AnalyzeInfo info;
		info.instr = Instruction::aam;
		AddImmParam(info, v);
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::aas<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::aas;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::aas<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::aas;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::aas<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::aas;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cbw<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cbw;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cbw<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cbw;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cbw<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cbw;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cwde<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cwde;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cwde<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cwde;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cwde<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cwde;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cdqe<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cdqe;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cdqe<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cdqe;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cdqe<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cdqe;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cwd<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cwd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cwd<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cwd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cwd<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cwd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cdq<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cdq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cdq<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cdq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cdq<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cdq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cqo<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cqo;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cqo<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cqo;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cqo<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cqo;
		Assemble64(info);
		return info;
	}

#pragma endregion "Quick helpers"

}
