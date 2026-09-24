/*

# GBA / Game Boy debug

The debug interface of the two portable machines (`src/gba`), for the new debugger (debugui2).

The module of the portable machines is deliberately self contained: it includes nothing outside
`src/gba/` and the C++ standard library, so that its core can be built and tested without SDL,
OpenGL or the GameCube side of the emulator (see `src/gba/Readme.md`). The debug interface is the
one thing that cannot live inside it, because JDI, Markdown and the debugger belong to the host.
So the bridge sits here, next to the SDL frontend of the same machines, and the core stays as
clean as it was.

## The commands

Every report is Markdown, which is the format the new debugger shows a command answer in, so the
same text is what the "GBA" and "Game Boy" panels of the debugger display and what a user of the
JDI command line sees. The three save state commands work on exactly the files the frontend's
quick save keys do, so a state written at the keyboard can be listed, loaded and moved from a JDI
session:

| Command | Subject |
|---|---|
| `gba` | the machine: the cartridge, the clock, the interrupts, the frame |
| `gbaregs` | the ARM7TDMI register file and the CPU mode |
| `gbacpu [count]` | the ARM / Thumb disassembly at the program counter |
| `gbamem <address> [lines]` | any part of the address space, as a hexdump |
| `gbappu` | the LCD controller: DISPCNT / DISPSTAT / VCOUNT, the layers, the frame |
| `gbadma` | the four DMA channels |
| `gbtimers` | the four timers and the interrupt controller |
| `gbsio` | the link port and its state |
| `gbcart` | the cartridge: the header and the save memory |
| `gbasavestate [slot]` | write a save state into a slot (`0`..`9`, `.st<slot>` next to the cartridge's `.sav`) |
| `gbaloadstate [slot]` | put the machine back into the state of a slot |
| `gbastates` | the slots that hold a save state, with their files and sizes |
| `gb` | the Game Boy machine: the console kind, the cartridge, the interrupts |
| `gbregs` | the LR35902 register file |
| `gbcpu [count]` | the SM83 disassembly at the program counter |
| `gbmem <address> [lines]` | the Game Boy address space, as a hexdump |
| `gbppu` | the Game Boy LCD: LCDC / STAT / LY, the palettes, the frame |
| `gbsavestate [slot]` | write a Game Boy save state into a slot (the CGB's palettes, banks and speed are in it) |
| `gbloadstate [slot]` | put the Game Boy back into the state of a slot |
| `gbstates` | the Game Boy slots that hold a save state |

A report answers with the `markdown` object the debugger renders, exactly like the Flipper reports
of `hwdebug.cpp`. With no machine running the command says so instead.

## What the debugger shows

The debugger asks for a report every refresh tick and puts it into the panel of the same name,
so the panels are live views of the machine. The disassembly panel is the exception: like the
Gekko one it is built from the instruction at the program counter and the ones after it, one JDI
call per instruction (`gbacpu`, `gbcpu` take the count and do the walking themselves).

## How the frontend drives it

`DebugStart` / `DebugStop` / `DebugActive` / `DebugPumpEvents` / `DebugFrame` are the whole of the
debugger the SDL frontend of the portable machines sees. They exist because that frontend cannot
include the debugger's own headers: those belong to the GameCube side and need the emulator's
precompiled header, which this module does not pull in (see `src/gba/Readme.md`). The front end
sets the machine it is running with `SetDebugMachine`, starts a session, and drives the window from
its own event loop - `F2` starts and stops it, as it does in the GameCube front end. Whether the
window is there when the machine starts is the `emulation.debugger` member of GBASettings: the
frontend passes it to `DebugStart` (the JDI node and the MCP transport come up either way).

`--mcp` is started from here too: the local MCP server is normally started by the GameCube UI, and
a portable session is a front end of its own, so without that an MCP client that launched
`pureikyubu --gba` would see the GameCube commands and none of the portable ones.

*/

#pragma once

#include <string>

namespace GBA
{
	class GbaSystem;
	class GbSystem;

	// The machine the debugger is looking at. The SDL frontend sets it for the time a machine is
	// running; the commands and the debugger's panels read it. Only one of the two is ever set.
	void SetDebugMachine(GbaSystem* machine);
	void SetDebugMachine(GbSystem* machine);

	// The kind of machine behind the debug interface.
	enum class DebugMachine
	{
		None = 0,		// nothing is running: the reports say so
		Gba,			// the Game Boy Advance
		Gb,				// the Game Boy / Game Boy Color
	};

	DebugMachine CurrentDebugMachine();

	// The machine the user is running, or nullptr when it is not the portable emulator.
	GbaSystem* CurrentGba();
	GbSystem* CurrentGb();

	// The title of the cartridge the machine is running, without pulling the machine's own
	// headers into a caller that only wants a name (the debugger names its session after it).
	// An empty string means the machine has no cartridge.
	std::string DebugMachineRomTitle();

	// True when that machine is set, i.e. the portable emulator is the one the user is running.
	bool DebugMachineActive();

	// The commands and the JDI node of the module. The node is registered by the SDL frontend,
	// because the GameCube build must not publish the portable commands at all.
	void DebugReflector();

	// -- the frontend's half of the new debugger ------------------------------------------
	//
	// The SDL frontend of the portable machines drives the debugger window, but it cannot include
	// the debugger's headers: those belong to the GameCube side and need the emulator's own
	// precompiled header, which this module deliberately does not pull in. These four calls are
	// the whole of the debugger the frontend sees.
	//
	// The session belongs to the machine that is running: `DebugStart` names it and builds its
	// panels from the machine the frontend set with SetDebugMachine, and `DebugStop` closes it
	// (the collected log goes into the session folder). Both are safe to call in any state.
	//
	// `showWindow` is false when the frontend wants the debug interface up (the JDI node and the
	// MCP transport) without the debugger window: the `emulation.debugger` member of GBASettings
	// decides that at startup, while `F2` calls DebugStart() and opens the window.

	void DebugStart(bool showWindow = true);
	void DebugStop();
	bool DebugActive();

	// One frame of the frontend's event loop: the debugger window pumps and handles the SDL
	// events that belong to it and puts the rest back into the queue for the caller's own poll
	// loop, then renders. A no-op when no debugger window is open.
	void DebugPumpEvents();
	void DebugFrame();
}
