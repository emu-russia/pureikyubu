// The Game Boy LCD controller. See gb_ppu.h for the specifications and for the deviations this
// implementation makes from the dot exact hardware.
//
// The structure is the same as the GBA side of the emulator: the LCD is a small state machine
// over 456-dot scanlines, and a visible scanline is composed when its mode 3 ends. What differs
// from the GBA is everything the DMG/CGB does that the GBA's LCD does not: the window's own line
// counter, the ten-sprite limit with the two different priority rules, the per-tile CGB
// attributes in the second VRAM bank, and the STAT interrupt line whose rising edge - not its
// level - requests the interrupt.

#include "gb_ppu.h"

#include <cstdio>

namespace GBA
{
	// The four shades of the monochrome picture, as the frontend draws them. Pan Docs gives the
	// abstract palette (0 = white .. 3 = black) but no RGB triplets; the well known DMG green
	// (0x9BBC0F .. 0x0F380F) is used for GbPalette::Green and the other schemes are presentation
	// choices for the frontend, not hardware values.
	namespace
	{
		const u32 Shades[4][4] =
		{
			// Green (the original DMG LCD)
			{ 0xFF9BBC0F, 0xFF8BAC0F, 0xFF306230, 0xFF0F380F },
			// Grey (the Game Boy Pocket / Light)
			{ 0xFFFFFFFF, 0xFFAAAAAA, 0xFF555555, 0xFF000000 },
			// Amber
			{ 0xFFFFE9B0, 0xFFE0B060, 0xFF905020, 0xFF301000 },
			// Blue
			{ 0xFFE0F0FF, 0xFF80B0E0, 0xFF305080, 0xFF001020 },
		};

		// The blank LCD is "white, whiter than colour #0" (Pan Docs "LCDC" bit 7).
		const u32 BlankWhite = 0xFFFFFFFF;
	}

	const char* GbPpu::PaletteName(GbPalette palette)
	{
		switch (palette)
		{
		case GbPalette::Green: return "green";
		case GbPalette::Grey: return "grey";
		case GbPalette::Amber: return "amber";
		default: return "blue";
		}
	}

	void GbPpu::Reset()
	{
		// The register values the boot ROM leaves behind (Pan Docs "Power Up Sequence"):
		// LCDC = 0x91, BGP = 0xFC, STAT = 0x85, the rest zero. OBP0/OBP1 are left uninitialised
		// by the real boot ROM; this emulator starts them at 0xFF (the erased value).
		lcdc = 0x91;
		stat = 0x85;
		scy = 0x00;
		scx = 0x00;
		ly = 0x00;
		lyc = 0x00;
		wy = 0x00;
		wx = 0x00;
		bgp = 0xFC;
		obp0 = 0xFF;
		obp1 = 0xFF;
		vbk = 0xFE;
		bgpi = 0x00;
		obpi = 0x00;

		for (int i = 0; i < 64; i++)
		{
			bgPalette[i] = 0x7F;		// the boot ROM fades the BG palettes to white
			objPalette[i] = 0x7F;		// the OBJ colours are left uninitialised
		}

		for (int bank = 0; bank < 2; bank++)
			for (int i = 0; i < 0x2000; i++)
				vram[bank][i] = 0x00;
		for (int i = 0; i < 0xA0; i++)
			oam[i] = 0x00;

		dots = 0;
		mode = 2;
		modeEndDots = 80;
		lcdEnabled = true;
		frameCounter = 0;
		statLine = false;
		lastMode3Length = 172;
		windowActive = false;
		windowLine = 0;
		lineSpriteCount = 0;

		frame.assign((size_t)GbScreenWidth * GbScreenHeight, BlankWhite);
		for (int x = 0; x < GbScreenWidth; x++)
			line[x] = BlankWhite;

		BeginVisibleLine();
		RefreshStat();
	}

	int GbPpu::LineSpriteIndex(int n) const
	{
		if (n < 0 || n >= lineSpriteCount)
			return -1;
		return lineSprites[n].index;
	}

	// ---------------------------------------------------------------------------------------
	// The register file
	// ---------------------------------------------------------------------------------------

	void GbPpu::SetGreyscalePalettes()
	{
		// The four shades as RGB555 (Pan Docs "Palettes": a CGB colour is 15 bits, five per
		// channel, stored little-endian). White, light grey, dark grey and black.
		const u16 shades[4] = { 0x7FFF, 0x5294, 0x294A, 0x0000 };

		for (int palette = 0; palette < 8; palette++)
		{
			for (int index = 0; index < 4; index++)
			{
				int at = palette * 8 + index * 2;
				bgPalette[at] = (u8)(shades[index] & 0xFF);
				bgPalette[at + 1] = (u8)(shades[index] >> 8);
				objPalette[at] = (u8)(shades[index] & 0xFF);
				objPalette[at + 1] = (u8)(shades[index] >> 8);
			}
		}
	}

	void GbPpu::SetLcdEnabled(bool enabled)
	{
		// The same side effects as writing LCDC bit 7, used by the machine to put the LCD into
		// its power-up state (off) before the boot ROM starts.
		if (lcdEnabled == enabled)
			return;

		lcdEnabled = enabled;
		dots = 0;
		ly = 0;
		windowActive = false;
		windowLine = 0;

		if (enabled)
		{
			lcdc |= 0x80;
			BeginVisibleLine();
		}
		else
		{
			lcdc &= (u8)~0x80;
			mode = 0;
			modeEndDots = LineStart() + GbDotsPerLine;
			statLine = false;
		}
		RefreshStat();
	}

	u8 GbPpu::ReadRegister(u16 address) const
	{
		switch (address)
		{
		case 0xFF40: return lcdc;
		case 0xFF41:
			// Bits 1-0 (the mode) read 0 while the PPU is disabled (Pan Docs "STAT"), bit 7 is
			// unused and reads back as one on hardware.
			if (!lcdEnabled)
				return (u8)((stat & 0xF8) | 0x80);
			return (u8)(stat | 0x80);
		case 0xFF42: return scy;
		case 0xFF43: return scx;
		case 0xFF44: return lcdEnabled ? ly : 0x00;
		case 0xFF45: return lyc;
		case 0xFF47: return bgp;
		case 0xFF48: return obp0;
		case 0xFF49: return obp1;
		case 0xFF4A: return wy;
		case 0xFF4B: return wx;
		case 0xFF4F: return cgb ? vbk : 0xFF;
		case 0xFF68: return cgb ? bgpi : 0xFF;
		case 0xFF69:
			if (cgb && !CramBlocked())
				return bgPalette[bgpi & 0x3F];
			return 0xFF;
		case 0xFF6A: return cgb ? obpi : 0xFF;
		case 0xFF6B:
			if (cgb && !CramBlocked())
				return objPalette[obpi & 0x3F];
			return 0xFF;
		default:
			return 0xFF;
		}
	}

	void GbPpu::WriteRegister(u16 address, u8 value)
	{
		switch (address)
		{
		case 0xFF40:
		{
			// LCDC (Pan Docs "LCDC"). The only bit with a side effect beyond the picture is
			// bit 7: turning the LCD off resets LY to 0 and the mode to 0 and stops the LCD.
			bool wasEnabled = lcdEnabled;
			lcdc = value;
			lcdEnabled = (value & 0x80) != 0;

			// Pan Docs "Window" note: on a CGB, clearing the window enable bit resets the
			// window's Y condition, so WY must be reached again before the window can come back.
			if (cgb && (value & 0x20) == 0)
				windowActive = false;

			if (wasEnabled && !lcdEnabled)
			{
				dots = 0;
				ly = 0;
				mode = 0;
				modeEndDots = LineStart() + GbDotsPerLine;
				windowActive = false;
				windowLine = 0;
				statLine = false;
			}
			else if (!wasEnabled && lcdEnabled)
			{
				// Re-enabling restarts the LCD at the top of the frame. Pan Docs notes that the
				// screen stays blank for the first frame; this implementation draws it at once.
				dots = 0;
				ly = 0;
				windowActive = false;
				windowLine = 0;
				BeginVisibleLine();
			}
			RefreshStat();
			break;
		}

		case 0xFF41:
			// Only bits 3-6 are writable; bits 0-2 belong to the PPU (Pan Docs "STAT"). The
			// monochrome "spurious interrupt on a STAT write" quirk is not modelled.
			stat = (u8)((stat & 0x07) | (value & 0x78));
			RefreshStat();
			break;

		case 0xFF42: scy = value; break;
		case 0xFF43: scx = value; break;
		case 0xFF44: break;				// LY is read-only; a write is ignored
		case 0xFF45:
			// LYC is re-compared "constantly" (Pan Docs "STAT"), so a write can raise the STAT
			// line immediately and request an interrupt.
			lyc = value;
			RefreshStat();
			break;
		case 0xFF47: bgp = value; break;
		case 0xFF48: obp0 = value; break;
		case 0xFF49: obp1 = value; break;
		case 0xFF4A: wy = value; break;
		case 0xFF4B: wx = value; break;

		case 0xFF4F:
			if (cgb)
				vbk = (u8)(0xFE | (value & 0x01));
			break;

		case 0xFF68:
			if (cgb)
				bgpi = (u8)(value & 0xBF);
			break;
		case 0xFF69:
			// The data register is inaccessible during mode 3: the write is ignored, but the
			// auto increment still happens (Pan Docs "Palettes", an explicit quirk).
			if (cgb)
			{
				if (!CramBlocked())
					bgPalette[bgpi & 0x3F] = value;
				if (bgpi & 0x80)
					bgpi = (u8)(0x80 | ((bgpi + 1) & 0x3F));
			}
			break;

		case 0xFF6A:
			if (cgb)
				obpi = (u8)(value & 0xBF);
			break;
		case 0xFF6B:
			if (cgb)
			{
				if (!CramBlocked())
					objPalette[obpi & 0x3F] = value;
				if (obpi & 0x80)
					obpi = (u8)(0x80 | ((obpi + 1) & 0x3F));
			}
			break;

		default:
			break;
		}
	}

	// ---------------------------------------------------------------------------------------
	// STAT and the interrupt line
	// ---------------------------------------------------------------------------------------

	bool GbPpu::StatLineLevel() const
	{
		// Pan Docs "Interrupt Sources": each enabled source's state (inactive low, active high)
		// is ORed into one shared line, and the interrupt is requested on a rising edge.
		bool level = false;
		int currentMode = lcdEnabled ? mode : 0;

		if ((stat & 0x08) && currentMode == 0)		// mode 0 (HBlank) selected
			level = true;
		if ((stat & 0x10) && currentMode == 1)		// mode 1 (VBlank) selected
			level = true;
		if ((stat & 0x20) && currentMode == 2)		// mode 2 (OAM scan) selected
			level = true;
		if ((stat & 0x40) && (ly == lyc))			// LYC = LY selected
			level = true;

		return level;
	}

	void GbPpu::RefreshStat()
	{
		// Bit 2 tracks LY = LYC, bits 1-0 the mode (both are read-only).
		stat = (u8)(stat & 0x78);
		if (ly == lyc)
			stat |= 0x04;
		if (lcdEnabled)
			stat |= (u8)(mode & 0x03);
		statLine = StatLineLevel();
	}

	u8 GbPpu::UpdateStatLine()
	{
		// Bit 2 is "constantly" updated (Pan Docs "STAT").
		if (ly == lyc)
			stat |= 0x04;
		else
			stat &= (u8)~0x04;

		bool level = StatLineLevel();
		u8 request = 0;
		if (level && !statLine)
			request = 0x02;			// a low to high transition of the STAT line
		statLine = level;
		return request;
	}

	u8 GbPpu::EnterMode(int newMode)
	{
		mode = newMode;
		stat = (u8)((stat & 0x7C) | (newMode & 0x03));
		return UpdateStatLine();
	}

	// ---------------------------------------------------------------------------------------
	// The scanline state machine
	// ---------------------------------------------------------------------------------------

	void GbPpu::BeginVisibleLine()
	{
		// Mode 2 is the OAM scan (Pan Docs "Rendering": 80 dots). The deadline is stored as an
		// absolute dot count so a caller can stop ticking in the middle of a mode safely.
		EnterMode(2);
		modeEndDots = LineStart() + 80;
		ScanOam();

		// The window's "Y condition" (Pan Docs "Window"): cleared on each VBlank, set at the
		// beginning of a scanline when WY = LY and held for the rest of the frame.
		if (lcdEnabled && (unsigned)ly == wy && ly < GbVisibleLines)
			windowActive = true;
	}

	void GbPpu::ScanOam()
	{
		// Pan Docs "OAM": during mode 2 the PPU compares LY against every object's Y byte, in
		// OAM order, and keeps the first ten that match. Only Y is compared, so an object that
		// is hidden by its X still takes a slot.
		int height = (lcdc & 0x04) ? 16 : 8;
		lineSpriteCount = 0;

		if (!lcdEnabled)
			return;

		for (int i = 0; i < 40 && lineSpriteCount < 10; i++)
		{
			int objectY = (int)oam[i * 4 + 0] - 16;
			if (ly < objectY || ly >= objectY + height)
				continue;

			LineSprite& sprite = lineSprites[lineSpriteCount++];
			sprite.index = i;
			sprite.x = (int)oam[i * 4 + 1] - 8;
			sprite.tile = oam[i * 4 + 2];
			sprite.attributes = oam[i * 4 + 3];
			sprite.row = ly - objectY;
		}
	}

	int GbPpu::Mode3Length()
	{
		// Pan Docs "Rendering": mode 3 is 160 pixels plus 12 dots for the two initial tile
		// fetches, plus SCX % 8 for the fine scroll, plus 6 dots when the window is switched in,
		// plus 6..11 dots for every object on the line. The documented range is 172..289.
		int length = 160 + 12 + (scx & 0x07);

		bool windowRenders = windowActive && (lcdc & 0x20) && wx >= 7 && wx <= 166;
		if (windowRenders)
			length += 6;

		for (int i = 0; i < lineSpriteCount; i++)
		{
			// Pan Docs "Rendering" footnote: an object whose OAM X is 0 (fully off the left
			// side) always costs 11 dots regardless of SCX; otherwise 6..11.
			if (oam[lineSprites[i].index * 4 + 1] == 0)
				length += 11;
			else
				length += 11 - (lineSprites[i].x & 0x07);
		}

		if (length < 172)
			length = 172;
		if (length > 289)
			length = 289;

		lastMode3Length = length;
		return length;
	}

	u8 GbPpu::Tick(int cycles, bool doubleSpeed)
	{
		// A dot is four of the CPU's clocks in normal speed and two in double speed (Pan Docs
		// "Rendering": the dot clock is 2^22 Hz and the CPU runs at twice that in double speed).
		int dotsToRun = doubleSpeed ? (cycles / 2) : (cycles / 4);
		u8 request = 0;

		while (true)
		{
			if (!lcdEnabled)
			{
				// With the LCD off LY stays 0 and the mode reads 0; nothing advances.
				return request;
			}

			if (dots >= modeEndDots)
			{
				// A mode boundary. A line is 2, 3, 0, and a frame is 144 such lines followed by
				// ten VBlank lines (mode 1) whose LY is 144..153. Every mode's deadline is an
				// absolute dot count, so no arithmetic on the line offset can go wrong.
				if (mode == 2)
				{
					request |= EnterMode(3);
					modeEndDots = LineStart() + 80 + (u64)Mode3Length();
				}
				else if (mode == 3)
				{
					RenderLine();
					request |= EnterMode(0);
					modeEndDots = LineStart() + GbDotsPerLine;
				}
				else if (mode == 0)
				{
					// The line is complete: publish it and move to LY + 1.
					for (int x = 0; x < GbScreenWidth; x++)
						frame[(size_t)ly * GbScreenWidth + x] = line[x];

					ly++;
					if (ly == GbVisibleLines)
					{
						request |= EnterMode(1);
						request |= 0x01;		// the VBlank interrupt, requested at LY = 144

						// LY = 144 lasts a whole scanline, so mode 1's first deadline has to be
						// set here as well; without it the PPU would step straight from 143 to
						// 145 and a program polling LY for VBlank would never see it.
						modeEndDots = LineStart() + GbDotsPerLine;

						// The window's Y condition is cleared on each VBlank (Pan Docs "Window").
						windowActive = false;
						windowLine = 0;
					}
					else
					{
						BeginVisibleLine();
					}
				}
				else
				{
					// Mode 1: LY 144..153. The frame ends after LY 153.
					ly++;
					if (ly > 153)
					{
						ly = 0;
						frameCounter++;
						windowLine = 0;
						BeginVisibleLine();
					}
					else
					{
						// The mode does not change during VBlank, but LYC may now match LY.
						request |= UpdateStatLine();
						modeEndDots = LineStart() + GbDotsPerLine;
					}
				}
				continue;
			}

			if (dotsToRun <= 0)
				break;

			u64 remaining = modeEndDots - dots;
			if (remaining > (u64)dotsToRun)
				remaining = (u64)dotsToRun;

			dots += remaining;
			dotsToRun -= (int)remaining;
		}

		return request;
	}

	// ---------------------------------------------------------------------------------------
	// Composing a scanline
	// ---------------------------------------------------------------------------------------

	u8 GbPpu::FetchBgPixel(int x, int y, int& palette, u8& attributes)
	{
		// Pan Docs "Scrolling" / "Tile Maps" / "pixel_fifo": the BG tile is chosen with the
		// scrolled 16-bit coordinate, masked to the 256 x 256 pixel map (the map wraps by
		// masking the tile X with 31 and the pixel Y with 255).
		int mapX, mapY, mapBase, pixelY;

		bool windowHere = windowActive && (lcdc & 0x20) && wx >= 7 && wx <= 166 && x >= wx - 7;
		if (windowHere)
		{
			// The window's top-left pixel is (WX - 7, WY); it has its own line counter and does
			// not use SCY (Pan Docs "Window" / "Tile Maps").
			mapX = (x - (wx - 7)) & 0xFF;
			mapY = windowLine & 0xFF;
			mapBase = (lcdc & 0x40) ? 0x1C00 : 0x1800;
		}
		else
		{
			mapX = (x + scx) & 0xFF;
			mapY = (y + scy) & 0xFF;
			mapBase = (lcdc & 0x08) ? 0x1C00 : 0x1800;
		}

		pixelY = mapY & 0x07;

		int tileX = (mapX >> 3) & 0x1F;
		int tileY = (mapY >> 3) & 0x1F;
		int mapAddress = mapBase + tileY * 32 + tileX;

		attributes = 0;
		palette = 0;
		int tileBank = 0;

		if (cgb)
		{
			// VRAM bank 1 holds the attribute byte for every map entry (Pan Docs "Tile Maps").
			attributes = vram[1][mapAddress];

			// Bit 6 is the Y flip, bit 5 the X flip (both apply to the tile's pixels).
			if (attributes & 0x40)
				pixelY = 7 - pixelY;
			if (attributes & 0x20)
				mapX = (mapX & ~0x07) | (7 - (mapX & 0x07));

			palette = attributes & 0x07;
			tileBank = (attributes & 0x08) ? 1 : 0;
		}

		int tileIndex = vram[0][mapAddress];

		// The tile data select (LCDC bit 4): 0 = the 0x8800 method (a signed index relative to
		// 0x9000), 1 = the 0x8000 method (an unsigned index).
		int tileAddress;
		if (lcdc & 0x10)
			tileAddress = tileIndex * 16;
		else
			tileAddress = 0x1000 + ((s8)tileIndex) * 16;

		tileAddress += pixelY * 2;
		if (tileAddress < 0 || tileAddress + 1 >= 0x2000)
			return 0;

		u8 low = vram[tileBank][tileAddress];
		u8 high = vram[tileBank][tileAddress + 1];

		// Bit 7 of each byte is the leftmost pixel; the high plane is the second bit of the
		// colour index (Pan Docs "Tile Data").
		int bit = mapX & 0x07;
		return (u8)((((high >> (7 - bit)) & 1) << 1) | ((low >> (7 - bit)) & 1));
	}

	void GbPpu::RenderLine()
	{
		int y = ly;
		bool tall = (lcdc & 0x04) != 0;
		int height = tall ? 16 : 8;

		// The object priority order. On a monochrome console the object with the smaller X wins
		// and ties are broken by the OAM index; on a CGB only the OAM index matters (Pan Docs
		// "OAM": "the smaller the X coordinate, the higher the priority" vs "only the object's
		// location in OAM determines its priority"). This is a small insertion sort over at most
		// ten entries.
		int order[10];
		for (int i = 0; i < lineSpriteCount; i++)
			order[i] = i;
		for (int i = 1; i < lineSpriteCount; i++)
		{
			int key = order[i];
			int j = i - 1;
			while (j >= 0)
			{
				bool swap;
				if (cgb)
					swap = lineSprites[order[j]].index > lineSprites[key].index;
				else if (lineSprites[order[j]].x != lineSprites[key].x)
					swap = lineSprites[order[j]].x > lineSprites[key].x;
				else
					swap = lineSprites[order[j]].index > lineSprites[key].index;
				if (!swap)
					break;
				order[j + 1] = order[j];
				j--;
			}
			order[j + 1] = key;
		}

		for (int x = 0; x < GbScreenWidth; x++)
		{
			int bgPalette = 0;
			u8 bgAttributes = 0;
			int bgIndex = 0;

			if (cgb || (lcdc & 0x01) || (windowActive && (lcdc & 0x20)))
				bgIndex = FetchBgPixel(x, y, bgPalette, bgAttributes);

			// On a monochrome console a clear LCDC bit 0 blanks the background and the window to
			// colour 0 (Pan Docs "LCDC": "both background and window become blank"); on a CGB
			// the bit is only the master priority.
			if (!cgb && !(lcdc & 0x01))
				bgIndex = 0;

			int index = bgIndex;
			int palette = bgPalette;		// the CGB palette the attribute map selected
			bool fromObject = false;
			bool bgHasPriority = false;

			if ((lcdc & 0x02) && lineSpriteCount != 0)
			{
				// Pan Docs "OAM": object-to-object priority is resolved first (the first
				// non-transparent pixel in drawing order), and only then is the chosen object's
				// BG-over-OBJ flag consulted - so a high priority object with the flag set masks
				// the lower priority ones.
				for (int n = 0; n < lineSpriteCount; n++)
				{
					const LineSprite& sprite = lineSprites[order[n]];
					if (x < sprite.x || x >= sprite.x + 8)
						continue;

					int objectRow = sprite.row;
					if (sprite.attributes & 0x40)
						objectRow = height - 1 - objectRow;		// the Y flip

					int tileIndex = sprite.tile;
					if (tall)
						tileIndex = (tileIndex & 0xFE) | ((objectRow >= 8) ? 1 : 0);

					int objectBank = (cgb && (sprite.attributes & 0x08)) ? 1 : 0;
					int column = x - sprite.x;
					if (sprite.attributes & 0x20)
						column = 7 - column;					// the X flip

					int tileAddress = tileIndex * 16 + (objectRow & 0x07) * 2;
					if (tileAddress + 1 >= 0x2000)
						continue;

					u8 low = vram[objectBank][tileAddress];
					u8 high = vram[objectBank][tileAddress + 1];
					int bit = 7 - column;
					int objectIndex = (((high >> bit) & 1) << 1) | ((low >> bit) & 1);

					if (objectIndex == 0)
						continue;			// colour index 0 is transparent for objects

					index = objectIndex;
					palette = cgb ? (sprite.attributes & 0x07) : ((sprite.attributes & 0x10) ? 1 : 0);
					fromObject = true;

					// The BG-over-OBJ flag (Pan Docs "OAM" byte 3 bit 7), and the CGB three-flag
					// table (Pan Docs "Tile Maps"): the BG wins when LCDC.0 is set, at least one
					// of the two priority flags is set, and the BG colour index is not 0.
					if (sprite.attributes & 0x80)
						bgHasPriority = true;
					break;
				}
			}

			if (fromObject)
			{
				// Pan Docs "OAM" (the BG-over-OBJ flag) and "Tile Maps" (the CGB three-flag
				// table): on a CGB the background wins when LCDC bit 0 is set, the BG colour
				// index is not zero and *either* the object's or the background's priority bit is
				// set. On a monochrome console only the object's flag matters.
				bool bgWins;
				if (cgb)
					bgWins = (lcdc & 0x01) && bgIndex != 0 && (bgHasPriority || (bgAttributes & 0x80));
				else
					bgWins = bgHasPriority && bgIndex != 0;

				if (bgWins)
				{
					index = bgIndex;
					palette = cgb ? (bgAttributes & 0x07) : 0;
					fromObject = false;
				}
			}

			line[x] = ShadePixel(index, palette, fromObject ? 0 : bgAttributes, fromObject);
		}

		// The window's own line counter advances when the window actually rendered, and then the
		// background fetcher is reset - Pan Docs: the counter "only gets incremented when the
		// window is visible".
		bool windowRendered = windowActive && (lcdc & 0x20) && wx >= 7 && wx <= 166 && wx - 7 < GbScreenWidth;
		if (cgb && !(lcdc & 0x20))
			windowActive = false;		// the CGB resets the Y condition when the window is off
		if (windowRendered)
			windowLine = (windowLine + 1) & 0xFF;
	}

	// ---------------------------------------------------------------------------------------
	// Colours
	// ---------------------------------------------------------------------------------------

	u32 GbPpu::CgbColor(int palette, int index, bool object) const
	{
		// Pan Docs "Palettes": 8 palettes of 4 colours, two little-endian bytes each, RGB555 in
		// the low 15 bits (bit 15 is ignored by the rendering).
		const u8* ram = object ? objPalette : bgPalette;
		int at = (palette & 0x07) * 8 + (index & 0x03) * 2;
		u16 color = (u16)(ram[at] | (ram[at + 1] << 8));
		int r = color & 0x1F;
		int g = (color >> 5) & 0x1F;
		int b = (color >> 10) & 0x1F;
		// The usual 5-bit to 8-bit expansion (replicating the top bits into the low ones).
		return PackXrgb((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2));
	}

	u32 GbPpu::DmgShade(int index, int palette, bool object) const
	{
		// BGP/OBP0/OBP1: two bits per colour index (Pan Docs "Palettes"). The lower two bits of
		// OBP0/OBP1 are ignored because colour index 0 is transparent for objects.
		u8 registerValue = object ? (palette == 0 ? obp0 : obp1) : bgp;
		int shade = (registerValue >> (index * 2)) & 0x03;
		return Shades[(int)shadePalette][shade];
	}

	u32 GbPpu::ShadePixel(int index, int palette, u8 attributes, bool object) const
	{
		// A CGB always draws through the colour palette memory; the attribute's bit 7 belongs to
		// the priority logic and is not part of the palette index. A DMG (non-CGB) picture uses
		// BGP for the background and the window and OBP0/OBP1 for the objects.
		if (cgb)
			return CgbColor(palette, index, object);

		(void)attributes;
		return DmgShade(index, palette, object);
	}
}
