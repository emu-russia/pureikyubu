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
out: the core itself has no SDL dependency), together with the emulator's shared Json engine
(`src/json.cpp`, which the settings reader uses; it is self contained and needs nothing from the
GameCube side). `get_test_roms.sh` fetches three public, MIT-licensed
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
| `GbApu` | The Game Boy (DMG/CGB) sound controller: the divider rates of the four channels against the manual's frequency formulas, the duty waveform phases, the length timer, the 512 Hz frame sequencer with the envelope at 64 Hz and the sweep at 128 Hz, the wave RAM access order and volume shifts, the noise LFSR sequence, NR50/NR51 mixing, the exact sample clock, PCM12/PCM34, the CGB's high pass filter and the setting that turns it off |
| `GbPpu` | The Game Boy's LCD: the mode timings in the machine's own clocks (one clock to a dot, 456 to a line), text and CGB colour backgrounds with the attribute map, sprites with their priorities, the window, the STAT/LYC interrupts, the LCD-off rules |
| `Audio` | The mixer buffer between the machine and the sound device (`src/gba/gba_audio.h`): the push/play round trip and the ring wrap, the cushion the clock correction steers the level to, the rate that correction settles at (and how little it jerks between two frames), the resampler, the catch-up of a buffer that ran dry, a callback period the buffer cannot fill, the block a stalled frontend pushes, and a whole minute of the frame loop against a simulated device clock - every sample of it played once, in order |
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
* **The GBA's sound output stage is not modelled**: SOUNDBIAS is stored and read back, but neither
  its bias level (which only shifts the unsigned representation of the samples) nor the PWM
  amplitude resolution (9/8/7/6 bit at 32.768/65.536/131.072/262.144 kHz) change the mix, which is
  produced as clean 16 bit samples. The wave RAM is plain memory rather than the shift register the
  hardware uses, so a read cannot return a digit that has moved, and the duty waveforms follow
  GBATEK's drawing (a high run that starts at the first of the eight steps) where the Pan Docs print
  the same four cycles as bit strings - the two differ only in the phase.
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

Every test passes: **311 of 311**. (The two that compare `build/Data/GBASettings.json` with
`DefaultJson()` byte for byte need that file to have LF endings; a Windows checkout with
`core.autocrlf` turns it into CRLF and they then report the `\r`s as a difference, which says
nothing about the emulator.) The suite was not green
while the core was being written, and every failure was resolved by fixing the emulator or by
correcting an expectation that did not match GBATEK - never by deleting a test. The failures are
listed here because each one names a real bug that is easy to reintroduce.

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
| `Apu.DutyWaveform` and every later test (a crash, not a failure) | The tests drained `Apu::ReadSamples` into an `int16_t buffer[128]`, but the call hands over `maxFrames` *stereo* frames and writes two samples per frame: the 256 byte stack buffer was overflowed and the run died later, in the middle of another suite. Found with AddressSanitizer; the buffers are sized for stereo frames now. |
| `Apu.FifoFasterThanTheHostRate`, `Timers.OverflowTotalCountsEveryWrap` | A direct-sound FIFO moves one byte per timer overflow, but the mixer compared two readings of the timer's *counter* once per host sample. A timer faster than the host sample rate therefore lost overflows, and one whose period divided the host sample length exactly (TM0CNT_L = FF00h against 512 cycles) lost all of them, because after every second wrap the counter was back at the value it had: a FIFO clocked that way never moved a byte, and direct sound played at the wrong rate. The timers now keep a running total of their wraps (`Timers::Overflows`) and both the FIFOs and channel 3's timer-driven digit clock count with it. |
| `GbBus, cgb_power_up_svbk_maps_bank_one_not_bank_zero`, `GbBus, cgb_svbk_selects_banks_two_to_seven_and_zero_means_one` (found by running Metroid II, not by a unit test) | The WRAM bank register (SVBK, 0xFF70) mapped a written 0 to bank **0** instead of bank 1, so a CGB whose SVBK was still at its power-up value 0xF8 aliased `0xC000-0xCFFF` and `0xD000-0xDFFF` onto one 4 KByte page. A DMG-only cartridge that runs in compatibility mode and keeps its variables or stack in the upper bank then had its own low-RAM scratch overwrite them: Metroid II stored a return address at 0xDFFB, read back 0x0000 and restarted from the reset vector for ever (the screen never left the boot marker). Pan Docs "CGB Registers" FF70: "except 0, which maps bank 1 instead". |
| `GbApu, DutyWaveformPhases` and the rest of the `GbApu` suite (the module had **no** tests at all, which is how these lived in it) | The Game Boy APU was wrong in almost every clock it has. The frame sequencer stepped every **512** system clocks instead of every **8192** (Pan Docs: one DIV-APU tick is 512 Hz, and its bit 4 falls every 8192 clocks), so the length, the envelope and the sweep all ran sixteen times too fast, and it clocked the length on step 7 as well, which made it 320 Hz with a jitter instead of 256. The length counters were loaded with `64 - NRx1` and counted *up* to 64, i.e. backwards: a note meant to last (64 - st) / 256 s lasted st / 256 s. The pulse and wave dividers ran at `(2048 - x) * 2` and `(2048 - x)` system clocks instead of `*4` and `*2` (the manuals: the pulse divider is clocked at 1048576 Hz and the wave divider at 2097152 Hz), so every pulse and wave note was an **octave high**. The four duty waveforms had the right ratios but the wrong phases (the docs print 12.5 % as `00000001` and 75 % as `01111110`; the table was `0x01, 0x03, 0x0F, 0xFC`), NR50's master volume was never applied to the mix at all, and NR51's two halves were swapped, so a channel panned hard left came out on the right and the whole stereo image was mirrored. The `GbApu` suite now pins the divider rates, the duty phases, the length timings, the envelope and sweep clocks, the wave RAM read order, the LFSR sequence, the mixer's levels and NR50, the PCM12/PCM34 registers and the CGB's own high pass filter. |
| `Apu.WaveTimerSampling` (the test had to be rewritten), `Apu.WaveRamBanks`, `Apu.DirectSoundIsFourTimesAPsgChannel`, `Apu.MasterVolumeDoesNotTouchDirectSound`, `Apu.SweepUnderflowKeepsTheFrequency` | The GBA's sound controller was checked against GBATEK and the AGB Programming Manual (chapter 10) and had four more things wrong. **Channel 3 was clocked by a timer**: SOUND3CNT_X bit 14 was read as a timer selector as well as the length flag, so any program that enabled the length while a timer was running - the usual case for music - had its wave channel detuned (the manual and GBATEK: bit 14 is the length flag and nothing else, only the two direct sound FIFOs are timer driven). **The mixer's levels were wrong**: the FIFOs were given twice a PSG channel's amplitude instead of four times it, and SOUNDCNT_L's output level scaled the direct sound as well, while the manual says it has "no effect on direct sound"; the levels now follow GBATEK's "Max Output Levels" (a PSG channel spans a quarter of the output range, a FIFO the whole of it, and the sum is clipped by the 10 bit output stage). **The waveform RAM was a single bank**: the GBA has two banks of 16 bytes, the CPU reaches the one that is not playing, and it never played the second bank - games that double buffer a wave pattern played the wrong data (also, the levels had to stop being packed into bytes, which silently truncated the quarter's top level). **A decreasing sweep that would underflow stopped the channel**, where the manual says "the result is the pre-calculation value X(t) = X(t-1)". |
| `Audio.FrameLoopAndCallbackStayInStep` (found by listening to a real game, and reported twice) | With the machine on its own clock, the mixer's rate correction was the one-liner `step += (level - cushion) / 4` once per frame, and the level it read saws up and down by a whole callback period at the beat of the two clocks (512 frames at a few hertz here), because the device takes 512 frames in one go while the machine pushes a frame's worth per frame. That is an undamped integrator whose gain is far past what one frame of sampling allows - its period works out at half a frame - so it swung the full +/-1 % between neighbouring frames, 20001 ppm of it, and its *average* was whatever the sawtooth's phase left behind: in a minute of a 60.00 Hz frame loop the buffer threw 9450 frames away, a few at a time, which is heard as a rattle. The correction is now a slow PI controller on a *filtered* level (`AudioBuffer::ClockCorrection`), the level is steered back to the cushion and the rate settles on the 0.46 % the two clocks disagree by; the test plays a minute of the whole loop and checks that every one of the ~1.9 M frames comes out once, in order, with the rate moving by at most a thousandth of a percent between two frames. |
| `GbPpu, mode_timing_is_the_documented_one` and the rest of the `GbPpu` suite (found by capturing the sound of `zelda.gb` and comparing it with another emulator's, not by a unit test) | The Game Boy's LCD was advanced one dot per **four** clocks of the bus, which is the relationship a *GBA* has between its 16.7 MHz system clock and its LCD, not the one this machine has: on a DMG/CGB the dot clock *is* the 4.194304 MHz clock, so a line is 456 clocks and a frame 70224 of them (16.74 ms). With four clocks to a dot a frame came to 280896 clocks, four times the machine's own, and every device that counts clocks got four times its rate for each frame of picture: the timer's DIV ran at 64 kHz instead of 16384 Hz, the serial port four times its baud rate, the APU mixed 2940 samples a frame where the sound device plays 738.35 (44100 Hz / 59.7275 Hz) - so the mixer threw three quarters of the music away in bursts, which is the rattle - and the CPU had four frames of its own work to get through in one. The tests drove the PPU with `dots * 4` because the implementation did; they now drive it with one clock per dot, and the whole suite is green with the correct clock. Neither the picture nor the frame rate changes (the frontend still shows one frame per 16.74 ms), but the sound, the timer and the emulated work per frame are now the hardware's. |

| `Dma.ARepeatingFifoTransferStreamsForward`, `Dma.RepeatTransferRestartedByVBlank` (the second one had to be rewritten, because it pinned this), and Metroid Fusion's soundtrack (heard first, then captured) | A repeating DMA put its **source address** back on every repeat, which is not what GBATEK lists: "Upon DMA Enable (Bit 15) changing from 0 to 1: Reloads SAD, DAD, CNT_L. Upon Repeat: Reloads CNT_L, and optionally DAD (Increment+Reload)." SAD is not on the repeat list - the source pointer carries on - and a sound DMA is *always* repeating, because the FIFO asks for 16 bytes at a time. So every refill restarted the music at the same SAD and the FIFO played one 16 byte block over and over: a high buzzy squeak that vaguely followed the tune, which is exactly what a GBA game's music sounded like. The channel now keeps the running source pointer, and the register value separately (a 0 -> 1 enable *does* reload from it, and a transfer never changes the register). The new test streams a 64 byte buffer through the FIFO and checks the bytes come out in order; with the old code it stops at the first refill. |

Everything else passes: the ARM7TDMI (66 tests), the Game Boy machine (83 - its CPU, LCD, cartridge,
boot ROM, the CGB WRAM/palette bus paths and the sixteen APU tests), the LCD controller (30), the
sound (41: the GBA's 29 channel and mixer tests and the twelve of the mixer buffer between the
machine and the sound device), the settings (19), the cartridge (14), the disassemblers (13), the
boot ROM and the emitter (12), the DMA (10), the SIO (7), the timers/keypad/interrupts (10), the demo
machine (5) and the BIOS harness (2).

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
