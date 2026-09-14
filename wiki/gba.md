# GBA

The Game Boy Advance emulator that lives inside the emulator (issue
[#388](https://github.com/emu-russia/pureikyubu/issues/388)). It is a separate machine with its own
CPU, its own memory map and its own frontend; the GameCube side of pureikyubu does not take part in
it. The reason it exists is the use case the issue describes: a GBA is needed both as a
**GBA Link** peer (the cable between a Game Boy Advance and a GameCube) and as a stand-in for the
**Game Boy Player**, so the emulator has to be able to run GBA code, drive the link port, and boot
with no cartridge at all.

## Starting it

```
pureikyubu --gba game.gba        # a cartridge, with the boot ROM animation first
pureikyubu --gba                 # no cartridge: the boot ROM's link driver takes over
pureikyubu game.gba              # the extension selects the mode by itself
pureikyubu --gba --gba-bios my_bios.bin
pureikyubu --gba --no-gba-bootrom    # skip the BIOS entirely and start the cartridge
```

`--gba-link` initializes the link port even when a cartridge is loaded. The window and the sound
come from SDL2 (the same library the rest of the emulator uses); the input mapping, the scale, the
sample rate and the link options live in `build/Data/GBASettings.json`.

## The boot ROM

The official IPL is copyrighted and is not in this repository, so `src/gba/gba_bootrom.cpp` builds
a free replacement from source with the emitter in `src/gba/gba_armasm.h`:

* a rotating wireframe hypercube that collapses into the flat pureikyubu cube mark, with the
  **pureikyubu** wordmark scrolling in underneath it;
* after the animation, the header of the cartridge is checked and the boot ROM jumps to
  `0x08000000` with the machine in the state the real BIOS leaves behind (System mode, the three
  stack pointers, `POSTFLG` = 1);
* with no cartridge (or with a header that does not check out), the **SIO link driver** starts
  instead: the port is put into multiplayer mode with the transfer interrupt enabled and the driver
  serves the link handshake, publishing its state in IWRAM so a peer (or a test) can see it. This
  is the state "GBA Link" mode runs in.

The image can be dumped for review, with its assembly listing:

```
testing/gba_bench/check.sh --dump-bootrom /tmp/gba_bootrom.bin
```

A real BIOS image can be used instead of the boot ROM (`--gba-bios`); the emulator then executes
the actual BIOS code, and the BIOS service calls (`SWI`) are executed by it rather than by the
host. The host-side implementations (`src/gba/gba_hlebios.cpp`) cover the calls a game can make
without a BIOS image: `SoftReset`, `RegisterRamReset`, `Halt`, `Stop`, `IntrWait`,
`VBlankIntrWait`, `Div`, `DivArm`, `Sqrt`, `ArcTan`, `ArcTan2`, `CpuSet`, `CpuFastSet`,
`BgAffineSet`, `ObjAffineSet`, `BitUnPack`, the LZ77/run-length decompressors, the two difference
filters, `SoundBias`, `MidiKey2Freq`, `MultiBoot` and the sound-driver stubs.

## The link port

`src/gba/gba_sio.cpp` implements the four modes of the serial port from GBATEK: normal 8/32-bit
(two players, internal or external clock), multiplayer (up to four players, 16 bits per slot),
UART, and the general purpose mode. Two `Sio` objects can be plugged into each other
(`GbaSystem::AttachLink`) - that is the emulated link cable, and the unit tests drive a two-player
transfer and a four-slot multiplayer transfer through it. The JOY bus (`RCNT` bit 15), which is how
a Game Boy Player talks to a GameCube controller, is decoded but not driven.

## Settings

`build/Data/GBASettings.json`:

| Section | Members |
|---|---|
| `info` | a description of the file |
| `boot` | `biosPath`, `useCustomBootRom`, `skipBootAnimation`, `hleBios` |
| `video` | `videoScale`, `fullscreen`, `vsync`, `integerScale`, `showFps`, `frameSkip` |
| `audio` | `audioEnabled`, `sampleRate`, `volume` |
| `input` | the eleven bindings (`A`, `B`, `SELECT`, `START`, `RIGHT`, `LEFT`, `UP`, `DOWN`, `R`, `L`, `SPEED`) |
| `link` | `linkEnabled`, `linkServer`, `linkAddress`, `linkPlayers` |
| `emulation` | `rtcEnabled`, `bootWithNoCartridge`, `saveDirectory`, `logLevel` |

The file is read by a small self-contained JSON reader inside the GBA module (the core must build
without the GameCube side of the emulator, and therefore without its `Json`/JDI machinery). A
malformed file is rejected with a readable message and the built-in defaults are used; the
shipped file, `GbaSettings::DefaultJson()` and a round trip of the defaults are byte-identical,
and the test suite asserts that.

## Tests

The GBA core has no dependency on SDL, OpenGL or ImGui, so its tests are a standalone console
harness rather than a CppUnitTest DLL:

```
testing/gba_bench/check.sh                 # every unit test
testing/gba_bench/check.sh --demo          # run the demo cartridge the harness assembles
testing/gba_bench/check.sh --bootrom --frames 300 --png /tmp/shots
testing/gba_bench/check.sh --run game.gba --frames 600 --bench
```

See `testing/Readme.md` for the file list and `testing/gba_bench/Readme.md` for the deviations the
tests pin down.

## The Game Boy machine

The GBA runs Game Boy cartridges only because the console carries an older machine inside it, so
that machine is part of this module too (`gb_*.cpp`, `GB::GbSystem` in `src/gba/gb.h`):

* the **LR35902 (SM83)** CPU with the whole instruction set and the flag rules of the CPU manual,
  including the `ADD SP,e8`/`LD HL,SP+e8` flag quirk, DAA, the interrupt round trip with the EI
  delay, HALT and the CGB's double speed;
* the **DMG/CGB LCD**: the background, the window at WX-7, 8x8 and 8x16 sprites with the
  10-per-line limit and the X-priority rule, the DMG's palette registers and the CGB's eight
  background and eight sprite palettes with the VRAM attribute map (flips, tile bank, BG-to-OAM
  priority), the STAT/LY/LYC interrupts and the CGB's HDMA/GDMA;
* the **four sound channels** (two squares with sweep and envelope, the wave channel, the noise
  LFSR) and a host-rate mixer;
* the **cartridge**: the header and the MBC1/2/3/5 mappers with battery-backed RAM written to a
  `.sav` next to the ROM;
* a **free 256-byte boot ROM** built from source by the LR35902 emitter in `gb_asm.cpp`, in which
  the "pureikyubu" wordmark slides into the middle of the screen before the cartridge starts (a
  real boot ROM can be supplied instead, and the cartridge can also be started directly).

The console kind follows the cartridge's CGB flag; a DMG cartridge runs on a CGB in compatibility
mode. In the frontend a `.gb`, `.gbc` or `.sgb` file selects this machine:

```
pureikyubu game.gbc
pureikyubu --gb --gb-dmg game.gb      # force the monochrome console
```

The window and audio options come from the same `GBASettings.json`; the Game Boy has eight
buttons and no bindings of its own, so its layout is fixed: the arrow keys, `Z` = A, `X` = B,
`Return` = Start, `Backspace` = Select (plus a game controller's A/B/Start/Back/d-pad).

## Tests

The GBA core has no dependency on SDL, OpenGL or ImGui, so its tests are a standalone console
harness rather than a CppUnitTest DLL:

```
testing/gba_bench/check.sh                 # every unit test (GBA and Game Boy)
testing/gba_bench/check.sh --demo          # run the demo cartridge the harness assembles
testing/gba_bench/check.sh --bootrom --frames 300 --png /tmp/shots
testing/gba_bench/check.sh --run game.gba --frames 600 --bench
testing/gba_bench/check.sh --run game.gbc --gb
```

See `testing/Readme.md` for the file list and `testing/gba_bench/Readme.md` for the deviations the
tests pin down.

## What is implemented, and what is not

The Game Boy **is** implemented and runs cartridges (see the section above): it is a machine of its
own in the same module, selected by a `.gb`/`.gbc`/`.sgb` file or by `--gb`. What is *not*
implemented is the GBA's own Game Boy compatibility path - a Game Boy cartridge in the **GBA's**
slot, borrowing the GBA's LCD and sound hardware - so a Game Boy cartridge is always run by the
Game Boy core rather than by the GBA one, and the GBA's cartridge slot does not take one.

Also not implemented:

* **The JOY bus** (a GameCube controller on the link port) and the Game Boy Player's own boot
  protocol. The emulator-to-emulator link cable *is* implemented and tested on both machines.
* **The BIOS sound driver** (`SWI 0x28`-`0x2F`) and the Huffman decompressor: reported and
  returned from, not implemented.
* **Save states and rewind**; the battery-backed save memory (SRAM/Flash/EEPROM on the GBA,
  the mapper RAM on the Game Boy) *is* implemented and written to a `.sav` next to the ROM.
* **Cycle-exact LCD timing**: a scanline is composed when its HBlank starts rather than dot by
  dot, so a game that rewrites VRAM inside the visible part of a line sees the change one line
  early. On the Game Boy side a line is composed at the end of mode 3 as well, and a few DMG-only
  quirks (the WX=166 wraparound, the dropped leftmost sprite pixel and the spurious STAT write
  interrupt) are not modelled. The full list of deviations is in
  `testing/gba_bench/Readme.md`, together with the open findings (the official GBA BIOS executes
  but does not finish its boot yet, so the emulator's own boot ROM is what boots cartridges, and a
  Game Boy Color game's picture is not trustworthy yet).
