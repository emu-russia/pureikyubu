# DebugConsole

The entire debug console UI has moved here.

This code is very oriented on the Win32 Console API, so it was natural to take it outside.

All debug instances work non-invasively, in their own thread, so they can be used at any time without disrupting the normal flow of system emulation.

In turn, the emulated system does not react in any way to the work (or not work) of the debugger, it simply does not know that it is being debugged.

As usual, let me remind you that the master driving force of the entire emulator is the GekkoCore thread.
When the thread is stopped (Gekko TBR does not increase its value), all other emulator systems (including DspCore) are also stopped. 
This does not apply to the debugger (and any other UI), which are architecturally located above the emulator core and work in their own threads.

## Architectural features

Debug UI contains 2 instances:
- System-wide debugger focused on GekkoCore (code rewritten on Cui.cpp)
- DSP debugger (the code is based on the more convenient Cui.cpp from ground up)

The Win32 Console API does not allow you to create more than one console per process, so you can only use one instance at a time.

Debug UIs can be opened via the `Debug` menu.

## Cui

All Win32 code for interacting with the Console API is integrated into Cui.cpp.
I'm not sure that someone would want to port the console debugger somewhere other than Windows (the Console API is very specific), but for convenience I brought all the code there.

All other modules are based on Cui, as custom CuiWindows.

## Interacting with emulator components

Communication with the Gekko and DSP cores is done through the Debug JDI Client.

You can watch registers, memory, manage breakpoints and all that stuff.

Gekko and DSP disassemblers are in the emulator core, in the corresponding components, JDI returns already disassembled text.
