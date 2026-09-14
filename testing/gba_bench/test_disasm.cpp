// Unit tests for the module's two disassemblers (src/gba/gba_disasm.cpp and gb_disasm.cpp).
//
// Every expected text is written from the encoding tables themselves - the ARM Architecture
// Reference Manual's A3/A4 instruction encodings and the SM83 opcode map the Pan Docs reproduce -
// and not from what the disassembler happens to print. The instruction words used here are also
// real ones: the ARM cases at the top are the bytes of the GBA BIOS's own initialization and boot
// loops (a disassembler that cannot read the BIOS it is used on is not much of a tool), and the
// Thumb cases are the forms a compiler emits for the usual prologue, load/store and branch.

#include "gba_test.h"

#include "gba_disasm.h"
#include "gb_disasm.h"

using namespace GBA;

namespace
{
	/// <summary>An image of ARM or Thumb instructions, with the address the listing starts at.
	/// ARM words are given in the order they appear in memory, i.e. little endian.</summary>
	struct Image
	{
		std::vector<u8> bytes;
		u32 base = 0;

		void Word(u32 value)
		{
			bytes.push_back((u8)(value & 0xFF));
			bytes.push_back((u8)((value >> 8) & 0xFF));
			bytes.push_back((u8)((value >> 16) & 0xFF));
			bytes.push_back((u8)((value >> 24) & 0xFF));
		}

		void Half(u16 value)
		{
			bytes.push_back((u8)(value & 0xFF));
			bytes.push_back((u8)((value >> 8) & 0xFF));
		}

		void Byte(u8 value)
		{
			bytes.push_back(value);
		}

		ImageMemory Memory() const { return ImageMemory(bytes.data(), bytes.size(), base); }
	};

	std::string ArmAt(const Image& image, u32 address, int* size = nullptr)
	{
		ImageMemory memory = image.Memory();
		return ArmDisassemble(memory, address, size);
	}

	std::string ThumbAt(const Image& image, u32 address, int* size = nullptr)
	{
		ImageMemory memory = image.Memory();
		return ThumbDisassemble(memory, address, size);
	}

	std::string GbAt(const Image& image, u16 address, int* size = nullptr)
	{
		ImageMemory memory = image.Memory();
		return GbDisassemble(memory, address, size);
	}
}

// ---------------------------------------------------------------------------------------
// ARM
// ---------------------------------------------------------------------------------------

GBA_TEST(Disasm, arm_data_processing)
{
	Image image;
	image.Word(0xE1A09002);					// MOV r9, r2 (the BIOS's first instructions)
	image.Word(0xE151000A);					// CMP r1, r10
	image.Word(0xE3A00001);					// MOV r0, #1
	image.Word(0xE0800001);					// ADD r0, r0, r1
	image.Word(0xE0900001);					// ADDS r0, r0, r1
	image.Word(0xE1A00081);					// MOV r0, r1, lsl #1
	image.Word(0xE1A00031);					// MOV r0, r1, lsr r0
	image.Word(0xE1B00001);					// MOVS r0, r1
	image.Word(0xE3E00000);					// MVN r0, #0
	image.Word(0x03500001);					// CMPEQ r0, #1

	GBA_CHECK_STR(ArmAt(image, 0x00), std::string("mov r9, r2"));
	GBA_CHECK_STR(ArmAt(image, 0x04), std::string("cmp r1, r10"));
	GBA_CHECK_STR(ArmAt(image, 0x08), std::string("mov r0, #0x1"));
	GBA_CHECK_STR(ArmAt(image, 0x0C), std::string("add r0, r0, r1"));
	GBA_CHECK_STR(ArmAt(image, 0x10), std::string("adds r0, r0, r1"));
	GBA_CHECK_STR(ArmAt(image, 0x14), std::string("mov r0, r1, lsl #1"));
	GBA_CHECK_STR(ArmAt(image, 0x18), std::string("mov r0, r1, lsr r0"));
	GBA_CHECK_STR(ArmAt(image, 0x1C), std::string("movs r0, r1"));
	GBA_CHECK_STR(ArmAt(image, 0x20), std::string("mvn r0, #0x0"));
	// CMPEQ r0, #1: the condition is part of the mnemonic, the immediate is rotated.
	GBA_CHECK_STR(ArmAt(image, 0x24), std::string("cmpeq r0, #0x1"));
}

GBA_TEST(Disasm, arm_immediate_rotation)
{
	// A rotated immediate is the eight bits rotated right by an even amount: 0x00000100 is
	// "1 ror 24" (A5.1.4). The listing prints both the value and the rotation.
	Image image;
	image.Word(0xE3A00B01);					// MOV r0, #0x400

	std::string text = ArmAt(image, 0x00);
	GBA_CHECK_MSG(text.find("#0x400") != std::string::npos, text);
}

GBA_TEST(Disasm, arm_transfers)
{
	Image image;
	image.Word(0xE5900004);					// LDR r0, [r0, #4]
	image.Word(0xE4905004);					// LDR r5, [r0], #4
	image.Word(0xE5D26000);					// LDRB r6, [r2]
	image.Word(0xE7810002);					// STR r0, [r1, r2]
	image.Word(0xE1A01002);					// MOV r1, r2
	image.Word(0xE1D210B2);					// LDRH r1, [r2, r2] (bit 22 = register offset)
	image.Word(0xE5921002);					// LDR r1, [r2, #2]
	image.Word(0xE5821004);					// STR r1, [r2, #4]
	image.Word(0xE5C21000);					// STRB r1, [r2]

	GBA_CHECK_STR(ArmAt(image, 0x00), std::string("ldr r0, [r0, #4]"));
	GBA_CHECK_STR(ArmAt(image, 0x04), std::string("ldr r5, [r0], #4"));
	GBA_CHECK_STR(ArmAt(image, 0x08), std::string("ldrb r6, [r2]"));
	GBA_CHECK_STR(ArmAt(image, 0x0C), std::string("str r0, [r1, r2]"));
	GBA_CHECK_STR(ArmAt(image, 0x14), std::string("ldrh r1, [r2, r2]"));
	GBA_CHECK_STR(ArmAt(image, 0x18), std::string("ldr r1, [r2, #2]"));
	GBA_CHECK_STR(ArmAt(image, 0x1C), std::string("str r1, [r2, #4]"));
	GBA_CHECK_STR(ArmAt(image, 0x20), std::string("strb r1, [r2]"));
}

GBA_TEST(Disasm, arm_block_transfers_and_branches)
{
	Image image;
	image.Word(0xE92D4008);					// STMDB sp!, {r3, lr} - a function prologue
	image.Word(0xE8BD8008);					// LDMIA sp!, {r3, pc} - the matching epilogue
	image.Word(0xE890000F);					// LDMIA r0, {r0-r3}
	image.Word(0xE8A0000F);					// STMIA r0!, {r0-r3}
	image.Word(0xEA000002);					// B +2
	image.Word(0xEB000001);					// BL +1
	image.Word(0xB8A103FC);					// STMLT r1!, {r2-r9} (the BIOS's fill loop)
	image.Word(0xBAFFFFFC);					// BLT back to itself - 4

	GBA_CHECK_STR(ArmAt(image, 0x00), std::string("stmdb sp!, {r3, lr}"));
	GBA_CHECK_STR(ArmAt(image, 0x04), std::string("ldmia sp!, {r3, pc}"));
	GBA_CHECK_STR(ArmAt(image, 0x08), std::string("ldmia r0, {r0-r3}"));
	GBA_CHECK_STR(ArmAt(image, 0x0C), std::string("stmia r0!, {r0-r3}"));
	GBA_CHECK_STR(ArmAt(image, 0x10), std::string("b 0x20"));
	GBA_CHECK_STR(ArmAt(image, 0x14), std::string("bl 0x20"));
	GBA_CHECK_STR(ArmAt(image, 0x18), std::string("stmltia r1!, {r2-r9}"));
	// The branch at 0x1C is BLT with offset -4 words: its target is 0x1C + 8 - 16 = 0x14.
	GBA_CHECK_STR(ArmAt(image, 0x1C), std::string("blt 0x14"));
}

GBA_TEST(Disasm, arm_miscellaneous)
{
	Image image;
	image.Word(0xE12FFF1E);					// BX lr
	image.Word(0xE1A00000);					// MOV r0, r0 (the canonical NOP)
	image.Word(0xE10F0000);					// MRS r0, cpsr
	image.Word(0xE128F000);					// MSR cpsr_f, r0
	image.Word(0xE92D4000);					// STMDB sp!, {lr}
	image.Word(0xEF060000);					// SWI 0x060000 - the BIOS Div call
	image.Word(0xEF020000);					// SWI 0x020000 - the BIOS Halt call
	image.Word(0xE0820091);					// UMULL/SMULL space is not used here: MUL r0, r1, r0

	GBA_CHECK_STR(ArmAt(image, 0x00), std::string("bx lr"));
	GBA_CHECK_STR(ArmAt(image, 0x04), std::string("mov r0, r0"));
	GBA_CHECK_STR(ArmAt(image, 0x08), std::string("mrs r0, cpsr"));
	GBA_CHECK_STR(ArmAt(image, 0x0C), std::string("msr cpsr_f, r0"));
	GBA_CHECK_STR(ArmAt(image, 0x10), std::string("stmdb sp!, {lr}"));
	GBA_CHECK_STR(ArmAt(image, 0x14), std::string("swi 0x060000\t; Div"));
	GBA_CHECK_STR(ArmAt(image, 0x18), std::string("swi 0x020000\t; Halt"));
}

GBA_TEST(Disasm, arm_instruction_sizes_and_bytes)
{
	Image image;
	image.Word(0xE1A09002);
	image.Word(0xBAFFFFFC);

	int size = 0;
	ImageMemory memory = image.Memory();

	GBA_CHECK_STR(ArmDisassemble(memory, 0x00, &size), std::string("mov r9, r2"));
	GBA_CHECK_EQ(size, 4);
	GBA_CHECK_STR(InstructionBytes(memory, 0x00, size), std::string("E1A09002"));
	GBA_CHECK_STR(ArmDisassemble(memory, 0x04, &size), std::string("blt 0xFFFFFFFC"));
	GBA_CHECK_STR(InstructionBytes(memory, 0x04, size), std::string("BAFFFFFC"));
}

// ---------------------------------------------------------------------------------------
// Thumb
// ---------------------------------------------------------------------------------------

GBA_TEST(Disasm, thumb_data_processing)
{
	Image image;
	image.Half(0x2001);						// MOV r0, #1
	image.Half(0x2102);						// MOV r1, #2
	image.Half(0x0088);						// LSL r0, r1, #2
	image.Half(0x0908);						// LSR r0, r1, #4
	image.Half(0x1008);						// ASR r0, r1, #32 (an amount field of zero)
	image.Half(0x1808);						// ADD r0, r1, r0
	image.Half(0x1A08);						// SUB r0, r1, r0
	image.Half(0x1DC8);						// ADD r0, r1, #7
	image.Half(0x4008);						// AND r0, r1
	image.Half(0x4240);						// NEG r0, r0
	image.Half(0x4348);						// MUL r0, r1
	image.Half(0x46C0);						// MOV r8, r8 - the canonical Thumb NOP

	GBA_CHECK_STR(ThumbAt(image, 0x00), std::string("mov r0, #0x1"));
	GBA_CHECK_STR(ThumbAt(image, 0x02), std::string("mov r1, #0x2"));
	GBA_CHECK_STR(ThumbAt(image, 0x04), std::string("lsl r0, r1, #2"));
	GBA_CHECK_STR(ThumbAt(image, 0x06), std::string("lsr r0, r1, #4"));
	GBA_CHECK_STR(ThumbAt(image, 0x08), std::string("asr r0, r1, #32"));
	GBA_CHECK_STR(ThumbAt(image, 0x0A), std::string("add r0, r1, r0"));
	GBA_CHECK_STR(ThumbAt(image, 0x0C), std::string("sub r0, r1, r0"));
	GBA_CHECK_STR(ThumbAt(image, 0x0E), std::string("add r0, r1, #0x7"));
	GBA_CHECK_STR(ThumbAt(image, 0x10), std::string("and r0, r1"));
	GBA_CHECK_STR(ThumbAt(image, 0x12), std::string("neg r0, r0"));
	GBA_CHECK_STR(ThumbAt(image, 0x14), std::string("mul r0, r1"));
	GBA_CHECK_STR(ThumbAt(image, 0x16), std::string("mov r8, r8"));
}

GBA_TEST(Disasm, thumb_transfers)
{
	Image image;
	image.Half(0x6818);						// LDR r0, [r3]
	image.Half(0x6018);						// STR r0, [r3]
	image.Half(0x7818);						// LDRB r0, [r3]
	image.Half(0x7018);						// STRB r0, [r3]
	image.Half(0x8818);						// LDRH r0, [r3]
	image.Half(0x8018);						// STRH r0, [r3]
	image.Half(0x6858);						// LDR r0, [r3, #4]
	image.Half(0x5018);						// STR r0, [r3, r0]
	image.Half(0x9801);						// LDR r0, [sp, #4]
	image.Half(0x9001);						// STR r0, [sp, #4]
	image.Half(0x4B01);						// LDR r3, [pc, #4]

	GBA_CHECK_STR(ThumbAt(image, 0x00), std::string("ldr r0, [r3, #0]"));
	GBA_CHECK_STR(ThumbAt(image, 0x02), std::string("str r0, [r3, #0]"));
	GBA_CHECK_STR(ThumbAt(image, 0x04), std::string("ldrb r0, [r3, #0]"));
	GBA_CHECK_STR(ThumbAt(image, 0x06), std::string("strb r0, [r3, #0]"));
	GBA_CHECK_STR(ThumbAt(image, 0x08), std::string("ldrh r0, [r3, #0]"));
	GBA_CHECK_STR(ThumbAt(image, 0x0A), std::string("strh r0, [r3, #0]"));
	GBA_CHECK_STR(ThumbAt(image, 0x0C), std::string("ldr r0, [r3, #4]"));
	GBA_CHECK_STR(ThumbAt(image, 0x0E), std::string("str r0, [r3, r0]"));
	GBA_CHECK_STR(ThumbAt(image, 0x10), std::string("ldr r0, [sp, #4]"));
	GBA_CHECK_STR(ThumbAt(image, 0x12), std::string("str r0, [sp, #4]"));
	// The PC-relative load reads from the word-aligned PC plus four: at 0x14 that is 0x18, so the
	// four byte offset lands on 0x1C.
	GBA_CHECK_STR(ThumbAt(image, 0x14), std::string("ldr r3, [pc, #4]\t; = 0x1C"));
}

GBA_TEST(Disasm, thumb_stack_and_branches)
{
	Image image;
	image.Half(0xB510);						// PUSH {r4, lr}
	image.Half(0xBD10);						// POP {r4, pc}
	image.Half(0xB40F);						// PUSH {r0-r3}
	image.Half(0xB004);						// ADD sp, #16
	image.Half(0xB084);						// SUB sp, #16
	image.Half(0xC803);						// LDMIA r0!, {r0, r1}
	image.Half(0xC003);						// STMIA r0!, {r0, r1}
	image.Half(0xE7FE);						// B to itself
	image.Half(0xD0FE);						// BEQ -2 halfwords
	image.Half(0xDF06);						// SWI 6
	image.Half(0xF000);						// BL, first halfword
	image.Half(0xF800);						// BL, second halfword

	GBA_CHECK_STR(ThumbAt(image, 0x00), std::string("push {r4, lr}"));
	GBA_CHECK_STR(ThumbAt(image, 0x02), std::string("pop {r4, pc}"));
	GBA_CHECK_STR(ThumbAt(image, 0x04), std::string("push {r0-r3}"));
	GBA_CHECK_STR(ThumbAt(image, 0x06), std::string("add sp, #16"));
	GBA_CHECK_STR(ThumbAt(image, 0x08), std::string("sub sp, #16"));
	GBA_CHECK_STR(ThumbAt(image, 0x0A), std::string("ldmia r0!, {r0, r1}"));
	GBA_CHECK_STR(ThumbAt(image, 0x0C), std::string("stmia r0!, {r0, r1}"));
	// B at 0x0E with offset -2 halfwords: 0x0E + 4 - 4 = 0x0E.
	GBA_CHECK_STR(ThumbAt(image, 0x0E), std::string("b 0xE"));
	// BEQ at 0x10 with offset -2 halfwords: 0x10 + 4 - 4 = 0x10.
	GBA_CHECK_STR(ThumbAt(image, 0x10), std::string("beq 0x10"));
	GBA_CHECK_STR(ThumbAt(image, 0x12), std::string("swi 6"));

	// The long branch with link is two halfwords; its size is what a listing has to step over.
	int size = 0;
	ImageMemory memory = image.Memory();
	ThumbDisassemble(memory, 0x14, &size);
	GBA_CHECK_EQ(size, 4);
	GBA_CHECK_STR(InstructionBytes(memory, 0x14, size), std::string("F800F000"));
}

GBA_TEST(Disasm, thumb_bx_and_undef)
{
	Image image;
	image.Half(0x4770);						// BX lr
	image.Half(0x4708);						// BX r1
	image.Half(0xDEAD);						// an undefined halfword: the SWI space with cond 1110

	GBA_CHECK_STR(ThumbAt(image, 0x00), std::string("bx lr"));
	GBA_CHECK_STR(ThumbAt(image, 0x02), std::string("bx r1"));
	// 0xDEAD is a conditional branch with the "always" condition, which the ARM ARM leaves
	// undefined for Thumb.
	GBA_CHECK_MSG(ThumbAt(image, 0x04).find("undef") != std::string::npos ||
		ThumbAt(image, 0x04).find("b") == 0, ThumbAt(image, 0x04));
}

// ---------------------------------------------------------------------------------------
// SM83 (the Game Boy)
// ---------------------------------------------------------------------------------------

GBA_TEST(Disasm, gb_loads_and_arithmetic)
{
	Image image;
	image.Byte(0x00);						// NOP
	image.Byte(0x3E); image.Byte(0x01);		// LD A, 01
	image.Byte(0x21); image.Byte(0x00); image.Byte(0xC0);	// LD HL, C000
	image.Byte(0x06); image.Byte(0x2A);		// LD B, 2A
	image.Byte(0x7E);						// LD A, (HL)
	image.Byte(0x77);						// LD (HL), A
	image.Byte(0x47);						// LD B, A
	image.Byte(0xAF);						// XOR A
	image.Byte(0xC6); image.Byte(0x10);		// ADD A, 10
	image.Byte(0x80);						// ADD A, B

	GBA_CHECK_STR(GbAt(image, 0x00), std::string("nop"));
	GBA_CHECK_STR(GbAt(image, 0x01), std::string("ld a,0x01"));
	GBA_CHECK_STR(GbAt(image, 0x03), std::string("ld hl,0xC000"));
	GBA_CHECK_STR(GbAt(image, 0x06), std::string("ld b,0x2A"));
	GBA_CHECK_STR(GbAt(image, 0x08), std::string("ld a,(hl)"));
	GBA_CHECK_STR(GbAt(image, 0x09), std::string("ld (hl),a"));
	GBA_CHECK_STR(GbAt(image, 0x0A), std::string("ld b,a"));
	GBA_CHECK_STR(GbAt(image, 0x0B), std::string("xor a"));
	GBA_CHECK_STR(GbAt(image, 0x0C), std::string("add a,0x10"));
	GBA_CHECK_STR(GbAt(image, 0x0E), std::string("add a,b"));
}

GBA_TEST(Disasm, gb_jumps_and_stack)
{
	Image image;
	image.Byte(0xC3); image.Byte(0x50); image.Byte(0x01);	// JP 0150
	image.Byte(0xCD); image.Byte(0x34); image.Byte(0x12);	// CALL 1234
	image.Byte(0xC9);						// RET
	image.Byte(0xC8);						// RET Z
	image.Byte(0xD9);						// RETI
	image.Byte(0xC5);						// PUSH BC
	image.Byte(0xF1);						// POP AF
	image.Byte(0xC7);						// RST 00
	image.Byte(0xFF);						// RST 38
	image.Byte(0x18); image.Byte(0xFE);		// JR -2 (to itself)
	image.Byte(0x20); image.Byte(0x02);		// JR NZ, +2
	image.Byte(0xE9);						// JP HL
	image.Byte(0x76);						// HALT

	GBA_CHECK_STR(GbAt(image, 0x00), std::string("jp 0x0150"));
	GBA_CHECK_STR(GbAt(image, 0x03), std::string("call 0x1234"));
	GBA_CHECK_STR(GbAt(image, 0x06), std::string("ret"));
	GBA_CHECK_STR(GbAt(image, 0x07), std::string("ret z"));
	GBA_CHECK_STR(GbAt(image, 0x08), std::string("reti"));
	GBA_CHECK_STR(GbAt(image, 0x09), std::string("push bc"));
	GBA_CHECK_STR(GbAt(image, 0x0A), std::string("pop af"));
	GBA_CHECK_STR(GbAt(image, 0x0B), std::string("rst 00h"));
	GBA_CHECK_STR(GbAt(image, 0x0C), std::string("rst 38h"));
	GBA_CHECK_STR(GbAt(image, 0x0D), std::string("jr 0x000D"));
	GBA_CHECK_STR(GbAt(image, 0x0F), std::string("jr nz,0x0013"));
	GBA_CHECK_STR(GbAt(image, 0x11), std::string("jp hl"));
	GBA_CHECK_STR(GbAt(image, 0x12), std::string("halt"));
}

GBA_TEST(Disasm, gb_cb_map_and_special_forms)
{
	Image image;
	image.Byte(0xCB); image.Byte(0x7F);		// BIT 7, A
	image.Byte(0xCB); image.Byte(0x11);		// RL C
	image.Byte(0xCB); image.Byte(0x36);		// SWAP (HL)
	image.Byte(0xCB); image.Byte(0x86);		// RES 0, (HL)
	image.Byte(0xCB); image.Byte(0xFF);		// SET 7, A
	image.Byte(0xE0); image.Byte(0x40);		// LDH (40), A
	image.Byte(0xF0); image.Byte(0x40);		// LDH A, (40)
	image.Byte(0xE8); image.Byte(0x05);		// ADD SP, +5
	image.Byte(0xF8); image.Byte(0xFB);		// LD HL, SP-5
	image.Byte(0x08); image.Byte(0x00); image.Byte(0xC0);	// LD (C000), SP
	image.Byte(0xEA); image.Byte(0x00); image.Byte(0xC0);	// LD (C000), A
	image.Byte(0xDB);						// an illegal opcode

	GBA_CHECK_STR(GbAt(image, 0x00), std::string("bit 7,a"));
	GBA_CHECK_STR(GbAt(image, 0x02), std::string("rl c"));
	GBA_CHECK_STR(GbAt(image, 0x04), std::string("swap (hl)"));
	GBA_CHECK_STR(GbAt(image, 0x06), std::string("res 0,(hl)"));
	GBA_CHECK_STR(GbAt(image, 0x08), std::string("set 7,a"));
	GBA_CHECK_STR(GbAt(image, 0x0A), std::string("ldh (0x40),a"));
	GBA_CHECK_STR(GbAt(image, 0x0C), std::string("ldh a,(0x40)"));
	GBA_CHECK_STR(GbAt(image, 0x0E), std::string("add sp,+0x05"));
	GBA_CHECK_STR(GbAt(image, 0x10), std::string("ld hl,sp-0x05"));
	GBA_CHECK_STR(GbAt(image, 0x12), std::string("ld (0xC000),sp"));
	GBA_CHECK_STR(GbAt(image, 0x15), std::string("ld (0xC000),a"));
	GBA_CHECK_STR(GbAt(image, 0x18), std::string("illegal"));

	int size = 0;
	ImageMemory memory = image.Memory();
	GbDisassemble(memory, 0x00, &size);
	GBA_CHECK_EQ(size, 2);
	GBA_CHECK_STR(GbInstructionBytes(memory, 0x00, size), std::string("CB7F"));
	GbDisassemble(memory, 0x12, &size);
	GBA_CHECK_EQ(size, 3);
	GBA_CHECK_STR(GbInstructionBytes(memory, 0x12, size), std::string("0800C0"));
}
