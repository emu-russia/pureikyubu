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
//    WX = 0 "stutter" where the window is switched in before the fine scroll adjustment, the
//    colour-0 pixel inserted when the window is disabled exactly on a tile boundary, the
//    DMG spurious STAT interrupt on a STAT write, the dropped leftmost sprite pixel) are *not*
//    modelled; each one is called out in the code where it would apply. The spurious STAT
//    interrupt is the one quirk that Pan Docs says the CGB in DMG mode does *not* have, so its
//    absence is exact on a CGB.
//
// The console kind and the cartridge's own mode are two different things, and this class carries
// both: `cgb` is the hardware (the colour palettes, the VRAM banks, the extra registers) and
// `dmgCompat` is a CGB running a monochrome cartridge in DMG compatibility mode. The manual's
// rules for that mode - LCDC bit 0 blanks the background and the window, the window bit 5 is
// overridden by it, the OBJ priority is the X coordinate rather than the OAM position, and the
// OBJ palette and bank bits are the DMG's - are applied when `dmgCompat` is set, while the
// picture still comes from the CGB palette memory (Pan Docs "Power Up Sequence": "the CGB
// palettes are still being used ... BGP, OBP0, and OBP1 actually index into the CGB palettes").

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

		/// <summary>
		/// A CGB running a monochrome cartridge in DMG compatibility mode (the cartridge's CGB
		/// flag is clear, so the real boot ROM switches the machine to the DMG rules). It only
		/// means anything when `cgb` is set; a plain DMG is in the mode by definition.
		/// </summary>
		void SetDmgCompat(bool value) { dmgCompat = value; }
		bool DmgCompat() const { return dmgCompat; }

		/// <summary>Use the DMG's OBJ selection priority (the X coordinate) rather than the CGB's
		/// OAM order. This is what OPRI (0xFF6C) selects on a CGB; Pan Docs "CGB Registers".</summary>
		void SetDmgObjectPriority(bool value) { dmgObjectPriority = value; }
		bool DmgObjectPriority() const { return dmgObjectPriority; }

		void Reset();

		// -- the CPU's view of the register file ---------------------------------------------

		uint8_t ReadRegister(uint16_t address) const;

		/// <summary>
		/// Write a register. Returns the interrupt bits the write requested: 0x02 when the STAT
		/// line rose (a STAT or LYC write can raise it, Pan Docs "STAT": LYC is compared
		/// "constantly" and the shared line is edge triggered), and no bits otherwise. The caller
		/// ORs them into IF, exactly as it does with Tick's.
		/// </summary>
		uint8_t WriteRegister(uint16_t address, uint8_t value);

		/// <summary>True when the PPU owns VRAM or OAM right now (Pan Docs "Accessing VRAM and
		/// OAM"): VRAM is blocked in mode 3, OAM in modes 2 and 3, CRAM in mode 3.</summary>
		bool VramBlocked() const { return lcdEnabled && mode == 3; }
		bool OamBlocked() const { return lcdEnabled && (mode == 2 || mode == 3); }
		bool CramBlocked() const { return lcdEnabled && mode == 3; }

		/// <summary>The two 8 KByte VRAM banks, the 160 bytes of OAM and the palette memory, for
		/// the bus (which serves the CPU's accesses to them).</summary>
		uint8_t* VramBank(int bank) { return vram[bank & 1]; }
		const uint8_t* VramBank(int bank) const { return vram[bank & 1]; }
		uint8_t* Oam() { return oam; }
		const uint8_t* Oam() const { return oam; }
		uint8_t* PaletteRam(bool object) { return object ? objPalette : bgPalette; }
		const uint8_t* PaletteRam(bool object) const { return object ? objPalette : bgPalette; }

		// -- running -------------------------------------------------------------------------

		/// <summary>
		/// Advance the LCD by `cycles` clocks of the machine's 4.194304 MHz clock. The dot clock
		/// is that same 4.194304 MHz clock on a DMG/CGB, so one clock is one dot, a line is 456 of
		/// them and a frame 70224, i.e. 16.74 ms. `doubleSpeed` is the CGB's CPU speed and does not
		/// touch the LCD. Returns the interrupt bits the LCD requested: 0x01 for VBlank, 0x02 for
		/// the STAT interrupt. The caller ORs them into IF.
		/// </summary>
		uint8_t Tick(int cycles, bool doubleSpeed);

		/// <summary>The composed frame, XRGB8888 (0xAARRGGBB), 160 x 144 pixels.</summary>
		const uint32_t* Frame() const { return frame.data(); }

		/// <summary>How many frames the LCD has finished (the harness uses it to skip a boot).</summary>
		int FrameCounter() const { return frameCounter; }

		/// <summary>The dots the LCD has run since Reset (a test can check the timing with it).</summary>
		uint64_t Dots() const { return dots; }

		int Mode() const { return lcdEnabled ? mode : 0; }
		int Ly() const { return lcdEnabled ? (int)ly : 0; }
		uint8_t Stat() const { return stat; }
		uint8_t Lcdc() const { return lcdc; }
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

		/// <summary>Recompute STAT's read-only bits and the interrupt line, returning the STAT
		/// request bit (0x02) when the line rose (a test can call it after a register write to see
		/// the effect).</summary>
		uint8_t RefreshStat();

	private:
		// -- registers (Pan Docs "LCDC" / "STAT" / "Scrolling") -------------------------------

		uint8_t lcdc = 0x91;			// the post-boot value (Pan Docs "Power Up Sequence")
		uint8_t stat = 0x85;
		uint8_t scy = 0x00, scx = 0x00;
		uint8_t ly = 0x00, lyc = 0x00;
		uint8_t wy = 0x00, wx = 0x00;		// the window's position (WX is the left edge plus 7)
		uint8_t bgp = 0xFC;			// the post-boot value
		uint8_t obp0 = 0xFF, obp1 = 0xFF;

		// The CGB registers (Pan Docs "Palettes" / "CGB Registers").
		uint8_t vbk = 0xFE;			// only bit 0 is writable; the upper bits read as ones
		uint8_t bgpi = 0x00, obpi = 0x00;
		uint8_t bgPalette[64]{};
		uint8_t objPalette[64]{};

		// -- memory --------------------------------------------------------------------------

		uint8_t vram[2][0x2000]{};
		uint8_t oam[0xA0]{};

		// -- timing --------------------------------------------------------------------------

		uint64_t dots = 0;			// dots since Reset
		int mode = 2;
		uint64_t modeEndDots = 80;	// the absolute dot count at which the current mode ends
		bool lcdEnabled = true;
		int frameCounter = 0;
		bool statLine = false;	// the STAT interrupt line's level (rising edge requests only)
		int lastMode3Length = 172;

		// -- the window ----------------------------------------------------------------------

		bool windowActive = false;	// the "Y condition": true once LY has reached WY
		int windowLine = 0;			// the window's own line counter (it never uses SCY)

		// -- the line being composed ---------------------------------------------------------

		uint32_t line[GbScreenWidth]{};
		std::vector<uint32_t> frame;		// GbScreenWidth * GbScreenHeight

		struct LineSprite
		{
			int x = 0;				// the screen X of the leftmost pixel (OAM X - 8)
			int index = 0;			// its OAM index, for the priority rules
			int row = 0;			// the row inside the object's tile (before the Y flip)
			uint8_t tile = 0, attributes = 0;
		};

		LineSprite lineSprites[10];
		int lineSpriteCount = 0;

		// -- configuration -------------------------------------------------------------------

		bool cgb = false;
		bool dmgCompat = false;			// a CGB running a monochrome cartridge
		bool dmgObjectPriority = false;	// OPRI (0xFF6C): the DMG's X order, not the OAM order
		GbPalette shadePalette = GbPalette::Green;

		/// <summary>True when the DMG's display rules apply: a monochrome console, or a CGB in
		/// DMG compatibility mode (the manual's "DMG or CGB in DMG mode").</summary>
		bool DmgRules() const { return !cgb || dmgCompat; }

		// -- the scanline --------------------------------------------------------------------

		/// <summary>
		/// Start a visible line: the OAM scan (mode 2) and the window's Y condition. Returns the
		/// interrupt bits the mode 2 entry requested (0x02 when the STAT line rose), which the
		/// caller has to pass on - the mode 2 source and an LYC that matches the new LY are both
		/// detected here, and dropping the return value loses every line start's STAT interrupt.
		/// </summary>
		uint8_t BeginVisibleLine();

		/// <summary>The blank (white) picture a disabled LCD shows (Pan Docs "LCDC" bit 7: "the
		/// screen is blank, which on DMG is displayed as a white 'whiter' than color #0").</summary>
		void BlankFrame();

		/// <summary>The dot count the current scanline started at (a multiple of 456).</summary>
		uint64_t LineStart() const { return dots - (dots % GbDotsPerLine); }

		/// <summary>Pick the objects that cover this line (Pan Docs "OAM", the mode 2 scan).</summary>
		void ScanOam();

		/// <summary>The mode 3 duration in dots: 172 plus the window and object penalties.</summary>
		int Mode3Length();

		/// <summary>Compose one whole scanline into `line` (the deviation noted at the top).</summary>
		void RenderLine();

		/// <summary>One background/window pixel: its colour index, the palette to use and the
		/// attribute byte (CGB).</summary>
		uint8_t FetchBgPixel(int x, int y, int& palette, uint8_t& attributes);

		/// <summary>Turn a colour index and a palette selector into the host pixel.</summary>
		uint32_t ShadePixel(int index, int palette, uint8_t attributes, bool object) const;

		/// <summary>One RGB555 CGB colour, expanded to XRGB8888.</summary>
		uint32_t CgbColor(int palette, int index, bool object) const;

		/// <summary>The DMG shade a colour index maps to under BGP/OBP0/OBP1.</summary>
		uint32_t DmgShade(int index, int palette, bool object) const;

		static uint32_t PackXrgb(int r, int g, int b) { return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b; }

		/// <summary>The level of the STAT interrupt line (Pan Docs "Interrupt Sources").</summary>
		bool StatLineLevel() const;

		/// <summary>
		/// Move to a mode. The STAT interrupt is requested only on a rising edge of the shared
		/// STAT line, which is what produces the documented "STAT blocking" behaviour.
		/// </summary>
		uint8_t EnterMode(int newMode);

		/// <summary>Re-evaluate the STAT interrupt line and return the request bit (0x02) when the
		/// line rose.</summary>
		uint8_t UpdateStatLine();
	};
}
