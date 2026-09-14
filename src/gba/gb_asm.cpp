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
			std::string HexByte(u8 value)
			{
				char text[8];
				snprintf(text, sizeof(text), "%02X", value);
				return text;
			}

			/// <summary>A 16-bit address, for the listing.</summary>
			std::string HexWord(u16 value)
			{
				char text[8];
				snprintf(text, sizeof(text), "%04X", value);
				return text;
			}
		}

		// -----------------------------------------------------------------------------------
		// Location and data
		// -----------------------------------------------------------------------------------

		void Assembler::Org(u16 address)
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

		void Assembler::Emit(u8 value)
		{
			Reserve(1);
			code[pc] = value;
			pc = (u16)(pc + 1);
		}

		void Assembler::Record(const std::string& text)
		{
			listing += HexWord((u16)(pc - 1));
			listing += ": ";
			listing += text;
			listing += "\n";
			instructionCount++;
		}

		void Assembler::EmitInstruction(u8 value, const std::string& text)
		{
			Emit(value);
			Record(text);
		}

		void Assembler::EmitInstruction2(u8 first, u8 second, const std::string& text)
		{
			Emit(first);
			Emit(second);
			listing += HexWord((u16)(pc - 2));
			listing += ": ";
			listing += text;
			listing += "\n";
			instructionCount++;
		}

		void Assembler::Data8(u8 value)
		{
			Emit(value);
		}

		void Assembler::Data16(u16 value)
		{
			// The Game Boy is little-endian: the low byte comes first.
			Emit((u8)(value & 0xFF));
			Emit((u8)(value >> 8));
		}

		void Assembler::Data16(u16 value, const std::string& text)
		{
			u16 at = pc;
			Data16(value);
			listing += HexWord(at);
			listing += ": ";
			listing += text;
			listing += "\n";
		}

		void Assembler::DataBytes(const u8* data, size_t count)
		{
			for (size_t i = 0; i < count; i++)
				Emit(data[i]);
		}

		void Assembler::Text(const std::string& ascii)
		{
			for (char c : ascii)
				Emit((u8)c);
		}

		void Assembler::Fill(u8 value, u16 count)
		{
			for (u16 i = 0; i < count; i++)
				Emit(value);
		}

		// -----------------------------------------------------------------------------------
		// The 8-bit load family
		// -----------------------------------------------------------------------------------

		void Assembler::Ld(R8 dest, R8 source)
		{
			// LD r8,r8: 01 ddd sss. The 0x76 "ld (hl),(hl)" slot is HALT instead.
			u8 opcode = (u8)(0x40 + ((u8)dest << 3) + (u8)source);
			if (opcode == 0x76)
				throw std::runtime_error("gb asm: LD (HL),(HL) is the HALT opcode");
			EmitInstruction(opcode, "ld r" + std::to_string((int)dest) + ",r" + std::to_string((int)source));
		}

		void Assembler::Ld(R8 dest, u8 value)
		{
			// LD r8,n8: 00 ddd 110.
			u8 opcode = (u8)(0x06 + ((u8)dest << 3));
			EmitInstruction(opcode, "ld r" + std::to_string((int)dest) + ",n8");
			Emit(value);
		}

		void Assembler::LdA16(u16 address)
		{
			EmitInstruction(0xFA, "ld a,(" + HexWord(address) + ")");
			Emit((u8)(address & 0xFF));
			Emit((u8)(address >> 8));
		}

		void Assembler::Ld16A(u16 address)
		{
			EmitInstruction(0xEA, "ld (" + HexWord(address) + "),a");
			Emit((u8)(address & 0xFF));
			Emit((u8)(address >> 8));
		}

		void Assembler::LdA8(u8 offset)
		{
			// LDH (n8),A: 0xE0, the high byte of the address is 0xFF.
			EmitInstruction(0xE0, "ldh ($FF" + HexByte(offset) + "),a");
			Emit(offset);
		}

		void Assembler::Ld8A(u8 offset)
		{
			// LDH A,(n8): 0xF0.
			EmitInstruction(0xF0, "ldh a,($FF" + HexByte(offset) + ")");
			Emit(offset);
		}

		void Assembler::LdAC() { EmitInstruction(0xE2, "ld (c),a"); }
		void Assembler::LdCA() { EmitInstruction(0xF2, "ld a,(c)"); }

		void Assembler::Ld16SP(u16 address)
		{
			// LD (n16),SP: 0x08. One of the "undocumented" opcodes in the sense that the
			// mnemonic table does not spell it out, but it is a documented instruction.
			EmitInstruction(0x08, "ld (" + HexWord(address) + "),sp");
			Emit((u8)(address & 0xFF));
			Emit((u8)(address >> 8));
		}

		void Assembler::Ld16(R16 dest, u16 value)
		{
			// LD r16,n16: 00 rr 0001.
			u8 opcode = (u8)(0x01 + ((u8)dest << 4));
			EmitInstruction(opcode, "ld r16,n16");
			Emit((u8)(value & 0xFF));
			Emit((u8)(value >> 8));
		}

		void Assembler::Ld16(R16 dest, const std::string& label)
		{
			// The same instruction with a label's address as the immediate, so a boot ROM can
			// point a register at one of its own tables without hard coded addresses.
			absoluteFixups.push_back(AbsoluteFixup{ code.size() + 1, label });
			EmitInstruction((u8)(0x01 + ((u8)dest << 4)), "ld r16," + label);
			Emit(0);
			Emit(0);
		}

		void Assembler::LdSPHL() { EmitInstruction(0xF9, "ld sp,hl"); }

		void Assembler::LdHLSPe(s8 offset)
		{
			EmitInstruction(0xF8, "ld hl,sp+e8");
			Emit((u8)offset);
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

		void Assembler::AddHL(R16 source) { EmitInstruction((u8)(0x09 + ((u8)source << 4)), "add hl,r16"); }
		void Assembler::AddSPe(s8 offset) { EmitInstruction(0xE8, "add sp,e8"); Emit((u8)offset); }
		void Assembler::Inc16(R16 reg) { EmitInstruction((u8)(0x03 + ((u8)reg << 4)), "inc r16"); }
		void Assembler::Dec16(R16 reg) { EmitInstruction((u8)(0x0B + ((u8)reg << 4)), "dec r16"); }
		void Assembler::Push(R16Stk pair) { EmitInstruction((u8)(0xC5 + ((u8)pair << 4)), "push r16stk"); }
		void Assembler::Pop(R16Stk pair) { EmitInstruction((u8)(0xC1 + ((u8)pair << 4)), "pop r16stk"); }

		// -----------------------------------------------------------------------------------
		// 8-bit arithmetic
		// -----------------------------------------------------------------------------------

		void Assembler::Add(R8 source) { EmitInstruction((u8)(0x80 + (u8)source), "add a,r8"); }
		void Assembler::Add(u8 value) { EmitInstruction(0xC6, "add a,n8"); Emit(value); }
		void Assembler::Adc(R8 source) { EmitInstruction((u8)(0x88 + (u8)source), "adc a,r8"); }
		void Assembler::Adc(u8 value) { EmitInstruction(0xCE, "adc a,n8"); Emit(value); }
		void Assembler::Sub(R8 source) { EmitInstruction((u8)(0x90 + (u8)source), "sub r8"); }
		void Assembler::Sub(u8 value) { EmitInstruction(0xD6, "sub n8"); Emit(value); }
		void Assembler::Sbc(R8 source) { EmitInstruction((u8)(0x98 + (u8)source), "sbc a,r8"); }
		void Assembler::Sbc(u8 value) { EmitInstruction(0xDE, "sbc a,n8"); Emit(value); }
		void Assembler::And(R8 source) { EmitInstruction((u8)(0xA0 + (u8)source), "and a,r8"); }
		void Assembler::And(u8 value) { EmitInstruction(0xE6, "and n8"); Emit(value); }
		void Assembler::Xor(R8 source) { EmitInstruction((u8)(0xA8 + (u8)source), "xor a,r8"); }
		void Assembler::Xor(u8 value) { EmitInstruction(0xEE, "xor n8"); Emit(value); }
		void Assembler::Or(R8 source) { EmitInstruction((u8)(0xB0 + (u8)source), "or a,r8"); }
		void Assembler::Or(u8 value) { EmitInstruction(0xF6, "or n8"); Emit(value); }
		void Assembler::Cp(R8 source) { EmitInstruction((u8)(0xB8 + (u8)source), "cp a,r8"); }
		void Assembler::Cp(u8 value) { EmitInstruction(0xFE, "cp n8"); Emit(value); }
		void Assembler::Inc(R8 reg) { EmitInstruction((u8)(0x04 + ((u8)reg << 3)), "inc r8"); }
		void Assembler::Dec(R8 reg) { EmitInstruction((u8)(0x05 + ((u8)reg << 3)), "dec r8"); }

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

		void Assembler::Rlc(R8 reg) { EmitInstruction2(0xCB, (u8)(0x00 + (u8)reg), "rlc r8"); }
		void Assembler::Rrc(R8 reg) { EmitInstruction2(0xCB, (u8)(0x08 + (u8)reg), "rrc r8"); }
		void Assembler::Rl(R8 reg) { EmitInstruction2(0xCB, (u8)(0x10 + (u8)reg), "rl r8"); }
		void Assembler::Rr(R8 reg) { EmitInstruction2(0xCB, (u8)(0x18 + (u8)reg), "rr r8"); }
		void Assembler::Sla(R8 reg) { EmitInstruction2(0xCB, (u8)(0x20 + (u8)reg), "sla r8"); }
		void Assembler::Sra(R8 reg) { EmitInstruction2(0xCB, (u8)(0x28 + (u8)reg), "sra r8"); }
		void Assembler::Swap(R8 reg) { EmitInstruction2(0xCB, (u8)(0x30 + (u8)reg), "swap r8"); }
		void Assembler::Srl(R8 reg) { EmitInstruction2(0xCB, (u8)(0x38 + (u8)reg), "srl r8"); }

		void Assembler::Bit(int bit, R8 reg)
		{
			if (bit < 0 || bit > 7)
				throw std::runtime_error("gb asm: BIT bit index out of range");
			EmitInstruction2(0xCB, (u8)(0x40 + (bit << 3) + (u8)reg), "bit " + std::to_string(bit) + ",r8");
		}

		void Assembler::Res(int bit, R8 reg)
		{
			if (bit < 0 || bit > 7)
				throw std::runtime_error("gb asm: RES bit index out of range");
			EmitInstruction2(0xCB, (u8)(0x80 + (bit << 3) + (u8)reg), "res " + std::to_string(bit) + ",r8");
		}

		void Assembler::Set(int bit, R8 reg)
		{
			if (bit < 0 || bit > 7)
				throw std::runtime_error("gb asm: SET bit index out of range");
			EmitInstruction2(0xCB, (u8)(0xC0 + (bit << 3) + (u8)reg), "set " + std::to_string(bit) + ",r8");
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

		void Assembler::Jp(u16 address)
		{
			EmitInstruction(0xC3, "jp " + HexWord(address));
			Emit((u8)(address & 0xFF));
			Emit((u8)(address >> 8));
		}

		void Assembler::Jp(const std::string& label)
		{
			absoluteFixups.push_back(AbsoluteFixup{ code.size() + 1, label });
			EmitInstruction(0xC3, "jp " + label);
			Emit(0);
			Emit(0);
		}

		void Assembler::Jp(Cond condition, u16 address)
		{
			// JP cc,n16: 11 ccc 010.
			EmitInstruction((u8)(0xC2 + ((u8)condition << 3)), "jp cc," + HexWord(address));
			Emit((u8)(address & 0xFF));
			Emit((u8)(address >> 8));
		}

		void Assembler::Jp(Cond condition, const std::string& label)
		{
			absoluteFixups.push_back(AbsoluteFixup{ code.size() + 1, label });
			EmitInstruction((u8)(0xC2 + ((u8)condition << 3)), "jp cc," + label);
			Emit(0);
			Emit(0);
		}

		void Assembler::Jr(s8 offset)
		{
			EmitInstruction(0x18, "jr " + std::to_string((int)offset));
			Emit((u8)offset);
		}

		void Assembler::Jr(const std::string& label)
		{
			relativeFixups.push_back(RelativeFixup{ code.size() + 1, label });
			EmitInstruction(0x18, "jr " + label);
			Emit(0);
		}

		void Assembler::Jr(Cond condition, s8 offset)
		{
			EmitInstruction((u8)(0x20 + ((u8)condition << 3)), "jr cc," + std::to_string((int)offset));
			Emit((u8)offset);
		}

		void Assembler::Jr(Cond condition, const std::string& label)
		{
			relativeFixups.push_back(RelativeFixup{ code.size() + 1, label });
			EmitInstruction((u8)(0x20 + ((u8)condition << 3)), "jr cc," + label);
			Emit(0);
		}

		void Assembler::Call(u16 address)
		{
			EmitInstruction(0xCD, "call " + HexWord(address));
			Emit((u8)(address & 0xFF));
			Emit((u8)(address >> 8));
		}

		void Assembler::Call(const std::string& label)
		{
			absoluteFixups.push_back(AbsoluteFixup{ code.size() + 1, label });
			EmitInstruction(0xCD, "call " + label);
			Emit(0);
			Emit(0);
		}

		void Assembler::Call(Cond condition, u16 address)
		{
			// CALL cc,n16: 11 ccc 100.
			EmitInstruction((u8)(0xC4 + ((u8)condition << 3)), "call cc," + HexWord(address));
			Emit((u8)(address & 0xFF));
			Emit((u8)(address >> 8));
		}

		void Assembler::Call(Cond condition, const std::string& label)
		{
			absoluteFixups.push_back(AbsoluteFixup{ code.size() + 1, label });
			EmitInstruction((u8)(0xC4 + ((u8)condition << 3)), "call cc," + label);
			Emit(0);
			Emit(0);
		}

		void Assembler::Ret() { EmitInstruction(0xC9, "ret"); }
		void Assembler::Ret(Cond condition) { EmitInstruction((u8)(0xC0 + ((u8)condition << 3)), "ret cc"); }
		void Assembler::Reti() { EmitInstruction(0xD9, "reti"); }

		void Assembler::Rst(u8 vector)
		{
			if ((vector & 0x07) != 0 || vector > 0x38)
				throw std::runtime_error("gb asm: RST vector must be a multiple of 8 below 0x40");
			EmitInstruction((u8)(0xC7 + vector), "rst " + HexWord(vector));
		}

		// -----------------------------------------------------------------------------------
		// Output
		// -----------------------------------------------------------------------------------

		std::vector<u8> Assembler::TakeImage(u32 size, u8 pad)
		{
			for (const auto& fixup : absoluteFixups)
			{
				auto found = labels.find(fixup.label);
				if (found == labels.end())
					throw std::runtime_error("gb asm: undefined label: " + fixup.label);
				u16 target = found->second;
				if (fixup.position + 1 >= code.size())
					throw std::runtime_error("gb asm: fixup outside the image");
				code[fixup.position] = (u8)(target & 0xFF);
				code[fixup.position + 1] = (u8)(target >> 8);
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
				code[fixup.position] = (u8)(s8)delta;
			}

			std::vector<u8> image = code;

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
