// The SM83 disassembler. See gb_disasm.h for the opcode map it is written from.

#include "gb_disasm.h"

#include <cstdio>

namespace GBA
{
	namespace
	{
		/// <summary>The eight 8-bit operands of the register field (opcode bits 5-3), in the order
		/// the opcode map uses: B, C, D, E, H, L, (HL) and A.</summary>
		const char* const GbRegisters[8] = { "b", "c", "d", "e", "h", "l", "(hl)", "a" };

		/// <summary>The four register pairs of the "rr" field (opcode bits 5-4).</summary>
		const char* const GbPairs[4] = { "bc", "de", "hl", "sp" };

		/// <summary>The four register pairs of the "rr" field where the last one is AF instead of
		/// SP, i.e. PUSH and POP.</summary>
		const char* const GbStackPairs[4] = { "bc", "de", "hl", "af" };

		/// <summary>The four conditions of the "cc" field (opcode bits 4-3).</summary>
		const char* const GbConditions[4] = { "nz", "z", "nc", "c" };

		/// <summary>The eight restart targets of RST (opcode bits 5-3, times eight).</summary>
		const char* const GbRestartTargets[8] =
		{
			"00h", "08h", "10h", "18h", "20h", "28h", "30h", "38h",
		};

		std::string Hex8(u16 value)
		{
			char text[16];
			snprintf(text, sizeof text, "0x%02X", value & 0xFF);
			return text;
		}

		std::string Hex16(u16 value)
		{
			char text[16];
			snprintf(text, sizeof text, "0x%04X", value);
			return text;
		}

		/// <summary>The relative offset of a jump, as the signed byte the instruction holds, with
		/// the absolute target in the comment the listings print.</summary>
		std::string RelativeTarget(u16 address, u8 offset)
		{
			s16 relative = (s16)(s8)offset;
			return Hex16((u16)(address + 3 + relative));
		}

		/// <summary>The 256 entries of the unprefixed opcode map. The placeholders the formatter
		/// fills in are documented next to it.</summary>
		const char* const GbOpcodeTable[256] =
		{
			/* 0x00 */ "nop", "ld %p,%w", "ld (bc),a", "inc %p",
			/* 0x04 */ "inc %r", "dec %r", "ld %r,%b", "rlca",
			/* 0x08 */ "ld (%w),sp", "add hl,%p", "ld a,(bc)", "dec %p",
			/* 0x0C */ "inc %r", "dec %r", "ld %r,%b", "rrca",
			/* 0x10 */ "stop", "ld %p,%w", "ld (de),a", "inc %p",
			/* 0x14 */ "inc %r", "dec %r", "ld %r,%b", "rla",
			/* 0x18 */ "jr %j", "add hl,%p", "ld a,(de)", "dec %p",
			/* 0x1C */ "inc %r", "dec %r", "ld %r,%b", "rra",
			/* 0x20 */ "jr nz,%j", "ld %p,%w", "ld (hl+),a", "inc %p",
			/* 0x24 */ "inc %r", "dec %r", "ld %r,%b", "daa",
			/* 0x28 */ "jr z,%j", "add hl,%p", "ld a,(hl+)", "dec %p",
			/* 0x2C */ "inc %r", "dec %r", "ld %r,%b", "cpl",
			/* 0x30 */ "jr nc,%j", "ld %p,%w", "ld (hl-),a", "inc %p",
			/* 0x34 */ "inc %r", "dec %r", "ld %r,%b", "scf",
			/* 0x38 */ "jr c,%j", "add hl,%p", "ld a,(hl-)", "dec %p",
			/* 0x3C */ "inc %r", "dec %r", "ld %r,%b", "ccf",

			/* 0x40-0x7F: ld r,r' from the two register fields; 0x76 is halt. */
			"ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s",
			"ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s",
			"ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s",
			"ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s",
			"ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s",
			"ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s",
			"ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "halt", "ld %r,%s",
			"ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s", "ld %r,%s",

			/* 0x80-0xBF: the ALU operations, one group of eight per operation. */
			"add a,%s", "add a,%s", "add a,%s", "add a,%s", "add a,%s", "add a,%s", "add a,%s", "add a,%s",
			"adc a,%s", "adc a,%s", "adc a,%s", "adc a,%s", "adc a,%s", "adc a,%s", "adc a,%s", "adc a,%s",
			"sub %s", "sub %s", "sub %s", "sub %s", "sub %s", "sub %s", "sub %s", "sub %s",
			"sbc a,%s", "sbc a,%s", "sbc a,%s", "sbc a,%s", "sbc a,%s", "sbc a,%s", "sbc a,%s", "sbc a,%s",
			"and %s", "and %s", "and %s", "and %s", "and %s", "and %s", "and %s", "and %s",
			"xor %s", "xor %s", "xor %s", "xor %s", "xor %s", "xor %s", "xor %s", "xor %s",
			"or %s", "or %s", "or %s", "or %s", "or %s", "or %s", "or %s", "or %s",
			"cp %s", "cp %s", "cp %s", "cp %s", "cp %s", "cp %s", "cp %s", "cp %s",

			/* 0xC0 */ "ret %c", "pop %q", "jp %c,%t", "jp %t",
			/* 0xC4 */ "call %c,%t", "push %q", "add a,%b", "rst %k",
			/* 0xC8 */ "ret %c", "ret", "jp %c,%t", "db 0xCB",
			/* 0xCC */ "call %c,%t", "call %t", "adc a,%b", "rst %k",
			/* 0xD0 */ "ret %c", "pop %q", "jp %c,%t", "illegal",
			/* 0xD4 */ "call %c,%t", "push %q", "sub %b", "rst %k",
			/* 0xD8 */ "ret %c", "reti", "jp %c,%t", "illegal",
			/* 0xDC */ "call %c,%t", "illegal", "sbc a,%b", "rst %k",
			/* 0xE0 */ "ldh (%b),a", "pop %q", "ld (c),a", "illegal",
			/* 0xE4 */ "illegal", "push %q", "and %b", "rst %k",
			/* 0xE8 */ "add sp,%e", "jp hl", "ld (%t),a", "illegal",
			/* 0xEC */ "illegal", "illegal", "xor %b", "rst %k",
			/* 0xF0 */ "ldh a,(%b)", "pop %q", "ld a,(c)", "di",
			/* 0xF4 */ "illegal", "push %q", "or %b", "rst %k",
			/* 0xF8 */ "ld hl,sp%e", "ld sp,hl", "ld a,(%t)", "ei",
			/* 0xFC */ "illegal", "illegal", "cp %b", "rst %k",
		};

		std::string FormatGbInstruction(const DisasmMemory& memory, u16 address, u8 opcode,
			const char* const* table, int* size)
		{
			const char* pattern = table[opcode];
			u16 immediate = memory.Read8((u16)(address + 1));

			std::string text;
			int length = 1;

			for (const char* at = pattern; *at != '\0'; at++)
			{
				if (*at != '%')
				{
					text += *at;
					continue;
				}

				switch (*++at)
				{
				case 'r':									// the destination register field
					text += GbRegisters[(opcode >> 3) & 7];
					break;
				case 's':									// the source register field
					text += GbRegisters[opcode & 7];
					break;
				case 'p':									// a register pair
					text += GbPairs[(opcode >> 4) & 3];
					break;
				case 'q':									// a register pair for PUSH/POP
					text += GbStackPairs[(opcode >> 4) & 3];
					break;
				case 'c':									// a condition
					text += GbConditions[(opcode >> 3) & 3];
					break;
				case 'k':									// the RST target
					text += GbRestartTargets[(opcode >> 3) & 7];
					break;
				case 'b':									// an eight-bit immediate
					text += Hex8(immediate);
					length = 2;
					break;
				case 'w':									// a sixteen-bit immediate
					text += Hex16((u16)(immediate | (memory.Read8((u16)(address + 2)) << 8)));
					length = 3;
					break;
				case 't':									// a sixteen-bit address
					text += Hex16((u16)(immediate | (memory.Read8((u16)(address + 2)) << 8)));
					length = 3;
					break;
				case 'e':									// a signed byte offset (ADD SP, LD HL,SP+)
					if ((s8)immediate < 0)
						text += "-" + Hex8((u16)(-(s8)immediate));
					else
						text += "+" + Hex8(immediate);
					length = 2;
					break;
				case 'j':									// the target of a relative jump
					// The SM83 adds the offset to the address *after* the instruction, which for a
					// two byte JR is address + 2.
					text += Hex16((u16)(address + 2 + (s16)(s8)immediate));
					length = 2;
					break;
				default:
					text += '%';
					text += *at;
					break;
				}
			}

			if (size != nullptr)
				*size = length;
			return text;
		}

		std::string DecodeCb(u8 opcode, int* size)
		{
			// The CB map: eight shifts/rotates, then BIT, RES and SET, each over the eight
			// register operands (Pan Docs "CPU Opcode Table").
			static const char* const Operations[8] =
			{
				"rlc", "rrc", "rl", "rr", "sla", "sra", "swap", "srl",
			};

			u32 group = opcode >> 6;
			u32 operand = opcode & 7;
			std::string text;

			if (group == 0)
			{
				text = std::string(Operations[(opcode >> 3) & 7]) + " " + GbRegisters[operand];
			}
			else
			{
				static const char* const Names[3] = { "bit", "res", "set" };
				text = std::string(Names[group - 1]) + " " + std::to_string((opcode >> 3) & 7) + "," +
					GbRegisters[operand];
			}

			if (size != nullptr)
				*size = 2;
			return text;
		}
	}

	std::string GbDisassemble(const DisasmMemory& memory, u16 address, int* size)
	{
		u8 opcode = memory.Read8(address);

		if (opcode == 0xCB)
			return DecodeCb(memory.Read8((u16)(address + 1)), size);

		int length = 1;
		std::string text = FormatGbInstruction(memory, address, opcode, GbOpcodeTable, &length);

		if (size != nullptr)
			*size = length;
		return text;
	}

	std::string GbInstructionBytes(const DisasmMemory& memory, u16 address, int size)
	{
		char text[16];
		if (size == 3)
			snprintf(text, sizeof text, "%02X%02X%02X", memory.Read8(address),
				memory.Read8((u16)(address + 1)), memory.Read8((u16)(address + 2)));
		else if (size == 2)
			snprintf(text, sizeof text, "%02X%02X", memory.Read8(address),
				memory.Read8((u16)(address + 1)));
		else
			snprintf(text, sizeof text, "%02X", memory.Read8(address));
		return text;
	}
}
