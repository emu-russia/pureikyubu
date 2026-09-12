// Json Debug Interface (JDI) specifications.
//
// Every JDI node of the emulator is described by a JSON text. All of those texts are collected
// in one module (jdispecs.cpp), so that the whole debug interface can be reviewed and edited in
// one place, instead of being scattered over the components that implement the commands.

#pragma once

namespace JdiSpecs
{
	extern const char* EmuJdi;			// Emulator control commands (main.cpp)
	extern const char* DebuggerJdi;		// Debugger commands (debug.cpp)
	extern const char* DebugUiJdi;		// Debug UI commands (debugui.cpp)
	extern const char* GekkoCoreJdi;	// Gekko (CPU) debug commands (gekkodebug.cpp)
	extern const char* DspJdi;			// DSP debug commands (dsp.cpp)
	extern const char* HwJdi;			// Flipper HW debug commands (flipperdebug.cpp)
	extern const char* DduJdi;			// DVD Drive Unit commands (dvddebug.cpp)
	extern const char* GfxJdi;			// GFX (GX) debug commands (gfx.cpp)
	extern const char* HleJdi;			// High-level (HLE) commands (os.cpp)
	extern const char* UiJdi;			// User interface commands (uijdi.cpp)
}
