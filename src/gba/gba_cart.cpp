// GBA cartridge: the ROM image, the save memory (SRAM / Flash / EEPROM) and the GPIO/RTC port.
//
// Written from GBATEK (problemkaputt.de/gbatek.htm), sections:
//
//   * "GBA Cartridge Header"     - the title/game code/maker code and the complement check;
//   * "GBA Cartridge ROM"        - the 16bit ROM bus and its mirroring;
//   * "GBA Cart Backup IDs"      - the "EEPROM_V"/"SRAM_V"/"FLASH_V"/"FLASH512_V"/"FLASH1M_V"
//                                  linker signatures and what they mean;
//   * "GBA Cart Backup SRAM/FRAM"- the 32 KByte byte wide SRAM in its 64 KByte window;
//   * "GBA Cart Backup Flash ROM"- the SST command set (64 KByte / 128 KByte devices, banking);
//   * "GBA Cart Backup EEPROM"   - the serial 512 Byte / 8 KByte EEPROM behind DMA3;
//   * "GBA Cart I/O Port (GPIO)" - the 4bit port at 0x080000C4..0x080000C8;
//   * "GBA Cart Real-Time Clock (RTC)" and "DS Real-Time Clock (RTC)" - the Seiko S-3511A
//                                  three wire protocol (the GBA chip is the DS chip's ancestor);
//   * "GBA System Control"       - WAITCNT and the Game Pak waitstates;
//   * "GBA GamePak Prefetch"     - why a sequential access can be a single cycle.
//
// The deviations from the hardware are written down where they happen; the two worth knowing up
// front are the ROM mirroring of images that are not powers of two (homebrew and test ROMs) and
// the EEPROM transfer end, which is derived from the bit count instead of a chip select line
// (the Cart interface only sees the halfword stream, GBATEK's chip select is implicit in the
// DMA3 length).

#include "gba_cart.h"
#include "gba_bus.h"

#include <chrono>
#include <ctime>
#include <fstream>

namespace GBA
{
	namespace
	{
		// The largest Game Pak: 32 MByte (GBATEK "GBA Technical Data": Game Pak max. 32MB ROM).
		const size_t MaxRomBytes = 32 * 1024 * 1024;

		// The save memory window at 0x0E000000 is 64 KByte wide (GBATEK "GBA Memory Map").
		const u32 SaveWindowSize = 64 * 1024;

		// GBATEK "GBA Cart Backup IDs": Nintendo's linker inserts one of these strings at a
		// word aligned address. Only the length of the signature itself is compared, the digits
		// that follow are the library version ("nnn").
		struct SaveSignature
		{
			const char* text;
			SaveType type;
		};

		const SaveSignature SaveSignatures[] =
		{
			{ "EEPROM_V",	SaveType::Eeprom512B },	// the size is decided below
			{ "SRAM_V",		SaveType::Sram32K },
			{ "FLASH_V",	SaveType::Flash64K },
			{ "FLASH512_V",	SaveType::Flash64K },
			{ "FLASH1M_V",	SaveType::Flash128K },
		};

		// The longest of the strings above, for the end of the scan.
		const size_t MaxSignatureLength = 10;	// "FLASH512_V"

		// GBATEK "4000204h - WAITCNT": the first access codes are 4,3,2,8 cycles for 0..3.
		const int RomFirstWaits[4] = { 4, 3, 2, 8 };

		// The S-3511A control register bits (GBATEK "GBA Cart Real-Time Clock (RTC)").
		const u8 RtcControlIrq = 0x08;		// per minute IRQ (30s duty)
		const u8 RtcControlHour24 = 0x40;	// 0 = 12 hour mode, 1 = 24 hour mode (usually 1)
		const u8 RtcControlPower = 0x80;	// power failed, read only, cleared by the read
		const u8 RtcControlWritable = 0x6A;	// bits 1,3,5,6 (bit7 is the read only power flag)

		// -----------------------------------------------------------------------------------
		// The host clock
		// -----------------------------------------------------------------------------------

		/// <summary>The machine's own clock in seconds (the RTC's substitute time source).</summary>
		u32 HostSecond()
		{
			return (u32)std::time(nullptr);
		}

		/// <summary>Fill the RTC calendar fields from the host's local time.</summary>
		void ReadHostClock(RtcRegisters& rtc)
		{
			using namespace std::chrono;

			auto now = system_clock::now();
			std::time_t seconds = system_clock::to_time_t(now);
			std::tm tm{};
#if defined(_MSC_VER)
			localtime_s(&tm, &seconds);
#else
			localtime_r(&seconds, &tm);
#endif
			rtc.year = (tm.tm_year + 1900) % 100;		// the chip stores 00..99 = 2000..2099
			rtc.month = tm.tm_mon + 1;
			rtc.day = tm.tm_mday;
			rtc.weekday = tm.tm_wday;					// 0 = Sunday, like the header documents
			rtc.hour = tm.tm_hour;
			rtc.minute = tm.tm_min;
			rtc.second = tm.tm_sec;

			// std::tm has no sub-second field, so take the centiseconds from the full clock.
			auto millis = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;
			rtc.centisecond = (int)(millis / 10);
		}

		// -----------------------------------------------------------------------------------
		// Packed BCD, the encoding the RTC's registers use
		// -----------------------------------------------------------------------------------

		u8 ToBcd(int value)
		{
			if (value < 0) value = 0;
			return (u8)(((value / 10) << 4) | (value % 10));
		}

		bool BcdOk(u8 value)
		{
			return (value & 0x0F) <= 9 && ((value >> 4) & 0x0F) <= 9;
		}

		int FromBcd(u8 value)
		{
			return (value >> 4) * 10 + (value & 0x0F);
		}

		/// <summary>A BCD field, or `fallback` when the byte is not BCD (the chip replaces
		/// malformed values, WSdev's S-3511A notes: year 00h, month 01h, day 01h, time 00h).</summary>
		int BcdField(u8 value, int fallback, int maxValue)
		{
			if (!BcdOk(value)) return fallback;
			int v = FromBcd(value);
			return (v <= maxValue) ? v : fallback;
		}

		// -----------------------------------------------------------------------------------
		// The calendar, for advancing a clock the game has set
		// -----------------------------------------------------------------------------------

		bool LeapYear(int year)
		{
			// The RTC only knows 2000..2099, so "divisible by 4" is the whole rule.
			return (year % 4) == 0;
		}

		int MonthDays(int year, int month)
		{
			static const int days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
			int count = days[month - 1];
			if (month == 2 && LeapYear(year)) count++;
			return count;
		}

		/// <summary>Days from 2000-01-01 to the given date (the RTC's epoch).</summary>
		u32 DayNumber(int year, int month, int day)
		{
			u32 total = 0;
			for (int y = 0; y < year; y++)
				total += LeapYear(y) ? 366 : 365;
			for (int m = 1; m < month; m++)
				total += (u32)MonthDays(year, m);
			return total + (u32)(day - 1);
		}

		/// <summary>Advance the calendar fields by a whole number of seconds.</summary>
		void AddSeconds(RtcRegisters& rtc, u32 seconds)
		{
			if (seconds == 0) return;

			u32 dayBefore = DayNumber(rtc.year, rtc.month, rtc.day);
			u32 secondOfDay = (u32)(rtc.hour * 3600 + rtc.minute * 60 + rtc.second);
			u64 stamp = (u64)dayBefore * 86400 + secondOfDay + seconds;

			u32 dayAfter = (u32)(stamp / 86400);
			u32 rest = (u32)(stamp % 86400);

			// The weekday moves with the date (0 = Sunday).
			rtc.weekday = (int)((rtc.weekday + (dayAfter - dayBefore)) % 7);

			int year = 0;
			u32 day = dayAfter;
			while (true)
			{
				u32 length = LeapYear(year) ? 366 : 365;
				if (day < length) break;
				day -= length;
				year++;
			}
			int month = 1;
			while (true)
			{
				u32 length = (u32)MonthDays(year, month);
				if (day < length) break;
				day -= length;
				month++;
			}

			rtc.year = year % 100;			// the field wraps at 2100
			rtc.month = month;
			rtc.day = (int)day + 1;
			rtc.hour = (int)(rest / 3600);
			rtc.minute = (int)((rest / 60) % 60);
			rtc.second = (int)(rest % 60);
		}

		// -----------------------------------------------------------------------------------
		// EEPROM bit counts
		// -----------------------------------------------------------------------------------

		// GBATEK "GBA Cart Backup EEPROM": the requests are
		//   read:  2 bits "11" + n address bits + 1 bit "0"
		//   write: 2 bits "10" + n address bits + 64 data bits + 1 bit "0"
		// with n = 6 (512 Byte chip) or 14 (8 KByte chip), and the data read back is
		//   4 ignored bits + 64 data bits.
		u32 EepromRequestBits(int addressBits) { return 2 + (u32)addressBits + 1; }
		u32 EepromWriteBits(int addressBits) { return 2 + (u32)addressBits + 64 + 1; }
		const u32 EepromReadBits = 4 + 64;
	}

	// ---------------------------------------------------------------------------------------
	// Lifecycle
	// ---------------------------------------------------------------------------------------

	void Cart::Reset()
	{
		// A reset leaves the ROM and the battery backed save memory alone: it only clears the
		// command/bit stream state machines and the port registers, exactly like the cartridge
		// hardware surviving a reset of the console. The RTC chip is powered by its own battery
		// and keeps running; the emulator takes the time from the host clock again.
		flashState = 0;
		flashBank = 0;
		flashIdMode = 0;
		flashCmdCount = 0;

		eepromBits = 0;
		eepromBuffer = 0;
		eepromReadMode = false;
		eepromChipSelect = false;
		eepromOutput = 1;			// idle: bit0 high, the "ready" the games poll
		eepromAddress = 0;

		gpioData = 0;
		gpioDirection = 0;
		gpioControl = 0;
		gpioPrevious = 0;

		rtcState = 0;
		rtcCommand = 0;
		rtcParameter = 0;
		rtcReadMode = false;
		rtcControl = RtcControlPower | 0x02;	// S-3511A power on state, 82h (GBATEK)
		rtcSetByGame = false;
		rtcHostSecond = HostSecond();
		for (int i = 0; i < 8; i++)
			rtcResponse[i] = 0;
		ReadHostClock(rtc);
	}

	bool Cart::LoadRom(std::vector<u8> image, std::string& error)
	{
		error.clear();

		if (image.empty())
		{
			error = "the ROM image is empty";
			return false;
		}
		if (image.size() > MaxRomBytes)
		{
			error = "the ROM image is " + std::to_string(image.size()) +
				" bytes, the largest Game Pak is " + std::to_string(MaxRomBytes) + " bytes";
			return false;
		}

		rom = std::move(image);

		// GBATEK "GBA Cartridge ROM": the ROM is mirrored through its size, the cartridge
		// latches the lower 16 address bits. Real chips are powers of two, but homebrew,
		// multiboot and test images are arbitrary sizes, so the reads are masked through the
		// next power of two and the partial last bank reads as open bus (0FFh) - on hardware it
		// is open bus as well, only the mirroring of that bank is a simplification.
		romMask = 1;
		while (romMask < rom.size())
			romMask <<= 1;
		romMask -= 1;

		if (rom.size() < 0xC0)
			Log(LogLevel::Warn, "ROM: %u bytes is too short for a cartridge header", (unsigned)rom.size());
		if (!HeaderChecksumOk())
			Log(LogLevel::Warn, "ROM: the header complement check is wrong (loading the image anyway)");

		DetectSaveType();
		ResetSaveMemory();
		DetectRtc();

		Reset();
		saveDirty = false;			// a freshly loaded cartridge has nothing to write back

		Log(LogLevel::Info, "ROM: \"%s\" (%s), %u bytes, %s%s",
			Title().c_str(), GameCode().c_str(), (unsigned)rom.size(), SaveTypeName(),
			rtcEnabled ? ", GPIO/RTC port" : "");

		return true;
	}

	bool Cart::LoadRomFile(const std::string& path, std::string& error)
	{
		error.clear();

		if (path.empty())
		{
			error = "no ROM file name given";
			return false;
		}

		// The usual extensions are .gba, .bin, .agb and .rom; multiboot and test images use
		// whatever name they like, so the extension is not checked.
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file)
		{
			error = "cannot open the ROM file \"" + path + "\"";
			return false;
		}

		std::streamoff length = file.tellg();
		if (length <= 0)
		{
			error = "the ROM file \"" + path + "\" is empty";
			return false;
		}
		if ((u64)length > MaxRomBytes)
		{
			error = "the ROM file \"" + path + "\" is " + std::to_string((long long)length) +
				" bytes, the largest Game Pak is " + std::to_string(MaxRomBytes) + " bytes";
			return false;
		}

		file.seekg(0, std::ios::beg);
		std::vector<u8> image((size_t)length);
		if (!file.read((char*)image.data(), length))
		{
			error = "cannot read the ROM file \"" + path + "\"";
			return false;
		}

		return LoadRom(std::move(image), error);
	}

	void Cart::Eject()
	{
		rom.clear();
		romMask = 0;
		save.clear();
		saveDirty = false;
		saveType = SaveType::None;
		rtcEnabled = false;
		Reset();
		Log(LogLevel::Info, "ROM: cartridge removed");
	}

	// ---------------------------------------------------------------------------------------
	// The ROM window
	// ---------------------------------------------------------------------------------------

	u8 Cart::RomByte(u32 index) const
	{
		// The address decoder mirrors the ROM through its size; the bytes of the partial last
		// bank are not there (open bus, which the bus region models as 0FFh).
		index &= romMask;
		return (index < rom.size()) ? rom[index] : 0xFF;
	}

	u8 Cart::ReadRom8(u32 offset) const
	{
		if (rom.empty())
			return 0xFF;
		return RomByte(offset);
	}

	u16 Cart::ReadRom16(u32 offset) const
	{
		if (rom.empty())
			return 0xFFFF;

		// An EEPROM read transfer shifts one bit out per access, so the read path has to
		// advance a state machine. The header declares the ROM reads const (the CPU's path
		// only inspects the image), hence the const_cast: a Cart is never really const.
		Cart* self = const_cast<Cart*>(this);
		if (self->EepromDriving())
			return self->EepromReadWord();

		// The cartridge bus is 16 bits wide: a read at an odd address returns the *aligned*
		// halfword instead of rotating the byte lanes (the 32bit regions do rotate; the ROM
		// does not, which is why ARM code should not use unaligned ROM accesses).
		u32 index = (offset & romMask) & ~1u;
		u16 low = RomByte(index);
		u16 high = RomByte(index + 1);
		return (u16)(low | (high << 8));
	}

	u32 Cart::ReadRom32(u32 offset) const
	{
		if (rom.empty())
			return 0xFFFFFFFF;

		Cart* self = const_cast<Cart*>(this);
		if (self->EepromDriving())
		{
			// GBATEK "GBA System Control": a 32bit access is split into two 16bit accesses,
			// so it clocks two bits out of the EEPROM (the low halfword first).
			u32 low = self->EepromReadWord();
			u32 high = self->EepromReadWord();
			return low | (high << 16);
		}

		u16 low = ReadRom16(offset);
		u16 high = ReadRom16(offset + 2);
		return ((u32)high << 16) | low;
	}

	void Cart::WriteRom8(GbaBus& bus, u32 offset, u8 value)
	{
		// GBATEK "GBA Cart I/O Port (GPIO)": ROM bus writes are limited to 16bit/32bit access,
		// STRB opcodes into the ROM area are ignored.
		(void)bus;
		(void)offset;
		(void)value;
	}

	void Cart::WriteRom16(GbaBus& bus, u32 offset, u16 value)
	{
		// The ROM side needs no help from the bus: the EEPROM write is completed inside this
		// call, so a game that polls the DMA3 enable bit right after starting the transfer
		// already sees it finished.
		(void)bus;
		(void)offset;

		if (rom.empty())
			return;					// no cartridge: the write is ignored

		if (saveType == SaveType::Eeprom512B || saveType == SaveType::Eeprom8K)
		{
			EepromWriteBit(value);
			return;
		}

		// A plain ROM ignores writes; the Flash commands arrive through the SRAM window
		// (0x0E000000) instead, so nothing else can get here.
		Log(LogLevel::Debug, "ROM: write %04X to the ROM window ignored", value);
	}

	int Cart::WaitStates(u32 offset, bool sequential, int waitcnt) const
	{
		// GBATEK "4000204h - WAITCNT": the Game Pak ROM is mirrored into three regions with
		// separate (first, second) access timings. The offset is relative to 0x08000000, so
		// bits 25-26 select the region (0 = wait state 0, 1 = wait state 1, 2 = wait state 2).
		int region = (int)((offset >> 25) & 3);

		int firstIndex = (region == 1) ? (int)Bits((u16)waitcnt, 5, (u16)3)
			: (region == 2) ? (int)Bits((u16)waitcnt, 8, (u16)3)
			: (int)Bits((u16)waitcnt, 2, (u16)3);

		int second = (region == 1) ? (((waitcnt & 0x0080) != 0) ? 1 : 4)
			: (region == 2) ? (((waitcnt & 0x0400) != 0) ? 1 : 8)
			: (((waitcnt & 0x0010) != 0) ? 1 : 2);

		// GBATEK "GBA GamePak Prefetch": with the prefetch buffer running, the CPU is served
		// from the eight halfword buffer, which is the cheap form of "the second access is one
		// cycle when the buffer is on and the region runs at its lowest first access time".
		if (sequential && (waitcnt & 0x4000) != 0 && firstIndex == 2)
			return 0;

		int waits = sequential ? second : RomFirstWaits[firstIndex & 3];

		// A 128 Mbit (16 MByte) pak takes one cycle longer on the first access: the bigger
		// chips latch the address one cycle later (this is the rule the cartridge chapter of
		// the brief asks for; GBATEK's WAITCNT notes call the 128 Mbit paks out as the slow
		// ones, without giving the exact number).
		if (!sequential && rom.size() >= 16 * 1024 * 1024)
			waits += 1;

		return waits;
	}

	// ---------------------------------------------------------------------------------------
	// The header
	// ---------------------------------------------------------------------------------------

	std::string Cart::ReadHeaderText(u32 offset, size_t length) const
	{
		if (rom.size() < (size_t)offset + length)
			return std::string();

		std::string text;
		for (size_t i = 0; i < length; i++)
		{
			char c = (char)rom[offset + i];
			if (c == '\0')
				break;
			text.push_back(c);
		}
		while (!text.empty() && (text.back() == ' ' || text.back() == '\0'))
			text.pop_back();
		return text;
	}

	std::string Cart::Title() const
	{
		// GBATEK "GBA Cartridge Header": 0A0h, 12 characters, padded with 00h (older tools pad
		// with spaces, both are trimmed).
		return ReadHeaderText(0xA0, 12);
	}

	std::string Cart::GameCode() const
	{
		return ReadHeaderText(0xAC, 4);
	}

	std::string Cart::MakerCode() const
	{
		return ReadHeaderText(0xB0, 2);
	}

	bool Cart::HeaderChecksumOk() const
	{
		// GBATEK "0BDh - Complement check": chk = 0; for i = 0A0h to 0BCh: chk = chk - [i];
		// chk = (chk - 19h) and 0FFh. The stored byte therefore makes the sum of 0A0h..0BCh
		// plus 19h plus itself wrap to zero.
		if (rom.size() < 0xBE)
			return false;

		u32 sum = 0x19 + rom[0xBD];
		for (u32 i = 0xA0; i <= 0xBC; i++)
			sum += rom[i];
		return (sum & 0xFF) == 0;
	}

	// ---------------------------------------------------------------------------------------
	// Save type detection
	// ---------------------------------------------------------------------------------------

	void Cart::DetectSaveType()
	{
		saveType = SaveType::None;
		eepromAddressBits = 6;
		if (rom.empty())
			return;

		// The ID string is word aligned ("GBA Cart Backup IDs"). The scan runs over the whole
		// image in 4 byte steps, which covers the first bytes right after the header as well as
		// the last 64 KByte where the save library of a bigger game ends up.
		for (size_t base = 0; base + MaxSignatureLength <= rom.size(); base += 4)
		{
			const SaveSignature* hit = nullptr;
			for (const SaveSignature& signature : SaveSignatures)
			{
				if (memcmp(rom.data() + base, signature.text, strlen(signature.text)) == 0)
				{
					hit = &signature;
					break;
				}
			}
			if (hit == nullptr)
				continue;

			if (hit->type == SaveType::Eeprom512B)
			{
				// GBATEK "GBA Cart Backup EEPROM" says there is no autodetection mechanism:
				// the bus width (6 or 14 bits) has to be hardcoded, found in the game's code or
				// taken from a database. The heuristic used here: a 128 Mbit class image
				// (16 MByte and up) carries the 8 KByte chip (the RTC games like Boktai),
				// everything else the 512 Byte chip. SetSaveType() overrides it.
				bool large = rom.size() >= 16 * 1024 * 1024;
				saveType = large ? SaveType::Eeprom8K : SaveType::Eeprom512B;
				eepromAddressBits = large ? 14 : 6;
				Log(LogLevel::Info, "save: \"%s\" at %06zX, using the %s chip (ROM size heuristic)",
					hit->text, base, SaveTypeName());
			}
			else
			{
				saveType = hit->type;
				eepromAddressBits = 6;
				Log(LogLevel::Info, "save: \"%s\" at %06zX, using %s", hit->text, base, SaveTypeName());
			}
			return;
		}

		Log(LogLevel::Warn, "save: no save type signature in the ROM (SetSaveType can force one)");
	}

	void Cart::DetectRtc()
	{
		rtcEnabled = false;
		if (rom.empty())
			return;

		// GBATEK "GBA Cart I/O Port (GPIO)" describes no header flag for the add-ons: the port
		// is part of the ROM chip and only answers when the game drives it. Games that use it
		// (Boktai's RTC and solar sensor, WarioWare Twisted's gyro and rumble, the rumble paks)
		// load the register addresses 080000C4h and 080000C8h as literals, so their presence in
		// the image is a good hint. The UI can override the result with SetRtcEnabled().
		bool data = false;
		bool control = false;
		for (size_t i = 0; i + 4 <= rom.size() && !(data && control); i += 4)
		{
			u32 word = (u32)rom[i] | ((u32)rom[i + 1] << 8) | ((u32)rom[i + 2] << 16) | ((u32)rom[i + 3] << 24);
			if (word == 0x080000C4) data = true;
			else if (word == 0x080000C8) control = true;
		}
		rtcEnabled = data && control;
		if (rtcEnabled)
			Log(LogLevel::Info, "ROM: the cartridge uses the GPIO port (RTC/sensor)");
	}

	// ---------------------------------------------------------------------------------------
	// The save memory window (0x0E000000, 64 KByte)
	// ---------------------------------------------------------------------------------------

	size_t Cart::SaveSize() const
	{
		switch (saveType)
		{
		case SaveType::Sram32K:		return 32 * 1024;
		case SaveType::Flash64K:	return 64 * 1024;
		case SaveType::Flash128K:	return 128 * 1024;
		case SaveType::Eeprom512B:	return 512;
		case SaveType::Eeprom8K:	return 8 * 1024;
		default:					return 0;
		}
	}

	void Cart::ResetSaveMemory()
	{
		// The erased state of Flash and EEPROM is all ones, and a missing .sav file starts from
		// the same state (SRAM has no defined content, ones is the friendliest choice).
		save.assign(SaveSize(), 0xFF);
	}

	u8 Cart::ReadSave(u32 offset)
	{
		switch (saveType)
		{
		case SaveType::Sram32K:
			if (save.empty()) return 0xFF;
			// The 64 KByte window mirrors the 32 KByte chip.
			return save[offset & (u32)(save.size() - 1)];

		case SaveType::Flash64K:
		case SaveType::Flash128K:
			return FlashRead(offset);

		default:
			// No save memory at all, or an EEPROM (which is not in the SRAM window): the bus
			// floats high.
			return 0xFF;
		}
	}

	void Cart::WriteSave(u32 offset, u8 value)
	{
		switch (saveType)
		{
		case SaveType::Sram32K:
			if (save.empty()) return;
			save[offset & (u32)(save.size() - 1)] = value;
			saveDirty = true;
			return;

		case SaveType::Flash64K:
		case SaveType::Flash128K:
			FlashWrite(offset, value);
			return;

		default:
			return;
		}
	}

	// ---------------------------------------------------------------------------------------
	// Flash (the SST command set, GBATEK "GBA Cart Backup Flash ROM")
	// ---------------------------------------------------------------------------------------

	u8 Cart::FlashRead(u32 offset)
	{
		if (save.empty())
			return 0xFF;

		// The window is 64 KByte wide and the bank select command picks the 64 KByte half of a
		// 128 KByte device (GBATEK "Bank Switching").
		u32 index = (offset & (SaveWindowSize - 1));
		index += (u32)flashBank * 0x10000;
		index &= (u32)(save.size() - 1);

		if (flashIdMode == 1)
		{
			// GBATEK "Chip Identification": the ID is read as two bytes, the manufacturer at
			// 0E000000h and the device at 0E000001h. The device table lists the 16bit code with
			// the device in the MSB and the manufacturer in the LSB:
			//   D4BFh SST 64K      1CC2h Macronix 64K   1B32h Panasonic 64K   3D1Fh Atmel 64K
			//   1362h Sanyo 128K   09C2h Macronix 128K
			// One chip model reports one pair per size: the SST part for 64 KByte (the one
			// Nintendo used most) and the Sanyo part for 128 KByte.
			bool large = save.size() > 0x10000;
			if (index & 1)
				return large ? 0x13 : 0xD4;		// device
			return large ? 0x62 : 0xBF;			// manufacturer
		}

		if (flashIdMode == 2)
		{
			// Command 98h: the CFI query (JEDEC JESD68). GBATEK does not document the layout;
			// the three magic bytes are answered so a probing game sees a CFI capable chip, the
			// device size follows the part we report. Nothing in the array is touched.
			switch (index & 0xFF)
			{
			case 0x10: return 'Q';
			case 0x11: return 'R';
			case 0x12: return 'Y';
			case 0x2C: return (save.size() > 0x10000) ? 0x11 : 0x10;	// 2^N bytes
			default: return 0x00;
			}
		}

		return save[index];
	}

	void Cart::FlashWrite(u32 offset, u8 value)
	{
		u32 address = offset & 0xFFFF;

		// GBATEK "Terminate ID mode" / "Terminate Command after Timeout": F0h ends the ID (or
		// CFI) mode and abandons a pending erase from any state. Some SST parts accept a lone
		// F0h without the unlock sequence, which is what the GBA libraries do. In the program
		// state F0h is data like any other byte, not a command.
		if (value == 0xF0 && (flashState == 0 || flashState == 3))
		{
			flashIdMode = 0;
			flashState = 0;
			flashCmdCount = 0;
			return;
		}

		switch (flashState)
		{
		case 0:		// read array: the unlock sequence starts here
			if (address == 0x5555 && value == 0xAA)
			{
				flashState = 1;
				flashCmdCount = 1;
			}
			return;

		case 1:		// the unlock sequence (AAh@5555h, 55h@2AAAh, command@5555h)
			if (flashCmdCount == 1)
			{
				if (address == 0x2AAA && value == 0x55)
				{
					flashCmdCount = 2;
					return;
				}
				// Anything else aborts the sequence and leaves the array readable.
				flashState = 0;
				flashCmdCount = 0;
				return;
			}
			if (address != 0x5555)
			{
				flashState = 0;
				flashCmdCount = 0;
				return;
			}
			flashState = 0;
			flashCmdCount = 0;
			switch (value)
			{
			case 0x90:	flashIdMode = 1; return;			// read the ID
			case 0x98:	flashIdMode = 2; return;			// CFI query
			case 0xF0:	flashIdMode = 0; return;			// reset
			case 0x80:	flashState = 3; return;				// erase setup
			case 0xA0:	flashState = 4; return;				// byte program
			case 0xB0:	flashState = 2; return;				// bank select (128 KByte only)
			default:
				// An unknown command must not corrupt the array (GBATEK lists only the
				// commands above for the SST parts).
				Log(LogLevel::Warn, "flash: unknown command %02X ignored", value);
				return;
			}

		case 2:		// bank select: the next write carries the bank number
			// GBATEK "Bank Switching": [E000000h]=bnk, the address of the write is ignored.
			if (save.size() > 0x10000)
			{
				flashBank = value & 1;
				saveDirty = true;
			}
			else
			{
				Log(LogLevel::Warn, "flash: bank select on a 64 KByte device ignored");
			}
			flashState = 0;
			flashCmdCount = 0;
			return;

		case 3:		// erase: a second unlock sequence, then 30h (sector) or 10h (chip)
			if (flashCmdCount == 0)
			{
				// A write that is not the start of the sequence abandons the pending erase:
				// without that, a stray 30h later on could erase a sector the game never
				// asked for.
				if (address == 0x5555 && value == 0xAA) flashCmdCount = 1;
				else { flashState = 0; flashCmdCount = 0; }
				return;
			}
			if (flashCmdCount == 1)
			{
				if (address == 0x2AAA && value == 0x55) flashCmdCount = 2;
				else { flashState = 0; flashCmdCount = 0; }
				return;
			}
			if (address == 0x5555 && value == 0x10)
			{
				// Chip erase: every sector of the device becomes FFh.
				for (size_t i = 0; i < save.size(); i++)
					save[i] = 0xFF;
				saveDirty = true;
				Log(LogLevel::Info, "flash: chip erased");
			}
			else if (value == 0x30)
			{
				// Sector erase: 4 KByte at the given sector address, inside the selected bank.
				u32 start = ((u32)flashBank * 0x10000 + (address & 0xF000)) & (u32)(save.size() - 1);
				for (u32 i = 0; i < 0x1000 && start + i < save.size(); i++)
					save[start + i] = 0xFF;
				saveDirty = true;
				Log(LogLevel::Info, "flash: sector %04X erased", address & 0xF000);
			}
			else
			{
				Log(LogLevel::Warn, "flash: erase command %02X ignored", value);
			}
			flashState = 0;
			flashCmdCount = 0;
			return;

		case 4:		// byte program
		{
			// A Flash cell can only be pulled from 1 to 0, so the programmed byte reads back as
			// the AND of the old and the new value until the sector is erased. That is the
			// quirk GBATEK's "wait until [E00xxxxh]=dat" warning is about; modelling it keeps
			// the verify-retry loops of the games honest.
			u32 index = ((u32)flashBank * 0x10000 + address) & (u32)(save.size() - 1);
			save[index] = (u8)(save[index] & value);
			saveDirty = true;
			flashState = 0;
			flashCmdCount = 0;
			return;
		}

		default:
			flashState = 0;
			flashCmdCount = 0;
			return;
		}
	}

	// ---------------------------------------------------------------------------------------
	// EEPROM (GBATEK "GBA Cart Backup EEPROM")
	//
	// The chip has no address bus of its own: it hangs off bit0 of the data bus and off the top
	// address bit, and talks in bits. The games put one bit per halfword into a stack buffer and
	// let DMA3 move the buffer to/from the ROM window, so every 16bit write in the ROM window
	// clocks one bit in and every 16bit read clocks one bit out, most significant bit first:
	//
	//   write request  1100 0000 ...  1 halfword per bit:
	//     bit 0     "1"  (the leading start bit of both requests)
	//     bit 1     "0"  (0 = write, 1 = read)
	//     bit 2..7  6 bit block address, MSB first (14 address bits for the 8 KByte chip)
	//     ...       64 data bits, MSB first (8 bytes, the first byte's MSB first)
	//     last bit  "0"  the trailing dummy bit that raises the chip select
	//   read request   "11" + address bits + "0" (9 or 17 halfwords), then 68 reads:
	//     4 ignored bits, then the same 64 data bits.
	//
	// The chip select is not visible to us (the bus only passes the halfword), so the transfer
	// is ended by the bit count: exactly what the DMA3 length tells the chip on hardware.
	// ---------------------------------------------------------------------------------------

	bool Cart::EepromDriving() const
	{
		// True while the chip shifts its 64bit block out: after the read request's dummy bit
		// and until the 68th read has been served.
		return eepromReadMode && eepromBits >= EepromRequestBits(eepromAddressBits);
	}

	void Cart::EepromWriteBit(u16 value)
	{
		if (saveType != SaveType::Eeprom512B && saveType != SaveType::Eeprom8K)
			return;
		if (save.empty())
			return;

		u32 addressBits = (u32)eepromAddressBits;
		u32 requestBits = EepromRequestBits(eepromAddressBits);
		u32 writeBits = EepromWriteBits(eepromAddressBits);
		u32 bit = value & 1;

		if (eepromBits == 0)
		{
			// The first bit starts a fresh transfer.
			eepromBuffer = 0;
			eepromAddress = 0;
			eepromReadMode = false;
			eepromChipSelect = true;
			eepromOutput = 0;
		}
		else if (EepromDriving())
		{
			// The chip is already driving the bus; on hardware this would be a new transfer,
			// but the games start one only after the bus was released.
			return;
		}

		eepromBits++;

		if (eepromBits == 1)
		{
			if (bit == 0)
			{
				// Both requests start with a 1 bit; anything else is not a request.
				eepromBits = 0;
				eepromChipSelect = false;
			}
			return;
		}

		if (eepromBits == 2)
		{
			eepromReadMode = (bit != 0);
			return;
		}

		if (eepromBits <= 2 + addressBits)
		{
			eepromAddress = (eepromAddress << 1) | bit;		// MSB first
			return;
		}

		if (!eepromReadMode)
		{
			u32 dataBit = eepromBits - (2 + addressBits);	// 1..64 = data, 65 = the dummy bit
			if (dataBit <= 64)
				eepromBuffer = (eepromBuffer << 1) | bit;
			if (eepromBits >= writeBits)
				EepromFinish();
			return;
		}

		// A read request: the last (dummy) bit turns the bus around.
		if (eepromBits >= requestBits)
			EepromFinish();
	}

	void Cart::EepromFinish()
	{
		if (save.empty())
		{
			eepromBits = 0;
			eepromChipSelect = false;
			return;
		}

		// Addressing works in units of 64 bits and the block number wraps through the chip's
		// address decoder: the 512 Byte chip has 64 blocks (a 6 bit address field), the
		// 8 KByte chip 1024 (a 14 bit field of which only the lower 10 bits are used, GBATEK
		// "Data and Address Width"). Both follow from the array size.
		u32 blocks = (u32)(save.size() / 8);
		u32 block = eepromAddress & (blocks - 1);
		u32 offset = block * 8;
		if (offset + 8 > save.size())
		{
			// Defensive: a misdetected width must not write outside the array.
			Log(LogLevel::Warn, "EEPROM: block %X is outside the %u byte chip, ignored",
				block, (unsigned)save.size());
			eepromBits = 0;
			eepromChipSelect = false;
			eepromOutput = 1;
			return;
		}

		if (eepromReadMode)
		{
			// Latch the block: the reads at the EEPROM address shift these 64 bits out.
			u64 data = 0;
			for (u32 i = 0; i < 8; i++)
				data = (data << 8) | save[offset + i];
			eepromBuffer = data;
			eepromBits = EepromRequestBits(eepromAddressBits);
			eepromOutput = 0;						// the first of the four ignored bits
			return;
		}

		// A write request programs (and internally erases) the whole 64bit block at once.
		for (u32 i = 0; i < 8; i++)
			save[offset + i] = (u8)(eepromBuffer >> (56 - i * 8));

		saveDirty = true;
		eepromBits = 0;
		eepromChipSelect = false;
		eepromBuffer = 0;
		eepromOutput = 1;							// ready: the games poll bit0 after a write
	}

	u16 Cart::EepromReadWord()
	{
		u16 out = eepromOutput;

		u32 requestBits = EepromRequestBits(eepromAddressBits);
		if (EepromDriving())
		{
			eepromBits++;
			u32 done = eepromBits - requestBits;	// how many of the 68 bits have been read

			if (done >= EepromReadBits)
			{
				// The 68 bit stream is over: the chip releases the bus (bit0 reads high).
				eepromBits = 0;
				eepromReadMode = false;
				eepromChipSelect = false;
				eepromOutput = 1;
			}
			else if (done < 4)
			{
				eepromOutput = 0;					// the four ignored bits
			}
			else
			{
				u32 index = done - 4;				// 0..63, MSB first
				eepromOutput = (u16)((eepromBuffer >> (63 - index)) & 1);
			}
		}

		return out;
	}

	// ---------------------------------------------------------------------------------------
	// GPIO (GBATEK "GBA Cart I/O Port (GPIO)")
	// ---------------------------------------------------------------------------------------

	bool Cart::GpioEnabled() const
	{
		// GBATEK: bit0 of the control register selects write-only (0) or read/write (1) access
		// for the other two registers. In write-only mode the ROM bytes show through on reads.
		return rtcEnabled && (gpioControl & 1) != 0;
	}

	u8 Cart::ReadGpio(u32 offset)
	{
		// The caller passes the offset inside the 0x080000C0 window: 04h/06h/08h, or the
		// 0C4h/0C6h/0C8h addresses when the whole ROM offset is passed through.
		u32 reg = offset & 0xFF;

		if (!GpioEnabled())
		{
			// The port is not there, or the game has not enabled read access: the ROM bytes at
			// 080000C4h..080000C8h answer (GBATEK "In write-only mode, reads return 00h (or
			// possible other data, if the rom contains non-zero data at that location)").
			if (reg == 0x04 || reg == 0xC4) return RomByte(0xC4);
			if (reg == 0x06 || reg == 0xC6) return RomByte(0xC6);
			if (reg == 0x08 || reg == 0xC8) return RomByte(0xC8);
			return 0xFF;
		}

		if (reg == 0x04 || reg == 0xC4)
		{
			u8 value = (u8)(gpioData & 0x0F);
			// The RTC drives the data line while the game has it configured as an input
			// (direction bit1 clear); otherwise the port reads back what it wrote.
			if ((gpioDirection & 0x02) == 0)
				value = (u8)((value & ~0x02) | (RtcReadBit() << 1));
			return value;
		}
		if (reg == 0x06 || reg == 0xC6)
			return (u8)(gpioDirection & 0x0F);
		if (reg == 0x08 || reg == 0xC8)
			return (u8)(gpioControl & 1);
		return 0xFF;
	}

	void Cart::WriteGpio(u32 offset, u8 value)
	{
		// Without the port hardware these addresses are ordinary ROM bytes, and a write into
		// the ROM window is ignored.
		if (!rtcEnabled)
			return;

		u32 reg = offset & 0xFF;

		if (reg == 0x08 || reg == 0xC8)
		{
			// GBATEK: the control register's bit0 switches between write-only and read/write.
			gpioControl = (u8)(value & 1);
			if (!gpioControl)
			{
				// Write-only again: the RTC sees the chip select drop.
				rtcState = 0;
				rtcReadMode = false;
			}
			gpioPrevious = gpioData;
			return;
		}

		if (reg == 0x06 || reg == 0xC6)
		{
			// Only the lower nibble is the direction; the rest is not used.
			gpioDirection = (u8)(value & 0x0F);
			return;
		}

		if (reg == 0x04 || reg == 0xC4)
		{
			// The data register is written in both modes (write-only still drives the RTC);
			// the RTC watches the SCK and chip select edges of every write.
			gpioPrevious = gpioData;
			gpioData = (u8)(value & 0x0F);
			RtcClock();
		}
	}

	// ---------------------------------------------------------------------------------------
	// The Seiko S-3511A RTC (GBATEK "GBA Cart Real-Time Clock (RTC)")
	//
	// The chip hangs off GPIO bits 0..2: SCK (clock), SIO (data) and CS (chip select)
	// (GBATEK's GPIO connection table). Its protocol is the same three wire protocol as the
	// DS RTC's: CS high selects the chip, the command byte and the parameter bytes are shifted
	// in LSB first on the rising clock edge, and the chip drives the data line itself on the
	// falling edge while the game reads it.
	//
	// The on-the-wire commands are 60h..6Fh: bit0 is the read flag (1 = read, the chip drives
	// the data line), bits 1-3 select one of the eight register groups (GBATEK's table maps the
	// GBA chip's groups onto the DS ones):
	//
	//   0  force reset          0 bytes   (strobed by any access, read or write)
	//   1  control/status       1 byte    12/24 hour mode, per minute IRQ, power flags
	//   2  datetime             7 bytes   year, month, day, weekday, hour, minute, second
	//   3  time                 3 bytes   hour, minute, second
	//   4  alarm INT1           1 byte    (reads back FFh on the GBA chip)
	//   5  alarm 2              2 bytes   (reads back FFh)
	//   6  force IRQ            0 bytes
	//   7  free                 1 byte    (reads back FFh)
	// ---------------------------------------------------------------------------------------

	bool Cart::RtcSelected() const
	{
		return rtcEnabled && (gpioData & 0x04) != 0;
	}

	void Cart::RtcTick()
	{
		// The cartridge's RTC runs from its own battery, which the emulator substitutes with the
		// machine clock (the time the frontend's system has). Once the game has written the
		// clock the written value is kept and advanced by the host's elapsed seconds instead, so
		// a game that sets the time sees its own value and then a running clock.
		u32 now = HostSecond();
		if (!rtcSetByGame)
		{
			ReadHostClock(rtc);
			rtcHostSecond = now;
			return;
		}

		u32 elapsed = (now - rtcHostSecond) & 0xFFFF;
		if (elapsed != 0)
		{
			AddSeconds(rtc, elapsed);
			rtcHostSecond = now;
		}
	}

	int Cart::RtcParameterBytes(u8 command) const
	{
		switch ((command >> 1) & 7)
		{
		case 0: return 0;		// force reset
		case 1: return 1;		// control/status
		case 2: return 7;		// datetime
		case 3: return 3;		// time
		case 4: return 1;		// alarm, reads back FFh
		case 5: return 2;		// alarm 2, reads back FFh
		case 6: return 0;		// force IRQ
		default: return 1;		// free register, reads back FFh
		}
	}

	void Cart::RtcReset()
	{
		// GBATEK "Force Reset": all registers become 00h, except day and month which become 01h,
		// and the control register goes to 00h.
		rtc.year = 0;
		rtc.month = 1;
		rtc.day = 1;
		rtc.weekday = 0;
		rtc.hour = 0;
		rtc.minute = 0;
		rtc.second = 0;
		rtc.centisecond = 0;
		rtcControl = 0;
		rtcSetByGame = false;
		rtcHostSecond = HostSecond();
	}

	u8 Cart::RtcHourByte() const
	{
		// GBATEK "Datetime and Time Registers": the GBA's AM/PM flag sits in bit7 of the hour
		// byte (bit6 on the DS). In 24 hour mode the flag is forced by the value (PM = hour >=
		// 12), in 12 hour mode the hour runs 00h..11h with 12 o'clock stored as 00h.
		u8 hour = (rtcControl & RtcControlHour24) ? ToBcd(rtc.hour) : ToBcd(rtc.hour % 12);
		return (u8)(hour | (rtc.hour >= 12 ? 0x80 : 0x00));
	}

	void Cart::RtcWriteHour(u8 value)
	{
		// In 24 hour mode the AM/PM bit is ignored (it is forced when read); in 12 hour mode it
		// is the stored state that selects the half of the day.
		if (rtcControl & RtcControlHour24)
		{
			rtc.hour = BcdField((u8)(value & 0x3F), 0, 23);
			return;
		}
		int hour = BcdField((u8)(value & 0x3F), 0, 11);
		bool pm = (value & 0x80) != 0;
		rtc.hour = (hour % 12) + (pm ? 12 : 0);
	}

	void Cart::RtcWriteParameter(u32 index, u8 value)
	{
		switch ((rtcCommand >> 1) & 7)
		{
		case 1:
			// The control register. The power flag is read only and is only cleared by reading
			// it, so it survives a write.
			rtcControl = (u8)((rtcControl & RtcControlPower) | (value & RtcControlWritable));
			return;

		case 2:
			// The datetime registers (all BCD except the weekday counter).
			switch (index)
			{
			case 0: rtc.year = BcdField(value, 0, 99); break;
			case 1: rtc.month = BcdField(value, 1, 12); break;
			case 2: rtc.day = BcdField(value, 1, 31); break;
			case 3: rtc.weekday = value & 7; break;
			case 4: RtcWriteHour(value); break;
			case 5: rtc.minute = BcdField(value, 0, 59); break;
			default: rtc.second = BcdField(value, 0, 59); break;
			}
			rtcSetByGame = true;
			return;

		case 3:
			// The time registers.
			if (index == 0) RtcWriteHour(value);
			else if (index == 1) rtc.minute = BcdField(value, 0, 59);
			else rtc.second = BcdField(value, 0, 59);
			rtcSetByGame = true;
			return;

		default:
			// The alarm, clock adjust and free registers: the GBA cartridges do not use them,
			// the writes are accepted and dropped (GBATEK lists them as "always FFh").
			return;
		}
	}

	void Cart::RtcDecodeCommand()
	{
		u8 command = (u8)(rtcCommand & 0xFF);

		if ((command & 0xF0) != 0x60)
		{
			// Not an S-3511A command (all eight groups live in 60h..6Fh): the chip ignores the
			// rest of the transfer until the chip select drops.
			rtcState = -1;
			Log(LogLevel::Warn, "RTC: unknown command byte %02X ignored", command);
			return;
		}

		rtcReadMode = (command & 1) != 0;
		rtcParameter = 0;

		int group = (command >> 1) & 7;

		// The force reset register is strobed by any access to it, a read as well as a write
		// (GBATEK "Force Reset/Irq Registers").
		if (group == 0)
		{
			RtcReset();
			return;
		}

		if (!rtcReadMode)
			return;

		// Latch the bytes the chip will drive, so that each bit of the transfer sees the same
		// value (and the "cleared on read" flags are handled exactly once).
		for (int i = 0; i < 8; i++)
			rtcResponse[i] = 0xFF;

		switch (group)
		{
		case 1:
			// Reading the control register returns the power failure flag and clears it
			// (GBATEK: "auto cleared on read").
			rtcResponse[0] = rtcControl;
			rtcControl &= (u8)~RtcControlPower;
			break;

		case 2:
			rtcResponse[0] = ToBcd(rtc.year);
			rtcResponse[1] = ToBcd(rtc.month);
			rtcResponse[2] = ToBcd(rtc.day);
			rtcResponse[3] = (u8)(rtc.weekday & 7);
			rtcResponse[4] = RtcHourByte();
			rtcResponse[5] = ToBcd(rtc.minute);
			rtcResponse[6] = ToBcd(rtc.second);
			break;

		case 3:
			rtcResponse[0] = RtcHourByte();
			rtcResponse[1] = ToBcd(rtc.minute);
			rtcResponse[2] = ToBcd(rtc.second);
			break;

		default:
			// The alarm and free registers leave the data line high: FFh (GBATEK "always FFh").
			break;
		}
	}

	void Cart::RtcShiftIn(u32 bit)
	{
		if (rtcState < 0)
			return;								// the chip rejected this transfer

		if (rtcState < 8)
		{
			// The command byte, LSB first.
			rtcCommand |= (bit & 1) << rtcState;
			rtcState++;
			if (rtcState == 8)
				RtcDecodeCommand();
			return;
		}

		if (rtcReadMode)
			return;								// the chip drives the line during a read

		int params = RtcParameterBytes((u8)(rtcCommand & 0xFF));
		u32 index = ((u32)rtcState - 8) / 8;
		u32 position = ((u32)rtcState - 8) % 8;
		if (index >= (u32)params)
			return;								// the transfer is complete

		rtcParameter = (u8)(rtcParameter | ((bit & 1) << position));
		rtcState++;

		if (position == 7)
		{
			RtcWriteParameter(index, rtcParameter);
			rtcParameter = 0;
		}
	}

	void Cart::RtcClock()
	{
		RtcTick();

		bool clock = (gpioData & 0x01) != 0;
		bool data = (gpioData & 0x02) != 0;
		bool select = (gpioData & 0x04) != 0;
		bool wasClock = (gpioPrevious & 0x01) != 0;
		bool wasSelect = (gpioPrevious & 0x04) != 0;

		if (select && !wasSelect)
		{
			// The chip select rises: a new command byte starts here.
			rtcState = 0;
			rtcCommand = 0;
			rtcParameter = 0;
			rtcReadMode = false;
			return;
		}

		if (!select && wasSelect)
		{
			// The chip select drops: the transfer is over (a half finished byte is dropped,
			// like the chip does).
			rtcState = 0;
			rtcReadMode = false;
			return;
		}

		if (!select)
			return;

		if (clock && !wasClock)
		{
			// Rising clock edge: the chip takes the data bit (GBATEK "DS Real-Time Clock").
			RtcShiftIn(data ? 1 : 0);
			return;
		}

		if (!clock && wasClock)
		{
			// Falling clock edge: during a read the chip puts the next bit on the data line.
			int params = RtcParameterBytes((u8)(rtcCommand & 0xFF));
			if (rtcReadMode && params > 0 && rtcState < 8 + params * 8)
				rtcState++;
		}
	}

	u8 Cart::RtcReadBit()
	{
		if (!RtcSelected())
			return 1;							// not selected: the open drain line idles high
		if (rtcState < 8)
			return 1;							// the command byte is still being clocked in
		if (!rtcReadMode)
			return 1;							// a write transfer: the game drives the line

		int params = RtcParameterBytes((u8)(rtcCommand & 0xFF));
		if (params == 0)
			return 1;

		// The falling edge that precedes the first bit leaves rtcState at 8; each following
		// edge moves the index on by one.
		u32 index = (rtcState > 8) ? (u32)(rtcState - 9) : 0;
		if (index >= (u32)params * 8)
			index = (u32)params * 8 - 1;

		u8 value = rtcResponse[index / 8];
		return (u8)((value >> (index % 8)) & 1);	// LSB first, like the command byte
	}

	// ---------------------------------------------------------------------------------------
	// Save files
	// ---------------------------------------------------------------------------------------

	bool Cart::LoadSaveFile(const std::string& path, std::string* error)
	{
		if (error != nullptr)
			error->clear();

		size_t size = SaveSize();
		if (size == 0)
			return true;						// the cartridge has no save memory at all

		std::string target = path.empty() ? savePath : path;
		if (target.empty())
		{
			if (error != nullptr)
				*error = "no save file path was set";
			return false;
		}

		// Start from the erased state: a missing file is not an error, the game then sees a
		// freshly formatted cartridge (GBATEK: erased Flash/EEPROM reads FFh).
		save.assign(size, 0xFF);
		saveDirty = false;

		std::ifstream file(target, std::ios::binary | std::ios::ate);
		if (!file)
		{
			Log(LogLevel::Info, "save: \"%s\" does not exist yet, starting from an erased cartridge",
				target.c_str());
			return true;
		}

		std::streamoff length = file.tellg();
		if (length < 0)
		{
			if (error != nullptr)
				*error = "cannot read the save file \"" + target + "\"";
			return false;
		}

		if ((u64)length > size)
		{
			// A bigger file is a different cartridge's save (or a different save type): refuse
			// it instead of loading a truncated image.
			if (error != nullptr)
				*error = "the save file \"" + target + "\" is " + std::to_string((long long)length) +
					" bytes, the " + SaveTypeName() + " save memory is only " + std::to_string(size) + " bytes";
			Log(LogLevel::Warn, "save: \"%s\" is too large for %s, not loaded", target.c_str(), SaveTypeName());
			return false;
		}

		file.seekg(0, std::ios::beg);
		if (!file.read((char*)save.data(), length))
		{
			if (error != nullptr)
				*error = "cannot read the save file \"" + target + "\"";
			save.assign(size, 0xFF);
			return false;
		}

		// A shorter file leaves the rest of the memory erased (the assign above), which is how
		// a save written by a smaller cartridge is migrated.
		Log(LogLevel::Info, "save: loaded %lld bytes from \"%s\"", (long long)length, target.c_str());
		return true;
	}

	bool Cart::SaveSaveFile(const std::string& path, std::string* error)
	{
		if (error != nullptr)
			error->clear();

		size_t size = SaveSize();
		if (size == 0)
			return true;						// nothing to write

		std::string target = path.empty() ? savePath : path;
		if (target.empty())
		{
			if (error != nullptr)
				*error = "no save file path was set";
			return false;
		}

		if (!saveDirty)
		{
			Log(LogLevel::Debug, "save: \"%s\" is unchanged, not written", target.c_str());
			return true;
		}

		std::ofstream file(target, std::ios::binary | std::ios::trunc);
		if (!file)
		{
			if (error != nullptr)
				*error = "cannot write the save file \"" + target + "\"";
			return false;
		}

		file.write((const char*)save.data(), (std::streamsize)size);
		if (!file)
		{
			if (error != nullptr)
				*error = "cannot write the save file \"" + target + "\"";
			return false;
		}

		saveDirty = false;
		Log(LogLevel::Info, "save: wrote %u bytes to \"%s\"", (unsigned)size, target.c_str());
		return true;
	}

	void Cart::SetSaveType(SaveType type)
	{
		saveType = type;
		eepromAddressBits = (type == SaveType::Eeprom8K) ? 14 : 6;

		// The state machines refer to the old layout, and the memory changes size: start clean.
		flashState = 0;
		flashBank = 0;
		flashIdMode = 0;
		flashCmdCount = 0;
		eepromBits = 0;
		eepromBuffer = 0;
		eepromReadMode = false;
		eepromChipSelect = false;
		eepromOutput = 1;
		eepromAddress = 0;

		ResetSaveMemory();
		saveDirty = false;

		Log(LogLevel::Info, "save: type forced to %s", SaveTypeName());
	}

	const char* Cart::SaveTypeName() const
	{
		switch (saveType)
		{
		case SaveType::Sram32K:		return "SRAM 32K";
		case SaveType::Flash64K:	return "Flash 64K";
		case SaveType::Flash128K:	return "Flash 128K";
		case SaveType::Eeprom512B:	return "EEPROM 512B";
		case SaveType::Eeprom8K:	return "EEPROM 8K";
		default:					return "None";
		}
	}
}
