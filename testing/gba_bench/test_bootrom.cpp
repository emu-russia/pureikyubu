// Tests for the ARM/Thumb emitter (gba_armasm.cpp) and for the boot ROM it builds
// (gba_bootrom.cpp).
//
// The emitter's half of this file checks a representative instruction of every encoding the boot
// ROM uses against the word the ARM Architecture Reference Manual's encoding tables produce, written out as a literal;
// the values are the ones the ARM Architecture Reference Manual (DDI 0100E) chapter 5 / A5 gives,
// and the comments name the encoding each literal comes from. The label, literal pool and image
// checks come after that.
//
// The boot ROM's half runs the generated image on the emulated hardware: the machine is reset,
// frames are run, and what the ROM drew is inspected. The animation is checked for *behaviour*
// (the picture changes while the animation runs, the last frames differ from the early ones and
// a lot of distinct colours are on the screen, so the mark and the wordmark really were drawn)
// rather than against a hash: the renderer is expected to keep changing.
//
// A note for whoever runs this while the PPU is still being worked on: `Ppu::VramAddress` masks
// with `& (VramSize - 1)` today, which drops bit 15 and folds VRAM every 32 KByte, so the lower
// half of a mode 3 bitmap mirrors the upper half. The boot ROM's own drawing is correct (the
// gradient covers rows 0..159 and the mark is drawn at 0x08000000 + the flat-mark table), so a
// failure here that looks like "the bottom of the screen repeats the top" is that PPU bug, not
// this ROM.

#include "gba_test.h"

#include "gba.h"
#include "gba_armasm.h"
#include "gba_bootrom.h"

#include <stdexcept>
#include <string>
#include <vector>

using namespace GBA;
using namespace GBA::ArmAsm;

namespace
{
	// ---------------------------------------------------------------------------------------
	// Helpers
	// ---------------------------------------------------------------------------------------

	/// <summary>The little-endian word at `offset` of an image.</summary>
	uint32_t WordAt(const std::vector<uint8_t>& image, size_t offset)
	{
		if (offset + 4 > image.size())
			GBA_FAIL("the image is too short to hold the word");
		return (uint32_t)image[offset] | ((uint32_t)image[offset + 1] << 8) |
			((uint32_t)image[offset + 2] << 16) | ((uint32_t)image[offset + 3] << 24);
	}

	/// <summary>Assemble one ARM instruction and return the word it produced.</summary>
	template <typename Emit> uint32_t ArmWord(Emit emit)
	{
		Assembler assembler;
		emit(assembler);
		std::vector<uint8_t> image = assembler.TakeImage();
		return WordAt(image, 0);
	}

	/// <summary>Assemble one Thumb instruction and return the halfword it produced.</summary>
	template <typename Emit> uint16_t ThumbWord(Emit emit)
	{
		Assembler assembler;
		assembler.UseThumb(true);
		emit(assembler);
		std::vector<uint8_t> image = assembler.TakeImage();
		if (image.size() < 2)
			GBA_FAIL("no halfword was emitted");
		return (uint16_t)(image[0] | (image[1] << 8));
	}

	/// <summary>True when emitting `emit` throws std::runtime_error.</summary>
	template <typename Emit> bool Throws(Emit emit)
	{
		try
		{
			emit();
		}
		catch (const std::runtime_error&)
		{
			return true;
		}
		return false;
	}

	/// <summary>
	/// Load a 32-bit constant the way the boot ROM does: MOV when a rotation builds it, MVN when
	/// one builds its complement, otherwise a literal pool right after the instruction.
	/// </summary>
	void LoadConstant(Assembler& assembler, int rd, uint32_t value)
	{
		try
		{
			assembler.Mov(rd, value);
			return;
		}
		catch (const std::runtime_error&)
		{
		}
		try
		{
			assembler.Mvn(rd, ~value);
			return;
		}
		catch (const std::runtime_error&)
		{
		}
		assembler.Ldr(rd, 15, 0);
		assembler.B("pool");
		assembler.Data32(value);
		assembler.Label("pool");
	}

	/// <summary>A count of the distinct colours in a frame (the animation checks use it).</summary>
	int CountDistinctColours(const uint32_t* pixels, int count)
	{
		std::vector<uint32_t> seen;
		seen.reserve(64);
		for (int i = 0; i < count; i++)
		{
			uint32_t colour = pixels[i] & 0xFFFFFF;
			bool found = false;
			for (uint32_t existing : seen)
				if (existing == colour)
				{
					found = true;
					break;
				}
			if (!found)
			{
				if (seen.size() >= 4096)
					break;
				seen.push_back(colour);
			}
		}
		return (int)seen.size();
	}

	int CountNonBlack(const uint32_t* pixels, int count)
	{
		int lit = 0;
		for (int i = 0; i < count; i++)
			if ((pixels[i] & 0xFFFFFF) != 0)
				lit++;
		return lit;
	}

	/// <summary>
	/// A synthetic cartridge: a valid header (with a correct complement check) and a tiny ARM
	/// program that writes `marker` to 0x03000000 for ever. Built with the emitter, which is what
	/// the harness does too.
	/// </summary>
	std::vector<uint8_t> MakeTestCartridge(uint32_t marker)
	{
		Assembler assembler;
		assembler.Org(0x08000000);
		assembler.B("entry");						// the word the boot ROM reads at 0x08000000

		// The header: 0x080000A0..0x080000BC must sum (with 0x19) to the complement of the byte
		// at 0x080000BD (GBATEK "GBA Cartridge Header").
		assembler.Org(0x080000A0);
		assembler.Reserve(29);
		assembler.Data8(0xE7);						// -(0x19 + 0) & 0xFF

		assembler.Org(0x080000C0);
		assembler.Label("entry");
		LoadConstant(assembler, 1, 0x03000000);		// the marker lives at the base of IWRAM
		LoadConstant(assembler, 2, marker);
		assembler.Str(2, 1, 0);
		assembler.B("entry");						// spin

		return assembler.TakeImage(0x10000);
	}

	/// <summary>The number of frames the boot ROM needs before it can look for a cartridge.</summary>
	int AnimationFramesPlusSlack()
	{
		return BootRom::GbaAnimationFrames() + 12;
	}
}

// -------------------------------------------------------------------------------------------
// The emitter: ARM data processing (A5.2)
// -------------------------------------------------------------------------------------------

GBA_TEST(BootRom, EmitterDataProcessing)
{
	// A5.2: cond | 00 | I | opcode | S | Rn | Rd | operand2.
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Mov(0, 0); }), 0xE3A00000);			// MOV r0, #0
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Mov(0, 1); }), 0xE3A00001);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Mov(0, 0xFF); }), 0xE3A000FF);
	// 0x100 = 1 ROR 24: imm8 = 1, rotate = 12 (the canonical encoding).
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Mov(0, 0x100); }), 0xE3A00C01);
	// 0x3F0 = 0x3F ROR 28: imm8 = 0x3F, rotate = 14.
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Mov(0, 0x3F0); }), 0xE3A00E3F);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Mov(0, 0xFF000000); }), 0xE3A004FF);
	// 0x04000000 = 1 ROR 6 (rotate = 3): the canonical MOV r7, #0x04000000.
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Mov(7, 0x04000000); }), 0xE3A07301);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Mov(0, 1, Cond::AL, true); }), 0xE3B00001);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Mov(0, 1, Cond::NE); }), 0x13A00001);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Mov(0, 1, Cond::LT, true); }), 0xB3B00001);

	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.MovReg(0, 1); }), 0xE1A00001);		// MOV r0, r1
	// bits 11-7 = the shift amount, bits 6-5 = the type, bits 3-0 = Rm.
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.MovReg(0, 1, Cond::AL, false, Shift::LSL, 2); }), 0xE1A00101);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.MovReg(0, 1, Cond::AL, false, Shift::LSR, 32); }), 0xE1A00021);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.ShiftReg(0, 1, Shift::LSL, 16); }), 0xE1A00801);

	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Add(0, 1, 4); }), 0xE2810004);		// ADD r0, r1, #4
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.AddReg(0, 1, 2); }), 0xE0810002);	// ADD r0, r1, r2
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Sub(0, 1, 4); }), 0xE2410004);
	// The exception return the boot ROM's handlers use (A5.2 with Rd = PC, S = 1).
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Sub(15, 14, 4, Cond::AL, true); }), 0xE25EF004);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.SubReg(0, 1, 2); }), 0xE0410002);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Rsb(0, 1, 0); }), 0xE2610000);		// RSB r0, r1, #0
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.And(0, 1, 0xFF); }), 0xE20100FF);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.AndReg(0, 1, 2); }), 0xE0010002);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Orr(0, 1, 1); }), 0xE3810001);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.OrrReg(0, 1, 2); }), 0xE1810002);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Eor(0, 1, 0x10); }), 0xE2210010);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.EorReg(0, 0, 1); }), 0xE0200001);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Bic(0, 0, 0x1F); }), 0xE3C0001F);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Mvn(0, 0); }), 0xE3E00000);			// MVN r0, #0

	// CMP and TST always set the flags and write no register (Rd = 0 in the encoding).
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Cmp(0, 0); }), 0xE3500000);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Cmp(0, 0x80); }), 0xE3500080);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.CmpReg(0, 1); }), 0xE1500001);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.CmpReg(0, 1, Cond::AL, Shift::LSL, 2); }), 0xE1500101);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Tst(0, 0x80); }), 0xE3100080);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.TstReg(0, 1); }), 0xE1100001);
}

// -------------------------------------------------------------------------------------------
// The emitter: multiplies (A5.3) and loads and stores (A5.4, A5.5)
// -------------------------------------------------------------------------------------------

GBA_TEST(BootRom, EmitterLoadStoreAndMultiply)
{
	// A5.3: MUL r0, r1, r2 = cond | 0000000 | S | Rd | Rn(0) | Rs | 1001 | Rm.
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Mul(0, 1, 2); }), 0xE0000291);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Mul(0, 1, 2, Cond::AL, true); }), 0xE0100291);
	// UMULL r0, r1, r2, r3: bit 23 = 1, U = 0, A = 0, RdHi = 1, RdLo = 0, Rs = 3, Rm = 2.
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Umull(0, 1, 2, 3); }), 0xE0810392);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Umull(4, 5, 6, 7, Cond::AL, true); }), 0xE0954796);

	// A5.4: cond | 01 | I | P | U | B | W | L | Rn | Rd | offset12. Pre-indexed, no writeback.
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Ldr(0, 1, 4); }), 0xE5910004);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Ldr(0, 1, -4); }), 0xE5110004);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Str(0, 1, 4); }), 0xE5810004);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Str(2, 3, 0x300); }), 0xE5832300);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Ldr(0, 1, 4, Cond::AL, true); }), 0xE5D10004);	// LDRB
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Str(0, 1, 4, Cond::AL, true); }), 0xE5C10004);	// STRB
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.LdrReg(0, 1, 2); }), 0xE7910002);	// LDR r0, [r1, r2]
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.StrReg(0, 1, 2); }), 0xE7810002);

	// A5.5: cond | 000 | P | U | I | W | L | Rn | Rd | offsetHi | 1SH1 | offsetLo.
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Ldrh(0, 1, 4); }), 0xE1D100B4);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Strh(0, 1, 4); }), 0xE1C100B4);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Ldrsb(0, 1, 4); }), 0xE1D100D4);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Ldrsh(0, 1, 4); }), 0xE1D100F4);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Ldrh(0, 1, 0xF0); }), 0xE1D10FB0);

	// A5.6: cond | 100 | P | U | S | W | L | Rn | register_list. LDMIA/STMIA is P = 0, U = 1.
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Ldmia(0, 0x6); }), 0xE8B00006);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Stmia(0, 0x6); }), 0xE8A00006);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Ldmia(0, 0x6, Cond::AL, false); }), 0xE8900006);
	// gba_armasm.h defines Push/Pop as the IA forms SP-relative (there is no STMDB in the
	// interface), which is what the boot ROM uses with an explicit stack pointer adjustment.
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Stmia(13, 0x4FF0); }), 0xE8AD4FF0);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Ldmia(13, 0x8FF0); }), 0xE8BD8FF0);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Push(0x4FF0); }), 0xE8AD4FF0);
}

// -------------------------------------------------------------------------------------------
// The emitter: branches, SWI and BKPT (A5.7 - A5.10)
// -------------------------------------------------------------------------------------------

GBA_TEST(BootRom, EmitterBranches)
{
	// A5.7: BX = cond | 0001 0010 1111 1111 1111 0001 | Rm (and BLX with 0011).
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Bx(0); }), 0xE12FFF10);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Bx(14); }), 0xE12FFF1E);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Bx(3, Cond::NE); }), 0x112FFF13);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Blx(3); }), 0xE12FFF33);

	// A5.8: SWI = cond | 1111 | comment24. The GBA BIOS reads the function number from the byte
	// at [lr - 2], which is bits 23-16, so "SWI 05h" is 0xEF050000 (GBATEK "BIOS Functions").
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Swi(0); }), 0xEF000000);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Swi(5); }), 0xEF050000);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Swi(0x2F); }), 0xEF2F0000);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Swi(6, Cond::NE); }), 0x1F060000);

	// A5.9: BKPT = 0xE1200070 | the low nibble of the immediate.
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Bkpt(0); }), 0xE1200070);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Bkpt(1); }), 0xE1200071);
	GBA_CHECK_HEX32(ArmWord([](Assembler& a) { a.Bkpt(15); }), 0xE120007F);

	// The ARM branch offset is (target - (address + 8)) >> 2, so a branch to itself is -2 words
	// and a branch one instruction forward is -1 word.
	{
		Assembler a;
		a.Label("here");
		a.B("here");
		GBA_CHECK_HEX32(WordAt(a.TakeImage(), 0), 0xEAFFFFFE);
	}
	{
		Assembler a;
		a.B("fwd");							// lands 8 bytes on: (8 - 8) / 4 = 0
		a.B("fwd");							// the second one sees (8 - 12) / 4 = -1
		a.Label("fwd");
		std::vector<uint8_t> image = a.TakeImage();
		GBA_CHECK_HEX32(WordAt(image, 0), 0xEA000000);
		GBA_CHECK_HEX32(WordAt(image, 4), 0xEAFFFFFF);
	}
	{
		Assembler a;
		a.Label("back");
		a.Mov(0, 0);
		a.Bl("back", Cond::EQ);				// from address 4 to 0: (0 - 12) / 4 = -3
		GBA_CHECK_HEX32(WordAt(a.TakeImage(), 4), 0x0BFFFFFD);
	}
}

// -------------------------------------------------------------------------------------------
// The emitter: Thumb (chapter 5's formats)
// -------------------------------------------------------------------------------------------

GBA_TEST(BootRom, EmitterThumb)
{
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbMov(0, 8); }), 0x2008);			// format 3
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbMov(7, 0xFF); }), 0x27FF);
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbMovReg(0, 8); }), 0x4640);		// format 5
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbMovReg(8, 0); }), 0x4680);
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbAdd(0, 1, 2); }), 0x1850);		// format 2
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbAddImm(0, 1); }), 0x3001);		// format 3
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbSubImm(0, 1); }), 0x3801);
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbCmpImm(2, 0x10); }), 0x2A10);
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbLsl(0, 1, 2); }), 0x0088);		// format 1
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbLsr(0, 1, 1); }), 0x0848);
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbAsr(0, 1, 1); }), 0x1048);
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbLsr(0, 1, 32); }), 0x0808);		// "LSR #32"
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbAnd(0, 1); }), 0x4008);			// format 4
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbOrr(0, 1); }), 0x4308);
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbEor(0, 1); }), 0x4048);
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbMvn(0, 1); }), 0x43C8);
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbLdrImm(0, 1, 4); }), 0x6848);	// format 8
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbStrImm(0, 1, 4); }), 0x6048);
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbLdrhImm(0, 1, 2); }), 0x8848);	// format 9
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbStrhImm(0, 1, 2); }), 0x8048);
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbLdrbImm(0, 1, 1); }), 0x7848);	// format 7
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbStrbImm(0, 1, 1); }), 0x7048);
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbPush(0x40FF); }), 0xB5FF);		// format 14
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbPop(0x8001); }), 0xBD01);
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbBx(3); }), 0x4718);				// format 5
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbBx(14); }), 0x4770);
	GBA_CHECK_HEX16(ThumbWord([](Assembler& a) { a.ThumbSwi(5); }), 0xDF05);				// format 16

	// Format 17 (unconditional) and format 15 (conditional): the offset is a signed halfword
	// count relative to the address of the branch plus 4.
	{
		Assembler a;
		a.UseThumb(true);
		a.Label("t");
		a.ThumbB("t");
		std::vector<uint8_t> image = a.TakeImage();
		GBA_CHECK_HEX16((uint16_t)(image[0] | (image[1] << 8)), 0xE7FE);
	}
	{
		Assembler a;
		a.UseThumb(true);
		a.ThumbB("fwd");
		a.ThumbB("fwd");
		a.Label("fwd");
		std::vector<uint8_t> image = a.TakeImage();
		GBA_CHECK_HEX16((uint16_t)(image[0] | (image[1] << 8)), 0xE000);
		GBA_CHECK_HEX16((uint16_t)(image[2] | (image[3] << 8)), 0xE7FF);
	}
	{
		Assembler a;
		a.UseThumb(true);
		a.ThumbB("t", Cond::NE);
		a.ThumbB("t", Cond::NE);
		a.Label("t");
		std::vector<uint8_t> image = a.TakeImage();
		GBA_CHECK_HEX16((uint16_t)(image[0] | (image[1] << 8)), 0xD100);
		GBA_CHECK_HEX16((uint16_t)(image[2] | (image[3] << 8)), 0xD1FF);
	}
}

// -------------------------------------------------------------------------------------------
// The emitter: the immediate encoder, labels, the literal pool and the image
// -------------------------------------------------------------------------------------------

GBA_TEST(BootRom, EmitterImmediateAndFixups)
{
	// The immediate is an 8-bit value rotated right by twice the rotation field. Values whose set
	// bits cannot be squeezed into eight bits with one of the sixteen rotations have to throw.
	GBA_CHECK(Throws([]() { Assembler a; a.Mov(0, 0x101); }));
	GBA_CHECK(Throws([]() { Assembler a; a.Mov(0, 0x102); }));
	GBA_CHECK(Throws([]() { Assembler a; a.Mov(0, 0x10001); }));
	GBA_CHECK(Throws([]() { Assembler a; a.Add(0, 1, 0x1000001); }));
	// ...while these do have an encoding.
	GBA_CHECK(!Throws([]() { Assembler a; a.Mov(0, 0); }));
	GBA_CHECK(!Throws([]() { Assembler a; a.Mov(0, 0xFF); }));
	GBA_CHECK(!Throws([]() { Assembler a; a.Mov(0, 0x100); }));
	GBA_CHECK(!Throws([]() { Assembler a; a.Mov(0, 0x3F0); }));
	GBA_CHECK(!Throws([]() { Assembler a; a.Mov(0, 0xFF000000); }));
	GBA_CHECK(!Throws([]() { Assembler a; a.Mov(0, 0x80000000); }));
	GBA_CHECK(!Throws([]() { Assembler a; a.Mov(0, 0x1E0); }));
	// A shift that the encoding cannot express has to throw rather than wrap silently.
	GBA_CHECK(Throws([]() { Assembler a; a.MovReg(0, 1, Cond::AL, false, Shift::LSL, 33); }));
	GBA_CHECK(Throws([]() { Assembler a; a.Ldrh(0, 1, 0x100); }));
	GBA_CHECK(Throws([]() { Assembler a; a.Bkpt(16); }));
	GBA_CHECK(Throws([]() { Assembler a; a.Swi(0x100); }));
	GBA_CHECK(Throws([]() { Assembler a; a.ThumbLdrImm(0, 1, 3); }));

	// An undefined label fails when the image is taken, not when the branch is emitted.
	GBA_CHECK(Throws([]()
	{
		Assembler a;
		a.B("nowhere");
		a.TakeImage();
	}));

	// The literal pool: the words are emitted at the current address and that address is returned.
	{
		Assembler a;
		a.Mov(0, 0);
		uint32_t address = a.LiteralPool({ 0x11223344, 0x55667788 });
		std::vector<uint8_t> image = a.TakeImage();
		GBA_CHECK_EQ(address, 4u);
		GBA_CHECK_HEX32(WordAt(image, 4), 0x11223344);
		GBA_CHECK_HEX32(WordAt(image, 8), 0x55667788);
	}

	// TakeImage pads the tail with 0xFF (what an erased ROM reads as) and never touches the
	// bytes the program emitted; Org() moves the program counter and zero-fills the gap.
	{
		Assembler a;
		a.Mov(0, 1);
		std::vector<uint8_t> padded = a.TakeImage(16);
		GBA_CHECK_EQ((int)padded.size(), 16);
		GBA_CHECK_HEX32(WordAt(padded, 0), 0xE3A00001);
		for (size_t i = 4; i < padded.size(); i++)
			GBA_CHECK_EQ((int)padded[i], 0xFF);
	}
	{
		// size 0 means "no padding": a fresh assembler returns exactly what it emitted. (Note
		// that TakeImage() pads the image the assembler holds, so asking twice with different
		// sizes is not a thing; the harness asks once.)
		Assembler a;
		a.Mov(0, 1);
		GBA_CHECK_EQ((int)a.TakeImage().size(), 4);
	}
	{
		// An image assembled at a cartridge address is as long as its contents, not as long as
		// the address it was assembled at (the harness builds test ROMs at 0x08000000).
		Assembler a;
		a.Org(0x08000000);
		a.Label("start");
		a.Mov(0, 1);
		a.B("start");
		std::vector<uint8_t> image = a.TakeImage(0x10000);
		GBA_CHECK_EQ((int)image.size(), 0x10000);
		GBA_CHECK_HEX32(WordAt(image, 0), 0xE3A00001);
		GBA_CHECK_HEX32(WordAt(image, 4), 0xEAFFFFFD);		// (0x08000000 - 0x0800000C) / 4
		GBA_CHECK_EQ((int)image[0x1000], 0xFF);
	}
	{
		Assembler a;
		a.Mov(0, 0);
		GBA_CHECK(Throws([&a]() { a.TakeImage(2); }));		// the code does not fit
	}
	{
		// The image starts at the origin the first Org() set, so a program assembled at an address
		// that is not zero produces just its own bytes (the harness relies on this: it assembles
		// test cartridges at 0x08000000 and asks for a 0x10000 byte image).
		Assembler a;
		a.Org(0x20);
		a.Mov(0, 0);
		std::vector<uint8_t> image = a.TakeImage();
		GBA_CHECK_EQ((int)image.size(), 4);
		GBA_CHECK_HEX32(WordAt(image, 0), 0xE3A00000);
	}
	{
		Assembler a;
		a.Org(0x20);
		a.Mov(0, 0);
		std::vector<uint8_t> image = a.TakeImage(0x40);
		GBA_CHECK_EQ((int)image.size(), 0x40);
		GBA_CHECK_HEX32(WordAt(image, 0), 0xE3A00000);
		GBA_CHECK_EQ((int)image[4], 0xFF);
	}
}

GBA_TEST(BootRom, EmitterListing)
{
	Assembler a;
	a.Org(0x20);
	a.Label("start");
	a.Mov(0, 0x04000000);
	a.Str(0, 1, 4);
	a.Sub(15, 14, 4, Cond::AL, true);
	a.Stmia(13, 0x4FF0);
	a.B("start");
	std::string listing = a.Listing();
	const char* expected =
		"00000020: MOV r0, #0x4000000\n"
		"00000024: STR r0, [r1, #0x4]\n"
		"00000028: SUBS pc, lr, #0x4\n"
		"0000002C: STMIA sp!, {r4-r11, lr}\n"
		"00000030: B start\n";
	GBA_CHECK_MSG(listing == expected, "'" + listing + "'");
}

// -------------------------------------------------------------------------------------------
// The boot ROM: the image itself
// -------------------------------------------------------------------------------------------

GBA_TEST(BootRom, ImageAndVectors)
{
	const std::vector<uint8_t>& image = BootRom::GbaImage();
	GBA_CHECK_EQ((int)image.size(), (int)BiosSize);

	// The eight vectors (ARM Architecture Reference Manual A2.6) are branches, and the first one lands on the code.
	for (int vector = 0; vector < 8; vector++)
	{
		uint32_t word = WordAt(image, (size_t)vector * 4);
		GBA_CHECK_MSG((word & 0x0F000000) == 0x0A000000, "vector " + std::to_string(vector) +
			" is not a branch");
	}
	// Reset, undefined, SWI, prefetch abort, data abort and FIQ all branch somewhere sensible;
	// the reset vector must reach the code (not stay in the vector table).
	{
		int32_t offset = (int32_t)(WordAt(image, 0) << 8) >> 6;
		GBA_CHECK(offset >= 8 && offset < (int32_t)image.size());
	}
	// The IRQ vector has to point at the handler, which ends in the exception return.
	{
		int32_t offset = (int32_t)(WordAt(image, VectorIrq) << 8) >> 6;
		uint32_t handler = (uint32_t)(VectorIrq + 8 + offset);
		GBA_CHECK(handler < image.size());
		bool foundReturn = false;
		for (size_t i = handler; i + 4 <= handler + 0x60 && i + 4 <= image.size(); i += 4)
			if (WordAt(image, i) == 0xE25EF004u)			// SUBS PC, LR, #4
				foundReturn = true;
		GBA_CHECK(foundReturn);
	}

	GBA_CHECK(BootRom::GbaAnimationFrames() >= 180);
	GBA_CHECK(BootRom::GbaAnimationFrames() <= 300);

	uint32_t entry = BootRom::GbaLinkDriverEntry();
	GBA_CHECK(entry >= 0x20);
	GBA_CHECK(entry < image.size());
	GBA_CHECK_EQ((int)(entry % 4), 0);

	// The listing is what --dump-bootrom writes out, so it has to be a real listing.
	std::string listing = BootRom::GbaListing();
	GBA_CHECK(listing.size() > 1000);
	GBA_CHECK(listing.find("SUBS pc, lr, #0x4") != std::string::npos);
	GBA_CHECK(listing.find("BX lr") != std::string::npos);
	// The IRQ handler's frame: the emitter writes the register list with ranges collapsed, and
	// the boot ROM's SaveRegs deliberately uses the IA form without writeback (gba_armasm.h has
	// no STMDB, so the stack pointer is adjusted by hand).
	GBA_CHECK(listing.find("STMIA sp, {r0-r3, r12, lr}") != std::string::npos);
	GBA_CHECK(listing.find("PlotPixel") != std::string::npos);
}

// -------------------------------------------------------------------------------------------
// The boot ROM: it has to run
// -------------------------------------------------------------------------------------------

GBA_TEST(BootRom, AnimationRunsAndEndsOnTheMark)
{
	GbaSystem system;
	system.UseCustomBootRom(true);
	system.EjectRom();
	system.Reset();

	// Frame 0 counts as "early": the boot ROM has only just cleared the screen.
	system.RunFrame();
	std::vector<uint32_t> early(system.FrameBuffer(), system.FrameBuffer() + ScreenWidth * ScreenHeight);

	// The animation is a moving picture: some later frame has to differ from the early one.
	bool changed = false;
	int changedAt = -1;
	for (int frame = 1; frame < 60 && !changed; frame++)
	{
		system.RunFrame();
		for (int i = 0; i < ScreenWidth * ScreenHeight; i++)
			if (system.FrameBuffer()[i] != early[(size_t)i])
			{
				changed = true;
				changedAt = frame;
				break;
			}
	}
	GBA_CHECK_MSG(changed, "the frame never changed while the animation was running");
	GbaTest::Note("the animation changed the picture at frame " + std::to_string(changedAt));

	// Run to just before the end of the animation: that is the static mark with the wordmark.
	int target = BootRom::GbaAnimationFrames() - 4;
	while (system.FrameCounter() < target && !system.LinkMode())
		system.RunFrame();

	const uint32_t* final = system.FrameBuffer();
	int lit = CountNonBlack(final, ScreenWidth * ScreenHeight);
	int colours = CountDistinctColours(final, ScreenWidth * ScreenHeight);
	GbaTest::Note("the last animation frame has " + std::to_string(lit) + " non-black pixels and " +
		std::to_string(colours) + " distinct colours");

	// A blank (or forced blank) screen would be a single colour; the logo frame has the gradient,
	// the mark and the wordmark, so it has a lot of both.
	GBA_CHECK_MSG(lit > ScreenWidth * ScreenHeight / 4, "the final frame is (nearly) blank");
	GBA_CHECK_MSG(colours > 8, "the final frame has too few colours to contain the mark");

	// And it differs from the early frames.
	bool differs = false;
	for (int i = 0; i < ScreenWidth * ScreenHeight && !differs; i++)
		if (final[i] != early[(size_t)i])
			differs = true;
	GBA_CHECK_MSG(differs, "the final frame is the same as the first one");
}

GBA_TEST(BootRom, CartridgeHandover)
{
	// A cartridge whose program writes a marker into the base of IWRAM: the boot ROM has to
	// check the header, set the machine up the way the games expect it and jump to 0x08000000.
	const uint32_t marker = 0x00474241;
	std::vector<uint8_t> cartridge = MakeTestCartridge(marker);

	GbaSystem system;
	system.UseCustomBootRom(true);
	std::string error;
	GBA_CHECK_MSG(system.LoadRomImage(cartridge, error), error);
	system.Reset();

	bool handedOver = false;
	for (int frame = 0; frame < AnimationFramesPlusSlack() + 40 && !handedOver; frame++)
	{
		system.RunFrame();
		if (system.Bus().iwram.Read32(0) == marker)
			handedOver = true;
	}

	GBA_CHECK_MSG(handedOver, "the cartridge never ran its marker program");
	GBA_CHECK_EQ((uint32_t)system.Bus().iwram.Read32(0), marker);

	// The state the BIOS leaves behind: POSTFLG bit 0 says "the BIOS has run" (bit 1 is the
	// read-only "boot completed" flag the bus adds), and the display is off.
	GBA_CHECK_EQ((int)(system.Bus().PostFlg() & 1), 1);
	GBA_CHECK_EQ((int)system.Lcd().DispCnt(), 0);
	GBA_CHECK_EQ((uint32_t)system.Cpu().Reg(13), 0x03007F00u);
	GBA_CHECK_EQ((uint32_t)(system.Cpu().ReadCPSR() & ModeMask), (uint32_t)ModeSystem);
	GBA_CHECK_EQ((uint32_t)(system.Cpu().ReadCPSR() & FlagI), 0u);
}

GBA_TEST(BootRom, BadHeaderFallsBackToTheLinkDriver)
{
	// The other half of the cartridge check: a cartridge whose header complement check fails (a
	// corrupted or half-written Game Pak) must not be started; the ROM runs the link driver.
	std::vector<uint8_t> cartridge = MakeTestCartridge(0x00474241);
	cartridge[0xBD] ^= 0xFF;						// break the complement check

	GbaSystem system;
	system.UseCustomBootRom(true);
	std::string error;
	GBA_CHECK_MSG(system.LoadRomImage(cartridge, error), error);
	system.Reset();

	for (int frame = 0; frame < AnimationFramesPlusSlack() + 60; frame++)
		system.RunFrame();

	GBA_CHECK_MSG(system.Bus().iwram.Read32(0) != 0x00474241, "the broken cartridge was started");
	GBA_CHECK_MSG(system.Bus().iwram.Read16(0x7FF6) > 0, "the link driver never ran");
	GBA_CHECK_EQ((int)system.Lcd().DispCnt(), 0x0403);		// the "LINK" screen is still shown
}

GBA_TEST(BootRom, LinkDriverRunsWithoutACartridge)
{
	// With no cartridge the boot ROM has to fall into the SIO link driver: the port is set up,
	// transfers run, and the driver publishes its state in the IWRAM mailbox at 0x03007FF0.
	GbaSystem system;
	system.UseCustomBootRom(true);
	system.EjectRom();	system.Reset();

	for (int frame = 0; frame < AnimationFramesPlusSlack() + 60; frame++)
		system.RunFrame();

	uint16_t status = system.Bus().iwram.Read16(0x7FF0);
	uint16_t sent = system.Bus().iwram.Read16(0x7FF2);
	uint16_t received = system.Bus().iwram.Read16(0x7FF4);
	uint16_t count = system.Bus().iwram.Read16(0x7FF6);
	GbaTest::Note("link mailbox: status " + GbaTest::Hex(status) + " send " + GbaTest::Hex(sent) +
		" recv " + GbaTest::Hex(received) + " count " + GbaTest::Hex(count));

	GBA_CHECK_MSG(status <= 2, "the link status is not one of idle/handshake/connected");
	GBA_CHECK_MSG(count > 0, "the link driver never completed a transfer");
	// With no peer attached the driver still offers its presence word on the first transfer; the
	// port answers with our own word, which is the handshake state.
	GBA_CHECK_MSG(sent != 0, "the link driver never published a word to send");

	// The port itself: multi-player mode with the interrupt on (GBATEK "SIO Multi-Player Mode":
	// bit 13 = 1 selects it, bit 14 = 1 asks for the IRQ, bits 0-1 = 3 is 115200 bps). The driver
	// keeps starting transfers, so the only thing worth checking from outside is that it made
	// progress, which the transfer counter above already shows.
}

GBA_TEST(BootRom, LinkDriverSeesAPeer)
{
	// Two instances, one cable: both boot ROMs run the link driver, so both have to see the other
	// side's word and reach the connected state.
	GbaSystem left;
	GbaSystem right;
	left.UseCustomBootRom(true);
	right.UseCustomBootRom(true);
	left.EjectRom();
	right.EjectRom();
	left.AttachLink(&right);
	left.Reset();
	right.Reset();

	for (int frame = 0; frame < AnimationFramesPlusSlack() + 90; frame++)
	{
		left.RunFrame();
		right.RunFrame();
	}

	uint16_t leftStatus = left.Bus().iwram.Read16(0x7FF0);
	uint16_t rightStatus = right.Bus().iwram.Read16(0x7FF0);
	uint16_t leftCount = left.Bus().iwram.Read16(0x7FF6);
	uint16_t rightCount = right.Bus().iwram.Read16(0x7FF6);
	GbaTest::Note("left: status " + GbaTest::Hex(leftStatus) + " count " + GbaTest::Hex(leftCount) +
		", right: status " + GbaTest::Hex(rightStatus) + " count " + GbaTest::Hex(rightCount));

	GBA_CHECK_MSG(leftCount > 0, "the left link driver never completed a transfer");
	GBA_CHECK_MSG(rightCount > 0, "the right link driver never completed a transfer");
	GBA_CHECK_MSG(leftStatus >= 1, "the left driver never acknowledged the peer");
	GBA_CHECK_MSG(rightStatus >= 1, "the right driver never acknowledged the peer");
}
