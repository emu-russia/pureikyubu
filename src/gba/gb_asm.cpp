// The LR35902 code emitter (see gb_asm.h for why it exists and where the encodings come from).
//
// Every method here is a direct transcription of one row of the Pan Docs instruction tables, so
// the encodings stay checkable against the specification: the arithmetic family is a base byte
// plus the r8 code, the CB family is the prefix plus a base plus the r8 code, and the branches are
// their fixed opcodes with a little-endian address or a signed offset.

#include "gb_asm.h"

#include <cstdio>

namespace GBA
{
	namespace GbAsm
	{
		namespace
		{
			/// <summary>Two hex digits, for the listing.</summary>
			std::string HexByte(uint8_t value)
			{
				char text[8];
				snprintf(text, sizeof(text), "%02X", value);
				return text;
			}

			/// <summary>A 16-bit address, for the listing.</summary>
			std::string HexWord(uint16_t value)
			{
				char text[8];
				snprintf(text, sizeof(text), "%04X", value);
				return text;
			}
		}

		// -----------------------------------------------------------------------------------
		// Location and data
		// -----------------------------------------------------------------------------------

		void Assembler::Org(uint16_t address)
		{
			pc = address;
			if (code.size() < address)
				code.resize(address, 0x00);
		}

		void Assembler::Label(const std::string& name)
		{
			if (labels.find(name) != labels.end())
				throw std::runtime_error("gb asm: label defined twice: " + name);
			labels[name] = pc;
		}

		void Assembler::Reserve(size_t bytes)
		{
			if (code.size() < (size_t)pc + bytes)
				code.resize((size_t)pc + bytes, 0x00);
		}

		void Assembler::Emit(uint8_t value)
		{
			Reserve(1);
			code[pc] = value;
			pc = (uint16_t)(pc + 1);
		}

		void Assembler::Record(const std::string& text)
		{
			listing += HexWord((uint16_t)(pc - 1));
			listing += ": ";
			listing += text;
			listing += "\n";
			instructionCount++;
		}

		void Assembler::EmitInstruction(uint8_t value, const std::string& text)
		{
			Emit(value);
			Record(text);
		}

		void Assembler::EmitInstruction2(uint8_t first, uint8_t second, const std::string& text)
		{
			Emit(first);
			Emit(second);
			listing += HexWord((uint16_t)(pc - 2));
			listing += ": ";
			listing += text;
			listing += "\n";
			instructionCount++;
		}

		void Assembler::Data8(uint8_t value)
		{
			Emit(value);
		}

		void Assembler::Data16(uint16_t value)
		{
			// The Game Boy is little-endian: the low byte comes first.
			Emit((uint8_t)(value & 0xFF));
			Emit((uint8_t)(value >> 8));
		}

		void Assembler::Data16(uint16_t value, const std::string& text)
		{
			uint16_t at = pc;
			Data16(value);
			listing += HexWord(at);
			listing += ": ";
			listing += text;
			listing += "\n";
		}

		void Assembler::DataBytes(const uint8_t* data, size_t count)
		{
			for (size_t i = 0; i < count; i++)
				Emit(data[i]);
		}

		void Assembler::Text(const std::string& ascii)
		{
			for (char c : ascii)
				Emit((uint8_t)c);
		}

		void Assembler::Fill(uint8_t value, uint16_t count)
		{
			for (uint16_t i = 0; i < count; i++)
				Emit(value);
		}

		// -----------------------------------------------------------------------------------
		// The 8-bit load family
		// -----------------------------------------------------------------------------------

		void Assembler::Ld(R8 dest, R8 source)
		{
			// LD r8,r8: 01 ddd sss. The 0x76 "ld (hl),(hl)" slot is HALT instead.
			uint8_t opcode = (uint8_t)(0x40 + ((uint8_t)dest << 3) + (uint8_t)source);
			if (opcode == 0x76)
				throw std::runtime_error("gb asm: LD (HL),(HL) is the HALT opcode");
			EmitInstruction(opcode, "ld r" + std::to_string((int)dest) + ",r" + std::to_string((int)source));
		}

		void Assembler::Ld(R8 dest, uint8_t value)
		{
			// LD r8,n8: 00 ddd 110.
			uint8_t opcode = (uint8_t)(0x06 + ((uint8_t)dest << 3));
			EmitInstruction(opcode, "ld r" + std::to_string((int)dest) + ",n8");
			Emit(value);
		}

		void Assembler::LdA16(uint16_t address)
		{
			EmitInstruction(0xFA, "ld a,(" + HexWord(address) + ")");
			Emit((uint8_t)(address & 0xFF));
			Emit((uint8_t)(address >> 8));
		}

		void Assembler::Ld16A(uint16_t address)
		{
			EmitInstruction(0xEA, "ld (" + HexWord(address) + "),a");
			Emit((uint8_t)(address & 0xFF));
			Emit((uint8_t)(address >> 8));
		}

		void Assembler::LdA8(uint8_t offset)
		{
			// LDH (n8),A: 0xE0, the high byte of the address is 0xFF.
			EmitInstruction(0xE0, "ldh ($FF" + HexByte(offset) + "),a");
			Emit(offset);
		}

		void Assembler::Ld8A(uint8_t offset)
		{
			// LDH A,(n8): 0xF0.
			EmitInstruction(0xF0, "ldh a,($FF" + HexByte(offset) + ")");
			Emit(offset);
		}

		void Assembler::LdAC() { EmitInstruction(0xE2, "ld (c),a"); }
		void Assembler::LdCA() { EmitInstruction(0xF2, "ld a,(c)"); }

		void Assembler::Ld16SP(uint16_t address)
		{
			// LD (n16),SP: 0x08. One of the "undocumented" opcodes in the sense that the
			// mnemonic table does not spell it out, but it is a documented instruction.
			EmitInstruction(0x08, "ld (" + HexWord(address) + "),sp");
			Emit((uint8_t)(address & 0xFF));
			Emit((uint8_t)(address >> 8));
		}

		void Assembler::Ld16(R16 dest, uint16_t value)
		{
			// LD r16,n16: 00 rr 0001.
			uint8_t opcode = (uint8_t)(0x01 + ((uint8_t)dest << 4));
			EmitInstruction(opcode, "ld r16,n16");
			Emit((uint8_t)(value & 0xFF));
			Emit((uint8_t)(value >> 8));
		}

		void Assembler::Ld16(R16 dest, const std::string& label)
		{
			// The same instruction with a label's address as the immediate, so a boot ROM can
			// point a register at one of its own tables without hard coded addresses.
			absoluteFixups.push_back(AbsoluteFixup{ code.size() + 1, label });
			EmitInstruction((uint8_t)(0x01 + ((uint8_t)dest << 4)), "ld r16," + label);
			Emit(0);
			Emit(0);
		}

		void Assembler::LdSPHL() { EmitInstruction(0xF9, "ld sp,hl"); }

		void Assembler::LdHLSPe(int8_t offset)
		{
			EmitInstruction(0xF8, "ld hl,sp+e8");
			Emit((uint8_t)offset);
		}

		void Assembler::LdAHLI() { EmitInstruction(0x22, "ld (hl+),a"); }
		void Assembler::LdAHLD() { EmitInstruction(0x32, "ld (hl-),a"); }
		void Assembler::LdAHLA() { EmitInstruction(0x2A, "ld a,(hl+)"); }
		void Assembler::LdAHDA() { EmitInstruction(0x3A, "ld a,(hl-)"); }

		// The BC/DE memory forms are not part of the r8 table: the LR35902 only has them for the
		// accumulator (Pan Docs "Block 0": ld [r16mem],a / ld a,[r16mem]).
		void Assembler::LdADE() { EmitInstruction(0x1A, "ld a,(de)"); }
		void Assembler::LdDEA() { EmitInstruction(0x12, "ld (de),a"); }
		void Assembler::LdABC() { EmitInstruction(0x0A, "ld a,(bc)"); }
		void Assembler::LdBCA() { EmitInstruction(0x02, "ld (bc),a"); }

		// -----------------------------------------------------------------------------------
		// 16-bit arithmetic and the stack
		// -----------------------------------------------------------------------------------

		void Assembler::AddHL(R16 source) { EmitInstruction((uint8_t)(0x09 + ((uint8_t)source << 4)), "add hl,r16"); }
		void Assembler::AddSPe(int8_t offset) { EmitInstruction(0xE8, "add sp,e8"); Emit((uint8_t)offset); }
		void Assembler::Inc16(R16 reg) { EmitInstruction((uint8_t)(0x03 + ((uint8_t)reg << 4)), "inc r16"); }
		void Assembler::Dec16(R16 reg) { EmitInstruction((uint8_t)(0x0B + ((uint8_t)reg << 4)), "dec r16"); }
		void Assembler::Push(R16Stk pair) { EmitInstruction((uint8_t)(0xC5 + ((uint8_t)pair << 4)), "push r16stk"); }
		void Assembler::Pop(R16Stk pair) { EmitInstruction((uint8_t)(0xC1 + ((uint8_t)pair << 4)), "pop r16stk"); }

		// -----------------------------------------------------------------------------------
		// 8-bit arithmetic
		// -----------------------------------------------------------------------------------

		void Assembler::Add(R8 source) { EmitInstruction((uint8_t)(0x80 + (uint8_t)source), "add a,r8"); }
		void Assembler::Add(uint8_t value) { EmitInstruction(0xC6, "add a,n8"); Emit(value); }
		void Assembler::Adc(R8 source) { EmitInstruction((uint8_t)(0x88 + (uint8_t)source), "adc a,r8"); }
		void Assembler::Adc(uint8_t value) { EmitInstruction(0xCE, "adc a,n8"); Emit(value); }
		void Assembler::Sub(R8 source) { EmitInstruction((uint8_t)(0x90 + (uint8_t)source), "sub r8"); }
		void Assembler::Sub(uint8_t value) { EmitInstruction(0xD6, "sub n8"); Emit(value); }
		void Assembler::Sbc(R8 source) { EmitInstruction((uint8_t)(0x98 + (uint8_t)source), "sbc a,r8"); }
		void Assembler::Sbc(uint8_t value) { EmitInstruction(0xDE, "sbc a,n8"); Emit(value); }
		void Assembler::And(R8 source) { EmitInstruction((uint8_t)(0xA0 + (uint8_t)source), "and a,r8"); }
		void Assembler::And(uint8_t value) { EmitInstruction(0xE6, "and n8"); Emit(value); }
		void Assembler::Xor(R8 source) { EmitInstruction((uint8_t)(0xA8 + (uint8_t)source), "xor a,r8"); }
		void Assembler::Xor(uint8_t value) { EmitInstruction(0xEE, "xor n8"); Emit(value); }
		void Assembler::Or(R8 source) { EmitInstruction((uint8_t)(0xB0 + (uint8_t)source), "or a,r8"); }
		void Assembler::Or(uint8_t value) { EmitInstruction(0xF6, "or n8"); Emit(value); }
		void Assembler::Cp(R8 source) { EmitInstruction((uint8_t)(0xB8 + (uint8_t)source), "cp a,r8"); }
		void Assembler::Cp(uint8_t value) { EmitInstruction(0xFE, "cp n8"); Emit(value); }
		void Assembler::Inc(R8 reg) { EmitInstruction((uint8_t)(0x04 + ((uint8_t)reg << 3)), "inc r8"); }
		void Assembler::Dec(R8 reg) { EmitInstruction((uint8_t)(0x05 + ((uint8_t)reg << 3)), "dec r8"); }

		// -----------------------------------------------------------------------------------
		// The accumulator-only operations
		// -----------------------------------------------------------------------------------

		void Assembler::Daa() { EmitInstruction(0x27, "daa"); }
		void Assembler::Cpl() { EmitInstruction(0x2F, "cpl"); }
		void Assembler::Scf() { EmitInstruction(0x37, "scf"); }
		void Assembler::Ccf() { EmitInstruction(0x3F, "ccf"); }
		void Assembler::Rlca() { EmitInstruction(0x07, "rlca"); }
		void Assembler::Rrca() { EmitInstruction(0x0F, "rrca"); }
		void Assembler::Rla() { EmitInstruction(0x17, "rla"); }
		void Assembler::Rra() { EmitInstruction(0x1F, "rra"); }

		// -----------------------------------------------------------------------------------
		// The CB prefix family
		// -----------------------------------------------------------------------------------

		void Assembler::Rlc(R8 reg) { EmitInstruction2(0xCB, (uint8_t)(0x00 + (uint8_t)reg), "rlc r8"); }
		void Assembler::Rrc(R8 reg) { EmitInstruction2(0xCB, (uint8_t)(0x08 + (uint8_t)reg), "rrc r8"); }
		void Assembler::Rl(R8 reg) { EmitInstruction2(0xCB, (uint8_t)(0x10 + (uint8_t)reg), "rl r8"); }
		void Assembler::Rr(R8 reg) { EmitInstruction2(0xCB, (uint8_t)(0x18 + (uint8_t)reg), "rr r8"); }
		void Assembler::Sla(R8 reg) { EmitInstruction2(0xCB, (uint8_t)(0x20 + (uint8_t)reg), "sla r8"); }
		void Assembler::Sra(R8 reg) { EmitInstruction2(0xCB, (uint8_t)(0x28 + (uint8_t)reg), "sra r8"); }
		void Assembler::Swap(R8 reg) { EmitInstruction2(0xCB, (uint8_t)(0x30 + (uint8_t)reg), "swap r8"); }
		void Assembler::Srl(R8 reg) { EmitInstruction2(0xCB, (uint8_t)(0x38 + (uint8_t)reg), "srl r8"); }

		void Assembler::Bit(int bit, R8 reg)
		{
			if (bit < 0 || bit > 7)
				throw std::runtime_error("gb asm: BIT bit index out of range");
			EmitInstruction2(0xCB, (uint8_t)(0x40 + (bit << 3) + (uint8_t)reg), "bit " + std::to_string(bit) + ",r8");
		}

		void Assembler::Res(int bit, R8 reg)
		{
			if (bit < 0 || bit > 7)
				throw std::runtime_error("gb asm: RES bit index out of range");
			EmitInstruction2(0xCB, (uint8_t)(0x80 + (bit << 3) + (uint8_t)reg), "res " + std::to_string(bit) + ",r8");
		}

		void Assembler::Set(int bit, R8 reg)
		{
			if (bit < 0 || bit > 7)
				throw std::runtime_error("gb asm: SET bit index out of range");
			EmitInstruction2(0xCB, (uint8_t)(0xC0 + (bit << 3) + (uint8_t)reg), "set " + std::to_string(bit) + ",r8");
		}

		// -----------------------------------------------------------------------------------
		// Control flow
		// -----------------------------------------------------------------------------------

		void Assembler::Nop() { EmitInstruction(0x00, "nop"); }
		void Assembler::Halt() { EmitInstruction(0x76, "halt"); }

		void Assembler::Stop()
		{
			// STOP is a two-byte instruction whose second byte the hardware does not always
			// ignore (Pan Docs "Reducing Power Consumption"): 0x10 0x00 is the canonical form.
			EmitInstruction2(0x10, 0x00, "stop");
		}

		void Assembler::Di() { EmitInstruction(0xF3, "di"); }
		void Assembler::Ei() { EmitInstruction(0xFB, "ei"); }
		void Assembler::JpHL() { EmitInstruction(0xE9, "jp hl"); }

		void Assembler::Jp(uint16_t address)
		{
			EmitInstruction(0xC3, "jp " + HexWord(address));
			Emit((uint8_t)(address & 0xFF));
			Emit((uint8_t)(address >> 8));
		}

		void Assembler::Jp(const std::string& label)
		{
			absoluteFixups.push_back(AbsoluteFixup{ code.size() + 1, label });
			EmitInstruction(0xC3, "jp " + label);
			Emit(0);
			Emit(0);
		}

		void Assembler::Jp(Cond condition, uint16_t address)
		{
			// JP cc,n16: 11 ccc 010.
			EmitInstruction((uint8_t)(0xC2 + ((uint8_t)condition << 3)), "jp cc," + HexWord(address));
			Emit((uint8_t)(address & 0xFF));
			Emit((uint8_t)(address >> 8));
		}

		void Assembler::Jp(Cond condition, const std::string& label)
		{
			absoluteFixups.push_back(AbsoluteFixup{ code.size() + 1, label });
			EmitInstruction((uint8_t)(0xC2 + ((uint8_t)condition << 3)), "jp cc," + label);
			Emit(0);
			Emit(0);
		}

		void Assembler::Jr(int8_t offset)
		{
			EmitInstruction(0x18, "jr " + std::to_string((int)offset));
			Emit((uint8_t)offset);
		}

		void Assembler::Jr(const std::string& label)
		{
			relativeFixups.push_back(RelativeFixup{ code.size() + 1, label });
			EmitInstruction(0x18, "jr " + label);
			Emit(0);
		}

		void Assembler::Jr(Cond condition, int8_t offset)
		{
			EmitInstruction((uint8_t)(0x20 + ((uint8_t)condition << 3)), "jr cc," + std::to_string((int)offset));
			Emit((uint8_t)offset);
		}

		void Assembler::Jr(Cond condition, const std::string& label)
		{
			relativeFixups.push_back(RelativeFixup{ code.size() + 1, label });
			EmitInstruction((uint8_t)(0x20 + ((uint8_t)condition << 3)), "jr cc," + label);
			Emit(0);
		}

		void Assembler::Call(uint16_t address)
		{
			EmitInstruction(0xCD, "call " + HexWord(address));
			Emit((uint8_t)(address & 0xFF));
			Emit((uint8_t)(address >> 8));
		}

		void Assembler::Call(const std::string& label)
		{
			absoluteFixups.push_back(AbsoluteFixup{ code.size() + 1, label });
			EmitInstruction(0xCD, "call " + label);
			Emit(0);
			Emit(0);
		}

		void Assembler::Call(Cond condition, uint16_t address)
		{
			// CALL cc,n16: 11 ccc 100.
			EmitInstruction((uint8_t)(0xC4 + ((uint8_t)condition << 3)), "call cc," + HexWord(address));
			Emit((uint8_t)(address & 0xFF));
			Emit((uint8_t)(address >> 8));
		}

		void Assembler::Call(Cond condition, const std::string& label)
		{
			absoluteFixups.push_back(AbsoluteFixup{ code.size() + 1, label });
			EmitInstruction((uint8_t)(0xC4 + ((uint8_t)condition << 3)), "call cc," + label);
			Emit(0);
			Emit(0);
		}

		void Assembler::Ret() { EmitInstruction(0xC9, "ret"); }
		void Assembler::Ret(Cond condition) { EmitInstruction((uint8_t)(0xC0 + ((uint8_t)condition << 3)), "ret cc"); }
		void Assembler::Reti() { EmitInstruction(0xD9, "reti"); }

		void Assembler::Rst(uint8_t vector)
		{
			if ((vector & 0x07) != 0 || vector > 0x38)
				throw std::runtime_error("gb asm: RST vector must be a multiple of 8 below 0x40");
			EmitInstruction((uint8_t)(0xC7 + vector), "rst " + HexWord(vector));
		}

		// -----------------------------------------------------------------------------------
		// Output
		// -----------------------------------------------------------------------------------

		std::vector<uint8_t> Assembler::TakeImage(uint32_t size, uint8_t pad)
		{
			for (const auto& fixup : absoluteFixups)
			{
				auto found = labels.find(fixup.label);
				if (found == labels.end())
					throw std::runtime_error("gb asm: undefined label: " + fixup.label);
				uint16_t target = found->second;
				if (fixup.position + 1 >= code.size())
					throw std::runtime_error("gb asm: fixup outside the image");
				code[fixup.position] = (uint8_t)(target & 0xFF);
				code[fixup.position + 1] = (uint8_t)(target >> 8);
			}

			for (const auto& fixup : relativeFixups)
			{
				auto found = labels.find(fixup.label);
				if (found == labels.end())
					throw std::runtime_error("gb asm: undefined label: " + fixup.label);
				// JR measures from the byte after the instruction, and the offset is signed.
				int delta = (int)found->second - (int)(fixup.position + 1);
				if (delta < -128 || delta > 127)
					throw std::runtime_error("gb asm: JR out of range: " + fixup.label);
				code[fixup.position] = (uint8_t)(int8_t)delta;
			}

			std::vector<uint8_t> image = code;

			if (size != 0)
			{
				image.resize(size, pad);
				for (size_t i = code.size(); i < size; i++)
					image[i] = pad;
			}

			return image;
		}
	}
}
