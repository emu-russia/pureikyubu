// GBA LCD controller (the "PPU"): the tile and bitmap backgrounds, the sprites, the window
// regions, the mosaic filter, the colour special effects and the scanline timing.
//
// Everything here is written from GBATEK (problemkaputt.de/gbatek.htm), sections "LCD",
// "LCD VRAM Bitmap BG", "LCD OBJ", "LCD Color Palettes" and "LCD Dimensions and Timings". The
// specification rule each block implements is named in the comment above it.
//
// The file uses the public interface of the frozen header (gba_ppu.h) and nothing else: the
// renderers are file-scope helpers that read the register state through Read16()/ReadPalette()/
// ReadVram()/ReadOam() and the observation accessors, and RenderLine() drives them.
//
// How a scanline is composed:
//
//   1. RenderSprites() decodes the 128 OAM entries and, per dot, picks the winning sprite twice:
//      the top-most one to display (OAM index first, OBJ/BG priority second) and the top-most
//      one with the "OBJ Window" mode, which is not drawn but marks the OBJ window region
//      (GBATEK "The OBJ Window").
//   2. ApplyWindows() turns WIN0/WIN1/WINOBJ/WINOUT into a region tag per dot, with the
//      documented priority "window 0 over window 1 over the OBJ window over the outside area".
//   3. The layers are painted into pixels[]: the backdrop first, then the sprite layer, then the
//      four backgrounds, each dot keeping only the best (lowest) priority number seen so far, so
//      pixels[x] ends up holding the top-most pixel. While a background or the sprite is painted,
//      the previous winner of the dot is copied to a `below` array: that is the pixel directly
//      underneath it, which the alpha blending pass needs as its 2nd target (GBATEK 4000052h:
//      "the top-most non-transparent pixel must be selected as 1st Target, and the next-lower
//      non-transparent pixel must be selected as 2nd Target"). The backdrop is opaque everywhere,
//      so every dot has a second target unless the backdrop itself is the top-most pixel.
//   4. ApplyBlending() applies the colour special effect to the top-most pixel, using the pixel
//      below it, and writes the result into line[].
//   5. The 15-bit colour is expanded to XRGB8888 into `frame`, with the Green Swap bit applied.
//
// The scanline timing (GBATEK "LCD Dimensions and Timings"): 240 visible dots = 960 cycles and
// 68 blank dots = 272 cycles make 1232 cycles per line; 160 visible lines and 68 VBlank lines.

#include "gba_ppu.h"
#include "gba_bus.h"

namespace GBA
{
	namespace
	{
		// -----------------------------------------------------------------------------------
		// Register offsets (relative to 0x04000000).
		// -----------------------------------------------------------------------------------

		const u32 REG_DISPCNT = 0x000;
		const u32 REG_GREENSWP = 0x002;
		const u32 REG_DISPSTAT = 0x004;
		const u32 REG_VCOUNT = 0x006;
		const u32 REG_WININ = 0x048;
		const u32 REG_MOSAIC = 0x04C;
		const u32 REG_BLDCNT = 0x050;

		// DISPSTAT: bits 0-2 are the read-only flags, bits 3-5 the IRQ enables and bits 8-15 the
		// V-Count setting (GBATEK 4000004h).
		const u16 STAT_VBLANK = 0x0001;
		const u16 STAT_HBLANK = 0x0002;
		const u16 STAT_VCOUNT = 0x0004;
		const u16 STAT_ENABLES = 0x0038;

		// DISPCNT bits (GBATEK 4000000h).
		const int DC_FRAME_SELECT = 4;			// BG modes 4/5: which of the two frames is shown
		const int DC_OBJ_1D = 6;				// OBJ character VRAM mapping
		const int DC_FORCED_BLANK = 7;
		const int DC_BG_ENABLE = 8;				// bits 8..11 = BG0..BG3
		const int DC_OBJ_ENABLE = 12;
		const int DC_WIN0_ENABLE = 13;
		const int DC_WIN1_ENABLE = 14;
		const int DC_OBJ_WIN_ENABLE = 15;

		// BGxCNT bits (GBATEK 4000008h).
		const int BGCNT_PRIORITY = 0;			// 2 bits
		const int BGCNT_CHAR_BASE = 2;			// 2 bits, units of 16 KByte
		const int BGCNT_MOSAIC = 6;
		const int BGCNT_256_COLORS = 7;
		const int BGCNT_SCREEN_BASE = 8;		// 5 bits, units of 2 KByte
		const int BGCNT_AREA_OVERFLOW = 13;		// BG2/BG3 only: 1 = wrap around the BG area
		const int BGCNT_SIZE = 14;				// 2 bits

		// WININ/WINOUT (GBATEK 4000048h/400004Ah): BG0-3 in bits 0-3, OBJ in bit 4 and the colour
		// special effect in bit 5. WININ holds window 0 then window 1; WINOUT holds the outside
		// area then the OBJ window.
		const int WIN_EFFECT = 5;
		const int WIN_SECOND = 8;

		// BLDCNT (GBATEK 4000050h): the 1st targets in bits 0-5 (BG0-3, OBJ, backdrop), the
		// effect in bits 6-7 and the 2nd targets in bits 8-13.
		const int BLD_EFFECT = 6;
		const int BLD_2ND = 8;

		// The layer tags: the backgrounds are 0..3 (their bit position in WININ/WINOUT/BLDCNT),
		// the sprites are 4 (the same) and the backdrop is 5 (the BG palette colour 0).
		const u8 LAYER_OBJ = 4;
		const u8 LAYER_BACKDROP = 5;
		const u8 LAYER_NONE = 0xFF;

		// The window region tags stored in Ppu::windowMask.
		const u8 MASK_OUTSIDE = 0;
		const u8 MASK_WIN0 = 1;
		const u8 MASK_WIN1 = 2;
		const u8 MASK_OBJWIN = 3;

		const int TILE_SIZE_4BPP = 32;			// GBATEK: one 4bit tile is 20h bytes
		const int TILE_SIZE_8BPP = 64;			// GBATEK: one 8bit tile is 40h bytes

		const int PALETTE_OBJ_OFFSET = 0x200;	// 05000200: the 256 OBJ colours

		// The OBJ tile area inside the 96 KByte VRAM window (GBATEK "LCD VRAM Overview").
		const u32 OBJ_TILE_BASE = 0x10000;
		const u32 BITMAP_OBJ_TILES = 0x14000;	// the same area in the bitmap modes
		const u32 BITMAP_FRAME1 = 0x0A000;

		const int BITMAP_STRIDE_16 = 480;		// modes 3/5: two bytes per dot
		const int BITMAP_STRIDE_8 = 240;		// mode 4: one byte per dot
		const int MODE3_HEIGHT = 160;
		const int MODE4_HEIGHT = 160;
		const int MODE5_WIDTH = 160;
		const int MODE5_HEIGHT = 128;

		// The 15-bit "no pixel here" marker of the sprite and bitmap fetches.
		const u16 NO_PIXEL = 0xFFFF;

		// Up to 128 OBJs exist, so a line cannot be covered by more.
		const int MAX_SPRITES = 128;

		// -----------------------------------------------------------------------------------
		// The OBJ shapes and sizes (GBATEK "OBJ Attribute 1": size 0..3 per shape).
		// -----------------------------------------------------------------------------------

		struct ObjDimensions
		{
			int width;
			int height;
		};

		const ObjDimensions ObjSizeTable[3][4] =
		{
			// Square:     8x8, 16x16, 32x32, 64x64
			{ { 8, 8 }, { 16, 16 }, { 32, 32 }, { 64, 64 } },
			// Horizontal: 16x8, 32x8, 32x16, 64x32
			{ { 16, 8 }, { 32, 8 }, { 32, 16 }, { 64, 32 } },
			// Vertical:   8x16, 8x32, 16x32, 32x64
			{ { 8, 16 }, { 8, 32 }, { 16, 32 }, { 32, 64 } },
		};

		// -----------------------------------------------------------------------------------
		// A sprite decoded from OAM (attributes 0-2 plus its rotation/scaling group).
		// -----------------------------------------------------------------------------------

		struct Sprite
		{
			int baseWidth = 8;		// the size the OAM shape/size fields name
			int baseHeight = 8;
			int width = 8;			// the display area (double size when affine + double size)
			int height = 8;
			int left = 0;			// the left edge of the display area, in screen dots
			int top = 0;
			int objMode = 0;		// 0 = normal, 1 = semi-transparent, 2 = OBJ window, 3 = bad
			bool affine = false;
			bool doubleSize = false;
			bool colors256 = false;
			bool mosaic = false;
			bool semiTransparent = false;
			bool objWindow = false;
			int tileNumber = 0;
			int paletteIndex = 0;
			int objPriority = 0;	// the OBJ/BG priority, 0 = highest
			bool hFlip = false;		// attribute 1 bit 12 (only without rotation/scaling)
			bool vFlip = false;		// attribute 1 bit 13 (only without rotation/scaling)
			int pa = 0, pb = 0, pc = 0, pd = 0;		// the affine matrix in 8.8 fixed point
		};

		// -----------------------------------------------------------------------------------
		// Ppu::LayerBelow and Ppu::LineState are declared in the header, because the per-line
		// painters are members: they speak about the private LayerPixel stack.
		// -----------------------------------------------------------------------------------

		// -----------------------------------------------------------------------------------
		// Fixed point helpers for the rotation/scaling reference point.
		//
		// GBATEK 4000028h: the reference point is a 28-bit value, "shifted left by eight", so
		// bits 0-7 are the fraction and bits 8-26 the signed integer part. The value wraps
		// modulo 2^28 when the matrix advance makes it overflow.
		// -----------------------------------------------------------------------------------

		const u32 REFERENCE_MASK = 0x0FFFFFFF;

		/// <summary>Round a source coordinate to the nearest texel, which is the texel the
		/// hardware samples for a rotated/scaled layer.</summary>
		inline s32 ReferenceRounded(s32 value)
		{
			const s32 integer = value >> 8;
			const s32 fraction = value & 0xFF;
			return (fraction >= 0x80) ? (integer + 1) : integer;
		}

		/// <summary>True for the bitmap modes 3, 4 and 5.</summary>
		inline bool IsBitmapMode(int mode)
		{
			return mode >= 3 && mode <= 5;
		}

		// -----------------------------------------------------------------------------------
		// Small read helpers. Everything goes through the public PPU accessors.
		// -----------------------------------------------------------------------------------

		inline u16 ReadVram16(const Ppu& ppu, u32 offset)
		{
			return (u16)(ppu.ReadVram(offset) | (ppu.ReadVram(offset + 1) << 8));
		}

		/// <summary>Read a whole palette entry. A halfword assembled from two byte reads would be
		/// wrong (the byte accessor applies GBATEK's OR rule to the two halves of the entry), so
		/// this uses the halfword accessor that GbaBus also uses for 0x05000000.</summary>
		inline u16 ReadPaletteEntry(const Ppu& ppu, u32 offset)
		{
			return ppu.ReadPalette16(offset);
		}

		inline u16 OamWord(const Ppu& ppu, u32 offset)
		{
			return (u16)(ppu.ReadOam(offset) | (ppu.ReadOam(offset + 1) << 8));
		}

		/// <summary>Read one of the 32 OBJ rotation/scaling parameters through OAM. GBATEK
		/// "Location of Rotation/Scaling Parameters in OAM": group n stores PA, PB, PC, PD in the
		/// unused 16-bit gaps of the OAM entries 4n, 4n+1, 4n+2 and 4n+3.</summary>
		inline s16 AffineParam(const Ppu& ppu, int index, int component)
		{
			return (s16)OamWord(ppu, (u32)(index * 0x20 + 0x06 + component * 8));
		}

		// -----------------------------------------------------------------------------------
		// The window region bits of a dot.
		// -----------------------------------------------------------------------------------

		/// <summary>The WININ/WINOUT bits of the window region that contains `x` (GBATEK
		/// "Window Priority": window 0 wins over window 1, which wins over the OBJ window, and
		/// everything else is the outside region).</summary>
		u16 WindowBits(u8 region, u16 winin, u16 winout)
		{
			switch (region)
			{
			case MASK_WIN0: return winin;
			case MASK_WIN1: return (u16)(winin >> WIN_SECOND);
			case MASK_OBJWIN: return (u16)(winout >> WIN_SECOND);
			default: return (u16)(winout & 0xFF);
			}
		}

		// The winner bits of a dot: "all layers and the special effect" (BG0-BG3 in bits 0-3, OBJ
		// in bit 4, the effect in bit 5). With the window feature off (none of DISPCNT bits 13/14/15
		// set) every layer is displayed everywhere and the window registers are ignored, which is
		// what these bits then say (GBATEK 4000000h).
		const u16 WINDOW_ALL = 0x3F;

		// -----------------------------------------------------------------------------------
		// The window columns and rows.
		// -----------------------------------------------------------------------------------

		void WindowColumns(u16 reg, int& x1, int& x2)
		{
			int left = (reg >> 8) & 0xFF;		// bits 8-15: X1, the leftmost coordinate
			int right = reg & 0xFF;				// bits 0-7: X2, the rightmost coordinate + 1
			const int limit = ScreenWidth;

			if (left > limit) left = limit;
			if (right > limit) right = limit;

			// GBATEK 4000040h: "Garbage values of X2>240 or X1>X2 are interpreted as X2=240".
			// An inverted range (X1 > X2) therefore encloses no dots.
			if (left > right)
				right = left;

			x1 = left;
			x2 = right;
		}

		void WindowRows(u16 reg, int& y1, int& y2)
		{
			int first = (reg >> 8) & 0xFF;
			int last = reg & 0xFF;

			if (first > ScreenHeight) first = ScreenHeight;
			if (last > ScreenHeight) last = ScreenHeight;
			if (first > last)
				last = first;

			y1 = first;
			y2 = last;
		}

		// -----------------------------------------------------------------------------------
		// The OBJ tile fetch. NO_PIXEL means "no visible dot of this sprite here".
		// -----------------------------------------------------------------------------------

		u16 SpritePixel(const Ppu& ppu, const Sprite& sprite, int sx, int sy)
		{
			const int px = sx - sprite.left;	// 0..width-1
			const int py = sy - sprite.top;		// 0..height-1

			// GBATEK "OBJ Reference Point & Rotation Center": the reference point is the upper-left
			// of the OBJ, so a dot of the display area is measured from its middle, which is where
			// the rotation center sits. The matrix is 8.8 fixed point.
			const s32 offsetX = px - sprite.width / 2;
			const s32 offsetY = py - sprite.height / 2;
			const s32 srcX = sprite.pa * offsetX + sprite.pb * offsetY;
			const s32 srcY = sprite.pc * offsetX + sprite.pd * offsetY;

			// The source coordinate comes out relative to the rotation center, so half the base
			// size (in 8.8 fixed point) puts it back into the base OBJ.
			s32 texelX = ReferenceRounded(srcX + sprite.baseWidth * 128);
			s32 texelY = ReferenceRounded(srcY + sprite.baseHeight * 128);

			// A dot that falls outside of the base OBJ is not displayed: that is what clips a
			// rotated OBJ to its non double-sized rectangle.
			if (texelX < 0 || texelX >= sprite.baseWidth || texelY < 0 || texelY >= sprite.baseHeight)
				return NO_PIXEL;

			if (!sprite.affine)
			{
				// Without rotation/scaling, attribute 1 bits 12/13 mirror the OBJ (GBATEK "OBJ
				// Attribute 1"). An affine OBJ has the parameter-selection field there instead,
				// so the bits mean nothing for it.
				if (sprite.hFlip) texelX = sprite.baseWidth - 1 - texelX;
				if (sprite.vFlip) texelY = sprite.baseHeight - 1 - texelY;
			}

			// GBATEK "OBJ Tile Number": in the bitmap modes only tile numbers 512-1023 may be
			// used, because the lower 16 KByte of the OBJ area holds the frame buffer.
			if (IsBitmapMode(ppu.DispCnt() & 7) && sprite.tileNumber < 512)
				return NO_PIXEL;

			const int tileX = (int)(texelX / 8);
			const int tileY = (int)(texelY / 8);
			const bool oneDimensional = (ppu.DispCnt() & (1 << DC_OBJ_1D)) != 0;
			// The tile matrix of an OBJ is as wide as the OBJ itself: the next 8x8 row of the OBJ
			// starts that many tiles on (GBATEK "OBJ - VRAM Character (Tile) Mapping"). Only the
			// 2-dimensional matrix uses the fixed 32-tile stride of the VRAM layout.
			const int rowTiles = sprite.baseWidth / 8;
			int tileNumber = sprite.tileNumber;

			if (sprite.colors256)
			{
				// GBATEK "OBJ Tile Number": in 256 colour mode only every second tile may be
				// used, so the tile numbers step by two ("in 2-dimensional mapping mode, the bit
				// is completely ignored").
				tileNumber += oneDimensional ? (tileY * rowTiles + tileX) * 2 : tileY * 32 + tileX * 2;
			}
			else
			{
				// 16 colour mode: "the upper row of the OBJ will consist of tile 04h and 05h, the
				// next row of 24h and 25h" (2D) or "06h and 07h" (1D).
				tileNumber += oneDimensional ? (tileY * rowTiles + tileX) : tileY * 32 + tileX;
			}

			// GBATEK "LCD OBJ - Overview": the object tiles are at 06010000-06017FFF in the BG
			// modes 0-2 and at 06014000-06017FFF in the bitmap modes 3-5, where the frame buffer
			// takes 0x00000-0x13FFF. The in-tile offset is one byte per two dots in 16 colour mode.
			//
			// The tile number is a 32-byte slot index in *both* colour depths (GBATEK "OBJ Tile
			// Number": in 256 colour mode "only each second tile may be used", so the number above
			// already steps by two and a 256-colour tile is the two slots tileNumber and
			// tileNumber+1). The byte address is therefore always tileNumber * 32 - multiplying by
			// 64 here would skip the second slot every 256-colour tile occupies.
			const u32 objTileBase = IsBitmapMode(ppu.DispCnt() & 7) ? BITMAP_OBJ_TILES : OBJ_TILE_BASE;
			const u32 inTile = (u32)(texelY & 7) * (sprite.colors256 ? 8 : 4) +
				(u32)((texelX & 7) / (sprite.colors256 ? 1 : 2));
			const u32 address = objTileBase + (u32)tileNumber * TILE_SIZE_4BPP + inTile;
			const u8 packed = ppu.ReadVram(address);

			if (sprite.colors256)
			{
				if (packed == 0)
					return NO_PIXEL;			// colour 0 is transparent

				return ReadPaletteEntry(ppu, PALETTE_OBJ_OFFSET + (u32)packed * 2);
			}

			const u8 pixelIndex = (texelX & 1) ? (u8)(packed >> 4) : (u8)(packed & 0xF);
			if (pixelIndex == 0)
				return NO_PIXEL;

			return ReadPaletteEntry(ppu, PALETTE_OBJ_OFFSET + (u32)(sprite.paletteIndex * 16 + pixelIndex) * 2);
		}

		// -----------------------------------------------------------------------------------
		// The bitmap background of the modes 3, 4 and 5.
		// -----------------------------------------------------------------------------------

		/// <summary>Read a dot of the bitmap background. NO_PIXEL means that the dot is outside
		/// of the bitmap (always transparent) or uses the transparent palette colour 0.</summary>
		u16 BitmapPixel(const Ppu& ppu, s32 x, s32 y)
		{
			const int mode = ppu.DispCnt() & 7;
			const u32 page = ((ppu.DispCnt() >> DC_FRAME_SELECT) & 1) ? BITMAP_FRAME1 : 0;

			if (mode == 3)
			{
				// 240x160 dots, 32768 colours, two bytes per dot and 480 bytes per line. There is
				// no transparent colour in this mode (GBATEK "BG Mode 3").
				if (x < 0 || x >= ScreenWidth || y < 0 || y >= MODE3_HEIGHT)
					return NO_PIXEL;

				return ReadVram16(ppu, page + (u32)y * BITMAP_STRIDE_16 + (u32)x * 2);
			}

			if (mode == 4)
			{
				// 240x160 dots, 256 of the 32768 colours, one byte per dot and 240 bytes per line
				// (GBATEK "BG Mode 4"). Colour 0 is transparent so that OBJs may be displayed
				// behind the bitmap.
				if (x < 0 || x >= ScreenWidth || y < 0 || y >= MODE4_HEIGHT)
					return NO_PIXEL;

				const u8 pixelIndex = ppu.ReadVram(page + (u32)y * BITMAP_STRIDE_8 + (u32)x);
				if (pixelIndex == 0)
					return NO_PIXEL;

				return ReadPaletteEntry(ppu, (u32)pixelIndex * 2);
			}

			// Mode 5: the frame is 160x128 dots inside the 240x160 screen, two bytes per dot and
			// 320 bytes per line. The dots outside of the 160x128 area are not part of the frame
			// buffer, so the layers below (ultimately the backdrop) show through there.
			if (x < 0 || x >= MODE5_WIDTH || y < 0 || y >= MODE5_HEIGHT)
				return NO_PIXEL;

			return ReadVram16(ppu, page + (u32)y * BITMAP_STRIDE_16 + (u32)x * 2);
		}
	}

	// ---------------------------------------------------------------------------------------
	// Lifecycle
	// ---------------------------------------------------------------------------------------

	void Ppu::Reset()
	{
		// The display registers come up clear (GBATEK "GBA I/O Map": they are plain R/W registers
		// with no documented power-on value beyond zero).
		dispcnt = 0;
		greenswap = 0;
		bldcnt = 0;
		bldalpha = 0;
		bldy = 0;

		for (int i = 0; i < 4; i++)
		{
			bgcnt[i] = 0;
			bghofs[i] = 0;
			bgvofs[i] = 0;
		}

		for (int i = 0; i < 2; i++)
		{
			bgpa[i] = 0;
			bgpb[i] = 0;
			bgpc[i] = 0;
			bgpd[i] = 0;
			bgx[i] = 0;
			bgy[i] = 0;
			bgxLatch[i] = 0;
			bgyLatch[i] = 0;
		}

		win0h = 0;
		win1h = 0;
		win0v = 0;
		win1v = 0;
		winin = 0;
		winout = 0;
		mosaic = 0;

		dispstat = 0;
		vcount = 0;

		currentLine = 0;
		lineCycles = 0;
		frameCounter = 0;

		memset(line, 0, sizeof(line));
		memset(windowMask, 0, sizeof(windowMask));
		memset(pixels, 0, sizeof(pixels));

		// The frame buffer is ScreenWidth * ScreenHeight XRGB8888 pixels.
		frame.assign((size_t)ScreenWidth * ScreenHeight, 0);

		// The display memory: 1 KByte of palette RAM, 96 KByte of VRAM and 1 KByte of OAM. Every
		// display mode shares the 96 KByte linearly (see VramAddress).
		palette.Init(PaletteSize);
		vram.Init(VramSize);
		oam.Init(OamSize);

		palette.Fill(0);
		vram.Fill(0);
		oam.Fill(0);

		// The V-Counter flag reflects VCOUNT against the (still zero) setting.
		UpdateVCountMatch(nullptr);
	}

	// ---------------------------------------------------------------------------------------
	// CPU side
	// ---------------------------------------------------------------------------------------

	u16 Ppu::Read16(u32 offset, u16 openBus) const
	{
		if (offset >= 0x060)
		{
			// Anything past 0x05F is outside the LCD register block.
			Log(LogLevel::Warn, "PPU: read of unmapped display register 0x%03X", offset);
			return openBus;
		}

		switch (offset & ~1u)
		{
		case REG_DISPCNT: return dispcnt;
		case REG_GREENSWP: return (u16)(greenswap & 1);

		case REG_DISPSTAT: return ComputeDispStat();

		case REG_VCOUNT: return vcount;

		// The BG control registers (GBATEK 4000008h).
		case 0x008: return bgcnt[0];
		case 0x00A: return bgcnt[1];
		case 0x00C: return bgcnt[2];
		case 0x00E: return bgcnt[3];

		case REG_WININ: return winin;
		case 0x04A: return winout;
		case REG_BLDCNT: return bldcnt;
		case 0x052: return bldalpha;

		// BG0-3HOFS/VOFS (0x010..0x01F) are write-only on the hardware (GBATEK 4000010h marks
		// them (W)): the PPU keeps no readable copy of the offsets, so a read returns the low
		// byte, which is the part the debugger and the tests are interested in. The affine
		// parameters keep their 16-bit value, and a reference point returns its write latch, the
		// readable half of the two-register write protocol (GBATEK 4000028h).
		case 0x010: return (u16)(bghofs[0] & 0xFF);
		case 0x012: return (u16)(bgvofs[0] & 0xFF);
		case 0x014: return (u16)(bghofs[1] & 0xFF);
		case 0x016: return (u16)(bgvofs[1] & 0xFF);
		case 0x018: return (u16)(bghofs[2] & 0xFF);
		case 0x01A: return (u16)(bgvofs[2] & 0xFF);
		case 0x01C: return (u16)(bghofs[3] & 0xFF);
		case 0x01E: return (u16)(bgvofs[3] & 0xFF);
		case 0x020: return (u16)bgpa[0];
		case 0x022: return (u16)bgpb[0];
		case 0x024: return (u16)bgpc[0];
		case 0x026: return (u16)bgpd[0];
		case 0x028: return (u16)(bgxLatch[0] & 0xFFFF);
		case 0x02A: return (u16)(bgxLatch[0] >> 16);
		case 0x02C: return (u16)(bgyLatch[0] & 0xFFFF);
		case 0x02E: return (u16)(bgyLatch[0] >> 16);
		case 0x030: return (u16)bgpa[1];
		case 0x032: return (u16)bgpb[1];
		case 0x034: return (u16)bgpc[1];
		case 0x036: return (u16)bgpd[1];
		case 0x038: return (u16)(bgxLatch[1] & 0xFFFF);
		case 0x03A: return (u16)(bgxLatch[1] >> 16);
		case 0x03C: return (u16)(bgyLatch[1] & 0xFFFF);
		case 0x03E: return (u16)(bgyLatch[1] >> 16);

		// The window dimensions and the mosaic size are write-only (GBATEK 4000040h/400004Ch);
		// these copies are kept for the debugger's benefit.
		case 0x040: return win0h;
		case 0x042: return win1h;
		case 0x044: return win0v;
		case 0x046: return win1v;
		case REG_MOSAIC: return mosaic;
		case 0x054: return (u16)(bldy & 0x1F);

		default: break;
		}

		Log(LogLevel::Warn, "PPU: read of unused display register 0x%03X", offset);
		return openBus;
	}

	void Ppu::Write16(GbaBus& bus, u32 offset, u16 value, int cycles)
	{
		(void)cycles;

		if (offset >= 0x060)
		{
			Log(LogLevel::Warn, "PPU: write to unmapped display register 0x%03X", offset);
			return;
		}

		switch (offset & ~1u)
		{
		case REG_DISPCNT: dispcnt = value; break;
		case REG_GREENSWP: greenswap = (u16)(value & 1); break;

		case REG_DISPSTAT:
			// Bits 3-5 are the IRQ enables and bits 8-15 the V-Count setting, both R/W; bits 0-2
			// are the read-only flags and bits 6-7 are not used in GBA mode (GBATEK 4000004h). The
			// whole word is kept (the read side masks it), because the setting shares the member
			// with the flags.
			dispstat = value;
			UpdateVCountMatch(&bus);
			break;

		case REG_VCOUNT:
			Log(LogLevel::Warn, "PPU: write to the read-only VCOUNT register");
			break;

		case 0x008: bgcnt[0] = value; break;
		case 0x00A: bgcnt[1] = value; break;
		case 0x00C: bgcnt[2] = value; break;
		case 0x00E: bgcnt[3] = value; break;

		case 0x010:
			bghofs[0] = (u16)(value & 0x1FF);
			break;
		case 0x012: bgvofs[0] = (u16)(value & 0x1FF); break;
		case 0x014: bghofs[1] = (u16)(value & 0x1FF); break;
		case 0x016: bgvofs[1] = (u16)(value & 0x1FF); break;
		case 0x018: bghofs[2] = (u16)(value & 0x1FF); break;
		case 0x01A: bgvofs[2] = (u16)(value & 0x1FF); break;
		case 0x01C: bghofs[3] = (u16)(value & 0x1FF); break;
		case 0x01E: bgvofs[3] = (u16)(value & 0x1FF); break;

		case 0x020: bgpa[0] = (s16)value; break;
		case 0x022: bgpb[0] = (s16)value; break;
		case 0x024: bgpc[0] = (s16)value; break;
		case 0x026: bgpd[0] = (s16)value; break;

		case 0x028:
			// BG2X_L: the low 16 bits of the write latch, then the whole reference point goes to
			// the internal register (GBATEK 4000028h: a write outside of VBlank takes effect for
			// the current scanline immediately). The latch's high 4 bits are never written.
			bgxLatch[0] = (bgxLatch[0] & 0x0FFF0000u) | value;
			bgx[0] = (s32)(bgxLatch[0] & REFERENCE_MASK);
			break;

		case 0x02A:
			// BG2X_H: only bits 0-11 exist; writing it must not disturb the low 16 bits.
			bgxLatch[0] = ((u32)(value & 0x0FFF) << 16) | (bgxLatch[0] & 0xFFFF);
			bgx[0] = (s32)(bgxLatch[0] & REFERENCE_MASK);
			break;

		case 0x02C:
			bgyLatch[0] = (bgyLatch[0] & 0x0FFF0000u) | value;
			bgy[0] = (s32)(bgyLatch[0] & REFERENCE_MASK);
			break;

		case 0x02E:
			bgyLatch[0] = ((u32)(value & 0x0FFF) << 16) | (bgyLatch[0] & 0xFFFF);
			bgy[0] = (s32)(bgyLatch[0] & REFERENCE_MASK);
			break;

		case 0x030: bgpa[1] = (s16)value; break;
		case 0x032: bgpb[1] = (s16)value; break;
		case 0x034: bgpc[1] = (s16)value; break;
		case 0x036: bgpd[1] = (s16)value; break;

		case 0x038:
			bgxLatch[1] = (bgxLatch[1] & 0x0FFF0000u) | value;
			bgx[1] = (s32)(bgxLatch[1] & REFERENCE_MASK);
			break;

		case 0x03A:
			bgxLatch[1] = ((u32)(value & 0x0FFF) << 16) | (bgxLatch[1] & 0xFFFF);
			bgx[1] = (s32)(bgxLatch[1] & REFERENCE_MASK);
			break;

		case 0x03C:
			bgyLatch[1] = (bgyLatch[1] & 0x0FFF0000u) | value;
			bgy[1] = (s32)(bgyLatch[1] & REFERENCE_MASK);
			break;

		case 0x03E:
			bgyLatch[1] = ((u32)(value & 0x0FFF) << 16) | (bgyLatch[1] & 0xFFFF);
			bgy[1] = (s32)(bgyLatch[1] & REFERENCE_MASK);
			break;

		case 0x040: win0h = value; break;
		case 0x042: win1h = value; break;
		case 0x044: win0v = value; break;
		case 0x046: win1v = value; break;
		case REG_WININ: winin = value; break;
		case 0x04A: winout = value; break;
		case REG_MOSAIC: mosaic = value; break;
		case REG_BLDCNT: bldcnt = value; break;
		case 0x052: bldalpha = value; break;
		case 0x054: bldy = (u16)(value & 0x1F); break;

		default:
			Log(LogLevel::Warn, "PPU: write to unused display register 0x%03X", offset);
			break;
		}
	}

	u8 Ppu::ReadPalette(u32 offset) const
	{
		// GBATEK "LCD Color Palettes": a colour is one 16-bit entry. A byte read is not a real
		// access - the hardware returns the OR of the two bytes of the halfword.
		const u32 index = offset & (PaletteSize - 1) & ~1u;
		return (u8)(palette.Read8(index) | palette.Read8(index + 1));
	}

	void Ppu::WritePalette(u32 offset, u8 value)
	{
		// A byte write drives only the byte lane the CPU drove, so the other half of the entry
		// keeps its old value: palette RAM sits on a 16-bit bus and the hardware only latches the
		// lanes an 8-bit store actually drives. (Real code writes halfwords.)
		palette.Write8(offset & (PaletteSize - 1), value);
	}

	u32 Ppu::VramAddress(u32 offset) const
	{
		// GBATEK "GBA Memory Map" and "LCD VRAM Overview": the whole 96 KByte is real, linearly
		// addressed memory in every display mode. The only difference between the mode families is
		// where the *renderer* looks for the object tiles (0x10000 in the tile modes, 0x14000 in
		// the bitmap modes, where the frame buffer occupies 0x00000-0x13FFF), which the sprite
		// decoder applies - not an address translation.
		//
		// 96 KByte is not a power of two, so the mirroring wrap is a modulo and not a bit mask:
		// with `offset & 0x17FFF` the second 32 KByte would alias onto the first. The region also
		// mirrors every 128 KByte (GBATEK "GBA Memory Map"), so 0x06018000 and 0x06020000 both
		// read the byte at 0x06000000 - the `memory` test ROM of `jsmolka/gba-tests` checks it.
		return (offset % VramMirror) % VramSize;
	}

	u8 Ppu::ReadVram(u32 offset) const
	{
		const u32 address = VramAddress(offset);
		if (address >= VramSize)
			return 0;					// the unmapped hole in the bitmap modes
		return vram.Read8(address);
	}

	void Ppu::WriteVram(u32 offset, u8 value)
	{
		const u32 address = VramAddress(offset);
		if (address >= VramSize)
			return;

		// As for the palette: a byte write drives only the byte lane the CPU drove, which is the
		// documented simplification of VRAM's 16-bit bus.
		vram.Write8(address, value);
	}

	u8 Ppu::ReadOam(u32 offset) const
	{
		return oam.Read8(offset & (OamSize - 1));
	}

	void Ppu::WriteOam(u32 offset, u8 value)
	{
		// GBATEK "GBA Memory Map": OAM accepts 16-bit and 32-bit writes only, so a byte write to
		// the high byte of a halfword is ignored. A byte write to the low byte stores that byte
		// and leaves the high half as it was (the value driven on the upper lanes is open bus,
		// modelled here as zero).
		const u32 index = offset & (OamSize - 1);
		if ((index & 1) == 0)
			oam.Write8(index, value);
	}

	// ---------------------------------------------------------------------------------------
	// Timing
	// ---------------------------------------------------------------------------------------

	void Ppu::Tick(GbaBus& bus, int cycles)
	{
		// The visible part of a line is 240 dots of four cycles each; the remaining cycles of the
		// 1232 are the HBlank interval (GBATEK "Horizontal Dimensions").
		const int visibleCycles = ScreenWidth * 4;

		while (cycles > 0)
		{
			// Consume up to the next edge of the LCD. There are two per line: the start of HBlank
			// (960 cycles into the line, where the visible part ends and the whole scanline is
			// composed) and the end of the line (1232). Stopping at both is what makes the HBlank
			// edge observable no matter how large the budget handed in is; a single step to the
			// end of the line would jump straight over it.
			int next = (lineCycles < visibleCycles) ? visibleCycles : CyclesPerScanline;
			int step = next - lineCycles;
			if (step > cycles)
				step = cycles;

			const int before = lineCycles;
			lineCycles += step;
			cycles -= step;

			if (before < visibleCycles && lineCycles >= visibleCycles && vcount < ScreenHeight)
			{
				// The whole scanline is composed here, when its HBlank starts (the documented
				// deviation: the hardware draws it dot by dot during the visible part).
				RenderLine(bus, vcount);

				// The HBlank edge sets DISPSTAT's H-Blank flag (which Read16 derives from the
				// line position), raises the HBlank interrupt when bit 4 enables it and triggers
				// the HBlank DMA and the video capture DMA. GBATEK "LCD Dimensions and Timings":
				// "no H-Blank interrupts are generated within V-Blank", so the hidden lines do
				// none of this.
				if ((dispstat & 0x10) != 0)
					bus.irq.Raise(INT_HBLANK);

				bus.dma.OnHBlank(bus);
				bus.dma.OnScanline(bus);
			}

			if (lineCycles >= CyclesPerScanline)
			{
				lineCycles -= CyclesPerScanline;
				AdvanceVCount(bus);
			}
		}
	}

	void Ppu::AdvanceVCount(GbaBus& bus)
	{
		vcount++;
		if (vcount >= ScanlinesTotal)
		{
			// 228 lines have been drawn (160 visible and 68 hidden): the frame is complete.
			vcount = 0;
			frameCounter++;
		}

		if (vcount == ScreenHeight)
		{
			// Entering line 160: set the V-Blank flag, raise INT_VBLANK when it is enabled and
			// start the VBlank DMA (GBATEK 4000004h: "set in line 160..226").
			dispstat |= STAT_VBLANK;
			if ((dispstat & 0x08) != 0)
				bus.irq.Raise(INT_VBLANK);

			bus.dma.OnVBlank(bus);
		}
		else if (vcount == ScanlinesTotal - 1)
		{
			// Line 227 is not part of the VBlank period any more.
			dispstat &= (u16)~STAT_VBLANK;
		}

		UpdateVCountMatch(&bus);
	}

	void Ppu::UpdateVCountMatch(GbaBus* bus)	{
		// The V-Counter flag is set while VCOUNT equals the setting in the high byte of DISPSTAT
		// (bits 8-15); the interrupt is requested when bit 5 enables it (GBATEK 4000004h).
		if (vcount == (u16)(dispstat >> 8))
		{
			dispstat |= STAT_VCOUNT;

			if (bus != nullptr && (dispstat & 0x20) != 0)
				bus->irq.Raise(INT_VCOUNT);
		}
		else
		{
			dispstat &= (u16)~STAT_VCOUNT;
		}
	}

	// ---------------------------------------------------------------------------------------
	// The scanline
	// ---------------------------------------------------------------------------------------

	u16 Ppu::ComputeDispStat() const
	{
		// The stored bits: the IRQ enables (3-5) and the V-Count setting (8-15); bits 6-7 are not
		// used in GBA mode. Everything else the CPU reads is generated from the current position
		// (GBATEK 4000004h).
		u16 value = (u16)(dispstat & 0xFF38);

		if (vcount >= ScreenHeight && vcount < ScanlinesTotal - 1)
			value |= STAT_VBLANK;			// set in the lines 160..226

		if (lineCycles >= ScreenWidth * 4)
			value |= STAT_HBLANK;			// the visible part of the line is over

		if (vcount == (u16)(dispstat >> 8))
			value |= STAT_VCOUNT;			// VCOUNT matches the setting

		return value;
	}

	void Ppu::RenderLine(GbaBus& bus, int y)
	{
		currentLine = y;
		vcount = (u16)y;

		if (y == 0)
		{
			// GBATEK 4000028h: the reference points are copied from the write latches to the
			// internal registers during each VBlank, i.e. they define the origin of the topmost
			// scanline.
			bgx[0] = (s32)(bgxLatch[0] & REFERENCE_MASK);
			bgy[0] = (s32)(bgyLatch[0] & REFERENCE_MASK);
			bgx[1] = (s32)(bgxLatch[1] & REFERENCE_MASK);
			bgy[1] = (s32)(bgyLatch[1] & REFERENCE_MASK);
		}
		else
		{
			// The internal registers are then incremented by dmx (PB) and dmy (PD) after each
			// scanline, in 8.8 fixed point (GBATEK "Internal Reference Point Registers"). The
			// parameters carry the same eight fractional bits as the reference point, so one dot
			// of PB is 0x0100 and the increment is the raw register value.
			for (int i = 0; i < 2; i++)
			{
				bgx[i] = (s32)((u32)(bgx[i] + (s32)bgpb[i]) & REFERENCE_MASK);
				bgy[i] = (s32)((u32)(bgy[i] + (s32)bgpd[i]) & REFERENCE_MASK);
			}
		}

		// The register state of this scanline, gathered once for the helpers.
		LineState state;
		state.line = y;
		state.dispcnt = dispcnt;
		state.mosaic = mosaic;
		state.winin = winin;
		state.winout = winout;
		state.bldcnt = bldcnt;
		state.bldalpha = bldalpha;
		state.bldy = bldy;
		state.mode = (u8)(dispcnt & 7);
		state.forcedBlank = ForcedBlank();
		// GBATEK 4000000h: the window feature only exists while at least one of DISPCNT bits
		// 13/14/15 enables a window. With all three clear, WININ/WINOUT are ignored entirely and
		// every layer is displayed everywhere (which is why a reset screen with WINOUT = 0 still
		// shows its layers).
		state.windowsActive = (dispcnt & ((1 << DC_WIN0_ENABLE) | (1 << DC_WIN1_ENABLE) |
			(1 << DC_OBJ_WIN_ENABLE))) != 0;
		state.windowMask = windowMask;

		// 1. The sprites. They must come first: the OBJ window region is made of sprites, and the
		// top-most OBJ takes part in the priority order of the layer stack.
		RenderSprites(bus);

		// 2. The window regions.
		ApplyWindows();

		// The dot directly below the winner of every dot, for the alpha blending pass. It is
		// local to this render (a file-static buffer, because the paint helpers are file scope).
		static LayerBelow below[ScreenWidth];

		// 3. The backdrop: BG palette colour 0, opaque everywhere, at the bottom of the stack
		// (GBATEK "LCD Color Palettes": "Color 0 of BG Palette 0 is used as backdrop color").
		const u16 backdropColor = ReadPaletteEntry(*this, 0);
		const bool objEnabled = !state.forcedBlank && (dispcnt & (1 << DC_OBJ_ENABLE)) != 0;

		for (int x = 0; x < ScreenWidth; x++)
		{
			// The OBJ layer, already resolved by RenderSprites, is pushed before the backgrounds
			// so that a sprite wins against a background of the same priority (GBATEK
			// "Priority": "the OBJ becomes higher priority and is displayed on top of that BG
			// layer").
			const u16 objColor = pixels[x].color;
			const u8 objPriority = pixels[x].priority;
			const bool objSemi = pixels[x].semiTransparent;
			const bool pushObj = (pixels[x].layer == LAYER_OBJ) && objEnabled &&
				(((state.windowsActive
					? WindowBits(state.windowMask[x], state.winin, state.winout) : WINDOW_ALL) &
					(1 << LAYER_OBJ)) != 0);

			pixels[x].color = backdropColor;
			pixels[x].layer = LAYER_BACKDROP;
			pixels[x].priority = 0xFF;
			pixels[x].semiTransparent = false;

			below[x].color = 0;
			below[x].layer = LAYER_NONE;
			below[x].priority = 0xFF;
			below[x].semitransparent = false;

			if (pushObj)
				PushDot(pixels[x], below[x], objPriority, LAYER_OBJ, objColor, objSemi);
		}

		// 4. The four backgrounds, each of them over the whole line.
		if (!state.forcedBlank)
		{
			for (int index = 0; index < 4; index++)
				DrawBackgroundLayer(state, index, below);
		}

		// 5. The colour special effects.
		for (int x = 0; x < ScreenWidth; x++)
			ApplyDotEffect(state, x, below[x]);

		// 6. The host's XRGB8888 frame.
		u32* target = frame.data() + (size_t)y * ScreenWidth;

		for (int x = 0; x < ScreenWidth; x++)
		{
			u16 color = line[x];

			if (state.forcedBlank)
			{
				// Forced blank (DISPCNT bit 7) makes the LCD display white lines (GBATEK
				// "Blanking Bits"). It is the last stage of the pipeline, so the line buffer -
				// what the rest of the emulator and the debugger see - holds the white dot too.
				color = 0x7FFF;
			}

			if ((greenswap & 1) != 0)
			{
				// Undocumented Green Swap (4000002h): the green intensities of each group of two
				// dots are exchanged. It is a final-stage effect, so it applies to the white lines
				// of a forced blank as well.
				color = Swap16(color);
			}

			line[x] = color;
			target[x] = Color15ToXrgb(color);
		}
	}

	void Ppu::RenderSprites(GbaBus& bus)
	{
		(void)bus;

		const bool objWindowEnabled = (dispcnt & (1 << DC_OBJ_WIN_ENABLE)) != 0;

		// GBATEK 400004Ch: the OBJ mosaic block is anchored at the upper-left of the screen and
		// its size comes from the upper half of MOSAIC. An OBJ with the mosaic bit samples the
		// origin dot of the block, so the sampled position moves with the sprite.
		const int mosaicH = ((mosaic >> 8) & 0xF) + 1;
		const int mosaicV = ((mosaic >> 12) & 0xF) + 1;
		const int mosaicOriginY = (currentLine / mosaicV) * mosaicV;

		// First pass: decode the sprites that can cover this scanline. OBJ0 is the first entry of
		// OAM and has the highest priority (GBATEK "Priority").
		Sprite sprites[MAX_SPRITES];
		int spriteCount = 0;

		for (int index = 0; index < 128; index++)
		{
			const u32 base = (u32)index * 8;
			const int attr0 = (int)OamWord(*this, base);
			const int attr1 = (int)OamWord(*this, base + 2);
			const int attr2 = (int)OamWord(*this, base + 4);

			Sprite sprite;
			const int shape = (attr0 >> 14) & 3;
			const int size = (attr1 >> 14) & 3;
			sprite.objMode = (attr0 >> 10) & 3;
			sprite.affine = (attr0 & 0x100) != 0;
			sprite.doubleSize = sprite.affine && (attr0 & 0x200) != 0;
			sprite.mosaic = (attr0 & 0x1000) != 0;
			sprite.colors256 = (attr0 & 0x2000) != 0;
			sprite.semiTransparent = (sprite.objMode == 1);
			sprite.objWindow = (sprite.objMode == 2);
			sprite.tileNumber = attr2 & 0x3FF;
			sprite.objPriority = (attr2 >> 10) & 3;
			sprite.paletteIndex = (attr2 >> 12) & 0xF;

			// GBATEK "OBJ Attribute 0": shape 3 is prohibited, and so is OBJ mode 3 when the
			// rotation/scaling flag is clear. Such an OBJ is not displayed. (No$GBA reportedly
			// renders mode 3 as a rotation/scaling OBJ; that is an undocumented quirk and is not
			// implemented here.)
			if (shape == 3)
				continue;
			if (sprite.objMode == 3 && !sprite.affine)
				continue;

			// With the rotation/scaling flag clear, attribute 0 bit 9 is the OBJ Disable bit: an
			// OBJ with it set is not displayed at all (GBATEK "OBJ Attribute 0"). This is what
			// keeps the 128 entries of an unprogrammed OAM (all zero, which would otherwise every
			// one of them be a visible 8x8 OBJ at 0,0) out of the line.
			if (!sprite.affine && (attr0 & 0x200) != 0)
				continue;

			const ObjDimensions dim = ObjSizeTable[shape][size];
			sprite.baseWidth = dim.width;
			sprite.baseHeight = dim.height;
			sprite.width = sprite.doubleSize ? dim.width * 2 : dim.width;
			sprite.height = sprite.doubleSize ? dim.height * 2 : dim.height;

			// GBATEK "OBJ Reference Point & Rotation Center": the reference point is the OBJ's X/Y
			// attributes and the rotation center sits in the middle of the base OBJ, i.e. half the
			// base size right and below the reference point. That center is also the middle of the
			// display area (twice the base size when the double-size flag is set), so the display
			// area starts half of itself before the center: with the identity matrix the OBJ lands
			// exactly on (X, Y)-(X + width, Y + height).
			sprite.left = (attr1 & 0x1FF) + sprite.baseWidth / 2 - sprite.width / 2;
			sprite.top = (attr0 & 0xFF) + sprite.baseHeight / 2 - sprite.height / 2;

			// A sprite off the top or bottom of this scanline is not sampled at all. (It still
			// consumes its OBJ slot, as GBATEK "Maximum Number of Sprites per Line" warns.)
			if (sprite.top >= ScreenHeight || sprite.top + sprite.height <= 0 ||
				sprite.left >= ScreenWidth || sprite.left + sprite.width <= 0)
				continue;

			if (sprite.affine)
			{
				// The rotation/scaling group is selected by bits 9-13 of attribute 1 and its four
				// parameters are 8.8 fixed point (GBATEK "OBJ Rotation/Scaling PA,PB,PC,PD"), the
				// same units the source coordinate is computed in - a 1.0 scale is 0x0100.
				const int group = (attr1 >> 9) & 0x1F;
				sprite.pa = (s32)AffineParam(*this, group, 0);
				sprite.pb = (s32)AffineParam(*this, group, 1);
				sprite.pc = (s32)AffineParam(*this, group, 2);
				sprite.pd = (s32)AffineParam(*this, group, 3);
			}
			else
			{
				// A normal OBJ uses the identity matrix, and attribute 1 bits 12/13 mirror it
				// (GBATEK "OBJ Attribute 1"; the bits are the affine group number for an OBJ that
				// rotates, so they only mean a flip here).
				sprite.hFlip = (attr1 & 0x1000) != 0;
				sprite.vFlip = (attr1 & 0x2000) != 0;
				sprite.pa = 256;
				sprite.pb = 0;
				sprite.pc = 0;
				sprite.pd = 256;
			}

			sprites[spriteCount++] = sprite;
		}

		// Second pass, per dot: the first sprite in OAM order that covers the dot is the top-most
		// OBJ ("OBJ0 is always having priority above OBJ1-127", GBATEK "Priority"), so the scan
		// stops at the first hit. An OBJ-window sprite is only tested for the window mask.
		for (int x = 0; x < ScreenWidth; x++)
		{
			pixels[x].color = 0;
			pixels[x].layer = LAYER_NONE;
			pixels[x].priority = 0xFF;
			pixels[x].semiTransparent = false;

			// The OBJ window region belongs to this line alone: the stamp ApplyWindows reads is
			// written per line here, so a dot that an OBJ covered on the previous line is not
			// still part of the window.
			windowMask[x] = MASK_OUTSIDE;

			bool inObjWindow = false;

			for (int i = 0; i < spriteCount; i++)
			{
				const Sprite& sprite = sprites[i];

				if (x < sprite.left || x >= sprite.left + sprite.width)
					continue;

				// A mosaic OBJ samples the origin dot of its mosaic block.
				int sampleX = x;
				if (sprite.mosaic)
					sampleX = (x / mosaicH) * mosaicH;

				const u16 color = SpritePixel(*this, sprite, sampleX, mosaicOriginY);

				if (color == NO_PIXEL)
					continue;

				if (sprite.objWindow)
				{
					// The OBJ window: the sprite is not drawn, its non-transparent dots mark the
					// window region (GBATEK "The OBJ Window"). The region only exists when
					// DISPCNT bit 15 is set.
					if (objWindowEnabled)
						inObjWindow = true;

					continue;
				}

				pixels[x].color = color;
				pixels[x].layer = LAYER_OBJ;
				pixels[x].priority = (u8)sprite.objPriority;
				pixels[x].semiTransparent = sprite.semiTransparent;
				break;
			}

			if (inObjWindow)
				windowMask[x] = MASK_OBJWIN;
		}
	}

	// ---------------------------------------------------------------------------------------
	// Windows and blending
	// ---------------------------------------------------------------------------------------

	void Ppu::ApplyWindows()
	{
		// GBATEK "LCD I/O Window Feature" and "Window Priority": window 0 wins over window 1,
		// which wins over the OBJ window; every dot outside of them is the "outside" region. The
		// dimension fields are inclusive of X1/Y1 and exclusive of X2/Y2 ("the rightmost
		// coordinate, plus 1").
		const bool win0On = (dispcnt & (1 << DC_WIN0_ENABLE)) != 0;
		const bool win1On = (dispcnt & (1 << DC_WIN1_ENABLE)) != 0;
		const bool objWinOn = (dispcnt & (1 << DC_OBJ_WIN_ENABLE)) != 0;

		int win0X1, win0X2, win1X1, win1X2;
		int win0Y1, win0Y2, win1Y1, win1Y2;
		WindowColumns(win0h, win0X1, win0X2);
		WindowColumns(win1h, win1X1, win1X2);
		WindowRows(win0v, win0Y1, win0Y2);
		WindowRows(win1v, win1Y1, win1Y2);

		const bool win0Line = win0On && currentLine >= win0Y1 && currentLine < win0Y2;
		const bool win1Line = win1On && currentLine >= win1Y1 && currentLine < win1Y2;

		for (int x = 0; x < ScreenWidth; x++)
		{
			// RenderSprites has already tagged the OBJ window dots; keep that tag when the window
			// exists.
			u8 mask = (objWinOn && windowMask[x] == MASK_OBJWIN) ? MASK_OBJWIN : MASK_OUTSIDE;

			if (win0Line && x >= win0X1 && x < win0X2)
				mask = MASK_WIN0;
			else if (win1Line && x >= win1X1 && x < win1X2)
				mask = MASK_WIN1;

			windowMask[x] = mask;
		}
	}

	void Ppu::ApplyBlending()
	{
		// The pixel stack of the line is complete at this point and the second target of every
		// dot was recorded by RenderLine, so there is nothing left for this documented pass to do
		// on its own: RenderLine applies the effects dot by dot through ApplyDotEffect, exactly as
		// GBATEK "LCD I/O Color Special Effects" gives them.
	}

	// ---------------------------------------------------------------------------------------
	// Register helpers used by the renderers
	// ---------------------------------------------------------------------------------------

	bool Ppu::TextBgIs256(int index) const
	{
		// GBATEK 4000008h bit 7: 0 = 16 colours/16 palettes, 1 = 256 colours/1 palette.
		return (bgcnt[index] & (1 << BGCNT_256_COLORS)) != 0;
	}

	void Ppu::RenderMosaic()
	{
		// The mosaic filter is not a separate pass: it is applied while a layer is sampled, by
		// replacing the sample coordinate with the origin dot of the mosaic block. See
		// DrawBackgroundLayer (both axes, anchored at the upper-left of the screen) and
		// RenderSprites (the same block for the OBJs that carry the OBJ mosaic bit).
	}

	// ---------------------------------------------------------------------------------------
	// The per-line paint helpers. They are members because the pixel stack (LayerPixel) and the
	// second-target array (LayerBelow) are private; they only ever touch this->pixels, this->line
	// and the register values gathered into the LineState.
	// ---------------------------------------------------------------------------------------

	void Ppu::PushDot(LayerPixel& winner, LayerBelow& below, int priority, u8 layer, u16 color,
		bool semitransparent)
	{
		if (priority < (int)winner.priority)
		{
			// A better (lower) priority number takes the dot over; the layer that was on top
			// until now becomes the dot that is directly below the new winner.
			below.color = winner.color;
			below.layer = winner.layer;
			below.priority = winner.priority;
			below.semitransparent = winner.semiTransparent;

			winner.color = color;
			winner.layer = layer;
			winner.priority = (u8)priority;
			winner.semiTransparent = semitransparent;
		}
		else if (priority < (int)below.priority)
		{
			// The layer lost, but it is still above the dot that is currently below the winner,
			// so it becomes the new second target.
			below.color = color;
			below.layer = layer;
			below.priority = (u8)priority;
			below.semitransparent = semitransparent;
		}
	}

	void Ppu::DrawBackgroundLayer(const LineState& state, int index, LayerBelow* below)
	{
		// A layer is displayed only when its DISPCNT master enable bit is set (GBATEK "The
		// DISPCNT Register"); the per-dot window test is done in the loop below.
		if ((state.dispcnt & (1 << (DC_BG_ENABLE + index))) == 0)
			return;

		// Which backgrounds a mode has (GBATEK DISPCNT mode table): BG0-3 in mode 0, BG0/BG1 and
		// BG2 in mode 1, BG2/BG3 in mode 2 and BG2 in the bitmap modes.
		bool exists = false;
		switch (state.mode)
		{
		case 0: exists = true; break;
		case 1: exists = (index <= 2); break;
		case 2: exists = (index >= 2); break;
		case 3:
		case 4:
		case 5: exists = (index == 2); break;
		default: break;
		}

		if (!exists)
			return;

		// BG2/BG3 are the rotation/scaling layers in the modes 2-5. In the tile mode 0 they are
		// text layers, and in mode 1 only BG2 is affine (see the mode table above).
		const bool affine = (state.mode >= 2 && index >= 2);

		const u16 control = Read16(0x008 + index * 2, 0);
		const int priority = (control >> BGCNT_PRIORITY) & 3;

		// GBATEK 400004Ch: the mosaic size is the register field plus one (1..16 dots) and the
		// block is anchored at the upper-left of the screen; the origin dot of the block colours
		// the whole block.
		const int mosaicH = (control & (1 << BGCNT_MOSAIC)) ? ((state.mosaic & 0xF) + 1) : 1;
		const int mosaicV = (control & (1 << BGCNT_MOSAIC)) ? (((state.mosaic >> 4) & 0xF) + 1) : 1;
		const int sourceY = (state.line / mosaicV) * mosaicV;

		for (int x = 0; x < ScreenWidth; x++)
		{
			// GBATEK "The Window Feature": a layer is displayed only when both DISPCNT and
			// WININ/WINOUT enable it.
			const u16 windowBits = state.windowsActive
				? WindowBits(state.windowMask[x], state.winin, state.winout) : WINDOW_ALL;
			if ((windowBits & (1 << index)) == 0)
				continue;

			const int sourceX = (x / mosaicH) * mosaicH;
			u16 color = 0;
			bool opaque;

			if (IsBitmapMode(state.mode))
			{
				// GBATEK "LCD VRAM Bitmap BG": the bitmap modes have no tile map and no
				// rotation/scaling of their own - BG2PA-PD and the reference point belong to the
				// tile affine modes 1 and 2 - so the frame buffer is displayed 1:1 at the screen
				// coordinate (BitmapPixel handles the page, the mode 5 window and the mode 4
				// palette). The mosaic still quantises the sample point.
				color = BitmapPixel(*this, sourceX, sourceY);
				opaque = (color != NO_PIXEL);
			}
			else
			{
				opaque = affine ? AffineBgDot(state, index, sourceX, sourceY, color)
					: TextBgDot(index, sourceX, sourceY, color);
			}

			if (opaque)
				PushDot(pixels[x], below[x], priority, (u8)index, color, false);
		}
	}

	bool Ppu::TextBgDot(int index, int sourceX, int sourceY, u16& color) const
	{
		// The BG control registers are at 0x008, 0x00A, 0x00C and 0x00E.
		const u16 control = Read16(0x008 + index * 2, 0);

		// A text map is a whole number of 256x256 dot areas of 32x32 entries: size 0 is one area,
		// size 1 is two side by side (512x256), size 2 is two stacked (256x512) and size 3 is a
		// 2x2 arrangement (512x512) (GBATEK 4000008h). SC0 is at the map base, SC1 at +2K, SC2 at
		// +4K and SC3 at +6K.
		const int size = (control >> BGCNT_SIZE) & 3;
		const int mapWidthTiles = 32 << (size & 1);
		const int mapHeightTiles = 32 << ((size >> 1) & 1);
		const int mapWidthDots = mapWidthTiles * 8;
		const int mapHeightDots = mapHeightTiles * 8;

		// Bits 2-3 are the character base block; bit 7 is the colour depth and must not leak into
		// it (GBATEK 4000008h).
		const u32 charBase = (u32)(((control >> BGCNT_CHAR_BASE) & 3) * 0x4000);
		const u32 screenBase = (u32)(((control >> BGCNT_SCREEN_BASE) & 0x1F) * 0x800);

		// The scroll offsets are applied and the map wraps through its own size (GBATEK 4000008h:
		// "When the screen is scrolled it'll always wraparound").
		const int sx = (sourceX + Read16(0x010 + index * 4, 0)) & (mapWidthDots - 1);
		const int sy = (sourceY + Read16(0x012 + index * 4, 0)) & (mapHeightDots - 1);

		// The map entry: bits 0-9 the tile number, bit 10 the horizontal flip, bit 11 the vertical
		// flip and bits 12-15 the palette (GBATEK "Text BG Screen"). The map is made of 256x256
		// dot areas of 32x32 entries that sit one after the other in memory: with two areas side
		// by side, the source dot 256..511 is SC1 (2 KByte further on), which is why the block is
		// selected from which 256-dot area the source lands in and not from the tile column of a
		// linear row.
		const int blockRow = sy / 256;
		const int blockColumn = sx / 256;
		const u32 blocksPerRow = (u32)(mapWidthDots / 256);
		const u32 mapOffset = ((u32)blockRow * blocksPerRow + (u32)blockColumn) * 0x800 +
			(u32)((sy % 256) / 8) * 32 * 2 + (u32)((sx % 256) / 8) * 2;
		const u16 entry = ReadVram16(*this, screenBase + mapOffset);

		const int tileNumber = entry & 0x3FF;
		const int paletteIndex = (entry >> 12) & 0xF;
		int tileX = sx & 7;
		int tileY = sy & 7;
		if (entry & 0x400) tileX = 7 - tileX;
		if (entry & 0x800) tileY = 7 - tileY;

		if ((control & (1 << BGCNT_256_COLORS)) != 0)
		{
			// 8bit depth: 64 bytes per tile, one byte per dot, 256 colours of palette 0.
			const u8 pixelIndex = ReadVram(charBase + (u32)tileNumber * TILE_SIZE_8BPP +
				(u32)tileY * 8 + (u32)tileX);

			if (pixelIndex == 0)
				return false;				// colour 0 is transparent

			color = ReadPaletteEntry(*this, (u32)pixelIndex * 2);
			return true;
		}

		// 4bit depth: 32 bytes per tile, the low nibble is the left dot.
		const u8 packed = ReadVram(charBase + (u32)tileNumber * TILE_SIZE_4BPP +
			(u32)tileY * 4 + (u32)(tileX / 2));
		const u8 pixelIndex = (tileX & 1) ? (u8)(packed >> 4) : (u8)(packed & 0xF);

		if (pixelIndex == 0)
			return false;

		color = ReadPaletteEntry(*this, (u32)(paletteIndex * 16 + pixelIndex) * 2);
		return true;
	}

	bool Ppu::AffineBgDot(const LineState& state, int index, int sourceX, int sourceY, u16& color) const
	{
		// GBATEK "LCD I/O BG Rotation/Scaling": the reference point is the source coordinate of
		// the upper-left dot of the display, PA/PC (dx/dy) are the increments along a scanline and
		// PB/PD (dmx/dmy) the increments from one scanline to the next.
		const u16 control = Read16(0x008 + index * 2, 0);
		const u32 charBase = (u32)(((control >> BGCNT_CHAR_BASE) & 3) * 0x4000);
		const u32 screenBase = (u32)(((control >> BGCNT_SCREEN_BASE) & 0x1F) * 0x800);
		const int mapDots = 128 << ((control >> BGCNT_SIZE) & 3);
		const int mapTiles = mapDots / 8;
		const bool wrap = (control & (1 << BGCNT_AREA_OVERFLOW)) != 0;

		// BG2 uses the first set of affine registers (0x020..0x02F) and BG3 the second
		// (0x030..0x03F). The reference point to sample from is the *internal* register: it is
		// copied from the write latch during VBlank and then advanced by PB/PD after every
		// scanline (GBATEK "Internal Reference Point Registers"). Reading the latch here would
		// throw the per-line advance away and paint every line from the same origin.
		const int slot = index - 2;
		const u32 base = 0x020 + (u32)slot * 0x10;
		const s32 pa = (s16)Read16(base + 0, 0);
		const s32 pb = (s16)Read16(base + 2, 0);
		const s32 pc = (s16)Read16(base + 4, 0);
		const s32 pd = (s16)Read16(base + 6, 0);
		const s32 referenceX = bgx[slot];
		const s32 referenceY = bgy[slot];

		// The reference point is the origin of the *current* scanline, so the dot is offset from
		// it by the dot's position within the line. A mosaic block that starts above this line
		// moves the sample up by the lines it spans, which is what the negative offsetY does.
		const int offsetX = sourceX;
		const int offsetY = sourceY - state.line;

		// (srcX, srcY) = reference + M * (screen - mosaic origin). Both the reference point and
		// the matrix parameters are 8.8 fixed point (GBATEK 4000020h: PA-PD have a "fractional
		// portion" of 8 bits; 4000028h: the reference point is "shifted left by eight"), so the
		// products already carry the reference point's units.
		const s32 srcX = referenceX + pa * offsetX + pb * offsetY;
		const s32 srcY = referenceY + pc * offsetX + pd * offsetY;

		// The hardware samples the texel nearest to the computed point.
		const s32 texelX = ReferenceRounded(srcX);
		const s32 texelY = ReferenceRounded(srcY);

		if (IsBitmapMode(state.mode))
		{
			// GBATEK "LCD VRAM Bitmap BG" and 4000020h: the bitmap modes have no rotation or
			// scaling at all. BG2PA-PD and the reference point registers belong to the *tile*
			// affine modes 1 and 2; in modes 3/4/5 the frame buffer is displayed 1:1, so the
			// source dot is the screen dot. (Routing a bitmap mode through the matrix makes every
			// dot of a fresh machine read (0,0) - the registers reset to zero - and the whole
			// screen falls back to the backdrop.)
			color = BitmapPixel(*this, sourceX, sourceY);
			return color != NO_PIXEL;
		}

		s32 wrappedX = texelX;
		s32 wrappedY = texelY;

		if (wrap)
		{
			// The area overflow bit (BGxCNT bit 13) repeats the map through its own size.
			wrappedX &= mapDots - 1;
			wrappedY &= mapDots - 1;
		}
		else if (texelX < 0 || texelX >= mapDots || texelY < 0 || texelY >= mapDots)
		{
			return false;					// outside of the map, i.e. transparent
		}

		// A rotation/scaling map is one byte per entry and always 256 colours (GBATEK
		// "Rotation/Scaling BG Screen").
		const u8 tileNumber = ReadVram(screenBase + (u32)((wrappedY / 8) * mapTiles + wrappedX / 8));
		const u8 pixelIndex = ReadVram(charBase + (u32)tileNumber * TILE_SIZE_8BPP +
			(u32)(wrappedY & 7) * 8 + (u32)(wrappedX & 7));

		if (pixelIndex == 0)
			return false;

		color = ReadPaletteEntry(*this, (u32)pixelIndex * 2);
		return true;
	}

	void Ppu::ApplyDotEffect(const LineState& state, int x, const LayerBelow& below)
	{
		// GBATEK "LCD I/O Color Special Effects".
		const LayerPixel& top = pixels[x];
		u16 color = top.color;

		const int effect = (state.bldcnt >> BLD_EFFECT) & 3;

		// The coefficients saturate at 16/16 (GBATEK 4000052h/4000054h).
		int eva = state.bldalpha & 0x1F;
		int evb = (state.bldalpha >> 8) & 0x1F;
		int evy = state.bldy & 0x1F;
		if (eva > 16) eva = 16;
		if (evb > 16) evb = 16;
		if (evy > 16) evy = 16;

		// A semi-transparent OBJ is always a 1st target and always uses alpha blending, whatever
		// BLDCNT says; the brightness effect must not take place for it (GBATEK "Semi-Transparent
		// OBJs").
		const bool semi = (top.layer == LAYER_OBJ) && top.semiTransparent;
		const bool firstTarget = semi || ((state.bldcnt & (1 << top.layer)) != 0);

		// The 2nd target is the dot directly below the top-most one. The backdrop is opaque
		// everywhere, so a dot that no layer covers has it as its next lower pixel.
		const bool hasBelow = (below.layer != LAYER_NONE);
		const u8 secondLayer = hasBelow ? below.layer : LAYER_BACKDROP;
		const u16 secondColor = hasBelow ? below.color : top.color;

		// The colour special effect is enabled per window region by bit 5 of WININ/WINOUT
		// (GBATEK 4000048h/400004Ah).
		const u16 windowBits = state.windowsActive
			? WindowBits(state.windowMask[x], state.winin, state.winout) : WINDOW_ALL;
		const bool effectAllowed = (windowBits & (1 << WIN_EFFECT)) != 0;

		bool hasSecond = false;
		u16 second = 0;

		if (semi)
		{
			// A semi-transparent OBJ is alpha blended with whatever it covers, and that
			// semi-transparency then wins over the brightness effect of BLDCNT.
			hasSecond = true;
			second = secondColor;
		}
		else if (effect == 1 && firstTarget &&
			(state.bldcnt & (1 << (BLD_2ND + secondLayer))) != 0)
		{
			// "For this effect, the top-most non-transparent pixel must be selected as 1st
			// Target, and the next-lower non-transparent pixel must be selected as 2nd Target,
			// if so - and only if so, then color intensities of 1st and 2nd Target are mixed"
			// (GBATEK 4000050h). The 1st target does not have to be a 2nd target as well.
			hasSecond = true;
			second = secondColor;
		}

		if (hasSecond)
		{
			// I = MIN (31, I1st*EVA + I2nd*EVB) per colour component; Blend15 saturates.
			color = Blend15(color, second, eva, evb);
		}
		else if (firstTarget && effectAllowed)
		{
			if (effect == 2)
			{
				// Brightness increase: I = I1st + (31-I1st)*EVY/16 (GBATEK 4000054h).
				int r = color & 0x1F;
				int g = (color >> 5) & 0x1F;
				int b = (color >> 10) & 0x1F;
				r += ((31 - r) * evy) >> 4;
				g += ((31 - g) * evy) >> 4;
				b += ((31 - b) * evy) >> 4;
				color = (u16)(r | (g << 5) | (b << 10));
			}
			else if (effect == 3)
			{
				// Brightness decrease: I = I1st - I1st*EVY/16 (GBATEK 4000054h).
				int r = color & 0x1F;
				int g = (color >> 5) & 0x1F;
				int b = (color >> 10) & 0x1F;
				r -= (r * evy) >> 4;
				g -= (g * evy) >> 4;
				b -= (b * evy) >> 4;
				color = (u16)(r | (g << 5) | (b << 10));
			}
		}

		line[x] = color;
	}
}
