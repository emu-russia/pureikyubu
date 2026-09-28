/*

# The GBA link cable (DOL-011)

The cable that turns a Game Boy Advance into a **Joybus device** on one of the console's four
controller sockets. There is no hardware in the cable: both ends speak the protocol the standard
controller speaks, and the console drives the GBA with an extended command set that can halt it,
upload a program into it and exchange 32-bit words with it (see gba-link-cable.md).

## The two ways a Game Boy Advance is configured

The Game Boy Advance is a machine of its own - the emulator runs it as a portable console - and this
module is what happens when it is put *behind the console*:

  * as a **separate device** the GBA is in the peripheral pool with no port: it runs its Game Pak
    the way the portable front end runs it, and the console knows nothing about it. That is what an
    unplugged "GBA + Link Cable" entry in the settings *is* - the device keeps its cartridge, its
    BIOS and its running state, and plugging it into a socket is what makes the console talk to it;
  * as a **peripheral** it is plugged into a controller socket, and from that moment the console's
    SI channel drives it with the Joybus command set below. The device answers out of the Game Boy
    Advance's own JOY registers (see gba_sio.cpp), so a Game Pak that has selected JOY bus mode
    behaves exactly as it would on real hardware.

Both are the same device model with the same settings; only `Attach` differs.

## The command set

| Command | Name | Out / in | What it does |
|---|---|---|---|
| `0x00` | JOY-STATUS | 1 / 3 | the two type bytes and the GBA's JOY status |
| `0x14` | JOY-READ | 1 / 5 | the 32-bit word in the GBA's JOY_TRANS and the status |
| `0x15` | JOY-WRITE | 5 / 1 | the 32-bit word into the GBA's JOY_RECV, and the status |
| `0xFF` | JOY-RESET | 1 / 3 | reset the GBA's JOY interface; repeated resets ask for a JOY Reboot |

The console library masks the status byte with `0x3A` and exposes four bits of it
(`GBA_JSTAT_RECV` `0x02`, `GBA_JSTAT_SEND` `0x08`, `GBA_JSTAT_PSF0` `0x10`, `GBA_JSTAT_PSF1`
`0x20`). The status a command answers with is the GBA's `JOYSTAT`, which is the same register the
Game Pak's program watches, so the two sides cannot disagree about what the port is doing.

## The machine behind it

The Game Pak has to be *running* for any of this to mean anything: the console's first probe finds a
GBA that has not reached its JOY loop yet, and the protocol's timing rules (a command at least once
every six video frames, a reset followed by another within about 100 ms) exist because the two
sides are two computers booting at once. The device therefore advances the Game Boy Advance by the
emulated time that passed since the console last spoke to it - see `RunToNow` - and only while its
"Run Game Pak" switch is on.

*/

#pragma once

//! The controls of the Game Boy Advance this device carries, in the order it publishes them. The
//! ids are the ones a binding in the configuration is named after, and they are the same ids the
//! Game Boy Player uses (a Game Pak is a Game Pak wherever it is played).
enum
{
	GBL_ACT_A = 0,
	GBL_ACT_B,
	GBL_ACT_SELECT,
	GBL_ACT_START,
	GBL_ACT_RIGHT,
	GBL_ACT_LEFT,
	GBL_ACT_UP,
	GBL_ACT_DOWN,
	GBL_ACT_R,
	GBL_ACT_L,

	GBL_ACT_MAX
};
