/*

# HW Debug

The Flipper subsystems, rendered as Markdown for the new debugger (debugui2) and for the JDI
command line.

Every block of the Flipper ASIC keeps its state in a `XxxState` structure next to the block itself
(`ai.h`, `vi.h`, `pi.h`, `mem.h`, `di.h`, `si.h`, `exi.h`, `cp.h`). This module turns those
structures into a report: one function per block, and one JDI command per function
(`airegs`, `viregs`, `piregs`, `miregs`, `diregs`, `siregs`, `exiregs`, `cpregs`).

A report is Markdown, which is the format the new debugger shows a command answer in, so the very
same text is what the debugger puts into the "Audio", "Video", ... panels and what a user of the
command line sees. The reports are the raw register values plus the decoded fields that matter:
nobody wants to unpack the VI display configuration register by hand in the middle of a session.

The panels of a GameCube session are built from these reports (see `debugui2.h`, "The view"), one
tab per block, next to the Gekko registers, the disassembly, the memory and the profiler.

The blocks are read from the debugger thread while the emulation thread is running - the same
arrangement the rest of the debug interface uses (the registers are what the guest last wrote, and
a snapshot taken while the guest is between two writes is exactly what one expects to see).

*/

#pragma once

#include <string>

namespace Flipper
{
	// Render the state of the hardware block as Markdown. Each one answers nullptr when there is
	// nothing to report (no Flipper, i.e. no image is running), which is what the JDI handlers
	// turn into "nothing is running" and what the debugger turns into an empty panel.

	std::string AIReport();			// `airegs` - the audio interface (AI streaming)
	std::string VIReport();			// `viregs` - the video interface (the raster and the XFB)
	std::string PIReport();			// `piregs` - the processor interface (interrupts, the CP FIFO)
	std::string MIReport();			// `miregs` - the memory interface (1T-SRAM, the counters)
	std::string DIReport();			// `diregs` - the disk interface (the drive registers)
	std::string SIReport();			// `siregs` - the serial interface (the four controller ports)
	std::string EXIReport();		// `exiregs` - the external interface (the three channels)
	std::string CPReport();			// `cpregs` - the command processor (the parser and the ring)
	std::string DSPReport();		// `dspstate` - the DSP core and its Flipper interface

	// Register the commands above with the debug interface (the HW JDI node).
	void hwdebug_init_handlers();
}
