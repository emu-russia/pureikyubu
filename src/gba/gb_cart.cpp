// The Game Boy cartridge and its mappers. See gb_cart.h for the specifications this is written
// from; every register write below names the Pan Docs rule it implements.
//
// The address decoding is deliberately done in one place (UpdateBanks) so a test can write the
// mapper registers and then ask which bank is mapped without executing any code.

#include "gb_cart.h"

#include <cstdio>
#include <ctime>

namespace GBA
{
	namespace
	{
		// The ROM size codes of the header (Pan Docs "The Cartridge Header"). Codes 0x00..0x08
		// are 32 KByte << code; 0x52/0x53/0x54 are the "only the first 1.1/1.2/1.5 MByte are
		// used" variants, which are rounded up to their mask size.
		u32 RomBytesForCode(u8 code)
		{
			if (code <= 0x08)
				return 0x8000u << code;

			switch (code)
			{
			case 0x52: return 0x120000;		// 1.1 MByte
			case 0x53: return 0x140000;		// 1.2 MByte
			case 0x54: return 0x180000;		// 1.5 MByte
			default: return 0;
			}
		}

		// The RAM size codes (Pan Docs): 0x00 none, 0x01 2 KByte, 0x02 8 KByte, 0x03 32 KByte,
		// 0x04 128 KByte (a few MBC5 cartridges), 0x05 64 KByte.
		u32 RamBytesForCode(u8 code)
		{
			switch (code)
			{
			case 0x01: return 2 * 1024;
			case 0x02: return 8 * 1024;
			case 0x03: return 32 * 1024;
			case 0x04: return 128 * 1024;
			case 0x05: return 64 * 1024;
			default: return 0;
			}
		}
	}

	GbMapper GbMapperForType(u8 cartType)
	{
		switch (cartType)
		{
		case 0x00: return GbMapper::None;
		case 0x01: case 0x02: case 0x03: return GbMapper::Mbc1;
		case 0x05: case 0x06: return GbMapper::Mbc2;
		case 0x08: case 0x09: return GbMapper::RomRam;
		case 0x0B: case 0x0C: case 0x0D: return GbMapper::Mmm01;
		case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13: return GbMapper::Mbc3;
		case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: return GbMapper::Mbc5;
		case 0x20: return GbMapper::Mbc6;
		case 0x22: return GbMapper::Mbc7;
		case 0xFC: return GbMapper::PocketCamera;
		case 0xFD: return GbMapper::BandaiTama5;
		case 0xFE: return GbMapper::Huc3;
		case 0xFF: return GbMapper::Huc1;
		default: return GbMapper::Unknown;
		}
	}

	const char* GbMapperName(GbMapper mapper)
	{
		switch (mapper)
		{
		case GbMapper::None: return "ROM only";
		case GbMapper::Mbc1: return "MBC1";
		case GbMapper::Mbc2: return "MBC2";
		case GbMapper::RomRam: return "ROM+RAM";
		case GbMapper::Mmm01: return "MMM01";
		case GbMapper::Mbc3: return "MBC3";
		case GbMapper::Mbc5: return "MBC5";
		case GbMapper::Mbc6: return "MBC6";
		case GbMapper::Mbc7: return "MBC7";
		case GbMapper::PocketCamera: return "Pocket Camera";
		case GbMapper::BandaiTama5: return "Bandai TAMA5";
		case GbMapper::Huc3: return "HuC3";
		case GbMapper::Huc1: return "HuC1";
		default: return "unknown";
		}
	}

	u8 GbComputeHeaderChecksum(const std::vector<u8>& rom)
	{
		// Pan Docs "The Cartridge Header": x = 0, then for each byte at 0x0134..0x014C,
		// x = x - byte - 1. The result is compared with the byte at 0x014D.
		u8 x = 0;
		for (u16 address = GbHeaderTitle; address <= GbHeaderVersion; address++)
		{
			if (address >= rom.size())
				return 0;
			x = (u8)(x - rom[address] - 1);
		}
		return x;
	}

	GbCartHeader GbParseCartHeader(const std::vector<u8>& rom)
	{
		GbCartHeader header;

		if (rom.size() < GbHeaderStart + 0x50)
			return header;

		header.valid = true;
		for (int i = 0; i < 0x50; i++)
			header.raw[i] = rom[GbHeaderStart + i];

		// The title is 16 bytes at 0x0134; a zero byte ends it. (On a CGB-only cartridge the
		// last four bytes are the manufacturer code, which this parser keeps as part of the
		// title - the Pan Docs note that the two uses overlap.)
		for (int i = 0; i < 16; i++)
		{
			u8 value = rom[GbHeaderTitle + i];
			if (value == 0)
				break;
			if (value >= 0x20 && value < 0x7F)
				header.title.push_back((char)value);
		}

		header.cgbFlag = (rom[GbHeaderCgbFlag] & 0x80) != 0;
		header.cgbOnly = (rom[GbHeaderCgbFlag] == 0xC0);
		header.sgbFlag = (rom[GbHeaderSgbFlag] == 0x03);
		header.cartType = rom[GbHeaderCartType];
		header.romSizeCode = rom[GbHeaderRomSize];
		header.ramSizeCode = rom[GbHeaderRamSize];
		header.destination = rom[GbHeaderDestination];
		header.version = rom[GbHeaderVersion];
		header.headerChecksum = rom[GbHeaderChecksum];
		header.globalChecksum = (u16)((rom[GbHeaderGlobalChecksum] << 8) | rom[GbHeaderGlobalChecksum + 1]);

		header.romBytes = RomBytesForCode(header.romSizeCode);
		header.ramBytes = RamBytesForCode(header.ramSizeCode);
		header.mapper = GbMapperForType(header.cartType);

		// The cartridge types that carry extra hardware, from the Pan Docs table.
		switch (header.cartType)
		{
		case 0x03: case 0x06: case 0x09: case 0x0D: case 0x0F: case 0x10:
		case 0x13: case 0x1B: case 0x1E:
			header.battery = true;
			break;
		default:
			header.battery = false;
			break;
		}

		header.rtc = (header.cartType == 0x0F || header.cartType == 0x10);
		header.rumble = (header.cartType == 0x1C || header.cartType == 0x1D || header.cartType == 0x1E);

		return header;
	}

	// ---------------------------------------------------------------------------------------
	// The real time clock
	// ---------------------------------------------------------------------------------------

	u8 GbRealTimeClock::Read(u8 index) const
	{
		switch (index)
		{
		case 0x08: return seconds;
		case 0x09: return minutes;
		case 0x0A: return hours;
		case 0x0B: return (u8)(days & 0xFF);
		case 0x0C:
			// Bit 7 is the day counter carry, bit 6 the halt flag, bit 0 the ninth day bit.
			return (u8)((dayCarry ? 0x80 : 0x00) | (halt ? 0x40 : 0x00) | ((days >> 8) & 0x01));
		default: return 0xFF;
		}
	}

	void GbRealTimeClock::Write(u8 index, u8 value)
	{
		switch (index)
		{
		case 0x08: seconds = (u8)(value % 60); break;
		case 0x09: minutes = (u8)(value % 60); break;
		case 0x0A: hours = (u8)(value % 24); break;
		case 0x0B: days = (u16)((days & 0x100) | value); break;
		case 0x0C:
			days = (u16)((days & 0xFF) | ((value & 0x01) << 8));
			dayCarry = (value & 0x80) != 0;
			halt = (value & 0x40) != 0;
			break;
		default: break;
		}
	}

	void GbRealTimeClock::Latch(u64 unixSeconds)
	{
		// The day counter is taken from the host clock (the Pan Docs note that the cartridge
		// keeps its own oscillator; following the host keeps a game's clock moving between
		// sessions without a battery backed RTC chip in the emulator).
		u64 total = (unixSeconds + 9 * 3600) % 86400;	// the Game Boy RTC's epoch is midnight
		seconds = (u8)(total % 60);
		minutes = (u8)((total / 60) % 60);
		hours = (u8)((total / 3600) % 24);
		days = (u16)((unixSeconds / 86400) & 0x1FF);
	}

	void GbRealTimeClock::SaveBytes(u8* out) const
	{
		out[0] = seconds;
		out[1] = minutes;
		out[2] = hours;
		out[3] = (u8)(days & 0xFF);
		out[4] = (u8)(days >> 8);
		out[5] = (u8)(dayCarry ? 1 : 0);
		out[6] = (u8)(halt ? 1 : 0);
	}

	void GbRealTimeClock::LoadBytes(const u8* in)
	{
		seconds = in[0];
		minutes = in[1];
		hours = in[2];
		days = (u16)(in[3] | (in[4] << 8));
		dayCarry = in[5] != 0;
		halt = in[6] != 0;
	}

	// ---------------------------------------------------------------------------------------
	// Loading
	// ---------------------------------------------------------------------------------------

	bool GbCart::LoadRomImage(const std::vector<u8>& image, std::string& error)
	{
		if (image.size() < GbHeaderStart + 0x50)
		{
			error = "the image is too small to hold a Game Boy header";
			return false;
		}

		GbCartHeader parsed = GbParseCartHeader(image);

		// Refuse the mappers that are not implemented, with a readable message instead of a
		// cartridge that silently does nothing (the task's "readable error for an unsupported
		// type").
		switch (parsed.mapper)
		{
		case GbMapper::Mmm01:
		case GbMapper::Mbc6:
		case GbMapper::Mbc7:
		case GbMapper::PocketCamera:
		case GbMapper::BandaiTama5:
		case GbMapper::Huc3:
		case GbMapper::Huc1:
			error = "unsupported cartridge type 0x";
			{
				char text[16];
				snprintf(text, sizeof(text), "%02X", parsed.cartType);
				error += text;
			}
			error += " (";
			error += GbMapperName(parsed.mapper);
			error += " is not implemented)";
			return false;
		case GbMapper::Unknown:
			error = "unknown cartridge type 0x";
			{
				char text[16];
				snprintf(text, sizeof(text), "%02X", parsed.cartType);
				error += text;
			}
			return false;
		default:
			break;
		}

		rom = image;
		header = parsed;
		title = parsed.title;

		// The RAM: MBC2 has 512 x 4 bits built in (Pan Docs), everything else takes its size
		// from the header. A header that says "no RAM" still gets the 8 KByte window, because
		// programs that write to 0xA000 with no RAM in the cartridge would otherwise hit the
		// open bus.
		u32 ramBytes = header.ramBytes;
		if (header.mapper == GbMapper::Mbc2)
			ramBytes = 512;
		if (ramBytes == 0)
			ramBytes = 0x2000;
		ram.assign(ramBytes, 0xFF);

		loaded = true;
		ResetMapper();

		// The ROM size code is a promise about the image; a short image is tolerated (the
		// address decoder mirrors the missing banks) but worth a warning.
		if (header.romBytes != 0 && image.size() < header.romBytes)
			GBA::Log(LogLevel::Warn, "gb cart: the header says %u bytes of ROM, the image has %u",
				(unsigned)header.romBytes, (unsigned)image.size());

		return true;
	}

	bool GbCart::LoadRomFile(const std::string& path, std::string& error)
	{
		FILE* file = fopen(path.c_str(), "rb");
		if (file == nullptr)
		{
			error = "cannot open " + path;
			return false;
		}

		std::vector<u8> image;
		u8 buffer[8192];
		while (true)
		{
			size_t got = fread(buffer, 1, sizeof(buffer), file);
			if (got > 0)
				image.insert(image.end(), buffer, buffer + got);
			if (got < sizeof(buffer))
				break;
		}
		fclose(file);

		if (!LoadRomImage(image, error))
			return false;

		// The GBA convention: the `.sav` sits next to the ROM.
		std::string save = path;
		size_t dot = save.find_last_of('.');
		size_t slash = save.find_last_of("/\\");
		if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
			save = save.substr(0, dot);
		savePath = save + ".sav";

		return true;
	}

	void GbCart::Eject()
	{
		loaded = false;
		header = GbCartHeader{};
		rom.clear();
		ram.clear();
		savePath.clear();
		title.clear();
		ResetMapper();
	}

	void GbCart::ResetMapper()
	{
		ramEnabled = (header.mapper == GbMapper::None || header.mapper == GbMapper::RomRam);
		bankingMode = false;
		romBank = 1;
		ramBank = 0;
		rtcSelect = false;
		rtcLatchState = 0;
		UpdateBanks();
	}

	u8 GbCart::BootRegisterA(bool cgbConsole) const
	{
		if (!loaded)
			return 0x00;

		if (!cgbConsole)
			return 0x01;			// a DMG always reports a DMG cartridge

		return header.cgbFlag ? 0x11 : 0x00;
	}

	// ---------------------------------------------------------------------------------------
	// The address decoding
	// ---------------------------------------------------------------------------------------

	void GbCart::UpdateBanks()
	{
		// The "a zero bank selects bank one" rules of the earlier controllers (Pan Docs "MBC1",
		// "MBC2", "MBC3"). The MBC5 allows bank 0 at 0x4000, so it is left alone.
		switch (header.mapper)
		{
		case GbMapper::Mbc2:
			// The low four bits select the ROM bank, 0 counts as 1.
			if ((romBank & 0x0F) == 0)
				romBank = 1;
			else
				romBank &= 0x0F;
			break;

		case GbMapper::Mbc1:
			// "If the lower five bits are zero, the value 1 is used instead".
			if ((romBank & 0x1F) == 0)
				romBank = (romBank & 0x60) | 0x01;
			break;

		case GbMapper::Mbc3:
			if (romBank == 0)
				romBank = 1;
			break;

		case GbMapper::Mbc5:
			// The MBC5 allows bank 0 at 0x4000 (Pan Docs "MBC5"), so there is nothing to correct.
			break;

		default:
			if (romBank == 0)
				romBank = 1;
			break;
		}
	}

	u8 GbCart::ReadRom(u16 address) const
	{
		if (!loaded || rom.empty())
			return OpenBus();

		u32 bank = 0;
		u32 offset = address;

		if (address < 0x4000)
		{
			// 0x0000..0x3FFF: bank 0, except in MBC1 mode 1 where the two "high" bits select the
			// bank (Pan Docs "MBC1": "in mode 1 ... 0x0000-0x3FFF can be switched").
			if (header.mapper == GbMapper::Mbc1 && bankingMode)
				bank = ramBank << 5;
		}
		else if (address < 0x8000)
		{
			bank = romBank;
			offset = (u32)(address - 0x4000);
		}
		else
		{
			return OpenBus();
		}

		// A short image mirrors, as a real mask ROM's address pins do.
		u32 at = ((bank * 0x4000) + offset) % (u32)rom.size();
		return rom[at];
	}

	u8 GbCart::ReadRam(u16 address) const
	{
		if (!loaded || ram.empty())
			return OpenBus();

		u32 offset = (u32)(address - 0xA000);

		// The RAM is only reachable while it is enabled (Pan Docs, all mappers: "the RAM is
		// disabled by default and must be enabled by writing to 0x0000..0x1FFF"). Until then a
		// real cartridge leaves the bus floating, which reads as 0xFF.
		if (!ramEnabled)
			return OpenBus();

		if (header.mapper == GbMapper::Mbc3 && rtcSelect)
			return rtc.Read((u8)(0x08 + ramBank));

		// MBC1 in mode 1 uses the bank bits for the ROM at 0x0000 and keeps the RAM at bank 0
		// (Pan Docs "MBC1": "in mode 1 the RAM bank is always 0").
		u32 bank = ramBank;
		if (header.mapper == GbMapper::Mbc1 && bankingMode)
			bank = 0;

		u32 at = (bank * 0x2000 + offset) % (u32)ram.size();

		if (header.mapper == GbMapper::Mbc2)
		{
			// MBC2 has 512 x 4 bit of RAM: the high nibble mirrors the low one (Pan Docs).
			u8 value = (u8)(ram[at & 0x1FF] & 0x0F);
			return (u8)(value | (value << 4));
		}

		return ram[at];
	}

	void GbCart::WriteRom(u16 address, u8 value)
	{
		if (!loaded)
			return;

		switch (header.mapper)
		{
		case GbMapper::None:
		case GbMapper::RomRam:
			// A ROM-only cartridge has no mapper; writes to the ROM area do nothing.
			break;

		case GbMapper::Mbc1:
			if (address < 0x2000)
			{
				// 0x0000..0x1FFF: RAM enable. 0x0A in the low nibble enables it (Pan Docs).
				ramEnabled = (value & 0x0F) == 0x0A;
			}
			else if (address < 0x4000)
			{
				// 0x2000..0x3FFF: the low five bits of the ROM bank.
				romBank = (romBank & 0x60) | (value & 0x1F);
			}
			else if (address < 0x6000)
			{
				// 0x4000..0x5FFF: the two high bits, which select a RAM bank in mode 1.
				ramBank = value & 0x03;
				romBank = (romBank & 0x1F) | ((u32)(value & 0x03) << 5);
			}
			else
			{
				// 0x6000..0x7FFF: the mode select.
				bankingMode = (value & 0x01) != 0;
			}
			UpdateBanks();
			break;

		case GbMapper::Mbc2:
			// "The least significant bit of the address decides what the write selects": bit 8
			// clear = RAM enable, bit 8 set = ROM bank (Pan Docs "MBC2").
			if ((address & 0x0100) == 0)
				ramEnabled = (value & 0x0F) == 0x0A;
			else
				romBank = value & 0x0F;
			UpdateBanks();
			break;

		case GbMapper::Mbc3:
			if (address < 0x2000)
			{
				ramEnabled = (value & 0x0F) == 0x0A;
			}
			else if (address < 0x4000)
			{
				romBank = value & 0x7F;
				if (romBank == 0)
					romBank = 1;
			}
			else if (address < 0x6000)
			{
				// 0x4000..0x5FFF: a RAM bank (0x00..0x03) or an RTC register (0x08..0x0C).
				if (value >= 0x08 && value <= 0x0C)
				{
					rtcSelect = true;
					ramBank = (u32)(value - 0x08);
				}
				else
				{
					rtcSelect = false;
					ramBank = value & 0x03;
				}
			}
			else
			{
				// 0x6000..0x7FFF: the latch. Writing 0x00 then 0x01 copies the running clock
				// into the visible registers (Pan Docs "MBC3").
				if (value == 0x00)
					rtcLatchState = 0;
				else if (value == 0x01 && rtcLatchState == 0)
					rtc.Latch(unixTime);
				rtcLatchState = value;
			}
			UpdateBanks();
			break;

		case GbMapper::Mbc5:
			if (address < 0x2000)
			{
				ramEnabled = (value & 0x0F) == 0x0A;
			}
			else if (address < 0x3000)
			{
				// 0x2000..0x2FFF: the low eight bits of the 9-bit ROM bank (bank 0 is allowed on
				// the MBC5, unlike the earlier controllers).
				romBank = (romBank & 0x100) | value;
			}
			else if (address < 0x4000)
			{
				// 0x3000..0x3FFF: bit 8 of the ROM bank.
				romBank = (romBank & 0x0FF) | ((u32)(value & 0x01) << 8);
			}
			else if (address < 0x6000)
			{
				// 0x4000..0x5FFF: the RAM bank; bit 3 is the rumble motor, which this emulator
				// ignores (a rumble test would need the frontend's feedback device).
				ramBank = value & 0x0F;
			}
			UpdateBanks();
			break;

		default:
			break;
		}
	}

	void GbCart::WriteRam(u16 address, u8 value)
	{
		if (!loaded || ram.empty() || !ramEnabled)
			return;

		u32 offset = (u32)(address - 0xA000);

		if (header.mapper == GbMapper::Mbc3 && rtcSelect)
		{
			rtc.Write((u8)(0x08 + ramBank), value);
			return;
		}

		u32 bank = ramBank;
		if (header.mapper == GbMapper::Mbc1 && bankingMode)
			bank = 0;

		u32 at = (bank * 0x2000 + offset) % (u32)ram.size();

		if (header.mapper == GbMapper::Mbc2)
		{
			// Only the low nibble is stored (Pan Docs "MBC2": "only the lower 4 bits ... are
			// used, the upper 4 bits are undefined").
			ram[at & 0x1FF] = (u8)(value & 0x0F);
			return;
		}

		ram[at] = value;
	}

	// ---------------------------------------------------------------------------------------
	// The save file
	// ---------------------------------------------------------------------------------------

	bool GbCart::LoadSaveFile(std::string* error)
	{
		if (!loaded || !header.battery || savePath.empty())
			return false;

		FILE* file = fopen(savePath.c_str(), "rb");
		if (file == nullptr)
			return false;			// a cartridge that was never saved is not an error

		std::vector<u8> data;
		u8 buffer[8192];
		while (true)
		{
			size_t got = fread(buffer, 1, sizeof(buffer), file);
			if (got > 0)
				data.insert(data.end(), buffer, buffer + got);
			if (got < sizeof(buffer))
				break;
		}
		fclose(file);

		if (data.size() < ram.size())
		{
			if (error != nullptr)
				*error = "the save file is shorter than the cartridge's RAM";
			return false;
		}

		for (size_t i = 0; i < ram.size(); i++)
			ram[i] = data[i];

		// The RTC state follows the RAM (the same trailer SaveSaveFile writes).
		if (header.rtc && data.size() >= ram.size() + 8)
			rtc.LoadBytes(&data[ram.size()]);

		return true;
	}

	bool GbCart::SaveSaveFile(std::string* error)
	{
		if (!loaded || !header.battery || savePath.empty())
			return false;

		FILE* file = fopen(savePath.c_str(), "wb");
		if (file == nullptr)
		{
			if (error != nullptr)
				*error = "cannot open " + savePath + " for writing";
			return false;
		}

		if (!ram.empty())
			fwrite(ram.data(), 1, ram.size(), file);

		if (header.rtc)
		{
			u8 bytes[8]{};
			rtc.SaveBytes(bytes);
			fwrite(bytes, 1, sizeof(bytes), file);
		}

		fclose(file);
		return true;
	}

	bool GbCart::Describe(std::string& text) const
	{
		if (!loaded)
		{
			text = "no cartridge";
			return false;
		}

		char buffer[256];
		snprintf(buffer, sizeof(buffer), "\"%s\" %s, %u KByte ROM, %u KByte RAM%s%s%s%s",
			title.c_str(), GbMapperName(header.mapper),
			(unsigned)(rom.size() / 1024), (unsigned)(ram.size() / 1024),
			header.cgbFlag ? (header.cgbOnly ? ", CGB only" : ", CGB enhanced") : "",
			header.sgbFlag ? ", SGB" : "",
			header.battery ? ", battery" : "",
			header.rtc ? ", RTC" : "");
		text = buffer;
		return true;
	}
}
