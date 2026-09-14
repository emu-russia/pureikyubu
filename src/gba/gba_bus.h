// The GBA address bus and the system clock.
//
// GbaBus is the hub every other module talks to: the CPU reaches memory through it, the devices
// raise their interrupts through it, and it advances all of them in step with the 16.78 MHz
// system clock.
//
// The regions and their mirrors (GBATEK "GBA Memory Map"):
//
//   0x00000000  BIOS        16 KByte, reads only
//   0x02000000  EWRAM       256 KByte, 16-bit bus
//   0x03000000  IWRAM       32 KByte, 32-bit bus
//   0x04000000  I/O         1 KByte, mirrored every 1 KByte up to 0x04001000
//   0x05000000  Palette     1 KByte
//   0x06000000  VRAM        96 KByte
//   0x07000000  OAM         1 KByte
//   0x08000000  ROM         16-bit bus, mirrored up to 0x0DFFFFFF
//   0x0E000000  SRAM/Flash  64 KByte window
//
// Reads of an unmapped address return the last value the bus drove ("open bus"), which some
// games rely on. The waitstates of the ROM/SRAM accesses are modelled through WAITCNT and are
// charged to the master clock, so the CPU's own cycle count stays the architecture's.

#pragma once

#include "gba_types.h"
#include "gba_irq.h"
#include "gba_keypad.h"
#include "arm7tdmi.h"
#include "gba_timers.h"
#include "gba_dma.h"
#include "gba_ppu.h"
#include "gba_apu.h"
#include "gba_sio.h"
#include "gba_cart.h"

namespace GBA
{
	class GbaBus
	{
	public:
		GbaBus();
		~GbaBus();

		// -- lifecycle ---------------------------------------------------------------------

		/// <summary>Reset every device and the memory (the BIOS and the cartridge stay).</summary>
		void Reset();

		/// <summary>Install a BIOS image (16 KByte; a shorter image is padded with 0xFF).</summary>
		void SetBios(const u8* data, size_t size);

		/// <summary>True when the BIOS image is the built-in (custom) one.</summary>
		bool UsingCustomBios() const { return customBios; }
		void SetCustomBios(bool custom) { customBios = custom; }

		/// <summary>True when the BIOS calls are handled in the host instead of being executed
		/// by the emulated CPU. The custom boot ROM runs for real in either case: it does not
		/// call the BIOS service functions.</summary>
		bool HleBiosEnabled = true;

		// -- CPU interface -----------------------------------------------------------------

		u8 Read8(u32 address);
		u16 Read16(u32 address);
		u32 Read32(u32 address);

		void Write8(u32 address, u8 value);
		void Write16(u32 address, u16 value);
		void Write32(u32 address, u32 value);

		/// <summary>An instruction fetch (16-bit). Same as Read16, but it counts the cartridge
		/// prefetch/waitstate cycles of a code fetch.</summary>
		u16 Fetch16(u32 address);

		/// <summary>An instruction fetch of a 32-bit ARM instruction.</summary>
		u32 Fetch32(u32 address);

		/// <summary>The value the bus drove last (open bus).</summary>
		u32 OpenBus() const { return openBus; }

		// -- the clock ---------------------------------------------------------------------

		/// <summary>Add the waitstates of a memory access to the current slice.</summary>
		void AddWaitCycles(int cycles) { waitCycles += cycles; }

		/// <summary>Take (and clear) the waitstates accumulated since the last call.</summary>
		int TakeWaitCycles();

		/// <summary>Total cycles the system has run since the reset.</summary>
		u64 TotalCycles() const { return totalCycles; }
		void SetTotalCycles(u64 cycles) { totalCycles = cycles; }

		/// <summary>Advance every device by the CPU's cycles plus the waitstates.</summary>
		void Tick(int cpuCycles);

		/// <summary>The cycle counter the DMA and the PPU use as their time base.</summary>
		u64 CycleCounter() const { return totalCycles; }

		// -- BIOS calls --------------------------------------------------------------------

		/// <summary>
		/// The HLE hook the CPU calls when it decodes a SWI. Returns true when the call was
		/// handled in the host, in which case the handler has already set up the CPU state
		/// (normally: the return address in the PC). Returns false when the SWI has to take the
		/// exception vector (a real BIOS image is installed and HLE is off).
		/// </summary>
		bool Swi(u32 comment);

		// -- devices -----------------------------------------------------------------------

		Arm7tdmi cpu;
		Ppu ppu;
		Apu apu;
		Sio sio;
		Dma dma;
		Timers timers;
		Irq irq;
		Keypad keypad;
		Cart cart;

		// The memory the bus owns.
		MemoryBank ewram;
		MemoryBank iwram;
		MemoryBank bios;
		MemoryBank io;				// the raw register file (the devices decode it)

		/// <summary>The EEPROM's DMA3 transfer, which the bus has to complete before the CPU
		/// continues (the games poll the DMA enable bit). The bus calls this from Tick.</summary>
		void ServiceEepromDma();

		/// <summary>POSTFLG (0x04000300), which the BIOS sets to 1 after the boot; some games check
		/// it to tell a reset from a cold start.</summary>
		u8 PostFlg() const { return postFlg; }

		/// <summary>HALTCNT (0x04000301): 0x80 asks the CPU to halt.</summary>
		void WriteHaltCnt(u8 value);

	private:
		u32 openBus = 0;
		int waitCycles = 0;
		u64 totalCycles = 0;
		bool customBios = false;

		u8 postFlg = 0;
		int haltState = 0;

		// WAITCNT (0x04000204) and the derived SRAM/ROM waitstates.
		u16 waitcnt = 0;
		int sramWait = 0;

		// The registers the bus decodes itself (WAITCNT, IE/IF/IME, POSTFLG, HALTCNT).
		u16 ReadIo16(u32 offset);
		u8 ReadIo8(u32 offset);
		void WriteIo16(u32 offset, u16 value);
		void WriteIo8(u32 offset, u8 value);

		// The devices that need the byte lanes of a 16-bit register.
		u16 IoRead16(u32 offset);
		void IoWrite16(u32 offset, u16 value);

		void UpdateWaitStates();

		friend class Dma;
	};
}
