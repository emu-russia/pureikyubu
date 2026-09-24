// The GBA address decoder and the system clock.
//
// Everything the CPU does to the outside world passes through here, and everything the devices do
// to each other is scheduled from here. The region decode follows GBATEK "GBA Memory Map"; the
// byte-lane rules of the 16-bit I/O registers follow GBATEK "I/O Register Byte Access" (a byte
// access to a 16-bit register writes the byte the CPU drove; where the hardware mirrors the low
// byte in both lanes - GREENSWP and KEYINPUT are the famous ones - that is implemented
// explicitly).
//
// One simplification is worth knowing: the waitstates are charged from the cartridge's WAITCNT
// model, but the internal RAM's own access times (EWRAM is a 16-bit bus, IWRAM a 32-bit one) are
// not. The cycle counts of the CPU itself are exact. See testing/gba_bench/Readme.md.

#include "gba_bus.h"
#include "gba_hlebios.h"
#include "gba_savestate.h"

namespace GBA
{
	void GbaBus::SaveState(StateWriter& writer) const
	{
		writer.Fields(openBus, waitCycles, totalCycles, customBios, postFlg, haltState, waitcnt,
			sramWait, romNext, romChain, fetching, memControl, dmaAccess);

		writer.Raw(ewram.Data(), ewram.Size());
		writer.Raw(iwram.Data(), iwram.Size());
		writer.Raw(io.Data(), io.Size());
	}

	void GbaBus::LoadState(StateReader& reader)
	{
		reader.Fields(openBus, waitCycles, totalCycles, customBios, postFlg, haltState, waitcnt,
			sramWait, romNext, romChain, fetching, memControl, dmaAccess);

		reader.Raw(ewram.Data(), ewram.Size());
		reader.Raw(iwram.Data(), iwram.Size());
		reader.Raw(io.Data(), io.Size());
	}

	/// <summary>
	/// True when the address is inside the cartridge's GPIO/RTC window.
	///
	/// GBATEK "GBA GPIO": the real time clock port of the cartridges that have one lives at
	/// 0x080000C4 (data), 0x080000C6 (direction) and 0x080000C8 (control) - which is inside the
	/// cartridge header. The port only exists while the game has enabled it in the control
	/// register; until then these are ordinary ROM bytes, which is exactly what
	/// Cart::ReadGpio/WriteGpio implement. The bus therefore hands the whole window to the
	/// cartridge and lets it decide.
	/// </summary>
	static bool IsGpioAddress(uint32_t address)
	{
		// The window is exactly the three registers (0xC4 data, 0xC6 direction, 0xC8 control), not
		// the whole header: a cartridge whose entry code sits at 0x080000C0 (the harness's own
		// demo does) must still execute from there, and the cartridge returns the ROM bytes for
		// these addresses while the port is not enabled.
		return address >= MemRom1 + 0xC4 && address <= MemRom1 + 0xC9;
	}

	GbaBus::GbaBus()
		: cpu(this)
	{
		ewram.Init(EwramSize);
		iwram.Init(IwramSize);
		bios.Init(BiosSize);
		io.Init(IoSize);

		// The BIOS is whatever was installed; the custom boot ROM is installed by GbaSystem.
		bios.Fill(0xFF);

		Reset();
	}

	GbaBus::~GbaBus()
	{
	}

	void GbaBus::Reset()
	{
		ewram.Fill(0);
		iwram.Fill(0);
		io.Fill(0);

		cpu.Reset();
		ppu.Reset();
		apu.Reset();
		sio.Reset();
		dma.Reset();
		timers.Reset();
		irq.Reset();
		keypad.Reset();
		cart.Reset();

		openBus = 0;
		waitCycles = 0;
		totalCycles = 0;
		postFlg = 0;
		haltState = 0;
		waitcnt = 0;
		UpdateWaitStates();
	}

	void GbaBus::SetBios(const uint8_t* data, size_t size)
	{
		bios.Fill(0xFF);

		if (data != nullptr && size != 0)
		{
			if (size > BiosSize)
			{
				size = BiosSize;
			}
			memcpy(bios.Data(), data, size);
		}
	}

	// ---------------------------------------------------------------------------------------
	// Waitstates
	// ---------------------------------------------------------------------------------------

	void GbaBus::UpdateWaitStates()
	{
		// GBATEK 4000204h: SRAM and the three ROM windows. The cartridge's model does the
		// arithmetic for the ROM; the SRAM's own wait is kept here because the bus drives those
		// accesses itself.
		static const int sramWaits[4] = { 4, 3, 2, 8 };
		sramWait = sramWaits[waitcnt & 3];
	}

	int GbaBus::WramWaitStates() const
	{
		// GBATEK 4000800h: "The default value 0Dh in Bits 24-27 selects 2 waitstates for 256K
		// WRAM (ie. 3/3/6 cycles 8/16/32bit accesses). The fastest possible setting would be
		// 0Eh (1 waitstate ...)"; value 15 is no waitstate at all.
		int waits = 15 - (int)((memControl >> 24) & 0xF);
		return (waits < 0) ? 0 : waits;
	}

	int GbaBus::InternalWaitCycles(uint32_t address, int bytes) const
	{
		switch (address >> 24)
		{
		case 0x02:
			// On-board 256K WRAM: 1 + waits cycles for 8 and 16 bit accesses, and two such
			// accesses for a 32 bit one (2 * (1 + waits)).
			return (bytes == 4) ? (2 * (1 + WramWaitStates()) - 1) : WramWaitStates();

		case 0x05:			// Palette RAM
		case 0x06:			// VRAM
		case 0x07:			// OAM
			// 1/1/2: a 32 bit access is two bus cycles where the CPU counts one.
			return (bytes == 4) ? 1 : 0;

		default:
			// BIOS, the 32K on-chip WRAM and the I/O area are 1/1/1.
			return 0;
		}
	}

	bool GbaBus::RomSequential(uint32_t address) const
	{
		// "The GBA forcefully uses non-sequential timing at the beginning of each 128K-block of
		// gamepak ROM, eg. 'LDMIA [801fff8h],r0-r7' will have non-sequential timing at 8020000h"
		// (GBATEK "GBA GamePak Prefetch").
		if ((address & 0x1FFFF) == 0)
			return false;

		return romChain && address == romNext;
	}

	void GbaBus::ChargeRom(uint32_t address, int bytes, bool fetch)
	{
		// Whether this access continues the previous one has to be asked *before* the chain is
		// moved on to this access's end.
		bool sequential = RomSequential(address);

		romNext = address + (uint32_t)bytes;
		romChain = true;

		// The prefetch buffer holds the next eight halfwords, so a fetch is served from it and
		// costs nothing beyond the CPU's own cycle: the AGB aging cartridge's PREFETCH BUFFER
		// check measures a tight loop in the cartridge twice, 24 cycles with bit 14 of WAITCNT
		// set against 51 without, and only the first of those two adds up this way.
		if (fetch && (waitcnt & 0x4000) != 0)
			return;

		int total = cart.WaitStates(address, sequential, waitcnt);

		if (bytes == 4)
		{
			// "GamePak uses 16bit data bus, so that a 32bit access is split into TWO 16bit
			// accesses (of which, the second fragment is always sequential, even if the first
			// fragment was non-sequential)" (GBATEK 4000204h).
			total += cart.WaitStates(address + 2, true, waitcnt);
		}

		// A 32 bit access is two fragments on this 16 bit bus, so the CPU pays one cycle more
		// than the single N cycle our interpreter counts for it; the second fragment's cycle is
		// added here. The DMA engine counts both fragments itself, so it does not need it.
		int counted = 1;
		if (bytes == 4)
			counted = dmaAccess ? 2 : 0;

		if (total > counted)
			AddWaitCycles(total - counted);

		if (getenv("GBA_TRACE_ROM") && bytes == 4 && address >= 0x08003260u && address < 0x080032B0u)
			fprintf(stderr, "ROMF32 %08X total=%d counted=%d seq=%d pref=%d fetch=%d\n",
				address, total, counted, (int)sequential, (waitcnt & 0x4000) ? 1 : 0, (int)fetch);
	}

	int GbaBus::TakeWaitCycles()
	{
		int cycles = waitCycles;
		waitCycles = 0;
		return cycles;
	}

	// ---------------------------------------------------------------------------------------
	// The region decode
	// ---------------------------------------------------------------------------------------

	uint8_t GbaBus::Read8(uint32_t address)
	{
		uint32_t region = address >> 24;

		switch (region)
		{
			case 0x00:
				// The BIOS is 16 KByte; the rest of the first 32 MByte is unused.
				if (address < BiosSize)
				{
					openBus = (openBus & 0xFFFFFF00u) | bios.Read8(address);
					return (uint8_t)openBus;
				}
				return (uint8_t)(openBus >> ((address & 3) * 8));

			case 0x02:
				AddWaitCycles(InternalWaitCycles(address, 1));
				openBus = (openBus & 0xFFFFFF00u) | ewram.Read8(address - MemEwram);
				return (uint8_t)openBus;

			case 0x03:
				openBus = (openBus & 0xFFFFFF00u) | iwram.Read8(address - MemIwram);
				return (uint8_t)openBus;

			case 0x04:
				if (IsMemControl(address))
					return (uint8_t)(memControl >> ((address & 3) * 8));
				return ReadIo8(address & (IoSize - 1));

			case 0x05:
				openBus = (openBus & 0xFFFFFF00u) | ppu.ReadPalette(address & (PaletteSize - 1));
				return (uint8_t)openBus;

			case 0x06:
				// VRAM is mirrored by the PPU itself (with a modulo - 96 KByte is not a power of
				// two, so masking the address here with `VramSize - 1` would drop bit 15 and fold
				// the second 32 KByte onto the first, which is exactly what a mode 3 bitmap must
				// not do).
				openBus = (openBus & 0xFFFFFF00u) | ppu.ReadVram(address);
				return (uint8_t)openBus;

			case 0x07:
				openBus = (openBus & 0xFFFFFF00u) | ppu.ReadOam(address & (OamSize - 1));
				return (uint8_t)openBus;

			case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
			{
				ChargeRom(address, 1, fetching);

				uint8_t value = IsGpioAddress(address) ? cart.ReadGpio(address) : cart.ReadRom8(address);
				openBus = (openBus & 0xFFFFFF00u) | value;
				return value;
			}

			case 0x0E: case 0x0F:
				AddWaitCycles(sramWait);
				openBus = (openBus & 0xFFFFFF00u) | cart.ReadSave(address - MemSram);
				return (uint8_t)openBus;

			default:
				// Unmapped: the last value the bus drove, as the hardware leaves floating.
				return (uint8_t)(openBus >> ((address & 3) * 8));
		}
	}

	uint16_t GbaBus::Read16(uint32_t address)
	{
		address &= ~1u;

		uint32_t region = address >> 24;

		switch (region)
		{
			case 0x00:
				if (address < BiosSize)
				{
					uint16_t value = bios.Read16(address);
					openBus = value;
					return value;
				}
				return (uint16_t)openBus;

			case 0x02:
			{
				AddWaitCycles(InternalWaitCycles(address, 2));
				uint16_t value = ewram.Read16(address - MemEwram);
				openBus = value;
				return value;
			}

			case 0x03:
			{
				uint16_t value = iwram.Read16(address - MemIwram);
				openBus = value;
				return value;
			}

			case 0x04:
				if (IsMemControl(address))
					return (uint16_t)(memControl >> ((address & 2) * 8));
				if (getenv("GBA_TRACE_TMR") && (address & 0x00FFFFFF) == 0x000100)
				{
					uint16_t v = ReadIo16(address & (IoSize - 1));
					fprintf(stderr, "TM %d %04X\n", ppu.FrameCounter(), (unsigned)v);
					return v;
				}
				return ReadIo16(address & (IoSize - 1));

			case 0x05:
			{
				// A whole palette entry, not two byte reads: a byte read of the palette has the
				// hardware's OR rule (see Ppu::ReadPalette16).
				uint16_t value = ppu.ReadPalette16(address & (PaletteSize - 1));
				openBus = value;
				return value;
			}

			case 0x06:
			{
				uint16_t value = (uint16_t)(ppu.ReadVram(address)
					| (ppu.ReadVram(address + 1) << 8));
				openBus = value;
				return value;
			}

			case 0x07:
			{
				uint16_t value = (uint16_t)(ppu.ReadOam(address & (OamSize - 1))
					| (ppu.ReadOam((address + 1) & (OamSize - 1)) << 8));
				openBus = value;
				return value;
			}

			case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
			{
				ChargeRom(address, 2, fetching);

				uint16_t value;

				if (IsGpioAddress(address))
				{
					// The GPIO registers are byte wide (the RTC is bit-banged a byte at a time), so
					// only the byte at the register's own address belongs to the port; the rest of
					// the halfword is the cartridge's ROM. That is what makes a *code fetch* from
					// the entry area (0x080000C0 and up, which real cartridges use) work: with the
					// port switched off, Cart::ReadGpio returns the ROM byte, so the halfword is
					// exactly the ROM's.
					value = (uint16_t)(cart.ReadGpio(address) | (cart.ReadRom8(address + 1) << 8));
				}
				else
				{
					value = cart.ReadRom16(address);
				}

				openBus = value;
				return value;
			}

			case 0x0E: case 0x0F:
			{
				AddWaitCycles(sramWait);
				uint16_t value = (uint16_t)(cart.ReadSave(address - MemSram)
					| (cart.ReadSave(address + 1 - MemSram) << 8));
				openBus = value;
				return value;
			}

			default:
				return (uint16_t)openBus;
		}
	}

	// The debugger's view of the address space: the same decode as Read16, without the waitstates
	// and without the open-bus latch. A debugger walks memory continuously (a disassembly, a
	// memory panel), so the read it performs must not move the machine.
	uint16_t GbaBus::Peek16(uint32_t address) const
	{
		address &= ~1u;

		uint32_t region = address >> 24;

		switch (region)
		{
			case 0x00:
				return (address < BiosSize) ? bios.Read16(address) : (uint16_t)openBus;

			case 0x02:
				return ewram.Read16(address - MemEwram);

			case 0x03:
				return iwram.Read16(address - MemIwram);

			case 0x04:
				return io.Read16(address & (IoSize - 1));

			case 0x05:
				return ppu.ReadPalette16(address & (PaletteSize - 1));

			case 0x06:
				return (uint16_t)(ppu.ReadVram(address) | (ppu.ReadVram(address + 1) << 8));

			case 0x07:
				return (uint16_t)(ppu.ReadOam(address & (OamSize - 1))
					| (ppu.ReadOam((address + 1) & (OamSize - 1)) << 8));

			case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
				if (IsGpioAddress(address))
				{
					// Reading the port can advance the RTC (RtcReadBit clocks the bit stream), which
					// is a side effect a debugger must not have, so the GPIO window is read the way
					// a cartridge without the port answers: the ROM bytes at the register addresses
					// (GBATEK "GBA GPIO", write-only mode).
					uint32_t reg = address & 0xFF;
					uint8_t romByte = (reg == 0x04 || reg == 0xC4) ? cart.ReadRom8(0xC4)
						: ((reg == 0x06 || reg == 0xC6) ? cart.ReadRom8(0xC6) : cart.ReadRom8(0xC8));
					uint8_t second = (reg == 0x08 || reg == 0xC8) ? cart.ReadRom8(0xC9) : cart.ReadRom8(reg + 1);
					return (uint16_t)(romByte | (second << 8));
				}

				return cart.ReadRom16(address);

			case 0x0E: case 0x0F:
				return (uint16_t)(cart.PeekSave(address - MemSram)
					| (cart.PeekSave(address + 1 - MemSram) << 8));

			default:
				return (uint16_t)openBus;
		}
	}

	uint32_t GbaBus::Read32(uint32_t address)
	{
		address &= ~3u;

		uint32_t region = address >> 24;

		switch (region)
		{
			case 0x00:
				if (address < BiosSize)
				{
					uint32_t value = bios.Read32(address);
					openBus = value;
					return value;
				}
				return openBus;

			case 0x02:
			{
				if (!dmaAccess)
					AddWaitCycles(InternalWaitCycles(address, 4) - 2 * InternalWaitCycles(address, 2));
				uint32_t value = ewram.Read32(address - MemEwram);
				openBus = value;
				return value;
			}

			case 0x03:
			{
				uint32_t value = iwram.Read32(address - MemIwram);
				openBus = value;
				return value;
			}

			case 0x04:
				if (IsMemControl(address))
					return memControl;
				return (uint32_t)ReadIo16(address & (IoSize - 1))
					| ((uint32_t)ReadIo16((address + 2) & (IoSize - 1)) << 16);

			case 0x05:
			case 0x06:
			case 0x07:
			{
				// Palette RAM 1/1/2, VRAM 1/1/2, OAM 1/1/2 (GBATEK "GBA Memory Map"): the two
				// 16 bit halves cost a cycle each, the CPU counts one for the whole access.
				if (!dmaAccess)
					AddWaitCycles(InternalWaitCycles(address, 4) - 2 * InternalWaitCycles(address, 2));
				uint32_t value = (uint32_t)Read16(address) | ((uint32_t)Read16(address + 2) << 16);
				return value;
			}

			case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
			{
				ChargeRom(address, 4, fetching);

				uint32_t value;

				if (IsGpioAddress(address))
				{
					// See Read16: only the register's own byte is the port, the rest is ROM.
					value = (uint32_t)cart.ReadGpio(address)
						| ((uint32_t)cart.ReadRom8(address + 1) << 8)
						| ((uint32_t)cart.ReadRom8(address + 2) << 16)
						| ((uint32_t)cart.ReadRom8(address + 3) << 24);
				}
				else
				{
					value = cart.ReadRom32(address);
				}

				openBus = value;
				return value;
			}

			case 0x0E: case 0x0F:
			{
				AddWaitCycles(sramWait * 2);
				uint32_t value = (uint32_t)cart.ReadSave(address - MemSram)
					| ((uint32_t)cart.ReadSave(address + 1 - MemSram) << 8)
					| ((uint32_t)cart.ReadSave(address + 2 - MemSram) << 16)
					| ((uint32_t)cart.ReadSave(address + 3 - MemSram) << 24);
				openBus = value;
				return value;
			}

			default:
				return openBus;
		}
	}

	void GbaBus::Write8(uint32_t address, uint8_t value)
	{
		uint32_t region = address >> 24;

		switch (region)
		{
			case 0x00:
				// The BIOS is a mask ROM: writes do nothing.
				break;

			case 0x02:
				AddWaitCycles(InternalWaitCycles(address, 1));
				ewram.Write8(address - MemEwram, value);
				break;

			case 0x03:
				iwram.Write8(address - MemIwram, value);
				break;

			case 0x04:
				if (IsMemControl(address))
				{
					uint32_t shift = (address & 3) * 8;
					memControl = (memControl & ~(0xFFu << shift)) | ((uint32_t)value << shift);
					break;
				}
				WriteIo8(address & (IoSize - 1), value);
				break;

			case 0x05:
				// Palette RAM sits on the 16-bit bus and an 8-bit store drives *both* byte lanes,
				// so the byte is duplicated into the whole halfword (GBATEK "LCD Color Palettes";
				// the public test ROM `jsmolka/gba-tests` checks it: storing 0x01 and reading the
				// halfword back gives 0x0101).
				ppu.WritePalette(address & (PaletteSize - 1), value);
				ppu.WritePalette((address | 1) & (PaletteSize - 1), value);
				break;

			case 0x06:
			{
				// The mirroring is the PPU's business (see the 8-bit read above); an 8-bit store
				// inside the 0x10000-0x17FFF window (the object tile data) is ignored by the
				// hardware, and everywhere else the byte is duplicated into both lanes of the
				// halfword (checked by the `memory` test ROM of `jsmolka/gba-tests`).
				uint32_t offset = address % VramSize;

				if (offset < 0x10000)
				{
					ppu.WriteVram(address & ~1u, value);
					ppu.WriteVram((address | 1), value);
				}
				break;
			}

			case 0x07:
				// OAM ignores 8-bit stores (GBATEK "OAM"; checked by the `memory` test ROM).
				break;

			case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
				ChargeRom(address, 2, false);

				if (IsGpioAddress(address))
				{
					// The RTC protocol is bit-banged through these bytes.
					cart.WriteGpio(address, value);
				}
				else
				{
					cart.WriteRom8(*this, address, value);
				}
				break;

			case 0x0E: case 0x0F:
				AddWaitCycles(sramWait);
				cart.WriteSave(address - MemSram, value);
				break;

			default:
				break;
		}
	}

	void GbaBus::Write16(uint32_t address, uint16_t value)
	{
		address &= ~1u;

		uint32_t region = address >> 24;

		switch (region)
		{
			case 0x00:
				break;

			case 0x02:
				AddWaitCycles(InternalWaitCycles(address, 2));
				ewram.Write16(address - MemEwram, value);
				break;

			case 0x03:
				iwram.Write16(address - MemIwram, value);
				break;

			case 0x04:
				if (IsMemControl(address))
				{
					uint32_t shift = (address & 2) * 8;
					memControl = (memControl & ~(0xFFFFu << shift)) | ((uint32_t)value << shift);
					break;
				}
				WriteIo16(address & (IoSize - 1), value);
				break;

			case 0x05:
				ppu.WritePalette16(address & (PaletteSize - 1), value);
				break;

			case 0x06:
				// The mirroring is the PPU's business (see Read16/Read8 above).
				ppu.WriteVram(address, (uint8_t)value);
				ppu.WriteVram(address + 1, (uint8_t)(value >> 8));
				break;

			case 0x07:
				ppu.WriteOam16(address, value);
				break;

			case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
				ChargeRom(address, 2, false);

				if (IsGpioAddress(address))
				{
					cart.WriteGpio(address, (uint8_t)value);
					cart.WriteGpio(address + 1, (uint8_t)(value >> 8));
				}
				else
				{
					cart.WriteRom16(*this, address, value);
				}
				break;

			case 0x0E: case 0x0F:
				AddWaitCycles(sramWait);
				cart.WriteSave(address - MemSram, (uint8_t)value);
				cart.WriteSave(address + 1 - MemSram, (uint8_t)(value >> 8));
				break;

			default:
				break;
		}
	}

	void GbaBus::Write32(uint32_t address, uint32_t value)
	{
		// A 32-bit write is two 16-bit writes, which is exactly what the 16-bit bus does; the
		// order matters for the peripherals that auto-increment (the FIFOs), so the halves are
		// written low first, as the hardware does.
		//
		// The two halves have charged their own waitstates by then; what is left is the 32 bit
		// access's own extra cycle on the memories that take 1/1/2 (GBATEK "GBA Memory Map"),
		// because the CPU counts one cycle for the whole store.
		if (!dmaAccess)
			AddWaitCycles(InternalWaitCycles(address, 4) - 2 * InternalWaitCycles(address, 2));
		Write16(address, (uint16_t)value);
		Write16(address + 2, (uint16_t)(value >> 16));
	}

	// ---------------------------------------------------------------------------------------
	// Code fetches
	// ---------------------------------------------------------------------------------------

	uint16_t GbaBus::Fetch16(uint32_t address)
	{
		fetching = true;
		uint16_t value = Read16(address);
		fetching = false;

		openBus = value;
		return value;
	}

	uint32_t GbaBus::Fetch32(uint32_t address)
	{
		uint32_t region = address >> 24;

		if (region >= 0x08 && region <= 0x0D && !IsGpioAddress(address))
		{
			// An ARM instruction fetch from the cartridge is one 32 bit access, so that the
			// second 16 bit fragment takes the ROM's sequential timing and the access is charged
			// as a whole (see ChargeRom).
			ChargeRom(address, 4, true);
			uint32_t value = cart.ReadRom32(address);
			openBus = value;
			return value;
		}

		fetching = true;
		uint32_t low = Read16(address);
		uint32_t high = Read16(address + 2);
		fetching = false;

		openBus = low | (high << 16);
		return low | (high << 16);
	}

	// ---------------------------------------------------------------------------------------
	// The I/O register file
	// ---------------------------------------------------------------------------------------

	uint16_t GbaBus::ReadIo16(uint32_t offset)
	{
		offset &= 0x3FE;

		uint16_t open16 = (uint16_t)openBus;

		if (offset <= 0x05E)
		{
			uint16_t value = ppu.Read16(offset, open16);
			openBus = value;
			return value;
		}

		if (offset >= 0x060 && offset <= 0x0A6)
		{
			uint16_t value = (uint16_t)(apu.Read8(offset, (uint8_t)open16) | (apu.Read8(offset + 1, (uint8_t)(open16 >> 8)) << 8));
			openBus = value;
			return value;
		}

		if (offset >= 0x0B0 && offset <= 0x0DE)
		{
			uint16_t value = dma.Read16(offset, open16);
			openBus = value;
			return value;
		}

		if (offset >= 0x100 && offset <= 0x10E)
		{
			uint16_t value = timers.Read16(offset);
			openBus = value;
			return value;
		}

		if (offset == 0x130)
		{
			// KEYINPUT: a byte access mirrors the low byte in both lanes (GBATEK 4000130h).
			uint16_t value = keypad.ReadKeyInput();
			openBus = value;
			return value;
		}

		if (offset == 0x132)
		{
			uint16_t value = keypad.ReadKeyCnt();
			openBus = value;
			return value;
		}

		if (offset >= 0x120 && offset <= 0x15A)
		{
			uint16_t value = sio.Read16(*this, offset, open16);
			openBus = value;
			return value;
		}

		switch (offset)
		{
			case 0x200: return irq.ReadIE();
			case 0x202: return irq.ReadIF();
			case 0x204: return waitcnt;
			case 0x208: return irq.ReadIME() ? 1 : 0;
			default: break;
		}

		return open16;
	}

	uint8_t GbaBus::ReadIo8(uint32_t offset)
	{
		offset &= (IoSize - 1);

		// The byte lanes of a 16-bit register: the byte the CPU addressed, unless the hardware
		// mirrors the low byte in both lanes.
		uint8_t open8 = (uint8_t)openBus;

		if (offset <= 0x05F)
		{
			uint16_t value = ppu.Read16(offset & ~1u, (uint16_t)openBus);

			// GREENSWP (0x04000004) mirrors its low byte into both lanes.
			if ((offset & ~1u) == 0x004)
			{
				return (uint8_t)value;
			}

			return (uint8_t)(value >> ((offset & 1) * 8));
		}

		if (offset >= 0x060 && offset <= 0x0A7)
		{
			return apu.Read8(offset, open8);
		}

		if (offset >= 0x0B0 && offset <= 0x0DF)
		{
			uint16_t value = dma.Read16(offset & ~1u, (uint16_t)openBus);
			return (uint8_t)(value >> ((offset & 1) * 8));
		}

		if (offset >= 0x100 && offset <= 0x10F)
		{
			uint16_t value = timers.Read16(offset & ~1u);
			return (uint8_t)(value >> ((offset & 1) * 8));
		}

		if (offset == 0x130 || offset == 0x131)
		{
			// A byte read of KEYINPUT returns the low byte in both lanes.
			return (uint8_t)keypad.ReadKeyInput();
		}

		if (offset == 0x132 || offset == 0x133)
		{
			return (uint8_t)(keypad.ReadKeyCnt() >> ((offset & 1) * 8));
		}

		if (offset >= 0x120 && offset <= 0x15B)
		{
			return sio.Read8(*this, offset, open8);
		}

		switch (offset)
		{
			case 0x200: case 0x201: return (uint8_t)(irq.ReadIE() >> ((offset & 1) * 8));
			case 0x202: case 0x203: return (uint8_t)(irq.ReadIF() >> ((offset & 1) * 8));
			case 0x204: case 0x205: return (uint8_t)(waitcnt >> ((offset & 1) * 8));
			case 0x208: case 0x209: return irq.ReadIME() ? 1 : 0;
			case 0x300: return postFlg;
			case 0x301: return (uint8_t)haltState;
			default: break;
		}

		return open8;
	}

	void GbaBus::WriteIo16(uint32_t offset, uint16_t value)
	{
		offset &= 0x3FE;

		if (offset <= 0x05E)
		{
			ppu.Write16(*this, offset, value, (int)totalCycles);
			return;
		}

		if (offset >= 0x060 && offset <= 0x0A6)
		{
			apu.Write8(offset, (uint8_t)value);
			apu.Write8(offset + 1, (uint8_t)(value >> 8));
			return;
		}

		if (offset >= 0x0B0 && offset <= 0x0DE)
		{
			dma.Write16(*this, offset, value);
			return;
		}

		if (offset >= 0x100 && offset <= 0x10E)
		{
			timers.Write16(*this, offset, value);
			return;
		}

		if (offset == 0x130)
		{
			// KEYINPUT is read only.
			return;
		}

		if (offset == 0x132)
		{
			keypad.WriteKeyCnt(value);
			// A write to KEYCNT can satisfy its own condition immediately.
			if (keypad.IrqRequested())
			{
				irq.Raise(INT_KEYPAD);
			}
			return;
		}

		if (offset >= 0x120 && offset <= 0x15A)
		{
			sio.Write16(*this, offset, value);
			return;
		}

		switch (offset)
		{
			case 0x200:
				irq.WriteIE(value);
				break;

			case 0x202:
				// Writing IF acknowledges the causes whose bits are set.
				irq.WriteIF(value);
				break;

			case 0x204:
				// Bit 15 is the read-only Game Pak Type Flag (GBATEK 4000204h: "(Read Only)
				// (0=GBA, 1=CGB) (IN35 signal)"), so a write cannot set it on a GBA cartridge.
				waitcnt = (uint16_t)(value & 0x7FFF);
				UpdateWaitStates();
				break;

			case 0x208:
				irq.WriteIME((value & 1) != 0);
				break;

			case 0x300:
				// POSTFLG: bit 0 is writable, bit 1 is the "the BIOS has run" flag the hardware
				// sets and the games only read.
				postFlg = (uint8_t)(value & 1) | 0x02;
				break;

			case 0x301:
				WriteHaltCnt((uint8_t)value);
				break;

			default:
				break;
		}
	}

	void GbaBus::WriteIo8(uint32_t offset, uint8_t value)
	{
		offset &= (IoSize - 1);

		// A byte write to a 16-bit register keeps the other lane, which is what the hardware's
		// byte lanes do.
		auto writeLane = [&]()
		{
			uint32_t aligned = offset & ~1u;
			uint16_t old = ReadIo16(aligned);
			uint16_t merged = (offset & 1) ? (uint16_t)((old & 0x00FF) | (value << 8)) : (uint16_t)((old & 0xFF00) | value);
			WriteIo16(aligned, merged);
		};

		if (offset <= 0x05F)
		{
			if ((offset & ~1u) == 0x004)
			{
				// GREENSWP's low byte is mirrored in both lanes.
				WriteIo16(0x004, (uint16_t)(value | (value << 8)));
				return;
			}

			writeLane();
			return;
		}

		if (offset >= 0x060 && offset <= 0x0A7)
		{
			apu.Write8(offset, value);
			return;
		}

		if (offset >= 0x0B0 && offset <= 0x0DF)
		{
			writeLane();
			return;
		}

		if (offset >= 0x100 && offset <= 0x10F)
		{
			writeLane();
			return;
		}

		if (offset == 0x130 || offset == 0x131)
		{
			return;			// read only
		}

		if (offset == 0x132 || offset == 0x133)
		{
			writeLane();
			return;
		}

		if (offset >= 0x120 && offset <= 0x15B)
		{
			sio.Write8(*this, offset, value);
			return;
		}

		switch (offset)
		{
			case 0x200: case 0x201:
			case 0x202: case 0x203:
			case 0x204: case 0x205:
			case 0x208: case 0x209:
				writeLane();
				break;

			case 0x300:
				postFlg = (uint8_t)(value & 1) | 0x02;
				break;

			case 0x301:
				WriteHaltCnt(value);
				break;

			default:
				break;
		}
	}

	// ---------------------------------------------------------------------------------------
	// HALT
	// ---------------------------------------------------------------------------------------

	void GbaBus::WriteHaltCnt(uint8_t value)
	{
		if (value & 0x80)
		{
			// HALTCNT 0x80: the CPU stops until an interrupt is requested.
			haltState = value;
			cpu.Halt();
		}
		else
		{
			// 0x00 is STOP: the console would switch off most of the hardware and wait for a
			// keypad interrupt. The games use it to save power; modelling it as a HALT that the
			// keypad can always wake is enough for compatibility, and it is documented.
			haltState = value;
			cpu.Halt();
		}
	}

	// ---------------------------------------------------------------------------------------
	// BIOS calls
	// ---------------------------------------------------------------------------------------

	bool GbaBus::Swi(uint32_t comment)
	{
		if (!HleBiosEnabled)
		{
			return false;
		}

		return HleBios::Swi(*this, comment);
	}

	void GbaBus::ServiceEepromDma()
	{
		// The EEPROM's bit stream is collected by Cart::WriteRom16 as the DMA writes the ROM
		// window, and the transfer completes there when the chip select is raised (see
		// gba_cart.cpp). This hook exists for the bus to be able to poll it; nothing is left to
		// do synchronously.
	}

	// ---------------------------------------------------------------------------------------
	// The clock
	// ---------------------------------------------------------------------------------------

	void GbaBus::TickDevices(int cycles)
	{
		int budget = cycles;
		int guard = 0;

		while (budget > 0 && guard++ < 1000000)
		{
			int slice = (budget > 64) ? 64 : budget;

			totalCycles += slice;

			timers.Tick(*this, slice);
			ppu.Tick(*this, slice);
			sio.Tick(*this, slice);
			apu.Tick(*this, slice);

			budget -= slice;

			if (keypad.IrqRequested())
			{
				irq.Raise(INT_KEYPAD);
			}

			HleBios::Tick(*this);
		}
	}

	void GbaBus::Tick(int cpuCycles)
	{
		int budget = cpuCycles + waitCycles;
		waitCycles = 0;

		if (budget < 0)
		{
			budget = 0;
		}

		// The devices are advanced in slices so that a DMA that steals cycles cannot make the
		// loop run away: the stolen cycles are charged to the *next* slice (Dma::RunNow calls
		// AddWaitCycles), which keeps the accounting honest without a feedback loop.
		int guard = 0;

		while (budget > 0 && guard++ < 1000000)
		{
			int slice = (budget > 64) ? 64 : budget;

			totalCycles += slice;

			timers.Tick(*this, slice);
			ppu.Tick(*this, slice);
			sio.Tick(*this, slice);
			apu.Tick(*this, slice);

			budget -= slice;

			// The sound FIFOs are refilled once per slice instead of from inside the APU.
			dma.ServiceFifoRequests(*this);

			// The keypad's condition is level driven: while it holds, the interrupt stays up.
			if (keypad.IrqRequested())
			{
				irq.Raise(INT_KEYPAD);
			}

			// An IntrWait/VBlankIntrWait that was left behind is over as soon as its interrupt
			// shows up in IF.
			HleBios::Tick(*this);
		}
	}
}
