# testing/gba_bench - the GBA core harness

`gba_test` is two tools in one binary:

* the **unit tests** of the integrated GBA emulator (`GBA_TEST` registration, see `gba_test.h`), run
  with no arguments (or with a substring filter: `gba_test Ppu`);
* the **ROM harness** that made the core debuggable while it was written: it boots a cartridge (or
  the emulator's own boot ROM, or a demo cartridge it assembles itself) headlessly, prints a hash of
  every frame, dumps frames as PNG, measures the emulation speed and can plug two instances into
  each other through the emulated link cable.

```
check.sh                          # build and run every test
check.sh Ppu                      # one suite
check.sh --bootrom --frames 240 --png /tmp/shots
check.sh --demo --frames 120 --png /tmp/demo
check.sh --run roms/arm.gba --frames 120 --png /tmp/arm
check.sh --dump-bootrom /tmp/gba_bootrom.bin
check.sh --link-test
```

The core is compiled straight from `src/gba` (the SDL frontend `gba_sdl.cpp` is the only file left
out: the core itself has no SDL dependency). `get_test_roms.sh` fetches three public, MIT-licensed
test ROMs (`jsmolka/gba-tests`) that the harness can be pointed at; they are not part of the
repository. A **real BIOS image** can be dropped into `bios/` (also git-ignored) to run the tests
that compare the emulator against the official IPL.

## What the tests cover

| Suite | What it drives |
|---|---|
| `Cpu` | The ARM7TDMI: the data processing family with the flags and the barrel shifter, multiplies, the load/store family, the block transfers, the branch family, all the Thumb formats, the mode banking, the exceptions, HALT |
| `Ppu` | The LCD: text/affine/bitmap backgrounds, sprites, windows, blending, forced blank, the scanline timing and the HBlank/VBlank/VCount interrupts |
| `Apu` | The four legacy channels, the wave RAM, the noise LFSR, the two FIFOs with their DMA requests, the mixer and the sample rate |
| `Timers`, `Dma`, `Sio`, `Keypad`, `Irq` | The four timers and the cascade, the four DMA channels and their start timings, the serial port in normal and multiplayer mode between two attached machines, the keypad conditions, IE/IF/IME |
| `Cart` | The header, the save-type detection, SRAM, the Flash command set, the EEPROM protocol, the GPIO/RTC port, the `.sav` round trip |
| `BootRom` | The ARM emitter against the ARM Architecture Reference Manual, the boot ROM image, the animation and the cartridge handover |
| `Demo` | A whole cartridge assembled at run time and run on the whole machine |
| `Bios` | The official IPL, when the user has one (skipped otherwise) |
| `Settings` | `build/Data/GBASettings.json`: the defaults, the round trip, the shipped file matching the code, the malformed documents |

## Deviations the tests pin down

These are places where the emulator deliberately simplifies the hardware. They are recorded here so
that they are not mistaken for verified behaviour. Where a test depends on one, the test says so.

* **The LCD composes a whole scanline when its HBlank starts** instead of drawing it dot by dot
  during the visible part of the line. A game that rewrites VRAM or the scroll registers *inside*
  the visible part sees the change one line earlier than on hardware.
* **The internal RAM's own access times are not modelled.** The CPU's cycle counts are exact and
  the cartridge's waitstates follow WAITCNT, but EWRAM (a 16-bit bus) and IWRAM (32-bit) are
  treated as zero-wait. The cartridge prefetch buffer is modelled as a waitstate rule rather than
  as a real 8-halfword buffer.
* **DMA charges its stolen cycles to the next slice** of the system clock instead of interleaving
  with the CPU instruction that started it, and a DMA transfer runs to completion in one go. The
  word counts, the address adjustments and the start timings themselves follow GBATEK.
* **The BIOS service functions are implemented in the host** (`gba_hlebios.cpp`) from GBATEK, not
  executed from a BIOS image. `ArcTan`/`ArcTan2` use the host's `atan2` rather than the BIOS's
  table, `GetBiosChecksum` returns a checksum of the emulator's own BIOS, and the calls that need
  the BIOS's private RAM (`HuffUnComp`, `MultiBoot` and the sound driver entry points) are reported
  and returned from without doing the work.
* **The serial port's JOY bus mode** (`RCNT` bit 15, a GameCube controller on the link port) is
  decoded but not driven; UART mode works register-wise but has no peer to talk to. The
  emulator-to-emulator cable (normal and multiplayer modes) is implemented and tested.
* **Palette/VRAM byte access** follows the hardware's byte lanes: a byte store is duplicated into
  both lanes of the halfword everywhere except OAM and the VRAM window above 0x10000, where the
  hardware drops it (the `memory` test ROM checks all four cases), and a *palette* byte read has
  the hardware's OR rule.
* **VRAM is 96 KByte of linear memory mirrored through a 128 KByte window**, with no "unused"
  windows in the bitmap modes; the object tile base only differs for the renderer (0x14000 in the
  bitmap modes, 0x10000 in the tile modes).
* **Save states, rewind, the EEPROM's "last byte is the AND of the old and new value" quirk, and
  the real-time clock's per-minute interrupt register** are not implemented.
* **The Game Boy (DMG/CGB) machine** is a separate core (`src/gba/gb_*.cpp`) with its own tests
  (`test_gb_*.cpp`) and its own harness mode (`--gb`); the GBA in Game Boy compatibility mode does
  not run it, and the GBA cartridge slot does not accept a Game Boy cartridge.

## Known failing tests

The suite is not green at the time of writing: 223 of 243 tests pass. The failures are listed here
with the honest state of each, because a red test is only useful if it is understood.

| Test | State |
|---|---|
| `Dma.ImmediateTransferWithIncrementDecrementAndFixed` | The DMA0/DMA1 parts pass; the DMA2 fixed-destination scope fails although the transfer itself is verified to write the right value (the later read-back in the test sees zero). Belongs to `test_io.cpp`'s scope setup, or to `Dma::Perform`'s register restore - not yet diagnosed. |
| `Dma.FifoRefillCompletesWhenTheApuAsks` | The DMA side calls `Apu::FifoDmaDone` with the four words and clears the request (verified by its author in a standalone harness); the integrated failure needs the same look. |
| `Sio.NormalMode32BitTransferExchangesData` | The 32-bit normal-mode receive value is wrong for the end that finishes first (the deferred commit path). A real gap in `gba_sio.cpp`; the 8-bit form and the empty-cable case pass. |
| `Sio.MultiplayerEmptySlotsReadFFFF` | A parent slot reads the peer's word instead of 0xFFFF for an unconnected slot. A real gap in the multiplayer slot logic. |
| `Sio.TransferLengthsMatchTheSpecifiedBaudRate` | `Busy()` is not reported right after the start write in that test. |
| 15 `Ppu.*` tests (e.g. `TextBg8bpp`, `Sprite256Colors`, `AlphaBlendingOfTwoBgs`, `VCountMatchInterrupt`) | Their author's own report: the renderer implements the features (tile and bitmap backgrounds, sprites, windows, mosaic, blending - the boot ROM animation, the demo cartridge and the public test ROMs all render correctly through them), but these tests' setups collide inside one 96 KByte of VRAM (the same character/screen base reused by two cases) or target the wrong dot, so the expectations - not the emulator - are the problem. They are kept failing rather than weakened. |

Everything else passes: the ARM7TDMI (60 tests), the sound (23), the cartridge (14), the boot ROM
and the emitter (12), the timers/keypad/interrupts (12), the demo machine (5), the settings (19),
the Game Boy machine (61) and the memory-suite-driven bus rules.

## Open findings

These are things the tests look at and report, but do not (yet) pin down as correct behaviour.
They are recorded so that they are not mistaken for verified behaviour, and so that the next person
knows where to look.

* **The real BIOS executes but does not finish its boot.** With an official `gba_bios.bin` in
  `bios/`, the CPU starts in the BIOS, the BIOS sets `POSTFLG` and programs `DISPCNT`, and then it
  settles into a loop at `PC = 0x348` (with `DISPCNT` alternating between 0x1002 and 0x9802) and
  never reaches the health screen or the cartridge. The emulator's own boot ROM (and the HLE boot,
  `--no-gba-bootrom`) is what boots cartridges today. `test_bios.cpp` asserts what is certainly
  true and reports the rest.
* **Real Game Boy Color cartridge pictures are not trustworthy yet.** The DMG/CGB machine boots,
  runs the emulator's own boot ROM (whose "pureikyubu" wordmark slides in and settles - the
  documentation image), passes all 61 of its tests, and runs Link's Awakening DX with the right
  header (MBC5, 1 MByte, CGB enhanced) while the LCD, the interrupts and the VRAM writes are all
  alive; at several probed moments its frame matches VRAM exactly, but at others the picture is
  not right. Its author's reading is that the remaining difference is in the mode-3 line length
  (a line is composed at once rather than dot by dot), the HBlank/HDMA timing, and the
  PPU-internal VRAM read-block windows. Treat a colour game's picture as work in progress.

## Findings the tests produced and that are now fixed

Recorded because they are the reason several rules in the emulator look the way they do, and
because each one was found by an independent check rather than by reading the emulator:

* **VRAM mirroring.** The region mirrors every 128 KByte *and* the 96 KByte of real memory wrap
  inside it; the old `& 0x17FFF` mask dropped bit 15 and folded the second 32 KByte onto the
  first. This cut every mode 3 screen at row 136 (the boot ROM's gradient, the demo cartridge's
  picture) and made the demo's paint look four rows out of step with its own formula.
* **The byte-store rules of the video memory** (found by the `memory` test ROM): an 8-bit store is
  ignored for OAM and for VRAM above 0x10000, and duplicated into both byte lanes everywhere else.
* **The register-shift-by-zero carry rule** (found by the ARM and Thumb test ROMs): a shift whose
  amount comes from a register passes the operand through and leaves C alone, unlike the immediate
  encodings where a field of 0 means 32 (LSR/ASR) or RRX (ROR).
