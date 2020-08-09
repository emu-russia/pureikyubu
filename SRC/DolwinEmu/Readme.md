# DolwinEmu

This component is used to control guest emulation (GameCube).

You can assume that the "Loaded" state is equivalent to the fact that the GameCube is on (powered), 
and the "Running" state is equivalent to the main Gekko clock is running.

The architecture of the emulator is designed in such a way that the running Gekko thread is the main driving force of the emulator.
All other emulation threads are based on the Gekko emulated timer (Time Base Register).

Thus, if the Gekko thread is in a suspended state, all other hardware modules are also "sleeping".

## Supported file formats

Dolwin supports the following file formats:
- DOL
- ELF
- BIN (not sure if this works and generally there are probably no demos left in .bin format)
- GCM/ISO: unencrypted GameCube disk images

## HWConfig

To avoid using configuration from the HW component, all Flipper hardware emulation settings are aggregated
into the HWConfig structure.

## Jdi

Jey-Dai interface allows you to control guest emulation, load and unload executable files (DOL, ELF) or DVD images and so on.

A list of commands can be found in EmuJdi.json

## DolwinEmuForPlayground

A special build version that links with Null backends for use in DolwinPlayground.
