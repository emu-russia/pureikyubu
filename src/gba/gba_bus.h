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
		void SetBios(const uint8_t* data, size_t size);

		/// <summary>True when the BIOS image is the built-in (custom) one.</summary>
		bool UsingCustomBios() const { return customBios; }
		void SetCustomBios(bool custom) { customBios = custom; }

		/// <summary>True when the BIOS calls are handled in the host instead of being executed
		/// by the emulated CPU. The custom boot ROM runs for real in either case: it does not
		/// call the BIOS service functions.</summary>
		bool HleBiosEnabled = true;

		// -- CPU interface -----------------------------------------------------------------

		uint8_t Read8(uint32_t address);
		uint16_t Read16(uint32_t address);
		uint32_t Read32(uint32_t address);

		void Write8(uint32_t address, uint8_t value);
		void Write16(uint32_t address, uint16_t value);
		void Write32(uint32_t address, uint32_t value);

		/// <summary>An instruction fetch (16-bit). Same as Read16, but it counts the cartridge
		/// prefetch/waitstate cycles of a code fetch.</summary>
		uint16_t Fetch16(uint32_t address);

		/// <summary>An instruction fetch of a 32-bit ARM instruction.</summary>
		uint32_t Fetch32(uint32_t address);

		/// <summary>The value the bus drove last (open bus).</summary>
		uint32_t OpenBus() const { return openBus; }

		// -- the clock ---------------------------------------------------------------------

		/// <summary>Add the waitstates of a memory access to the current slice.</summary>
		void AddWaitCycles(int cycles) { waitCycles += cycles; }

		/// <summary>
		/// True while the DMA engine is transferring: a transfer accounts for its own bus cycles
		/// (see Dma::Perform), so a 32 bit access it makes must not add the reconciliation cycle
		/// that a 32 bit *CPU* access on a 16 bit bus costs.
		/// </summary>
		bool dmaAccess = false;

		/// <summary>Take (and clear) the waitstates accumulated since the last call.</summary>
		int TakeWaitCycles();

		/// <summary>Total cycles the system has run since the reset.</summary>
		uint64_t TotalCycles() const { return totalCycles; }
		void SetTotalCycles(uint64_t cycles) { totalCycles = cycles; }

		/// <summary>Advance every device by the CPU's cycles plus the waitstates.</summary>
		void Tick(int cpuCycles);

		/// <summary>
		/// Advance the clock-driven devices by `cycles` without servicing DMA requests. A DMA
		/// transfer spends its own cycles while it runs (see Dma::Perform), and a nested request
		/// must not start another transfer from inside it.
		/// </summary>
		void TickDevices(int cycles);

		/// <summary>The cycle counter the DMA and the PPU use as their time base.</summary>
		uint64_t CycleCounter() const { return totalCycles; }

		/// <summary>
		/// The halfword at `address`, decoded the way Read16 decodes it but without any of its
		/// side effects: no waitstates are charged and the open-bus latch is left alone. This is
		/// the read a debugger uses - a disassembler walks the memory continuously, and the
		/// emulator's own reports use the same rule (see GbBus::Peek). Where the address decode
		/// needs a register value (the GPIO port), the raw register is returned.
		/// </summary>
		uint16_t Peek16(uint32_t address) const;

		// -- BIOS calls --------------------------------------------------------------------

		/// <summary>
		/// The HLE hook the CPU calls when it decodes a SWI. Returns true when the call was
		/// handled in the host, in which case the handler has already set up the CPU state
		/// (normally: the return address in the PC). Returns false when the SWI has to take the
		/// exception vector (a real BIOS image is installed and HLE is off).
		/// </summary>
		bool Swi(uint32_t comment);

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
		uint8_t PostFlg() const { return postFlg; }

		/// <summary>HALTCNT (0x04000301): 0x80 asks the CPU to halt.</summary>
		void WriteHaltCnt(uint8_t value);

	private:
		uint32_t openBus = 0;
		int waitCycles = 0;
		uint64_t totalCycles = 0;
		bool customBios = false;

		uint8_t postFlg = 0;
		int haltState = 0;

		// WAITCNT (0x04000204) and the derived SRAM/ROM waitstates.
		uint16_t waitcnt = 0;
		int sramWait = 0;

		// Where the last Game Pak access ended, and whether the bus has been reading the
		// cartridge without interruption (GBATEK 4000204h's first/second access timing).
		uint32_t romNext = 0;
		bool romChain = false;

		// Set while the CPU is fetching an instruction, which is what the Game Pak prefetch
		// buffer serves.
		bool fetching = false;
		uint32_t memControl = 0x0D000020;	// 4000800h: bits 24-27 are the 256K WRAM waits

		/// <summary>The 256K WRAM waitstate count (2 by default, 4000800h bits 24-27 = 15-n).</summary>
		int WramWaitStates() const;

		/// <summary>
		/// True for the mirrored 4000800h access: the register is four bytes wide and repeats in
		/// every 64K page of the I/O area (GBATEK 4000800h).
		/// </summary>
		bool IsMemControl(uint32_t address) const
		{
			if ((address >> 24) != 0x04)
				return false;
			uint32_t within = address & 0xFFFF;
			return within >= 0x800 && within <= 0x803;
		}

		// The registers the bus decodes itself (WAITCNT, IE/IF/IME, POSTFLG, HALTCNT).
		uint16_t ReadIo16(uint32_t offset);
		uint8_t ReadIo8(uint32_t offset);
		void WriteIo16(uint32_t offset, uint16_t value);
		void WriteIo8(uint32_t offset, uint8_t value);

		// The devices that need the byte lanes of a 16-bit register.
		uint16_t IoRead16(uint32_t offset);
		void IoWrite16(uint32_t offset, uint16_t value);

		void UpdateWaitStates();

		/// <summary>
		/// The extra cycles an access to the internal memories costs on top of the CPU's own N/S
		/// cycle, from GBATEK's "GBA Memory Map" table: the on-board 256K WRAM is a 16 bit bus
		/// with waitstates (3/3/6 cycles by default, set by the undocumented 4000800h register)
		/// while VRAM, OAM and Palette RAM are 1/1/2 - one cycle for 8 and 16 bit accesses and
		/// two for a 32 bit one. The BIOS, the 32K on-chip WRAM and the I/O area are 1/1/1 and
		/// need nothing.
		/// </summary>
		int InternalWaitCycles(uint32_t address, int bytes) const;

		/// <summary>True when this Game Pak access continues the previous one (the ROM's "second
		/// access", the sequential timing).</summary>
		bool RomSequential(uint32_t address) const;

		/// <summary>
		/// Charge a Game Pak access. GBATEK's WAITCNT table gives the *total* access time of the
		/// first (non-sequential) and the second (sequential) access, and the CPU (or the DMA)
		/// already counts one cycle for the access itself, so the waitstates added here are that
		/// figure less one - per 16 bit fragment, of which a 32 bit access has two (the second
		/// always sequential). A *code fetch* with the prefetch buffer running costs nothing.
		/// </summary>
		void ChargeRom(uint32_t address, int bytes, bool fetch);

		friend class Dma;
	};
}
