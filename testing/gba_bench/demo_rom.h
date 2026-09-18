// A small GBA demo cartridge, assembled by the harness at run time.
//
// The boot ROM proves that the machine can render and that the cartridge handover works, but it is
// emulator code. This demo is a *cartridge*: a real ARM program in a real cartridge image with a
// real header, written with the same emitter the boot ROM uses (and therefore also a check of the
// emitter's arithmetic). It paints a moving colour field in bitmap mode 3, waits for VBlank with
// the VCOUNT poll a game uses, and leaves a marker in IWRAM so the tests can tell that the program
// ran at all.
//
// The image is built in memory; nothing is written to disk unless the harness is asked to.

#pragma once

#include <vector>

#include "gba_armasm.h"
#include "gba_types.h"

namespace GbaTest
{
	/// <summary>
	/// Emit "rd = value" on the emitter. The emitter can only encode an 8-bit immediate rotated by
	/// an even amount, so a 32-bit constant is built from its byte-aligned pieces, each of which is
	/// always encodable. A test that needs a constant is welcome not to care about this detail.
	/// </summary>
	inline void EmitMovImm32(GBA::ArmAsm::Assembler& a, int rd, uint32_t value)
	{
		bool first = true;

		for (int shift = 0; shift < 32; shift += 8)
		{
			uint32_t piece = (value >> shift) & 0xFF;

			if (piece == 0)
			{
				continue;
			}

			if (first)
			{
				a.Mov(rd, piece << shift);
				first = false;
			}
			else
			{
				a.Orr(rd, rd, piece << shift);
			}
		}

		if (first)
		{
			a.Mov(rd, 0);
		}
	}

	/// <summary>Fill in the cartridge header's complement check (0x080000BD).</summary>
	inline void FixHeaderChecksum(std::vector<uint8_t>& image)
	{
		uint32_t sum = 0x19;

		for (uint32_t i = 0xA0; i <= 0xBC; i++)
		{
			sum += image[i];
		}

		image[0xBD] = (uint8_t)(-(int)sum);
	}

	/// <summary>
	/// Build the demo cartridge: bitmap mode 3, one full-screen repaint per frame,
	/// colour = f(x, y, frame), with the frame counter in IWRAM at 0x03000008 and the markers
	/// 0x00474241 and 0xCAFEF00D at 0x03000000 and 0x03000004.
	/// </summary>
	inline std::vector<uint8_t> BuildDemoRom()
	{
		using namespace GBA::ArmAsm;

		Assembler a;
		// The image is assembled with a zero origin: a cartridge is *loaded* at 0x08000000, and
		// every branch in it is PC-relative, so the addresses the emitter resolves are the image
		// offsets.
		const uint32_t base = 0;

		// The cartridge: the entry branch and the other exception vectors of a cartridge run at
		// 0x08000000, so the vector table is emitted there.
		a.Org(base);
		for (int i = 0; i < 7; i++)
		{
			a.B("entry");
		}

		// The header: the title (0xA0), the game code (0xAC), the maker code (0xB0), the version
		// (0xBC) and the complement check at 0xBD (filled in below). The Nintendo logo area (0x04)
		// is left empty on purpose: the emulator's boot ROM does not verify it, and no copyrighted
		// artwork belongs in this repository.
		a.Org(base + 0xA0);
		a.DataBytes("PUKEIKYUBU DEMO", 15);
		a.DataBytes("PKDX", 4);
		a.DataBytes("01", 2);
		a.Data8(0x96);

		a.Org(base + 0xC0);
		a.Label("entry");

		// r10 = 0x04000000 (the I/O page), r11 = 0x06000000 (VRAM), r8 = 0x0421 (the green step).
		a.Mov(10, 0x04000000);
		a.Mov(11, 0x06000000);
		a.Mov(8, 0x21);
		a.Add(8, 8, 0x400);

		// DISPCNT = 0x0403: mode 3 (a 16-bit bitmap covering the whole screen), BG2 enabled.
		a.Mov(1, 0x400);
		a.Orr(1, 1, 3);
		a.Strh(1, 10, 0);

		// WAITCNT = 0x4317: the usual "fast cartridge" setting a game writes early (prefetch
		// enabled, the shortest second-access times). Without it the CPU is starved by the ROM
		// waitstates - which is correct hardware behaviour, but it makes the demo take several
		// times as long per frame. (A halfword store's offset is only 8 bits wide, so the register
		// address is loaded into a scratch register first.)
		EmitMovImm32(a, 1, 0x4317);
		EmitMovImm32(a, 5, 0x04000204);
		a.Strh(1, 5, 0);

		// The IWRAM markers: a test (and the harness) can tell that the cartridge really ran.
		a.Mov(0, 0x03000000);
		EmitMovImm32(a, 1, 0x00474241);
		a.Str(1, 0, 0);
		EmitMovImm32(a, 1, 0xCAFEF00D);
		a.Str(1, 0, 4);

		a.Mov(6, 0);				// the frame counter

		a.Label("frame");
		a.Mov(4, 0x06000000);		// the VRAM write pointer, advanced by one halfword per pixel
		a.Mov(2, 0);				// y

		a.Label("yloop");
		a.Mov(3, 0);				// x

		a.Label("xloop");
		// red = (x + frame) & 31
		a.And(7, 3, 0x1F);
		a.AddReg(7, 7, 6);
		a.And(7, 7, 0x1F);

		// green = (y + frame) & 31, shifted into place
		a.And(9, 2, 0x1F);
		a.AddReg(9, 9, 6);
		a.And(9, 9, 0x1F);
		a.ShiftReg(9, 9, Shift::LSL, 5);
		a.OrrReg(7, 7, 9);

		// blue = ((x ^ y) + frame) & 31, shifted into place
		a.EorReg(9, 3, 2);
		a.AddReg(9, 9, 6);
		a.And(9, 9, 0x1F);
		a.ShiftReg(9, 9, Shift::LSL, 10);
		a.OrrReg(7, 7, 9);

		a.Strh(7, 4, 0);
		a.Add(4, 4, 2);

		a.Add(3, 3, 1);
		a.Cmp(3, 240);
		a.B("xloop", Cond::LT);

		a.Add(2, 2, 1);
		a.Cmp(2, 160);
		a.B("yloop", Cond::LT);

		// Wait for the start of VBlank and then for its end, and count the frame. The games
		// synchronize to the display this way when they do not use the interrupt.
		a.Label("wait_vblank");
		a.Ldrh(5, 10, 6);			// VCOUNT (0x04000006)
		a.Cmp(5, 160);
		a.B("wait_vblank", Cond::LT);

		a.Label("wait_frame");
		a.Ldrh(5, 10, 6);
		a.Cmp(5, 160);
		a.B("wait_frame", Cond::GE);

		a.Add(6, 6, 1);
		a.Str(6, 0, 8);				// the frame counter, for the harness
		a.B("frame");

		std::vector<uint8_t> image = a.TakeImage(0x10000);
		FixHeaderChecksum(image);

		return image;
	}

	/// <summary>
	/// Build a minimal cartridge whose program writes `magic` to 0x03000000 and then spins. The
	/// boot ROM tests use it to prove that the cartridge handover really reaches the cartridge.
	/// </summary>
	inline std::vector<uint8_t> BuildMarkerRom(uint32_t magic)
	{
		using namespace GBA::ArmAsm;

		Assembler a;
		// The image is assembled with a zero origin: a cartridge is *loaded* at 0x08000000, and
		// every branch in it is PC-relative, so the addresses the emitter resolves are the image
		// offsets.
		const uint32_t base = 0;

		a.Org(base);
		for (int i = 0; i < 7; i++)
		{
			a.B("entry");
		}

		a.Org(base + 0xA0);
		a.DataBytes("PUKEIKYUBU MARK", 15);
		a.DataBytes("PKMK", 4);
		a.DataBytes("01", 2);
		a.Data8(0x96);

		a.Org(base + 0xC0);
		a.Label("entry");
		a.Mov(0, 0x03000000);
		EmitMovImm32(a, 1, magic);
		a.Str(1, 0, 0);

		a.Label("spin");
		a.B("spin");

		std::vector<uint8_t> image = a.TakeImage(0x1000);
		FixHeaderChecksum(image);

		return image;
	}
}
