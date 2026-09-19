// Helpers for the tests that use the official BIOS as the reference.
//
// The emulator cannot ship the official IPL, so its service calls are implemented in the host
// (`src/gba/gba_hlebios.cpp`). That makes the official image the perfect oracle: when the user has
// one in `testing/gba_bench/bios/`, these helpers run the *real* function through its own SWI
// dispatcher so a test can compare it against the host implementation, byte for byte. The image is
// copyrighted and not in the repository, so every test that needs it says so and is skipped
// without it.

#pragma once

#include "gba_test.h"
#include "gba.h"

#include <cstdio>
#include <string>
#include <vector>

namespace GbaProbe
{
	using namespace GBA;

	const uint32_t Stub = 0x02000000;			// the little test routine (EWRAM)
	const uint32_t Work = 0x02001000;			// a SoundArea-sized work area
	const uint32_t Source = 0x02020000;		// the test data
	const uint32_t Output = 0x02021000;		// the destination
	const uint32_t Info = 0x02022000;			// a parameter block

	/// <summary>The BIOS image the user may have put next to the harness ("" when there is none).</summary>
	inline std::string FindBiosImage()
	{
		static const char* candidates[] =
		{
			"testing/gba_bench/bios/gba_bios.bin",
			"../testing/gba_bench/bios/gba_bios.bin",
			"/mnt/c/Work/pureikyubu/testing/gba_bench/bios/gba_bios.bin",
			"bios/gba_bios.bin",
			"gba_bios.bin",
		};

		for (const char* candidate : candidates)
		{
			FILE* f = fopen(candidate, "rb");
			if (f != nullptr)
			{
				fseek(f, 0, SEEK_END);
				long size = ftell(f);
				fclose(f);

				if (size == (long)BiosSize)
					return candidate;
			}
		}

		return "";
	}

	/// <summary>
	/// A machine to call a SWI through: `real` selects the official BIOS (its own dispatcher runs
	/// the function) or the HLE path (the host implements it). Both are called the same way, by
	/// executing a `swi n` instruction from a stub in EWRAM, so a test can compare the two.
	/// </summary>
	class SwiCaller
	{
		GbaSystem system;

	public:
		explicit SwiCaller(bool real)
		{
			GbaSettings settings = GbaSettings::Defaults();
			settings.useCustomBootRom = !real;
			settings.hleBios = !real;

			if (real)
				settings.biosPath = FindBiosImage();

			system.ApplySettings(settings);
			system.Reset();

			if (real)
			{
				// The official BIOS starts with its own boot sequence; let it finish its
				// initialization (POSTFLG is what says it is done) and then take the CPU over,
				// because the boot itself (health screen, key wait) is not what we are testing.
				for (int i = 0; i < 200000 && Bus().Read8(0x04000300) == 0; i++)
					system.RunCycles(16);
			}
		}

		GbaBus& Bus() { return system.Bus(); }

		/// <summary>Call SWI `number` and run until it returns. False when it never came back.</summary>
		bool Call(uint32_t number, uint32_t r0 = 0, uint32_t r1 = 0, uint32_t r2 = 0, uint32_t r3 = 0)
		{
			// The SWI comment field is bits 16-23 of the instruction (GBATEK), and the stub ends
			// in an endless branch, which is where the test waits for the call to come back.
			Bus().Write32(Stub + 0, 0xEF000000u | ((number & 0xFF) << 16));
			Bus().Write32(Stub + 4, 0xEAFFFFFEu);

			system.Cpu().SetReg(0, r0);
			system.Cpu().SetReg(1, r1);
			system.Cpu().SetReg(2, r2);
			system.Cpu().SetReg(3, r3);
			system.Cpu().SetReg(13, 0x03007F00);
			system.Cpu().BranchTo(Stub);

			for (int i = 0; i < 2000000; i++)
			{
				if (system.Cpu().CurrentPC() == Stub + 4)
					return true;
				system.RunCycles(4);
			}

			return false;
		}

		uint32_t R0() { return system.Cpu().Reg(0); }
	};

	/// <summary>The bytes at an address, as a vector.</summary>
	inline std::vector<uint8_t> Bytes(GbaBus& bus, uint32_t address, uint32_t count)
	{
		std::vector<uint8_t> out;
		for (uint32_t i = 0; i < count; i++)
			out.push_back(bus.Read8(address + i));
		return out;
	}

	inline std::string Dump(const std::vector<uint8_t>& bytes)
	{
		std::string text;
		for (uint8_t byte : bytes)
			text += GbaTest::Hex(byte) + " ";
		return text;
	}
}
