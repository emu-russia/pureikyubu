# The Game Boy Player as the console sees it

This note is a report on the **hardware** of the DOL-017 Game Boy Player and on the way a retail
disc drives it. It is written from two sources that are both available to this project: the public
architecture documentation of the console (the port's place in the ARAM/SDRAM domain) and an
**execution trace of the real Start-up Disc** (DOL-018, game code `UGPE01`) on the emulator, plus a
static analysis of that disc's `main.dol`. Everything below is either observed in the disc's code or
marked as an open question.

The project's own `docs/`-level specification of the port stays where it is; this file is the
evidence behind it and the place to record what the emulation had to discover.

---

## 1. Where the port lives

The Game Boy Player does **not** appear on EXI and does **not** appear on SI. It is reached through
the **ARAM (SDRAM) controller** inside the audio/DSP block, which is why the port is also called an
"expansion" of ARAM. Two consequences follow from that and are confirmed by the disc:

* the console talks to the Player through **ARAM**, not through a register block in the IO window;
* the Player has the whole 16 MB of internal ARAM in front of it, because it hangs off the same
  SDRAM bus — that is how a video frame's worth of data can move at all.

There is no HSP register block in the `0x0C00_0000` IO window. A trace of every MMIO access the disc
makes over several minutes of emulated time reaches exactly these windows and no others:

| Window | Block |
|---|---|
| `0x0C00_2000` | VI |
| `0x0C00_3000` | PI |
| `0x0C00_4000` | MI |
| `0x0C00_5000` | **DSP / ARAM controller** |
| `0x0C00_6000` | DI |
| `0x0C00_6400` | SI |
| `0x0C00_6800` | EXI |
| `0x0C00_6C00` | AI |
| `0x0C00_8000` | GFX FIFO |

Nothing in the run is an unimplemented access: the disc never touches a register outside the blocks
above. The Player is therefore invisible in the IO register file, and the only console-side
registers the disc uses to drive it are the **ARAM DMA engine** (`AMMA`/`AMAA`/`AMBL`/`AMCR` at
`0x0C00_5020`-`0x0C00_502B`, `AMCR` at `+0x12`) and the **PI interrupt bit 13** (`HSP`).

## 2. The window: an ARAM expansion, not RAM

The ARAM controller addresses **two** memories on the same bus: the internal 16 MB and an optional
expansion chip-select group. `AMCR` (`0x0C00_5012`) carries both sizes:

| Field | Meaning |
|---|---|
| bits 2:0 | internal ARAM size, `0` = 2 MB, `1` = 4 MB, `2` = 8 MB, `3` = 16 MB, `4` = 32 MB |
| bits 5:3 | **expansion** size, same encoding |
| bit 6 | mode-register-set control |

This is directly visible in the disc. The SDK's `AR` library writes `AMCR = 0x0043` ("16 MB
internal, no expansion") and then probes for a *plain SDRAM* expansion module by moving test blocks
to and from ARAM addresses at and past the 16 MB boundary. On a console with no module the writes
are discarded and the reads return zero, so the probe concludes "nothing there".

The Game Boy Player's driver then does something else entirely: it **claims** the window.

```
AMCR write 0x0063   internal = 3 (16 MB), expansion = 4 (32 MB)   ; claim the largest window
   ... probe / handshake ...
AMCR write 0x005B   internal = 3 (16 MB), expansion = 3 (16 MB)   ; settle on 16 MB
```

Both writes are the disc's own, at `0x8007_ABB8` and `0x8008_9C00`. The order matters: **the driver
claims the window first and only then asks the window whether it is a Player.**

The window's address in ARAM space is the *base of the internal ARAM plus its size*: the driver
computes the internal size from `AMCR[2:0]` (16 MB) and hands that value to its own "set base"
helper, so all Player traffic is at **ARAM `0x0100_0000` and upwards** — beyond the internal array,
in the expansion.

> **Conclusion.** The Player's address space is the 16 MB that begins where the internal ARAM ends.
> It is a window of *registers and mailboxes*, not of memory: a read does not return what was last
> written, which is exactly why the SDK's plain-SDRAM probe must and does fail before the Player
> driver takes over. The driver treats a non-zero expansion size from that probe as an error and
> refuses to initialise (`0x8008_A9EC`, error code `4`).

## 3. What the disc does, in order

The whole bring-up is one function on the disc; the observed sequence is:

1. **Check that the SDK's ARAM probe found no expansion.** `AMCR`'s expansion field must be zero.
   If it is not, the Player is not initialised (a plain SDRAM module is plugged in instead).
2. **Claim the window**: `AMCR = 0x63`, i.e. internal 16 MB, expansion 32 MB.
3. **Set the Player base** to the internal ARAM size (`0x0100_0000`).
4. **Challenge/response handshake.** Four rounds at window offset `+0`:
   * fill a 32-byte block with a constant byte `X` and write it to the window's first mailbox;
   * read 32 bytes back;
   * require the byte at offset `+1` of the answer to be the **complement** of `X` (`~X`);
   * any round that does not answer `~X` fails the bring-up.
   This is the actual **presence detection**: it is a loopback that a plain memory window cannot
   pass. The four challenge bytes come from the disc's read-only data.

   > **Reproduced.** Implementing the window as a device that answers the first mailbox with the
   > complement of the last block written to it (and nothing else) makes the retail disc pass this
   > step: the trace of the run shows the console writing `DE AD`, `C3`, `3C`, `FF`, `00` and
   > reading back `21 52`, `3C`, `C3`, `00`, `FF`, and the disc then leaves its "no Player" state
   > and starts driving the real command blocks (`+0x40_0000`, `+0x80_0000`, `+0xC0_0000`,
   > `+0xD0_0000`). That is the proof that the detection is this handshake and not the `AMCR`
   > field or the plain-SDRAM probe.
5. **Settle the window size** to 16 MB: `AMCR = 0x5B`.
6. **Upload the "client image"** (see §5) into the *internal* ARAM at offset `0x0080_4000`.
7. **Program the mailboxes** (§4) and enable the **HSP interrupt** (PI `INTSR`/`INTMR` bit 13).
8. From then on the console and the Player exchange 32-byte command/status blocks and the frame
   stream, and the disc's menu is drawn.

The disc reports the outcome of bring-up with small codes (`3`, `4`, `5`, `6`): `4` is "an expansion
module is in the way", `5` is "the handshake did not answer", and the rest are earlier argument
errors. On a console with no Player the handshake fails at step 4 and the disc stays in its
"connect the Game Boy Player" state — which is exactly what the emulator does today.

## 4. The Player's window layout

The window is used as a set of **32-byte blocks** at fixed offsets from its base (`0x0100_0000`).
The disc's helpers write a 32-byte staging buffer in main memory to a window offset with an ARAM DMA
and read one back the same way; the offset is the only difference between the helpers:

| Window offset | Written | Read | Used for |
|---|---|---|---|
| `+0x000000` | all 32 bytes | all 32 bytes | identification handshake; the answer is the *complement* of the block written |
| `+0x400000` | byte 31 | yes | command / status — the answer is **byte 31** |
| `+0x500000` | byte 31 | yes | command / status — **byte 31** |
| `+0x800000` | — | yes | the **bulk stream**: 3840-byte blocks into a ring of 40 buffers |
| `+0x900000` | bytes 28-31 | yes | command / status — a **32-bit word** from bytes 25, 27, 29, 31 |
| `+0xC00000` | bytes 30-31 | — | command only |
| `+0xD00000` | bytes 30-31 | yes | command / status — **byte 31** |

That table is not read off the traffic; it is read off the disc's code. Every window transfer in the
disc goes through one ARAM-DMA helper, and its call sites are the whole protocol: six write helpers
(one per offset above, each filling its bytes of the staging block and starting a DMA into the
window) and six read helpers (one per offset, each starting a DMA out of the window and then taking
the field named in the table out of the block it read). The two `+0x800000` call sites are the
stream: the chunk is 3840 bytes and the destination walks a 40-entry ring of buffers
(`0x801E_4A80 + i * 0xF00`, `i` modulo 40).

An earlier reading of the traffic had this table wrong in three places, and the code settles them:
`+0x400000` and `+0x500000` are **read** as well as written (the `+0x400000` read is the one the
disc polls hardest — about twice as often as `+0xD00000`), `+0x900000` is **written** as well as
read, and `+0x800000` is not a status block at all but the one channel the console only ever reads.

The status reads take a field out of the block: **byte 31** for the three command channels (read
back as the halfword at `+0x1F`), and a 32-bit word assembled from bytes 25, 27, 29 and 31 for
`+0x900000`. The ISR tests bits `0x0555` of the resulting status — so the block has at least a
command word, a status word and a payload.

Open: which of the seven offsets is the frame buffer and which is the audio ring, and how the
`+0x800000` stream is framed. The disc's own strings name the two consumers (`GB-Video`,
`GB-Audio`) and an `aramStrm` object, and the ISR's status mask `0x0555` shows that the Player
reports several independent events at once.

## 5. The client image

The disc carries a program it uploads to a **Game Boy Advance**, and it prints its size
(`GBAPadInit : ClientImageSize = %d`, 19464 bytes). Where that program is on the disc, and where it
is *not*, is worth being exact about, because the two were once conflated here:

* **The program is a Game Boy Advance ROM, and it is in the disc's own data** at `0x8019_50A0`. Its
  first word is `2E 00 00 EA` — the standard GBA entry (`b` to `+0xB8`) — followed by the Nintendo
  boot logo, an empty 12-byte title, the Game Code **`AGBJ`** and version 0. 19 464 bytes is the
  size of the transfer the disc makes of it.
* **It does not go into the console's ARAM.** The ARAM trace does show a large contiguous upload —
  4096-byte blocks from main memory up to internal ARAM from `0x0080_4000` into the `0x008F_0000`
  area, 971 136 bytes — but the *content* of that region is data, not a program: its first `0x500`
  bytes are zero and what follows is a smoothly varying 16-bit sequence (sample-like), with no GBA
  header, no `AGBJ` and no code. It is the disc's own staging of assets (the disc's audio and
  graphics), which the AX/AR libraries put in ARAM the way every title does. An earlier note here
  read this upload as "the program the console puts into the Player"; the dump does not support
  that.

The upload of the GBA program is what the **link cable** is for: the console sends the 19 464 bytes
to a Game Boy Advance on an SI socket (the GBA-side "client" of the GBA-as-a-controller feature —
`GBAKey.c`, `GBAPadInit`). The device in this emulator (`gbalink.cpp`) speaks the *JOY* protocol a
GBA runs from its own cartridge (status, read, write, reset); it does **not** speak the multiboot
transfer that puts a program *into* a GBA, which is where this disc's client would go.

Two more pieces of the same mechanism are on the disc:

* a symbol/offset table at `0x800A_2AE0` whose literal name is **`binary_us.arc`**, followed by the
  names `OPTION_1`…`OPTION_6`, `FRAME_D`, `FRAME_U`, `circl_1a`…`circl_6a`, `icon_1`…`icon_6`,
  `logo`;
* the disc's **FST has exactly two files**: `binary_us.arc` (448 880 bytes, at disc offset
  `0x1E9960`) and `opening.bnr` (6496 bytes). The first is a **Yaz0-compressed RARC archive**
  (1 684 992 bytes once decompressed): the region-specific (US) form of the Player's assets; a
  Japanese disc carries `binary_jp.arc`.

The console-side driver of the GBA link is the disc's `GBAKey.c` (`GBAPadInit`, plus the "client
image" call dispatcher). The dispatcher indexes a table of 0x100-byte per-channel structures and
asserts with the message **`GBA - unexpected dsp call`** when it is handed a block that is not one
of them — so a "call" into the client image is an object with a slot at `+0xA8`, one object per
channel.

## 6. The interrupt

Only one console interrupt is involved: **PI bit 13, `HSP`** (`INTSR`/`INTMR` at `0x0C00_3000` /
`0x0C00_3004`). The disc's HSP handler:

* reads the status block out of the window (`+0xD00000`),
* acknowledges by **writing `0x2000` to `INTSR`** (`0xCC00_3000`) — the documented "acknowledged by
  setting the bit" for this source,
* tests `status & 0x0555` and dispatches to the per-event work,
* and can arm a callback that the main loop picks up.

The interrupt is not shared with EXI, SI or the DSP's own three causes: it is its own PI source, and
the console's OS keeps it in its own slot.

## 7. What the Player is, physically

The add-on has no video output of its own and no controller ports; it has a cartridge slot and a
Game Boy "External Extension Connector" on the front. Everything the user sees comes out of the
console's AV connector, and everything the user presses comes in through the console's four
controller sockets. That matches a device that is *inside* the console's memory system rather than
beside it: it reads the cartridge, runs it, and hands the console a picture and a sound.

The observations above are consistent with the widely-repeated description that the Player contains
"hardware nearly identical to a Game Boy Advance": the program the console uploads into it carries a
GBA Game Code (`AGBJ01`), and the console's library for it is written around a GBA-like client with
a call table. They are **not** proof: the code could still be a different implementation that
happens to speak the same conventions. What is established is that the Player executes the
cartridge itself and that the console uploads the software that runs the Player's user interface.

## 8. What is still open

* **The command set.** The 32-byte blocks, the meaning of the command word and of the status mask
  `0x0555`. The disc's own disassembly gives the mechanics (which offset, which direction, 32 bytes)
  but the opcodes are only exercised at run time, which needs a Player to answer.
* **The video and audio format.** How a frame is laid out in the window, whether the console pulls
  it or the Player pushes it, and how the console's display path turns it into a texture. The
  disc names the streams (`GB-Video`, `GB-Audio`, `aramStrm`).
* **The four challenge bytes** of the handshake (they live in the disc's read-only data and would
  need the SDA2 base to resolve statically).
* **The size the Player reports.** The disc settles on 16 MB; whether that is what the retail unit
  answers or what the driver decided to assume is not settled by this trace.
* Whether the internal ARAM copies the Player makes are visible to the console, i.e. whether the
  frame path is "Player writes into the console's ARAM and the console reads it", or "the Player
  keeps its own memory and the console reads it over the window". The upload of the client image
  into *internal* ARAM (§5) shows that the Player can read the console's ARAM at least, which makes
  the first reading the natural one.

## 9. What an emulation of the port has to do

For the emulator, the trace fixes the shape of the work:

1. an **ARAM expansion window** of 16 MB at ARAM `0x0100_0000` with a *non-RAM* response: the
   SDK's plain-expansion probe must fail, and the four-round complement handshake must succeed;
2. `AMCR` has to accept and keep the expansion size the driver writes, and report it back;
3. the **ARAM DMA engine** has to be able to move blocks into and out of the window (the window is
   reached by DMA, not by a CPU address);
4. **PI bit 13** has to be raiseable and its acknowledge path (write `0x2000` to `INTSR`) has to
   clear it;
5. the 32-byte blocks at the seven offsets, the client-image upload area at internal
   `0x0080_4000`, and the *client program itself* — which is where the Player's behaviour lives.

Items 1-4 are the port; item 5 is the Player, and it is the client image (a Game Boy Advance
program) that gives it its behaviour.

### What it costs

The Player runs a whole console of its own. A Game Pak that is running costs about as much host
time as the console it is attached to, so the device published by `gbplayer.cpp` has a **"Run Game
Pak"** switch: with it off the cartridge stays in the bay and nothing runs, which is what a user who
inserted a Game Pak and then went back to a GameCube game wants. The Player is also only *switched
on* (and the Game Pak with it) once the console has moved enough blocks through the window to have
really started talking to it - every GameCube title's ARAM library probes the expansion boundary on
the way up, and that probe alone must not start a Game Boy Advance.

The other half of the cost was in the emulator rather than the Player: the ARAM DMA engine used to
log every block it moved, unconditionally, with no way to turn it off. A Player disc that polls its
window moves tens of thousands of blocks a second, and every one of them cost two lines of text.
The block now honours an `AR_LOG` setting (off by default), like the other blocks' own logs; that
one change is worth about 4x on a run of the Start-up Disc.

## 10. The cartridge bay

The Player has the cartridge slot, so in the emulator the Game Pak is a property of the **Player
device**, exactly as the image file of a memory card is a property of the card: the file name is
kept in the device's own entry of the peripheral pool ("GamePak"), and the settings window draws a
"Game Pak" row with a file browser on it (`PERIPH_PROP_ROM` in peripherals.h - a file the device
only reads, which is why the browser opens on the cartridge extensions and not on the card images).
Inserting one loads it into the Game Boy Advance the Player runs; the empty value ejects it. The
property belongs to the device and not to the port, so the Game Pak travels with the Player when it
is unplugged and moved to another console.

The same device carries the "BIOS" row: the machine inside the Player boots like a Game Boy
Advance, and a real 16 KB BIOS image can be installed for it. Installing one changes nothing about a
Game Pak that is already running its own program from its own entry point, which is what a cartridge
does - the Start-up Disc's own test cartridge renders the same picture with and without a BIOS.

### Watching the Game Pak run

The Game Pak's picture has no way out of a GameCube session by itself (the portable machines publish
their GBA debug interface only when the emulator *is* one), so the device publishes a command of its
own: `gbpshot <file.png>` writes the 240x160 LCD of the machine in the Player to a PNG, and answers
with the file, its size and the machine's frame counter (see [mcp.md](mcp.md)).

That is what the Player's own test cartridge looks like when the Start-up Disc has brought the port
up: **`AGB/AGS TEST PROGRAM Version 7.0`**, drawn by the AGB CHECKER Game Pak that is in the bay. The
picture is the machine's own output, and its result lines are the machine's, not the emulator's
opinion:

```
MEMORY...... 000000XXX  FAIL
LCD......... 0000000    PASS
TIMER....... X00        FAIL
DMA......... 00000000X  FAIL
KEY INPUT... -0         PASS
INTERRUPT... 000000_
```

The frame counter runs at the Game Boy Advance's own rate (about 600 frames in the first half-minute
of emulated time), so the machine inside the Player is executing and scanning out. What the three
failing lines mean for the GBA core the Player runs is a question about that core rather than about
the port, and it is recorded here as observed.

The **"Run Game Pak"** switch is checked the same way, because it is the one setting that decides
whether the machine costs anything at all: with it off, the same run's frame counter stays at **0**
and the picture is empty, and with it on the run reaches its hundreds of frames. A title that never
touches the port leaves the Player off for the same reason (see "What it costs" below).

## What the Start-Up Disc does with the port

The disc the Player ships with is the one program that uses the port fully, so it is the yardstick
for the emulation. Run with the Player plugged into the Hi-Speed Port (port 7) and the disc's image
on the command line, it boots, reports the Player as `expansion code 3`, brings up
`***Initializing JFramework`, and then prints one more line before it goes quiet:

```
[Info] GBAPadInit : ClientImageSize = 19464
```

That line is **not** the Player's client program. It comes from the disc's `GBAKey.c` — the
GBA-as-a-controller support, which uploads a 19464-byte client image to a Game Boy Advance on the
**link cable** — and it sits in the disc's memory next to the strings `GBAKey.c` and
`GBA - unexpected dsp call` and in front of a table of the module's own code addresses. The
*Player's* client is the 971136 bytes of `binary_us.arc` that the disc DMAs into internal ARAM at
`0x0080_4000` (§5). The two are separate uploads to separate targets, and the message above is the
last thing the disc prints.

### The console's command routine, and why the channels have to hold their writes

The disc's own command routine (`0x8008_BE04`) is the whole protocol in one function:

```
read  +0x400000  -> S                  the status byte
write +0x400000  = S & ~0x02           acknowledge event bit 1
write +0x400000  = S & ~0x02 & ~0x04   acknowledge event bit 2
write +0x400000  = S        | 0x10     command bit 4
write +0x400000  = S        | 0x80     command bit 7
read  +0xD00000  -> T
write +0xD00000  = T | [saved mask]
write 0x2000 to PI INTSR               acknowledge the HSP interrupt
```

That is a **read-modify-write**, and it is the shape every command channel has: a channel is a
register *both* sides write, not a status the device alone owns. The console reads the byte, clears
the bits it is acknowledging and sets the bits of the command it is sending, and writes the result
back. The device answers the next read with what the channel then holds. The disc's driver state
machine (`0x8008_AF08` for the interrupt, `0x8008_B1F4` for the states) is built on exactly that.

Two things follow, and both were wrong in the emulation:

* **a write to a channel is a command, not an event of the device's own.** The port's interrupt line
  belongs to the device, and the console's ISR runs when the device sets a bit and stops when the
  console clears it. The device used to assert the line on *every* block the console moved, so the
  console spent its time servicing an interrupt it had caused itself.
* **a channel that answers a constant can never be acknowledged.** The console cleared bit 1, wrote
  the byte back, read the channel again and saw bit 1 set again — the same status, forever. Its
  driver then polled in a loop. This, and not a missing handshake, is what a 40-second run used to
  look like: **214 583 reads of `+0x400000` and 107 034 of `+0xD00000`**, with the mailbox at offset
  0 used 2 072 times and then never again. The handshake had long since finished.

### What the disc does now

With the channels holding their writes (`gbplayer.cpp`) and the interrupt raised when the *device*
changes state:

| | before | now |
|---|---|---|
| reads of `+0x400000` (20 s) | ~107 000 | 526 |
| reads of `+0xD00000` (20 s) | ~53 000 | 3 |
| mailbox reads (20 s) | ~1 000 | 2 676 |

The disc stops spinning and gets on with its bring-up. It prints, in order:

```
***Initializing JFramework
***-- Done
***Creating GbpSystemObject       <- new
***Allocating GbpSystemObject     <- new
***Creating Display object        <- new
***--Done                         <- new
GBAPadInit : ClientImageSize = 19464
```

`GbpSystemObject` is the disc's own Game Boy Player system object, so the port bring-up is now past
the point it used to be stuck at.

What is still missing is the **Player's side**: every command the console sends is answered by the
Player's own firmware, and nothing in this emulation implements it. The disc is not waiting for the
handshake any more, and it is not waiting for a program to be uploaded into the Player either (§5 —
the 971 136 bytes in ARAM are assets, not a program): it drives the port's channels and stops after
`GBAPadInit`. No 3840-byte block is ever read from `+0x800000` (the frame stream, §4) and the frame
buffer the VI scans out stays empty.

Two things that looked like the next gate were tried and are **not** it, which is worth recording so
they are not tried again:

* **a device that consumes the console's command bits** (clearing the `0x10`/`0x80` bits it wrote
  after taking them) changes nothing: the disc stops in the same place.
* **a Game Boy Advance on the link cable** changes nothing either. Attaching the link device to SI
  channel 3 and leaving it off produce the same last message, and the disc's own `GBAPadInit` is
  printed whether or not there is a GBA on a socket.
* **re-asserting the device's own level bits** (a Game Pak in the bay, the machine running) on every
  answer, so that the console finds the event again after acknowledging it, changes nothing either:
  the disc reaches the same last message and goes back to polling the channels hard (its own
  throughput drops by two thirds). Acknowledging an event is not what it is waiting for.

Where the disc actually is at that point, sampled by stopping it repeatedly and taking the PC: it is
in its normal frame loop — the SDK's SI helper calling out of `0x8006_FDB4` (the store to
`SI_COMCSR` that starts a transfer) with the SI block healthy (poll enabled for ports 0-2, manual
transfers on channel 3 for the link, every transfer completing), and the VI scanning frames out at
about a fifth of real time. Nothing is spinning on the port.

One bug did come out of this: the device's status byte had **no return on its default path** (only
the `GBP_STATUS` experiment override returned a value), so every status read the disc made was
whatever the return register happened to hold. It now returns the bay and running bits, and the whole
status block is decided rather than undefined.

The earlier note in this file said the disc repeats the identification about 400 times and never
leaves it. That was a reading of the traffic without the direction split and without the per-channel
counts. The status-byte question was also tried earlier by overriding the byte and looking at the
frame buffer: that covered eight of the 256 values and used a signal (a non-zero XFB) that a disc can
fail to produce for a dozen other reasons, so it settled neither the value nor the channel.

### The driver's state machine

The state lives at one word in the driver's small-data area and the transitions are plain in the
code (state 0 → `0x8008_B1F4`, state 1 → the block after it, errors to `status + 4`):

* **state 0** is the identification: four rounds of "fill a 32-byte block with one byte, write it to
  the mailbox at offset 0, read it back, require **byte 1** to be the complement". It runs only while
  two of the driver's own flags are clear. At its end it registers the callback the later states use
  (`0x8008_F45C`) and goes to state 1.
* **state 1** repeats the same four-round check and then runs the command routine above. A failure
  sends the state to `status + 4` (an error state); success keeps it in the exchange.
* **state 3** is the start: the handler reads `+0x400000`, sets **bit 4** in the status byte and
  writes it back (`0x8008_BD70`), then arms two of its own slots and reports 1.
* **state 2** is the other way forward: the handler reads `+0x400000` and sets **bit 3**
  (`0x8008_C2DC`), or clears bit 3 and then sets it, depending on the flags it holds.
* the driver also has three one-line predicates — "am I in state 1 / 2 / 3" — built on the same word
  (`0x8008_AD1C`, `0x8008_ACF4`, `0x8008_AD08`), which is how the rest of the module asks.

Watched live (a temporary trace inside the device reading the driver's own variables through the
CPU, since the debugger reads main memory and the console's write-back cache holds the values), the
disc goes 0 → 1 and stays in 1, cycling the command routine — which is what the exchange looks like
when the other side never answers.

### Where the disc's own code sits

A random stop puts the disc in a short loop at `0x80089D50`..`0x80089D7C`: it reads the halfword at
`0xCC00500A` (the DSP/ARAM control register), tests **bit 5** — the ARAM-DMA completion latch — and
acknowledges it. That is the *wait* half of the driver's single ARAM-DMA helper, which every window
transfer goes through, so being there only means a transfer was in flight; it does not say what the
disc is waiting for. What it waits for is the Player's answer on the command channels.
