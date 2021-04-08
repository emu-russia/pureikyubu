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

	void IntelAssembler::TriByte(AnalyzeInfo& info, uint8_t b1, uint8_t b2, uint8_t b3)
	{
		OneByte(info, b1);
		OneByte(info, b2);
		OneByte(info, b3);
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
		info.prefixSize = 0;
		info.instrSize = 0;

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

			case Instruction::clc: OneByte(info, 0xf8); break;
			case Instruction::cld: OneByte(info, 0xfc); break;
			case Instruction::cli: OneByte(info, 0xfa); break;
			case Instruction::clts: TwoByte(info, 0x0f, 0x06); break;
			case Instruction::cmc: OneByte(info, 0xf5); break;
			case Instruction::stc: OneByte(info, 0xf9); break;
			case Instruction::std: OneByte(info, 0xfd); break;
			case Instruction::sti: OneByte(info, 0xfb); break;

			case Instruction::cpuid: TwoByte(info, 0x0f, 0xa2); break;
			case Instruction::daa: OneByte(info, 0x27); break;
			case Instruction::das: OneByte(info, 0x2f); break;
			case Instruction::hlt: OneByte(info, 0xf4); break;
			case Instruction::int3: OneByte(info, 0xcc); break;
			case Instruction::into: OneByte(info, 0xce); break;
			case Instruction::int1: OneByte(info, 0xf1); break;
			case Instruction::invd: TwoByte(info, 0x0f, 0x08); break;
			case Instruction::iret: OneByte(info, 0xcf); break;
			case Instruction::iretd: TwoByte(info, 0x66, 0xcf); break;
			case Instruction::iretq: Invalid(); break;
			case Instruction::lahf: OneByte(info, 0x9f); break;
			case Instruction::sahf: OneByte(info, 0x9e); break;
			case Instruction::leave: OneByte(info, 0xc9); break;
			case Instruction::nop: OneByte(info, 0x90); break;
			case Instruction::rdmsr: TwoByte(info, 0x0f, 0x32); break;
			case Instruction::rdpmc: TwoByte(info, 0x0f, 0x33); break;
			case Instruction::rdtsc: TwoByte(info, 0x0f, 0x31); break;
			case Instruction::rdtscp: TriByte(info, 0x0f, 0x01, 0xf9); break;
			case Instruction::rsm: TwoByte(info, 0x0f, 0xaa); break;
			case Instruction::swapgs: Invalid(); break;
			case Instruction::syscall: Invalid(); break;
			case Instruction::sysret: Invalid(); break;
			case Instruction::sysretq: Invalid(); break;
			case Instruction::ud2: TwoByte(info, 0x0f, 0x0b); break;
			case Instruction::wait: OneByte(info, 0x9b); break;
			case Instruction::wbinvd: TwoByte(info, 0x0f, 0x09); break;
			case Instruction::wrmsr: TwoByte(info, 0x0f, 0x30); break;
			case Instruction::xlatb: OneByte(info, 0xd7); break;

			case Instruction::popa: OneByte(info, 0x61); break;
			case Instruction::popad: TwoByte(info, 0x66, 0x61); break;
			case Instruction::popf: OneByte(info, 0x9d); break;
			case Instruction::popfd: TwoByte(info, 0x66, 0x9d); break;
			case Instruction::popfq: Invalid(); break;
			case Instruction::pusha: OneByte(info, 0x60); break;
			case Instruction::pushad: TwoByte(info, 0x66, 0x60); break;
			case Instruction::pushf: OneByte(info, 0x9c); break;
			case Instruction::pushfd: TwoByte(info, 0x66, 0x9c); break;
			case Instruction::pushfq: Invalid(); break;

			case Instruction::cmpsb: OneByte(info, 0xa6); break;
			case Instruction::cmpsw: OneByte(info, 0xa7); break;
			case Instruction::cmpsd: TwoByte(info, 0x66, 0xa7); break;
			case Instruction::cmpsq: Invalid(); break;
			case Instruction::lodsb: OneByte(info, 0xac); break;
			case Instruction::lodsw: OneByte(info, 0xad); break;
			case Instruction::lodsd: TwoByte(info, 0x66, 0xad); break;
			case Instruction::lodsq: Invalid(); break;
			case Instruction::movsb: OneByte(info, 0xa4); break;
			case Instruction::movsw: OneByte(info, 0xa5); break;
			case Instruction::movsd: TwoByte(info, 0x66, 0xa5); break;
			case Instruction::movsq: Invalid(); break;
			case Instruction::scasb: OneByte(info, 0xae); break;
			case Instruction::scasw: OneByte(info, 0xaf); break;
			case Instruction::scasd: TwoByte(info, 0x66, 0xaf); break;
			case Instruction::scasq: Invalid(); break;
			case Instruction::stosb: OneByte(info, 0xaa); break;
			case Instruction::stosw: OneByte(info, 0xab); break;
			case Instruction::stosd: TwoByte(info, 0x66, 0xab); break;
			case Instruction::stosq: Invalid(); break;
			case Instruction::insb: OneByte(info, 0x6c); break;
			case Instruction::insw: OneByte(info, 0x6d); break;
			case Instruction::insd: TwoByte(info, 0x66, 0x6d); break;
			case Instruction::outsb: OneByte(info, 0x6e); break;
			case Instruction::outsw: OneByte(info, 0x6f); break;
			case Instruction::outsd: TwoByte(info, 0x66, 0x6f); break;
		}
	}

	void IntelAssembler::Assemble32(AnalyzeInfo& info)
	{
		info.prefixSize = 0;
		info.instrSize = 0;

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

			case Instruction::clc: OneByte(info, 0xf8); break;
			case Instruction::cld: OneByte(info, 0xfc); break;
			case Instruction::cli: OneByte(info, 0xfa); break;
			case Instruction::clts: TwoByte(info, 0x0f, 0x06); break;
			case Instruction::cmc: OneByte(info, 0xf5); break;
			case Instruction::stc: OneByte(info, 0xf9); break;
			case Instruction::std: OneByte(info, 0xfd); break;
			case Instruction::sti: OneByte(info, 0xfb); break;

			case Instruction::cpuid: TwoByte(info, 0x0f, 0xa2); break;
			case Instruction::daa: OneByte(info, 0x27); break;
			case Instruction::das: OneByte(info, 0x2f); break;
			case Instruction::hlt: OneByte(info, 0xf4); break;
			case Instruction::int3: OneByte(info, 0xcc); break;
			case Instruction::into: OneByte(info, 0xce); break;
			case Instruction::int1: OneByte(info, 0xf1); break;
			case Instruction::invd: TwoByte(info, 0x0f, 0x08); break;
			case Instruction::iret: TwoByte(info, 0x66, 0xcf); break;
			case Instruction::iretd: OneByte(info, 0xcf); break;
			case Instruction::iretq: Invalid(); break;
			case Instruction::lahf: OneByte(info, 0x9f); break;
			case Instruction::sahf: OneByte(info, 0x9e); break;
			case Instruction::leave: OneByte(info, 0xc9); break;
			case Instruction::nop: OneByte(info, 0x90); break;
			case Instruction::rdmsr: TwoByte(info, 0x0f, 0x32); break;
			case Instruction::rdpmc: TwoByte(info, 0x0f, 0x33); break;
			case Instruction::rdtsc: TwoByte(info, 0x0f, 0x31); break;
			case Instruction::rdtscp: TriByte(info, 0x0f, 0x01, 0xf9); break;
			case Instruction::rsm: TwoByte(info, 0x0f, 0xaa); break;
			case Instruction::swapgs: Invalid(); break;
			case Instruction::syscall: Invalid(); break;
			case Instruction::sysret: Invalid(); break;
			case Instruction::sysretq: Invalid(); break;
			case Instruction::ud2: TwoByte(info, 0x0f, 0x0b); break;
			case Instruction::wait: OneByte(info, 0x9b); break;
			case Instruction::wbinvd: TwoByte(info, 0x0f, 0x09); break;
			case Instruction::wrmsr: TwoByte(info, 0x0f, 0x30); break;
			case Instruction::xlatb: OneByte(info, 0xd7); break;

			case Instruction::popa: TwoByte(info, 0x66, 0x61); break;
			case Instruction::popad: OneByte(info, 0x61); break;
			case Instruction::popf: TwoByte(info, 0x66, 0x9d); break;
			case Instruction::popfd: OneByte(info, 0x9d); break;
			case Instruction::popfq: Invalid(); break;
			case Instruction::pusha: TwoByte(info, 0x66, 0x60); break;
			case Instruction::pushad: OneByte(info, 0x60); break;
			case Instruction::pushf: TwoByte(info, 0x66, 0x9c); break;
			case Instruction::pushfd: OneByte(info, 0x9c); break;
			case Instruction::pushfq: Invalid(); break;

			case Instruction::cmpsb: OneByte(info, 0xa6); break;
			case Instruction::cmpsw: TwoByte(info, 0x66, 0xa7); break;
			case Instruction::cmpsd: OneByte(info, 0xa7); break;
			case Instruction::cmpsq: Invalid(); break;
			case Instruction::lodsb: OneByte(info, 0xac); break;
			case Instruction::lodsw: TwoByte(info, 0x66, 0xad); break;
			case Instruction::lodsd: OneByte(info, 0xad); break;
			case Instruction::lodsq: Invalid(); break;
			case Instruction::movsb: OneByte(info, 0xa4); break;
			case Instruction::movsw: TwoByte(info, 0x66, 0xa5); break;
			case Instruction::movsd: OneByte(info, 0xa5); break;
			case Instruction::movsq: Invalid(); break;
			case Instruction::scasb: OneByte(info, 0xae); break;
			case Instruction::scasw: TwoByte(info, 0x66, 0xaf); break;
			case Instruction::scasd: OneByte(info, 0xaf); break;
			case Instruction::scasq: Invalid(); break;
			case Instruction::stosb: OneByte(info, 0xaa); break;
			case Instruction::stosw: TwoByte(info, 0x66, 0xab); break;
			case Instruction::stosd: OneByte(info, 0xab); break;
			case Instruction::stosq: Invalid(); break;
			case Instruction::insb: OneByte(info, 0x6c); break;
			case Instruction::insw: TwoByte(info, 0x66, 0x6d); break;
			case Instruction::insd: OneByte(info, 0x6d); break;
			case Instruction::outsb: OneByte(info, 0x6e); break;
			case Instruction::outsw: TwoByte(info, 0x66, 0x6f); break;
			case Instruction::outsd: OneByte(info, 0x6f); break;
		}
	}

	void IntelAssembler::Assemble64(AnalyzeInfo& info)
	{
		info.prefixSize = 0;
		info.instrSize = 0;

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

			case Instruction::clc: OneByte(info, 0xf8); break;
			case Instruction::cld: OneByte(info, 0xfc); break;
			case Instruction::cli: OneByte(info, 0xfa); break;
			case Instruction::clts: TwoByte(info, 0x0f, 0x06); break;
			case Instruction::cmc: OneByte(info, 0xf5); break;
			case Instruction::stc: OneByte(info, 0xf9); break;
			case Instruction::std: OneByte(info, 0xfd); break;
			case Instruction::sti: OneByte(info, 0xfb); break;

			case Instruction::cpuid: TwoByte(info, 0x0f, 0xa2); break;
			case Instruction::daa: Invalid(); break;
			case Instruction::das: Invalid(); break;
			case Instruction::hlt: OneByte(info, 0xf4); break;
			case Instruction::int3: OneByte(info, 0xcc); break;
			case Instruction::into: OneByte(info, 0xce); break;
			case Instruction::int1: OneByte(info, 0xf1); break;
			case Instruction::invd: TwoByte(info, 0x0f, 0x08); break;
			case Instruction::iret: TwoByte(info, 0x66, 0xcf); break;
			case Instruction::iretd: OneByte(info, 0xcf); break;
			case Instruction::iretq: TwoByte(info, 0x48, 0xcf); break;
			case Instruction::lahf: Invalid(); break;
			case Instruction::sahf: Invalid(); break;
			case Instruction::leave: OneByte(info, 0xc9); break;
			case Instruction::nop: OneByte(info, 0x90); break;
			case Instruction::rdmsr: TwoByte(info, 0x0f, 0x32); break;
			case Instruction::rdpmc: TwoByte(info, 0x0f, 0x33); break;
			case Instruction::rdtsc: TwoByte(info, 0x0f, 0x31); break;
			case Instruction::rdtscp: TriByte(info, 0x0f, 0x01, 0xf9); break;
			case Instruction::rsm: TwoByte(info, 0x0f, 0xaa); break;
			case Instruction::swapgs: TriByte(info, 0x0f, 0x01, 0xf8); break;
			case Instruction::syscall: TwoByte(info, 0x0f, 0x05); break;
			case Instruction::sysret: TwoByte(info, 0x0f, 0x07); break;
			case Instruction::sysretq: TriByte(info, 0x48, 0x0f, 0x07); break;
			case Instruction::ud2: TwoByte(info, 0x0f, 0x0b); break;
			case Instruction::wait: OneByte(info, 0x9b); break;
			case Instruction::wbinvd: TwoByte(info, 0x0f, 0x09); break;
			case Instruction::wrmsr: TwoByte(info, 0x0f, 0x30); break;
			case Instruction::xlatb: TwoByte(info, 0x48, 0xd7); break;

			case Instruction::popa: Invalid(); break;
			case Instruction::popad: Invalid(); break;
			case Instruction::popf: TwoByte(info, 0x66, 0x9d); break;
			case Instruction::popfd: Invalid(); break;
			case Instruction::popfq: OneByte(info, 0x9d); break;
			case Instruction::pusha: Invalid(); break;
			case Instruction::pushad: Invalid(); break;
			case Instruction::pushf: TwoByte(info, 0x66, 0x9c); break;
			case Instruction::pushfd: Invalid(); break;
			case Instruction::pushfq: OneByte(info, 0x9c); break;

			case Instruction::cmpsb: OneByte(info, 0xa6); break;
			case Instruction::cmpsw: TwoByte(info, 0x66, 0xa7); break;
			case Instruction::cmpsd: TwoByte(info, 0x67, 0xa7); break;
			case Instruction::cmpsq: TwoByte(info, 0x48, 0xa7); break;
			case Instruction::lodsb: OneByte(info, 0xac); break;
			case Instruction::lodsw: TwoByte(info, 0x66, 0xad); break;
			case Instruction::lodsd: TwoByte(info, 0x67, 0xad); break;
			case Instruction::lodsq: TwoByte(info, 0x48, 0xad); break;
			case Instruction::movsb: OneByte(info, 0xa4); break;
			case Instruction::movsw: TwoByte(info, 0x66, 0xa5); break;
			case Instruction::movsd: TwoByte(info, 0x67, 0xa5); break;
			case Instruction::movsq: TwoByte(info, 0x48, 0xa5); break;
			case Instruction::scasb: OneByte(info, 0xae); break;
			case Instruction::scasw: TwoByte(info, 0x66, 0xaf); break;
			case Instruction::scasd: TwoByte(info, 0x67, 0xaf); break;
			case Instruction::scasq: TwoByte(info, 0x48, 0xaf); break;
			case Instruction::stosb: OneByte(info, 0xaa); break;
			case Instruction::stosw: TwoByte(info, 0x66, 0xab); break;
			case Instruction::stosd: TwoByte(info, 0x67, 0xab); break;
			case Instruction::stosq: TwoByte(info, 0x48, 0xab); break;
			case Instruction::insb: OneByte(info, 0x6c); break;
			case Instruction::insw: TwoByte(info, 0x66, 0x6d); break;
			case Instruction::insd: TwoByte(info, 0x67, 0x6d); break;
			case Instruction::outsb: OneByte(info, 0x6e); break;
			case Instruction::outsw: TwoByte(info, 0x66, 0x6f); break;
			case Instruction::outsd: TwoByte(info, 0x67, 0x6f); break;
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


	template <> AnalyzeInfo& IntelAssembler::clc<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::clc;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::clc<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::clc;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::clc<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::clc;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cld<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cld;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cld<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cld;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cld<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cld;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cli<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cli;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cli<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cli;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cli<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cli;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::clts<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::clts;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::clts<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::clts;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::clts<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::clts;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cmc<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cmc;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cmc<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cmc;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cmc<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cmc;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::stc<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::stc;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::stc<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::stc;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::stc<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::stc;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::std<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::std;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::std<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::std;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::std<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::std;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::sti<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::sti;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::sti<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::sti;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::sti<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::sti;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cpuid<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cpuid;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cpuid<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cpuid;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cpuid<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::cpuid;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::daa<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::daa;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::daa<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::daa;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::daa<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::daa;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::das<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::das;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::das<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::das;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::das<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::das;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::hlt<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::hlt;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::hlt<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::hlt;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::hlt<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::hlt;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::int3<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::int3;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::int3<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::int3;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::int3<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::int3;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::into<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::into;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::into<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::into;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::into<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::into;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::int1<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::int1;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::int1<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::int1;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::int1<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::int1;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::invd<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::invd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::invd<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::invd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::invd<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::invd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::iret<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::iret;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::iret<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::iret;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::iret<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::iret;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::iretd<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::iretd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::iretd<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::iretd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::iretd<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::iretd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::iretq<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::iretq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::iretq<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::iretq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::iretq<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::iretq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::lahf<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::lahf;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::lahf<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::lahf;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::lahf<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::lahf;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::sahf<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::sahf;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::sahf<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::sahf;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::sahf<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::sahf;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::leave<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::leave;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::leave<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::leave;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::leave<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::leave;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::nop<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::nop;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::nop<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::nop;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::nop<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::nop;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::rdmsr<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::rdmsr;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::rdmsr<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::rdmsr;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::rdmsr<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::rdmsr;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::rdpmc<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::rdpmc;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::rdpmc<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::rdpmc;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::rdpmc<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::rdpmc;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::rdtsc<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::rdtsc;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::rdtsc<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::rdtsc;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::rdtsc<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::rdtsc;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::rdtscp<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::rdtscp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::rdtscp<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::rdtscp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::rdtscp<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::rdtscp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::rsm<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::rsm;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::rsm<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::rsm;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::rsm<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::rsm;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::swapgs<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::swapgs;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::swapgs<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::swapgs;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::swapgs<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::swapgs;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::syscall<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::syscall;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::syscall<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::syscall;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::syscall<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::syscall;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::sysret<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::sysret;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::sysret<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::sysret;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::sysret<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::sysret;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::sysretq<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::sysretq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::sysretq<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::sysretq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::sysretq<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::sysretq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::ud2<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::ud2;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::ud2<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::ud2;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::ud2<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::ud2;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::wait<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::wait;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::wait<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::wait;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::wait<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::wait;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::fwait<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::wait;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::fwait<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::wait;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::fwait<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::wait;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::wbinvd<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::wbinvd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::wbinvd<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::wbinvd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::wbinvd<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::wbinvd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::wrmsr<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::wrmsr;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::wrmsr<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::wrmsr;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::wrmsr<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::wrmsr;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::xlatb<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::xlatb;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::xlatb<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::xlatb;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::xlatb<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::xlatb;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::popa<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::popa;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::popa<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::popa;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::popa<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::popa;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::popad<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::popad;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::popad<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::popad;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::popad<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::popad;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::popf<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::popf;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::popf<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::popf;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::popf<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::popf;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::popfd<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::popfd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::popfd<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::popfd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::popfd<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::popfd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::popfq<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::popfq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::popfq<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::popfq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::popfq<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::popfq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::pusha<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::pusha;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::pusha<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::pusha;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::pusha<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::pusha;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::pushad<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::pushad;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::pushad<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::pushad;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::pushad<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::pushad;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::pushf<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::pushf;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::pushf<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::pushf;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::pushf<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::pushf;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::pushfd<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::pushfd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::pushfd<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::pushfd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::pushfd<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::pushfd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::pushfq<16>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::pushfq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::pushfq<32>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::pushfq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::pushfq<64>()
	{
		AnalyzeInfo info;
		info.instr = Instruction::pushfq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cmpsb<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::cmpsb;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cmpsb<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::cmpsb;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cmpsb<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::cmpsb;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cmpsw<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::cmpsw;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cmpsw<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::cmpsw;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cmpsw<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::cmpsw;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cmpsd<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::cmpsd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cmpsd<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::cmpsd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cmpsd<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::cmpsd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cmpsq<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::cmpsq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cmpsq<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::cmpsq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::cmpsq<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::cmpsq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::lodsb<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::lodsb;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::lodsb<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::lodsb;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::lodsb<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::lodsb;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::lodsw<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::lodsw;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::lodsw<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::lodsw;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::lodsw<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::lodsw;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::lodsd<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::lodsd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::lodsd<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::lodsd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::lodsd<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::lodsd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::lodsq<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::lodsq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::lodsq<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::lodsq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::lodsq<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::lodsq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::movsb<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::movsb;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::movsb<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::movsb;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::movsb<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::movsb;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::movsw<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::movsw;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::movsw<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::movsw;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::movsw<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::movsw;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::movsd<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::movsd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::movsd<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::movsd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::movsd<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::movsd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::movsq<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::movsq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::movsq<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::movsq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::movsq<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::movsq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::scasb<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::scasb;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::scasb<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::scasb;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::scasb<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::scasb;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::scasw<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::scasw;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::scasw<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::scasw;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::scasw<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::scasw;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::scasd<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::scasd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::scasd<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::scasd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::scasd<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::scasd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::scasq<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::scasq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::scasq<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::scasq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::scasq<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::scasq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::stosb<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::stosb;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::stosb<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::stosb;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::stosb<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::stosb;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::stosw<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::stosw;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::stosw<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::stosw;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::stosw<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::stosw;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::stosd<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::stosd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::stosd<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::stosd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::stosd<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::stosd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::stosq<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::stosq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::stosq<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::stosq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::stosq<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::stosq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::insb<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::insb;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::insb<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::insb;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::insb<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::insb;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::insw<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::insw;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::insw<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::insw;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::insw<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::insw;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::insd<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::insd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::insd<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::insd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::insd<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::insd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::outsb<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::outsb;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::outsb<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::outsb;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::outsb<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::outsb;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::outsw<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::outsw;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::outsw<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::outsw;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::outsw<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::outsw;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::outsd<16>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::outsd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::outsd<32>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::outsd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::outsd<64>(Prefix pre)
	{
		AnalyzeInfo info;
		if (pre != Prefix::NoPrefix)
		{
			info.prefixes[0] = pre;
			info.numPrefixes = 1;
		}
		info.instr = Instruction::outsd;
		Assemble64(info);
		return info;
	}

#pragma endregion "Quick helpers"

}
