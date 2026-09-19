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

## Sound

The sound controller (`src/gba/gba_apu.cpp`) mixes the four legacy channels and the two direct-sound
FIFOs at the GBA's own 32.768 kHz into a queue, and the frontend (`gba_sdl.cpp`) drains that queue
once per frame into the mixer buffer of `src/gba/gba_audio.h`. The host's sound device plays the
buffer from its **audio callback** (SDL is opened with a callback, which is how dmgemu does it), so
the machine *pushes* its samples and the device *pulls* them: nothing has to be polled, and the
delay of the sound cannot grow the way it does when the frontend only drains the core when a queue
looks short.

The buffer is kept at a cushion of about three video frames (50 ms at 32768 Hz) and the **machine
keeps its own clock, with the mixer following it**: the frame loop runs one frame per iteration and
`PaceFrame` holds it to the machine's own 59.7275 Hz frame period (the display's refresh only adds
its own limit on top), so the emulation's speed never follows the sound device's crystal. The two
clocks are reconciled by playing the buffer back at a slightly different rate - `AudioBuffer::Play`
resamples by the rate `UpdateClock` steers from the buffer's level - so the fraction of a percent by
which they disagree (a 60.00 Hz display against the GBA's 59.7275 is 0.46 %, i.e. 8 cents) costs
neither a growing delay nor audio thrown away sample by sample. The steering is a slow PI controller
on a *filtered* level (`ClockCorrection`): because the device takes a whole callback period at a
time, the raw level saws up and down at the beat of the two clocks, and a controller that answers
that sawtooth instead of the level swings the playback rate several percent between neighbouring
frames - a rattle, with the excess audio dropped on top of it. A buffer that ran dry (the host
stalled, a frame took far too long) is refilled with a few extra frames in the same iteration - a
machine in step with the device only makes up one frame's worth of audio per frame - and a callback
period the buffer cannot fill is silence rather than a repeat of the samples before it. The delay,
the rate correction and the counters are part of the window title when `video.showFps` is on.

The mixer's levels are the hardware's (GBATEK "Max Output Levels"): each of the four PSG channels
spans a quarter of the output range and each FIFO the whole of it, so a direct sound channel is four
times a PSG channel and a loud mix is clipped, exactly as the GBA's 10 bit output clips it. The
legacy channels follow the AGB Programming Manual: the (64 - st) / 256 s lengths, the n / 64 s
envelope, the n / 128 s sweep, the 131072 / (2048 - x) Hz tone, and the waveform RAM's two banks,
one of which plays while the CPU sees the other.

A FIFO byte is moved by a **timer overflow** (SOUNDCNT_H bits 10/14 select timer 0 or 1), and the
timer can be faster than the host's sample rate. The mixer counts the timer's wraps
(`Timers::Overflows`) instead of comparing two readings of its counter, so a FIFO clocked at
44.1 kHz or at 65 kHz keeps its rate rather than losing the overflows that fall between two host
samples.

## Settings

`build/Data/GBASettings.json`:

| Section | Members |
|---|---|
| `info` | a description of the file |
| `boot` | `biosPath`, `useCustomBootRom`, `skipBootAnimation`, `hleBios` |
| `video` | `videoScale`, `fullscreen`, `vsync`, `integerScale`, `showFps`, `frameSkip` |
| `audio` | `audioEnabled`, `sampleRate`, `volume`, `highPassFilter` |
| `input` | the eleven bindings (`A`, `B`, `SELECT`, `START`, `RIGHT`, `LEFT`, `UP`, `DOWN`, `R`, `L`, `SPEED`) |
| `link` | `linkEnabled`, `linkServer`, `linkAddress`, `linkPlayers` |
| `emulation` | `rtcEnabled`, `bootWithNoCartridge`, `debugger`, `saveDirectory`, `logLevel` |

The file is read with the emulator's shared Json engine (`src/json.cpp`), the same one the GameCube
side uses. The engine is self-contained (the C++ standard library and `verify.h` only), so the GBA
core still builds without the GameCube side of the emulator and links the engine instead of
carrying a parser of its own. A malformed file is rejected with a message that names the line and
the built-in defaults are used; the shipped file, `GbaSettings::DefaultJson()` and a round trip of
the defaults are byte-identical, and the test suite asserts that.

`emulation.debugger` (false by default) opens the debugger window together with the machine. The
portable debug interface - the JDI node and the MCP transport - comes up either way, and `F2` opens
and closes the window at any time.

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
  LFSR) driven by the manual's dividers (the pulse divider at 1048576 Hz, the wave divider at
  2097152 Hz and the LFSR at 262144 / (divisor * 2^shift) Hz), with the 512 Hz frame sequencer
  clocking the length at 256 Hz, the sweep at 128 Hz and the envelope at 64 Hz, NR50's master
  volume, NR51's routing and the console's own high pass filter (the CGB's is more aggressive than
  the DMG's; `audio.highPassFilter` in the settings turns the filter off), mixed down to the host's
  sample rate;
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
