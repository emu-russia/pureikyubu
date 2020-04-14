# HLE

This component is used to emulate hardware or some Dolphin SDK calls, using high-level emulation techniques.

Generally speaking, an emulator is already emulating hardware rather high-level, but in the emulation world, 
HLE usually means emulating even higher-level things, such as OS calls or data exchange protocol, like command packets, 
when it is not possible to emulate a device that processes packets by lower level emulation.

Also, sometimes it is required to ignore some OS calls in order to skip those places that cannot be emulated at a low level.

## Dolwin HLE Components

- HLE Traps
- Bootrom and Apploader emulation (Apploader is a special application on the game disc that is used to load the main application)
- Symbolic information support (Map files and Map Maker)
- OSTimeFormat: Convert GC time to human-usable time string
- Dumping Dolphin OS threads

## HLE Traps

HLE Trap is an artificial Gekko processor instruction that transfers control to an emulator ("virtual machine"). 
Thus, instead of calling some function of Dolphin OS, the replaced "Branch and Link" instruction calls the C code contained in this module.
