// Cartridge tests: the ROM image and its header, the save type detection, the three kinds of
// save memory (SRAM, the SST Flash command set, the serial EEPROM) and the GPIO/RTC port.
//
// Every expected value below is derived from GBATEK, not from the implementation:
//
//   * "GBA Cartridge Header"      - the field offsets and the complement check formula;
//   * "GBA Cartridge ROM"         - the 16bit ROM bus (the aligned halfword rule);
//   * "GBA Cart Backup IDs"       - the linker signature strings and what they select;
//   * "GBA Cart Backup Flash ROM" - the command set and the device table;
//   * "GBA Cart Backup EEPROM"    - the bit streams the DMA3 transfers carry;
//   * "GBA Cart Real-Time Clock (RTC)" / "DS Real-Time Clock (RTC)" - the S-3511A protocol;
//   * "GBA Cart I/O Port (GPIO)"  - the three registers at 080000C4h..080000C8h;
//   * "GBA System Control"        - WAITCNT's first/second access times.

#include "gba_test.h"

#include "gba_cart.h"
#include "gba_bus.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace GBA;

namespace
{
	// -------------------------------------------------------------------------------------
	// Small helpers
	// -------------------------------------------------------------------------------------

	// The cartridge's ROM writes only need the bus for the DMA hook the bus itself owns: the
	// EEPROM/flash paths complete the transfer inside WriteRom16/WriteSave, so nothing here
	// dereferences it. A GbaBus cannot be constructed in this test (its own translation unit
	// brings up the whole machine), hence the scratch object.
	GbaBus& ScratchBus()
	{
		static unsigned char storage[sizeof(GbaBus)] alignas(GbaBus);
		return *reinterpret_cast<GbaBus*>(storage);
	}

	// GBATEK "0BDh - Complement check": the byte that makes
	// 19h + sum(0A0h..0BCh) + the stored byte wrap to zero.
	uint8_t HeaderChecksum(const std::vector<uint8_t>& image)
	{
		uint32_t sum = 0x19;
		for (uint32_t i = 0xA0; i <= 0xBC; i++)
			sum += image[i];
		return (uint8_t)(0 - sum);
	}

	// A synthetic cartridge image: the header GBATEK describes (title at 0A0h, game code at
	// 0ACh, maker code at 0B0h, the fixed 96h at 0B2h) and a valid complement check. The rest
	// of the image is filled with 0FFh, the erased state, so every byte the tests look at is
	// set explicitly.
	std::vector<uint8_t> MakeRom(size_t size = 32 * 1024)
	{
		std::vector<uint8_t> image(size, 0xFF);

		const char* title = "CART TEST   ";		// 12 characters, space padded
		for (int i = 0; i < 12; i++) image[0xA0 + i] = (uint8_t)title[i];
		const char* code = "ATSE";
		for (int i = 0; i < 4; i++) image[0xAC + i] = (uint8_t)code[i];
		image[0xB0] = '0';
		image[0xB1] = '1';
		image[0xB2] = 0x96;						// the fixed header value
		for (uint32_t i = 0xB5; i <= 0xBC; i++) image[i] = 0x00;
		image[0xBD] = HeaderChecksum(image);
		return image;
	}

	// A save type ID string at a word aligned address (GBATEK "GBA Cart Backup IDs").
	void PutSignature(std::vector<uint8_t>& image, size_t offset, const char* text)
	{
		memcpy(image.data() + offset, text, strlen(text));
	}

	void PutWord(std::vector<uint8_t>& image, size_t offset, uint32_t value)
	{
		image[offset + 0] = (uint8_t)(value & 0xFF);
		image[offset + 1] = (uint8_t)((value >> 8) & 0xFF);
		image[offset + 2] = (uint8_t)((value >> 16) & 0xFF);
		image[offset + 3] = (uint8_t)((value >> 24) & 0xFF);
	}

	// The GPIO addresses a cartridge that uses the port loads as literals: that is how the
	// emulator detects the add-on (GBATEK "GBA Cart I/O Port (GPIO)" has no header flag).
	std::vector<uint8_t> WithGpioPort(const std::vector<uint8_t>& rom)
	{
		std::vector<uint8_t> image = rom;
		PutWord(image, 0x200, 0x080000C4);		// the data/direction registers
		PutWord(image, 0x204, 0x080000C8);		// the control register
		return image;
	}

	void WriteFile(const std::string& path, const std::vector<uint8_t>& data)
	{
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file.write((const char*)data.data(), (std::streamsize)data.size());
	}

	// -------------------------------------------------------------------------------------
	// Flash helpers (GBATEK "GBA Cart Backup Flash ROM")
	// -------------------------------------------------------------------------------------

	// The unlock sequence: AAh @ 05555h, 55h @ 02AAAh.
	void FlashUnlock(Cart& cart)
	{
		cart.WriteSave(0x5555, 0xAA);
		cart.WriteSave(0x2AAA, 0x55);
	}

	void FlashCommand(Cart& cart, uint8_t command)
	{
		FlashUnlock(cart);
		cart.WriteSave(0x5555, command);
	}

	// -------------------------------------------------------------------------------------
	// EEPROM helpers (GBATEK "GBA Cart Backup EEPROM")
	//
	// The chip sees one bit per 16bit write, so a transfer is a list of bits here and each
	// bit becomes one halfword write into the ROM window.
	// -------------------------------------------------------------------------------------

	void EepromSendBits(Cart& cart, const std::vector<uint8_t>& bits)
	{
		for (size_t i = 0; i < bits.size(); i++)
			cart.WriteRom16(ScratchBus(), 0, (uint16_t)(bits[i] & 1));
	}

	// The write request: "10", the address (MSB first), 64 data bits (MSB first), a "0".
	std::vector<uint8_t> EepromWriteStream(uint32_t block, int addressBits, const uint8_t* data)
	{
		std::vector<uint8_t> bits;
		bits.push_back(1);						// the leading start bit of both requests
		bits.push_back(0);						// 0 = write
		for (int i = addressBits - 1; i >= 0; i--)
			bits.push_back((uint8_t)((block >> i) & 1));
		for (int i = 0; i < 8; i++)
			for (int b = 7; b >= 0; b--)
				bits.push_back((uint8_t)((data[i] >> b) & 1));
		bits.push_back(0);						// the trailing dummy bit
		return bits;
	}

	// The read request: "11", the address (MSB first), a "0".
	std::vector<uint8_t> EepromReadRequest(uint32_t block, int addressBits)
	{
		std::vector<uint8_t> bits;
		bits.push_back(1);
		bits.push_back(1);						// 1 = read
		for (int i = addressBits - 1; i >= 0; i--)
			bits.push_back((uint8_t)((block >> i) & 1));
		bits.push_back(0);
		return bits;
	}

	// Send a request and check that the busy flag covers exactly the whole stream: the write
	// request ends with its last bit, the read request leaves the chip driving the bus.
	void EepromSendRequest(Cart& cart, const std::vector<uint8_t>& bits, bool completes)
	{
		GBA_CHECK_MSG(!cart.EepromBusy(), "the EEPROM must be idle before a transfer");
		for (size_t i = 0; i < bits.size(); i++)
		{
			cart.WriteRom16(ScratchBus(), 0, (uint16_t)(bits[i] & 1));
			if (completes && i + 1 == bits.size())
				GBA_CHECK_MSG(!cart.EepromBusy(), "the last write bit completes the transfer");
			else
				GBA_CHECK_MSG(cart.EepromBusy(), "the transfer stays busy until its last bit");
		}
	}

	// Read the 64 data bits the chip shifts out after a read request: 4 ignored bits, then the
	// 64 bits of the block, most significant bit first.
	void EepromReadData(Cart& cart, uint8_t* out)
	{
		for (int i = 0; i < 4; i++)
		{
			GBA_CHECK_HEX16(cart.ReadRom16(0), 0x0000);
			GBA_CHECK(cart.EepromBusy());
		}
		uint8_t data[8] = {};
		for (int i = 0; i < 64; i++)
		{
			uint8_t bit = (uint8_t)(cart.ReadRom16(0) & 1);
			data[i / 8] = (uint8_t)((data[i / 8] << 1) | bit);
		}
		GBA_CHECK_MSG(!cart.EepromBusy(), "the 68 bit read stream ends the transfer");
		memcpy(out, data, 8);
	}

	// -------------------------------------------------------------------------------------
	// GPIO helpers (GBATEK "GBA Cart I/O Port (GPIO)")
	//
	// Bit0 = SCK, bit1 = SIO, bit2 = chip select for the RTC; the registers sit at 080000C4h
	// (data), 080000C6h (direction) and 080000C8h (control), the offsets used here.
	// -------------------------------------------------------------------------------------

	const uint32_t GpioData = 0x04;
	const uint32_t GpioDirection = 0x06;
	const uint32_t GpioControl = 0x08;

	const uint8_t GpioSck = 0x01;
	const uint8_t GpioSio = 0x02;
	const uint8_t GpioCs = 0x04;

	void GpioWrite(Cart& cart, uint8_t value)
	{
		cart.WriteGpio(GpioData, value);
	}

	// One command/parameter byte, LSB first, on the rising clock edge (GBATEK "DS Real-Time
	// Clock (RTC)" bit transfer; the GBA's S-3511A uses the same protocol).
	void RtcSendByte(Cart& cart, uint8_t value)
	{
		cart.WriteGpio(GpioDirection, 0x07);			// SCK, SIO and CS are outputs
		for (int i = 0; i < 8; i++)
		{
			uint8_t data = (uint8_t)(((value >> i) & 1) ? GpioSio : 0);
			GpioWrite(cart, (uint8_t)(GpioCs | data));				// SCK low: the bit is set up
			GpioWrite(cart, (uint8_t)(GpioCs | data | GpioSck));		// rising edge: the chip takes it
		}
	}

	uint8_t RtcReceiveByte(Cart& cart)
	{
		cart.WriteGpio(GpioDirection, 0x05);			// SIO is an input while reading
		uint8_t value = 0;
		for (int i = 0; i < 8; i++)
		{
			GpioWrite(cart, GpioCs);					// falling edge: the chip drives the bit
			GpioWrite(cart, (uint8_t)(GpioCs | GpioSck));	// rising edge: the game samples it
			if (cart.ReadGpio(GpioData) & GpioSio)
				value = (uint8_t)(value | (1 << i));
		}
		return value;
	}

	// The chip select: it has to rise to select the chip and drop to end the transfer.
	void RtcSelect(Cart& cart)
	{
		GpioWrite(cart, 0);
		GpioWrite(cart, GpioCs);
	}

	void RtcDeselect(Cart& cart)
	{
		GpioWrite(cart, 0);
	}
}

// ---------------------------------------------------------------------------------------
// The ROM image and its header
// ---------------------------------------------------------------------------------------

GBA_TEST(Cart, HeaderFields)
{
	std::string error;
	std::vector<uint8_t> image = MakeRom();

	Cart cart;
	GBA_CHECK_MSG(cart.LoadRom(image, error), error);
	GBA_CHECK(cart.IsLoaded());
	GBA_CHECK_EQ(cart.RomSize(), (size_t)0x8000);
	GBA_CHECK_MSG(cart.Title() == std::string("CART TEST"), cart.Title());
	GBA_CHECK_MSG(cart.GameCode() == std::string("ATSE"), cart.GameCode());
	GBA_CHECK_MSG(cart.MakerCode() == std::string("01"), cart.MakerCode());
	GBA_CHECK_MSG(cart.HeaderChecksumOk(), "the synthetic header carries a valid complement check");

	// A wrong complement check is reported but the ROM is still usable (a homebrew image with
	// a sloppy header must not be rejected).
	image[0xBD] = (uint8_t)(image[0xBD] ^ 0xFF);
	Cart broken;
	GBA_CHECK_MSG(broken.LoadRom(image, error), error);
	GBA_CHECK(!broken.HeaderChecksumOk());
	GBA_CHECK_MSG(broken.Title() == std::string("CART TEST"), broken.Title());

	// An image too short for a header is loaded as well (multiboot/test images), it simply has
	// no header fields.
	std::vector<uint8_t> tiny(64, 0x00);
	Cart shortCart;
	GBA_CHECK_MSG(shortCart.LoadRom(tiny, error), error);
	GBA_CHECK(!shortCart.HeaderChecksumOk());
	GBA_CHECK(shortCart.Title().empty());
	GBA_CHECK(shortCart.GameCode().empty());

	// Only an empty image and one bigger than the 32 MByte Game Pak are refused.
	std::vector<uint8_t> empty;
	Cart refused;
	GBA_CHECK(!refused.LoadRom(empty, error));
	GBA_CHECK_MSG(!error.empty(), "an empty image must come with a readable error");
	GBA_CHECK(!refused.IsLoaded());

	std::vector<uint8_t> huge(32 * 1024 * 1024 + 1, 0x00);
	GBA_CHECK(!refused.LoadRom(huge, error));
	GBA_CHECK(!error.empty());
}

GBA_TEST(Cart, RomReadRules)
{
	std::string error;
	std::vector<uint8_t> image = MakeRom(0x8000);
	for (int i = 0; i < 8; i++)
		image[0x1000 + i] = (uint8_t)(0x40 + i);			// 40h,41h,42h,... at 01000h

	Cart cart;
	GBA_CHECK_MSG(cart.LoadRom(image, error), error);

	// 16bit reads: the low byte first, little endian (GBATEK "Data Format").
	GBA_CHECK_HEX16(cart.ReadRom16(0x1000), 0x4140);
	GBA_CHECK_HEX16(cart.ReadRom8(0x1000), 0x40);

	// A read at an odd address returns the aligned halfword: the cartridge bus is 16 bits wide
	// and does not rotate the byte lanes like the 32bit regions do (GBATEK "GBA Cartridge ROM"
	// describes the address latch, which is what makes this an aligned access).
	GBA_CHECK_HEX16(cart.ReadRom16(0x1001), 0x4140);
	GBA_CHECK_HEX16(cart.ReadRom16(0x1003), 0x4342);	// aligned to 1002h: 42h, 43h

	// A 32bit read is two halfwords, the low one first (GBATEK "GBA System Control": "a 32bit
	// access is split into TWO 16bit accesses").
	GBA_CHECK_HEX32(cart.ReadRom32(0x1000), 0x43424140u);
	GBA_CHECK_HEX32(cart.ReadRom32(0x1001), 0x43424140u);	// aligned like the 16bit read

	// The ROM window mirrors through the image size: offset 08000h is offset 00000h again.
	GBA_CHECK_HEX16(cart.ReadRom8(0x8000), cart.ReadRom8(0x0000));
	GBA_CHECK_HEX16(cart.ReadRom16(0x9000), cart.ReadRom16(0x1000));
	GBA_CHECK_HEX32(cart.ReadRom32(0x9000), cart.ReadRom32(0x1000));

	// An image that is not a power of two (the public test ROMs are a few KByte) is masked
	// through the next power of two; its partial last bank reads as open bus.
	std::vector<uint8_t> odd = MakeRom(0x2200);
	odd[0x400] = 0x5A;
	Cart oddCart;
	GBA_CHECK_MSG(oddCart.LoadRom(odd, error), error);
	GBA_CHECK_HEX16(oddCart.ReadRom8(0x400), 0x5A);
	GBA_CHECK_HEX16(oddCart.ReadRom8(0x4400), 0x5A);	// 4400h mirrors 0400h (mask 3FFFh)
	GBA_CHECK_HEX16(oddCart.ReadRom8(0x2200), 0xFF);	// past the image: open bus
	GBA_CHECK_HEX16(oddCart.ReadRom8(0x3FFF), 0xFF);
}

GBA_TEST(Cart, NoCartridge)
{
	Cart cart;
	GBA_CHECK(!cart.IsLoaded());

	// Every write path returns harmlessly ...
	cart.WriteRom16(ScratchBus(), 0x000000, 0x1234);
	cart.WriteRom8(ScratchBus(), 0x000000, 0x12);
	cart.WriteSave(0x0000, 0x34);
	cart.WriteGpio(GpioData, 0xFF);
	cart.WriteGpio(GpioControl, 0x01);

	// ... and every read floats high, like the bus' open bus value.
	GBA_CHECK_HEX16(cart.ReadRom8(0), 0xFF);
	GBA_CHECK_HEX16(cart.ReadRom16(0), 0xFFFF);
	GBA_CHECK_HEX32(cart.ReadRom32(0), 0xFFFFFFFFu);
	GBA_CHECK_HEX16(cart.ReadSave(0), 0xFF);
	GBA_CHECK_HEX16(cart.ReadGpio(GpioData), 0xFF);

	GBA_CHECK(cart.GetSaveType() == SaveType::None);
	GBA_CHECK_MSG(cart.SaveTypeName() == std::string("None"), cart.SaveTypeName());

	// Without save memory there is nothing to load or write, so both calls succeed and do not
	// even need a path (the path checks are exercised by the save memory tests below).
	std::string error;
	GBA_CHECK(cart.LoadSaveFile("/tmp/pureikyubu_gba_nosave.sav", &error));
	GBA_CHECK(cart.SaveSaveFile("/tmp/pureikyubu_gba_nosave.sav", &error));
	GBA_CHECK(cart.LoadSaveFile("", &error));
	GBA_CHECK(cart.SaveSaveFile("", &error));

	// Eject removes the cartridge and leaves the machine on the boot ROM alone.
	std::vector<uint8_t> image = MakeRom();
	GBA_CHECK_MSG(cart.LoadRom(image, error), error);
	GBA_CHECK(cart.IsLoaded());
	cart.Eject();
	GBA_CHECK(!cart.IsLoaded());
	GBA_CHECK_HEX16(cart.ReadRom16(0), 0xFFFF);
	GBA_CHECK_HEX16(cart.ReadSave(0), 0xFF);
}

GBA_TEST(Cart, LoadRomFile)
{
	const std::string path = "/tmp/pureikyubu_gba_cart_test.gba";
	std::remove(path.c_str());

	// The public GBA test ROMs are a few KByte: an image that is not a power of two and much
	// smaller than a commercial cartridge must load.
	WriteFile(path, MakeRom(8824));

	Cart cart;
	std::string error;
	GBA_CHECK_MSG(cart.LoadRomFile(path, error), error);
	GBA_CHECK_EQ(cart.RomSize(), (size_t)8824);
	GBA_CHECK_MSG(cart.Title() == std::string("CART TEST"), cart.Title());

	// A missing file reports a readable error instead of crashing.
	Cart missing;
	GBA_CHECK(!missing.LoadRomFile("/tmp/pureikyubu_gba_cart_absent.gba", error));
	GBA_CHECK_MSG(!error.empty(), "a missing ROM file must be reported");
	GBA_CHECK(!missing.IsLoaded());

	// An empty file is refused as well.
	WriteFile(path, std::vector<uint8_t>());
	Cart empty;
	GBA_CHECK(!empty.LoadRomFile(path, error));
	GBA_CHECK_MSG(!error.empty(), "an empty ROM file must be reported");

	std::remove(path.c_str());
}

// ---------------------------------------------------------------------------------------
// Save type detection
// ---------------------------------------------------------------------------------------

GBA_TEST(Cart, SaveTypeDetection)
{
	std::string error;
	const size_t offset = 0x120;		// word aligned, right after the header

	struct Case
	{
		const char* signature;
		SaveType expected;
	};

	// GBATEK "GBA Cart Backup IDs": the ID string selects the save memory. The EEPROM size is
	// the one thing the string does not tell, see below.
	const Case cases[] =
	{
		{ "EEPROM_V",	SaveType::Eeprom512B },	// a 32 KByte image -> the 512 Byte chip
		{ "SRAM_V",		SaveType::Sram32K },
		{ "FLASH_V",	SaveType::Flash64K },
		{ "FLASH512_V",	SaveType::Flash64K },
		{ "FLASH1M_V",	SaveType::Flash128K },
	};

	for (const Case& test : cases)
	{
		std::vector<uint8_t> image = MakeRom();
		PutSignature(image, offset, test.signature);

		Cart cart;
		GBA_CHECK_MSG(cart.LoadRom(image, error), error);
		GBA_CHECK_MSG(cart.GetSaveType() == test.expected,
			std::string(test.signature) + " should select " + std::to_string((int)test.expected));
	}

	// Without a signature there is no save memory (GBATEK: Nintendo's tools insert the string,
	// other tools have to do it by hand).
	Cart none;
	GBA_CHECK_MSG(none.LoadRom(MakeRom(), error), error);
	GBA_CHECK(none.GetSaveType() == SaveType::None);

	// The scan also covers the end of the image (GBATEK notes the ID strings sit "somewhere
	// right after the ROM header", the libraries of bigger games end up in the last bank).
	std::vector<uint8_t> big = MakeRom(256 * 1024);
	PutSignature(big, 250 * 1024, "FLASH1M_V");
	Cart bigCart;
	GBA_CHECK_MSG(bigCart.LoadRom(big, error), error);
	GBA_CHECK(bigCart.GetSaveType() == SaveType::Flash128K);

	// The EEPROM width heuristic (GBATEK: "there seems to be no autodetection mechanism, so
	// that a hardcoded bus width must be used"): a 128 Mbit class image (16 MByte and up) uses
	// the 8 KByte chip, smaller images the 512 Byte one.
	std::vector<uint8_t> large = MakeRom(16 * 1024 * 1024);
	PutSignature(large, offset, "EEPROM_V");
	Cart largeCart;
	GBA_CHECK_MSG(largeCart.LoadRom(large, error), error);
	GBA_CHECK(largeCart.GetSaveType() == SaveType::Eeprom8K);

	// The UI can override the detection.
	Cart forced;
	GBA_CHECK_MSG(forced.LoadRom(MakeRom(), error), error);
	forced.SetSaveType(SaveType::Flash128K);
	GBA_CHECK(forced.GetSaveType() == SaveType::Flash128K);
	GBA_CHECK_MSG(forced.SaveTypeName() == std::string("Flash 128K"), forced.SaveTypeName());
}

// ---------------------------------------------------------------------------------------
// SRAM
// ---------------------------------------------------------------------------------------

GBA_TEST(Cart, SramReadWrite)
{
	std::string error;
	std::vector<uint8_t> image = MakeRom();
	PutSignature(image, 0x120, "SRAM_V");

	Cart cart;
	GBA_CHECK_MSG(cart.LoadRom(image, error), error);
	GBA_CHECK(cart.GetSaveType() == SaveType::Sram32K);
	GBA_CHECK_MSG(cart.SaveTypeName() == std::string("SRAM 32K"), cart.SaveTypeName());

	// A freshly loaded cartridge without a .sav file is in the erased state.
	GBA_CHECK_HEX16(cart.ReadSave(0x0000), 0xFF);
	GBA_CHECK(!cart.SaveDirty());

	cart.WriteSave(0x0000, 0x5A);
	cart.WriteSave(0x7FFF, 0xA5);
	GBA_CHECK_HEX16(cart.ReadSave(0x0000), 0x5A);
	GBA_CHECK_HEX16(cart.ReadSave(0x7FFF), 0xA5);
	GBA_CHECK_MSG(cart.SaveDirty(), "a write must mark the save dirty");

	// The 64 KByte window mirrors the 32 KByte chip (GBATEK "GBA Memory Map").
	GBA_CHECK_HEX16(cart.ReadSave(0x8000), 0x5A);
	GBA_CHECK_HEX16(cart.ReadSave(0xFFFF), 0xA5);

	// Byte granularity: the neighbours of a written byte are untouched.
	GBA_CHECK_HEX16(cart.ReadSave(0x0001), 0xFF);
}

// ---------------------------------------------------------------------------------------
// Flash
// ---------------------------------------------------------------------------------------

GBA_TEST(Cart, FlashCommands)
{
	std::string error;
	std::vector<uint8_t> image = MakeRom();
	PutSignature(image, 0x120, "FLASH512_V");

	Cart cart;
	GBA_CHECK_MSG(cart.LoadRom(image, error), error);
	GBA_CHECK(cart.GetSaveType() == SaveType::Flash64K);

	// The ID mode: AAh@05555h, 55h@02AAAh, 90h@05555h, then the manufacturer is read at
	// 0E000000h and the device at 0E000001h (GBATEK "Chip Identification"). The device table
	// lists "D4BFh SST 64K", the device type in the MSB, the manufacturer in the LSB.
	FlashCommand(cart, 0x90);
	GBA_CHECK_HEX16(cart.ReadSave(0x0000), 0xBF);		// SST
	GBA_CHECK_HEX16(cart.ReadSave(0x0001), 0xD4);		// SST 39VF512, 512 Kbit

	// F0h terminates the ID mode and the array is readable again (it is erased).
	cart.WriteSave(0x0000, 0xF0);
	GBA_CHECK_HEX16(cart.ReadSave(0x0000), 0xFF);
	GBA_CHECK_HEX16(cart.ReadSave(0x0001), 0xFF);

	// 98h is the CFI query (JEDEC JESD68): the three magic bytes "QRY" answer at 1000Ch..,
	// which is how a game probes for a CFI capable chip. F0h leaves the mode again.
	FlashCommand(cart, 0x98);
	GBA_CHECK_HEX16(cart.ReadSave(0x0010), 'Q');
	GBA_CHECK_HEX16(cart.ReadSave(0x0011), 'R');
	GBA_CHECK_HEX16(cart.ReadSave(0x0012), 'Y');
	cart.WriteSave(0x0000, 0xF0);
	GBA_CHECK_HEX16(cart.ReadSave(0x0010), 0xFF);

	// Program a byte outside the sector we are about to erase (a plain write into the array
	// does nothing, Flash cells need the A0h command).
	FlashUnlock(cart);
	cart.WriteSave(0x5555, 0xA0);
	cart.WriteSave(0x0000, 0x11);
	GBA_CHECK_HEX16(cart.ReadSave(0x0000), 0x11);

	// Erase the 4 KByte sector at 04000h: 80h, then the second unlock sequence and 30h at the
	// sector address.
	FlashUnlock(cart);
	cart.WriteSave(0x5555, 0x80);
	FlashUnlock(cart);
	cart.WriteSave(0x4000, 0x30);
	GBA_CHECK_HEX16(cart.ReadSave(0x4000), 0xFF);
	GBA_CHECK_HEX16(cart.ReadSave(0x4FFF), 0xFF);
	GBA_CHECK_HEX16(cart.ReadSave(0x0000), 0x11);		// ... survives the sector erase
	GBA_CHECK_HEX16(cart.ReadSave(0x3FFF), 0xFF);

	// Program 5Ah over the erased FFh: A0h, then the data byte.
	FlashUnlock(cart);
	cart.WriteSave(0x5555, 0xA0);
	cart.WriteSave(0x4004, 0x5A);
	GBA_CHECK_HEX16(cart.ReadSave(0x4004), 0x5A);
	GBA_CHECK_HEX16(cart.ReadSave(0x4005), 0xFF);

	// Programming only pulls bits low: the second write reads back as the AND of the old and
	// the new value until the sector is erased (that is the flash cells' 1 -> 0 rule).
	FlashUnlock(cart);
	cart.WriteSave(0x5555, 0xA0);
	cart.WriteSave(0x4004, 0xF0);
	GBA_CHECK_HEX16(cart.ReadSave(0x4004), 0x50);

	// An unknown command must leave the array readable and unchanged.
	FlashCommand(cart, 0x77);
	GBA_CHECK_HEX16(cart.ReadSave(0x4004), 0x50);

	// Chip erase: 80h, the second unlock sequence and 10h at 05555h.
	FlashUnlock(cart);
	cart.WriteSave(0x5555, 0x80);
	FlashUnlock(cart);
	cart.WriteSave(0x5555, 0x10);
	GBA_CHECK_HEX16(cart.ReadSave(0x4004), 0xFF);
	GBA_CHECK_HEX16(cart.ReadSave(0x0000), 0xFF);
	GBA_CHECK_HEX16(cart.ReadSave(0x7FFF), 0xFF);
}

GBA_TEST(Cart, Flash128BankSelect)
{
	std::string error;
	std::vector<uint8_t> image = MakeRom();
	PutSignature(image, 0x120, "FLASH1M_V");

	Cart cart;
	GBA_CHECK_MSG(cart.LoadRom(image, error), error);
	GBA_CHECK(cart.GetSaveType() == SaveType::Flash128K);

	// Erase and program a byte in the default bank 0.
	FlashUnlock(cart);
	cart.WriteSave(0x5555, 0x80);
	FlashUnlock(cart);
	cart.WriteSave(0x0000, 0x30);
	FlashUnlock(cart);
	cart.WriteSave(0x5555, 0xA0);
	cart.WriteSave(0x0000, 0x11);
	GBA_CHECK_HEX16(cart.ReadSave(0x0000), 0x11);

	// Bank switching: AAh, 55h, B0h at 05555h, then the bank number written anywhere
	// (GBATEK "Bank Switching": [E000000h]=bnk).
	FlashUnlock(cart);
	cart.WriteSave(0x5555, 0xB0);
	cart.WriteSave(0x0000, 0x01);
	GBA_CHECK_HEX16(cart.ReadSave(0x0000), 0xFF);		// bank 1 is still erased

	FlashUnlock(cart);
	cart.WriteSave(0x5555, 0xA0);
	cart.WriteSave(0x0000, 0x22);
	GBA_CHECK_HEX16(cart.ReadSave(0x0000), 0x22);

	// Back in bank 0 the first byte is still there ...
	FlashUnlock(cart);
	cart.WriteSave(0x5555, 0xB0);
	cart.WriteSave(0x0000, 0x00);
	GBA_CHECK_HEX16(cart.ReadSave(0x0000), 0x11);

	// ... and bank 1 kept its own copy of the same window address.
	FlashUnlock(cart);
	cart.WriteSave(0x5555, 0xB0);
	cart.WriteSave(0x0000, 0x01);
	GBA_CHECK_HEX16(cart.ReadSave(0x0000), 0x22);
}

// ---------------------------------------------------------------------------------------
// EEPROM
// ---------------------------------------------------------------------------------------

GBA_TEST(Cart, Eeprom512WriteRead)
{
	std::string error;
	std::vector<uint8_t> image = MakeRom();
	PutSignature(image, 0x120, "EEPROM_V");

	Cart cart;
	GBA_CHECK_MSG(cart.LoadRom(image, error), error);
	GBA_CHECK(cart.GetSaveType() == SaveType::Eeprom512B);

	const uint8_t written[8] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0 };
	const uint32_t block = 0x2A;		// 64bit blocks 00h..3Fh for the 512 Byte chip

	// A write request is "10" + 6 address bits + 64 data bits + the dummy bit = 73 bits.
	std::vector<uint8_t> write = EepromWriteStream(block, 6, written);
	GBA_CHECK_EQ(write.size(), (size_t)73);
	EepromSendRequest(cart, write, true);

	// A read request is "11" + 6 address bits + the dummy bit = 9 bits, then the chip drives
	// 4 ignored bits and the 64 data bits (68 accesses in total).
	std::vector<uint8_t> request = EepromReadRequest(block, 6);
	GBA_CHECK_EQ(request.size(), (size_t)9);
	EepromSendRequest(cart, request, false);

	uint8_t readBack[8] = {};
	EepromReadData(cart, readBack);
	for (int i = 0; i < 8; i++)
		GBA_CHECK_HEX16(readBack[i], written[i]);

	// A different block was not touched.
	EepromSendRequest(cart, EepromReadRequest(block + 1, 6), false);
	EepromReadData(cart, readBack);
	for (int i = 0; i < 8; i++)
		GBA_CHECK_HEX16(readBack[i], 0xFF);
}

GBA_TEST(Cart, Eeprom8KAddressWidth)
{
	std::string error;
	std::vector<uint8_t> image = MakeRom();
	PutSignature(image, 0x120, "EEPROM_V");

	Cart cart;
	GBA_CHECK_MSG(cart.LoadRom(image, error), error);

	// The heuristic picks the 512 Byte chip for a small image; the UI overrides it.
	cart.SetSaveType(SaveType::Eeprom8K);
	GBA_CHECK(cart.GetSaveType() == SaveType::Eeprom8K);
	GBA_CHECK_MSG(cart.SaveTypeName() == std::string("EEPROM 8K"), cart.SaveTypeName());

	const uint8_t written[8] = { 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x07, 0x18 };
	const uint32_t block = 0x123;

	// A 14 bit address field makes the write request 2 + 14 + 64 + 1 = 81 bits long.
	std::vector<uint8_t> write = EepromWriteStream(block, 14, written);
	GBA_CHECK_EQ(write.size(), (size_t)81);
	EepromSendRequest(cart, write, true);

	// The chip's address decoder uses the lower 10 bits only (GBATEK: "an address range of
	// 0-3FFh, 14bit bus width (only the lower 10 address bits are used, upper 4 bits should be
	// zero)"), so block 523h is block 123h again - the same block level wrap the 512 Byte chip
	// does with its 6 bit field.
	std::vector<uint8_t> request = EepromReadRequest(block + 0x400, 14);
	GBA_CHECK_EQ(request.size(), (size_t)17);
	EepromSendRequest(cart, request, false);

	uint8_t readBack[8] = {};
	EepromReadData(cart, readBack);
	for (int i = 0; i < 8; i++)
		GBA_CHECK_HEX16(readBack[i], written[i]);
}

// ---------------------------------------------------------------------------------------
// Save files
// ---------------------------------------------------------------------------------------

GBA_TEST(Cart, SaveFileRoundTrip)
{
	const std::string path = "/tmp/pureikyubu_gba_cart_test.sav";
	std::remove(path.c_str());

	std::string error;
	std::vector<uint8_t> image = MakeRom();
	PutSignature(image, 0x120, "FLASH512_V");

	{
		Cart cart;
		GBA_CHECK_MSG(cart.LoadRom(image, error), error);

		// A missing file is not an error: the cartridge simply starts erased.
		GBA_CHECK_MSG(cart.LoadSaveFile(path, &error), error);
		GBA_CHECK_MSG(error.empty(), error);
		GBA_CHECK_HEX16(cart.ReadSave(0x0000), 0xFF);

		FlashUnlock(cart);
		cart.WriteSave(0x5555, 0xA0);
		cart.WriteSave(0x0123, 0x77);
		GBA_CHECK(cart.SaveDirty());

		GBA_CHECK_MSG(cart.SaveSaveFile(path, &error), error);
		GBA_CHECK(!cart.SaveDirty());
	}

	// A second cartridge loads the file back.
	{
		Cart cart;
		GBA_CHECK_MSG(cart.LoadRom(image, error), error);
		GBA_CHECK_MSG(cart.LoadSaveFile(path, &error), error);
		GBA_CHECK_HEX16(cart.ReadSave(0x0123), 0x77);
		GBA_CHECK_HEX16(cart.ReadSave(0x0124), 0xFF);
		GBA_CHECK(!cart.SaveDirty());
	}

	// A clean save is not written back (the frontend calls SaveSaveFile on every shutdown).
	{
		Cart cart;
		GBA_CHECK_MSG(cart.LoadRom(image, error), error);
		GBA_CHECK_MSG(cart.LoadSaveFile(path, &error), error);
		GBA_CHECK(!cart.SaveDirty());
		std::remove(path.c_str());
		GBA_CHECK_MSG(cart.SaveSaveFile(path, &error), error);

		std::ifstream check(path, std::ios::binary);
		GBA_CHECK_MSG(!check.good(), "a clean save must not be written back");
	}

	// A shorter file is accepted: the rest of the memory stays erased.
	{
		WriteFile(path, std::vector<uint8_t>(100, 0x11));
		Cart cart;
		GBA_CHECK_MSG(cart.LoadRom(image, error), error);
		GBA_CHECK_MSG(cart.LoadSaveFile(path, &error), error);
		GBA_CHECK_HEX16(cart.ReadSave(0x0000), 0x11);
		GBA_CHECK_HEX16(cart.ReadSave(99), 0x11);
		GBA_CHECK_HEX16(cart.ReadSave(100), 0xFF);
	}

	// A bigger file is refused with a message instead of loading a truncated image.
	{
		WriteFile(path, std::vector<uint8_t>(64 * 1024 + 1, 0x22));
		Cart cart;
		GBA_CHECK_MSG(cart.LoadRom(image, error), error);
		std::string message;
		GBA_CHECK_MSG(!cart.LoadSaveFile(path, &message), "a larger save file must be refused");
		GBA_CHECK_MSG(!message.empty(), "the refusal must come with a message");
		GBA_CHECK_HEX16(cart.ReadSave(0x0000), 0xFF);		// the memory stays erased
	}

	// An empty path fails gracefully.
	{
		Cart cart;
		GBA_CHECK_MSG(cart.LoadRom(image, error), error);
		GBA_CHECK(!cart.LoadSaveFile("", &error));
		GBA_CHECK(!cart.SaveSaveFile("", &error));
	}

	// An EEPROM save file is 512 bytes / 8 KByte, never the 64 KByte SRAM window.
	{
		std::vector<uint8_t> eeprom = MakeRom();
		PutSignature(eeprom, 0x120, "EEPROM_V");

		Cart cart;
		GBA_CHECK_MSG(cart.LoadRom(eeprom, error), error);
		cart.SetSaveType(SaveType::Eeprom8K);

		const uint8_t written[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
		EepromSendRequest(cart, EepromWriteStream(0x10, 14, written), true);
		GBA_CHECK(cart.SaveDirty());
		GBA_CHECK_MSG(cart.SaveSaveFile(path, &error), error);

		std::ifstream check(path, std::ios::binary | std::ios::ate);
		GBA_CHECK(check.good());
		GBA_CHECK_EQ((size_t)check.tellg(), (size_t)8192);
	}

	std::remove(path.c_str());
}

// ---------------------------------------------------------------------------------------
// GPIO / RTC
// ---------------------------------------------------------------------------------------

GBA_TEST(Cart, GpioRomFallback)
{
	std::string error;

	// A cartridge without the port: the GPIO addresses are ordinary ROM bytes.
	std::vector<uint8_t> image = MakeRom();
	image[0xC4] = 0x5A;
	image[0xC6] = 0x3C;
	image[0xC8] = 0x00;

	Cart plain;
	GBA_CHECK_MSG(plain.LoadRom(image, error), error);
	GBA_CHECK(!plain.HasRtc());
	GBA_CHECK_HEX16(plain.ReadGpio(GpioData), 0x5A);
	GBA_CHECK_HEX16(plain.ReadGpio(GpioDirection), 0x3C);
	GBA_CHECK_HEX16(plain.ReadGpio(GpioControl), 0x00);

	// A cartridge with the port but the control register's bit0 clear is in write-only mode:
	// the ROM bytes still show through on reads (GBATEK "GBA Cart I/O Port (GPIO)").
	Cart rtc;
	GBA_CHECK_MSG(rtc.LoadRom(WithGpioPort(image), error), error);
	GBA_CHECK_MSG(rtc.HasRtc(), "the GPIO register literals must be detected");
	GBA_CHECK_HEX16(rtc.ReadGpio(GpioData), 0x5A);
	GBA_CHECK_HEX16(rtc.ReadGpio(GpioDirection), 0x3C);
	GBA_CHECK_HEX16(rtc.ReadGpio(GpioControl), 0x00);

	// Bit0 of the control register turns the port into a readable one. A data register read
	// returns the pin levels: SIO (bit1) is an input while its direction bit is clear, and the
	// RTC leaves the line high when it is not selected.
	rtc.WriteGpio(GpioControl, 0x01);
	GBA_CHECK_HEX16(rtc.ReadGpio(GpioControl), 0x01);
	GBA_CHECK_HEX16(rtc.ReadGpio(GpioDirection), 0x00);
	GBA_CHECK_HEX16(rtc.ReadGpio(GpioData), 0x02);

	// With the three lines driven as outputs the register reads back what was written.
	rtc.WriteGpio(GpioDirection, 0x07);
	GBA_CHECK_HEX16(rtc.ReadGpio(GpioDirection), 0x07);
	rtc.WriteGpio(GpioData, 0x0F);
	GBA_CHECK_HEX16(rtc.ReadGpio(GpioData), 0x0F);

	// The detection is only a hint: the UI can switch the port on for a cartridge whose
	// register literals it did not find.
	std::vector<uint8_t> plainRom = MakeRom();
	Cart forced;
	GBA_CHECK_MSG(forced.LoadRom(plainRom, error), error);
	GBA_CHECK(!forced.HasRtc());
	forced.SetRtcEnabled(true);
	GBA_CHECK(forced.HasRtc());
	forced.WriteGpio(GpioControl, 0x01);
	GBA_CHECK_HEX16(forced.ReadGpio(GpioControl), 0x01);
}

GBA_TEST(Cart, RtcProtocol)
{
	std::string error;
	Cart cart;
	GBA_CHECK_MSG(cart.LoadRom(WithGpioPort(MakeRom()), error), error);
	GBA_CHECK(cart.HasRtc());

	// Enable read/write access to the port.
	cart.WriteGpio(GpioControl, 0x01);

	// The chip powers up with the power failure flag set (GBATEK: "Setting after
	// Battery-Shortcut is 82h", bit7 is "auto cleared on read").
	RtcSelect(cart);
	RtcSendByte(cart, 0x63);					// function 13h = read the control register
	GBA_CHECK_HEX16(RtcReceiveByte(cart), 0x82);
	RtcDeselect(cart);

	RtcSelect(cart);
	RtcSendByte(cart, 0x63);
	GBA_CHECK_HEX16(RtcReceiveByte(cart), 0x02);	// the power flag was cleared by the read
	RtcDeselect(cart);

	// Write the control register (function 12h): bit6 = 24 hour mode, bit3 = per minute IRQ.
	RtcSelect(cart);
	RtcSendByte(cart, 0x62);
	RtcSendByte(cart, 0x48);
	RtcDeselect(cart);

	RtcSelect(cart);
	RtcSendByte(cart, 0x63);
	GBA_CHECK_HEX16(RtcReceiveByte(cart), 0x48);
	RtcDeselect(cart);

	// Set a known time (function 14h): year, month, day, weekday, hour, minute, second, all
	// packed BCD except the weekday counter. The seconds are written as 00 so the running
	// clock cannot roll anything over while the test reads the value back.
	RtcSelect(cart);
	RtcSendByte(cart, 0x64);
	RtcSendByte(cart, 0x24);					// 2024
	RtcSendByte(cart, 0x12);					// December
	RtcSendByte(cart, 0x31);					// 31st
	RtcSendByte(cart, 0x02);					// Tuesday
	RtcSendByte(cart, 0x13);					// 13:00 in 24 hour mode
	RtcSendByte(cart, 0x45);					// 45 minutes
	RtcSendByte(cart, 0x00);					// 00 seconds
	RtcDeselect(cart);

	// Read it back (function 15h). In 24 hour mode the AM/PM flag is forced by the hour value
	// (GBATEK "Time Registers": the GBA's AM/PM bit is bit7, on the DS it is bit6), so 13:00
	// reads back as 13h | 80h.
	RtcSelect(cart);
	RtcSendByte(cart, 0x65);
	uint8_t date[7];
	for (int i = 0; i < 7; i++)
		date[i] = RtcReceiveByte(cart);
	RtcDeselect(cart);

	GBA_CHECK_HEX16(date[0], 0x24);
	GBA_CHECK_HEX16(date[1], 0x12);
	GBA_CHECK_HEX16(date[2], 0x31);
	GBA_CHECK_HEX16(date[3], 0x02);
	GBA_CHECK_HEX16(date[4], 0x93);
	GBA_CHECK_HEX16(date[5], 0x45);
	GBA_CHECK_MSG(date[6] == 0x00 || date[6] == 0x01,
		"the clock may tick one second between the write and the read");

	// The time registers (function 17h) read the same hour/minute/second.
	RtcSelect(cart);
	RtcSendByte(cart, 0x67);
	uint8_t time[3];
	for (int i = 0; i < 3; i++)
		time[i] = RtcReceiveByte(cart);
	RtcDeselect(cart);
	GBA_CHECK_HEX16(time[0], 0x93);
	GBA_CHECK_HEX16(time[1], 0x45);

	// Switching to 12 hour mode (control bit6 clear) turns the same 13:00 into 01h with the
	// PM flag set; 12 o'clock is stored as 00h in that mode.
	RtcSelect(cart);
	RtcSendByte(cart, 0x62);
	RtcSendByte(cart, 0x08);
	RtcDeselect(cart);

	RtcSelect(cart);
	RtcSendByte(cart, 0x65);
	for (int i = 0; i < 7; i++)
		date[i] = RtcReceiveByte(cart);
	RtcDeselect(cart);
	GBA_CHECK_HEX16(date[4], 0x81);
	GBA_CHECK_HEX16(date[5], 0x45);
}

// ---------------------------------------------------------------------------------------
// Waitstates
// ---------------------------------------------------------------------------------------

GBA_TEST(Cart, WaitStates)
{
	std::string error;
	Cart cart;
	GBA_CHECK_MSG(cart.LoadRom(MakeRom(4 * 1024 * 1024), error), error);

	// WAITCNT = 0000h: wait state 0 first access 4 cycles, second access 2 cycles
	// (GBATEK "4000204h - WAITCNT"). The returned value is the waitstates alone, the access
	// itself adds one cycle.
	GBA_CHECK_EQ(cart.WaitStates(0x0000000, false, 0x0000), 4);
	GBA_CHECK_EQ(cart.WaitStates(0x0000000, true, 0x0000), 2);

	// WAITCNT = 4317h, the setting the cartridges ship with: WS0 = 3 cycles first, 1 second.
	GBA_CHECK_EQ(cart.WaitStates(0x0000000, false, 0x4317), 3);
	GBA_CHECK_EQ(cart.WaitStates(0x0000000, true, 0x4317), 1);

	// The second access is a single cycle when the prefetch buffer is on (bit14) and the
	// region runs at its lowest first access time (2 cycles, bits 2-3 = 2).
	GBA_CHECK_EQ(cart.WaitStates(0x0000000, false, 0x4008), 2);
	GBA_CHECK_EQ(cart.WaitStates(0x0000000, true, 0x4008), 0);
	// Without the prefetch buffer the second access keeps its WAITCNT setting (bit4 = 1 -> 1,
	// bit4 = 0 -> 2).
	GBA_CHECK_EQ(cart.WaitStates(0x0000000, true, 0x0010), 1);
	GBA_CHECK_EQ(cart.WaitStates(0x0000000, true, 0x0008), 2);

	// The three regions have their own fields: bits 2-3 (WS0), 5-6 (WS1) and 8-9 (WS2) for the
	// first access, bits 4, 7 and 10 for the second. The address may be given relative to
	// 08000000h (bits 25-26 then select the region) or as the full bus address the bus passes
	// in, both select the same region.
	GBA_CHECK_EQ(cart.WaitStates(0x08000000, false, 0x0000), 4);	// WS0 first
	GBA_CHECK_EQ(cart.WaitStates(0x0A000000, false, 0x0000), 4);	// WS1 first
	GBA_CHECK_EQ(cart.WaitStates(0x0C000000, true, 0x0000), 8);		// WS2 second = 8 cycles
	GBA_CHECK_EQ(cart.WaitStates(0x02000000, false, 0x0000), 4);	// WS1 first
	GBA_CHECK_EQ(cart.WaitStates(0x02000000, true, 0x0000), 4);		// WS1 second = 4 cycles
	GBA_CHECK_EQ(cart.WaitStates(0x04000000, false, 0x0000), 4);	// WS2 first
	GBA_CHECK_EQ(cart.WaitStates(0x04000000, true, 0x0000), 8);		// WS2 second = 8 cycles
	GBA_CHECK_EQ(cart.WaitStates(0x02000000, false, 0x0040), 2);	// WS1 first = 2 cycles
	GBA_CHECK_EQ(cart.WaitStates(0x04000000, false, 0x0200), 2);	// WS2 first = 2 cycles
	GBA_CHECK_EQ(cart.WaitStates(0x02000000, true, 0x0080), 1);		// WS1 second = 1 cycle
	GBA_CHECK_EQ(cart.WaitStates(0x04000000, true, 0x0400), 1);		// WS2 second = 1 cycle

	// A 128 Mbit (16 MByte) pak takes one cycle longer on the first access.
	Cart large;
	GBA_CHECK_MSG(large.LoadRom(MakeRom(16 * 1024 * 1024), error), error);
	GBA_CHECK_EQ(large.WaitStates(0x0000000, false, 0x0000), 5);
	GBA_CHECK_EQ(large.WaitStates(0x0000000, true, 0x0000), 2);
}
