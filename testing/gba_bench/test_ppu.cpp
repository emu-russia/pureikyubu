// Unit tests for the GBA LCD controller (src/gba/gba_ppu.cpp).
//
// The tests drive a real GbaBus (so a real Ppu) and read the rendered line back through
// LinePixel()/Frame(). Every expected pixel is derived here from the GBATEK rules - either as a
// literal or with the small reference helpers below, which read the same memory the PPU reads but
// never call the renderer. The expected values are therefore independent of gba_ppu.cpp.
//
// GBATEK sections used: "LCD", "LCD VRAM BG Screen Data Format", "LCD VRAM Character Data",
// "LCD VRAM Bitmap BG", "LCD VRAM Overview", "LCD OBJ", "LCD Color Palettes", "LCD Dimensions
// and Timings", "LCD I/O Color Special Effects".

#include "gba_test.h"

// The GBA core headers are found through -Isrc/gba (see build.sh and test_io.cpp).
#include "gba_bus.h"

using namespace GBA;

namespace
{
	// ---------------------------------------------------------------------------------------
	// Register offsets (0x04000000 + offset), matching GBATEK's I/O map.
	// ---------------------------------------------------------------------------------------

	const u32 DISPCNT = 0x000;
	const u32 GREENSWP = 0x002;
	const u32 DISPSTAT = 0x004;
	const u32 BG0CNT = 0x008;
	const u32 BG1CNT = 0x00A;
	const u32 BG2CNT = 0x00C;
	const u32 BG0HOFS = 0x010;
	const u32 BG0VOFS = 0x012;
	const u32 BG2PA = 0x020;
	const u32 BG2X_L = 0x028;
	const u32 WIN0H = 0x040;
	const u32 WIN0V = 0x044;
	const u32 WININ = 0x048;
	const u32 WINOUT = 0x04A;
	const u32 BLDCNT = 0x050;
	const u32 BLDALPHA = 0x052;

	// DISPCNT bits.
	const u16 DC_MODE0 = 0x0000;
	const u16 DC_MODE1 = 0x0001;
	const u16 DC_MODE3 = 0x0003;
	const u16 DC_MODE4 = 0x0004;
	const u16 DC_MODE5 = 0x0005;
	const u16 DC_FRAME1 = 0x0010;
	const u16 DC_OBJ_1D = 0x0040;
	const u16 DC_FORCED_BLANK = 0x0080;
	const u16 DC_BG0 = 0x0100;
	const u16 DC_BG1 = 0x0200;
	const u16 DC_BG2 = 0x0400;
	const u16 DC_BG3 = 0x0800;
	const u16 DC_OBJ = 0x1000;
	const u16 DC_WIN0 = 0x2000;
	const u16 DC_WIN1 = 0x4000;
	const u16 DC_OBJ_WIN = 0x8000;

	// DISPSTAT bits.
	const u16 STAT_VBLANK = 0x0001;
	const u16 STAT_HBLANK = 0x0002;
	const u16 STAT_VCOUNT = 0x0004;
	const u16 STAT_VBLANK_IRQ = 0x0008;
	const u16 STAT_HBLANK_IRQ = 0x0010;
	const u16 STAT_VCOUNT_IRQ = 0x0020;

	// The VRAM windows the tests use.
	const u32 OBJ_TILES = 0x10000;			// the OBJ tile area of the tile modes
	const u32 OBJ_TILES_BITMAP = 0x14000;	// the OBJ tile area of the bitmap modes

	// ---------------------------------------------------------------------------------------
	// The tests drive the PPU directly (Tick is exercised by the timing tests only). In the
	// assembled emulator GbaBus::Tick advances the PPU; calling Ppu::Tick on a freshly reset bus
	// without running the CPU keeps the two from double counting.
	// ---------------------------------------------------------------------------------------

	/// <summary>Reset the bus and bring the display up in mode 0 with every layer off: only the
	/// backdrop would be visible. WININ/WINOUT are left at their reset value of zero: with no
	/// window enabled the window feature is off and every layer is displayed everywhere, so a
	/// test does not have to set them up (GBATEK 4000000h bits 13-15).</summary>
	void SetupDisplay(GbaBus& bus)
	{
		bus.Reset();
		bus.ppu.Write16(bus, DISPCNT, DC_MODE0, 0);
	}

	/// <summary>Build a BGxCNT value out of the fields the tests care about (GBATEK 4000008h):
	/// priority (0-3), the character base address in bytes (0..0xC000, a multiple of 16 KByte) and
	/// the screen base block (0-31, units of 2 KByte). The size and colour-depth bits are added by
	/// the tests that need them.</summary>
	u16 BG_CNT(int priority, u32 charBaseBytes, int screenBaseBlock)
	{
		return (u16)((priority & 3) | (((charBaseBytes / 0x4000) & 3) << 2) |
			((screenBaseBlock & 0x1F) << 8));
	}

	/// <summary>Write a 16-bit register.</summary>
	void WriteReg(GbaBus& bus, u32 offset, u16 value, int cycles = 0)
	{
		bus.ppu.Write16(bus, offset, value, cycles);
	}

	/// <summary>Write a 16-bit halfword of palette memory.</summary>
	void WritePal16(GbaBus& bus, u32 offset, u16 value)
	{
		// A 16-bit palette access must not be assembled from two byte writes either: the byte
		// accessor only drives one lane, so the two halves would both end up holding the same
		// value and the OR rule of a byte read would then show it twice.
		bus.ppu.WritePalette16(offset, value);
	}

	/// <summary>Write a 16-bit halfword of VRAM.</summary>
	void WriteVram16(GbaBus& bus, u32 offset, u16 value)
	{
		bus.ppu.WriteVram(offset, (u8)(value & 0xFF));
		bus.ppu.WriteVram(offset + 1, (u8)(value >> 8));
	}

	/// <summary>Write a 16-bit halfword of OAM.</summary>
	void WriteOam16(GbaBus& bus, u32 offset, u16 value)
	{
		bus.ppu.WriteOam(offset, (u8)(value & 0xFF));
		bus.ppu.WriteOam(offset + 1, (u8)(value >> 8));
	}

	/// <summary>Fill `count` bytes of VRAM starting at `offset`.</summary>
	void FillVram(GbaBus& bus, u32 offset, u32 count, u8 value)
	{
		for (u32 i = 0; i < count; i++)
			bus.ppu.WriteVram(offset + i, value);
	}

	/// <summary>True when every dot of the row [from, to) has `expected`.</summary>
	bool RowMatches(const Ppu& ppu, int from, int to, u16 expected)
	{
		for (int x = from; x < to; x++)
		{
			if (ppu.LinePixel(x) != expected)
				return false;
		}

		return true;
	}

	// ---------------------------------------------------------------------------------------
	// Reference helpers. These read the same memory the PPU reads, but they never call the
	// renderer: every expected pixel is computed here from the GBATEK rules.
	// ---------------------------------------------------------------------------------------

	/// <summary>Read a 15-bit colour from BG palette RAM (each colour is one halfword).</summary>
	u16 RefBgColor(const Ppu& ppu, int index)
	{
		return ppu.ReadPalette16((u32)index * 2);
	}

	/// <summary>Read a 15-bit colour from OBJ palette RAM (at 0x05000200).</summary>
	u16 RefObjColor(const Ppu& ppu, int index)
	{
		return ppu.ReadPalette16(0x200 + (u32)index * 2);
	}

	/// <summary>Resolve one dot of a 4bpp or 8bpp tile, applying the per-tile flips.</summary>
	u16 RefTileDot(const Ppu& ppu, u32 tileBase, int tileNumber, bool colors256, int paletteIndex,
		int tileX, int tileY, bool flipX, bool flipY)
	{
		if (flipX) tileX = 7 - tileX;
		if (flipY) tileY = 7 - tileY;

		if (colors256)
		{
			const u8 index = ppu.ReadVram(tileBase + (u32)tileNumber * 64 + (u32)tileY * 8 + (u32)tileX);
			return (index == 0) ? 0x8000 : RefBgColor(ppu, index);	// 0x8000 = transparent marker
		}

		const u8 packed = ppu.ReadVram(tileBase + (u32)tileNumber * 32 + (u32)tileY * 4 + (u32)tileX / 2);
		const u8 index = (tileX & 1) ? (u8)(packed >> 4) : (u8)(packed & 0xF);
		return (index == 0) ? 0x8000 : RefBgColor(ppu, paletteIndex * 16 + index);
	}
}

// ===========================================================================================
// Registers and memory access
// ===========================================================================================

GBA_TEST(Ppu, RegisterReadBack)
{
	GbaBus bus;
	SetupDisplay(bus);

	// DISPCNT, the BG controls and the effect registers are plain R/W (GBATEK "GBA I/O Map").
	WriteReg(bus, DISPCNT, DC_MODE0 | DC_BG0 | DC_OBJ);
	GBA_CHECK_HEX16(bus.ppu.Read16(DISPCNT, 0), DC_MODE0 | DC_BG0 | DC_OBJ);
	GBA_CHECK_HEX16(bus.ppu.DispCnt(), DC_MODE0 | DC_BG0 | DC_OBJ);

	WriteReg(bus, BG0CNT, 0x1C42);
	GBA_CHECK_HEX16(bus.ppu.Read16(BG0CNT, 0), 0x1C42);
	WriteReg(bus, BG1CNT, 0x0F03);
	GBA_CHECK_HEX16(bus.ppu.Read16(BG1CNT, 0), 0x0F03);

	WriteReg(bus, BLDCNT, 0x3F4C);
	GBA_CHECK_HEX16(bus.ppu.Read16(BLDCNT, 0), 0x3F4C);
	WriteReg(bus, BLDALPHA, 0x0C08);
	GBA_CHECK_HEX16(bus.ppu.Read16(BLDALPHA, 0), 0x0C08);
	WriteReg(bus, WININ, 0x3F3F);
	GBA_CHECK_HEX16(bus.ppu.Read16(WININ, 0), 0x3F3F);
	WriteReg(bus, WINOUT, 0x1F1F);
	GBA_CHECK_HEX16(bus.ppu.Read16(WINOUT, 0), 0x1F1F);

	// BLDY only has five bits (GBATEK 4000054h).
	WriteReg(bus, 0x054, 0xFFFF);
	GBA_CHECK_HEX16(bus.ppu.Read16(0x054, 0), 0x001F);

	// The undocumented Green Swap bit is bit 0 only (GBATEK 4000002h).
	WriteReg(bus, GREENSWP, 0xFFFF);
	GBA_CHECK_HEX16(bus.ppu.Read16(GREENSWP, 0), 0x0001);

	// VCOUNT is read-only: a write is dropped.
	WriteReg(bus, 0x006, 0x0055);
	GBA_CHECK_HEX16(bus.ppu.Read16(0x006, 0), bus.ppu.VCount());

	// Reading past the LCD block returns the open bus.
	GBA_CHECK_HEX16(bus.ppu.Read16(0x080, 0xBEEF), 0xBEEF);

	// The PPU is back to a known state.
	GBA_CHECK(bus.ppu.FrameCounter() == 0);
}

GBA_TEST(Ppu, PaletteByteAccessIsTheOrOfTheHalfword)
{
	GbaBus bus;
	SetupDisplay(bus);

	// GBATEK "LCD Color Palettes": a colour is one 16-bit entry. A byte read of palette RAM is
	// not a real access - the hardware returns the OR of the two bytes of the halfword.
	WritePal16(bus, 0x0000, 0x1234);
	GBA_CHECK_HEX16(bus.ppu.ReadPalette(0x0000), 0x12 | 0x34);
	GBA_CHECK_HEX16(bus.ppu.ReadPalette(0x0001), 0x12 | 0x34);

	// The low entry of the pair 0x0F00 | 0xF000 = 0xFF00... take two entries with distinct bytes.
	WritePal16(bus, 0x0002, 0x0F00);
	GBA_CHECK_HEX16(bus.ppu.ReadPalette(0x0002), 0x0F);
	GBA_CHECK_HEX16(bus.ppu.ReadPalette(0x0003), 0x0F);

	// A byte write only drives the lane the CPU drove; the other half keeps its value. The entry
	// 0x0F00 left in the high half therefore shows up in the byte read's OR.
	bus.ppu.WritePalette(0x0002, 0x55);
	GBA_CHECK_HEX16(bus.ppu.ReadPalette(0x0002), 0x55 | 0x0F);
	GBA_CHECK_HEX16(bus.ppu.ReadPalette(0x0003), 0x55 | 0x0F);

	// Clear the high half and check the plain byte.
	bus.ppu.WritePalette(0x0003, 0x00);
	GBA_CHECK_HEX16(bus.ppu.ReadPalette(0x0002), 0x55);
	GBA_CHECK_HEX16(bus.ppu.ReadPalette(0x0003), 0x55);

	// An 8-bit store of the low lane must not disturb the high one.
	bus.ppu.WritePalette(0x0002, 0x0A);
	GBA_CHECK_HEX16(bus.ppu.ReadPalette(0x0002), 0x0A);
	GBA_CHECK_HEX16(bus.ppu.ReadPalette16(0x0002), 0x000A);

	// The 1 KByte of palette RAM is mirrored through its size (GBATEK "GBA Memory Map"), and the
	// byte access keeps its OR rule through the mirror.
	WritePal16(bus, 0x0000, 0x7FFF);
	GBA_CHECK_HEX16(bus.ppu.ReadPalette16(0x400), 0x7FFF);
	GBA_CHECK_HEX16(bus.ppu.ReadPalette16(PaletteSize), 0x7FFF);
	GBA_CHECK_HEX16(bus.ppu.ReadPalette(0x400), 0xFF);		// 0x7F | 0xFF
}

GBA_TEST(Ppu, VramWindowsPerMode)
{
	GbaBus bus;
	SetupDisplay(bus);

	// BG modes 0-2: the whole 96 KByte is one window. (96 KByte is not a power of two, so the
	// byte bank wraps an address one past the end with a modulo instead of a mask.)
	bus.ppu.WriteVram(0x0000, 0x5A);
	GBA_CHECK_HEX16(bus.ppu.ReadVram(0x0000), 0x5A);
	GBA_CHECK_HEX16(bus.ppu.ReadVram(VramSize * 2), 0x5A);	// 0x30000 % 0x18000 == 0

	// BG modes 3-5: the OBJ tiles live at 0x06014000 (GBATEK "LCD VRAM Overview"), and the
	// 0x10000..0x13FFF hole is not real memory.
	WriteReg(bus, DISPCNT, DC_MODE3);
	bus.ppu.WriteVram(0x0000, 0x11);
	bus.ppu.WriteVram(OBJ_TILES_BITMAP, 0x22);
	GBA_CHECK_HEX16(bus.ppu.ReadVram(0x0000), 0x11);
	GBA_CHECK_HEX16(bus.ppu.ReadVram(OBJ_TILES_BITMAP), 0x22);
	GBA_CHECK_HEX16(bus.ppu.ReadVram(OBJ_TILES), 0x00);		// the hole reads as zero

	// OAM takes 16-bit writes: a byte write to the high byte is ignored (GBATEK "GBA Memory Map"),
	// a byte write to the low byte stores that byte.
	WriteOam16(bus, 0x0000, 0xABCD);
	bus.ppu.WriteOam(0x0001, 0x99);
	GBA_CHECK_HEX16(bus.ppu.ReadOam(0x0000), 0xCD);
	GBA_CHECK_HEX16(bus.ppu.ReadOam(0x0001), 0x00);
	GBA_CHECK_HEX16(bus.ppu.ReadOam(OamSize), 0xCD);
}

// ===========================================================================================
// Backgrounds
// ===========================================================================================

GBA_TEST(Ppu, BackdropColor)
{
	GbaBus bus;
	SetupDisplay(bus);

	// GBATEK "LCD Color Palettes": colour 0 of BG palette 0 is the backdrop, displayed where no
	// non-transparent dot of a layer covers the screen.
	WritePal16(bus, 0x0000, 0x1234);
	WriteReg(bus, DISPCNT, DC_MODE0);

	bus.ppu.RenderLine(bus, 0);

	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x1234);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(239), 0x1234);

	// The frame buffer holds the same colour in XRGB8888 (the 5-bit fields expanded with the
	// "replicate the high bits" rule of Color15ToXrgb).
	const u32 expected = Color15ToXrgb(0x1234);
	GBA_CHECK_HEX32(bus.ppu.Frame()[0], expected);
	GBA_CHECK_HEX32(bus.ppu.FramePixels()[0], expected);

	// Every dot of every line has it.
	bus.ppu.RenderLine(bus, 159);
	for (int x = 0; x < ScreenWidth; x++)
		GBA_CHECK_HEX16(bus.ppu.LinePixel(x), 0x1234);
}

GBA_TEST(Ppu, TextBg4bppScrollAndFlip)
{
	GbaBus bus;
	SetupDisplay(bus);

	// The tile data lives in character base block 1 (0x06004000) and the map in screen base
	// block 0 (0x06000000); keeping them apart is what the addresses in BG0CNT are for
	// (GBATEK 4000008h: character base in units of 16 KByte, screen base in units of 2 KByte).
	const u32 charBase = 0x4000;
	const u32 screenBase = 0x0000;
	WriteReg(bus, BG0CNT, BG_CNT(0, 0x4000, 0));

	// BG palette entry 1 of palette 0 = red.
	WritePal16(bus, 0x0002, 0x001F);

	// Tile 0: every dot is index 1 (a 4bpp tile is 32 bytes, the low nibble is the left dot).
	FillVram(bus, charBase, 32, 0x11);

	// Make the leftmost dot of the top row transparent (low nibble of the row's first byte).
	bus.ppu.WriteVram(charBase, 0x10);

	// Map entry 0: tile 0 of palette 0, no flips.
	WriteVram16(bus, screenBase, 0x0000);

	// BG0HOFS = 3: the dot at screen x shows the source dot x + 3.
	WriteReg(bus, BG0HOFS, 3);
	WriteReg(bus, DISPCNT, DC_MODE0 | DC_BG0);

	bus.ppu.RenderLine(bus, 0);

	// Screen x = 0 samples source dot 3, which is index 1.
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x001F);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(4), 0x001F);

	// Scroll the transparent dot into view: with HOFS = 252 the source dot at the right of the
	// visible line is (239 + 252) & 255 = 235 in the tile column 29 - which has no map entry, so
	// the transparent dot of tile 0 is only reachable through the flip below.

	// A per-tile horizontal flip (BG map entry bit 10) mirrors the tile, putting the transparent
	// dot at tile dot 7.
	WriteReg(bus, BG0HOFS, 0);
	WriteVram16(bus, screenBase, 0x0400);
	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x001F);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(6), 0x001F);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(7), 0x0000);		// the transparent dot

	// Without the flip dot 0 is the transparent one.
	WriteVram16(bus, screenBase, 0x0000);
	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x0000);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(1), 0x001F);

	// A vertical flip (bit 11) moves the transparent dot between the rows: make only row 0
	// transparent and check line 0 and line 7.
	for (int y = 0; y < 8; y++)
		FillVram(bus, charBase + (u32)y * 4, 4, (y == 0) ? 0x00 : 0x11);

	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x0000);		// row 0 is transparent

	WriteVram16(bus, screenBase, 0x0800);				// vertical flip
	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x001F);		// shows the tile's row 7
	bus.ppu.RenderLine(bus, 7);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x0000);		// shows the tile's row 0
}

GBA_TEST(Ppu, TextBg8bpp)
{
	GbaBus bus;
	SetupDisplay(bus);

	// BG0CNT bit 7 set: 256 colours/1 palette (GBATEK 4000008h); the tile data is in character
	// base block 1 again, the map in screen base block 0.
	const u32 charBase = 0x4000;
	WriteReg(bus, BG0CNT, BG_CNT(0, 0x4000, 0) | 0x0080);

	// 8bpp tiles are 64 bytes, one byte per dot selecting a BG palette entry directly.
	for (int y = 0; y < 8; y++)
	{
		for (int x = 0; x < 8; x++)
			bus.ppu.WriteVram(charBase + (u32)y * 8 + (u32)x, (u8)(y * 8 + x + 1));
	}

	// The dot at (3, 2) selects palette entry 2*8 + 3 + 1 = 20.
	WritePal16(bus, 20 * 2, 0x03E0);
	WriteVram16(bus, 0x0000, 0x0000);

	WriteReg(bus, DISPCNT, DC_MODE0 | DC_BG0);
	bus.ppu.RenderLine(bus, 2);

	GBA_CHECK_HEX16(bus.ppu.LinePixel(3), 0x03E0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(4), RefBgColor(bus.ppu, 2 * 8 + 4 + 1));

	// Dot (0, 0) selects palette entry 1, which is still 0, and colour 0 is transparent: the
	// backdrop shows instead (GBATEK "Transparent Colors").
	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x0000);

	// Give the backdrop a colour and check that the dot of palette entry 1 (still black) really
	// falls through to it.
	WritePal16(bus, 0x0000, 0x7C00);
	WritePal16(bus, 0x0002, 0x1234);
	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x1234);		// tile dot, palette entry 1
	GBA_CHECK_HEX16(bus.ppu.LinePixel(9), 0x7C00);		// the backdrop: an unwritten map entry
}

GBA_TEST(Ppu, TextBgTileMapWrap512)
{
	GbaBus bus;
	SetupDisplay(bus);

	// Size 1: two 256x256 dot areas side by side, i.e. a 512x256 map, with SC1 at map base + 2K
	// (GBATEK 4000008h). Character base block 1 (0x06004000), screen base block 0.
	const u32 charBase = 0x4000;
	WriteReg(bus, BG0CNT, BG_CNT(0, 0x4000, 0) | (1 << 14));
	WritePal16(bus, 0x0002, 0x001F);
	WritePal16(bus, 0x0004, 0x03E0);

	// A solid tile 0 and a solid tile 1, both filled with 0x11 (pixel index 1 in both nibbles).
	// The two layers are told apart by the palette field of the map entry: SC0 uses palette 0 and
	// SC1 uses palette 1, so the same tile colour selects a different palette entry.
	FillVram(bus, charBase + 0 * 32, 32, 0x11);
	FillVram(bus, charBase + 1 * 32, 32, 0x11);

	// SC0 is a 32x32 entry map at the screen base; its entry for the tile column 30 is tile 0
	// with palette 0. The whole of SC1 (2 KByte further on) is filled with tile 1 of palette 1,
	// so every dot that falls into the second 256x256 area selects the second colour.
	for (u32 i = 0; i < 0x800; i += 2)
		WriteVram16(bus, 0x0800 + i, 0x1001);
	WriteVram16(bus, 0x0000 + 30 * 2, 0x0000);

	// Scroll so that screen x = 0 shows source dot 240, i.e. tile column 30. Screen x = 16 then
	// shows source dot 256, the first dot of SC1's tile row.
	WriteReg(bus, BG0HOFS, 240);
	WriteReg(bus, DISPCNT, DC_MODE0 | DC_BG0);

	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x001F);		// SC0, tile 0, palette 0
	GBA_CHECK_HEX16(bus.ppu.LinePixel(15), 0x001F);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(16), 0x03E0);		// SC1, tile 1, palette 1
	GBA_CHECK_HEX16(bus.ppu.LinePixel(31), 0x03E0);

	// Every dot of the second area uses SC1, which is what proves the block selection: a
	// linear-row read of the map would have used SC0's (empty) row for most of the line.
	GBA_CHECK_HEX16(bus.ppu.LinePixel(239), 0x03E0);

	// Prove the wrap: a scroll of 240 + 512 = 752 is the same map dot as a scroll of 240
	// (GBATEK 4000008h: "When the screen is scrolled it'll always wraparound").
	WriteReg(bus, BG0HOFS, 752);
	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x001F);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(16), 0x03E0);

	// The vertical axis wraps through the 256-dot height just the same.
	WriteReg(bus, BG0HOFS, 240);
	WriteReg(bus, BG0VOFS, 256);
	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x001F);
}

GBA_TEST(Ppu, PriorityFightBetweenTwoBgs)
{
	GbaBus bus;
	SetupDisplay(bus);

	// Both layers cover the whole screen. BG1 has the better priority number (0) and must win.
	// Character base block 1 for both, screen base block 0 for BG0 and block 1 (0x0800) for BG1.
	const u32 charBase = 0x4000;
	WriteReg(bus, BG0CNT, BG_CNT(1, 0x4000, 0));			// BG0: priority 1
	WriteReg(bus, BG1CNT, BG_CNT(0, 0x4000, 1));			// BG1: priority 0

	WritePal16(bus, 0x0002, 0x001F);					// palette 0 index 1 = red
	WritePal16(bus, 0x0022, 0x7C00);					// palette 1 index 1 = blue

	// BG0 uses tile 0 with palette 0, BG1 uses tile 0 with palette 1.
	FillVram(bus, charBase + 0 * 32, 32, 0x11);
	WriteVram16(bus, 0x0000, 0x0000);					// BG0 map entry: tile 0, palette 0
	WriteVram16(bus, 0x0000 + 0x800, 0x1000);			// BG1 map entry: tile 0, palette 1

	WriteReg(bus, DISPCNT, DC_MODE0 | DC_BG0 | DC_BG1);
	bus.ppu.RenderLine(bus, 0);

	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x7C00);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(239), 0x7C00);

	// Swap the priorities: now BG0 wins.
	WriteReg(bus, BG0CNT, BG_CNT(0, 0x4000, 0));
	WriteReg(bus, BG1CNT, BG_CNT(1, 0x4000, 1));
	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x001F);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(239), 0x001F);

	// Equal priorities: BG0 wins, because with the same priority number BG0 is above BG1
	// (GBATEK 4000008h: "In case that some or all BGs are set to same priority then BG0 is
	// having the highest, and BG3 the lowest priority").
	WriteReg(bus, BG0CNT, BG_CNT(0, 0x4000, 0));
	WriteReg(bus, BG1CNT, BG_CNT(0, 0x4000, 1));
	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x001F);
}

GBA_TEST(Ppu, AffineBgRotationAndReferenceAdvance)
{
	GbaBus bus;
	SetupDisplay(bus);

	// Mode 2 with BG2: the only affine layer (GBATEK DISPCNT mode table).
	// BG2CNT: priority 0, char base 0x0000, screen base 0x0000, size 0 (128x128 dots).
	WriteReg(bus, BG2CNT, 0x0000);

	// The rotation/scaling map is one byte per entry and always 256 colours (GBATEK
	// "Rotation/Scaling BG Screen"): tile 5.
	bus.ppu.WriteVram(0x0000, 5);

	// Tile 5 (at character base 0x0000, 64 bytes per 8bpp tile) is solid palette index 6.
	FillVram(bus, 5 * 64, 64, 6);
	WritePal16(bus, 6 * 2, 0x1234);

	// A = 0x0100 (1.0), B = 0x0010 (0.0625), C = 0x0000, D = 0x0100.
	// (srcX, srcY) = reference + M * (x, y), so the rotation moves the sampled column to the
	// right as the line grows, and PB advances the reference between scanlines.
	WriteReg(bus, BG2PA, 0x0100);
	WriteReg(bus, 0x022, 0x0010);
	WriteReg(bus, 0x024, 0x0000);
	WriteReg(bus, 0x026, 0x0100);

	// Reference point (44, 66) in 8.8 fixed point: X_L then X_H, Y_L then Y_H (GBATEK 4000028h).
	WriteReg(bus, BG2X_L, 44);
	WriteReg(bus, 0x02A, 0);
	WriteReg(bus, 0x02C, 66);
	WriteReg(bus, 0x02E, 0);

	WriteReg(bus, DISPCNT, DC_MODE1 | DC_BG2);		// mode 2 == the affine layers, use mode 2
	WriteReg(bus, DISPCNT, 0x0002 | DC_BG2);

	// Line 0: dot 0 samples src (44, 66) = tile (5, 8) -> tile map entry 0 (tile 5), dot (4, 2)
	// of tile 5, which is solid index 6.
	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x1234);

	// Dot 1 samples srcX = 44 + 1 = 45, which is still tile column 5.
	GBA_CHECK_HEX16(bus.ppu.LinePixel(1), 0x1234);

	// The reference point is advanced by PB (0.0625) per scanline: line 1 samples
	// 44 + 0.0625 + 1 = 45.0625 for dot 0, i.e. the same tile dot, so the colour is unchanged.
	bus.ppu.RenderLine(bus, 1);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x1234);

	// With a reference point that lands on the transparent part of the map - an empty tile map
	// entry - the layer is transparent and the backdrop shows. Scroll the map column to 16 (the
	// entry at 16 is unwritten) and use a pure X reference of 128.
	WriteReg(bus, BG2PA, 0x0100);
	WriteReg(bus, 0x022, 0x0000);
	WriteReg(bus, 0x024, 0x0000);
	WriteReg(bus, 0x026, 0x0100);
	WriteReg(bus, BG2X_L, 128);
	WriteReg(bus, 0x02A, 0);
	WriteReg(bus, 0x02C, 0);
	WriteReg(bus, 0x02E, 0);

	WritePal16(bus, 0x0000, 0x03E0);		// the backdrop, so a miss is visible
	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x03E0);
}

GBA_TEST(Ppu, Mode3DirectColor)
{
	GbaBus bus;
	SetupDisplay(bus);

	WriteReg(bus, DISPCNT, DC_MODE3 | DC_BG2);

	// GBATEK "BG Mode 3": two bytes per dot, 480 bytes per line, 240x160 dots at 0x06000000.
	const u32 offset = (u32)10 * 480 + (u32)50 * 2;
	WriteVram16(bus, offset, 0x7FFF);

	bus.ppu.RenderLine(bus, 10);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(50), 0x7FFF);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(49), 0x0000);		// the backdrop

	// Mode 3 has no transparent colour, so a black bitmap dot is drawn as black and not as the
	// backdrop. Put a distinct backdrop in and check that dot (0, 0) stays black.
	WritePal16(bus, 0x0000, 0x001F);
	bus.ppu.RenderLine(bus, 10);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x0000);
}

GBA_TEST(Ppu, Mode4PagesAndPalette)
{
	GbaBus bus;
	SetupDisplay(bus);

	// GBATEK "BG Mode 4": one byte per dot, 240 bytes per line, frame 0 at 0x06000000 and frame 1
	// at 0x0600A000 (selected by DISPCNT bit 4).
	bus.ppu.WriteVram(0x00000 + 20 * 240 + 30, 5);
	bus.ppu.WriteVram(0x0A000 + 20 * 240 + 30, 7);
	WritePal16(bus, 5 * 2, 0x001F);
	WritePal16(bus, 7 * 2, 0x03E0);

	WriteReg(bus, DISPCNT, DC_MODE4 | DC_BG2);
	bus.ppu.RenderLine(bus, 20);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(30), 0x001F);

	WriteReg(bus, DISPCNT, DC_MODE4 | DC_BG2 | DC_FRAME1);
	bus.ppu.RenderLine(bus, 20);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(30), 0x03E0);

	// Palette colour 0 is transparent in mode 4, so the backdrop shows (GBATEK "BG Mode 4").
	WritePal16(bus, 0x0000, 0x7C00);
	bus.ppu.WriteVram(0x0A000 + 20 * 240 + 31, 0);
	bus.ppu.RenderLine(bus, 20);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(31), 0x7C00);
}

// ===========================================================================================
// Sprites
// ===========================================================================================

GBA_TEST(Ppu, Sprite16Colors)
{
	GbaBus bus;
	SetupDisplay(bus);

	// A solid OBJ tile 0 (32 bytes of 0x55: colour index 5 in both dots of every row).
	FillVram(bus, OBJ_TILES, 32, 0x55);
	WritePal16(bus, 0x200 + 5 * 2, 0x001F);

	// OBJ0: 8x8 square at (10, 20), 16 colours, palette 0, tile 0, OBJ/BG priority 0.
	// attr0 = Y | shape(0) | colour depth(0); attr1 = X | size(0); attr2 = tile | priority | pal.
	WriteOam16(bus, 0x00, 20);
	WriteOam16(bus, 0x02, 10);
	WriteOam16(bus, 0x04, 0);

	WriteReg(bus, DISPCNT, DC_MODE0 | DC_OBJ);
	bus.ppu.RenderLine(bus, 20);

	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x001F);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(17), 0x001F);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(9), 0x0000);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(18), 0x0000);

	// The sprite only covers the lines 20..27.
	bus.ppu.RenderLine(bus, 19);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x0000);
	bus.ppu.RenderLine(bus, 28);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x0000);

	// DISPCNT bit 12 is the OBJ master enable: without it the sprite disappears.
	WriteReg(bus, DISPCNT, DC_MODE0);
	bus.ppu.RenderLine(bus, 20);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x0000);
}

GBA_TEST(Ppu, Sprite256Colors)
{
	GbaBus bus;
	SetupDisplay(bus);

	// A 16x16 square (shape 0, size 1) of 256-colour tiles at (100, 20). The tile data is written
	// for the whole 16x16 dot area so that the assertion does not depend on which 8x8 tile of the
	// matrix a dot lands in: the left half is colour index 7 and the right half index 9.
	// GBATEK "OBJ Tile Number": each tile number is even in 256-colour mode, so the second tile of
	// the row is number 2 (a 16x16 OBJ has no tile to its right within tile 0).
	for (int y = 0; y < 16; y++)
	{
		for (int x = 0; x < 8; x++)
		{
			bus.ppu.WriteVram(OBJ_TILES + (u32)y * 64 + (u32)x, 7);
			bus.ppu.WriteVram(OBJ_TILES + (u32)y * 64 + 8 + (u32)x, 9);
		}
	}

	WritePal16(bus, 0x200 + 7 * 2, 0x001F);
	WritePal16(bus, 0x200 + 9 * 2, 0x7C00);

	// attr0: Y | shape 0 | colours 256 (bit 13); attr1: X | size 1; attr2: tile 0.
	WriteOam16(bus, 0x00, 20 | 0x2000);
	WriteOam16(bus, 0x02, 100 | (1 << 14));
	WriteOam16(bus, 0x04, 0);

	WriteReg(bus, DISPCNT, DC_MODE0 | DC_OBJ);
	bus.ppu.RenderLine(bus, 25);

	GBA_CHECK_HEX16(bus.ppu.LinePixel(100), 0x001F);	// tile 0
	GBA_CHECK_HEX16(bus.ppu.LinePixel(107), 0x001F);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(108), 0x7C00);	// tile 2, the second tile of the row
	GBA_CHECK_HEX16(bus.ppu.LinePixel(115), 0x7C00);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(116), 0x0000);
}

GBA_TEST(Ppu, SpriteHorizontalFlip)
{
	GbaBus bus;
	SetupDisplay(bus);

	// A 8x8 sprite whose tile has index 2 in its left half (dots 0-3) and index 1 in its right
	// half (dots 4-7): the low nibble of a byte is the left dot (GBATEK "LCD VRAM Character
	// Data"), so byte 0 is 0x22 and byte 3 is 0x11.
	for (int y = 0; y < 8; y++)
	{
		bus.ppu.WriteVram(OBJ_TILES + (u32)y * 4 + 0, 0x22);
		bus.ppu.WriteVram(OBJ_TILES + (u32)y * 4 + 1, 0x22);
		bus.ppu.WriteVram(OBJ_TILES + (u32)y * 4 + 2, 0x11);
		bus.ppu.WriteVram(OBJ_TILES + (u32)y * 4 + 3, 0x11);
	}

	WritePal16(bus, 0x200 + 1 * 2, 0x001F);
	WritePal16(bus, 0x200 + 2 * 2, 0x03E0);

	WriteOam16(bus, 0x00, 20);
	WriteOam16(bus, 0x02, 10);
	WriteOam16(bus, 0x04, 0);

	WriteReg(bus, DISPCNT, DC_MODE0 | DC_OBJ);
	bus.ppu.RenderLine(bus, 20);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x03E0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(14), 0x001F);

	// Bit 12 of attribute 1 mirrors the sprite (GBATEK "OBJ Attribute 1").
	WriteOam16(bus, 0x02, 10 | 0x1000);
	bus.ppu.RenderLine(bus, 20);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x001F);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(14), 0x03E0);

	// The vertical flip (bit 13) mirrors it on the Y axis.
	for (int y = 0; y < 4; y++)
		FillVram(bus, OBJ_TILES + (u32)y * 4, 4, 0x11);
	for (int y = 4; y < 8; y++)
		FillVram(bus, OBJ_TILES + (u32)y * 4, 4, 0x22);

	WriteOam16(bus, 0x02, 10);
	bus.ppu.RenderLine(bus, 20);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x001F);
	bus.ppu.RenderLine(bus, 24);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x03E0);

	WriteOam16(bus, 0x02, 10 | 0x2000);
	bus.ppu.RenderLine(bus, 20);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x03E0);
	bus.ppu.RenderLine(bus, 24);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x001F);
}

GBA_TEST(Ppu, AffineSpriteIdentityMatrix)
{
	GbaBus bus;
	SetupDisplay(bus);

	// A 16x16 affine sprite at (50, 30) with the identity matrix. The 256-colour tile data is
	// written for the whole 16x16 dot area (each tile is 64 bytes, one byte per dot; the second
	// tile of a matrix row is number 2 in 256-colour mode).
	for (int y = 0; y < 16; y++)
	{
		for (int x = 0; x < 16; x++)
			bus.ppu.WriteVram(OBJ_TILES + (u32)y * 64 + (u32)x, 5);
	}
	WritePal16(bus, 0x200 + 5 * 2, 0x001F);

	// attr0: Y | rotation/scaling flag (bit 8); attr1: X | size 1 | matrix group 0 (bits 9-13 = 0).
	WriteOam16(bus, 0x00, 30 | 0x0100);
	WriteOam16(bus, 0x02, 50 | (1 << 14));
	WriteOam16(bus, 0x04, 0);

	// Group 0: PA at 0x006, PB at 0x00E, PC at 0x016, PD at 0x01E (GBATEK "Location of Rotation/
	// Scaling Parameters in OAM").
	WriteOam16(bus, 0x006, 0x0100);
	WriteOam16(bus, 0x00E, 0x0000);
	WriteOam16(bus, 0x016, 0x0000);
	WriteOam16(bus, 0x01E, 0x0100);

	WriteReg(bus, DISPCNT, DC_MODE0 | DC_OBJ);
	bus.ppu.RenderLine(bus, 30);

	// The reference point is the OBJ's own X/Y, and the rotation center is the middle of the base
	// OBJ; with the identity matrix the sprite lands exactly on (50, 30)-(65, 45).
	GBA_CHECK_HEX16(bus.ppu.LinePixel(50), 0x001F);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(65), 0x001F);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(49), 0x0000);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(66), 0x0000);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(50 - 1), 0x0000);

	// The sprite does not cover line 46.
	bus.ppu.RenderLine(bus, 46);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(50), 0x0000);
}

GBA_TEST(Ppu, SpriteVersusBackgroundPriority)
{
	GbaBus bus;
	SetupDisplay(bus);

	// BG0 (priority 0) with a solid tile, and a sprite with OBJ/BG priority 1: the background
	// wins even though the sprite is drawn on top of the stack first (GBATEK "Priority": a
	// lower priority number is displayed above a higher one). The BG uses character base block 1
	// (0x06004000) so its tile does not collide with the OBJ tiles at 0x06010000.
	WriteReg(bus, BG0CNT, BG_CNT(0, 0x4000, 0));
	WritePal16(bus, 0x0002, 0x001F);		// BG palette index 1 = red
	FillVram(bus, 0x4000, 32, 0x11);		// BG tile 0 solid index 1
	WriteVram16(bus, 0x0000, 0x0000);		// BG map entry: tile 0, palette 0

	FillVram(bus, OBJ_TILES, 32, 0x55);		// OBJ tile 0 solid index 5
	WritePal16(bus, 0x200 + 5 * 2, 0x7C00);	// OBJ palette index 5 = blue

	// attr2 = tile 0 | priority 1 (bits 10-11).
	WriteOam16(bus, 0x00, 20);
	WriteOam16(bus, 0x02, 10);
	WriteOam16(bus, 0x04, 0 | (1 << 10));

	WriteReg(bus, DISPCNT, DC_MODE0 | DC_BG0 | DC_OBJ);
	bus.ppu.RenderLine(bus, 20);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x001F);

	// With OBJ priority 0 and BG priority 0 the sprite wins: "the OBJ becomes higher priority and
	// is displayed on top of that BG layer".
	WriteOam16(bus, 0x04, 0 | (0 << 10));
	bus.ppu.RenderLine(bus, 20);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x7C00);

	// A background with a better priority number hides the sprite again.
	WriteReg(bus, BG0CNT, BG_CNT(0, 0x4000, 0));
	WriteOam16(bus, 0x04, 0 | (2 << 10));
	bus.ppu.RenderLine(bus, 20);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x001F);
}

// ===========================================================================================
// Blending, windows, forced blank
// ===========================================================================================

GBA_TEST(Ppu, SemiTransparentSpriteAlphaBlendsWithTheBg)
{
	GbaBus bus;
	SetupDisplay(bus);

	// A solid red BG0 under a semi-transparent blue sprite. With EVA = EVB = 8 each component is
	// (I1st * 8 + I2nd * 8) >> 4 (GBATEK 4000052h), so red 31 and blue 31 give r = 15 and
	// b = 15 (the colour layout is 0bbbbbgggggrrrrr, so 0x7C00 is blue and 0x001F is red).
	WriteReg(bus, BG0CNT, BG_CNT(0, 0x4000, 0));
	WritePal16(bus, 0x0002, 0x001F);
	FillVram(bus, 0x4000, 32, 0x11);
	WriteVram16(bus, 0x0000, 0x0000);

	FillVram(bus, OBJ_TILES, 32, 0x55);
	WritePal16(bus, 0x200 + 5 * 2, 0x7C00);

	// attr0: Y | OBJ mode 1 (bits 10-11 = 01), i.e. semi-transparent.
	WriteOam16(bus, 0x00, 20 | (1 << 10));
	WriteOam16(bus, 0x02, 10);
	WriteOam16(bus, 0x04, 0);

	// BLDCNT: alpha blending (bits 6-7 = 01), BG0 both as 1st (bit 0) and 2nd (bit 8) target, the
	// backdrop as 2nd target (bit 13). A semi-transparent OBJ ignores BLDCNT and always blends.
	WriteReg(bus, BLDCNT, 0x2101 | (1 << 6));
	WriteReg(bus, BLDALPHA, 8 | (8 << 8));

	WriteReg(bus, DISPCNT, DC_MODE0 | DC_BG0 | DC_OBJ);
	bus.ppu.RenderLine(bus, 20);

	// The sprite blends with the red background underneath it: r = (0*8 + 31*8)/16 = 15,
	// b = (31*8 + 0*8)/16 = 15.
	const u16 expected = Blend15(0x7C00, 0x001F, 8, 8);
	GBA_CHECK_HEX16(expected, 0x3C0F);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), expected);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(17), expected);

	// Outside of the sprite the background is unchanged (the backdrop is the 2nd target but the
	// background is opaque, so the 1st/2nd pair is BG0 with the backdrop behind it).
	GBA_CHECK_HEX16(bus.ppu.LinePixel(9), 0x001F);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(18), 0x001F);
}

GBA_TEST(Ppu, AlphaBlendingOfTwoBgs)
{
	GbaBus bus;
	SetupDisplay(bus);

	// BG0 red on top of BG1 blue, with BLDCNT selecting BG0 as the 1st and BG1 as the 2nd target.
	// Both layers use character base block 1 (0x06004000) so their tiles do not collide with the
	// BG0 map at 0x06000000 or with BG1's map at 0x06000800.
	WriteReg(bus, BG0CNT, BG_CNT(0, 0x4000, 0));		// BG0: priority 0, screen base 0
	WriteReg(bus, BG1CNT, BG_CNT(1, 0x4000, 1));		// BG1: priority 1, screen base 1 (0x0800)

	WritePal16(bus, 0x0002, 0x001F);				// index 1 = red
	WritePal16(bus, 0x0022, 0x7C00);				// index 17 = blue

	FillVram(bus, 0x4000, 32, 0x11);				// BG0 tile 0: index 1 of palette 0
	FillVram(bus, 0x4020, 32, 0x11);				// BG1 tile 0: index 1 of palette 1
	WriteVram16(bus, 0x0000, 0x0000);				// BG0 map entry: tile 0, palette 0
	WriteVram16(bus, 0x0800, 0x1000);				// BG1 map entry: tile 0, palette 1

	// BLDCNT: effect 1 (alpha), BG0 1st (bit 0), BG1 2nd (bit 9).
	WriteReg(bus, BLDCNT, 0x0001 | (1 << 9) | (1 << 6));
	WriteReg(bus, BLDALPHA, 8 | (8 << 8));

	WriteReg(bus, DISPCNT, DC_MODE0 | DC_BG0 | DC_BG1);
	bus.ppu.RenderLine(bus, 0);

	// BG1 is drawn first, then BG0 wins, and the blend uses BG1 as the 2nd target.
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), Blend15(0x001F, 0x7C00, 8, 8));

	// Turn the effect off: the top layer is displayed at its normal intensity (GBATEK 4000050h).
	WriteReg(bus, BLDCNT, 0x0001);
	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x001F);
}

GBA_TEST(Ppu, BrightnessDecreaseOfTheBackdrop)
{
	GbaBus bus;
	SetupDisplay(bus);

	// With the backdrop as the 1st target, BLDY darkens it (GBATEK 4000054h:
	// I = I1st - I1st*EVY/16). With EVY = 16 every component loses all of its intensity, so the
	// white backdrop becomes black.
	WritePal16(bus, 0x0000, 0x7FFF);

	// BLDCNT: effect 3 (brightness decrease), backdrop 1st target (bit 5).
	WriteReg(bus, BLDCNT, 0x0020 | (3 << 6));
	WriteReg(bus, 0x054, 16);

	WriteReg(bus, DISPCNT, DC_MODE0);
	bus.ppu.RenderLine(bus, 0);

	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x0000);

	// Half: (31 - 31*8/16) = 15 for every component, i.e. 0x3DEF.
	WriteReg(bus, 0x054, 8);
	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x3DEF);

	// Brightness increase of the backdrop with EVY = 16 saturates to white.
	WritePal16(bus, 0x0000, 0x0000);
	WriteReg(bus, BLDCNT, 0x0020 | (2 << 6));
	WriteReg(bus, 0x054, 16);
	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x7FFF);
}

GBA_TEST(Ppu, WindowMasksOneBg)
{
	GbaBus bus;
	SetupDisplay(bus);

	// A solid BG0 covers the whole screen, but window 0 only enables it in the columns 10..49 and
	// the lines 5..24 (GBATEK 4000040h: X1 inclusive, X2 exclusive).
	WriteReg(bus, BG0CNT, 0x0000);
	WritePal16(bus, 0x0002, 0x03E0);
	FillVram(bus, 0x0000, 32, 0x11);
	WriteVram16(bus, 0x4000 + 0, 0x0000);

	WriteReg(bus, WIN0H, (10 << 8) | 50);
	WriteReg(bus, WIN0V, (5 << 8) | 25);

	// WININ: window 0 shows BG0 (bit 0) and enables the colour effect (bit 5); window 1 is unused.
	WriteReg(bus, WININ, 0x0021);
	// WINOUT: the outside area shows the backdrop only (no layer bits), the OBJ window is unused.
	WriteReg(bus, WINOUT, 0x0000);

	WriteReg(bus, DISPCNT, DC_MODE0 | DC_BG0 | DC_WIN0);
	bus.ppu.RenderLine(bus, 10);

	// Inside the window BG0 shows, outside only the backdrop (colour 0 = black).
	GBA_CHECK_HEX16(bus.ppu.LinePixel(9), 0x0000);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x03E0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(49), 0x03E0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(50), 0x0000);

	// The window is also limited vertically.
	bus.ppu.RenderLine(bus, 4);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x0000);
	bus.ppu.RenderLine(bus, 24);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x0000);

	// Window 0 wins over window 1 in an overlap (GBATEK "Window Priority"). Window 1 shows BG0 in
	// the columns 0..99, so in the overlap the BG is still shown, and outside window 0 but inside
	// window 1 it is shown too.
	WriteReg(bus, 0x042, (0 << 8) | 100);		// WIN1H: 0..99
	WriteReg(bus, 0x046, (0 << 8) | 160);		// WIN1V: 0..159
	WriteReg(bus, WININ, 0x0021 | (0x21 << 8));

	bus.ppu.RenderLine(bus, 10);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(5), 0x03E0);		// inside window 1 only
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x03E0);		// window 0 wins
	GBA_CHECK_HEX16(bus.ppu.LinePixel(99), 0x03E0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(100), 0x0000);	// outside both windows

	// Disabling window 0 (DISPCNT bit 13) makes the outside region cover everything.
	WriteReg(bus, DISPCNT, DC_MODE0 | DC_BG0);
	bus.ppu.RenderLine(bus, 10);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(10), 0x0000);
}

GBA_TEST(Ppu, ForcedBlankIsWhite)
{
	GbaBus bus;
	SetupDisplay(bus);

	// Draw a red background first, then force the blank (GBATEK "Blanking Bits": "Setting Forced
	// Blank causes the video controller to display white lines").
	WriteReg(bus, BG0CNT, 0x0000);
	WritePal16(bus, 0x0002, 0x001F);
	FillVram(bus, 0x0000, 32, 0x11);
	WriteVram16(bus, 0x4000 + 0, 0x0000);

	WriteReg(bus, DISPCNT, DC_MODE0 | DC_BG0 | DC_FORCED_BLANK);
	GBA_CHECK(bus.ppu.ForcedBlank());

	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x7FFF);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(239), 0x7FFF);
	GBA_CHECK_HEX32(bus.ppu.Frame()[0], Color15ToXrgb(0x7FFF));

	// The Green Swap bit is applied to the blank screen too (it is a final-stage effect).
	WriteReg(bus, GREENSWP, 1);
	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), Swap16(0x7FFF));

	WriteReg(bus, GREENSWP, 0);
	WriteReg(bus, DISPCNT, DC_MODE0 | DC_BG0);
	GBA_CHECK(!bus.ppu.ForcedBlank());
	bus.ppu.RenderLine(bus, 0);
	GBA_CHECK_HEX16(bus.ppu.LinePixel(0), 0x001F);
}

// ===========================================================================================
// Timing
// ===========================================================================================

GBA_TEST(Ppu, ScanlineTimingOneFrame)
{
	GbaBus bus;
	SetupDisplay(bus);

	bus.ppu.ResetFrameCounter();

	// Enable the VBlank and HBlank interrupts (DISPSTAT bits 3 and 4).
	WriteReg(bus, DISPSTAT, STAT_VBLANK_IRQ | STAT_HBLANK_IRQ);

	// One frame is 228 lines of 1232 cycles (GBATEK "LCD Dimensions and Timings"). The system
	// clock is what advances the LCD in the assembled emulator, so the test drives GbaBus::Tick
	// (which forwards to Ppu::Tick) rather than the PPU directly.
	bus.Tick(CyclesPerFrame);

	GBA_CHECK_EQ(bus.ppu.FrameCounter(), 1);
	GBA_CHECK_HEX16(bus.ppu.VCount(), 0);

	// The counters are back at the top of a frame: line 0, HBlank low, VBlank cleared. The
	// V-Counter flag is set because DISPSTAT's setting is still 0.
	GBA_CHECK((bus.ppu.DispStat() & STAT_VBLANK) == 0);
	GBA_CHECK((bus.ppu.DispStat() & STAT_HBLANK) == 0);
	GBA_CHECK((bus.ppu.DispStat() & STAT_VCOUNT) != 0);

	// Both edges fired: VBlank exactly once, when line 160 started, and HBlank for the visible
	// lines (the tests only observe that the bit is up; the count is checked below).
	GBA_CHECK_EQ(bus.irq.ReadIF() & INT_VBLANK, INT_VBLANK);
	GBA_CHECK_EQ(bus.irq.ReadIF() & INT_HBLANK, INT_HBLANK);
	GBA_CHECK_EQ(bus.irq.ReadIF() & INT_VCOUNT, 0);

	// The second frame keeps the cadence.
	bus.Tick(CyclesPerFrame);
	GBA_CHECK_EQ(bus.ppu.FrameCounter(), 2);

	// Half a line is not enough to complete a line.
	bus.ppu.ResetFrameCounter();
	bus.Tick(CyclesPerScanline / 2);
	GBA_CHECK_EQ(bus.ppu.FrameCounter(), 0);
	bus.Tick(CyclesPerScanline / 2);
	GBA_CHECK_EQ(bus.ppu.FrameCounter(), 0);
	GBA_CHECK_HEX16(bus.ppu.VCount(), 1);
}

GBA_TEST(Ppu, HBlankInterruptOncePerVisibleLine)
{
	GbaBus bus;
	SetupDisplay(bus);

	// The line counter is observed through DISPSTAT: the PPU raises the interrupt at the start of
	// every visible line's HBlank, i.e. 160 times in a frame, and never during VBlank
	// (GBATEK "LCD Dimensions and Timings": "no H-Blank interrupts are generated within V-Blank").
	bus.ppu.ResetFrameCounter();
	WriteReg(bus, DISPSTAT, STAT_HBLANK_IRQ);

	int hblankLines = 0;

	for (int line = 0; line < ScanlinesTotal; line++)
	{
		bus.irq.WriteIF(0x3FFF);
		bus.Tick(CyclesPerScanline);

		if ((bus.irq.ReadIF() & INT_HBLANK) != 0)
			hblankLines++;
	}

	GBA_CHECK_EQ(hblankLines, ScreenHeight);
	GBA_CHECK_EQ(bus.ppu.FrameCounter(), 1);
}

GBA_TEST(Ppu, VBlankInterruptOncePerFrame)
{
	GbaBus bus;
	SetupDisplay(bus);

	// INT_VBLANK is raised exactly once, when the counter enters line 160 (GBATEK 4000004h).
	bus.ppu.ResetFrameCounter();
	WriteReg(bus, DISPSTAT, STAT_VBLANK_IRQ);

	int vblankCount = 0;

	for (int line = 0; line < ScanlinesTotal; line++)
	{
		bus.irq.WriteIF(0x3FFF);
		bus.Tick(CyclesPerScanline);

		if ((bus.irq.ReadIF() & INT_VBLANK) != 0)
			vblankCount++;
	}

	GBA_CHECK_EQ(vblankCount, 1);

	// The V-Blank flag is set in the lines 160..226 and cleared for line 227.
	bus.Tick(CyclesPerScanline * 160);		// back into line 160
	GBA_CHECK((bus.ppu.DispStat() & STAT_VBLANK) != 0);
	bus.Tick(CyclesPerScanline * 66);		// line 226
	GBA_CHECK((bus.ppu.DispStat() & STAT_VBLANK) != 0);
	bus.Tick(CyclesPerScanline);			// line 227: cleared
	GBA_CHECK((bus.ppu.DispStat() & STAT_VBLANK) == 0);
}

GBA_TEST(Ppu, VCountMatchInterrupt)
{
	GbaBus bus;
	SetupDisplay(bus);

	// The V-Count setting is the high byte of DISPSTAT and the IRQ enable is bit 5
	// (GBATEK 4000004h).
	const int matchLine = 42;
	WriteReg(bus, DISPSTAT, (u16)((matchLine << 8) | STAT_VCOUNT_IRQ));

	// Run the system clock forward line by line and count how often the match interrupt appears.
	int matches = 0;

	for (int line = 0; line < 50; line++)
	{
		bus.irq.WriteIF(0x3FFF);
		bus.Tick(CyclesPerScanline);

		if ((bus.irq.ReadIF() & INT_VCOUNT) != 0)
			matches++;
	}

	GBA_CHECK_HEX16(bus.ppu.VCount(), 50);
	GBA_CHECK_EQ(matches, 1);				// only when the counter passed line 42

	// The flag is set exactly while VCOUNT equals the setting: step through the match line and
	// record the stored flag on it and on the line after it (GBATEK 4000004h).
	int flagSetAtMatch = 0;
	int flagSetAfterMatch = 0;

	for (int line = 0; line < matchLine + 3; line++)
	{
		bus.Tick(CyclesPerScanline);

		if (bus.ppu.VCount() == (u16)matchLine)
			flagSetAtMatch = (bus.ppu.DispStat() & STAT_VCOUNT) != 0 ? 1 : 0;
		else if (bus.ppu.VCount() == (u16)(matchLine + 1))
			flagSetAfterMatch = (bus.ppu.DispStat() & STAT_VCOUNT) != 0 ? 1 : 0;
	}

	GBA_CHECK_EQ(flagSetAfterMatch, 0);
	GBA_CHECK_EQ(flagSetAtMatch, 1);

	// The VCOUNT register itself reports the line the renderer is on.
	bus.ppu.RenderLine(bus, 7);
	GBA_CHECK_HEX16(bus.ppu.VCount(), 7);
	GBA_CHECK((bus.ppu.DispStat() & STAT_VCOUNT) == 0);

	// Without the enable bit no interrupt is raised, but the flag still appears.
	bus.irq.WriteIF(0x3FFF);
	WriteReg(bus, DISPSTAT, (u16)(matchLine << 8));
	bus.ppu.RenderLine(bus, matchLine);
	GBA_CHECK((bus.ppu.DispStat() & STAT_VCOUNT) != 0);
	GBA_CHECK_EQ(bus.irq.ReadIF() & INT_VCOUNT, 0);

	// A write to DISPSTAT while VCOUNT already equals the new setting sets the flag and raises
	// the interrupt at once.
	WriteReg(bus, DISPSTAT, (u16)((matchLine << 8) | STAT_VCOUNT_IRQ));
	GBA_CHECK((bus.ppu.DispStat() & STAT_VCOUNT) != 0);
	GBA_CHECK_EQ(bus.irq.ReadIF() & INT_VCOUNT, INT_VCOUNT);
}
