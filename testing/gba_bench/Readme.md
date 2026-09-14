# testing/gba_bench - the GBA core harness

`gba_test` is three tools in one binary:

* the **unit tests** of the integrated GBA emulator (`GBA_TEST` registration, see `gba_test.h`), run
  with no arguments (or with a substring filter: `gba_test Ppu`);
* the **ROM harness** that made the core debuggable while it was written: it boots a cartridge (or
  the emulator's own boot ROM, or a demo cartridge it assembles itself) headlessly, prints a hash of
  every frame, dumps frames as PNG, measures the emulation speed and can plug two instances into
  each other through the emulated link cable;
* the module's **debugging tools**: the ARM/Thumb and SM83 disassemblers of `src/gba/gba_disasm.cpp`
  and `src/gba/gb_disasm.cpp`. `--disasm-*` lists an instruction stream (a BIOS, a cartridge, any
  file), and `--trace N` runs the last frame instruction by instruction and prints the last N
  instructions with the register file, which is what answers "what is this program doing now" for a
  machine that seems stuck.

```
check.sh                          # build and run every test
check.sh Ppu                      # one suite
check.sh --bootrom --frames 240 --png /tmp/shots
check.sh --demo --frames 120 --png /tmp/demo
check.sh --run roms/arm.gba --frames 120 --png /tmp/arm
check.sh --dump-bootrom /tmp/gba_bootrom.bin
check.sh --link-test
check.sh --bootrom --frames 300 --wav /tmp/boot.wav          # listen to the boot animation
check.sh --bootrom --bios bios/gba_bios.bin --frames 900 --wav /tmp/bios.wav
check.sh --disasm-arm bios/gba_bios.bin 0x340 10             # read the BIOS's Halt loop
check.sh --disasm-thumb roms/metroid_fusion.gba 0xA00 20     # a cartridge's own code
check.sh --disasm-gb roms/zelda_ladx.gbc 0x150 20            # the Game Boy's code
check.sh --run game.gba --frames 200 --trace 60              # what the last frame executed
```

The core is compiled straight from `src/gba` (the SDL frontend `gba_sdl.cpp` is the only file left
out: the core itself has no SDL dependency). `get_test_roms.sh` fetches three public, MIT-licensed
GBA test ROMs (`jsmolka/gba-tests`) and the two Game Boy Acid2 pictures (`dmg-acid2`, `cgb-acid2`)
that the harness can be pointed at; they are not part of the repository. A **real BIOS image** can be
dropped into `bios/` (also git-ignored) to run the tests that compare the emulator against the
official IPL.

## What the tests cover

| Suite | What it drives |
|---|---|
| `Disasm` | The two disassemblers: the ARM data processing, transfer, block transfer, branch and miscellaneous encodings, the Thumb formats (including the two halfword long branch) and the SM83 opcode and CB maps, each checked against the encoding table it was written from and, for ARM, against the bytes of the real BIOS it is used on |
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
* **The Game Boy's picture is composed one line at a time**, like the GBA's, so a Game Boy program
  that changes a register *inside* the visible part of a line sees the change one line early. The
  two Acid2 images (`get_test_roms.sh`: `dmg-acid2.gb`, `cgb-acid2.gbc`) are the end-to-end check
  of that picture: the monochrome one (background, window, objects, their priorities, the object
  8x16 size and the flips) comes out as the reference drawing, and the colour one draws the right
  colours, the right background and the right objects, but its nose, its diagonal and the ring of
  its right eye - the parts the test moves around to catch mid-line behaviour - still differ from
  the reference.

## The test suite

Every test passes: **245 of 245**. The suite was not green while the core was being written, and
every failure was resolved by fixing the emulator or by correcting an expectation that did not
match GBATEK - never by deleting a test. The failures are listed here because each one names a real
bug that is easy to reintroduce.

| Test that used to fail | What it turned out to be |
|---|---|
| `Dma.FifoRefillCompletesWhenTheApuAsks` | A FIFO-mode DMA collected its four words but never handed them to the APU (`Apu::FifoDmaDone`), and it used the 16-bit unit size for a transfer the hardware always performs 32bits wide. Sound DMA produced no sound at all. |
| `Ppu.Sprite256Colors`, `Ppu.AffineSpriteIdentityMatrix` | The 1-dimensional OBJ tile mapping stepped the tile rows by a fixed 16 tiles instead of the OBJ's own width, an affine OBJ scaled its matrix parameters by 256, and it was placed with the reference point in its middle instead of at its upper-left. |
| `Ppu.SpriteHorizontalFlip` | Attribute 1 bits 12/13, the flips of a non-affine OBJ, were not implemented at all. |
| `Ppu.AffineBgRotationAndReferenceAdvance` | The affine renderer sampled the write latch instead of the internal reference register (throwing the per-line PB/PD advance away), the advance itself multiplied PB/PD by 256, and the source X subtracted the scanline number. |
| `Ppu.AlphaBlendingOfTwoBgs`, `Ppu.SemiTransparentSpriteAlphaBlendsWithTheBg` | Alpha blending required the top pixel to be a 2nd target as well, which is not what GBATEK 4000050h asks for: only the next-lower pixel has to be one, so BG-to-BG blending never happened. |
| Every sprite test | The bus assembled an OAM halfword out of two `Ppu::WriteOam` byte writes, and that accessor drops the high byte (OAM has no 8-bit write access). Every sprite attribute above bit 7 - tile number, size, 256-colour flag, priority, flips, the affine parameters - was lost. |
| `Ppu.ForcedBlankIsWhite`, `Ppu.WindowMasksOneBg`, `Ppu.TextBg8bpp`, `Ppu.PriorityFightBetweenTwoBgs`, `Ppu.TextBgTileMapWrap512`, `Ppu.VCountMatchInterrupt`, `Ppu.VramWindowsPerMode`, `Ppu.BrightnessDecreaseOfTheBackdrop` | Expectations that did not follow the hardware: the forced blank and the green swap did not reach the line buffer, a window row that is still inside the window, a palette entry written to the wrong offset, a background map that collided with its own tile data, a V-Count match line the test's loop never reached again, the VRAM mirroring formula, and the truncation order of the brightness decrease. |
| The colour half of `cgb-acid2` (found by running the ROM, not by a unit test) | `GbBus::WriteIo`/`ReadIo` passed the PPU registers `0xFF40-0xFF4B` through but not the CGB palette registers `0xFF68-0xFF6B`, so a colour game could not define a single colour: the whole picture - sprites included, which is why they seemed to be missing - stayed on the grey ramp the machine installs at reset. `GbBus, cgb_palette_registers_are_reachable_through_the_bus` and `GbPpu, cgb_sprites_use_the_object_palette_and_the_tile_bank` now cover the path. |
| `Apu.DutyWaveform` and every later test (a crash, not a failure) | The tests drained `Apu::ReadSamples` into an `s16 buffer[128]`, but the call hands over `maxFrames` *stereo* frames and writes two samples per frame: the 256 byte stack buffer was overflowed and the run died later, in the middle of another suite. Found with AddressSanitizer; the buffers are sized for stereo frames now. |

Everything else passes: the ARM7TDMI (60 tests), the Game Boy machine (63 with its boot ROM and its
colour palette path), the LCD controller (25), the sound (22), the settings (19), the cartridge (14),
the boot ROM and the emitter (12), the DMA (7), the SIO (7), the timers/keypad/interrupts (9), the
demo machine (5) and the BIOS harness (2).

## Open findings

These are things the tests look at and report, but do not (yet) pin down as correct behaviour.
They are recorded so that they are not mistaken for verified behaviour, and so that the next person
knows where to look.

* **The real BIOS runs its boot but its picture does not appear yet.** With an official
  `gba_bios.bin` in `bios/`, the CPU starts in the BIOS (and the host-side BIOS calls are switched
  off automatically, as they must be: a real BIOS with intercepted service functions is not a real
  BIOS), it runs its initialization, sets `POSTFLG`, programs `DISPCNT`, fills the sound registers
  (`SOUNDCNT_X = 0x80`, `SOUNDCNT_H = 0x210E`, `SOUNDBIAS = 0x4200`, `IE = 0x0081`), arms DMA1/DMA2
  for the sound FIFOs (`CNT_H = 0xB600`), and then drives the boot animation from its own Thumb code
  (the routine around `PC = 0x2B48`, which manipulates `DISPCNT` and the animation counter, and the
  helper at `0x2D5C` it calls). With a **valid** cartridge the animation's data really is
  decompressed into VRAM (682 bytes of OBJ tiles and 698 of BG data with Metroid Fusion, where the
  harness's own marker ROM - an invalid header - leaves them empty, which is why the BIOS used to
  look like it never got there at all) and the animation's palettes and sprites are programmed.

  What is still missing is the picture itself: the animation is drawn as a mode 2 background on
  **BG3** (256 colours, screen base 23, size 1, area overflow) that is only visible **inside the OBJ
  window**, which nine mode 2 sprites of 32x64/64x64 dots form, with the sprite palette, `WINOUT =
  0x3F27` and alpha blending around it - and the frame stays the backdrop colour. The `Disasm`
  suite and `--trace` (see above) are what made this readable, and the OBJ window had no test until
  now: `Ppu.ObjWindowMasksTheLayers` covers it and found that the window stamp of one scanline
  survived into the next. The real BIOS's boot therefore still waits for the picture path to be
  right, and the emulator's own boot ROM (and the HLE boot, `--no-gba-bootrom`) is what boots
  cartridges today. `test_bios.cpp` asserts what is certainly true and reports the rest.
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
