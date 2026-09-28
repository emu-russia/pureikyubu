# Peripheral devices

The GameCube reaches its peripherals through two different buses, and every peripheral is a device
of its own. The emulation of them used to be spread over the modules that happened to touch them:
`si.cpp` knew the answer bytes of the controller, `padsdl.cpp` knew both the SDL side and the way a
pad is polled, `memcard.cpp` mixed the card format with the EXI lifecycle, and the settings of a
device were kept wherever its consumer was. The subsystem described here is the single place a
peripheral lives, and `uisettings.cpp` is the single place its settings are edited.

## The pieces

| Piece | What it is |
|---|---|
| **DeviceID** | What a device model is, in one number: `0x00010001` is the standard controller (DOL-003), `0x00020001` is a memory card, `0x00030001` is the Game Boy Player. A model is registered by the module that implements it (`Peripherals::RegisterFactory`), so the subsystem does not know what a pad or a card is, and a build that does not compile that module simply has no such devices |
| **Port** | A socket of the console: the four SI channels, the two memory card slots, serial port 1 (EXI0 CS2B, the two network adapters), and the Hi-Speed Port (the Game Boy Player). A port belongs to a bus (SI, EXI or HSP), and a device can only be plugged into a port of its own bus |
| **Pool** | The devices the user has, which is the list the configuration holds. Every device owns the settings of its own entry: its model, its name, the port it is plugged into and whatever else the model keeps. An entry that does not name a port at all is a device that has never been placed - the shipped defaults are written that way - and it goes into the first free port of its bus when the pool is opened; an entry that names `-1` stays unplugged, because that is what the user asked for |
| **Actuator** | One control of a device (`PAD_ACT_*` in cont.h for a pad, `GBP_ACT_*` in gbplayer.h for a Game Pak). A device publishes its actuators and the settings window draws one row per actuator |
| **Binding** | What drives an actuator: a keyboard key and a host game controller button or axis |
| **HostInput** | The host side of the bindings, and the host controls a device drives back (the rumble motor). Implemented by the front end (`padsdl.cpp` over SDL2, `padnull.cpp` for the headless build) and by the unit tests |
| **Property** | Everything else a device shows about itself in the settings window: a checkbox, a read-only line, a file the device owns (a memory card's image, `PERIPH_PROP_FILE`), or a file it only reads (a Game Pak or a BIOS, `PERIPH_PROP_ROM`). The kind picks the editor and the browser, so a device does not have to know how the dialog draws it |

The device implementations are modules of their own: the standard controller is `cont.cpp` (the
Joybus command set, the analog channels, the rumble motor and the actuator table), the memory card
is `memcard.cpp` (the card format and the flash protocol it already owned), the Game Boy Player is
`gbplayer.cpp` over the Hi-Speed Port of `hsp.cpp` (the port's ARAM window, the identification
handshake the console runs on it, and the Game Boy Advance the Game Pak runs on - see
[gbplayer.md](gbplayer.md)), and the Game Boy Advance on a link cable is `gbalink.cpp` (the DOL-011
command set, with the same machine behind it; the two Game Boy devices share the controls and the
settings, and differ only in which bus the console reaches them by).

A device model is registered by its module, so a build that compiles another one offers it in the
settings window without a line of front end code: the "Add" row of a page lists the models of *that
bus* (`Peripherals::ModelCount` / `ModelAt`), which is how a GBA link cable or a network adapter is
put into the pool.

## The Game Boy Advance in a device

Both Game Boy devices own the same machine (`src/gba`), and the machine is run **only when it has
something to do**: it is advanced by the emulated time that has passed since the console last spoke
to it, never on a timer of its own, so a device the console ignores costs nothing. What tells the
two apart is what counts as "something to do":

* the **Game Boy Player** (`gbplayer.cpp`) runs the Game Pak once the console has powered the
  Player through the ARAM window (see [gbplayer.md](gbplayer.md)) and the "Run Game Pak" switch is
  on;
* the **link cable** (`gbalink.cpp`) runs it when a Game Pak is in the bay **or** when the console
  has driven the port as a link - the JOY commands, not the plain pad polls. The two devices are
  also the two that carry a picture a GameCube session cannot see on its own, which is what the
  `gbpshot` debug command writes out (see [mcp.md](mcp.md)). The second case is the
  one a *multi-boot* needs: a title that streams its program into the partner's RAM (the Tingle
  Tuner, WarioWare's GBA modes, the Four Swords) does it with an **empty** bay, and it is the
  machine's BIOS that receives the program. A device that is only polled as a pad, with an empty
  bay, still does not run.

The link device says so in the log, once, when the console first drives the port:

```
GBA Link: the console drives the port as a link (command 15)
```

and the settings window's "GBA" line reports what the bay holds, how many frames the machine has
produced, and - with an empty bay - that the console is driving the link. A title that never drives
the port is what a wrong socket or a title without the feature looks like, and that is what the log
is for.

What the device speaks is the **JOY** protocol a Game Pak runs from its own cartridge (status, read,
write, reset). It does **not** speak the transfer that puts a program *into* a Game Boy Advance -
the multi-boot upload. That gap is visible in the Game Boy Player Start-up Disc, which carries a
19 464-byte GBA program of its own (the `AGBJ` ROM at `0x8019_50A0`, with the standard GBA entry and
boot logo) and prints `GBAPadInit : ClientImageSize = 19464` for it: that program is meant for a GBA
on a socket, and the console's side of sending it is not implemented here (see
[gbplayer.md](gbplayer.md) 5).

## The two network adapters

The Broadband Adapter (DOL-015) and the Modem Adapter (DOL-012) share **serial port 1** - EXI
channel 0, chip select 2 - and are told apart by their EXI device ID (`0x04020200` and
`0x02020000`). Both are devices of the EXI bus in the pool, so which one is in the socket (and
whether the socket holds anything at all) is a setting like any other.

The adapter itself is `bba.cpp`, and its network is `bbaudp.cpp`: the console reaches an **EXI
shim**, which fronts an off-the-shelf Ethernet controller, and a selection is a **command word
followed by the data** - the shape the console's own driver speaks (`eth.a`'s `readEXIcmd`,
`writeEXIcmd`, `readcmdLong` and `writecmdLong`):

| access | command word | then |
|---|---|---|
| controller read | four bytes: `0x8000` and the address in bits 15:8 | an `n`-byte read |
| controller write | four bytes: `0xC000` and the address | an `n`-byte write |
| shim read | two bytes: `reg << 8` | an `n`-byte read |
| shim write | two bytes: `0x4000 \| reg << 8` | an `n`-byte write |

The command word is the first transfer after the chip select is asserted; the transfers after it
carry the data, most significant byte first, and the **address walks forward with every byte** -
which is how the driver reads the six MAC bytes or a packet in one burst. The shim's register is
six bits (bit 6 of the byte is the write flag) and the controller's address is sixteen: the register
file is page 0 and the packet memory is the pages above it. The device is told which transfer begins
the sequence (`Peripherals::TransferEXI` passes the channel's `firstImm` flag, exi.cpp), because a
two-byte write can be either a command or a byte of data. The controller's register file, its
identification, its MAC address and its receive/transmit ring are emulated; the **network behind it is not** - the device hands frames to a
`BbaBackend` (see bba.h) and takes the ones it receives from the same place, and the front end is
what installs one. With no backend the adapter is a console whose cable is unplugged: it identifies
itself, reports the link down and no frame ever arrives. The backend the builds ship (`bbaudp.cpp`)
puts the frames on **UDP**, which is what makes two emulators play on one emulated segment: the
"GCN Hardware" page of the settings names the other end (`BBA_PEER`, a `host:port`) and the port
this emulator listens on (`BBA_PORT`), and the network is opened with the emulator. An empty peer is
the default and means "no network", so a user who has not set anything up gets an adapter that
cannot reach anyone - and the log says which of the two it is. Two things in the device are **models rather
than reconstructions** and are marked as such in the source: the challenge/response handshake (its
algorithm is not public, so the emulated adapter accepts any response) and the split length field of
a received packet's descriptor.

The modem is `mdm.cpp`, and it speaks the same command-then-data protocol (its library's `readcmd`
and `writecmd` build the same two-byte words), with a six bit register number of its own page:
register `0x01` is the status the driver polls, `0x0D` the error status, `0x12` the raw status, and
registers `0x06`/`0x07`, `0x0E`/`0x0F`, `0x10`/`0x11` are the divisor and threshold pairs. The
device ID is the four-byte read of register 0 (the same register read as one byte is the UART's
data register), and the modem itself is driven with AT commands through that data register. The register file, the identification, the acknowledge
commands and the AT interface are emulated; the **telephone line is not** - the setup commands are
answered `OK`, which is what the driver's bring-up waits for, and a dial is answered `NO DIALTONE`,
which is the result code a modem with nothing in its socket reports. A front end with a line
simulator behind it installs a `ModemLine` (see mdm.h) and the call is handed over instead.

## Checking the two adapters without a game

An adapter is not much use without a title that speaks IP, and none of the SDK's demos do. What can
be checked without one is the half of the driver that lives on the *console* side: the bring-up the
ethernet library performs before any packet moves. The replay driver in `bbadrv.py` (outside the
repository, together with its loader) assembles exactly that sequence out of the shipped library -
select the adapter on EXI channel 0, chip select 2, at 32 MHz; read the device type the way
`EXIGetID` does; read the controller's own identification; take the MAC; poll the link state; write
and read back a controller register and a shim register; send a challenge - and leaves the answers
in main memory where the debugger can read them. The data cache is write-back, so the driver flushes
its results before it stops, or the dump would show zeros.

Both adapters answer it, and they answer *differently*, which is the point: the library tells them
apart by exactly these values.

| The driver reads | Broadband Adapter | Modem Adapter |
|---|---|---|
| the EXI device type | `0x04020200` | `0x02020000` |
| the controller id at `0x44` | `4D 58` (`"MX"`) | `0x00020000` |
| the MAC burst | a MAC (`00 09 BF 00 00 01`) | the adapter's own id |
| the link state at `0x31` | `03` | `00` |
| MISC2 (`0x50`) written `0x80`, read back | `80` | `00` |
| the shim revision written `0xD107`, read back | `D1` | `D1` |
| the challenge burst | `5A 5A A5 A5` | `00 00 00 00` |

What that shows: the transport (the command-then-data protocol of both chips) works end to end from
the console's side, the two adapters are told apart the way the library tells them apart, the
registers that belong to the common shim behave the same on both, and the registers that belong to
each chip are the chip's own. What it cannot show is the top half of the stack - ARP, IP, the
sockets the library builds on them - which needs a title that uses them.

### The command word, and the trap of keying on a change

Both network adapters - and the AD16 debugger - read a command word and then data, and the device
has to know which transfer is which. The emulator marks the first transfer after a *chip select*
with a flag the devices look at, and it used to set that flag only when the *selected device
changed*: reasonable-looking, and wrong. A driver that is handed a bus on which the socket is
already selected - which is what happens when a driver is dropped into a running console, and what
the console's own `EXISelect` produces every time, since it writes the chip-select field whether or
not it was already set - begins its sequence with a select that changes nothing. The flag stayed
down, its command word was taken for data, and the device answered from whatever register the
previous sequence had left behind.

The symptom was intermittent, which is why it survived so long: running the console-side replay
driver against the **modem** adapter three times gave the identification as `0x02020000` twice and
`0x00000000` once, depending on what the demo the driver was dropped into had left on the bus. What
the hardware keys on is the *assertion*, not the change, so the flag is now set by every write that
asserts a select and by nothing else - the write path is the only caller, so a write that carries no
select cannot reset a sequence in progress. Three runs out of three read `0x02020000` afterwards,
the Broadband Adapter still reports `0x04020200` and the memory card is unaffected (it decodes its
own command bytes rather than the flag).

## The memory card, reviewed

The card is the oldest device in the pool, and reviewing it against the console's own library
(`card.a`) and the DolphinSDK card demos turned up four things that had to be right before a card
could be used at all. They are recorded here because each of them is invisible until a real program
mounts a card:

* **the command decoder has to start idle.** `Command` is what tells the emulated card that the next
  byte of a write is an opcode rather than the tail of the previous command, and the value that means
  "idle" is `0xFF`. The device came up with the field zeroed, so it read every byte of its first
  write as an extra, never decoded a command and answered nothing - which the console's library
  sees as "no card in this slot". `MCConnect` now parks the decoder.
* **the EXI device ID is not the card ID.** The 32-bit type (`EXI_MEMORY_CARD_59` = `0x00000004`,
  and so on up the capacity ladder) is read with a four-byte immediate transfer, and the bytes go out
  most significant first, so the capacity code lives in the *low* byte of the register. The module
  shifted it into the top byte, which the library compares against nothing it knows. The two-byte
  card ID and the one-byte status *are* shifted (their transfers are shorter), which is what made
  the mistake easy to keep.
* **erase card is three bytes** (`0xF4 0x00 0x00`). The table gave it no trailing bytes, so the two
  zeros were refused as extras - and the refusal used `Halt`, which raises a front-end report on
  every retry and cost about 80x on the demo that does it.
* **the card wraps its addresses.** A flash part is smaller than the address its controller can put
  on the bus, so the lines above its capacity are not connected and a transfer at an address the
  part does not have is the same bytes as the wrapped address. The console's library reads at
  addresses that only exist on larger parts while it identifies the card; refusing them (or leaving
  the buffer untouched) leaves it unable to mount a card. Reads, writes and sector erases now wrap.
* **a read streams from an internal address.** The four offset bytes a read carries are where the
  block *starts*, and every transfer of that command continues where the last one stopped; only a
  new command word resets the counter. The module re-read the command's address on every transfer,
  so one command returned the same four bytes over and over: the library's mount reads the card's
  first 32 bytes as nine immediate transfers and got the first four bytes nine times, which is what
  broke every read the unlock unit makes. `MCReadArrayProc` now advances `readAddress` by what each
  transfer took, and `MCTransfer` drops the counter when it decodes a new command.

### The unlock, and why the card has one

An original card does not hand its code over to anyone who asks: the library hides a **challenge**
in the read command's spare fields and only recovers the code if the card answers it. That code is
not in the flash at all - it is the controller's, which is why nothing the console wrote into the
array can stand in for it, and why the exchange is worth having. The sequence the console's own
library spells out (`CARDUnlock`) is:

1. The library picks a challenge (a 20-bit value from a linear congruential generator seeded by the
   tick counter) and a length of 4 to 32 bytes from the same generator. Both differ on every call.
2. It sends the challenge **as the four address bytes** of a read, scrambled by `ReadArrayUnlock`,
   and clocks out exactly that many bytes. What comes back is discarded: this read is the challenge.
3. It works out the answer for itself: the challenge through a bit-serial mixing step `n * 8 + 1`
   times, one closing operation, and a 32-bit reversal.
4. It reads the card's code and takes the answer back out of it, **walking the answer on between the
   words**: the first word is mixed with the answer, the answer is advanced 32 mixing steps plus a
   closing, the next word is mixed with that, and so on.
5. It reads the status byte and requires **bit 6 (`0x40`)**. A card that does not report it never
   accepted the challenge, and the mount ends with `CARD_RESULT_IOERROR` (-5).

The mixing step comes in two forms, which is why the unit carries two near-identical routines: the
challenge chain shifts the state right and takes its new bit from the bottom (`exnor_1st`), and the
walk over the words shifts it left and takes its new bit from the top (`exnor`). Each run's last
operation is not another step: the state stays and one bit is set from the mixer.

`memcard.cpp` implements all of it (`MCUnlockMask`, `MCExnorStep`/`MCExnorStepUp`,
`MCExnorClose`/`MCExnorCloseUp`, `MCBitReverse`, `MCUnlockChallenge`). Four details are the ones that
make a mount work at all, and all four were established by watching the library rather than by
guessing:

* **the code read comes from the controller, not the flash.** While the unlock is in progress a read
  returns the card's code, mixed with the walking answer, and this card's code is zero. Reading the
  flash instead makes the library recover whatever the *format* last wrote there - and the header's
  first twelve bytes are not the code, they are the code plus a value the library derives from the
  header's own `0x0C`/`0x10` words. The library compares its cached copy against exactly that sum, so
  a card that answers with the flash can never satisfy it.
* **bit `0x40` is the unlock flag, and only a challenge sets it.** The module used to set it on every
  read (it doubles as the flash's array-to-buffer bit). Reporting it before the challenge makes the
  library skip the unlock and trust whatever key is already in the SRAM, which is how a card could
  appear to mount while actually verifying a *previous* card's key.
* **a challenge is immediate, never a page read.** The first version of the arming test took any read
  of more than four bytes with a nonzero address for a challenge, which every 512-byte page read of
  the mount's own header read answers too. Arming on those claimed the card was unlocked before it
  had been asked anything, so the test now excludes DMA transfers outright.
* **a challenge can be exactly four bytes.** The length the library picks runs from 4 to 32, and four
  bytes is a single immediate transfer - it does not "look" longer than an ordinary read. The test
  used to require *more* than four bytes, which silently skipped the exchange whenever the generator
  drew a four, leaving the code read answered from the flash. That is what made one capacity fail
  while the others passed, and it is a reminder that the generator is seeded by the tick counter, so
  a fixed run draws the same length every time.

With all four right, `CARDMount`, `CARDCheck`, the directory listing and `CARDFreeBlocks` all return
0 on the SDK's own demos, and a card formatted in one run mounts in the next (the key lives in
`Data/sram.bin` between them). **All six capacities were verified end to end** - 4, 8, 16, 32, 64 and
128 Mbit, each reporting its own size and `size - 5` free blocks with 127 free directory entries -
and the two largest were mounted repeatedly (4/4 and 6/6) so that a fresh challenge was drawn every
time.

Where the model still is what it was: the flash is a flat image with no page-program timing, the
cards' transfer rates are not modelled (EXI is instant), and the file system itself is the guest's -
the emulation only ever moves blocks.

## Who calls whom

The emulation calls a device, never the other way around:

* the SI poll of a channel (`SerialInterface::SIPoll`) asks the pool for the device in that socket.
  The pool resolves the bindings against the host input, hands the device the value of every
  actuator (`SetState`) and takes its per-channel state (`Poll`), which the SI register file packs
  into `SICnINBUFH`/`SICnINBUFL` the way the hardware does;
* a communication transfer (`SerialInterface::SICommand`) hands the device the command bytes of the
  channel and takes its answer back (`Transfer`): 0x00 is the identification, 0x41 the calibration
  origins, and the motor is programmed by the payload of 0x40;
* an EXI transfer of a card slot (`ExternalInterface::exi_write_cr`) is dispatched to the card that
  is in the slot (`ExiTransfer`), which runs the flash protocol;
* an ARAM transfer that lands past the internal 16 MB (`DSP::ARAMDmaRun`) is the Hi-Speed Port's
  window, and the port hands the block to the device plugged into it (`HSPDevice::Read`/`Write`);
* the rumble motor of a pad reaches the host game controller through `HostInput::Rumble`.

A device never makes an SDL call, and the front end never knows what a pad is: that split is what
the backend used to lack.

## The configuration

The pool is the `Devices` list of the `peripherals` section: one entry per device, and the entry is
what holds the settings of that device. What a device keeps is its entry - a handle of its own
object - and not its position in the list, so a device that is taken out of the pool takes its
settings with it: the list is a list, not a table with holes, and nothing that is stored next to a
device can end up belonging to another one (see the list accessors in `config.h`).

```json
"peripherals":
{
    "Devices":
    [
        { "Type": 65537, "Name": "Controller 1", "Port": 0, "VKEY_FOR_A": 27, "GCKEY_FOR_A": 65536 },
        { "Type": 131073, "Name": "Memory Card A", "File": "Data/card.mci" }
    ]
}
```

The pool of a fresh console is what `DefaultSettings.json` ships - the four sockets and the two card
slots, each with the device that port is meant for, and none of them plugged in - and the settings
window edits that list. `Type` is the model (see DeviceID above), `Name` is the name the user gave
the device, `Port` is the socket it is plugged into, and everything else belongs to the device the
model registered: a pad keeps its bindings there (`VKEY_FOR_A`, `GCKEY_FOR_A`), a memory card its
file (`File`) and its save policy.

The pads and the memory cards used to keep their settings in sections of their own (`controllers`
with `PluggedIn_<n>` / `VKEY_FOR_*_<n>`, `memcards` with `MemcardA_*`), from the builds before the
pool. Nothing reads them any more: they are dropped from the document when the settings are read
(and so are the variables a device of the pool used to be named by, `Device3_Name`), so that a
configuration this build writes does not carry them along.

## The lifetime

The pool is opened with the **emulator**, not with the machine: the settings window edits the pool
with no game loaded, and the devices are plugged into the sockets of the console, which exist
whether or not a machine is running (`Peripherals::Open`, called by `EMUCtor`). It is closed by
`EMUDtor`.

The machine matters to a device that talks to it - a memory card is read through an EXI channel - so
the pool is told when one appears and when one goes away: `MachineOpened` (from the `Flipper`
constructor) puts the cards into their slots, and `MachineClosed` (from the destructor) takes them
out and flushes the files they were writing. The pool keeps the devices and the ports they are in,
so the next machine finds them where the user put them.

A device is never destroyed while the emulator runs: the emulation thread takes one out of the pool
(to poll a pad, to run a card transfer) while the settings window may be reconfiguring it. A device
that is removed from the pool is detached, taken out of the list with its settings, and kept alive
until the pool itself is closed in one piece.

## The settings window

Every setting of the emulator is in one window with a vertical strip of tabs on the left and a
property grid on the right (`uisettings.cpp`): the tabs are "General" (the game selector),
"GCN Hardware" (the console version and the firmware), "Controllers" (the pads and their bindings,
and the Game Boy Advance on a link cable), "Memory Cards" (the two card slots), "Network" (the
broadband and modem adapters of serial port 1) and "High-Speed Port" (the Game Boy Player). The
window is generic over the devices: it asks a device for its actuators and its properties and draws
them, and a device says which page it is edited on when it registers its model
(`PERIPH_PAGE_*`), so a new device model appears on the right page without a line of front end code.

A page follows the *device*, not the bus it hangs from, and that is not a detail: the cards and the
two network adapters are all EXI devices of the pool, so a page that took its devices from the bus
showed the adapters under "Memory Cards".

The dialog that each of those pages replaced - `Options -> Controllers -> Port n`,
`Options -> Memcards -> Slot A`, and the settings that had no dialog at all (the selector view) -
are gone from the menu, which now carries a single `Options -> Settings...` item.

A setting takes effect as it is edited (a binding is used by the next poll, a card file is opened at
once), and the pool writes the device back itself (`SetBinding`, `DefaultBindings`, `ClearBindings`,
`AddDevice`), so the window never has to remember to save anything: it has no OK/Cancel either.

## Tests

`testing/peripherals_test.cpp` drives a pad the way a user does - it binds a control to a host
control of the test input double, says that the host reports it, polls the channel and reads the
answer out of the SI registers - so the whole chain (binding, actuator, the device's state, the
register file) is covered. It also covers the pool, the buses, the motor, the origins, and that a
device which is added (with its name and its bindings) is still there after the emulator was closed
and started again. The SI tests (`testing/gfx_si_spec_test.cpp`) cover the interface block itself;
their machine plugs a pad into every socket for the same reason.

The settings are covered where they can be got wrong: a device's properties survive a restart
(`Peripherals_TheNetworkAndPlayerSettingsSurviveARestart`), an entry that names a port takes it
from the device the pool placed there (`Peripherals_AnEntryThatNamesAPortTakesItFromThePlacedDevice`
- the case the shipped defaults create, because they name no port for the pads), a negative value
comes back out of the settings file as itself (`Settings_ANegativeValueSurvivesTheFile`), and an
empty cartridge bay still runs a machine the console drives as a link
(`GbaLink_AnEmptyBayRunsWhenTheConsoleDrivesTheLink`).

The memory card is the one device the harness cannot reach: `memcard.cpp` is not part of the test
build, because a card registers itself with the pool from that file, so the pool the tests build has
no card slots and the transport is not driven by any test. What covers it instead is the console's
own library - the mount, format and check demos under `HW2/bin/demos/carddemo` - driven through the
emulated EXI port with the transfer trace on, which is how the read counter above was found.

The two network adapters and the Game Boy Player are driven both ways. `bba.cpp` and `mdm.cpp` have
a test each that hands a transfer straight to the device (so the flag that marks a command word is
set by the test itself), and the broadband adapter has one that goes through the channel's register
file instead - the CSR write, the data register, the CR that starts the transfer - which is the only
way the command-word rule is covered: `Bba_ASelectThatDoesNotChangeStartsANewCommandSequence` makes
the identification, then a sequence that leaves another register selected, then the identification
again, with the chip select asserted every time but never *changed*, and reads the device ID back.
The test machine has a real external interface for that (see `testing/gfx_test_support.cpp`), with
the handful of host-side pieces of the Macronix chip stubbed out beside the other device doubles,
since the machine under test loads no boot ROM, no fonts and no SRAM file.

The **chip selects** are the other half of the EXI channel that a device test would otherwise walk
past, and they are pinned through the same register window: a write that asks for two of them takes
none, a write that asks for a second one while another is asserted drops both, and the dropped one
can be taken again (`Exi_TheChipSelectsOfAChannelAreMutuallyExclusive`). A test that leaves a select
asserted would arbitrate against the next test's own select, which is why it deselects the channel at
its end - the register file belongs to the machine and outlives a test.

## What the devices are checked against besides the unit tests

The unit tests cover one device at a time. The devices that talk to the console through a library of
its own are checked by running the console's **own drivers** against them, and the results are what
the sections above were written from:

* **the memory cards**, with the SDK's `carddemo/format` and `carddemo/list` and a freshly created
  image of each capacity. All six models pass: `CARDFormat` reports the right size (`4`, `8`, `16`,
  `32`, `64`, `128` Mbits, 8192-byte sectors) and `CARDMount`/`CARDCheck` then answer `0`, with the
  free space the model should have - `483328` bytes (59 blocks) on a 4 Mbit card up to `16736256`
  bytes (2043 blocks) on a 128 Mbit one, and 127 files free on every one. The mount that `format`
  does *before* formatting fails with `-6` (`CARD_RESULT_BROKEN`), which is what an empty card is.
* **the two network adapters**, with a small replay driver the debug interface loads into a booted
  demo (`bbadrv.py` / `mdmdrv.py`). The broadband adapter answers `0x04020200`, the family signature
  `MX`, the address `00:09:BF:00:00:01`, link state `03`, and round-trips `0x80` through MISC2 and
  `0xD1` through the shim revision; the modem answers `0x02020000`, a chip id of `0x00020000` and the
  same shim revision, and refuses a dial with the result code a real modem reports.
* **the controllers**, by pressing a key on the host and watching the console's own pad demo print
  it: `hkey 27 1` turns the demo's first port from `__` into `O_`, and releasing it turns it back.
  The whole chain is exercised - host key, binding, the device, the SI register file, the guest.
* **the whole SDK corpus**, which is the broadest check of the console's side: 168 executables under
  `HW2/bin/demos`, of which **91 run unattended and produce their reports**, with the rest needing
  arguments, failing on purpose or needing the demo disc's data (see [main.md](main.md)).

The **settings** are checked where each device meets them rather than in one place: the card sweep
switches a card's *file* and gets a card of that size, the adapter drivers move the adapter to
serial port 1 and the driver finds it there, and the pad test drives the *bindings* of the default
layout. The unit tests cover the rest - that a device's properties survive a restart, that an entry
naming a port takes it from the device the pool placed there, and that an empty cartridge bay still
runs a machine the console drives as a link.


