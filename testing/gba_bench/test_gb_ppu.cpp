// The Game Boy LCD controller: the background, the window, the sprites, the DMG palettes, the CGB
// colour palettes and the STAT/LY timing.
//
// The tests drive the real GbPpu (and, where the register path matters, a GbBus with no
// cartridge), build every expected picture from the specification - the tile format and the
// attribute bits of the Pan Docs "Tile Data" / "Tile Maps" / "OAM" / "Palettes" pages - and never
// from the emulator's own output. The frames are composed by the PPU's own scanline machine, so a
// test advances the LCD by whole lines with the helper at the top.

#include "gba_test.h"
#include "gb_ppu.h"
#include "gb_bus.h"

#include <string>

using namespace GBA;

namespace
{
	/// <summary>The four shades of the green DMG palette, as the PPU draws them.</summary>
	const uint32_t Shade0 = 0xFF9BBC0F;
	const uint32_t Shade1 = 0xFF8BAC0F;
	const uint32_t Shade2 = 0xFF306230;
	const uint32_t Shade3 = 0xFF0F380F;

	/// <summary>Turn one tile row into the two VRAM bitplane bytes (Pan Docs "Tile Data": the
	/// first byte is the low plane, bit 7 is the leftmost pixel).</summary>
	void TileRow(uint8_t* out, const char* pixels)
	{
		uint8_t low = 0, high = 0;
		for (int i = 0; i < 8; i++)
		{
			int index = pixels[i] - '0';
			if (index & 1)
				low |= (uint8_t)(0x80 >> i);
			if (index & 2)
				high |= (uint8_t)(0x80 >> i);
		}
		out[0] = low;
		out[1] = high;
	}

	/// <summary>Write a whole tile (eight rows of eight digits) into VRAM.</summary>
	void WriteTile(uint8_t* vram, int tileIndex, const char* const rows[8])
	{
		for (int row = 0; row < 8; row++)
			TileRow(vram + tileIndex * 16 + row * 2, rows[row]);
	}

	/// <summary>Run the LCD until the frame counter has advanced `frames` times.</summary>
	void RunFrames(GbPpu& ppu, int frames, bool cgb = false)
	{
		int start = ppu.FrameCounter();
		// A frame is 70224 dots and a dot is one clock, so a frame is 70224 ticks of this call:
		// give it six frames' worth as the budget and stop as soon as the counter moves.
		for (int i = 0; i < (int)(GbDotsPerFrame * 6) && ppu.FrameCounter() - start < frames; i++)
			ppu.Tick(1, cgb);
	}

	/// <summary>The pixel the frame buffer holds at (x, y).</summary>
	uint32_t Pixel(const GbPpu& ppu, int x, int y)
	{
		return ppu.Frame()[y * GbScreenWidth + x];
	}

	/// <summary>Prepare a PPU with the LCD on, the background on and the given palette.</summary>
	void SetupBackground(GbPpu& ppu, GbPalette palette = GbPalette::Green)
	{
		ppu.Reset();
		ppu.SetPalette(palette);
		ppu.WriteRegister(0xFF40, 0x91);		// LCD on, BG on, 0x8000 tile data, 0x9800 map
		ppu.WriteRegister(0xFF47, 0xE4);		// BGP: 0->0, 1->1, 2->2, 3->3 (the identity)
		ppu.WriteRegister(0xFF42, 0x00);		// SCY
		ppu.WriteRegister(0xFF43, 0x00);		// SCX
	}
}

// ---------------------------------------------------------------------------------------
// The background
// ---------------------------------------------------------------------------------------

GBA_TEST(GbPpu, background_tile_and_palette)
{
	GbPpu ppu;
	SetupBackground(ppu);

	// Tile 1 is a single dark pixel in its top-left corner; colour index 3 elsewhere is left at
	// zero so the tile is mostly the lightest shade.
	const char* const tile[8] =
	{
		"30000000", "00000000", "00000000", "00000000",
		"00000000", "00000000", "00000000", "00000000",
	};
	WriteTile(ppu.VramBank(0), 1, tile);

	// The map entry at (0, 0) selects tile 1 (the 0x9800 map with LCDC bit 3 clear).
	ppu.VramBank(0)[0x1800] = 1;

	RunFrames(ppu, 2);

	// The pixel at (0, 0) is colour index 3, which BGP maps to shade 3.
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade3);
	GBA_CHECK_HEX32(Pixel(ppu, 1, 0), Shade0);
	GBA_CHECK_HEX32(Pixel(ppu, 8, 0), Shade0);		// tile 0 everywhere else
}

GBA_TEST(GbPpu, background_scroll_wraps_the_map)
{
	GbPpu ppu;
	SetupBackground(ppu);

	// Tile 1 has its dark pixel one column in from its left edge, tile 2 the same: with SCX = 1
	// the map shifts one pixel left, so map column 1 lands on screen column 0.
	const char* const tile1[8] =
	{
		"03000000", "00000000", "00000000", "00000000",
		"00000000", "00000000", "00000000", "00000000",
	};
	const char* const tile2[8] =
	{
		"03000000", "00000000", "00000000", "00000000",
		"00000000", "00000000", "00000000", "00000000",
	};
	WriteTile(ppu.VramBank(0), 1, tile1);
	WriteTile(ppu.VramBank(0), 2, tile2);

	ppu.VramBank(0)[0x1800] = 1;			// map entry 0: tile 1 (map X 0..7)
	ppu.VramBank(0)[0x1801] = 2;			// map entry 1: tile 2 (map X 8..15)

	ppu.WriteRegister(0xFF43, 0x01);
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade3);		// map X 1
	GBA_CHECK_HEX32(Pixel(ppu, 8, 0), Shade3);		// map X 9
	GBA_CHECK_HEX32(Pixel(ppu, 9, 0), Shade0);		// map X 10

	// SCX = 255 shifts the map by almost a whole tile: the pixel at map X 255 (the last column of
	// the 32nd entry, which is tile 0) appears at screen X 0, and tile 1's dark pixel (map X 1)
	// appears at screen X 2.
	ppu.WriteRegister(0xFF43, 0xFF);
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 2, 0), Shade3);
}

GBA_TEST(GbPpu, tile_data_selection_0x8000_and_0x8800)
{
	GbPpu ppu;
	SetupBackground(ppu);

	// A dark tile at 0x8000 (the 0x8000 method's tile 0), a mid tile at 0x9000 (the 0x8800
	// method's tile 0) and another dark one at 0x97F0 (the 0x8800 method's tile 127).
	const char* const dark[8] =
	{
		"30000000", "00000000", "00000000", "00000000",
		"00000000", "00000000", "00000000", "00000000",
	};
	const char* const mid[8] =
	{
		"20000000", "00000000", "00000000", "00000000",
		"00000000", "00000000", "00000000", "00000000",
	};
	WriteTile(ppu.VramBank(0), 0x000, dark);
	WriteTile(ppu.VramBank(0), 0x100, mid);			// 0x1000 -> 0x9000
	WriteTile(ppu.VramBank(0), 0x17F, dark);		// 0x17F0 -> 0x97F0

	// The 0x8000 method (LCDC bit 4 set): the map entry is an unsigned tile index.
	ppu.WriteRegister(0xFF40, 0x91);
	ppu.VramBank(0)[0x1800] = 0x00;
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade3);		// 0x8000 + 0

	// The 0x8800 method (bit 4 clear): the index is signed and relative to 0x9000.
	ppu.WriteRegister(0xFF40, 0x81);
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade2);		// 0x9000 + 0 = the mid tile

	ppu.VramBank(0)[0x1800] = 0x7F;					// +127 -> 0x97F0
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade3);

	ppu.VramBank(0)[0x1800] = 0x80;					// -128 -> 0x8800, which is blank here
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade0);
}

GBA_TEST(GbPpu, bgp_selects_the_shade)
{
	GbPpu ppu;
	SetupBackground(ppu);

	const char* const tile[8] =
	{
		"30000000", "00000000", "00000000", "00000000",
		"00000000", "00000000", "00000000", "00000000",
	};
	WriteTile(ppu.VramBank(0), 1, tile);
	ppu.VramBank(0)[0x1800] = 1;

	// BGP = 0b11100100 puts index 3 at shade 0 and index 0 at shade 2 (Pan Docs "Palettes").
	ppu.WriteRegister(0xFF47, 0xE4);
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade3);
	GBA_CHECK_HEX32(Pixel(ppu, 1, 0), Shade0);

	// BGP = 0b00011011 inverts the palette.
	ppu.WriteRegister(0xFF47, 0x1B);
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade0);
	GBA_CHECK_HEX32(Pixel(ppu, 1, 0), Shade3);
}

GBA_TEST(GbPpu, monochrome_lcdc0_blanks_background_and_window)
{
	GbPpu ppu;
	SetupBackground(ppu);

	const char* const tile[8] =
	{
		"33333333", "33333333", "33333333", "33333333",
		"33333333", "33333333", "33333333", "33333333",
	};
	WriteTile(ppu.VramBank(0), 1, tile);
	ppu.VramBank(0)[0x1800] = 1;
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade3);

	// Clearing LCDC bit 0 blanks the background and the window to colour 0 (Pan Docs "LCDC").
	ppu.WriteRegister(0xFF40, 0x90);
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade0);
}

// ---------------------------------------------------------------------------------------
// The window
// ---------------------------------------------------------------------------------------

GBA_TEST(GbPpu, window_starts_at_wx_minus_seven)
{
	GbPpu ppu;
	SetupBackground(ppu);

	// The background is light; the window's tile is dark, and the window uses its own map.
	const char* const dark[8] =
	{
		"33333333", "33333333", "33333333", "33333333",
		"33333333", "33333333", "33333333", "33333333",
	};
	WriteTile(ppu.VramBank(0), 1, dark);
	ppu.VramBank(0)[0x1C00] = 1;			// the 0x9C00 window map (LCDC bit 6)

	ppu.WriteRegister(0xFF4A, 0x00);		// WY = 0
	ppu.WriteRegister(0xFF4B, 0x07);		// WX = 7 -> the window starts at screen X 0
	ppu.WriteRegister(0xFF40, 0xF1);		// LCD on, window on (bit 5), 0x9C00 window map (bit 6)
	RunFrames(ppu, 2);

	// The window map's first entry is the only dark tile, so screen X 0..7 are dark and the rest
	// of the row is the blank tile the window map holds.
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade3);
	GBA_CHECK_HEX32(Pixel(ppu, 7, 0), Shade3);
	GBA_CHECK_HEX32(Pixel(ppu, 8, 0), Shade0);

	// WX = 15 puts the window's left edge at screen X 8: the first eight pixels stay background.
	ppu.WriteRegister(0xFF4B, 0x0F);
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 7, 0), Shade0);
	GBA_CHECK_HEX32(Pixel(ppu, 8, 0), Shade3);
}

GBA_TEST(GbPpu, window_uses_its_own_line_counter_not_scy)
{
	GbPpu ppu;
	SetupBackground(ppu);

	// Two window tiles stacked in the window map: the first row dark, the row below it a
	// different shade. The window's counter ignores SCY, so scrolling the background does not
	// move the window.
	const char* const dark[8] =
	{
		"33333333", "33333333", "33333333", "33333333",
		"33333333", "33333333", "33333333", "33333333",
	};
	const char* const mid[8] =
	{
		"22222222", "22222222", "22222222", "22222222",
		"22222222", "22222222", "22222222", "22222222",
	};
	WriteTile(ppu.VramBank(0), 1, dark);
	WriteTile(ppu.VramBank(0), 2, mid);
	ppu.VramBank(0)[0x1C00] = 1;			// window map row 0
	ppu.VramBank(0)[0x1C20] = 2;			// window map row 1

	ppu.WriteRegister(0xFF42, 0x05);		// SCY = 5 (the window must ignore it)
	ppu.WriteRegister(0xFF4A, 0x00);		// WY = 0
	ppu.WriteRegister(0xFF4B, 0x07);		// WX = 7
	ppu.WriteRegister(0xFF40, 0xF1);
	RunFrames(ppu, 2);

	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade3);		// window row 0
	GBA_CHECK_HEX32(Pixel(ppu, 0, 8), Shade2);		// window row 1
	GBA_CHECK_HEX32(Pixel(ppu, 0, 16), Shade0);		// past the window's map: blank tiles

	// The window's line counter counts the lines it has rendered in this frame and is cleared at
	// the end of the frame, so at a frame boundary it is back in range (0..144).
	GBA_CHECK_MSG(ppu.WindowLine() >= 0 && ppu.WindowLine() <= GbVisibleLines,
		"the window line counter stays inside a frame");
}

GBA_TEST(GbPpu, window_needs_wy_to_match_ly)
{
	GbPpu ppu;
	SetupBackground(ppu);

	const char* const dark[8] =
	{
		"33333333", "33333333", "33333333", "33333333",
		"33333333", "33333333", "33333333", "33333333",
	};
	WriteTile(ppu.VramBank(0), 1, dark);
	ppu.VramBank(0)[0x1C00] = 1;

	ppu.WriteRegister(0xFF4A, 0x10);		// WY = 16
	ppu.WriteRegister(0xFF4B, 0x07);		// WX = 7
	ppu.WriteRegister(0xFF40, 0xF1);
	RunFrames(ppu, 2);

	// The window starts on the line whose LY is WY (Pan Docs "Window": the Y condition).
	GBA_CHECK_HEX32(Pixel(ppu, 0, 15), Shade0);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 16), Shade3);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 24), Shade0);		// past the window's first map row
	GBA_CHECK_HEX32(Pixel(ppu, 0, 100), Shade0);

	// A WX of 0..6 leaves the window off the left edge, so nothing of it is visible.
	ppu.WriteRegister(0xFF4B, 0x00);
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 100), Shade0);
}

// ---------------------------------------------------------------------------------------
// Sprites
// ---------------------------------------------------------------------------------------

GBA_TEST(GbPpu, sprites_draw_and_honour_the_coordinates)
{
	GbPpu ppu;
	SetupBackground(ppu);

	// Tile 4 is a dark block; the object uses it with OBP0 = 0xE4 (index 3 -> shade 3).
	const char* const dark[8] =
	{
		"33333333", "33333333", "33333333", "33333333",
		"33333333", "33333333", "33333333", "33333333",
	};
	WriteTile(ppu.VramBank(0), 4, dark);
	ppu.WriteRegister(0xFF48, 0xE4);		// OBP0

	// An object at OAM (Y = 16 + 8, X = 8 + 10) appears at screen (10, 8) (Pan Docs "OAM").
	uint8_t* oam = ppu.Oam();
	oam[0] = 24;			// Y: the screen Y is the OAM Y minus 16
	oam[1] = 18;			// X: the screen X is the OAM X minus 8
	oam[2] = 4;				// the tile
	oam[3] = 0x00;			// no flip, OBP0
	ppu.WriteRegister(0xFF40, 0x93);		// LCD on, BG on, OBJ on

	RunFrames(ppu, 2);

	GBA_CHECK_HEX32(Pixel(ppu, 10, 8), Shade3);
	GBA_CHECK_HEX32(Pixel(ppu, 9, 8), Shade0);
	GBA_CHECK_HEX32(Pixel(ppu, 18, 8), Shade0);
	GBA_CHECK_HEX32(Pixel(ppu, 10, 7), Shade0);

	// Clearing LCDC bit 1 hides every object (Pan Docs "LCDC").
	ppu.WriteRegister(0xFF40, 0x91);
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 10, 8), Shade0);
}

GBA_TEST(GbPpu, sprite_flips_and_the_8x16_size)
{
	GbPpu ppu;
	SetupBackground(ppu);

	// Tile 4 has a dark pixel at its top-left; tile 5 has one at its bottom-right.
	const char* const tl[8] =
	{
		"30000000", "00000000", "00000000", "00000000",
		"00000000", "00000000", "00000000", "00000000",
	};
	const char* const br[8] =
	{
		"00000000", "00000000", "00000000", "00000000",
		"00000000", "00000000", "00000000", "00000003",
	};
	WriteTile(ppu.VramBank(0), 4, tl);
	WriteTile(ppu.VramBank(0), 5, br);
	ppu.WriteRegister(0xFF48, 0xE4);

	uint8_t* oam = ppu.Oam();
	oam[0] = 16;
	oam[1] = 8;
	oam[2] = 4;
	oam[3] = 0x00;
	ppu.WriteRegister(0xFF40, 0x93);		// 8x8 objects
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade3);
	GBA_CHECK_HEX32(Pixel(ppu, 7, 7), Shade0);

	// The X flip mirrors the tile (attribute bit 5).
	oam[3] = 0x20;
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade0);
	GBA_CHECK_HEX32(Pixel(ppu, 7, 0), Shade3);

	// The Y flip mirrors it vertically (attribute bit 6).
	oam[3] = 0x40;
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade0);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 7), Shade3);

	// 8x16 objects use the tile pair NN and NN|1, with the low bit of NN ignored (Pan Docs "OAM").
	oam[3] = 0x00;
	oam[2] = 0x04;							// tiles 4 and 5
	ppu.WriteRegister(0xFF40, 0x97);		// LCDC bit 2 set: 8x16
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade3);		// the top tile's dark pixel
	GBA_CHECK_HEX32(Pixel(ppu, 7, 15), Shade3);		// the bottom tile's dark pixel
}

GBA_TEST(GbPpu, only_ten_sprites_per_line_and_the_x_priority_rule)
{
	GbPpu ppu;
	SetupBackground(ppu);

	const char* const dark[8] =
	{
		"33333333", "33333333", "33333333", "33333333",
		"33333333", "33333333", "33333333", "33333333",
	};
	WriteTile(ppu.VramBank(0), 4, dark);
	ppu.WriteRegister(0xFF48, 0xE4);

	// Eleven objects on the same line: the OAM scan keeps the first ten (Pan Docs "OAM"), so
	// object 10 (at X 100) never appears.
	uint8_t* oam = ppu.Oam();
	for (int i = 0; i < 11; i++)
	{
		oam[i * 4 + 0] = 16;
		oam[i * 4 + 1] = (uint8_t)(8 + i * 8);
		oam[i * 4 + 2] = 4;
		oam[i * 4 + 3] = 0x00;
	}
	ppu.WriteRegister(0xFF40, 0x93);
	RunFrames(ppu, 2);

	GBA_CHECK_EQ(ppu.LineSpriteCount(), 10);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade3);			// object 0
	GBA_CHECK_HEX32(Pixel(ppu, 72, 0), Shade3);			// object 9
	GBA_CHECK_HEX32(Pixel(ppu, 80, 0), Shade0);			// object 10 was dropped
}

GBA_TEST(GbPpu, dmg_sprite_priority_is_by_x_coordinate)
{
	GbPpu ppu;
	SetupBackground(ppu);

	// Two object tiles: tile 4 is dark (index 3), tile 6 is index 2.
	const char* const dark[8] =
	{
		"33333333", "33333333", "33333333", "33333333",
		"33333333", "33333333", "33333333", "33333333",
	};
	const char* const mid[8] =
	{
		"22222222", "22222222", "22222222", "22222222",
		"22222222", "22222222", "22222222", "22222222",
	};
	WriteTile(ppu.VramBank(0), 4, dark);
	WriteTile(ppu.VramBank(0), 6, mid);
	ppu.WriteRegister(0xFF48, 0xE4);		// index 3 -> shade 3
	ppu.WriteRegister(0xFF49, 0xE4);		// index 2 -> shade 2

	// The later object in OAM has the *smaller* X, and on a monochrome console the smaller X wins
	// (Pan Docs "OAM": "the smaller the X coordinate, the higher the priority").
	uint8_t* oam = ppu.Oam();
	oam[0] = 16; oam[1] = 8 + 30; oam[2] = 6; oam[3] = 0x00;		// at screen X 30, palette OBP0
	oam[4] = 16; oam[5] = 8 + 28; oam[6] = 4; oam[7] = 0x00;		// at screen X 28, tile 4
	ppu.WriteRegister(0xFF40, 0x93);
	RunFrames(ppu, 2);

	// Over the overlap (screen X 28..35) the object at X 28 wins.
	GBA_CHECK_HEX32(Pixel(ppu, 28, 0), Shade3);
	GBA_CHECK_HEX32(Pixel(ppu, 35, 0), Shade3);
	GBA_CHECK_HEX32(Pixel(ppu, 38, 0), Shade0);
}

GBA_TEST(GbPpu, obj_behind_bg_and_colour_zero_transparency)
{
	GbPpu ppu;
	SetupBackground(ppu);

	// The background is fully dark (index 3) in tile 1; the object is a lighter block.
	const char* const bgDark[8] =
	{
		"33333333", "33333333", "33333333", "33333333",
		"33333333", "33333333", "33333333", "33333333",
	};
	const char* const objMid[8] =
	{
		"22222222", "22222222", "22222222", "22222222",
		"22222222", "22222222", "22222222", "22222222",
	};
	WriteTile(ppu.VramBank(0), 1, bgDark);
	WriteTile(ppu.VramBank(0), 4, objMid);
	ppu.VramBank(0)[0x1800] = 1;
	ppu.WriteRegister(0xFF48, 0xE4);

	uint8_t* oam = ppu.Oam();
	oam[0] = 16; oam[1] = 8; oam[2] = 4; oam[3] = 0x00;

	// Attribute bit 7 set: the background's colour indices 1..3 are drawn over the object.
	oam[3] = 0x80;
	ppu.WriteRegister(0xFF40, 0x93);
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade3);		// the background wins

	// With the flag clear the object is visible over the background.
	oam[3] = 0x00;
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade2);		// the object's index 2

	// A background colour index of 0 always lets the object through, even with the flag set.
	WriteTile(ppu.VramBank(0), 1, objMid);			// now index 2 background, not 3
	ppu.WriteRegister(0xFF47, 0xE4);
	oam[3] = 0x80;
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade2);		// the background's index 2 wins here
}

// ---------------------------------------------------------------------------------------
// The CGB
// ---------------------------------------------------------------------------------------

GBA_TEST(GbPpu, cgb_sprites_use_the_object_palette_and_the_tile_bank)
{
	GbPpu ppu;
	ppu.Reset();
	ppu.SetCgb(true);
	ppu.WriteRegister(0xFF40, 0x93);		// LCD on, BG on, OBJ on
	ppu.WriteRegister(0xFF42, 0x00);		// SCY
	ppu.WriteRegister(0xFF43, 0x00);		// SCX

	// The two object palettes the test uses. A colour is two bytes (Pan Docs "Palettes"), and
	// OCPS's bit 7 makes the address advance by itself: palette 0 = black/red/green/blue and
	// palette 1 = black/yellow/cyan/magenta.
	const uint8_t palette0[8] = { 0x00, 0x00, 0x1F, 0x00, 0xE0, 0x03, 0x00, 0x7C };
	const uint8_t palette1[8] = { 0x00, 0x00, 0xFF, 0x03, 0xE0, 0x7F, 0x1F, 0x7C };
	ppu.WriteRegister(0xFF6A, 0x80);		// OCPS: palette 0, colour 0, auto-increment
	for (int i = 0; i < 8; i++)
		ppu.WriteRegister(0xFF6B, palette0[i]);
	ppu.WriteRegister(0xFF6A, 0x88);		// OCPS: palette 1, colour 0
	for (int i = 0; i < 8; i++)
		ppu.WriteRegister(0xFF6B, palette1[i]);
	GBA_CHECK_EQ(ppu.ReadRegister(0xFF6A), 0x90);	// the address advanced to byte 16

	// The background palette 0, so that the dots the object does not cover are a known white
	// instead of whatever the reset left in the palette memory.
	ppu.WriteRegister(0xFF68, 0x80);		// BCPS: palette 0, colour 0, auto-increment
	ppu.WriteRegister(0xFF69, 0xFF);		// 0: white (0x7FFF)
	ppu.WriteRegister(0xFF69, 0x7F);
	for (int i = 0; i < 6; i++)
		ppu.WriteRegister(0xFF69, 0x00);	// 1..3: black

	// The object's tile lives in VRAM bank 1 (OAM attribute bit 3) and is solid colour index 2.
	const char* const solid2[8] =
	{
		"22222222", "22222222", "22222222", "22222222",
		"22222222", "22222222", "22222222", "22222222",
	};
	WriteTile(ppu.VramBank(1), 4, solid2);

	// OAM entry 0: at screen (10, 8) (the coordinate bias is 16/8), tile 4, bank 1, palette 0.
	uint8_t* oam = ppu.Oam();
	oam[0] = 24;
	oam[1] = 18;
	oam[2] = 4;
	oam[3] = 0x08;
	RunFrames(ppu, 2, true);
	GBA_CHECK_HEX32(Pixel(ppu, 10, 8), 0xFF00FF00);		// palette 0, index 2: green
	GBA_CHECK_HEX32(Pixel(ppu, 9, 8), 0xFFFFFFFF);		// the white background

	// The palette field of the attribute (bits 0-2) picks the other object palette: the same
	// index 2 is now cyan.
	oam[3] = (uint8_t)(0x08 | 0x01);
	RunFrames(ppu, 2, true);
	GBA_CHECK_HEX32(Pixel(ppu, 10, 8), 0xFF00FFFF);

	// Clearing the OAM attribute's bank bit makes the PPU fetch from bank 0, where tile 4 is
	// blank: the object disappears.
	oam[3] = 0x01;
	RunFrames(ppu, 2, true);
	GBA_CHECK_HEX32(Pixel(ppu, 10, 8), 0xFFFFFFFF);
}

GBA_TEST(GbBus, cgb_palette_registers_are_reachable_through_the_bus)
{
	// A CGB game defines every colour it draws through BCPS/BCPD (0xFF68/0xFF69) and OCPS/OCPD
	// (0xFF6A/0xFF6B), so the bus has to pass those four registers to the PPU. This drives the
	// whole register path rather than the PPU's own WriteRegister.
	GbBus bus;
	bus.cpu.bus = &bus;
	bus.Reset();
	// A bare GbBus does not reset its devices (the machine does that, see GbSystem::Reset): the PPU
	// has to be reset before it can draw, because that is what sizes its frame buffer.
	bus.ppu.Reset();
	bus.ppu.SetCgb(true);
	bus.SetCgb(true);

	bus.WriteByte(0xFF40, 0x93);			// LCD on, BG on, OBJ on
	bus.WriteByte(0xFF42, 0x00);
	bus.WriteByte(0xFF43, 0x00);

	// BG palette 0: black/red/green/blue (the boot state is a grey ramp).
	const uint8_t colors[8] = { 0x00, 0x00, 0x1F, 0x00, 0xE0, 0x03, 0x00, 0x7C };
	bus.WriteByte(0xFF68, 0x80);			// BGPI: palette 0, colour 0, auto-increment
	for (int i = 0; i < 8; i++)
		bus.WriteByte(0xFF69, colors[i]);

	// Tile 1 is colour index 1 (red) everywhere, and the map's entry 0 selects it.
	char tile[8][9];
	for (int row = 0; row < 8; row++)
	{
		for (int column = 0; column < 8; column++)
			tile[row][column] = '1';
		tile[row][8] = '\0';
	}
	const char* rows[8];
	for (int row = 0; row < 8; row++)
		rows[row] = tile[row];
	WriteTile(bus.ppu.VramBank(0), 1, rows);
	bus.WriteByte(0x9800, 1);				// the map entry of the top-left tile

	// The attribute map (VRAM bank 1) says "palette 0" for that entry, which is the reset value.
	bus.ppu.VramBank(1)[0x1800] = 0x00;

	for (int frame = 0; frame < 2; frame++)
		for (int i = 0; i < 20000; i++)
			bus.ppu.Tick(1, false);

	// The picture is red - which it cannot be if the bus drops the palette writes (the grey ramp
	// the machine installs at reset is what the screen would keep showing).
	GBA_CHECK_HEX32(bus.ppu.Frame()[0], 0xFFFF0000);
}

GBA_TEST(GbPpu, cgb_palettes_and_the_attribute_map)
{
	GbPpu ppu;
	ppu.Reset();
	ppu.SetCgb(true);
	ppu.WriteRegister(0xFF40, 0x91);
	ppu.WriteRegister(0xFF42, 0x00);
	ppu.WriteRegister(0xFF43, 0x00);

	// BG palette 1 with four distinct colours: 0 = black, 1 = red (0x001F), 2 = green (0x03E0),
	// 3 = blue (0x7C00). A colour is two bytes and the address only advances by itself while
	// BGPI's bit 7 is set (Pan Docs "Palettes"), so the auto-increment form is used here.
	ppu.WriteRegister(0xFF68, 0x88);		// palette 1, colour 0, auto-increment on
	ppu.WriteRegister(0xFF69, 0x00);		// 0: black
	ppu.WriteRegister(0xFF69, 0x00);
	ppu.WriteRegister(0xFF69, 0x1F);		// 1: red
	ppu.WriteRegister(0xFF69, 0x00);
	ppu.WriteRegister(0xFF69, 0xE0);		// 2: green
	ppu.WriteRegister(0xFF69, 0x03);
	ppu.WriteRegister(0xFF69, 0x00);		// 3: blue
	ppu.WriteRegister(0xFF69, 0x7C);
	GBA_CHECK_EQ(ppu.ReadRegister(0xFF68), 0x90);	// the address advanced to byte 16

	// Tile 1 is colour index 1 everywhere; the attribute map (VRAM bank 1) selects palette 1.
	const char* const solid[8] =
	{
		"11111111", "11111111", "11111111", "11111111",
		"11111111", "11111111", "11111111", "11111111",
	};
	WriteTile(ppu.VramBank(0), 1, solid);
	ppu.VramBank(0)[0x1800] = 1;
	ppu.VramBank(1)[0x1800] = 0x01;			// attribute: palette 1, no flips, bank 0

	RunFrames(ppu, 2, true);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), 0xFFFF0000);		// red, expanded from RGB555

	// The attribute's Y flip mirrors the tile: index 2 only in the tile's bottom row arrives at
	// the top of the sprite area.
	const char* const bottom[8] =
	{
		"00000000", "00000000", "00000000", "00000000",
		"00000000", "00000000", "00000000", "22222222",
	};
	WriteTile(ppu.VramBank(0), 1, bottom);
	ppu.VramBank(1)[0x1800] = 0x41;			// palette 1, Y flip
	RunFrames(ppu, 2, true);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), 0xFF00FF00);		// the flipped row: green
	GBA_CHECK_HEX32(Pixel(ppu, 0, 7), 0xFF000000);		// the unflipped row is index 0: black

	// The X flip mirrors the columns: index 3 in the tile's left column arrives at the right.
	const char* const leftColumn[8] =
	{
		"30000000", "30000000", "30000000", "30000000",
		"30000000", "30000000", "30000000", "30000000",
	};
	WriteTile(ppu.VramBank(0), 1, leftColumn);
	ppu.VramBank(1)[0x1800] = 0x21;			// palette 1, X flip
	RunFrames(ppu, 2, true);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), 0xFF000000);		// the leftmost pixel is now blank
	GBA_CHECK_HEX32(Pixel(ppu, 7, 0), 0xFF0000FF);		// blue at the right edge
}

GBA_TEST(GbPpu, cgb_tile_vram_bank_attribute)
{
	GbPpu ppu;
	ppu.Reset();
	ppu.SetCgb(true);
	ppu.WriteRegister(0xFF40, 0x91);

	// Bank 0's tile 1 is red, bank 1's is blue; the attribute's bit 3 picks the bank.
	const char* const solid[8] =
	{
		"11111111", "11111111", "11111111", "11111111",
		"11111111", "11111111", "11111111", "11111111",
	};
	WriteTile(ppu.VramBank(0), 1, solid);
	WriteTile(ppu.VramBank(1), 1, solid);

	// BG palette 0 colour 1 = blue (0x7C00).
	ppu.WriteRegister(0xFF68, 0x80);
	ppu.WriteRegister(0xFF69, 0x00);		// colour 0
	ppu.WriteRegister(0xFF69, 0x00);
	ppu.WriteRegister(0xFF69, 0x00);		// colour 1 low
	ppu.WriteRegister(0xFF69, 0x7C);		// colour 1 high: 0x7C00

	ppu.VramBank(0)[0x1800] = 1;			// the map entry (bank 0)
	ppu.VramBank(1)[0x1800] = 0x08;			// attribute: tile bank 1, palette 0

	RunFrames(ppu, 2, true);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), 0xFF0000FF);		// blue
}

GBA_TEST(GbPpu, cgb_priority_between_bg_and_objects)
{
	GbPpu ppu;
	ppu.Reset();
	ppu.SetCgb(true);
	ppu.WriteRegister(0xFF40, 0x93);		// LCD on, BG on, OBJ on (LCDC.0 set)
	ppu.WriteRegister(0xFF47, 0xE4);

	// The background is colour index 1 and the object is index 1: with both attribute priority
	// bits clear the object wins, and with either set the background wins (Pan Docs "Tile Maps",
	// the CGB BG-to-OBJ priority table).
	const char* const solid1[8] =
	{
		"11111111", "11111111", "11111111", "11111111",
		"11111111", "11111111", "11111111", "11111111",
	};
	WriteTile(ppu.VramBank(0), 1, solid1);
	WriteTile(ppu.VramBank(0), 4, solid1);
	ppu.VramBank(0)[0x1800] = 1;
	ppu.VramBank(1)[0x1800] = 0x00;			// the BG attribute's priority bit clear

	// BG palette 0 colour 1 = green, OBJ palette 0 colour 1 = red.
	ppu.WriteRegister(0xFF68, 0x80);
	ppu.WriteRegister(0xFF69, 0x00);
	ppu.WriteRegister(0xFF69, 0x00);
	ppu.WriteRegister(0xFF69, 0xE0);
	ppu.WriteRegister(0xFF69, 0x03);		// 0x03E0: green
	ppu.WriteRegister(0xFF6A, 0x80);
	ppu.WriteRegister(0xFF6B, 0x00);
	ppu.WriteRegister(0xFF6B, 0x00);
	ppu.WriteRegister(0xFF6B, 0x1F);
	ppu.WriteRegister(0xFF6B, 0x00);		// 0x001F: red

	uint8_t* oam = ppu.Oam();
	oam[0] = 16; oam[1] = 8; oam[2] = 4; oam[3] = 0x00;

	RunFrames(ppu, 2, true);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), 0xFFFF0000);		// the object (red) wins

	// The OAM attribute's priority bit set (and the BG's clear) still lets the object win in the
	// CGB table because the BG index is not zero... with LCDC.0 set and *one* flag set the BG
	// wins.
	oam[3] = 0x80;
	RunFrames(ppu, 2, true);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), 0xFF00FF00);		// the background (green) wins

	// LCDC bit 0 clear: on a CGB that is the master priority, so the object always wins.
	ppu.WriteRegister(0xFF40, 0x92);
	RunFrames(ppu, 2, true);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), 0xFFFF0000);
}

// ---------------------------------------------------------------------------------------
// Timing, STAT and the LCD enable
// ---------------------------------------------------------------------------------------

GBA_TEST(GbPpu, mode_timing_is_the_documented_one)
{
	GbPpu ppu;
	ppu.Reset();

	// The LCD starts in mode 2 with the OAM scan (Pan Docs "Rendering": 80 dots).
	GBA_CHECK_EQ(ppu.Mode(), 2);
	GBA_CHECK_EQ(ppu.Ly(), 0);

	// 80 dots later the PPU is in mode 3.
	ppu.Tick(80, false);
	GBA_CHECK_EQ(ppu.Mode(), 3);

	// Mode 3 lasts 172..289 dots; a plain line with no objects and no window is the minimum.
	ppu.Tick(172, false);
	GBA_CHECK_EQ(ppu.Mode(), 0);
	GBA_CHECK_MSG(ppu.LastMode3Length() >= 172 && ppu.LastMode3Length() <= 289,
		"mode 3 must be between 172 and 289 dots");

	// The rest of the line is mode 0, and then LY becomes 1.
	ppu.Tick(204, false);
	GBA_CHECK_EQ(ppu.Ly(), 1);
	GBA_CHECK_EQ(ppu.Mode(), 2);

	// Run to the end of the visible lines: LY 144 is VBlank (mode 1).
	int guard = 0;
	while (ppu.Ly() < 144 && guard++ < 200)
		ppu.Tick(GbDotsPerLine, false);
	GBA_CHECK_EQ(ppu.Ly(), 144);
	GBA_CHECK_EQ(ppu.Mode(), 1);

	// VBlank is ten lines; the frame counter moves when LY wraps back to 0.
	while (ppu.FrameCounter() == 0 && guard++ < 400)
		ppu.Tick(GbDotsPerLine, false);
	GBA_CHECK_EQ(ppu.FrameCounter(), 1);
	GBA_CHECK_EQ(ppu.Ly(), 0);
}

GBA_TEST(GbPpu, stat_interrupt_rides_the_shared_line)
{
	GbPpu ppu;
	ppu.Reset();

	// Select the mode 2 source (STAT bit 5): the STAT line is high while the PPU scans OAM, so
	// the interrupt is requested on the rising edge at the start of the line (Pan Docs
	// "Interrupt Sources").
	ppu.WriteRegister(0xFF41, 0x20);

	// The line is already high (mode 2 is running), so nothing more is requested until it drops
	// and rises again.
	uint8_t request = ppu.Tick(GbDotsPerLine, false);
	(void)request;

	// Enable both mode 0 and mode 1: the line never goes low between them, so mode 1 produces no
	// request - the documented "STAT blocking" (Pan Docs "Interrupt Sources").
	ppu.Reset();
	ppu.WriteRegister(0xFF41, 0x18);		// mode 0 and mode 1 selected
	int requests = 0;
	for (int i = 0; i < 160; i++)
	{
		if (ppu.Tick(GbDotsPerLine, false) & 0x02)
			requests++;
	}
	// Mode 0 rises once per line, but mode 1 holds the line high across the bottom of the frame,
	// so the VBlank edge is blocked.
	GBA_CHECK_MSG(requests <= 150, "STAT blocking must drop the mode 1 edges");
}

GBA_TEST(GbPpu, lyc_comparison_and_the_stat_flag)
{
	GbPpu ppu;
	ppu.Reset();

	ppu.WriteRegister(0xFF45, 0x00);		// LYC = 0
	GBA_CHECK_MSG((ppu.Stat() & 0x04) != 0, "LY = LYC must set STAT bit 2");

	ppu.WriteRegister(0xFF45, 0x10);		// LYC = 16
	GBA_CHECK_MSG((ppu.Stat() & 0x04) == 0, "the comparison must clear again");

	// Run to LY 16 and check the flag comes back (the comparison is "constantly" updated).
	int guard = 0;
	while (ppu.Ly() < 16 && guard++ < 100)
		ppu.Tick(GbDotsPerLine, false);
	GBA_CHECK_EQ(ppu.Ly(), 16);
	GBA_CHECK_MSG((ppu.Stat() & 0x04) != 0, "LYC = LY must be visible while the line runs");
}

GBA_TEST(GbPpu, lcd_off_blanks_and_unlocks_memory)
{
	GbPpu ppu;
	ppu.Reset();

	// With the LCD on, mode 3 blocks VRAM (Pan Docs "Accessing VRAM and OAM"); with it off
	// everything is accessible and the mode reads 0.
	ppu.Tick(80, false);
	GBA_CHECK_EQ(ppu.Mode(), 3);
	GBA_CHECK_MSG(ppu.VramBlocked(), "VRAM is blocked in mode 3");
	GBA_CHECK_MSG(ppu.OamBlocked(), "OAM is blocked in mode 2/3");

	ppu.WriteRegister(0xFF40, 0x00);		// LCD off
	GBA_CHECK_MSG(!ppu.LcdEnabled(), "LCDC bit 7 clear turns the LCD off");
	GBA_CHECK_EQ(ppu.Mode(), 0);
	GBA_CHECK_EQ(ppu.Ly(), 0);
	GBA_CHECK_MSG(!ppu.VramBlocked(), "VRAM is free while the LCD is off");
	GBA_CHECK_MSG(!ppu.OamBlocked(), "OAM is free while the LCD is off");

	// Off means blank: the last frame stays as it was, and turning it back on starts at LY 0.
	ppu.WriteRegister(0xFF40, 0x91);
	GBA_CHECK_MSG(ppu.LcdEnabled(), "the LCD comes back on");
	GBA_CHECK_EQ(ppu.Ly(), 0);
	GBA_CHECK_EQ(ppu.Mode(), 2);
}

GBA_TEST(GbPpu, frame_output_is_xrgb8888_and_uses_the_palette)
{
	GbPpu ppu;
	SetupBackground(ppu, GbPalette::Grey);

	const char* const dark[8] =
	{
		"33333333", "33333333", "33333333", "33333333",
		"33333333", "33333333", "33333333", "33333333",
	};
	WriteTile(ppu.VramBank(0), 1, dark);
	ppu.VramBank(0)[0x1800] = 1;
	RunFrames(ppu, 2);

	// The grey palette's index 3 is pure black and index 0 pure white, both fully opaque.
	uint32_t ink = Pixel(ppu, 0, 0);
	uint32_t background = Pixel(ppu, 8, 0);
	GBA_CHECK_HEX32(ink, 0xFF000000);
	GBA_CHECK_HEX32(background, 0xFFFFFFFF);

	// The default (green) palette is the DMG's, not the grey one.
	ppu.SetPalette(GbPalette::Green);
	RunFrames(ppu, 2);
	GBA_CHECK_HEX32(Pixel(ppu, 0, 0), Shade3);
}
