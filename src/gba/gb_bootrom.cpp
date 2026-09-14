// The pureikyubu Game Boy boot ROM, assembled by the emitter in gb_asm.h. See gb_bootrom.h for
// what the image contains and what it does; this file is the ROM's source.
//
// The image is exactly 256 bytes and the code plus its tables have to fit in that, so the
// animation is done with the background scroller rather than with sprites:
//
//   * the eight tiles of "pureikyubu" (P U R E I K Y B - the two U's share a tile) are copied
//     into VRAM as tiles 1..8, and tile 0 stays blank because VRAM is cleared first;
//   * the ten letters are written into one row of the background map with those tile indices;
//   * SCX then walks from 112 to 240 four pixels a frame, which slides the wordmark in from the
//     right edge until it settles 40 pixels from the left, and holds it there;
//   * the screen is cleared and the ROM jumps to the cartridge (or spins with the wordmark still
//     on screen when there is no cartridge).
//
// Every instruction below is written the way the hardware reads it, and the comments name the
// rule it implements (the Pan Docs' register semantics, the tile format, the OAM coordinate bias).
// Every loop is bounded by a compile time constant (8 tiles, 32 slide frames, 90 hold frames), so
// the ROM always terminates.

#include "gb_bootrom.h"
#include "gb_asm.h"

namespace GBA
{
	namespace GbBootRom
	{
		using namespace GbAsm;

		namespace
		{
			// -------------------------------------------------------------------------------
			// The wordmark
			// -------------------------------------------------------------------------------

			// "pureikyubu" needs eight distinct tiles. Each letter is six rows of one byte (the
			// seventh and eighth rows of its tile stay zero, which puts the letters on a common
			// baseline); within a row bit 7 is the leftmost pixel (Pan Docs "Tile Data").
			const int LetterTileCount = 8;
			const int LetterRows = 6;
			const int LetterTilesBytes = LetterTileCount * LetterRows;		// 48

			const u8 Letters[LetterTileCount][LetterRows] =
			{
				{ 0x7C, 0x42, 0x42, 0x7C, 0x40, 0x40 },		// P
				{ 0x42, 0x42, 0x42, 0x42, 0x42, 0x7E },		// U
				{ 0x7C, 0x42, 0x42, 0x7C, 0x48, 0x46 },		// R
				{ 0x7E, 0x40, 0x40, 0x78, 0x40, 0x7E },		// E
				{ 0x7E, 0x18, 0x18, 0x18, 0x18, 0x7E },		// I
				{ 0x42, 0x44, 0x48, 0x70, 0x48, 0x44 },		// K
				{ 0x66, 0x66, 0x18, 0x18, 0x18, 0x18 },		// Y
				{ 0x7C, 0x42, 0x42, 0x7C, 0x42, 0x7C },		// B
			};

			/// <summary>Which letter of the wordmark each tile index draws.</summary>
			const char* const TileLetterNames = "PUREIKYB";

			/// <summary>The ten letters of "pureikyubu" as the tile indices they were copied to
			/// (tile 0 is the blank one).</summary>
			const u8 Wordmark[10] = { 1, 2, 3, 4, 5, 6, 7, 2, 8, 2 };

			const int WordmarkLength = 10;

			// The wordmark lives in map row 8 (0x9800 + 8 * 32 = 0x9900), columns 3..12, which is
			// the 80 pixel band at screen rows 64..71. SCX slides it: the left edge's screen X is
			// (24 - SCX) modulo 256, so SCX = 112 puts it at 168 (off the right edge) and SCX =
			// 240 puts it at 40, the settled position.
			const u16 MapRow = 0x9900;
			const int MapColumn = 3;
			const int SlideStartScx = 112;
			const int SlideEndScx = 240;
			const int SlideStep = 4;
			const int HoldFrames = 90;

			/// <summary>
			/// The sound registers the boot ROM initialises, in the order the real DMG boot ROM
			/// leaves them (Pan Docs "Power Up Sequence"): the four channels silenced, the mixer
			/// at both master volumes of 7.
			/// </summary>
			const u8 SoundInit[][2] =
			{
				{ 0x26, 0x80 },		// NR52: the APU on (it is on at power-up; this states it)
				{ 0x10, 0x80 },		// NR10: no sweep (bit 7 reads one)
				{ 0x11, 0xBF },		// NR11: duty 50%, length load 0x3F
				{ 0x12, 0xF3 },		// NR12: volume 15, decreasing, envelope period 3
				{ 0x14, 0xBF },		// NR14: trigger and length enable, as after a real boot
				{ 0x25, 0xF3 },		// NR51: all four channels into both outputs
				{ 0x24, 0x77 },		// NR50: both master volumes at 7
			};

			const int SoundInitEntries = (int)(sizeof(SoundInit) / sizeof(SoundInit[0]));

			/// <summary>Where the parts of the image ended up (the tests and the report use it).</summary>
			struct Placements
			{
				u16 codeEnd = 0;
				u16 tileData = 0;
				u16 wordmark = 0;
				u16 imageEnd = 0;
			};

			struct Built
			{
				std::vector<u8> image;
				std::string listing;
				Placements layout;
			};

			Built Build()
			{
				Assembler a;

				// -- 0x0000: reset ----------------------------------------------------------

				// The hardware starts with interrupts disabled, the boot ROM mapped and the LCD
				// off; the stack goes to the top of HRAM first.
				a.Di();
				a.Ld16(R16::SP, 0xFFFE);
				a.Xor(R8::A);
				a.LdA8(0x0F);						// IF = 0
				a.Ld16A(0xFF0F);
				a.LdA8(0x00);
				a.Ld16A(0xFFFF);					// IE = 0

				// -- the sound registers ----------------------------------------------------

				// The real boot ROM silences the channels before handing over. Each write is two
				// instructions (LD A,n8 then LDH (n8),A), which is cheaper in a 256 byte image
				// than a table walk.
				for (int i = 0; i < SoundInitEntries; i++)
				{
					a.Ld(R8::A, SoundInit[i][1]);
					a.LdA8(SoundInit[i][0]);
				}

				// -- the screen setup -------------------------------------------------------

				// BGP (0xFF47) = 0xFC: colour 0 white through colour 3 black (Pan Docs
				// "Palettes"), the value the real boot ROM leaves. LCDC (0xFF40) = 0x91: the LCD,
				// the background and the objects on, the 0x8000 tile data area and the 0x9800
				// maps (Pan Docs "LCDC"). The LCD is switched on here, after VRAM has been
				// cleared below, so nothing is drawn from the power-up garbage.
				a.Ld(R8::A, 0xFC);
				a.LdA8(0x47);
				a.Ld(R8::A, 0x00);
				a.LdA8(0x43);						// SCX = 0 while the tiles are copied
				a.LdA8(0x42);						// SCY = 0

				// Clear the whole background map (0x9800..0x9BFF) to the blank tile 0, so the
				// screen is the palette's lightest shade wherever the wordmark is not.
				a.Ld16(R16::HL, 0x9800);
				a.Ld16(R16::BC, 0x0400);			// 1024 bytes
				a.Label("ClearMap");
				a.Ld(R8::HL, 0x00);					// LD (HL),n8: tile 0, the blank tile
				a.Inc16(R16::HL);
				a.Dec16(R16::BC);
				a.Ld(R8::A, R8::B);
				a.Or(R8::C);
				a.Jr(Cond::NZ, "ClearMap");

				// The tiles are copied to VRAM with an outer loop over the eight letters and an
				// inner loop over its six rows: each row is two bytes in VRAM (the low bitplane
				// carries the letter, the high plane is zero, Pan Docs "Tile Data"), and the
				// destination therefore advances two bytes a row. The two blank rows of every
				// tile are skipped by the outer loop.
				a.Ld16(R16::HL, 0x8010);			// tile 1 (tile 0 stays blank)
				a.Ld16(R16::DE, "TileData");
				a.Ld(R8::B, LetterTileCount);
				a.Label("TileLoop");
				a.Ld(R8::C, LetterRows);
				a.Label("RowLoop");
				a.LdADE();							// LD A,(DE): the letter's row
				a.Ld(R8::HL, R8::A);
				a.Inc16(R16::HL);
				a.Inc16(R16::DE);
				a.Ld(R8::HL, 0x00);					// the high bitplane of every row
				a.Inc16(R16::HL);
				a.Dec(R8::C);
				a.Jr(Cond::NZ, "RowLoop");
				a.Inc16(R16::HL);					// skip the tile's two blank rows
				a.Inc16(R16::HL);
				a.Inc16(R16::HL);
				a.Inc16(R16::HL);
				a.Dec(R8::B);
				a.Jr(Cond::NZ, "TileLoop");

				// The background map row 8 spells out the wordmark: the ten tile indices are
				// copied into 0x9903..0x990C. The LCD is switched on after this so the first
				// visible frame is already the complete wordmark (scrolled off at SCX = 112).
				a.Ld16(R16::HL, MapRow + MapColumn);
				a.Ld16(R16::DE, "Wordmark");
				a.Ld(R8::B, WordmarkLength);
				a.Label("WordmarkLoop");
				a.LdADE();
				a.Ld(R8::HL, R8::A);
				a.Inc16(R16::HL);
				a.Inc16(R16::DE);
				a.Dec(R8::B);
				a.Jr(Cond::NZ, "WordmarkLoop");

				a.Ld(R8::A, 0x91);					// LCDC: LCD on, BG on, OBJ on, 0x8000 data
				a.LdA8(0x40);

				// -- the slide --------------------------------------------------------------

				// SCX walks from 112 to 240 four pixels a frame, which slides the wordmark in
				// from the right edge; the loop stops after the frame that stores 240 (the carry
				// from "240 + 4 = 244" is clear, so the comparison ends it).
				a.Ld(R8::A, SlideStartScx);
				a.Label("Slide");
				a.LdA8(0x43);						// SCX
				a.Push(R16Stk::AF);
				a.Call("WaitVBlank");
				a.Pop(R16Stk::AF);
				a.Add(SlideStep);
				a.Cp((u8)(SlideEndScx + 1));
				a.Jr(Cond::C, "Slide");

				// -- the hold ---------------------------------------------------------------

				// The wordmark stays where it settled for 90 frames.
				a.Ld(R8::B, HoldFrames);
				a.Label("Hold");
				a.Push(R16Stk::BC);
				a.Call("WaitVBlank");
				a.Pop(R16Stk::BC);
				a.Dec(R8::B);
				a.Jr(Cond::NZ, "Hold");

				// -- the hand over ----------------------------------------------------------

				// Read the cartridge header's type byte. 0xFF is what the bus returns with no
				// cartridge in the slot, so the ROM keeps the wordmark on the screen and spins
				// instead of jumping into empty memory (the task's "with no cartridge, spins with
				// the screen showing the emulator name").
				a.LdA16(0x0147);
				a.Cp(0xFF);
				a.Jr(Cond::Z, "NoCartridge");

				// Clear VRAM (0x8000..0x9FFF, tiles and both maps) the way the real boot ROM
				// leaves it, so the cartridge starts from the state it expects: a program that
				// only initialises the tiles it uses would otherwise find this ROM's letters.
				a.Ld16(R16::HL, 0x8000);
				a.Ld16(R16::BC, 0x2000);			// 8192 bytes
				a.Label("ClearVram");
				a.Ld(R8::HL, 0x00);
				a.Inc16(R16::HL);
				a.Dec16(R16::BC);
				a.Ld(R8::A, R8::B);
				a.Or(R8::C);
				a.Jr(Cond::NZ, "ClearVram");

				// The hand over itself: unmap the boot ROM by writing 0xFF50 (Pan Docs "Memory
				// Map") and jump to the cartridge's entry point. The register state a real boot
				// ROM leaves at 0x0100 (A = the console/cartridge kind, C = the header checksum,
				// HL = 0x014D, SP = 0xFFFE) is set by GbSystem when it sees the boot ROM unmap
				// itself, which is where the console kind is known; see GbSystem::PrepareBootState.
				a.Ld16(R16::SP, 0xFFFE);
				a.Ld(R8::A, 0x01);					// a DMG cartridge (the machine fixes this up)
				a.Xor(R8::A);
				a.LdA8(0x50);						// unmap the boot ROM
				a.Jp(0x0100);

				// No cartridge: the wordmark stays on the screen and the ROM spins on this
				// bounded wait loop (one VBlank a pass, so it keeps producing frames).
				a.Label("NoCartridge");
				a.Label("NoCartridgeSpin");
				a.Call("WaitVBlank");
				a.Ld16(R16::HL, "NoCartridgeSpin");
				a.JpHL();

				// -- the helpers ------------------------------------------------------------

				// WaitVBlank: wait until LY is 144 for the first time (Pan Docs "STAT": 144 is
				// where VBlank begins), then until it is not, so the caller is at the start of a
				// VBlank and gets one whole frame per call. Both loops look for one LY value, so
				// neither can spin forever.
				a.Label("WaitVBlank");
				a.Label("WaitVBlankStart");
				a.Ld8A(0x44);						// LDH A,(LY)
				a.Cp(0x90);
				a.Jr(Cond::NZ, "WaitVBlankStart");
				a.Label("WaitVBlankEnd");
				a.Ld8A(0x44);
				a.Cp(0x90);
				a.Jr(Cond::Z, "WaitVBlankEnd");
				a.Ret();

				// -- the data ---------------------------------------------------------------

				Placements layout;
				layout.codeEnd = a.PC();

				a.Label("TileData");
				layout.tileData = a.PC();
				for (int letter = 0; letter < LetterTileCount; letter++)
					for (int row = 0; row < LetterRows; row++)
						a.Data8(Letters[letter][row]);

				a.Label("Wordmark");
				layout.wordmark = a.PC();
				a.DataBytes(Wordmark, sizeof(Wordmark));

				layout.imageEnd = a.PC();

				Built built;
				built.listing = a.Listing();
				built.image = a.TakeImage(256, 0x00);
				built.layout = layout;
				return built;
			}
		}

		// ---------------------------------------------------------------------------------------
		// The images
		// ---------------------------------------------------------------------------------------

		const std::vector<u8>& DmgImage()
		{
			static const std::vector<u8> image = Build().image;
			return image;
		}

		const std::vector<u8>& CgbImage()
		{
			// The CGB image is the DMG one: the register state at 0x0100 is set by GbSystem,
			// which knows the console kind and the cartridge, and the animation is the same.
			return DmgImage();
		}

		const std::string& Listing()
		{
			static const std::string listing = Build().listing;
			return listing;
		}

		Layout ImageLayout()
		{
			const Built& built = Build();
			Layout layout;
			layout.codeEnd = built.layout.codeEnd;
			layout.tileData = built.layout.tileData;
			layout.wordmark = built.layout.wordmark;
			layout.imageEnd = built.layout.imageEnd;
			return layout;
		}

		int SlideFrames()
		{
			// The slide stores SCX = 112, 116, ... 240, so it is (240 - 112) / 4 + 1 frames.
			return (SlideEndScx - SlideStartScx) / SlideStep + 1;
		}

		int AnimationFrames() { return SlideFrames() + HoldFrames; }

		int LetterCount() { return LetterTileCount; }

		const char* TileLetters() { return TileLetterNames; }

		u8 LetterPixel(int letter, int x, int y)
		{
			if (letter < 0 || letter >= LetterTileCount || x < 0 || x > 7 || y < 0 || y > 7)
				return 0;
			if (y >= LetterRows)
				return 0;			// the bottom two rows of every tile are blank
			return (u8)((Letters[letter][y] >> (7 - x)) & 1);
		}
	}
}
