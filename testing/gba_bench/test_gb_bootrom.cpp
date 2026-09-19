// The LR35902 emitter and the pureikyubu boot ROM.
//
// Two things are checked here. First the emitter's encodings, one instruction at a time against
// the Pan Docs instruction tables, because the boot ROM's meaning depends on them; then the ROM
// itself: that it is exactly 256 bytes, that it leaves the machine in the documented post-boot
// state, that its wordmark animation really appears and moves on the LCD, and that it reaches a
// cartridge (a tiny one assembled here, whose program writes a marker into WRAM).

#include "gba_test.h"
#include "gb.h"
#include "gb_asm.h"
#include "gb_bootrom.h"

#include <string>
#include <vector>

using namespace GBA;

namespace
{
	/// <summary>Assemble one instruction and return its bytes.</summary>
	template <typename Emit>
	std::vector<uint8_t> Encode(Emit emit)
	{
		GbAsm::Assembler assembler;
		emit(assembler);
		return assembler.TakeImage();
	}

	/// <summary>Compare two strings (the framework's GBA_CHECK_EQ is for numbers).</summary>
	void CheckString(const std::string& actual, const std::string& expected, const char* what)
	{
		GBA_CHECK_MSG(actual == expected, std::string(what) + ": got \"" + actual + "\", expected \""
			+ expected + "\"");
	}

	/// <summary>The frame buffer's colour for the settled wordmark's ink.</summary>
	const uint32_t LightShade = 0xFF9BBC0F;

	/// <summary>
	/// Count the pixels the boot ROM has drawn: everything that is neither the palette's lightest
	/// shade (the blank background) nor the "LCD never finished a frame" white the frame buffer
	/// starts as.
	/// </summary>
	int InkPixels(const GbSystem& machine)
	{
		const uint32_t* frame = machine.FrameBuffer();
		int count = 0;
		for (int i = 0; i < GbScreenWidth * GbScreenHeight; i++)
			if (frame[i] != LightShade && frame[i] != 0xFFFFFFFF)
				count++;
		return count;
	}

	/// <summary>The screen X of the leftmost inked pixel, or -1 when the screen is blank.</summary>
	int LeftmostInk(const GbSystem& machine)
	{
		const uint32_t* frame = machine.FrameBuffer();
		for (int x = 0; x < GbScreenWidth; x++)
			for (int y = 0; y < GbScreenHeight; y++)
				if (frame[y * GbScreenWidth + x] != LightShade && frame[y * GbScreenWidth + x] != 0xFFFFFFFF)
					return x;
		return -1;
	}

	/// <summary>
	/// A tiny cartridge: a valid header (so the boot ROM hands over) whose program at 0x0100
	/// writes 0xA5 into 0xC000 and then spins. It is assembled with the same emitter the boot ROM
	/// uses, so the test also drives the emitter.
	/// </summary>
	std::vector<uint8_t> BuildTestCartridge()
	{
		GbAsm::Assembler a;

		// The entry point at 0x0100: LD A,0xA5 ; LD (0xC000),A ; then an endless JR -2.
		a.Org(0x0100);
		a.Ld(GbAsm::R8::A, 0xA5);
		a.Ld16A(0xC000);
		a.Label("spin");
		a.Jr("spin");

		std::vector<uint8_t> image = a.TakeImage(0x8000, 0x00);

		// The header the boot ROM (and GbCart) reads.
		image[GbHeaderTitle + 0] = 'P';
		image[GbHeaderTitle + 1] = 'K';
		image[GbHeaderCartType] = 0x00;			// ROM only
		image[GbHeaderRomSize] = 0x00;			// 32 KByte
		image[GbHeaderRamSize] = 0x00;
		image[GbHeaderChecksum] = GbComputeHeaderChecksum(image);
		return image;
	}
}

// ---------------------------------------------------------------------------------------
// The emitter
// ---------------------------------------------------------------------------------------

GBA_TEST(GbBootRom, emitter_encodes_the_load_family)
{
	// LD r16,n16 is 00 rr 0001 with the 16-bit operand little-endian (Pan Docs "Block 0").
	std::vector<uint8_t> bc = Encode([](GbAsm::Assembler& a) { a.Ld16(GbAsm::R16::BC, 0x1234); });
	GBA_CHECK_EQ(bc.size(), 3u);
	GBA_CHECK_EQ(bc[0], 0x01);
	GBA_CHECK_EQ(bc[1], 0x34);
	GBA_CHECK_EQ(bc[2], 0x12);

	std::vector<uint8_t> de = Encode([](GbAsm::Assembler& a) { a.Ld16(GbAsm::R16::DE, 0xABCD); });
	GBA_CHECK_EQ(de[0], 0x11);
	GBA_CHECK_EQ(de[2], 0xAB);

	std::vector<uint8_t> sp = Encode([](GbAsm::Assembler& a) { a.Ld16(GbAsm::R16::SP, 0xFFFE); });
	GBA_CHECK_EQ(sp[0], 0x31);
	GBA_CHECK_EQ(sp[1], 0xFE);
	GBA_CHECK_EQ(sp[2], 0xFF);

	std::vector<uint8_t> hl = Encode([](GbAsm::Assembler& a) { a.Ld16(GbAsm::R16::HL, 0x8000); });
	GBA_CHECK_EQ(hl[0], 0x21);

	// LD r8,r8 is 01 ddd sss: LD A,B is 0x78, LD (HL),A is 0x77.
	std::vector<uint8_t> ab = Encode([](GbAsm::Assembler& a) { a.Ld(GbAsm::R8::A, GbAsm::R8::B); });
	GBA_CHECK_EQ(ab.size(), 1u);
	GBA_CHECK_EQ(ab[0], 0x78);

	std::vector<uint8_t> hlA = Encode([](GbAsm::Assembler& a) { a.Ld(GbAsm::R8::HL, GbAsm::R8::A); });
	GBA_CHECK_EQ(hlA[0], 0x77);

	// LD r8,n8 is 00 ddd 110: LD B,0x12 is 0x06 0x12, and LD (HL),n8 is 0x36.
	std::vector<uint8_t> bn = Encode([](GbAsm::Assembler& a) { a.Ld(GbAsm::R8::B, 0x12); });
	GBA_CHECK_EQ(bn[0], 0x06);
	GBA_CHECK_EQ(bn[1], 0x12);

	std::vector<uint8_t> hln = Encode([](GbAsm::Assembler& a) { a.Ld(GbAsm::R8::HL, 0x00); });
	GBA_CHECK_EQ(hln[0], 0x36);
	GBA_CHECK_EQ(hln[1], 0x00);

	// LDH (n8),A is 0xE0 and LDH A,(n8) is 0xF0; the (C) forms are 0xE2 and 0xF2.
	std::vector<uint8_t> out = Encode([](GbAsm::Assembler& a) { a.LdA8(0x40); });
	GBA_CHECK_EQ(out[0], 0xE0);
	GBA_CHECK_EQ(out[1], 0x40);

	std::vector<uint8_t> in = Encode([](GbAsm::Assembler& a) { a.Ld8A(0x44); });
	GBA_CHECK_EQ(in[0], 0xF0);
	GBA_CHECK_EQ(in[1], 0x44);

	// LD A,(DE) and LD (DE),A are the accumulator-only memory forms (0x1A and 0x12).
	std::vector<uint8_t> lde = Encode([](GbAsm::Assembler& a) { a.LdADE(); });
	GBA_CHECK_EQ(lde[0], 0x1A);
	std::vector<uint8_t> sde = Encode([](GbAsm::Assembler& a) { a.LdDEA(); });
	GBA_CHECK_EQ(sde[0], 0x12);

	// LD (HL+),A and LD A,(HL+) are 0x22 and 0x2A; LD (n16),SP is the 0x08 form.
	std::vector<uint8_t> inc = Encode([](GbAsm::Assembler& a) { a.LdAHLI(); });
	GBA_CHECK_EQ(inc[0], 0x22);
	std::vector<uint8_t> dec = Encode([](GbAsm::Assembler& a) { a.LdAHLA(); });
	GBA_CHECK_EQ(dec[0], 0x2A);
	std::vector<uint8_t> store = Encode([](GbAsm::Assembler& a) { a.Ld16SP(0xC000); });
	GBA_CHECK_EQ(store.size(), 3u);
	GBA_CHECK_EQ(store[0], 0x08);
	GBA_CHECK_EQ(store[1], 0x00);
	GBA_CHECK_EQ(store[2], 0xC0);
}

GBA_TEST(GbBootRom, emitter_encodes_the_arithmetic_and_branches)
{
	// The arithmetic family is a base byte plus the r8 code: ADD A,B is 0x80, CP (HL) is 0xBE.
	std::vector<uint8_t> add = Encode([](GbAsm::Assembler& a) { a.Add(GbAsm::R8::B); });
	GBA_CHECK_EQ(add[0], 0x80);
	std::vector<uint8_t> addn = Encode([](GbAsm::Assembler& a) { a.Add(0x04); });
	GBA_CHECK_EQ(addn[0], 0xC6);
	GBA_CHECK_EQ(addn[1], 0x04);
	std::vector<uint8_t> cp = Encode([](GbAsm::Assembler& a) { a.Cp(GbAsm::R8::HL); });
	GBA_CHECK_EQ(cp[0], 0xBE);
	std::vector<uint8_t> sub = Encode([](GbAsm::Assembler& a) { a.Sub(GbAsm::R8::A); });
	GBA_CHECK_EQ(sub[0], 0x97);

	// INC r8 is 0x04 + r8*8, DEC r8 is 0x05 + r8*8; ADD SP,e8 is 0xE8.
	std::vector<uint8_t> inc = Encode([](GbAsm::Assembler& a) { a.Inc(GbAsm::R8::A); });
	GBA_CHECK_EQ(inc[0], 0x3C);
	std::vector<uint8_t> dec = Encode([](GbAsm::Assembler& a) { a.Dec(GbAsm::R8::B); });
	GBA_CHECK_EQ(dec[0], 0x05);
	std::vector<uint8_t> spe = Encode([](GbAsm::Assembler& a) { a.AddSPe(0xFF); });
	GBA_CHECK_EQ(spe[0], 0xE8);
	GBA_CHECK_EQ(spe[1], 0xFF);

	// The stack forms: PUSH BC is 0xC5, POP AF is 0xF1.
	std::vector<uint8_t> push = Encode([](GbAsm::Assembler& a) { a.Push(GbAsm::R16Stk::BC); });
	GBA_CHECK_EQ(push[0], 0xC5);
	std::vector<uint8_t> pop = Encode([](GbAsm::Assembler& a) { a.Pop(GbAsm::R16Stk::AF); });
	GBA_CHECK_EQ(pop[0], 0xF1);

	// JR is 0x18 with a signed offset measured from the byte after it; JR NZ is 0x20.
	std::vector<uint8_t> jr = Encode([](GbAsm::Assembler& a) { a.Label("here"); a.Jr(-2); });
	GBA_CHECK_EQ(jr[0], 0x18);
	GBA_CHECK_EQ(jr[1], 0xFE);
	std::vector<uint8_t> jrnz = Encode([](GbAsm::Assembler& a) { a.Label("top"); a.Jr(GbAsm::Cond::NZ, "top"); });
	GBA_CHECK_EQ(jrnz[0], 0x20);
	GBA_CHECK_EQ(jrnz[1], 0xFE);			// -2: back to the JR itself

	// A forward JR is resolved by the label pass.
	std::vector<uint8_t> forward = Encode([](GbAsm::Assembler& a)
	{
		a.Jr("end");
		a.Nop();
		a.Label("end");
		a.Nop();
	});
	GBA_CHECK_EQ(forward[0], 0x18);
	GBA_CHECK_EQ(forward[1], 0x01);			// skip the NOP

	// JP, CALL, RET, RETI and RST.
	std::vector<uint8_t> jp = Encode([](GbAsm::Assembler& a) { a.Jp(0x0100); });
	GBA_CHECK_EQ(jp.size(), 3u);
	GBA_CHECK_EQ(jp[0], 0xC3);
	std::vector<uint8_t> call = Encode([](GbAsm::Assembler& a) { a.Call(0x0200); });
	GBA_CHECK_EQ(call[0], 0xCD);
	std::vector<uint8_t> ret = Encode([](GbAsm::Assembler& a) { a.Ret(); });
	GBA_CHECK_EQ(ret[0], 0xC9);
	std::vector<uint8_t> reti = Encode([](GbAsm::Assembler& a) { a.Reti(); });
	GBA_CHECK_EQ(reti[0], 0xD9);
	std::vector<uint8_t> rst = Encode([](GbAsm::Assembler& a) { a.Rst(0x38); });
	GBA_CHECK_EQ(rst[0], 0xFF);

	// The miscellany: DAA, CPL, SCF, CCF, DI, EI, HALT, NOP and the two-byte STOP.
	GBA_CHECK_EQ(Encode([](GbAsm::Assembler& a) { a.Daa(); })[0], 0x27);
	GBA_CHECK_EQ(Encode([](GbAsm::Assembler& a) { a.Cpl(); })[0], 0x2F);
	GBA_CHECK_EQ(Encode([](GbAsm::Assembler& a) { a.Scf(); })[0], 0x37);
	GBA_CHECK_EQ(Encode([](GbAsm::Assembler& a) { a.Ccf(); })[0], 0x3F);
	GBA_CHECK_EQ(Encode([](GbAsm::Assembler& a) { a.Di(); })[0], 0xF3);
	GBA_CHECK_EQ(Encode([](GbAsm::Assembler& a) { a.Ei(); })[0], 0xFB);
	GBA_CHECK_EQ(Encode([](GbAsm::Assembler& a) { a.Halt(); })[0], 0x76);
	GBA_CHECK_EQ(Encode([](GbAsm::Assembler& a) { a.Nop(); })[0], 0x00);
	std::vector<uint8_t> stop = Encode([](GbAsm::Assembler& a) { a.Stop(); });
	GBA_CHECK_EQ(stop.size(), 2u);
	GBA_CHECK_EQ(stop[0], 0x10);
	GBA_CHECK_EQ(stop[1], 0x00);
}

GBA_TEST(GbBootRom, emitter_encodes_the_cb_family_and_tracks_labels)
{
	// The CB family is the prefix plus a base plus the r8 code: RLC B is CB 0x00, BIT 7,A is
	// CB 0x7F, SET 0,(HL) is CB 0xC6 (Pan Docs "CB prefix instructions").
	std::vector<uint8_t> rlc = Encode([](GbAsm::Assembler& a) { a.Rlc(GbAsm::R8::B); });
	GBA_CHECK_EQ(rlc.size(), 2u);
	GBA_CHECK_EQ(rlc[0], 0xCB);
	GBA_CHECK_EQ(rlc[1], 0x00);

	std::vector<uint8_t> bit = Encode([](GbAsm::Assembler& a) { a.Bit(7, GbAsm::R8::A); });
	GBA_CHECK_EQ(bit[1], 0x7F);

	std::vector<uint8_t> set = Encode([](GbAsm::Assembler& a) { a.Set(0, GbAsm::R8::HL); });
	GBA_CHECK_EQ(set[1], 0xC6);

	std::vector<uint8_t> res = Encode([](GbAsm::Assembler& a) { a.Res(3, GbAsm::R8::D); });
	GBA_CHECK_EQ(res[1], 0x9A);				// 0x80 + 3*8 + 2

	std::vector<uint8_t> sra = Encode([](GbAsm::Assembler& a) { a.Sra(GbAsm::R8::A); });
	GBA_CHECK_EQ(sra[1], 0x2F);
	std::vector<uint8_t> swap = Encode([](GbAsm::Assembler& a) { a.Swap(GbAsm::R8::A); });
	GBA_CHECK_EQ(swap[1], 0x37);
	std::vector<uint8_t> srl = Encode([](GbAsm::Assembler& a) { a.Srl(GbAsm::R8::A); });
	GBA_CHECK_EQ(srl[1], 0x3F);

	// The listing records one line per instruction and its address, and a label's address is used
	// by the Ld16 label form.
	GbAsm::Assembler a;
	a.Label("start");
	a.Nop();
	a.Nop();
	a.Label("here");
	a.Ld16(GbAsm::R16::HL, "start");
	std::vector<uint8_t> image = a.TakeImage();
	GBA_CHECK_EQ(image.size(), 5u);
	GBA_CHECK_EQ(image[2], 0x21);			// LD HL,
	GBA_CHECK_EQ(image[3], 0x00);			// start = 0x0000 (low byte)
	GBA_CHECK_EQ(image[4], 0x00);			// the high byte
	GBA_CHECK_MSG(a.Listing().find("ld r16,start") != std::string::npos,
		"the listing must name the label: " + a.Listing());
	GBA_CHECK_EQ(a.InstructionCount(), 3);

	// An undefined label is an error, not a silent zero.
	bool threw = false;
	try
	{
		GbAsm::Assembler bad;
		bad.Jp("nowhere");
		bad.TakeImage();
	}
	catch (const std::exception&)
	{
		threw = true;
	}
	GBA_CHECK_MSG(threw, "an undefined label must throw");
}

// ---------------------------------------------------------------------------------------
// The image
// ---------------------------------------------------------------------------------------

GBA_TEST(GbBootRom, image_is_exactly_256_bytes)
{
	const std::vector<uint8_t>& image = GbBootRom::DmgImage();
	GBA_CHECK_EQ(image.size(), 256u);

	// The CGB image is the same size, and the listing describes the same code.
	GBA_CHECK_EQ(GbBootRom::CgbImage().size(), 256u);

	const GbBootRom::Layout layout = GbBootRom::ImageLayout();
	GBA_CHECK_MSG(layout.codeEnd <= 256, "the code must fit: " + GbaTest::Hex(layout.codeEnd));
	GBA_CHECK_MSG(layout.tileData >= layout.codeEnd, "the tile data follows the code");
	GBA_CHECK_MSG(layout.wordmark >= layout.tileData, "the wordmark follows the tiles");
	GBA_CHECK_MSG(layout.imageEnd <= 256, "the whole image must fit: " + GbaTest::Hex(layout.imageEnd));

	// The ROM starts with the interrupt disable and the stack setup a boot ROM needs.
	GBA_CHECK_EQ(image[0], 0xF3);			// DI
	GBA_CHECK_EQ(image[1], 0x31);			// LD SP,n16
	GBA_CHECK_EQ(image[2], 0xFE);
	GBA_CHECK_EQ(image[3], 0xFF);

	// The listing is not empty and names the tile data.
	GBA_CHECK_MSG(!GbBootRom::Listing().empty(), "the listing must be produced");
	GBA_CHECK(GbBootRom::LetterCount() == 8);
	CheckString(GbBootRom::TileLetters(), "PUREIKYB", "the tile order");

	// The animation's length is the slide plus the hold, and both are finite.
	GBA_CHECK(GbBootRom::SlideFrames() > 0);
	GBA_CHECK(GbBootRom::AnimationFrames() > GbBootRom::SlideFrames());
}

GBA_TEST(GbBootRom, letters_are_eight_by_eight_glyphs)
{
	// Every letter has ink somewhere and blank rows at the bottom (the common baseline).
	for (int letter = 0; letter < GbBootRom::LetterCount(); letter++)
	{
		int ink = 0;
		for (int y = 0; y < 8; y++)
			for (int x = 0; x < 8; x++)
				ink += GbBootRom::LetterPixel(letter, x, y);
		GBA_CHECK_MSG(ink > 8, "letter " + std::to_string(letter) + " must have ink");

		for (int x = 0; x < 8; x++)
		{
			GBA_CHECK_EQ(GbBootRom::LetterPixel(letter, x, 6), 0);
			GBA_CHECK_EQ(GbBootRom::LetterPixel(letter, x, 7), 0);
		}
	}

	// Out of range reads are safe.
	GBA_CHECK_EQ(GbBootRom::LetterPixel(-1, 0, 0), 0);
	GBA_CHECK_EQ(GbBootRom::LetterPixel(99, 0, 0), 0);
	GBA_CHECK_EQ(GbBootRom::LetterPixel(0, 8, 0), 0);
}

// ---------------------------------------------------------------------------------------
// Running the ROM
// ---------------------------------------------------------------------------------------

GBA_TEST(GbBootRom, wordmark_appears_and_moves)
{
	GbSystem machine;
	machine.ApplySettings(GbSettings::Defaults());
	machine.Reset();

	// The wordmark's left edge starts off the right of the screen (screen X 168) and SCX slides it
	// four pixels a frame until it settles at screen X 40, so the leftmost inked pixel must walk
	// left across the frames and stop there.
	int firstInkFrame = -1;
	int firstLeft = -1;
	int settledLeft = -1;
	int settledInk = 0;
	int previousLeft = 1000;

	for (int frame = 0; frame < GbBootRom::AnimationFrames() + 2; frame++)
	{
		machine.RunFrame();

		int ink = InkPixels(machine);
		int left = LeftmostInk(machine);

		if (firstInkFrame < 0 && ink > 0)
		{
			firstInkFrame = frame;
			firstLeft = left;
		}
		if (firstInkFrame >= 0 && left >= 0)
		{
			// Once it is on the screen it only ever moves left (or stays settled).
			GBA_CHECK_MSG(left <= previousLeft, "the wordmark moved right at frame "
				+ std::to_string(frame) + ": " + std::to_string(previousLeft) + " -> "
				+ std::to_string(left));
			previousLeft = left;
		}
		if (frame == GbBootRom::SlideFrames() + 4)
		{
			settledLeft = left;
			settledInk = ink;
		}
	}

	GBA_CHECK_MSG(firstInkFrame >= 0, "the wordmark must appear");
	GBA_CHECK_MSG(firstInkFrame <= 8, "the wordmark appears within a few frames, got "
		+ std::to_string(firstInkFrame));
	GBA_CHECK_MSG(firstLeft > 140, "the wordmark slides in from the right edge, it first showed at "
		+ std::to_string(firstLeft));

	// It has settled by the end of the slide, at the position the map and SCX geometry give:
	// map X 24 minus SCX 240, wrapped through the 256 pixel map, is screen X 40. The 'P' whose
	// tile is the leftmost one has a blank leftmost column, so the leftmost *inked* pixel is one
	// pixel to the right of that.
	int leftBearing = 0;
	while (leftBearing < 7 && GbBootRom::LetterPixel(0, leftBearing, 0) == 0)
		leftBearing++;
	GBA_CHECK_EQ(settledLeft, 40 + leftBearing);
	GBA_CHECK_MSG(settledInk > 100, "the settled wordmark is on the screen, ink = "
		+ std::to_string(settledInk));

	// With no cartridge the ROM keeps running (it cannot hand over), and the wordmark stays.
	GBA_CHECK_MSG(machine.Bus().BootRomMapped(), "with no cartridge the ROM keeps running");
	int finalInk = InkPixels(machine);
	GBA_CHECK_MSG(finalInk > 100, "the wordmark is still on the screen, ink = "
		+ std::to_string(finalInk));
}

GBA_TEST(GbBootRom, reaches_a_cartridge_and_leaves_the_post_boot_state)
{
	std::vector<uint8_t> cartridge = BuildTestCartridge();

	GbSystem machine;
	machine.ApplySettings(GbSettings::Defaults());
	std::string error;
	GBA_CHECK_MSG(machine.LoadRomImage(cartridge, error), "the test cartridge must load: " + error);
	machine.Reset();

	// Step the machine (one instruction a call at this size) until the boot ROM unmaps itself, so
	// the register state can be read before the cartridge's own first instruction overwrites it.
	int guard = 0;
	while (machine.Bus().BootRomMapped() && guard++ < 4000000)
		machine.RunCycles(4);

	GBA_CHECK_MSG(!machine.Bus().BootRomMapped(), "the boot ROM must have handed over");
	GBA_CHECK_EQ(machine.Cpu().pc, 0x0100);					// the cartridge's entry point
	GBA_CHECK_EQ(machine.BootRegisterA(), 0x01);
	GBA_CHECK_EQ(machine.Cpu().a, 0x01);
	GBA_CHECK_EQ(machine.Cpu().c, GbComputeHeaderChecksum(cartridge));
	GBA_CHECK_EQ(machine.Cpu().HL(), 0x014D);
	GBA_CHECK_EQ(machine.Cpu().sp, 0xFFFE);
	GBA_CHECK_EQ(machine.Cpu().f, 0xB0);					// Z set, N clear, H and C set

	// Let the cartridge's program run: it writes 0xA5 into 0xC000.
	machine.RunFrames(2);
	GBA_CHECK_MSG(machine.Bus().Peek(0xC000) == 0xA5,
		"the cartridge program must have run, 0xC000 = " + GbaTest::Hex(machine.Bus().Peek(0xC000)));

	// The screen was cleared before the hand over: the boot ROM's tiles and map are gone.
	GBA_CHECK_EQ(machine.Bus().Peek(0x9800), 0x00);
	GBA_CHECK_EQ(machine.Bus().Peek(0x8010), 0x00);
}

GBA_TEST(GbBootRom, direct_start_skips_the_animation)
{
	// The "post-boot state" path: the frontend can start the cartridge at 0x0100 with the
	// registers a boot ROM would leave, without running the animation.
	std::vector<uint8_t> cartridge = BuildTestCartridge();

	GbSystem machine;
	GbSettings settings = GbSettings::Defaults();
	settings.skipBootRom = true;
	machine.ApplySettings(settings);

	std::string error;
	GBA_CHECK_MSG(machine.LoadRomImage(cartridge, error), "the test cartridge must load: " + error);
	machine.Reset();

	GBA_CHECK_MSG(!machine.Bus().BootRomMapped(), "no boot ROM is mapped with skipBootRom");
	GBA_CHECK_EQ(machine.Cpu().pc, 0x0100);

	// The program runs immediately: a handful of frames is plenty.
	machine.RunFrames(4);
	GBA_CHECK_EQ(machine.Bus().Peek(0xC000), 0xA5);
	GBA_CHECK_EQ(machine.Bus().Peek(0xFF40), 0x91);			// LCDC as the boot ROM leaves it
	GBA_CHECK_EQ(machine.Bus().Peek(0xFF47), 0xFC);			// BGP
	GBA_CHECK_EQ(machine.Bus().Peek(0xFF24), 0x77);			// NR50
	GBA_CHECK_EQ(machine.Bus().Peek(0xFF25), 0xF3);			// NR51
}

GBA_TEST(GbBootRom, cgb_boot_rom_is_split_around_the_cartridge_header)
{
	// The CGB's boot ROM is split in two parts with the cartridge header in the middle (Pan Docs
	// "Power Up Sequence" "Size"): 0x0000-0x00FF and 0x0200-0x08FF are the ROM, while
	// 0x0100-0x01FF keeps reading the cartridge the whole time the ROM is mapped. A colour boot
	// ROM therefore sees the real header, not the unused middle of its own image.

	// A cartridge with markers inside the header region.
	std::vector<uint8_t> cartridge = BuildTestCartridge();
	cartridge[0x0140] = 0xAB;			// a marker in the middle of the header region
	cartridge[0x01FF] = 0xCD;			// the last byte of the region

	// A fake 2304 byte CGB boot ROM: one value in the first half, another in the second, and a
	// third in the file's own middle - the "hole" - that must never be read.
	std::vector<uint8_t> rom(0x900, 0x00);
	for (int i = 0x000; i < 0x100; i++) rom[i] = 0x11;
	for (int i = 0x100; i < 0x200; i++) rom[i] = 0x22;
	for (int i = 0x200; i < 0x900; i++) rom[i] = 0x33;

	GbSystem machine;
	GbSettings settings = GbSettings::Defaults();
	settings.cgb = true;
	machine.ApplySettings(settings);

	std::string error;
	GBA_CHECK_MSG(machine.LoadRomImage(cartridge, error), "the test cartridge must load: " + error);
	machine.Reset();

	machine.Bus().SetBootRom(rom.data(), (uint32_t)rom.size());
	machine.Bus().MapBootRom(true);

	// The two halves are the ROM's own bytes...
	GBA_CHECK_EQ(machine.Bus().ReadByte(0x0000), 0x11);
	GBA_CHECK_EQ(machine.Bus().ReadByte(0x00FF), 0x11);
	GBA_CHECK_EQ(machine.Bus().ReadByte(0x0200), 0x33);
	GBA_CHECK_EQ(machine.Bus().ReadByte(0x08FF), 0x33);

	// ...and the header in the middle reads the cartridge, not the ROM's hole.
	GBA_CHECK_EQ(machine.Bus().ReadByte(0x0100), cartridge[0x0100]);
	GBA_CHECK_EQ(machine.Bus().ReadByte(0x0140), 0xAB);
	GBA_CHECK_EQ(machine.Bus().ReadByte(0x01FF), 0xCD);
}

GBA_TEST(GbBootRom, dmg_cartridge_on_a_cgb_selects_the_compatibility_mode)
{
	// The machine, not the PPU, decides the compatibility mode from the cartridge's CGB flag
	// (0x0143 bit 7): a monochrome cartridge on a CGB runs with the DMG display rules and OPRI
	// set, exactly as the real CGB boot ROM sets it up (Pan Docs "Power Up Sequence").
	std::vector<uint8_t> cartridge = BuildTestCartridge();
	GBA_CHECK_MSG((cartridge[GbHeaderCgbFlag] & 0x80) == 0, "the test cartridge is monochrome");

	GbSystem machine;
	GbSettings settings = GbSettings::Defaults();
	settings.cgb = true;
	machine.ApplySettings(settings);

	std::string error;
	GBA_CHECK_MSG(machine.LoadRomImage(cartridge, error), "the test cartridge must load: " + error);
	machine.Reset();

	GBA_CHECK_MSG(machine.Lcd().Cgb(), "the console is a CGB");
	GBA_CHECK_MSG(machine.Lcd().DmgCompat(), "a monochrome cartridge on a CGB is in compatibility mode");
	GBA_CHECK_MSG(machine.Lcd().DmgObjectPriority(), "OPRI selects the DMG object priority");
	GBA_CHECK_EQ(machine.Bus().Peek(0xFF6C), 0x01);

	// A cartridge that asks for CGB functions is not: the CGB's own rules (and its OAM object
	// priority) apply.
	cartridge[GbHeaderCgbFlag] = 0x80;
	GBA_CHECK_MSG(machine.LoadRomImage(cartridge, error), "the colour cartridge must load: " + error);
	machine.Reset();

	GBA_CHECK_MSG(!machine.Lcd().DmgCompat(), "a CGB cartridge is not in compatibility mode");
	GBA_CHECK_MSG(!machine.Lcd().DmgObjectPriority(), "and OPRI stays at the CGB's OAM order");
	GBA_CHECK_EQ(machine.Bus().Peek(0xFF6C), 0x00);
}
