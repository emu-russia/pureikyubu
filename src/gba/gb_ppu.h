// The Game Boy LCD controller (the PPU): the background, the window, the sprites, the DMG
// palettes and the CGB colour palettes with their VRAM attribute map.
//
// Written from the Pan Docs (gbdev.io/pandocs): "LCDC" (0xFF40), "STAT" (0xFF41), "Rendering"
// (the mode timings and the mode 3 penalty algorithm), "Scrolling", "Tile Data", "Tile Maps"
// (including the CGB attribute map and the three-flag BG-to-OBJ priority table), "OAM",
// "Palettes", "Interrupt Sources" (the STAT interrupt line and STAT blocking) and
// "Accessing VRAM and OAM".
//
// Deviations from the dot exact hardware, all deliberate and written down:
//
//  * **Line-at-a-time composition.** The background, the window and the sprites of a scanline are
//    composed when mode 3 ends, instead of being pushed through the two 8-pixel FIFOs dot by dot.
//    A program that rewrites VRAM, SCX/SCY or a palette *inside* the visible part of a line
//    therefore sees the change earlier than on hardware. This is the same simplification the GBA
//    side of the emulator makes and it is what the unit tests check.
//  * **Mode 3 length.** 172 dots (160 pixels + the 12 dots of the two initial fetches) plus the
//    documented penalties for the window and the objects, clamped to the documented maximum of
//    289. The fine grained per-object accounting of "Rendering.html" is not modelled, so a
//    program that counts dots inside mode 3 sees a slightly different number.
//  * **The monochrome-only quirks** that Pan Docs describes (the WX = 166 line-down bug, the
//    colour-0 pixel inserted when the window is disabled exactly on a tile boundary, the
//    DMG spurious STAT interrupt on a STAT write, the dropped leftmost sprite pixel) are *not*
//    modelled; each one is called out in the code where it would apply.

#pragma once

#include "gba_types.h"

#include <vector>

namespace GBA
{
	// ---------------------------------------------------------------------------------------
	// The geometry (Pan Docs "Rendering" / "Specifications"): 456 dots per line, 154 lines.
	// ---------------------------------------------------------------------------------------

	const int GbScreenWidth = 160;
	const int GbScreenHeight = 144;
	const int GbDotsPerLine = 456;
	const int GbLinesPerFrame = 154;
	const int GbVisibleLines = 144;
	const int GbDotsPerFrame = GbDotsPerLine * GbLinesPerFrame;		// 70224
	const int GbCyclesPerSecond = 4194304;							// 2^22 Hz

	/// <summary>The shades the monochrome (DMG) picture is drawn with.</summary>
	enum class GbPalette : int
	{
		Green = 0,			// the original DMG's green LCD
		Grey,				// the Game Boy Pocket / Light
		Amber,				// the "amber" front-lit look
		Blue,				// a cool blue-on-black scheme
	};

	class GbPpu
	{
	public:
		// -- configuration -------------------------------------------------------------------

		/// <summary>Choose the shades the DMG (non-CGB) picture is drawn with.</summary>
		void SetPalette(GbPalette palette) { shadePalette = palette; }
		GbPalette Palette() const { return shadePalette; }

		/// <summary>The palette the frontend may cycle through.</summary>
		static const char* PaletteName(GbPalette palette);

		/// <summary>The console kind: a CGB has the colour palettes and the extra registers.</summary>
		void SetCgb(bool value) { cgb = value; }
		bool Cgb() const { return cgb; }

		void Reset();

		// -- the CPU's view of the register file ---------------------------------------------

		u8 ReadRegister(u16 address) const;
		void WriteRegister(u16 address, u8 value);

		/// <summary>True when the PPU owns VRAM or OAM right now (Pan Docs "Accessing VRAM and
		/// OAM"): VRAM is blocked in mode 3, OAM in modes 2 and 3, CRAM in mode 3.</summary>
		bool VramBlocked() const { return lcdEnabled && mode == 3; }
		bool OamBlocked() const { return lcdEnabled && (mode == 2 || mode == 3); }
		bool CramBlocked() const { return lcdEnabled && mode == 3; }

		/// <summary>The two 8 KByte VRAM banks, the 160 bytes of OAM and the palette memory, for
		/// the bus (which serves the CPU's accesses to them).</summary>
		u8* VramBank(int bank) { return vram[bank & 1]; }
		const u8* VramBank(int bank) const { return vram[bank & 1]; }
		u8* Oam() { return oam; }
		const u8* Oam() const { return oam; }
		u8* PaletteRam(bool object) { return object ? objPalette : bgPalette; }
		const u8* PaletteRam(bool object) const { return object ? objPalette : bgPalette; }

		// -- running -------------------------------------------------------------------------

		/// <summary>
		/// Advance the LCD by `cycles` of the 4.194304 MHz clock (four cycles per dot in normal
		/// speed, two in CGB double speed). Returns the interrupt bits the LCD requested: 0x01 for
		/// VBlank, 0x02 for the STAT interrupt. The caller ORs them into IF.
		/// </summary>
		u8 Tick(int cycles, bool doubleSpeed);

		/// <summary>The composed frame, XRGB8888 (0xAARRGGBB), 160 x 144 pixels.</summary>
		const u32* Frame() const { return frame.data(); }

		/// <summary>How many frames the LCD has finished (the harness uses it to skip a boot).</summary>
		int FrameCounter() const { return frameCounter; }

		/// <summary>The dots the LCD has run since Reset (a test can check the timing with it).</summary>
		u64 Dots() const { return dots; }

		int Mode() const { return lcdEnabled ? mode : 0; }
		int Ly() const { return lcdEnabled ? (int)ly : 0; }
		u8 Stat() const { return stat; }
		u8 Lcdc() const { return lcdc; }
		bool LcdEnabled() const { return lcdEnabled; }

		/// <summary>Turn the LCD on or off without a full LCDC write (the machine uses it to put
		/// the LCD in its power-up state before the boot ROM writes LCDC itself).</summary>
		void SetLcdEnabled(bool enabled);

		/// <summary>
		/// Fill both colour palette banks with a grey ramp: palette 0 (and the object palettes
		/// 0 and 1) get white, light grey, dark grey and black, the other palettes the same four
		/// shades. The real CGB boot ROM derives these from BGP/OBP0/OBP1 (the "compatibility
		/// palettes", chosen with a table indexed by the cartridge's title); this emulator uses
		/// one fixed ramp, which is what makes a monochrome cartridge - and this boot ROM's own
		/// wordmark - visible on a CGB. A colour cartridge writes its own palettes at startup.
		/// </summary>
		void SetGreyscalePalettes();

		/// <summary>The window's own line counter (Pan Docs "Tile Maps": the window does not use
		/// SCY; it has a counter that only advances when the window actually renders).</summary>
		int WindowLine() const { return windowLine; }
		bool WindowActive() const { return windowActive; }

		/// <summary>The sprites the OAM scan picked for the line being rendered (Pan Docs "OAM":
		/// the first ten whose Y matches, in OAM order).</summary>
		int LineSpriteCount() const { return lineSpriteCount; }

		/// <summary>The OAM index of the n-th sprite of the current line.</summary>
		int LineSpriteIndex(int n) const;

		/// <summary>How long mode 3 lasted on the last line, in dots (the unit tests check the
		/// documented 172..289 range and the penalties).</summary>
		int LastMode3Length() const { return lastMode3Length; }

		/// <summary>Recompute STAT's read-only bits and the interrupt line (a test can call it
		/// after a register write to see the effect).</summary>
		void RefreshStat();

	private:
		// -- registers (Pan Docs "LCDC" / "STAT" / "Scrolling") -------------------------------

		u8 lcdc = 0x91;			// the post-boot value (Pan Docs "Power Up Sequence")
		u8 stat = 0x85;
		u8 scy = 0x00, scx = 0x00;
		u8 ly = 0x00, lyc = 0x00;
		u8 wy = 0x00, wx = 0x00;		// the window's position (WX is the left edge plus 7)
		u8 bgp = 0xFC;			// the post-boot value
		u8 obp0 = 0xFF, obp1 = 0xFF;

		// The CGB registers (Pan Docs "Palettes" / "CGB Registers").
		u8 vbk = 0xFE;			// only bit 0 is writable; the upper bits read as ones
		u8 bgpi = 0x00, obpi = 0x00;
		u8 bgPalette[64]{};
		u8 objPalette[64]{};

		// -- memory --------------------------------------------------------------------------

		u8 vram[2][0x2000]{};
		u8 oam[0xA0]{};

		// -- timing --------------------------------------------------------------------------

		u64 dots = 0;			// dots since Reset
		int mode = 2;
		u64 modeEndDots = 80;	// the absolute dot count at which the current mode ends
		bool lcdEnabled = true;
		int frameCounter = 0;
		bool statLine = false;	// the STAT interrupt line's level (rising edge requests only)
		int lastMode3Length = 172;

		// -- the window ----------------------------------------------------------------------

		bool windowActive = false;	// the "Y condition": true once LY has reached WY
		int windowLine = 0;			// the window's own line counter (it never uses SCY)

		// -- the line being composed ---------------------------------------------------------

		u32 line[GbScreenWidth]{};
		std::vector<u32> frame;		// GbScreenWidth * GbScreenHeight

		struct LineSprite
		{
			int x = 0;				// the screen X of the leftmost pixel (OAM X - 8)
			int index = 0;			// its OAM index, for the priority rules
			int row = 0;			// the row inside the object's tile (before the Y flip)
			u8 tile = 0, attributes = 0;
		};

		LineSprite lineSprites[10];
		int lineSpriteCount = 0;

		// -- configuration -------------------------------------------------------------------

		bool cgb = false;
		GbPalette shadePalette = GbPalette::Green;

		// -- the scanline --------------------------------------------------------------------

		/// <summary>Start a visible line: the OAM scan (mode 2) and the window's Y condition.</summary>
		void BeginVisibleLine();

		/// <summary>The dot count the current scanline started at (a multiple of 456).</summary>
		u64 LineStart() const { return dots - (dots % GbDotsPerLine); }

		/// <summary>Pick the objects that cover this line (Pan Docs "OAM", the mode 2 scan).</summary>
		void ScanOam();

		/// <summary>The mode 3 duration in dots: 172 plus the window and object penalties.</summary>
		int Mode3Length();

		/// <summary>Compose one whole scanline into `line` (the deviation noted at the top).</summary>
		void RenderLine();

		/// <summary>One background/window pixel: its colour index, the palette to use and the
		/// attribute byte (CGB).</summary>
		u8 FetchBgPixel(int x, int y, int& palette, u8& attributes);

		/// <summary>Turn a colour index and a palette selector into the host pixel.</summary>
		u32 ShadePixel(int index, int palette, u8 attributes, bool object) const;

		/// <summary>One RGB555 CGB colour, expanded to XRGB8888.</summary>
		u32 CgbColor(int palette, int index, bool object) const;

		/// <summary>The DMG shade a colour index maps to under BGP/OBP0/OBP1.</summary>
		u32 DmgShade(int index, int palette, bool object) const;

		static u32 PackXrgb(int r, int g, int b) { return 0xFF000000u | ((u32)r << 16) | ((u32)g << 8) | (u32)b; }

		/// <summary>The level of the STAT interrupt line (Pan Docs "Interrupt Sources").</summary>
		bool StatLineLevel() const;

		/// <summary>
		/// Move to a mode. The STAT interrupt is requested only on a rising edge of the shared
		/// STAT line, which is what produces the documented "STAT blocking" behaviour.
		/// </summary>
		u8 EnterMode(int newMode);

		/// <summary>Re-evaluate the STAT interrupt line and return the request bit (0x02) when the
		/// line rose.</summary>
		u8 UpdateStatLine();
	};
}
