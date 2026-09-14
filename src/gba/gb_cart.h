// The Game Boy cartridge: the header, the ROM, the battery backed RAM and the memory bank
// controllers (MBC1, MBC2, MBC3 with its real time clock, MBC5).
//
// Written from the Pan Docs "The Cartridge Header" and "Memory Bank Controllers"
// (gbdev.io/pandocs) plus the Game Boy Programming Manual's cartridge chapter. The register
// semantics below are quoted from those pages: MBC1's mode 0/mode 1 split, the 0 -> 1 bank
// correction, MBC2's 512 x 4 bit built-in RAM and the bit 8 trick that separates its RAM enable
// from its ROM bank register, MBC3's RTC latch sequence, and MBC5's 9-bit ROM bank register.
//
// The save file convention is the GBA cartridge's: a `.sav` next to the ROM holding the battery
// backed RAM (and the RTC registers of an MBC3), written by SaveSaveFile() and read back by
// LoadSaveFile().

#pragma once

#include "gba_types.h"

#include <string>
#include <vector>

namespace GBA
{
	// ---------------------------------------------------------------------------------------
	// The header (0x0100 .. 0x014F). Pan Docs "The Cartridge Header". The names carry the Gb
	// prefix because both cores of this module share namespace GBA.
	// ---------------------------------------------------------------------------------------

	const u16 GbHeaderStart = 0x0100;
	const u16 GbHeaderLogo = 0x0104;		// the Nintendo logo, 0x0104 .. 0x0133
	const u16 GbHeaderTitle = 0x0134;		// 0x0134 .. 0x0143 (title, 16 bytes at most)
	const u16 GbHeaderCgbFlag = 0x0143;
	const u16 GbHeaderNewLicensee = 0x0144;
	const u16 GbHeaderSgbFlag = 0x0146;
	const u16 GbHeaderCartType = 0x0147;
	const u16 GbHeaderRomSize = 0x0148;
	const u16 GbHeaderRamSize = 0x0149;
	const u16 GbHeaderDestination = 0x014A;
	const u16 GbHeaderOldLicensee = 0x014B;
	const u16 GbHeaderVersion = 0x014C;
	const u16 GbHeaderChecksum = 0x014D;
	const u16 GbHeaderGlobalChecksum = 0x014E;

	/// <summary>The mapper the header asks for. The names are the Pan Docs cartridge types.</summary>
	enum class GbMapper : int
	{
		None = 0,			// 0x00: ROM only
		Mbc1,				// 0x01..0x03
		Mbc2,				// 0x05, 0x06
		RomRam,				// 0x08, 0x09: ROM + RAM, no bank controller
		Mmm01,				// 0x0B..0x0D: not implemented
		Mbc3,				// 0x0F..0x13
		Mbc5,				// 0x19..0x1E
		Mbc6,				// 0x20: not implemented
		Mbc7,				// 0x22: not implemented
		PocketCamera,		// 0xFC: not implemented
		BandaiTama5,		// 0xFD: not implemented
		Huc3,				// 0xFE: not implemented
		Huc1,				// 0xFF: not implemented
		Unknown,			// an unassigned byte
	};

	/// <summary>What a header says about the cartridge (Pan Docs "The Cartridge Header").</summary>
	struct GbCartHeader
	{
		u8 raw[0x50]{};			// the bytes 0x0100 .. 0x014F themselves
		bool valid = false;		// a cartridge is loaded and long enough to hold the header

		std::string title;		// 0x0134..0x0143, cut at the first zero
		bool cgbFlag = false;	// 0x0143 bit 7: "this cartridge understands CGB functions"
		bool cgbOnly = false;	// 0x0143 == 0xC0: refuses to run on a DMG
		bool sgbFlag = false;	// 0x0146 == 0x03: Super Game Boy functions
		u8 cartType = 0x00;		// 0x0147
		u8 romSizeCode = 0x00;	// 0x0148
		u8 ramSizeCode = 0x00;	// 0x0149
		u8 destination = 0x00;	// 0x014A: 0x00 Japan, 0x01 everywhere else
		u8 version = 0x00;		// 0x014C
		u8 headerChecksum = 0x00;		// 0x014D
		u16 globalChecksum = 0x0000;	// 0x014E

		u32 romBytes = 0;		// from romSizeCode (32 KByte steps)
		u32 ramBytes = 0;		// from ramSizeCode (2/8/32 KByte)
		bool battery = false;	// the cartridge type carries a battery
		bool rtc = false;		// MBC3 with the timer registers
		bool rumble = false;	// MBC5 with a rumble motor (the bit is ignored)

		GbMapper mapper = GbMapper::None;
	};

	/// <summary>Parse a header out of a ROM image (Pan Docs "The Cartridge Header").</summary>
	GbCartHeader GbParseCartHeader(const std::vector<u8>& rom);

	/// <summary>The mapper a cartridge type byte means, and its Pan Docs name.</summary>
	GbMapper GbMapperForType(u8 cartType);
	const char* GbMapperName(GbMapper mapper);

	/// <summary>The standard header checksum: x = x - byte - 1 over 0x0134 .. 0x014C.</summary>
	u8 GbComputeHeaderChecksum(const std::vector<u8>& rom);

	// ---------------------------------------------------------------------------------------
	// The real time clock (MBC3 only). Pan Docs "Memory Bank Controllers" / "RTC".
	// ---------------------------------------------------------------------------------------

	struct GbRealTimeClock
	{
		// The five latched registers the cartridge exposes through 0xA000.
		u8 seconds = 0;			// 0..59
		u8 minutes = 0;			// 0..59
		u8 hours = 0;			// 0..23 (the 24 hour counter, not the 12 hour clock)
		u16 days = 0;			// 0..511, the low 9 bits of the day counter
		bool dayCarry = false;	// bit 7 of the day high register
		bool halt = false;		// bit 6 of the day high register: the clock stops

		/// <summary>The registers as the program sees them (index 0x08..0x0C).</summary>
		u8 Read(u8 index) const;

		/// <summary>Write one of the registers (index 0x08..0x0C).</summary>
		void Write(u8 index, u8 value);

		/// <summary>Latch the current time into the registers (the 0 then 1 sequence).</summary>
		void Latch(u64 unixSeconds);

		/// <summary>The five register bytes plus the two flags, for the save file.</summary>
		void SaveBytes(u8* out) const;
		void LoadBytes(const u8* in);
	};

	// ---------------------------------------------------------------------------------------
	// The cartridge
	// ---------------------------------------------------------------------------------------

	class GbCart
	{
	public:
		/// <summary>Install a ROM image. `error` is filled in when the cartridge cannot be used.</summary>
		bool LoadRomImage(const std::vector<u8>& image, std::string& error);

		/// <summary>Install a ROM and remember where its `.sav` goes (the GBA convention).</summary>
		bool LoadRomFile(const std::string& path, std::string& error);

		/// <summary>Take the cartridge out of the slot.</summary>
		void Eject();

		bool IsLoaded() const { return loaded; }

		/// <summary>What the header says; `valid` is false with no cartridge.</summary>
		const GbCartHeader& Header() const { return header; }

		/// <summary>True when the header's CGB flag makes the cartridge CGB compatible.</summary>
		bool CgbCompatible() const { return header.cgbFlag; }
		bool CgbOnly() const { return header.cgbOnly; }

		/// <summary>
		/// The value the boot ROM leaves in A when it hands over. On a DMG that is 0x01; on a CGB
		/// it is 0x11 for a CGB compatible cartridge and 0x00 for a DMG only one (a deliberate
		/// deviation from the Pan Docs "Power Up Sequence" table, which gives 0x11 in both cases
		/// because the real console keeps the mode decision in KEY0 instead - see GbSystem).
		/// </summary>
		u8 BootRegisterA(bool cgbConsole) const;

		// -- the bus side --------------------------------------------------------------------

		u8 ReadRom(u16 address) const;
		u8 ReadRam(u16 address) const;
		void WriteRom(u16 address, u8 value);
		void WriteRam(u16 address, u8 value);

		/// <summary>The offset the mapper currently maps (for Describe() and the tests).</summary>
		u32 MappedRomBank() const { return romBank; }
		u32 MappedRamBank() const { return ramBank; }
		bool RamEnabled() const { return ramEnabled; }
		bool BankingMode() const { return bankingMode; }
		const GbRealTimeClock& Rtc() const { return rtc; }

		/// <summary>Feed the wall clock the RTC reads when it latches.</summary>
		void SetUnixTime(u64 seconds) { unixTime = seconds; }
		u64 UnixTime() const { return unixTime; }

		/// <summary>The ROM image (the tests and the boot ROM's report read the header from it).</summary>
		const std::vector<u8>& Rom() const { return rom; }

		// -- the save file -------------------------------------------------------------------

		void SetSaveFilePath(const std::string& path) { savePath = path; }
		const std::string& SaveFilePath() const { return savePath; }

		/// <summary>The bytes SaveSaveFile() writes: the battery backed RAM plus the RTC state.</summary>
		bool HasBattery() const { return header.battery; }
		const std::vector<u8>& Ram() const { return ram; }

		bool LoadSaveFile(std::string* error);
		bool SaveSaveFile(std::string* error);

		bool Describe(std::string& text) const;

	private:
		bool loaded = false;
		GbCartHeader header;
		std::vector<u8> rom;
		std::vector<u8> ram;			// 2/8/32 KByte (MBC2: 512 bytes, one nibble each)
		std::string savePath;
		std::string title;

		u64 unixTime = 0;				// the host clock the RTC follows

		// -- the mapper registers -------------------------------------------------------------

		bool ramEnabled = false;
		bool bankingMode = false;		// MBC1 mode 0 (ROM banking) / mode 1 (RAM banking)
		u32 romBank = 1;				// the bank mapped at 0x4000..0x7FFF
		u32 ramBank = 0;				// the RAM bank, or the MBC3 RTC register select
		bool rtcSelect = false;			// MBC3: 0x08..0x0C was selected instead of a RAM bank
		u8 rtcLatchState = 0;			// MBC3: the 0 then 1 latch sequence
		GbRealTimeClock rtc;

		// -- helpers -------------------------------------------------------------------------

		/// <summary>Put every mapper register back to its power-on value.</summary>
		void ResetMapper();

		/// <summary>Redo the address decoding after any mapper register changed.</summary>
		void UpdateBanks();

		/// <summary>The value a region nothing drives reads as (0xFF).</summary>
		u8 OpenBus() const { return 0xFF; }
	};
}
