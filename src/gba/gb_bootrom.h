// The pureikyubu Game Boy boot ROM: a free replacement for the copyrighted DMG/CGB IPL, built
// from source with the emitter in gb_asm.h and exactly 256 bytes long.
//
// The real boot ROMs are Nintendo's and are not in this repository. This one does what the
// hardware's does in the ways a cartridge can observe, and shows the emulator's name in between:
//
//  * it initialises the sound registers to the values the Pan Docs list for the DMG's post-boot
//    state (NR10 = 0x80 ... NR52 = 0xF1, the "silence everything" setup), sets BGP to 0xFC and
//    leaves LCDC's bit 7 clear until it is ready to show something;
//  * it draws the "pureikyubu" wordmark, one 8x8 sprite per letter, sliding in from off the right
//    edge and settling in the middle of the screen, holds it, and then clears the screen;
//  * it reads the cartridge header and jumps to 0x0100 with the register state a real boot ROM
//    leaves (A = 0x01, C = the header checksum at 0x014D, HL = 0x014D on the CGB, SP = 0xFFFE);
//    with no cartridge it spins on the wordmark instead of jumping.
//
// The image is emulated LR35902 code, so the tests run it on the machine and look at the frames
// the LCD produces; see testing/gba_bench/test_gb_bootrom.cpp.
//
// Where the animation lives in the 256 bytes:
//
//   0x00..0x5F   the code (init, the title, the slide, the hand-off)
//   0x60..0xEF   144 bytes of tile data: nine 8x8 letters (16 bytes each, two bitplanes a row)
//   0xF0..0xF9   the ten target X coordinates of the wordmark's letters
//
// The sliding is done by scrolling one byte of every sprite's OAM entry: the letters all start
// off the right edge, 16 pixels apart, and the OAM X of the whole row advances four pixels a
// frame until each letter reaches its target, so the wordmark slides in and settles. Every loop
// is bounded (ten letters, 160 slide frames), so the ROM always terminates.

#pragma once

#include "gba_types.h"

#include <string>
#include <vector>

namespace GBA
{
	namespace GbBootRom
	{
		/// <summary>The 256 byte DMG boot ROM image.</summary>
		const std::vector<u8>& DmgImage();

		/// <summary>
		/// The 256 byte CGB boot ROM image. It differs from the DMG one only in the register state
		/// it hands over (A = 0x11 for a CGB compatible cartridge on a CGB console): the emulator
		/// does not implement the CGB's compatibility palette tables, so there is no second
		/// animation to run.
		/// </summary>
		const std::vector<u8>& CgbImage();

		/// <summary>How many frames the wordmark animation lasts (the slide plus the hold).</summary>
		int AnimationFrames();

		/// <summary>How many frames the wordmark takes to settle (the slide only).</summary>
		int SlideFrames();

		/// <summary>The 8x8 letter of the wordmark, for the tests: the colour index of every
		/// pixel, left to right and top to bottom.</summary>
		u8 LetterPixel(int letter, int x, int y);

		/// <summary>The number of letters in the wordmark ("pureikyubu" has nine distinct ones
		/// because the two U's share a tile).</summary>
		int LetterCount();

		/// <summary>The declaration order of the tiles (which letter each tile index draws).</summary>
		const char* TileLetters();

		/// <summary>The assembly listing of the image, one line per emitted instruction.</summary>
		const std::string& Listing();

		/// <summary>
		/// Where the parts of the 256 byte image live (the tests check the layout and the report
		/// prints it). The whole ROM is only 256 bytes, so the code and the tables have to share
		/// it; these offsets make the split visible.
		/// </summary>
		struct Layout
		{
			u16 codeEnd = 0;		// the first free byte after the code
			u16 tileData = 0;		// the eight letters, six bytes each (one bitplane a row)
			u16 wordmark = 0;		// the map row that spells out "pureikyubu"
			u16 imageEnd = 0;		// the last byte the ROM uses
		};

		/// <summary>The layout of the image.</summary>
		Layout ImageLayout();
	}
}
