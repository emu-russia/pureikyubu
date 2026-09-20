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
		bool LoadRom(std::vector<uint8_t> image, std::string& error);

		/// <summary>Load a .gba/.bin/.rom file from disk.</summary>
		bool LoadRomFile(const std::string& path, std::string& error);

		/// <summary>Remove the cartridge (the emulator then boots to the boot ROM alone).</summary>
		void Eject();

		bool IsLoaded() const { return !rom.empty(); }

		const uint8_t* Rom() const { return rom.empty() ? nullptr : rom.data(); }
		size_t RomSize() const { return rom.size(); }

		// -- the ROM window (0x08000000, mirrored to 0x09FFFFFE for a small ROM) -------------

		uint8_t ReadRom8(uint32_t offset) const;
		uint16_t ReadRom16(uint32_t offset) const;
		uint32_t ReadRom32(uint32_t offset) const;

		/// <summary>A write into the ROM window. This is where the Flash commands and the EEPROM
		/// bit stream arrive; a write to a plain ROM returns without effect (as on the hardware,
		/// where the ROM ignores writes).</summary>
		void WriteRom16(GbaBus& bus, uint32_t offset, uint16_t value);
		void WriteRom8(GbaBus& bus, uint32_t offset, uint8_t value);

		/// <summary>Wait states the cartridge adds to a ROM access (per WAITCNT and the ROM's
		/// size; a 128 Mbit ROM has a longer first-access time).</summary>
		int WaitStates(uint32_t offset, bool sequential, int waitcnt) const;

		// -- the save memory window (0x0E000000, 64 KByte) ------------------------------------

		uint8_t ReadSave(uint32_t offset);
		void WriteSave(uint32_t offset, uint8_t value);

		/// <summary>
		/// The same read without a side effect: SRAM is answered from the memory itself and every
		/// other save type answers 0xFF. The Flash bus advances its command state machine on a
		/// read (and the EEPROM has no read window at all), which is a side effect a debugger must
		/// not have - its memory panel walks the window continuously. This is what `GbaBus::Peek16`
		/// calls, so a debugger sees the memory the game wrote and never moves it.
		/// </summary>
		uint8_t PeekSave(uint32_t offset) const;

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

		/// <summary>True while the Flash chip answers the chip-identification command instead of
		/// its data (the debugger prints it).</summary>
		bool FlashReadId() const { return flashIdMode == 1; }

		// -- GPIO / RTC ----------------------------------------------------------------------

		bool HasRtc() const { return rtcEnabled; }
		void SetRtcEnabled(bool enable) { rtcEnabled = enable; }

		uint8_t ReadGpio(uint32_t offset);
		void WriteGpio(uint32_t offset, uint8_t value);

		/// <summary>True when the EEPROM is in the middle of a transfer (for tests).</summary>
		bool EepromBusy() const { return eepromBits != 0; }

		// -- header --------------------------------------------------------------------------

		std::string Title() const;
		std::string GameCode() const;
		std::string MakerCode() const;

		/// <summary>Verify the ROM's header checksum (0x080000BD, the "complement check").</summary>
		bool HeaderChecksumOk() const;

	private:
		std::vector<uint8_t> rom;
		std::vector<uint8_t> save;			// SRAM / Flash storage
		std::string savePath;
		bool saveDirty = false;

		SaveType saveType = SaveType::None;
		uint32_t romMask = 0;				// the read mirroring mask (next power of two - 1)

		// Flash state machine (the SST command set).
		int flashState = 0;				// 0 = read array, 1 = command, 2 = bank select,
										// 3 = erase pending, 4 = byte program pending
		int flashBank = 0;
		uint8_t flashIdMode = 0;				// 1 = the ID is being read, 2 = CFI
		int flashCmdCount = 0;			// how many unlock writes were seen

		// EEPROM state: the bit stream arrives as 16-bit writes to the ROM area.
		uint32_t eepromBits = 0;				// bits collected so far
		uint64_t eepromBuffer = 0;
		bool eepromReadMode = false;
		bool eepromChipSelect = false;
		uint16_t eepromOutput = 0;			// the value the reads return while the EEPROM drives the bus
		int eepromAddressBits = 6;
		uint32_t eepromAddress = 0;			// the 64-bit block the transfer in progress addresses

		// GPIO/RTC
		bool rtcEnabled = false;
		uint8_t gpioData = 0, gpioDirection = 0, gpioControl = 0;
		uint8_t gpioPrevious = 0;			// the data register before the last write (SCK/CS edges)
		int rtcState = 0;				// the bit position of the RTC command/response;
										// negative = the chip ignored an unknown command
		uint32_t rtcCommand = 0;				// the command byte, clocked in LSB first
		uint8_t rtcParameter = 0;		// the parameter byte being clocked in
		uint8_t rtcResponse[8] = {};		// the parameter bytes latched for a read transfer
		uint8_t rtcControl = 0x82;		// the control/status register (function 1)
		bool rtcSetByGame = false;		// the game wrote the clock: keep it, stop the host sync
		uint32_t rtcHostSecond = 0;		// the host clock's second count at the last tick
		bool rtcReadMode = false;
		RtcRegisters rtc;

		void DetectSaveType();
		void DetectRtc();
		void ResetSaveMemory();

		// Helpers the implementation needs on top of the public interface.
		size_t SaveSize() const;
		uint8_t RomByte(uint32_t index) const;
		std::string ReadHeaderText(uint32_t offset, size_t length) const;

		bool GpioEnabled() const;

		bool EepromDriving() const;
		uint16_t EepromReadWord();

		/// <summary>True for an idle access in the EEPROM's own window: the chip is selected but
		/// not shifting a block out, so it drives its ready line (bit0) on the bus.</summary>
		bool EepromIdle(uint32_t address) const;

		/// <summary>
		/// True when this address is in the EEPROM's own window rather than in the ROM image.
		/// The chip is decoded from the top of the cartridge address space, so an access inside
		/// the ROM image must keep returning the ROM even while the chip is driving the bus -
		/// that is what lets a game fetch its EEPROM routine's own instructions from the
		/// cartridge (The Legend of Zelda: The Minish Cap does exactly that).
		/// </summary>
		bool InEepromWindow(uint32_t address) const;

		bool RtcSelected() const;
		void RtcTick();
		void RtcDecodeCommand();
		void RtcReset();
		void RtcShiftIn(uint32_t bit);
		void RtcWriteParameter(uint32_t index, uint8_t value);
		void RtcWriteHour(uint8_t value);
		int RtcParameterBytes(uint8_t command) const;
		uint8_t RtcHourByte() const;

		void FlashWrite(uint32_t offset, uint8_t value);
		uint8_t FlashRead(uint32_t offset) const;

		void EepromWriteBit(uint16_t value);
		void EepromFinish();

		void RtcClock();
		uint8_t RtcReadBit();
	};
}
