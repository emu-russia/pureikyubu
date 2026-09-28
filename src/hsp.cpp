/*

# The Hi-Speed Port (HSP)

The port `hsp.h` declares. Read that header first: it is where the port's place in the machine (the
ARAM expansion window, the 32-byte blocks, the processor interface's interrupt) is described.

This module is small on purpose. It does not know what a Game Boy Player is: it routes the ARAM
traffic that lands in its window to whatever device is plugged in, reports the expansion size that
device claims, and carries the interrupt. The device is `gbplayer.cpp` today.

## The window and the interrupt

The console reaches the window in exactly one way: an ARAM DMA whose ARAM address is at or past
HSP_WINDOW_BASE. The DMA engine in `dsparam.cpp` asks the port first and only falls back to its own
"no expansion module" behaviour (discard the write, answer zeros) when no device answers. That
fallback is not a placeholder - the console's own ARAM driver probes for a *plain SDRAM* module by
writing test blocks past the boundary and reading them back, and on a console with a Game Boy Player
those reads must not return what was written.

The interrupt is the processor interface's bit 13 (`PI_INTERRUPT_HSP`). The port raises it for the
device (`AssertId`) and the guest clears it by writing 1 to that bit of `INTSR`, which the processor
interface forwards here as `Acknowledge` (see `ProcessorInterface::PIRegWrite`).

*/

#include "pch.h"

using namespace Debug;

namespace Flipper
{
	void HiSpeedPort::Plug(HSPDevice* device)
	{
		if (this->device == device)
		{
			return;
		}

		Unplug();
		this->device = device;

		if (device != nullptr)
		{
			Report(Channel::HSP, "Hi-Speed Port: device attached (expansion code %i)\n",
				device->ExpansionSizeCode());
		}
	}

	void HiSpeedPort::Unplug()
	{
		if (device != nullptr)
		{
			Report(Channel::HSP, "Hi-Speed Port: device detached\n");
			device = nullptr;
		}

		Acknowledge();
	}

	int HiSpeedPort::ExpansionCode() const
	{
		return device != nullptr ? device->ExpansionSizeCode() : 0;
	}

	bool HiSpeedPort::Write(uint32_t aramAddr, const uint8_t* src, uint32_t len)
	{
		if (device == nullptr || !InWindow(aramAddr))
		{
			return false;
		}

		// An address inside the window is the device's own offset into it, and a transfer that runs
		// past the end of the window is the device's to refuse (the emulation must not let one run
		// into the host's buffer). The window is exactly HSP_WINDOW_SIZE long, so the check is the
		// same 64-bit one every other DMA window uses.
		uint32_t offset = aramAddr - HSP_WINDOW_BASE;

		if (!Verify::Range(offset, len, (uint64_t)HSP_WINDOW_SIZE))
		{
			Report(Channel::HSP, "Hi-Speed Port write out of the window: offset 0x%08X, %u bytes\n",
				offset, len);
			return true;    // the port answers; it is the block that is not the device's
		}

		device->Write(offset, src, len);
		return true;
	}

	bool HiSpeedPort::Read(uint32_t aramAddr, uint8_t* dst, uint32_t len)
	{
		if (device == nullptr || !InWindow(aramAddr))
		{
			return false;
		}

		uint32_t offset = aramAddr - HSP_WINDOW_BASE;

		if (!Verify::Range(offset, len, (uint64_t)HSP_WINDOW_SIZE))
		{
			Report(Channel::HSP, "Hi-Speed Port read out of the window: offset 0x%08X, %u bytes\n",
				offset, len);
			memset(dst, 0, len);
			return true;
		}

		device->Read(offset, dst, len);
		return true;
	}

	void HiSpeedPort::AssertId()
	{
		if (cause)
		{
			return;
		}

		cause = true;

		if (HW != nullptr && HW->pi != nullptr)
		{
			HW->pi->PIAssertInt(PI_INTERRUPT_HSP);
		}
	}

	void HiSpeedPort::Acknowledge()
	{
		if (!cause)
		{
			return;
		}

		cause = false;

		if (HW != nullptr && HW->pi != nullptr)
		{
			HW->pi->PIClearInt(PI_INTERRUPT_HSP);
		}
	}

	void HiSpeedPort::Reset()
	{
		Acknowledge();

		if (device != nullptr)
		{
			device->Reset();
		}
	}

	// ---------------------------------------------------------------------------
	// save states

	// What the port owns is the interrupt cause and nothing else: the device behind it carries its
	// own section (the Game Boy Player is a separate block of the state, see gbplayer.cpp), and the
	// window has no memory of its own. The cause is a level the device raises and the guest clears,
	// so a state taken with an interrupt pending has to bring the level back and, with it, the
	// processor interface bit - which `LoadState` does the same way every other block does (the PI
	// section is applied before this one, see savestate.cpp).
	void HiSpeedPort::SaveState(SaveStates::StateWriter& writer) const
	{
		writer.Fields(cause);
	}

	void HiSpeedPort::LoadState(SaveStates::StateReader& reader)
	{
		reader.Fields(cause);

		if (reader.Failed())
		{
			return;
		}

		if (cause)
		{
			if (HW != nullptr && HW->pi != nullptr)
			{
				HW->pi->PIAssertInt(PI_INTERRUPT_HSP);
			}
		}
		else if (HW != nullptr && HW->pi != nullptr)
		{
			HW->pi->PIClearInt(PI_INTERRUPT_HSP);
		}
	}
}
