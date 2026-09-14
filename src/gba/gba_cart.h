// GBA cartridge: the ROM image, the save memory and the ROM-side hardware the games poke at.
//
// Covered here (GBATEK "GBA Cartridges"):
//
//   * the ROM image and the header fields the emulator needs (title, game code, the
//     "complement check" value and the save-type signature strings);
//   * save memory: SRAM (32 KByte), Flash (64 KByte, with the SST 39VF512/1M command set) and
//     EEPROM (512 Byte / 8 KByte), the latter reached through the DMA3 protocol the games use
//     (a 2-bit read or a 65/81-bit write on the ROM bus at 0x0D000000);
//   * the GPIO/RTC port some cartridges have (0x080000C4..0x080000C8, the Seiko S-3511A real
//     time clock).
//
// Save files are the usual `.sav` next to the ROM (or in the configured save directory), which
// is what every other GBA tool expects.

#pragma once

#include "gba_types.h"

namespace GBA
{
	class GbaBus;

	enum class SaveType
	{
		None,			// no save memory (the game is read-only)
		Sram32K,		// "SRAM_V"
		Flash64K,		// "FLASH_V" / "FLASH512_V"
		Flash128K,		// "FLASH1M_V"
		Eeprom512B,		// "EEPROM_V" with a 6-bit address
		Eeprom8K,		// "EEPROM_V" with a 14-bit address
	};

	/// <summary>The RTC registers the S-3511A exposes through the GPIO port.</summary>
	struct RtcRegisters
	{
		int year = 0;		// 0..99
		int month = 1;		// 1..12
		int day = 1;		// 1..31
		int weekday = 0;	// 0 = Sunday
		int hour = 0;
		int minute = 0;
		int second = 0;
		int centisecond = 0;	// 1/100 s, or 1/256 s reading
		bool reads256Hz = false;
	};

	class Cart
	{
	public:
		void Reset();

		/// <summary>Install a ROM image. The save type is detected from its signature.</summary>
		bool LoadRom(std::vector<u8> image, std::string& error);

		/// <summary>Load a .gba/.bin/.rom file from disk.</summary>
		bool LoadRomFile(const std::string& path, std::string& error);

		/// <summary>Remove the cartridge (the emulator then boots to the boot ROM alone).</summary>
		void Eject();

		bool IsLoaded() const { return !rom.empty(); }

		const u8* Rom() const { return rom.empty() ? nullptr : rom.data(); }
		size_t RomSize() const { return rom.size(); }

		// -- the ROM window (0x08000000, mirrored to 0x09FFFFFE for a small ROM) -------------

		u8 ReadRom8(u32 offset) const;
		u16 ReadRom16(u32 offset) const;
		u32 ReadRom32(u32 offset) const;

		/// <summary>A write into the ROM window. This is where the Flash commands and the EEPROM
		/// bit stream arrive; a write to a plain ROM returns without effect (as on the hardware,
		/// where the ROM ignores writes).</summary>
		void WriteRom16(GbaBus& bus, u32 offset, u16 value);
		void WriteRom8(GbaBus& bus, u32 offset, u8 value);

		/// <summary>Wait states the cartridge adds to a ROM access (per WAITCNT and the ROM's
		/// size; a 128 Mbit ROM has a longer first-access time).</summary>
		int WaitStates(u32 offset, bool sequential, int waitcnt) const;

		// -- the save memory window (0x0E000000, 64 KByte) ------------------------------------

		u8 ReadSave(u32 offset);
		void WriteSave(u32 offset, u8 value);

		// -- save files ----------------------------------------------------------------------

		SaveType GetSaveType() const { return saveType; }
		void SetSaveType(SaveType type);
		const char* SaveTypeName() const;

		/// <summary>Read the `.sav` next to the ROM (missing is not an error: the memory is
		/// filled with 0xFF, the erased state).</summary>
		bool LoadSaveFile(const std::string& path, std::string* error);

		/// <summary>Write the `.sav` back (called on shutdown and from the UI).</summary>
		bool SaveSaveFile(const std::string& path, std::string* error);

		std::string SaveFilePath() const { return savePath; }
		void SetSaveFilePath(const std::string& path) { savePath = path; }
		bool SaveDirty() const { return saveDirty; }

		// -- GPIO / RTC ----------------------------------------------------------------------

		bool HasRtc() const { return rtcEnabled; }
		void SetRtcEnabled(bool enable) { rtcEnabled = enable; }

		u8 ReadGpio(u32 offset);
		void WriteGpio(u32 offset, u8 value);

		/// <summary>True when the EEPROM is in the middle of a transfer (for tests).</summary>
		bool EepromBusy() const { return eepromBits != 0; }

		// -- header --------------------------------------------------------------------------

		std::string Title() const;
		std::string GameCode() const;
		std::string MakerCode() const;

		/// <summary>Verify the ROM's header checksum (0x080000BD, the "complement check").</summary>
		bool HeaderChecksumOk() const;

	private:
		std::vector<u8> rom;
		std::vector<u8> save;			// SRAM / Flash storage
		std::string savePath;
		bool saveDirty = false;

		SaveType saveType = SaveType::None;
		u32 romMask = 0;				// the read mirroring mask (next power of two - 1)

		// Flash state machine (the SST command set).
		int flashState = 0;				// 0 = read array, 1 = command, 2 = bank select,
										// 3 = erase pending, 4 = byte program pending
		int flashBank = 0;
		u8 flashIdMode = 0;				// 1 = the ID is being read, 2 = CFI
		int flashCmdCount = 0;			// how many unlock writes were seen

		// EEPROM state: the bit stream arrives as 16-bit writes to the ROM area.
		u32 eepromBits = 0;				// bits collected so far
		u64 eepromBuffer = 0;
		bool eepromReadMode = false;
		bool eepromChipSelect = false;
		u16 eepromOutput = 0;			// the value the reads return while the EEPROM drives the bus
		int eepromAddressBits = 6;
		u32 eepromAddress = 0;			// the 64-bit block the transfer in progress addresses

		// GPIO/RTC
		bool rtcEnabled = false;
		u8 gpioData = 0, gpioDirection = 0, gpioControl = 0;
		u8 gpioPrevious = 0;			// the data register before the last write (SCK/CS edges)
		int rtcState = 0;				// the bit position of the RTC command/response;
										// negative = the chip ignored an unknown command
		u32 rtcCommand = 0;				// the command byte, clocked in LSB first
		u8 rtcParameter = 0;			// the parameter byte being clocked in
		u8 rtcResponse[8] = {};			// the parameter bytes latched for a read transfer
		u8 rtcControl = 0x82;			// the control/status register (function 1)
		bool rtcSetByGame = false;		// the game wrote the clock: keep it, stop the host sync
		u32 rtcHostSecond = 0;			// the host clock's second count at the last tick
		bool rtcReadMode = false;
		RtcRegisters rtc;

		void DetectSaveType();
		void DetectRtc();
		void ResetSaveMemory();

		// Helpers the implementation needs on top of the public interface.
		size_t SaveSize() const;
		u8 RomByte(u32 index) const;
		std::string ReadHeaderText(u32 offset, size_t length) const;

		bool GpioEnabled() const;

		bool EepromDriving() const;
		u16 EepromReadWord();

		bool RtcSelected() const;
		void RtcTick();
		void RtcDecodeCommand();
		void RtcReset();
		void RtcShiftIn(u32 bit);
		void RtcWriteParameter(u32 index, u8 value);
		void RtcWriteHour(u8 value);
		int RtcParameterBytes(u8 command) const;
		u8 RtcHourByte() const;

		void FlashWrite(u32 offset, u8 value);
		u8 FlashRead(u32 offset);
		bool FlashReadId() const { return flashIdMode == 1; }

		void EepromWriteBit(u16 value);
		void EepromFinish();

		void RtcClock();
		u8 RtcReadBit();
	};
}
