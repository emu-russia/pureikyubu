// GBA LCD controller (the "PPU").
//
// Implemented from GBATEK "LCD" and "LCD VRAM Bitmap BG" / "LCD OBJ" / "LCD Color Palettes":
//
//   * DISPCNT/DISPSTAT/VCOUNT and the BG, window, mosaic and blending registers
//     (0x04000000 .. 0x0400004C);
//   * the tile modes 0, 1 and 2 (four text/affine backgrounds);
//   * the bitmap modes 3 (16-bit), 4 (8-bit, double buffered) and 5 (16-bit, 160x128);
//   * sprites: normal, semi-transparent, windowed, affine and affine double-size, with the
//     OBJ window;
//   * the mosaic filter for backgrounds and sprites;
//   * the window regions (WIN0, WIN1, WINOBJ) and the special effects (alpha blending,
//     brighten, darken), including the "first target" alpha rule of the sprite layer;
//   * the scanline timing: 1232 cycles per line, 160 visible lines, 68 VBlank lines, the
//     HBlank/VBlank/VCount interrupts and the forced-blank white screen.
//
// The output is a 240x160 XRGB8888 frame buffer for the host, plus the raw 15-bit colour of
// every pixel of the last rendered line, which is what the unit tests compare against their own
// reference renderer.

#pragma once

#include "gba_types.h"

namespace GBA
{
	class GbaBus;

	class Ppu
	{
	public:
		void Reset();

		// -- CPU side ----------------------------------------------------------------------

		/// <summary>Read a display register (offset relative to 0x04000000, 0x000..0x05F).</summary>
		u16 Read16(u32 offset, u16 openBus) const;

		/// <summary>Write a display register. `cycles` is the current system cycle counter, which
		/// the affine reference point registers latch when they are written twice.</summary>
		void Write16(GbaBus& bus, u32 offset, u16 value, int cycles);

		/// <summary>Palette memory (0x05000000, 1 KByte). byte access reads back the OR of the two
		/// halves of the halfword, as the hardware does.</summary>
		u8 ReadPalette(u32 offset) const;
		void WritePalette(u32 offset, u8 value);

		/// <summary>
		/// A whole palette entry. The bus uses these for 16-bit accesses: a halfword assembled from
		/// two byte reads would be wrong, because a byte read has the OR rule above (reading the
		/// entry 0x1234 as two bytes gives 0x3636).
		/// </summary>
		u16 ReadPalette16(u32 offset) const { return palette.Read16(offset & (PaletteSize - 1)); }
		void WritePalette16(u32 offset, u16 value) { palette.Write16(offset & (PaletteSize - 1), value); }

		/// <summary>Video memory (0x06000000, 96 KByte).</summary>
		u8 ReadVram(u32 offset) const;
		void WriteVram(u32 offset, u8 value);

		/// <summary>Object attribute memory (0x07000000, 1 KByte).</summary>
		u8 ReadOam(u32 offset) const;

		/// <summary>An 8-bit store of OAM. The hardware only has 16-bit and 32-bit write access
		/// to OAM (GBATEK "GBA Memory Map"), so this only drives the low byte of the halfword the
		/// address names and leaves the high byte alone.</summary>
		void WriteOam(u32 offset, u8 value);

		/// <summary>A whole halfword of OAM, i.e. what a 16-bit store drives. The bus uses this:
		/// assembling the halfword out of two WriteOam calls would drop its high byte.</summary>
		void WriteOam16(u32 offset, u16 value)
		{
			const u32 index = offset & (OamSize - 1);
			oam.Write8(index, (u8)value);
			oam.Write8((index + 1) & (OamSize - 1), (u8)(value >> 8));
		}

		// -- timing ------------------------------------------------------------------------

		/// <summary>Advance the LCD by `cycles`. Renders a scanline when one is drawn and raises
		/// the HBlank/VBlank/VCount interrupts through the bus's interrupt controller.</summary>
		void Tick(GbaBus& bus, int cycles);

		/// <summary>Render one scanline into the line buffer and the frame buffer.</summary>
		void RenderLine(GbaBus& bus, int y);

		// -- observation -------------------------------------------------------------------

		/// <summary>The frame as XRGB8888, ScreenWidth * ScreenHeight pixels, row 0 on top.</summary>
		const u32* Frame() const { return frame.data(); }

		/// <summary>The frame as the host expects it (BGRA byte order in memory, as SDL's
		/// SDL_PIXELFORMAT_ARGB8888 wants it on a little-endian host).</summary>
		const u32* FramePixels() const { return frame.data(); }

		/// <summary>The raw 15-bit colour of a pixel of the most recently rendered line.</summary>
		u16 LinePixel(int x) const { return (x >= 0 && x < ScreenWidth) ? line[x] : 0; }

		/// <summary>How many frames have been completed since the reset.</summary>
		int FrameCounter() const { return frameCounter; }

		u16 VCount() const { return vcount; }
		/// <summary>The DISPSTAT value the CPU would read right now, i.e. the stored IRQ
		/// enables and V-Count setting plus the flags of the current line position.</summary>
		u16 DispStat() const { return ComputeDispStat(); }
		u16 DispCnt() const { return dispcnt; }

		/// <summary>True while the LCD is inside the forced-blank window (DISPCNT bit 7).</summary>
		bool ForcedBlank() const { return (dispcnt & 0x80) != 0; }

		/// <summary>Reset the frame counter (the tests use it to check the frame cadence).</summary>
		void ResetFrameCounter() { frameCounter = 0; }

	private:
		// -- registers ---------------------------------------------------------------------

		u16 dispcnt = 0;			// 0x000
		u16 greenswap = 0;			// 0x002
		u16 bldcnt = 0;				// 0x004
		u16 bldalpha = 0;			// 0x006
		u16 bldy = 0;				// 0x008
		u16 bgcnt[4]{};				// 0x00A .. 0x010 (offset 0x08 is BLDY, so the array is not
									// contiguous in memory; the accessors decode the offsets)
		u16 bghofs[4]{};
		u16 bgvofs[4]{};
		s16 bgpa[2]{}, bgpb[2]{}, bgpc[2]{}, bgpd[2]{};
		s32 bgx[2]{}, bgy[2]{};
		u32 bgxLatch[2]{}, bgyLatch[2]{};
		u16 win0h = 0, win1h = 0, win0v = 0, win1v = 0;
		u16 winin = 0, winout = 0;
		u16 mosaic = 0;

		u16 dispstat = 0;			// 0x004 of DISPSTAT, the read-only bits are kept here
		u16 vcount = 0;

		// -- memory ------------------------------------------------------------------------

		MemoryBank palette;			// 0x05000000
		MemoryBank vram;			// 0x06000000
		MemoryBank oam;				// 0x07000000

		// -- timing ------------------------------------------------------------------------

		int lineCycles = 0;
		int frameCounter = 0;

		// -- rendering ---------------------------------------------------------------------

		u16 line[ScreenWidth]{};		// the 15-bit colours of the current line
		u8 windowMask[ScreenWidth]{};	// 0..5 = the window that won, 0xFF = outside all windows
		std::vector<u32> frame;		// XRGB8888

		// Per-pixel layer information, needed by the blending pass: which priority layer won and
		// whether it is a "first target" (BG0-BG3, OBJ) or a "second target" (the backdrop).
		struct LayerPixel
		{
			u16 color = 0;
			u8 layer = 0xFF;		// 0..3 = BG0..BG3, 4 = OBJ, 5 = backdrop, 0xFF = transparent
			u8 priority = 0xFF;
			bool semiTransparent = false;	// an OBJ pixel with the semi-transparent bit
		};
		LayerPixel pixels[ScreenWidth]{};

		// The scanline renderers.
		void RenderBackdrop();
		void RenderTextBg(int index);
		void RenderAffineBg(int index);
		void RenderBitmap();
		void RenderSprites(GbaBus& bus);
		void ApplyWindows();
		void ApplyBlending();
		void RenderMosaic();			// (the mosaic is applied while sampling, see RenderTextBg)

		// Address helpers.
		u32 VramAddress(u32 offset) const;
		bool TextBgIs256(int index) const;

		// The scanline the renderer is currently working on (set by Tick).
		int currentLine = 0;

		// The line counter and the V-Counter match, both driven by Tick. They only touch private
		// state, so they are not part of the interface the rest of the emulator uses.
		void AdvanceVCount(GbaBus& bus);
		void UpdateVCountMatch(GbaBus* bus);

		/// <summary>The DISPSTAT value to hand to the CPU: the stored IRQ enables and V-Count
		/// setting plus the read-only flags of the current line (GBATEK 4000004h).</summary>
		u16 ComputeDispStat() const;

		// -- the per-line priority stack ---------------------------------------------------
		//
		// The layers are painted into `pixels` (the best priority number wins) and the pixel each
		// layer covered is kept in a `LayerBelow`, which is the 2nd target of the alpha blending
		// pass. Everything below is used by gba_ppu.cpp only; it is private because it speaks
		// about LayerPixel and about the per-line register state, both of which are private.

		/// <summary>The pixel directly below the winning layer of one dot.</summary>
		struct LayerBelow
		{
			u16 color = 0;
			u8 layer = 0xFF;		// 0..3 = BG0..BG3, 4 = OBJ, 5 = backdrop, 0xFF = none
			u8 priority = 0xFF;
			bool semitransparent = false;
		};

		/// <summary>The register state of the scanline being rendered, gathered once by
		/// RenderLine so that the layer renderers do not have to re-read it per dot.</summary>
		struct LineState
		{
			int line = 0;					// the scanline being rendered
			u16 dispcnt = 0;
			u16 mosaic = 0;
			u16 winin = 0;
			u16 winout = 0;
			u16 bldcnt = 0;
			u16 bldalpha = 0;
			u16 bldy = 0;
			u8 mode = 0;
			bool forcedBlank = false;
			bool windowsActive = false;		// at least one of DISPCNT.13/14/15 enables a window
			const u8* windowMask = nullptr;	// the window region of every dot
		};

		/// <summary>Insert an opaque dot at `priority` (the better, i.e. lower, number wins).</summary>
		void PushDot(LayerPixel& winner, LayerBelow& below, int priority, u8 layer, u16 color,
			bool semitransparent);

		/// <summary>Draw one background layer over the whole line.</summary>
		void DrawBackgroundLayer(const LineState& state, int index, LayerBelow* below);

		/// <summary>Apply the colour special effect of one dot and store it into `line`.</summary>
		void ApplyDotEffect(const LineState& state, int x, const LayerBelow& below);

		/// <summary>Fetch one dot of a text background (with the scroll offsets applied).</summary>
		bool TextBgDot(int index, int sourceX, int sourceY, u16& color) const;

		/// <summary>Fetch one dot of a rotation/scaling background, or of the bitmap background
		/// of the modes 3-5; `sourceX`/`sourceY` are screen coordinates.</summary>
		bool AffineBgDot(const LineState& state, int index, int sourceX, int sourceY, u16& color) const;
	};
}
