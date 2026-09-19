// The Game Boy bus: the memory map, with the WRAM banking the CGB adds.
//
// The CGB's WRAM bank register (SVBK, 0xFF70) is the one piece of the memory map with a rule that
// is easy to get wrong: bank 0 is always at 0xC000..0xCFFF, banks 1-7 can be selected at
// 0xD000..0xDFFF, and "a written 0 maps bank 1" (Pan Docs "CGB Registers"). Mapping 0 to bank 0
// instead aliases the two windows onto one 4 KByte page, which is invisible to a test that only
// writes and reads one window - and fatal to a DMG cartridge whose variables or stack live in the
// upper bank, because its own low-RAM scratch then overwrites them (Metroid II kept its call/return
// state at 0xDFFB and never left its boot loop until this was fixed).

#include "gba_test.h"
#include "gb_bus.h"

using namespace GBA;

namespace
{
	/// <summary>A bus wired up the way GbSystem::Reset() does it: the CPU sees it and the
	/// devices are at their power-up state.</summary>
	void Prepare(GbBus& bus, bool cgb)
	{
		bus.cpu.bus = &bus;
		bus.SetCgb(cgb);
		bus.Reset();
	}
}

GBA_TEST(GbBus, cgb_power_up_svbk_maps_bank_one_not_bank_zero)
{
	GbBus bus;
	Prepare(bus, true);

	// The power-up SVBK keeps its bank bits clear, which by the register's own rule still means
	// bank 1 - so the machine starts with the upper bank separate from bank 0, as the hardware
	// does.
	GBA_CHECK_EQ(bus.ReadByte(0xFF70), 0xF8);

	bus.WriteByte(0xC000, 0x11);
	bus.WriteByte(0xD000, 0x22);
	GBA_CHECK_EQ(bus.ReadByte(0xC000), 0x11);
	GBA_CHECK_EQ(bus.ReadByte(0xD000), 0x22);		// not 0x11: 0xD000 is bank 1, not bank 0

	// The top of the upper bank and the top of bank 0 are different bytes; the debug report's
	// Peek() has to agree with the CPU's own reads.
	bus.WriteByte(0xDFFF, 0x33);
	bus.WriteByte(0xCFFF, 0x44);
	GBA_CHECK_EQ(bus.ReadByte(0xDFFF), 0x33);
	GBA_CHECK_EQ(bus.ReadByte(0xCFFF), 0x44);
	GBA_CHECK_EQ(bus.Peek(0xDFFF), 0x33);
	GBA_CHECK_EQ(bus.Peek(0xCFFF), 0x44);
}

GBA_TEST(GbBus, cgb_svbk_selects_banks_two_to_seven_and_zero_means_one)
{
	GbBus bus;
	Prepare(bus, true);

	bus.WriteByte(0xD000, 0x11);					// bank 1 (the power-up bank)

	bus.WriteByte(0xFF70, 0x02);					// bank 2
	GBA_CHECK_EQ(bus.ReadByte(0xFF70), 0xFA);		// the write is held, bits 3-7 read as ones
	GBA_CHECK_EQ(bus.ReadByte(0xD000), 0x00);
	bus.WriteByte(0xD000, 0x22);

	bus.WriteByte(0xFF70, 0x03);					// bank 3
	GBA_CHECK_EQ(bus.ReadByte(0xD000), 0x00);
	bus.WriteByte(0xD000, 0x33);

	bus.WriteByte(0xFF70, 0x02);					// back to bank 2
	GBA_CHECK_EQ(bus.ReadByte(0xD000), 0x22);

	bus.WriteByte(0xFF70, 0x00);					// "a written 0 maps bank 1"
	GBA_CHECK_EQ(bus.ReadByte(0xD000), 0x11);

	bus.WriteByte(0xFF70, 0x07);					// the last bank
	GBA_CHECK_EQ(bus.ReadByte(0xD000), 0x00);
	bus.WriteByte(0xD000, 0x77);
	bus.WriteByte(0xFF70, 0x01);					// bank 1 again
	GBA_CHECK_EQ(bus.ReadByte(0xD000), 0x11);

	// Whatever 0xD000 is showing, bank 0 at 0xC000 is untouched by it.
	GBA_CHECK_EQ(bus.ReadByte(0xC000), 0x00);
}

GBA_TEST(GbBus, dmg_upper_wram_is_bank_one_and_svbk_is_not_there)
{
	GbBus bus;
	Prepare(bus, false);

	bus.WriteByte(0xC000, 0x55);
	bus.WriteByte(0xD000, 0x66);
	GBA_CHECK_EQ(bus.ReadByte(0xC000), 0x55);
	GBA_CHECK_EQ(bus.ReadByte(0xD000), 0x66);

	// A DMG has no SVBK: it reads as 0xFF and ignores writes, and the upper bank stays put.
	GBA_CHECK_EQ(bus.ReadByte(0xFF70), 0xFF);
	bus.WriteByte(0xFF70, 0x02);
	GBA_CHECK_EQ(bus.ReadByte(0xFF70), 0xFF);
	GBA_CHECK_EQ(bus.ReadByte(0xD000), 0x66);
}
