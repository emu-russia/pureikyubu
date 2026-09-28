/*

# The Hi-Speed Port (HSP)

The Hi-Speed Port is the console's third external socket (the two serial ports on EXI are the other
two), and it is **not an EXI channel**: the port hangs off the ARAM (SDRAM) side of the audio/DSP
block, and it is served by the memory controller rather than by the IO register file. On a retail
console the only device for it is the Game Boy Player, which is why the port is also called the
"ARAM expansion".

The port is a **window**, and it begins exactly where the internal ARAM ends: the ARAM controller
addresses the internal 16 MB array and an expansion chip-select group, and the address that leaves
the internal array is the first address of the window. A CPU-side ARAM DMA whose ARAM address is at
or past that boundary is answered by the device on the port instead of by the internal array (see
`ARAMDmaRun` in dsparam.cpp). There is no CPU-visible register block for the port anywhere in the
`0x0C00_0000` IO window - a trace of a retail Game Boy Player disc reaches the ARAM DMA engine and
the processor interface's HSP interrupt bit (13) and nothing else.

The window is not memory. The console's own ARAM driver probes for a plain SDRAM expansion module by
moving test blocks at and past the boundary, and on a console with a Game Boy Player those blocks do
not read back (the module is not RAM), so the probe correctly reports "no module". The Player's own
driver then claims the window by writing the expansion size into `AMCR` and identifies the device
with a **loopback handshake**: it writes a 32-byte block of a constant byte and requires the byte
back to be its complement. Only then does it upload the program the Player runs and start
exchanging the 32-byte command blocks the device answers.

This module is the port itself: it owns the window's address range, routes the ARAM traffic that
lands in it to the device, reports the expansion size the device claims, and carries the processor
interface's HSP interrupt. What the device behind it *is* belongs to the device (see gbplayer.h for
the Game Boy Player).

*/

#pragma once

// The save state cursors, forward declared (see savestate.h).
namespace SaveStates
{
	class StateWriter;
	class StateReader;
}

namespace Flipper
{
	//! Where the port's window starts in ARAM space: the internal array is 16 MB and the expansion
	//! group begins exactly where it ends.
	#define HSP_WINDOW_BASE     (16 * 1024 * 1024)

	//! How large the window is: the expansion chip select decodes 16 MB, which is also the size the
	//! Game Boy Player's driver settles on (`AMCR` expansion code 3).
	#define HSP_WINDOW_SIZE     (16 * 1024 * 1024)

	//! The unit the console and the port exchange: every block of the window's command/status and
	//! handshake protocol is 32 bytes.
	#define HSP_BLOCK_SIZE      32

	// ---------------------------------------------------------------------------
	// The device on the port

	//! What the port asks of the device plugged into it. The device never touches the window's
	//! address arithmetic or the interrupt: it is handed the blocks the console moves and it raises
	//! the port interrupt through the port itself.
	class HSPDevice
	{
	public:
		virtual ~HSPDevice() {}

		//! The expansion size the device claims, in the `AMCR` encoding (0 = none, 1 = 4 MB,
		//! 2 = 8 MB, 3 = 16 MB, 4 = 32 MB). The port reports the largest of this and what the
		//! console wrote.
		virtual int ExpansionSizeCode() { return 0; }

		//! The console reset the port.
		virtual void Reset() {}

		//! A block the console wrote into the window at `offset` (0..HSP_WINDOW_SIZE). The write
		//! is a command: there is no answer of its own.
		virtual void Write(uint32_t offset, const uint8_t* data, uint32_t len) {}

		//! A block the console read out of the window at `offset`. A read is answered from the
		//! device's own state, not from what was written last - that is what makes the window
		//! useless as an SDRAM module and what the identification handshake relies on.
		virtual void Read(uint32_t offset, uint8_t* data, uint32_t len)
		{
			memset(data, 0, len);
		}

		//! The emulated time advanced by `ticks` (the console's time base). A device that runs a
		//! machine of its own - the Player runs a Game Boy Advance - steps it here, which is what
		//! makes the picture it hands the console advance with the console's own clock.
		virtual void Tick(int64_t ticks) {}
	};

	// ---------------------------------------------------------------------------
	// The port

	class HiSpeedPort
	{
		//! The device plugged into the port (nullptr: nothing is there, and the window answers
		//! "no device": writes are discarded and reads return zero).
		HSPDevice* device = nullptr;

		//! The cause of the port's own interrupt line. It is a level: the device raises it and the
		//! guest's acknowledge (a write of 1 to the HSP bit of `INTSR`) lowers it.
		bool cause = false;

	public:
		HiSpeedPort() {}
		~HiSpeedPort() {}

		//! Plug a device in (the previous one, if any, is unplugged first).
		void Plug(HSPDevice* device);

		//! Unplug the device.
		void Unplug();

		//! The device on the port, or nullptr.
		HSPDevice* Device() const { return device; }

		//! Whether the port holds a device.
		bool Attached() const { return device != nullptr; }

		//! The expansion size code the port reports for `AMCR` (0 with nothing plugged in).
		int ExpansionCode() const;

		//! Whether the address is inside the port's window.
		static bool InWindow(uint32_t aramAddr) { return aramAddr >= HSP_WINDOW_BASE; }

		//! A block of `len` bytes moved from main memory into the window (the device's command
		//! path). Answers false when there is no device, so that the caller can keep its "no
		//! expansion module" behaviour.
		bool Write(uint32_t aramAddr, const uint8_t* src, uint32_t len);

		//! A block of `len` bytes moved out of the window into main memory (the device's answer
		//! path). Answers false when there is no device.
		bool Read(uint32_t aramAddr, uint8_t* dst, uint32_t len);

		//! Raise the port's interrupt (the device's ID event). The processor interface's HSP bit
		//! (bit 13) is asserted through `PIAssertInt`.
		void AssertId();

		//! The guest acknowledged the port's interrupt (it wrote 1 to the HSP bit of `INTSR`).
		void Acknowledge();

		//! Whether the port is asking for the interrupt.
		bool Cause() const { return cause; }

		//! The console reset the port: the device is reset, the interrupt is dropped.
		void Reset();

		//! The emulated time advanced (the period work of the Flipper, see `Flipper::Update`): the
		//! device on the port is stepped.
		void Tick(int64_t ticks)
		{
			if (device != nullptr)
			{
				device->Tick(ticks);
			}
		}

		/// <summary>Write the port's own state into the save state section the caller opened.</summary>
		void SaveState(SaveStates::StateWriter& writer) const;

		/// <summary>Put the port's own state back from the section the caller opened.</summary>
		void LoadState(SaveStates::StateReader& reader);
	};
}
