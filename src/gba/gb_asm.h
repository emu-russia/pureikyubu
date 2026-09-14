// A very small LR35902 (SM83) code emitter, used to build the emulator's own Game Boy boot ROM
// and the test cartridges the unit tests assemble in memory.
//
// Why not an external assembler: the boot ROM is part of the emulator's source, so it has to be
// buildable by the same compiler on every platform the emulator builds on, with no toolchain
// besides the C++ compiler. The emitter covers the instruction subset a boot ROM needs: the 8-bit
// loads and the memory addressing modes that reach the I/O registers, the arithmetic and the bit
// operations, the stack, and the branch family with symbolic labels.
//
// The encodings are the ones tabulated in the Pan Docs "CPU Instruction Set"
// (gbdev.io/pandocs/CPU_Instruction_Set.html) and the RGBDS gbz80(7) opcode reference: one byte
// per instruction plus the immediate bytes, the CB prefix for the shift/rotate/bit family, and the
// two-byte `STOP` and `LD (nn),SP` forms.
//
// Labels are resolved when TakeImage() is called, so forward branches work; a branch to a label
// that was never bound throws std::runtime_error. Every emitted instruction is also recorded in a
// listing ("address: mnemonic / bytes"), which the boot ROM tests print and compare.

#pragma once

#include "gba_types.h"

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace GBA
{
	namespace GbAsm
	{
		/// <summary>The 8-bit registers in their encoding order (Pan Docs, r8 table).</summary>
		enum class R8 : u8
		{
			B = 0, C = 1, D = 2, E = 3, H = 4, L = 5, HL = 6, A = 7,
		};

		/// <summary>The 16-bit registers in their encoding order (Pan Docs, r16 table).</summary>
		enum class R16 : u8
		{
			BC = 0, DE = 1, HL = 2, SP = 3,
		};

		/// <summary>The stack pair order used by PUSH/POP (Pan Docs, r16stk table).</summary>
		enum class R16Stk : u8
		{
			BC = 0, DE = 1, HL = 2, AF = 3,
		};

		/// <summary>The branch conditions in their encoding order (Pan Docs, cond table).</summary>
		enum class Cond : u8
		{
			NZ = 0, Z = 1, NC = 2, C = 3,
		};

		class Assembler
		{
		public:
			// -- location and data ----------------------------------------------------------

			/// <summary>Set the assembly address (the image grows zero-filled up to it).</summary>
			void Org(u16 address);

			u16 PC() const { return pc; }

			/// <summary>Bind a label to the current address (redefining one throws).</summary>
			void Label(const std::string& name);

			void Data8(u8 value);
			void Data16(u16 value);
			void Data16(u16 value, const std::string& text);
			void DataBytes(const u8* data, size_t count);
			void Text(const std::string& ascii);

			/// <summary>Fill `count` bytes with `value` (a label can point into it).</summary>
			void Fill(u8 value, u16 count);

			// -- the 8-bit load family ------------------------------------------------------

			void Ld(R8 dest, R8 source);			// LD r8,r8 (0x40 + dest*8 + source)
			void Ld(R8 dest, u8 value);				// LD r8,n8 (0x06 + dest*8)
			void LdA16(u16 address);				// LD A,(n16)  0xFA
			void Ld16A(u16 address);				// LD (n16),A  0xEA
			void LdA8(u8 offset);					// LD (n8),A   -> LDH (n),A  0xE0
			void Ld8A(u8 offset);					// LD A,(n8)   -> LDH A,(n)  0xF0
			void LdAC();							// LD (C),A  0xE2
			void LdCA();							// LD A,(C)  0xF2
			void Ld16SP(u16 address);				// LD (n16),SP  0x08
			void Ld16(R16 dest, u16 value);			// LD r16,n16 (0x01 + dest*16)
			void Ld16(R16 dest, const std::string& label);	// ... with a label's address
			void LdSPHL();							// LD SP,HL  0xF9
			void LdHLSPe(s8 offset);				// LD HL,SP+e8  0xF8
			void LdAHLI();							// LD (HL+),A  0x22
			void LdAHLD();							// LD (HL-),A  0x32
			void LdAHLA();							// LD A,(HL+)  0x2A
			void LdAHDA();							// LD A,(HL-)  0x3A
			void LdADE();							// LD A,(DE)  0x1A
			void LdDEA();							// LD (DE),A  0x12
			void LdABC();							// LD A,(BC)  0x0A
			void LdBCA();							// LD (BC),A  0x02

			// -- 16-bit arithmetic and the stack -------------------------------------------

			void AddHL(R16 source);					// ADD HL,r16 (0x09 + source*16)
			void AddSPe(s8 offset);					// ADD SP,e8  0xE8
			void Inc16(R16 reg);					// INC r16 (0x03 + reg*16)
			void Dec16(R16 reg);					// DEC r16 (0x0B + reg*16)
			void Push(R16Stk pair);					// PUSH r16stk (0xC5 + pair*16)
			void Pop(R16Stk pair);					// POP r16stk (0xC1 + pair*16)

			// -- 8-bit arithmetic ----------------------------------------------------------

			void Add(R8 source);					// ADD A,r8
			void Add(u8 value);						// ADD A,n8
			void Adc(R8 source);
			void Adc(u8 value);
			void Sub(R8 source);
			void Sub(u8 value);
			void Sbc(R8 source);
			void Sbc(u8 value);
			void And(R8 source);
			void And(u8 value);
			void Xor(R8 source);
			void Xor(u8 value);
			void Or(R8 source);
			void Or(u8 value);
			void Cp(R8 source);
			void Cp(u8 value);
			void Inc(R8 reg);						// INC r8 (0x04 + reg*8)
			void Dec(R8 reg);						// DEC r8 (0x05 + reg*8)

			// -- the accumulator-only operations -------------------------------------------

			void Daa();								// 0x27
			void Cpl();								// 0x2F
			void Scf();								// 0x37
			void Ccf();								// 0x3F
			void Rlca();							// 0x07
			void Rrca();							// 0x0F
			void Rla();								// 0x17
			void Rra();								// 0x1F

			// -- the CB prefix family ------------------------------------------------------

			void Rlc(R8 reg);						// CB 0x00 + reg
			void Rrc(R8 reg);						// CB 0x08 + reg
			void Rl(R8 reg);						// CB 0x10 + reg
			void Rr(R8 reg);						// CB 0x18 + reg
			void Sla(R8 reg);						// CB 0x20 + reg
			void Sra(R8 reg);						// CB 0x28 + reg
			void Swap(R8 reg);						// CB 0x30 + reg
			void Srl(R8 reg);						// CB 0x38 + reg
			void Bit(int bit, R8 reg);				// CB 0x40 + bit*8 + reg
			void Res(int bit, R8 reg);				// CB 0x80 + bit*8 + reg
			void Set(int bit, R8 reg);				// CB 0xC0 + bit*8 + reg

			// -- control flow --------------------------------------------------------------

			void Nop();								// 0x00
			void Halt();							// 0x76
			void Stop();							// 0x10 0x00 (the second byte is ignored)
			void Di();								// 0xF3
			void Ei();								// 0xFB
			void Jp(u16 address);					// JP n16  0xC3
			void Jp(const std::string& label);
			void Jp(Cond condition, u16 address);	// JP cc,n16 (0xC2 + cond*8)
			void Jp(Cond condition, const std::string& label);
			void JpHL();							// JP HL  0xE9
			void Jr(s8 offset);						// JR e8  0x18
			void Jr(const std::string& label);
			void Jr(Cond condition, s8 offset);		// JR cc,e8 (0x20 + cond*8)
			void Jr(Cond condition, const std::string& label);
			void Call(u16 address);					// CALL n16  0xCD
			void Call(const std::string& label);
			void Call(Cond condition, u16 address);	// CALL cc,n16 (0xC4 + cond*8)
			void Call(Cond condition, const std::string& label);
			void Ret();								// 0xC9
			void Ret(Cond condition);				// RET cc (0xC0 + cond*8)
			void Reti();							// 0xD9
			void Rst(u8 vector);					// RST vec (0xC7 + vec/8*8), vec a multiple of 8

			// -- output --------------------------------------------------------------------

			/// <summary>
			/// Resolve the labels and return the image: every byte up to the highest emitted
			/// address. When `size` is non-zero the image is padded with `pad` (0xFF is what an
			/// erased mask ROM reads as) or truncated to exactly `size` bytes.
			/// </summary>
			std::vector<u8> TakeImage(u32 size = 0, u8 pad = 0xFF);

			/// <summary>The listing of everything emitted so far, one instruction per line.</summary>
			std::string Listing() const { return listing; }

			/// <summary>The number of instructions emitted (the listing is per instruction).</summary>
			int InstructionCount() const { return instructionCount; }

		private:
			u16 pc = 0;
			std::vector<u8> code;
			std::string listing;
			int instructionCount = 0;

			struct AbsoluteFixup
			{
				size_t position;		// where the little-endian address operand lives
				std::string label;
			};

			struct RelativeFixup
			{
				size_t position;		// where the signed offset byte lives
				std::string label;
			};

			std::vector<AbsoluteFixup> absoluteFixups;
			std::vector<RelativeFixup> relativeFixups;
			std::map<std::string, u16> labels;

			void Emit(u8 value);
			void EmitInstruction(u8 value, const std::string& text);
			void EmitInstruction2(u8 first, u8 second, const std::string& text);
			void Record(const std::string& text);
			void Reserve(size_t bytes);
		};
	}
}
