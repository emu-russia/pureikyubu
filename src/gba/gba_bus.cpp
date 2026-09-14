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

namespace GBA
{
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
	static bool IsGpioAddress(u32 address)
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

	void GbaBus::SetBios(const u8* data, size_t size)
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

	int GbaBus::TakeWaitCycles()
	{
		int cycles = waitCycles;
		waitCycles = 0;
		return cycles;
	}

	// ---------------------------------------------------------------------------------------
	// The region decode
	// ---------------------------------------------------------------------------------------

	u8 GbaBus::Read8(u32 address)
	{
		u32 region = address >> 24;

		switch (region)
		{
			case 0x00:
				// The BIOS is 16 KByte; the rest of the first 32 MByte is unused.
				if (address < BiosSize)
				{
					openBus = (openBus & 0xFFFFFF00u) | bios.Read8(address);
					return (u8)openBus;
				}
				return (u8)(openBus >> ((address & 3) * 8));

			case 0x02:
				openBus = (openBus & 0xFFFFFF00u) | ewram.Read8(address - MemEwram);
				return (u8)openBus;

			case 0x03:
				openBus = (openBus & 0xFFFFFF00u) | iwram.Read8(address - MemIwram);
				return (u8)openBus;

			case 0x04:
				return ReadIo8(address & (IoSize - 1));

			case 0x05:
				openBus = (openBus & 0xFFFFFF00u) | ppu.ReadPalette(address & (PaletteSize - 1));
				return (u8)openBus;

			case 0x06:
				// VRAM is mirrored by the PPU itself (with a modulo - 96 KByte is not a power of
				// two, so masking the address here with `VramSize - 1` would drop bit 15 and fold
				// the second 32 KByte onto the first, which is exactly what a mode 3 bitmap must
				// not do).
				openBus = (openBus & 0xFFFFFF00u) | ppu.ReadVram(address);
				return (u8)openBus;

			case 0x07:
				openBus = (openBus & 0xFFFFFF00u) | ppu.ReadOam(address & (OamSize - 1));
				return (u8)openBus;

			case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
			{
				AddWaitCycles(cart.WaitStates(address, true, waitcnt));

				u8 value = IsGpioAddress(address) ? cart.ReadGpio(address) : cart.ReadRom8(address);
				openBus = (openBus & 0xFFFFFF00u) | value;
				return value;
			}

			case 0x0E: case 0x0F:
				AddWaitCycles(sramWait);
				openBus = (openBus & 0xFFFFFF00u) | cart.ReadSave(address - MemSram);
				return (u8)openBus;

			default:
				// Unmapped: the last value the bus drove, as the hardware leaves floating.
				return (u8)(openBus >> ((address & 3) * 8));
		}
	}

	u16 GbaBus::Read16(u32 address)
	{
		address &= ~1u;

		u32 region = address >> 24;

		switch (region)
		{
			case 0x00:
				if (address < BiosSize)
				{
					u16 value = bios.Read16(address);
					openBus = value;
					return value;
				}
				return (u16)openBus;

			case 0x02:
			{
				u16 value = ewram.Read16(address - MemEwram);
				openBus = value;
				return value;
			}

			case 0x03:
			{
				u16 value = iwram.Read16(address - MemIwram);
				openBus = value;
				return value;
			}

			case 0x04:
				return ReadIo16(address & (IoSize - 1));

			case 0x05:
			{
				// A whole palette entry, not two byte reads: a byte read of the palette has the
				// hardware's OR rule (see Ppu::ReadPalette16).
				u16 value = ppu.ReadPalette16(address & (PaletteSize - 1));
				openBus = value;
				return value;
			}

			case 0x06:
			{
				u16 value = (u16)(ppu.ReadVram(address)
					| (ppu.ReadVram(address + 1) << 8));
				openBus = value;
				return value;
			}

			case 0x07:
			{
				u16 value = (u16)(ppu.ReadOam(address & (OamSize - 1))
					| (ppu.ReadOam((address + 1) & (OamSize - 1)) << 8));
				openBus = value;
				return value;
			}

			case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
			{
				AddWaitCycles(cart.WaitStates(address, true, waitcnt));

				u16 value;

				if (IsGpioAddress(address))
				{
					// The GPIO registers are byte wide (the RTC is bit-banged a byte at a time), so
					// only the byte at the register's own address belongs to the port; the rest of
					// the halfword is the cartridge's ROM. That is what makes a *code fetch* from
					// the entry area (0x080000C0 and up, which real cartridges use) work: with the
					// port switched off, Cart::ReadGpio returns the ROM byte, so the halfword is
					// exactly the ROM's.
					value = (u16)(cart.ReadGpio(address) | (cart.ReadRom8(address + 1) << 8));
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
				u16 value = (u16)(cart.ReadSave(address - MemSram)
					| (cart.ReadSave(address + 1 - MemSram) << 8));
				openBus = value;
				return value;
			}

			default:
				return (u16)openBus;
		}
	}

	u32 GbaBus::Read32(u32 address)
	{
		address &= ~3u;

		u32 region = address >> 24;

		switch (region)
		{
			case 0x00:
				if (address < BiosSize)
				{
					u32 value = bios.Read32(address);
					openBus = value;
					return value;
				}
				return openBus;

			case 0x02:
			{
				u32 value = ewram.Read32(address - MemEwram);
				openBus = value;
				return value;
			}

			case 0x03:
			{
				u32 value = iwram.Read32(address - MemIwram);
				openBus = value;
				return value;
			}

			case 0x04:
				return (u32)ReadIo16(address & (IoSize - 1))
					| ((u32)ReadIo16((address + 2) & (IoSize - 1)) << 16);

			case 0x05:
			{
				u32 value = (u32)Read16(address) | ((u32)Read16(address + 2) << 16);
				return value;
			}

			case 0x06:
			{
				u32 value = (u32)Read16(address) | ((u32)Read16(address + 2) << 16);
				return value;
			}

			case 0x07:
			{
				u32 value = (u32)Read16(address) | ((u32)Read16(address + 2) << 16);
				return value;
			}

			case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
			{
				AddWaitCycles(cart.WaitStates(address, true, waitcnt) * 2);

				u32 value;

				if (IsGpioAddress(address))
				{
					// See Read16: only the register's own byte is the port, the rest is ROM.
					value = (u32)cart.ReadGpio(address)
						| ((u32)cart.ReadRom8(address + 1) << 8)
						| ((u32)cart.ReadRom8(address + 2) << 16)
						| ((u32)cart.ReadRom8(address + 3) << 24);
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
				u32 value = (u32)cart.ReadSave(address - MemSram)
					| ((u32)cart.ReadSave(address + 1 - MemSram) << 8)
					| ((u32)cart.ReadSave(address + 2 - MemSram) << 16)
					| ((u32)cart.ReadSave(address + 3 - MemSram) << 24);
				openBus = value;
				return value;
			}

			default:
				return openBus;
		}
	}

	void GbaBus::Write8(u32 address, u8 value)
	{
		u32 region = address >> 24;

		switch (region)
		{
			case 0x00:
				// The BIOS is a mask ROM: writes do nothing.
				break;

			case 0x02:
				ewram.Write8(address - MemEwram, value);
				break;

			case 0x03:
				iwram.Write8(address - MemIwram, value);
				break;

			case 0x04:
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
				u32 offset = address % VramSize;

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
				AddWaitCycles(cart.WaitStates(address, true, waitcnt));

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

	void GbaBus::Write16(u32 address, u16 value)
	{
		address &= ~1u;

		u32 region = address >> 24;

		switch (region)
		{
			case 0x00:
				break;

			case 0x02:
				ewram.Write16(address - MemEwram, value);
				break;

			case 0x03:
				iwram.Write16(address - MemIwram, value);
				break;

			case 0x04:
				WriteIo16(address & (IoSize - 1), value);
				break;

			case 0x05:
				ppu.WritePalette16(address & (PaletteSize - 1), value);
				break;

			case 0x06:
				// The mirroring is the PPU's business (see Read16/Read8 above).
				ppu.WriteVram(address, (u8)value);
				ppu.WriteVram(address + 1, (u8)(value >> 8));
				break;

			case 0x07:
				ppu.WriteOam16(address, value);
				break;

			case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
				AddWaitCycles(cart.WaitStates(address, true, waitcnt));

				if (IsGpioAddress(address))
				{
					cart.WriteGpio(address, (u8)value);
					cart.WriteGpio(address + 1, (u8)(value >> 8));
				}
				else
				{
					cart.WriteRom16(*this, address, value);
				}
				break;

			case 0x0E: case 0x0F:
				AddWaitCycles(sramWait);
				cart.WriteSave(address - MemSram, (u8)value);
				cart.WriteSave(address + 1 - MemSram, (u8)(value >> 8));
				break;

			default:
				break;
		}
	}

	void GbaBus::Write32(u32 address, u32 value)
	{
		// A 32-bit write is two 16-bit writes, which is exactly what the 16-bit bus does; the
		// order matters for the peripherals that auto-increment (the FIFOs), so the halves are
		// written low first, as the hardware does.
		Write16(address, (u16)value);
		Write16(address + 2, (u16)(value >> 16));
	}

	// ---------------------------------------------------------------------------------------
	// Code fetches
	// ---------------------------------------------------------------------------------------

	u16 GbaBus::Fetch16(u32 address)
	{
		u16 value = Read16(address);

		// A code fetch from the cartridge also runs the prefetch buffer. The buffer is modelled
		// as "the second access is free when it is enabled" in Cart::WaitStates, so nothing else
		// is charged here.
		openBus = value;
		return value;
	}

	u32 GbaBus::Fetch32(u32 address)
	{
		// An ARM instruction fetch is two 16-bit fetches on the cartridge bus.
		u32 low = Read16(address);
		u32 high = Read16(address + 2);
		openBus = low | (high << 16);
		return low | (high << 16);
	}

	// ---------------------------------------------------------------------------------------
	// The I/O register file
	// ---------------------------------------------------------------------------------------

	u16 GbaBus::ReadIo16(u32 offset)
	{
		offset &= 0x3FE;

		u16 open16 = (u16)openBus;

		if (offset <= 0x05E)
		{
			u16 value = ppu.Read16(offset, open16);
			openBus = value;
			return value;
		}

		if (offset >= 0x060 && offset <= 0x0A6)
		{
			u16 value = (u16)(apu.Read8(offset, (u8)open16) | (apu.Read8(offset + 1, (u8)(open16 >> 8)) << 8));
			openBus = value;
			return value;
		}

		if (offset >= 0x0B0 && offset <= 0x0DE)
		{
			u16 value = dma.Read16(offset, open16);
			openBus = value;
			return value;
		}

		if (offset >= 0x100 && offset <= 0x10E)
		{
			u16 value = timers.Read16(offset);
			openBus = value;
			return value;
		}

		if (offset == 0x130)
		{
			// KEYINPUT: a byte access mirrors the low byte in both lanes (GBATEK 4000130h).
			u16 value = keypad.ReadKeyInput();
			openBus = value;
			return value;
		}

		if (offset == 0x132)
		{
			u16 value = keypad.ReadKeyCnt();
			openBus = value;
			return value;
		}

		if (offset >= 0x120 && offset <= 0x15A)
		{
			u16 value = sio.Read16(*this, offset, open16);
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

	u8 GbaBus::ReadIo8(u32 offset)
	{
		offset &= (IoSize - 1);

		// The byte lanes of a 16-bit register: the byte the CPU addressed, unless the hardware
		// mirrors the low byte in both lanes.
		u8 open8 = (u8)openBus;

		if (offset <= 0x05F)
		{
			u16 value = ppu.Read16(offset & ~1u, (u16)openBus);

			// GREENSWP (0x04000004) mirrors its low byte into both lanes.
			if ((offset & ~1u) == 0x004)
			{
				return (u8)value;
			}

			return (u8)(value >> ((offset & 1) * 8));
		}

		if (offset >= 0x060 && offset <= 0x0A7)
		{
			return apu.Read8(offset, open8);
		}

		if (offset >= 0x0B0 && offset <= 0x0DF)
		{
			u16 value = dma.Read16(offset & ~1u, (u16)openBus);
			return (u8)(value >> ((offset & 1) * 8));
		}

		if (offset >= 0x100 && offset <= 0x10F)
		{
			u16 value = timers.Read16(offset & ~1u);
			return (u8)(value >> ((offset & 1) * 8));
		}

		if (offset == 0x130 || offset == 0x131)
		{
			// A byte read of KEYINPUT returns the low byte in both lanes.
			return (u8)keypad.ReadKeyInput();
		}

		if (offset == 0x132 || offset == 0x133)
		{
			return (u8)(keypad.ReadKeyCnt() >> ((offset & 1) * 8));
		}

		if (offset >= 0x120 && offset <= 0x15B)
		{
			return sio.Read8(*this, offset, open8);
		}

		switch (offset)
		{
			case 0x200: case 0x201: return (u8)(irq.ReadIE() >> ((offset & 1) * 8));
			case 0x202: case 0x203: return (u8)(irq.ReadIF() >> ((offset & 1) * 8));
			case 0x204: case 0x205: return (u8)(waitcnt >> ((offset & 1) * 8));
			case 0x208: case 0x209: return irq.ReadIME() ? 1 : 0;
			case 0x300: return postFlg;
			case 0x301: return (u8)haltState;
			default: break;
		}

		return open8;
	}

	void GbaBus::WriteIo16(u32 offset, u16 value)
	{
		offset &= 0x3FE;

		if (offset <= 0x05E)
		{
			ppu.Write16(*this, offset, value, (int)totalCycles);
			return;
		}

		if (offset >= 0x060 && offset <= 0x0A6)
		{
			apu.Write8(offset, (u8)value);
			apu.Write8(offset + 1, (u8)(value >> 8));
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
				waitcnt = value;
				UpdateWaitStates();
				break;

			case 0x208:
				irq.WriteIME((value & 1) != 0);
				break;

			case 0x300:
				// POSTFLG: bit 0 is writable, bit 1 is the "the BIOS has run" flag the hardware
				// sets and the games only read.
				postFlg = (u8)(value & 1) | 0x02;
				break;

			case 0x301:
				WriteHaltCnt((u8)value);
				break;

			default:
				break;
		}
	}

	void GbaBus::WriteIo8(u32 offset, u8 value)
	{
		offset &= (IoSize - 1);

		// A byte write to a 16-bit register keeps the other lane, which is what the hardware's
		// byte lanes do.
		auto writeLane = [&]()
		{
			u32 aligned = offset & ~1u;
			u16 old = ReadIo16(aligned);
			u16 merged = (offset & 1) ? (u16)((old & 0x00FF) | (value << 8)) : (u16)((old & 0xFF00) | value);
			WriteIo16(aligned, merged);
		};

		if (offset <= 0x05F)
		{
			if ((offset & ~1u) == 0x004)
			{
				// GREENSWP's low byte is mirrored in both lanes.
				WriteIo16(0x004, (u16)(value | (value << 8)));
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
				postFlg = (u8)(value & 1) | 0x02;
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

	void GbaBus::WriteHaltCnt(u8 value)
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

	bool GbaBus::Swi(u32 comment)
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
