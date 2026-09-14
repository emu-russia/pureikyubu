// The emulator's own boot ROMs.
//
// The real IPL images (the GBA 16 KByte BIOS, the DMG/CGB 256 byte boot ROMs) are copyrighted
// and are not part of this repository. This module builds free replacements from source with the
// emitter in gba_armasm.h:
//
//   * the GBA boot ROM draws the pureikyubu logo animation - a rotating wireframe hypercube that
//     collapses into the flat cube mark with the "pureikyubu" wordmark scrolling in underneath -
//     waits for the animation to finish, checks that a cartridge is present and jumps to it at
//     0x08000000. With no cartridge it drops into the SIO link driver, which is what the
//     "GBA Link" mode of the emulator needs: the port is initialized (multiplayer mode, IRQ on
//     transfer complete), the driver publishes the emulator's presence and then serves the
//     multiboot/link protocol, so a second instance can talk to it.
//
// The image can be dumped for review and is what the tests execute (`--dump-bootrom` in the
// harness, `BootRom_DumpListing` in the unit tests).

#pragma once

#include "gba_types.h"

namespace GBA
{
	namespace BootRom
	{
		/// <summary>The 16 KByte GBA boot ROM, assembled on first use.</summary>
		const std::vector<u8>& GbaImage();

		/// <summary>The assembly listing of the GBA boot ROM (the mnemonics the emitter recorded).</summary>
		std::string GbaListing();

		/// <summary>
		/// Number of frames the boot animation runs for before the cartridge is started. Tests
		/// and the harness use it to know how long to run the ROM for.
		/// </summary>
		int GbaAnimationFrames();

		/// <summary>
		/// The entry point of the SIO link driver inside the boot ROM (the address the emulator
		/// reports when it runs without a cartridge).
		/// </summary>
		u32 GbaLinkDriverEntry();
	}
}
