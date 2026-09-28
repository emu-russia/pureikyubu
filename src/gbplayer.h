/*

# The Game Boy Player (DOL-017)

The Game Boy Player is the add-on that plays Game Boy, Game Boy Color and Game Boy Advance
cartridges on the console. It is not an EXI device and not a Joybus device: it plugs into the
console's **Hi-Speed Port** (see hsp.h), which is the console's ARAM expansion, and it is the only
retail device for that port.

The Player contains the machine that runs the cartridge. The console does not emulate a Game Boy:
it uploads the program the Player runs (its "client image"), talks to it through 32-byte blocks in
the port's window, and plays back the picture and the sound the Player hands it. This module is the
*device*: the cartridge bay, the machine behind it, the controls the host drives and the properties
the settings window edits. The port itself is `hsp.cpp`.

## The cartridge bay

A Game Pak is a **property of the device**, exactly like the image file of a memory card is a
property of the card: the file name lives in the device's own entry of the peripheral pool
(peripherals.h) under "GamePak", and the settings window draws a "Game Pak" row with a file browser
on it (see `PERIPH_PROP_ROM`). Inserting one loads it into the machine the Player runs; ejecting
(the empty value) takes it out. Because the property belongs to the device and not to the port, the
Game Pak survives unplugging the Player, and a Player that is moved to another console takes its
cartridge with it.

The Player also has a **BIOS** row: the machine inside the Player boots like a Game Boy Advance, and
a real 16 KB BIOS image can be installed for it (empty = the built-in boot ROM). Both files are
`PERIPH_PROP_ROM`: the device only ever reads them.

## What answers on the wire

The console identifies the Player with the loopback handshake the port's window carries (see
hsp.h): it writes a 32-byte block of a constant byte to the window's first mailbox and requires the
complement back. After that the two sides exchange the 32-byte command and status blocks the disc's
own driver programs. The block model here is deliberately small and is documented where it is
implemented: the handshake is the part the retail disc's bring-up depends on, and the rest is the
shape of the protocol the disc uses.

*/

#pragma once

//! The controls of a Game Boy (Advance) that the host drives, in the order the device publishes
//! them. The order is what a consumer that drives the device programmatically addresses a control
//! by; the id of an actuator (the name of its binding in the configuration) is in the descriptor.
enum
{
	GBP_ACT_A = 0,
	GBP_ACT_B,
	GBP_ACT_SELECT,
	GBP_ACT_START,
	GBP_ACT_RIGHT,
	GBP_ACT_LEFT,
	GBP_ACT_UP,
	GBP_ACT_DOWN,
	GBP_ACT_R,
	GBP_ACT_L,

	GBP_ACT_MAX
};
