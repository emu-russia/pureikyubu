// The Game Boy cartridge: the header, the memory bank controllers, the real time clock and the
// battery backed save file.
//
// The tests assemble cartridge images here (a header with the fields the Pan Docs describe, then a
// ROM the test can fill with a bank signature) and drive the real GbCart through the bus's address
// decoding, so the expected banks and bytes come from the specification and not from the emulator.

#include "gba_test.h"
#include "gb_cart.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace GBA;

namespace
{
	/// <summary>The save files these tests write (the GBA tests' convention: everything in /tmp).</summary>
	const char* const SavePath = "/tmp/gb_cart_test.sav";
	const char* const RomPath = "/tmp/gb_cart_test.gb";

	/// <summary>
	/// Build a cartridge image: a 32 KByte (or larger) ROM whose every 16 KByte bank starts with
	/// its own bank number, so a test can ask which bank the mapper put at 0x4000.
	/// </summary>
	std::vector<u8> BuildRom(u8 cartType, u8 romSizeCode, u8 ramSizeCode, int banks)
	{
		std::vector<u8> rom((size_t)banks * 0x4000, 0x00);

		// The entry point and the Nintendo logo area are not important here; the header fields
		// are (Pan Docs "The Cartridge Header").
		rom[GbHeaderTitle + 0] = 'T';
		rom[GbHeaderTitle + 1] = 'E';
		rom[GbHeaderTitle + 2] = 'S';
		rom[GbHeaderTitle + 3] = 'T';
		rom[GbHeaderCgbFlag] = 0x00;
		rom[GbHeaderSgbFlag] = 0x00;
		rom[GbHeaderCartType] = cartType;
		rom[GbHeaderRomSize] = romSizeCode;
		rom[GbHeaderRamSize] = ramSizeCode;
		rom[GbHeaderDestination] = 0x01;
		rom[GbHeaderVersion] = 0x00;

		// A distinctive byte at the start of every bank.
		for (int bank = 0; bank < banks && bank < 256; bank++)
			rom[(size_t)bank * 0x4000] = (u8)bank;

		// The header checksum the boot ROM (and Describe) reads.
		rom[GbHeaderChecksum] = GbComputeHeaderChecksum(rom);
		return rom;
	}
}

// ---------------------------------------------------------------------------------------
// The header
// ---------------------------------------------------------------------------------------

GBA_TEST(GbCart, header_is_parsed_field_by_field)
{
	std::vector<u8> rom = BuildRom(0x1B, 0x04, 0x03, 64);		// MBC5 + battery, 512 KByte, 32 KByte RAM
	rom[GbHeaderCgbFlag] = 0x80;								// CGB enhanced
	rom[GbHeaderSgbFlag] = 0x03;								// SGB functions
	rom[GbHeaderChecksum] = GbComputeHeaderChecksum(rom);

	GbCartHeader header = GbParseCartHeader(rom);
	GBA_CHECK(header.valid);
	GBA_CHECK_MSG(header.title == "TEST", "the title is " + header.title);
	GBA_CHECK(header.cgbFlag);
	GBA_CHECK(!header.cgbOnly);
	GBA_CHECK(header.sgbFlag);
	GBA_CHECK_EQ(header.cartType, 0x1B);
	GBA_CHECK_EQ(header.romSizeCode, 0x04);
	GBA_CHECK_EQ(header.ramSizeCode, 0x03);
	GBA_CHECK_EQ(header.destination, 0x01);
	GBA_CHECK_EQ(header.romBytes, 512u * 1024u);
	GBA_CHECK_EQ(header.ramBytes, 32u * 1024u);
	GBA_CHECK(header.battery);
	GBA_CHECK(!header.rtc);
	GBA_CHECK_EQ((int)header.mapper, (int)GbMapper::Mbc5);
	GBA_CHECK_EQ(header.headerChecksum, rom[GbHeaderChecksum]);

	// A CGB-only cartridge says so with 0xC0.
	rom[GbHeaderCgbFlag] = 0xC0;
	header = GbParseCartHeader(rom);
	GBA_CHECK(header.cgbFlag && header.cgbOnly);

	// Too small to hold a header: not valid, and nothing is parsed.
	std::vector<u8> tiny(0x100, 0x00);
	GBA_CHECK(!GbParseCartHeader(tiny).valid);
}

GBA_TEST(GbCart, header_checksum_matches_the_specification_algorithm)
{
	// x = 0; for each byte at 0x0134..0x014C: x = x - byte - 1 (Pan Docs "The Cartridge Header").
	std::vector<u8> rom = BuildRom(0x00, 0x01, 0x00, 8);
	u8 expected = 0;
	for (u16 address = GbHeaderTitle; address <= GbHeaderVersion; address++)
		expected = (u8)(expected - rom[address] - 1);
	GBA_CHECK_EQ(GbComputeHeaderChecksum(rom), expected);
}

GBA_TEST(GbCart, mapper_names_and_the_cgb_boot_register)
{
	GBA_CHECK_EQ((int)GbMapperForType(0x00), (int)GbMapper::None);
	GBA_CHECK_EQ((int)GbMapperForType(0x01), (int)GbMapper::Mbc1);
	GBA_CHECK_EQ((int)GbMapperForType(0x06), (int)GbMapper::Mbc2);
	GBA_CHECK_EQ((int)GbMapperForType(0x0F), (int)GbMapper::Mbc3);
	GBA_CHECK_EQ((int)GbMapperForType(0x1E), (int)GbMapper::Mbc5);
	GBA_CHECK_EQ((int)GbMapperForType(0x22), (int)GbMapper::Mbc7);
	GBA_CHECK_EQ((int)GbMapperForType(0x77), (int)GbMapper::Unknown);
	GBA_CHECK_MSG(std::string(GbMapperName(GbMapper::Mbc3)) == "MBC3", "the mapper name");

	// A = 0x01 for a DMG cartridge on a DMG, 0x11 for a CGB compatible one on a CGB and 0x00 for a
	// DMG only one there (the documented deviation from the Pan Docs table).
	GbCart cart;
	std::string error;
	std::vector<u8> dmg = BuildRom(0x00, 0x01, 0x00, 8);
	GBA_CHECK(cart.LoadRomImage(dmg, error));
	GBA_CHECK_EQ(cart.BootRegisterA(false), 0x01);
	GBA_CHECK_EQ(cart.BootRegisterA(true), 0x00);

	std::vector<u8> cgb = BuildRom(0x00, 0x01, 0x00, 8);
	cgb[GbHeaderCgbFlag] = 0x80;
	GBA_CHECK(cart.LoadRomImage(cgb, error));
	GBA_CHECK_EQ(cart.BootRegisterA(true), 0x11);
}

GBA_TEST(GbCart, unsupported_mapper_is_reported)
{
	GbCart cart;
	std::string error;

	// 0x0B..0x0D is the MMM01, which this emulator does not implement (Pan Docs "MBCs").
	std::vector<u8> mmm01 = BuildRom(0x0B, 0x01, 0x00, 8);
	GBA_CHECK_MSG(!cart.LoadRomImage(mmm01, error), "the MMM01 must be refused");
	GBA_CHECK_MSG(error.find("unsupported") != std::string::npos, "the error was: " + error);
	GBA_CHECK_MSG(error.find("MMM01") != std::string::npos, "the error names the mapper: " + error);

	// An unassigned cartridge type is refused too, and says so.
	std::vector<u8> unknown = BuildRom(0x77, 0x01, 0x00, 8);
	GBA_CHECK_MSG(!cart.LoadRomImage(unknown, error), "an unknown type must be refused");
	GBA_CHECK_MSG(error.find("unknown") != std::string::npos, "the error was: " + error);

	// A ROM-only cartridge is accepted.
	std::vector<u8> plain = BuildRom(0x00, 0x01, 0x00, 8);
	GBA_CHECK_MSG(cart.LoadRomImage(plain, error), "a ROM-only cartridge must load: " + error);
}

// ---------------------------------------------------------------------------------------
// The memory bank controllers
// ---------------------------------------------------------------------------------------

GBA_TEST(GbCart, rom_only_and_the_mbc1_banking)
{
	GbCart cart;
	std::string error;

	// ROM only: bank 0 at 0x0000 and the second 16 KByte bank at 0x4000.
	std::vector<u8> plain = BuildRom(0x00, 0x01, 0x00, 8);
	GBA_CHECK(cart.LoadRomImage(plain, error));
	GBA_CHECK_EQ(cart.ReadRom(0x0000), 0x00);
	GBA_CHECK_EQ(cart.ReadRom(0x4000), 0x01);
	GBA_CHECK_EQ(cart.ReadRom(0x4001), 0x00);		// the rest of the bank is the image's zeroes

	// MBC1: writing 0x0000..0x3FFF selects the low five bits of the bank at 0x4000.
	std::vector<u8> mbc1 = BuildRom(0x01, 0x04, 0x03, 64);
	GBA_CHECK(cart.LoadRomImage(mbc1, error));
	GBA_CHECK_EQ(cart.MappedRomBank(), 1u);			// the power-on value is bank 1
	cart.WriteRom(0x2000, 0x05);
	GBA_CHECK_EQ(cart.MappedRomBank(), 5u);
	GBA_CHECK_EQ(cart.ReadRom(0x4000), 0x05);
	GBA_CHECK_EQ(cart.ReadRom(0x0000), 0x00);		// bank 0 stays at 0x0000 in mode 0

	// A written 0 becomes 1 (Pan Docs "MBC1": "if the lower five bits are zero, the value 1 is
	// used instead").
	cart.WriteRom(0x2000, 0x00);
	GBA_CHECK_EQ(cart.MappedRomBank(), 1u);

	// The two high bits (0x4000..0x5FFF) extend the bank, and with mode 1 they also switch the
	// bank at 0x0000.
	cart.WriteRom(0x2000, 0x01);
	cart.WriteRom(0x4000, 0x01);					// bit 5
	GBA_CHECK_EQ(cart.MappedRomBank(), 0x21u);
	GBA_CHECK_EQ(cart.ReadRom(0x4000), 0x21);
	GBA_CHECK_EQ(cart.ReadRom(0x0000), 0x00);		// still mode 0

	cart.WriteRom(0x6000, 0x01);					// mode 1
	GBA_CHECK(cart.BankingMode());
	GBA_CHECK_EQ(cart.ReadRom(0x0000), 0x20);		// bank 0x20 at 0x0000
	GBA_CHECK_EQ(cart.ReadRom(0x4000), 0x21);

	// The RAM is disabled until 0x0A is written to 0x0000..0x1FFF, and only the low nibble
	// matters (Pan Docs "MBC1").
	GBA_CHECK(!cart.RamEnabled());
	cart.WriteRom(0x0000, 0x0A);
	GBA_CHECK(cart.RamEnabled());
	cart.WriteRom(0x0000, 0x00);
	GBA_CHECK(!cart.RamEnabled());
}

GBA_TEST(GbCart, mbc2_banking_and_its_four_bit_ram)
{
	GbCart cart;
	std::string error;
	std::vector<u8> rom = BuildRom(0x06, 0x01, 0x00, 8);		// MBC2 + battery
	GBA_CHECK(cart.LoadRomImage(rom, error));

	// Bit 8 of the address selects between the RAM enable and the ROM bank (Pan Docs "MBC2").
	cart.WriteRom(0x0000, 0x0A);					// bit 8 clear: enable the RAM
	GBA_CHECK(cart.RamEnabled());
	cart.WriteRom(0x0100, 0x03);					// bit 8 set: the ROM bank
	GBA_CHECK(cart.RamEnabled());
	GBA_CHECK_EQ(cart.MappedRomBank(), 3u);
	GBA_CHECK_EQ(cart.ReadRom(0x4000), 0x03);

	// A written bank of 0 becomes 1, and only four bits are kept.
	cart.WriteRom(0x0100, 0x00);
	GBA_CHECK_EQ(cart.MappedRomBank(), 1u);
	cart.WriteRom(0x0100, 0x17);
	GBA_CHECK_EQ(cart.MappedRomBank(), 7u);

	// MBC2's built-in RAM is 512 x 4 bit, and the high nibble reads back as the low one.
	cart.WriteRam(0xA000, 0xAB);
	GBA_CHECK_EQ(cart.ReadRam(0xA000), 0xBB);		// 0x0B in both nibbles
	cart.WriteRam(0xA1FF, 0x05);
	GBA_CHECK_EQ(cart.ReadRam(0xA1FF), 0x55);

	// The RAM window is 512 bytes and wraps: 0xA200 is the same cell as 0xA000.
	GBA_CHECK_EQ(cart.ReadRam(0xA200), 0xBB);
}

GBA_TEST(GbCart, mbc3_ram_banking)
{
	GbCart cart;
	std::string error;
	std::vector<u8> rom = BuildRom(0x13, 0x04, 0x03, 64);		// MBC3 + battery + RTC, 32 KByte RAM
	GBA_CHECK(cart.LoadRomImage(rom, error));

	cart.WriteRom(0x0000, 0x0A);					// enable the RAM
	GBA_CHECK(cart.RamEnabled());

	// The ROM bank is seven bits and a zero becomes one (Pan Docs "MBC3"). The image is 512 KByte,
	// so the bank has to stay inside it for the bank signature to be the one that is read.
	cart.WriteRom(0x2000, 0x00);
	GBA_CHECK_EQ(cart.MappedRomBank(), 1u);
	cart.WriteRom(0x2000, 0x05);
	GBA_CHECK_EQ(cart.MappedRomBank(), 0x05u);
	GBA_CHECK_EQ(cart.ReadRom(0x4000), 0x05);

	// The RAM bank (0x4000..0x5FFF, values 0..3) selects which 8 KByte window is visible.
	cart.WriteRam(0xA000, 0x11);
	cart.WriteRom(0x4000, 0x01);
	GBA_CHECK_EQ(cart.MappedRamBank(), 1u);
	cart.WriteRam(0xA000, 0x22);
	cart.WriteRom(0x4000, 0x00);
	GBA_CHECK_EQ(cart.ReadRam(0xA000), 0x11);
	cart.WriteRom(0x4000, 0x01);
	GBA_CHECK_EQ(cart.ReadRam(0xA000), 0x22);
}

GBA_TEST(GbCart, mbc5_banking_is_nine_bits)
{
	GbCart cart;
	std::string error;
	std::vector<u8> rom = BuildRom(0x19, 0x05, 0x03, 128);		// MBC5, 1 MByte
	GBA_CHECK(cart.LoadRomImage(rom, error));

	// 0x2000..0x2FFF is the low eight bits, 0x3000..0x3FFF bit 8 (Pan Docs "MBC5"). Unlike the
	// earlier controllers, a bank of zero is allowed.
	cart.WriteRom(0x2000, 0x00);
	GBA_CHECK_EQ(cart.MappedRomBank(), 0u);
	GBA_CHECK_EQ(cart.ReadRom(0x4000), 0x00);

	cart.WriteRom(0x2000, 0x7F);
	GBA_CHECK_EQ(cart.MappedRomBank(), 0x7Fu);
	cart.WriteRom(0x3000, 0x01);
	GBA_CHECK_EQ(cart.MappedRomBank(), 0x17Fu);
	GBA_CHECK_EQ(cart.ReadRom(0x4000), 0x7F);		// bank 0x17F's signature (bank 0x7F)

	cart.WriteRom(0x3000, 0x00);
	cart.WriteRom(0x2000, 0xFF);
	GBA_CHECK_EQ(cart.MappedRomBank(), 0xFFu);

	// The RAM bank comes from 0x4000..0x5FFF; bit 3 is the rumble motor, which the mapper keeps in
	// the register but ignores when it selects the bank window that follows the ROM.
	cart.WriteRom(0x4000, 0x03);
	GBA_CHECK_EQ(cart.MappedRamBank(), 3u);
	cart.WriteRom(0x4000, 0x0B);					// the rumble bit is set on top of bank 3
	GBA_CHECK_EQ(cart.MappedRamBank(), 0x0Bu);		// the register keeps every bit it was given
}

// ---------------------------------------------------------------------------------------
// The real time clock
// ---------------------------------------------------------------------------------------

GBA_TEST(GbCart, mbc3_rtc_latch_and_registers)
{
	GbCart cart;
	std::string error;
	std::vector<u8> rom = BuildRom(0x10, 0x04, 0x03, 64);		// MBC3 + RTC + battery
	GBA_CHECK(cart.LoadRomImage(rom, error));

	// The RTC registers are only reachable while the RAM is enabled and an RTC register is
	// selected (0x08..0x0C, Pan Docs "MBC3").
	cart.WriteRom(0x0000, 0x0A);
	cart.WriteRom(0x4000, 0x08);					// select the seconds register
	GBA_CHECK_EQ(cart.ReadRam(0xA000), 0x00);

	// Writing a register and latching it back: the 0 then 1 sequence latches the running clock
	// (Pan Docs "MBC3"), so first set the host time and latch.
	cart.SetUnixTime(1000000);
	cart.WriteRom(0x6000, 0x00);					// the latch step 1
	cart.WriteRom(0x6000, 0x01);					// the latch step 2

	// The latched values must match the host clock (the emulator's RTC follows it).
	u64 seconds = 1000000 + 9 * 3600;
	u8 expectedSeconds = (u8)(seconds % 60);
	u8 expectedMinutes = (u8)((seconds / 60) % 60);
	cart.WriteRom(0x4000, 0x08);
	GBA_CHECK_EQ(cart.ReadRam(0xA000), expectedSeconds);
	cart.WriteRom(0x4000, 0x09);
	GBA_CHECK_EQ(cart.ReadRam(0xA000), expectedMinutes);

	// A write to the days high register sets the carry and halt flags (bits 7 and 6).
	cart.WriteRom(0x4000, 0x0C);
	cart.WriteRam(0xA000, 0xC1);					// carry + halt + day bit 8
	GBA_CHECK_EQ(cart.ReadRam(0xA000), 0xC1);
	GBA_CHECK(cart.Rtc().dayCarry);
	GBA_CHECK(cart.Rtc().halt);
	GBA_CHECK_EQ(cart.Rtc().days & 0x100, 0x100);
}

// ---------------------------------------------------------------------------------------
// The save file
// ---------------------------------------------------------------------------------------

GBA_TEST(GbCart, save_file_round_trip)
{
	remove(SavePath);

	GbCart cart;
	std::string error;
	std::vector<u8> rom = BuildRom(0x03, 0x01, 0x02, 8);		// MBC1 + battery + 8 KByte RAM
	cart.SetSaveFilePath(SavePath);
	GBA_CHECK(cart.LoadRomImage(rom, error));
	GBA_CHECK(cart.HasBattery());

	// Fill the RAM through the mapper's window.
	cart.WriteRom(0x0000, 0x0A);
	for (int i = 0; i < 0x2000; i += 0x101)
		cart.WriteRam((u16)(0xA000 + i), (u8)(i >> 8));
	GBA_CHECK(cart.SaveSaveFile(&error));

	// A fresh cartridge reads it back byte for byte.
	GbCart reloaded;
	reloaded.SetSaveFilePath(SavePath);
	GBA_CHECK(reloaded.LoadRomImage(rom, error));
	GBA_CHECK_MSG(reloaded.LoadSaveFile(&error), "the save file must load: " + error);
	for (int i = 0; i < 0x2000; i++)
	{
		if (reloaded.Ram()[(size_t)i] != cart.Ram()[(size_t)i])
			GBA_FAIL("the RAM differs at offset " + GbaTest::Hex((uint64_t)i));
	}

	// A cartridge with no battery has nothing to save.
	GbCart plain;
	plain.SetSaveFilePath(SavePath);
	std::vector<u8> noBattery = BuildRom(0x01, 0x01, 0x02, 8);
	GBA_CHECK(plain.LoadRomImage(noBattery, error));
	GBA_CHECK_MSG(!plain.SaveSaveFile(&error), "a cartridge without a battery saves nothing");

	remove(SavePath);
}

GBA_TEST(GbCart, load_rom_file_beside_its_save)
{
	// Write a cartridge to disk and load it through LoadRomFile, which is where the ".sav next to
	// the ROM" convention lives (the same one the GBA cartridge uses).
	std::vector<u8> rom = BuildRom(0x03, 0x01, 0x02, 8);
	{
		std::ofstream file(RomPath, std::ios::binary);
		file.write((const char*)rom.data(), (std::streamsize)rom.size());
	}
	remove(SavePath);

	GbCart cart;
	std::string error;
	GBA_CHECK_MSG(cart.LoadRomFile(RomPath, error), "cannot load the cartridge: " + error);
	GBA_CHECK_MSG(cart.SaveFilePath() == SavePath, "the save path is " + cart.SaveFilePath());
	GBA_CHECK_EQ(cart.Rom().size(), rom.size());

	// Describe() reports the title and the mapper.
	std::string text;
	GBA_CHECK(cart.Describe(text));
	GBA_CHECK_MSG(text.find("TEST") != std::string::npos, "the report was: " + text);
	GBA_CHECK_MSG(text.find("MBC1") != std::string::npos, "the report was: " + text);

	remove(RomPath);
}
