# DolwinEmu

This component is used to control host emulation (GameCube).

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
- GCM/ISO: unencrypted GameCube disc images. Altered dumps from scenic groups are also supported.

Dolwin does not in any way support game piracy. Although who cares, after all, games are not for sale, according to Wikipedia, GameCube support was discontinued in 2007.

## HWConfig

To avoid using the UI to get configuration from the HW component, all Flipper hardware emulation settings are aggregated
into the HWConfig structure.

## Jdi

Jey-Dai interface allows you to control host emulation, load and unload executable files (DOL, ELF) or DVD images, apply patches and so on.

A list of commands can be found in EmuJdi.json
