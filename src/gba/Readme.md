# src/gba - the integrated GBA emulator

An emulator of the Game Boy Advance - and, in the same module, of the Game Boy (DMG/CGB) that the
GBA carries inside itself for its compatibility mode. It is written from the public hardware
specifications and meant to be used both as a standalone emulator and as the "portable side" of
the console (a GBA Link peer, a Game Boy Player replacement). The module is deliberately
independent of the GameCube side of pureikyubu: it includes nothing outside `src/gba/`, the C++
standard library and the emulator's shared Json engine (`src/json.h`/`src/json.cpp`, which the
settings reader uses and which is itself self-contained), so the whole core can be built and
tested without SDL, OpenGL, ImGui or the Flipper devices (`testing/gba_bench/build.sh` does
exactly that).

## The specifications this code is written from

* **ARM7TDMI**: ARM Architecture Reference Manual (ARM DDI 0100E), chapters 3 (ARM) and 4 (Thumb),
  plus the ARM7TDMI data sheet for the S/N/I cycle counts, the HALT state and the exception
  vectors.
* **GBA peripherals**: GBATEK (problemkaputt.de/gbatek.htm): the memory map, the LCD controller,
  the sound controller, DMA, the timers, the interrupt controller, the serial I/O port, the
  cartridge save memory, the GPIO/RTC port and the BIOS service functions.
* **Game Boy (DMG/CGB)**: the Pan Docs (gbdev.io/pandocs) for the machine, its cartridge mappers
  and the CGB additions, and the CPU manual for the LR35902 instruction set and its flag rules.

The sources of other emulators were not consulted (this is a hard rule of the task that produced
this module). Where the specifications disagree with what a real console does, the deviation is
written down in `testing/gba_bench/Readme.md`; the one open finding is recorded in
`testing/gba_bench/test_demo.cpp`.

## Layout

### The Game Boy Advance

| File | Contents |
|---|---|
| `gba_types.h` | Common types, the memory map constants, the interrupt bits and the colour helpers |
| `arm7tdmi.h/.cpp` | The ARM7TDMI interpreter (ARM and Thumb, all seven modes, the exceptions, HALT) |
| `gba_bus.h/.cpp` | The address decoder, the open bus, the waitstates and the system clock |
| `gba_ppu.h/.cpp` | The LCD controller: tile and bitmap backgrounds, sprites, windows, blending, scanline timing |
| `gba_apu.h/.cpp` | The four legacy channels, the two direct-sound FIFOs (clocked by the timer overflows) and the host mixer |
| `gba_audio.h` | The mixer buffer between the machine and the host's sound device: the machine pushes the samples into it, the frontend's audio callback plays them at the rate a slow controller steers from the buffer's level (the arrangement dmgemu uses in its `sound.cpp`) |
| `gba_sio.h/.cpp` | The serial port: normal, multiplayer, UART and JOY bus modes, and the link cable |
| `gba_dma.h/.cpp` | The four DMA channels, including the sound FIFO and video capture timings (a repeat reloads the word count, and DAD only for "increment + reload" - the source pointer keeps running, which is what streams a sound buffer through a FIFO) |
| `gba_timers.h/.cpp` | The four timers and their cascade |
| `gba_irq.h/.cpp` | IE/IF/IME |
| `gba_keypad.h/.cpp` | KEYINPUT/KEYCNT and the keypad interrupt |
| `gba_cart.h/.cpp` | The ROM, the save memory (SRAM/Flash/EEPROM), the GPIO/RTC port |
| `gba_hlebios.h/.cpp` | The BIOS service calls implemented in the host |
| `gba_armasm.h/.cpp` | A small ARM/Thumb emitter: the boot ROM and the test ROMs are built with it |
| `gba_bootrom.h/.cpp` | The pureikyubu boot ROM (logo animation) and its SIO link driver |
| `gba_settings.h/.cpp` | `build/Data/GBASettings.json` |
| `gba.h/.cpp` | `GbaSystem`: what a frontend talks to |
| `gba_debug.h/.cpp` | The debug interface of both machines for the new debugger (debugui2) and the JDI command line. This is the one file of the module that is compiled with the host rather than into the portable library: JDI, Markdown and the debugger live on the GameCube side, and their headers need the third-party paths the module deliberately does not have (see the header comment) |
| `gba_sdl.h/.cpp` | The SDL2 frontend of both machines (the `--gba` / `--gb` modes) |

### The Game Boy (DMG/CGB)

| File | Contents |
|---|---|
| `gb.h/.cpp` | `GbSystem`: the machine, its boot ROM choice, its link cable |
| `gb_cpu.h/.cpp` | The LR35902 (SM83) interpreter |
| `gb_ppu.h/.cpp` | The DMG/CGB LCD: the background, the window, the sprites, the CGB palettes and VRAM banks |
| `gb_apu.h/.cpp` | The four sound channels (the manual's dividers, the duty phases, the 512 Hz frame sequencer with the length at 256 Hz, the sweep at 128 Hz and the envelope at 64 Hz, the length timer, NR50/NR51 mixing and the high pass filter, which the settings can turn off) and the host mixer |
| `gb_cart.h/.cpp` | The cartridge header and the MBC1/2/3/5 mappers with battery saves |
| `gb_bus.h/.cpp` | The bus, the timer, the joypad, the serial port, OAM DMA and the CGB's double speed |
| `gb_bootrom.h/.cpp` | The emulator's own 256-byte DMG/CGB boot ROM (the wordmark slides in) |
| `gb_asm.h/.cpp` | The LR35902 emitter that builds the boot ROM and the test ROMs |

## How a frame runs

`GbaSystem::RunFrame` runs 228 scanlines of 1232 cycles:

```
for each scanline
    RenderLine(y)                 // the whole scanline is composed at once (see the note below)
    HBlank interrupt, DMA (HBlank), timers
VBlank interrupt, DMA (VBlank), the frame counter is bumped
```

The CPU and the devices share one clock: `GbaSystem::RunCycles` takes one `Arm7tdmi::Step`, adds
the waitstates the bus accumulated, and hands the total to `GbaBus::Tick`, which advances the
timers, the LCD, the serial port, the sound and the DMA.

### The sound path

The APU mixes one sample per 512 system cycles (32768 Hz, the rate the GBA mixes at) into its own
queue, and the frontend drains that queue once per frame into the mixer buffer (`gba_audio.h`) and
lets the host's sound device play it back from its audio callback (`gba_sdl.cpp` opens the device
with a callback, exactly as dmgemu does).

The **machine keeps its own clock and the mixer follows it**: the frame loop runs one frame per
iteration and `PaceFrame` holds it to the machine's own 59.7275 Hz frame period (the display's
refresh only adds its own limit on top), so the emulation's speed never follows the sound device's
crystal. The buffer is kept at a cushion of about three video frames (50 ms) by playing it back at a
slightly different rate: `AudioBuffer::Play` resamples by the rate `UpdateClock` steers from the
buffer's level, so the fraction of a percent by which the two clocks disagree - a 60.00 Hz display
against the GBA's 59.7275 is 0.46 %, i.e. 8 cents - costs neither a growing delay nor dropped
samples. The steering is a slow PI controller on a *filtered* level (`ClockCorrection`): the device
plays a whole callback period at a time, so the raw level saws up and down at the beat of the two
clocks, and a controller that answers that sawtooth instead of the level swings the playback rate
several percent between one frame and the next - heard as a rattle, with the excess audio thrown
away on top of it. A buffer that has run dry - the host stalled, a frame took far too long - is
refilled with a few extra frames in the same iteration, because a machine in step with the device
only makes up one frame's worth of audio per frame; and a period the device asks for that the buffer
cannot fill is silence rather than stale samples. The delay therefore stays at the cushion, whatever
the display and the host do. The window title reports the delay, the rate correction and the
counters when `showFps` is on.

The DMG/CGB APU's output high pass filter (see `gb_apu.h`) can be turned off with the settings
file's `audio.highPassFilter`; off, `ReadAudio` hands back the DACs' raw sum.

**Deviation worth knowing**: the LCD renders a whole scanline when its HBlank starts instead of
composing it dot by dot during the visible part. A game that rewrites VRAM (or the scroll
registers) *inside* the visible line therefore sees the change one line earlier than on hardware.
This is the usual simplification; it is recorded in `testing/gba_bench/Readme.md` together with
the other intentional deviations.

## The boot ROM

The real IPL is copyrighted and is not in this repository. `gba_bootrom.cpp` builds a free
replacement with the emitter in `gba_armasm.h`:

* a rotating wireframe hypercube that collapses into the flat pureikyubu cube mark, with the
  "pureikyubu" wordmark scrolling in underneath (the animation the task asked for);
* after the animation, the ROM checks for a cartridge and jumps to 0x08000000, or, when there is
  none, starts the SIO link driver and serves the link/multiboot protocol, which is what the
  "GBA Link" mode needs.

`--dump-bootrom <file>` in the harness writes both the 16 KByte image and its assembly listing so
the ROM can be reviewed without a disassembler.

## Building and testing

```
wsl -e bash -lc "cd /mnt/c/Work/pureikyubu && testing/gba_bench/build.sh"
wsl -e bash -lc "/tmp/gbabench/gba_test"                       # the unit tests
wsl -e bash -lc "/tmp/gbabench/gba_test --bootrom --frames 240 --png /tmp/shots"
wsl -e bash -lc "/tmp/gbabench/gba_test --run game.gba --frames 600 --bench"
```

On Windows the same sources build as the `GBA` project of `scripts/VS2026/pureikyubu.sln` (a
library the emulator links, exactly like SDL2) and as the `gba_bench` project of the same solution;
see the "GBA emulator" section of `testing/Readme.md`.
