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
| `HleBios` | The high level BIOS calls against the official BIOS image, when the user has one: the SWI numbers, `HuffUnComp`, `BitUnPack`, `LZ77` (both write variants), `RL` and the two delta filters byte for byte on crafted and on randomly generated streams, `MidiKey2Freq` against the BIOS's own fixed point, and the sound driver's identifier, frequency table, FIFO DMA setup, mixer and VSync/VSyncOff (the image is not shipped, so those tests skip themselves without it) |

## Deviations the tests pin down

These are places where the emulator deliberately simplifies the hardware. They are recorded here so
that they are not mistaken for verified behaviour. Where a test depends on one, the test says so.

* **The LCD composes a whole scanline when its HBlank starts** instead of drawing it dot by dot
  during the visible part of the line. A game that rewrites VRAM or the scroll registers *inside*
  the visible part sees the change one line earlier than on hardware.
* **The per-line OBJ cycle budget is not modelled.** GBATEK "Maximum Number of Sprites per Line"
  gives a visible line 1210 rendering cycles (954 with DISPCNT bit 5, the "H-Blank Interval Free"
  flag) and a cost per OBJ, so hardware drops the OBJs that no longer fit; the renderer draws
  every OBJ that covers the line and DISPCNT bit 5 changes nothing.
* **VRAM/OAM/Palette contention is not modelled.** GBATEK "VRAM, OAM, and Palette RAM Access":
  on the GBA the CPU may reach the display memory at any time, with a waitstate inserted when the
  controller is using it in the same cycle (unlike a DMG, no access is lost). The accesses here
  take their documented 1/1/2 cycles (`Bus, InternalMemoryTimingsFollowTheMemoryMap`) but never
  the extra contention cycle. The `Ppu` test `RegisterReadBack` and the `memory` test ROM pin the
  data rules.
* **DISPSTAT's H-Blank flag rises 46 cycles after the visible part of the line ends** (cycle
  1006), as GBATEK 4000004h has it: "Although the drawing time is only 960 cycles (240*4), the
  H-Blank flag is '0' for a total of 1006 cycles". The interrupt and the HBlank/video capture DMA
  triggers stay at the start of the blanking interval (cycle 960), which is where GBATEK's
  "LCD Dimensions and Timings" puts "H-Blanking 68 dots ... 272 cycles"; the aging cartridge's
  `H BLANK STATUS` check is what pinned the flag's own delay.
* **The internal RAM's own access times follow GBATEK's memory map, except for the display
  memory.** The CPU's cycle counts are exact, the cartridge's waitstates follow WAITCNT, and the
  on-board 256K WRAM (a 16-bit bus) charges its 3/3/6 cycles - with the waitstate count taken from
  the undocumented 4000800h register, whose default 0Dh is the documented two waits - while VRAM,
  OAM and Palette RAM charge the documented 1/1/2. What is still missing is GBATEK's "+1 cycle if
  the GBA accesses video memory at the same time" (the renderer composes a whole line at once
  rather than dot by dot), and the cartridge prefetch buffer is modelled as a waitstate rule
  rather than as a real 8-halfword buffer.
* **A DMA transfer runs to completion in one go**, so a request that arrives while one is running
  (an HBlank/VBlank trigger, or a FIFO refill the sound asks for) waits for the channel's next
  trigger instead of preempting it by priority. Its *cycles* are no longer part of this
  deviation: the transfer spends them as it runs - GBATEK "Transfer Rate/Timing" gives the read
  and write cycles per unit and the hardware steals them one at a time, so the timers, the LCD
  and the sound keep moving between one unit and the next (the AGB aging cartridge measures
  memory speed by DMA-sampling Timer 0, and `Dma, ATransferSpendsItsCyclesWhileItRuns` pins the
  behaviour). The word counts, the address adjustments and the start timings follow GBATEK.
* **The BIOS service functions are implemented in the host** (`gba_hlebios.cpp`) from GBATEK and the
  AGB Programming Manual, not executed from a BIOS image, and the ones that need the BIOS's private
  RAM were written by reading the official image's own code (see the `HleBios` row above).
  `ArcTan`/`ArcTan2` use the host's `atan2` rather than the BIOS's table, `GetBiosChecksum` returns a
  checksum of the emulator's own BIOS, and `MultiBoot` is reported and returned from without doing
  the work.
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
  of that picture, and both now come out **pixel for pixel identical to Matt Currie's reference
  images**: `dmg-acid2` on the DMG, `dmg-acid2` on a CGB in DMG compatibility mode (the same shade
  indices through the machine's own grey ramp - the emulator does not reproduce the CGB boot ROM's
  title based compatibility palettes, so the colours there are the emulator's, but every element is
  where the reference puts it) and `cgb-acid2` on the CGB (whose reference uses exactly the 5-bit
  to 8-bit expansion `GbPpu::CgbColor` performs). The ROM writes its registers during mode 2 and
  does not touch them inside mode 3 (its own README says a line based renderer is sufficient), so
  the test pins the register rules - the background, the window and its line counter, the objects
  and both priority rules, the palettes, the 8x16 size and the flips, and the mid-frame LCDC/SCX
  switches - and not the per-dot timing.

## The test suite

Every test passes: **349 of 349**. (The two that compare `build/Data/GBASettings.json` with
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
| `GbPpu, a_line_start_requests_the_mode_two_interrupt`, `GbPpu, a_line_start_requests_the_lyc_interrupt` (found by comparing `dmg-acid2` and `cgb-acid2` with Matt Currie's reference images pixel for pixel) | `GbPpu::BeginVisibleLine` called `EnterMode(2)` but threw its return value away, so the STAT request a line start produces - the mode 2 source turning on, and an LYC that matches the new `LY` - never reached `IF`. **Every visible line's mode 2 and LYC STAT interrupt was lost** (the ones inside VBlank still worked, which is why the bug hid). Raster effects driven by `LY=LYC`, which is how `dmg-acid2` draws the hair, the right eye, the mouth and the footer, therefore never ran: the acid picture kept the register state `Main` left behind. Both reference comparisons are exact now. |
| `GbPpu, a_register_write_can_raise_the_stat_line`, `GbPpu, stat_coincidence_flag_survives_the_lcd_being_off` | The PPU could not request a STAT interrupt from a register write at all (`WriteRegister` returned `void`, and the bus never asked again), so a write to `LYC` that made it match `LY` - which Pan Docs compares "constantly" - raised the shared line without requesting the interrupt, and enabling a STAT source whose condition was already true did the same. `STAT` bit 2 was also masked off while the LCD was off even though the comparison stays live (only the mode bits read 0). |
| `GbPpu, cgb_compat_lcdc0_blanks_the_background_like_a_dmg`, `GbPpu, cgb_compat_and_opri_use_the_dmg_x_priority`, `GbBus, opri_selects_the_dmg_object_priority` | A CGB running a monochrome cartridge (which is how the frontend runs a DMG game by default) ignored the DMG's own display rules: `LCDC` bit 0 was treated as the CGB's master priority instead of blanking the background and the window, the window bit 5 was not overridden by it, objects were prioritised by OAM position instead of X (OPRI 0xFF6C was stored but never reached the PPU), the bank 1 attribute map was read although the manual says that bank "is not present in this mode", and `BGP`/`OBP0`/`OBP1` were ignored instead of indexing the CGB palettes (Pan Docs "Power Up Sequence": the compatibility palettes). The `dmg-acid2` compatibility build now matches its reference. |
| `GbPpu, object_penalty_stays_in_the_documented_six_to_eleven_dots` | The mode 3 OBJ penalty was `11 - (X mod 8)`, which drops to 4 or 5 for an object on the last two columns of its background tile; Pan Docs' "or zero if negative" floors it at 6 (`11 - min(5, X mod 8)`), so the extra dots the PPU stalls for were undercounted. |
| `GbBus, hdma_writes_to_the_vram_bank_vbk_selects`, `GbBus, hdma5_reads_active_with_bit_seven_clear` | HDMA/GDMA always wrote VRAM bank 0, while the manual is explicit that "the LCD display RAM area (8000h-9FFFh) selected as the transfer destination is the bank specified by register VBK"; and reading HDMA5 during an active HBlank transfer returned bit 7 set, which Pan Docs defines as "Not Active". A stopped transfer (bit 7 set with the blocks left) keeps that value, and a finished one reads 0xFF. |
| `GbPpu, lcd_off_shows_the_blank_white` | Turning the LCD off left the last picture in the frame buffer; Pan Docs "LCDC" bit 7 says the disabled screen "is blank, which on DMG is displayed as a white 'whiter' than color #0". |
| `Ppu, Mode5UsesA320ByteLine` (an audit of the GBA LCD against GBATEK and the AGB Programming Manual v1.1) | The mode 5 bitmap used the mode 3 line stride: 480 bytes for a frame only 160 dots wide, where the manual's address map (6.2.4.3) puts line 1 at 140h and line 2 at 2A0h and GBATEK says the frame "occupies exactly 40 KBytes". A line is 320 bytes, so every mode 5 image below its first line was skewed. |
| `Ppu, Mode1Bg2IsTheAffineLayer` (found by running Final Fantasy V Advance) | A mode 1 renderer treated BG2 as a text layer (`mode >= 2 && index >= 2`), even though GBATEK's mode table prints mode 1 as "Mixed 012-" and the AGB manual's table marks it "Yes" under Rotation/Scaling, with 6.1.7 assigning the parameters to BG2/BG3. FF V Advance's intro draws its zooming logo on that layer, so the screen was a field of noise: the halfword text fetch walked one-byte affine map entries as tile numbers and read tiles out of unrelated VRAM. |
| `Ppu, BitmapModeSamplesThroughTheBg2AffineMatrix` | The bitmap modes were displayed 1:1 and the BG2 rotation/scaling registers were ignored, although the manual 6.2.2 says "The parameters for Bitmap BG Rotation/Scaling use BG2 related registers (BG2X_L, BG2X_H, BG2Y_L, BG2Y_H, BG2PA, BG2PB, BG2PC, and BG2PD)" and its BG mode table (and GBATEK's) lists the modes 3-5 as rotation/scaling capable. The frame buffer is now sampled through the same matrix as an affine tile layer; the boot ROM and the no-BIOS start leave BG2PA = BG2PD = 0x0100, which is what the real BIOS programs during its own display setup (traced through its writes), so a game that never touches the matrix still displays 1:1. |
| `Ppu, WindowGarbageDimensionsReachTheScreenEdge` | An inverted window range (X1 > X2, or Y1 > Y2) was treated as an empty window. GBATEK 4000040h/4000044h: "Garbage values of X2>240 or X1>X2 are interpreted as X2=240" (and Y2=160 for the rows), so such a window reaches the right (bottom) edge. |
| `Ppu, ObjWindowNeedsBothDisplayFlags` | The OBJ window was defined from DISPCNT bit 15 alone. GBATEK "The OBJ Window": "Both DISPCNT Bits 12 and 15 must be set when defining OBJ Window region(s)", so clearing the OBJ master enable removes the window as well. |
| `Ppu, GreenSwapExchangesTheGreenOfEachPair` | The undocumented Green Swap (4000002h) byte-swapped every pixel on its own. GBATEK: with the bit set "each pixel group is output as BgRbGr (ie. green intensity of each two pixels exchanged)", so the two dots of a pair trade their green 5-bit fields and the red and blue fields stay. (On the white forced-blank screen the old test used, both greens are 31, so it could not see the difference.) |
| `Ppu, SemiTransparentObjWithoutASecondTargetIsOpaque`, `Ppu, WindowEffectBitGatesTheAlphaBlend` | A semi-transparent OBJ alpha blended with whatever was under it even when BLDCNT selected no 2nd target, and the BLDCNT alpha effect ignored the window's colour special effect bit (it already gated the brightness). The manual's effect table has the semi-transparency "performed only when a semi-transparent OBJ is present and is followed immediately by a 2nd target screen", and the WININ/WINOUT effect bit is what enables the effects in a window region. |
| `Ppu, TallDoubleSizedObjWrapsToTheTopOfTheScreen` | An OBJ whose 8-bit Y range runs past line 255 was culled. GBATEK "OBJ Attribute 0" cautions that a 128 pixel tall OBJ "located at Y>128 will be treated as at Y>-128, the OBJ is then displayed parts offscreen at the TOP of the display, it is then NOT displayed at the bottom". The Y is now wrapped, which is the same as the hardware's `(line - Y) & 0xFF < height` test. |
| `Ppu, NegativeAffineReferenceSamplesTheWrappedTexel` | The 28-bit reference point was kept as an unsigned value, so a negative coordinate - the usual case for a rotation around the screen centre - was a huge positive one. GBATEK 4000028h makes bit 27 the sign; it is sign-extended now. (The wrap and the fetches reduce the coordinate modulo powers of two, so the sampled texels happened to agree before; the arithmetic is signed now because that is what the register means.) |

| `Ppu, VerticalScrollPastTheFirstMapBlock` (found by watching the attract demo of Castlevania: Circle of the Moon) | The text layers sampled their scroll offsets through `Read16(0x010..0x01E)`, the register's *readable* form, which the PPU trimmed to eight bits. GBATEK 4000010h makes the offset bits 0-8 (0-511), so a layer scrolled past dot 255 jumped 256 dots: Castlevania's demo scrolls a room to exactly that point, and the truncated offset showed the tiles the map's other half happens to hold - a band of green "corrupt background" across the top of the screen - while the camera appeared not to follow the player. The renderer reads the stored offsets now, and the registers read back all nine bits. |
| `Cart, EepromOnlyAnswersInItsOwnWindow` (found by running The Legend of Zelda: The Minish Cap, which hung on a blank screen a few seconds into its intro) | `Cart::ReadRom16`/`ReadRom32`/`WriteRom16` applied the EEPROM to **every** access in the cartridge area while the chip was selected, not just to the chip's own window. GBATEK "GBA Cart Backup EEPROM" has it answering "anywhere at D000000h-DFFFFFFh" on a cartridge of 16 MByte or less (and in the last 256 bytes of the image on a full 32 MByte one); everywhere else the ROM still drives the bus, which is what lets a game run its EEPROM routine **from the cartridge** - Minish Cap fetches the instructions at 080B1568h between the read request and the data transfer. Those fetches came back as EEPROM bits (0001h) instead of `push {r4-r6,lr}`, the CPU ran off into a title string at 0811E484h and the machine ended up in an undefined-instruction loop. The window is now the only place the chip drives the bus. |
| `Dma, ATransferSpendsItsCyclesWhileItRuns` (found by running Nintendo's AGB aging cartridge, `AGB_CHECKER_TCHK10.gba`) | A DMA transfer was atomic: it moved every unit and only *then* added its cycles to the bus, so the whole system clock stood still for the transfer's duration. GBATEK "Transfer Rate/Timing" counts the transfer's read and write cycles per unit and hardware steals them one at a time, so the timers, the LCD and the sound keep moving between one unit and the next. The aging cartridge times a memory block by DMA-sampling Timer 0 and comparing the samples against an arithmetic progression: with the clock frozen it read the same value 128 times, and now it reads the documented step (4 cycles per 16 bit unit over the on-board 256K WRAM). The transfer also marks the accesses it makes, because the reconciliation cycle a 32 bit *CPU* access pays on a 16 bit bus is not part of the transfer's own accounting. |
| `Dma.ARepeatingFifoTransferStreamsForward`, `Dma.RepeatTransferRestartedByVBlank` (the second one had to be rewritten, because it pinned this), and Metroid Fusion's soundtrack (heard first, then captured) | A repeating DMA put its **source address** back on every repeat, which is not what GBATEK lists: "Upon DMA Enable (Bit 15) changing from 0 to 1: Reloads SAD, DAD, CNT_L. Upon Repeat: Reloads CNT_L, and optionally DAD (Increment+Reload)." SAD is not on the repeat list - the source pointer carries on - and a sound DMA is *always* repeating, because the FIFO asks for 16 bytes at a time. So every refill restarted the music at the same SAD and the FIFO played one 16 byte block over and over: a high buzzy squeak that vaguely followed the tune, which is exactly what a GBA game's music sounded like. The channel now keeps the running source pointer, and the register value separately (a 0 -> 1 enable *does* reload from it, and a transfer never changes the register). The new test streams a 64 byte buffer through the FIFO and checks the bytes come out in order; with the old code it stops at the first refill. |
| `HleBios.HuffmanMatchesTheOfficialBios`, `HleBios.Lz77MatchesTheOfficialBios`, `HleBios.RlMatchesTheOfficialBios`, `HleBios.DiffFiltersMatchTheOfficialBios`, `HleBios.BitUnPackMatchesTheOfficialBios` (the official BIOS image as the oracle, read with the disassembler and a call probe) | With `HuffUnComp` implemented, Metroid Fusion still could not boot on the HLE: it froze on the title screen (its own IRQ handler found `IF` already cleared and did nothing) and, once that was fixed, its graphics were wrong. Reading each decompressor out of the official image found that all four of the others were wrong too. **`HuffUnComp`**: its bitstream starts at `source + 4 + (treeSize + 1) * 2` and, more importantly, the hardware loads its 32bit units with `ldr` from that (usually unaligned) address, so the bits are the ARM7TDMI's **rotated** word - the byte at the unit address first, the two before it last. That is not a detail: with a deep tree it puts the tree's own data bytes in front of the bitstream that follows them, and a byte-sequential read decoded `88 88 88 88` where the hardware gives `77 88 77 77` (the tree walk itself was right: `child0 = (address AND NOT 1) + offset * 2 + 2`, `child1 = child0 + 1`, and the two "is data" flags are bits 7 and 6). **`RLUnComp`**: its flag byte is not a bit field of eight blocks the way LZ77's is - bit 7 says whether the token is a run and bits 0-6 are its length (`N-1` bytes to copy, or `N-3` copies of one byte), so one flag byte is one token. The old reading turned every run into a handful of literals, which is a completely different stream. **`BitUnPack`**: the top bit of the 32bit data offset - not a byte of its own - is the "add the offset to zero units as well" flag, the units come out of each source byte least significant first, and a trailing partial 32bit word is dropped rather than written. **`LZ77`/`Diff8`/`Diff16`**: a block that reaches past the declared size is still copied whole, the Vram variants write halfwords through an accumulator (so a byte that is produced but not yet paired is *not* in memory: a back reference that reaches it reads the old contents, and an odd final byte is dropped), and `Diff16` accumulates in 16 bits, where the byte-wise version lost every carry into the high byte. **The IRQ contract**: the official BIOS's IRQ handler only saves the registers and calls whatever is at 03007FFCh - acknowledging is the *handler's* job. The emulator's own boot ROM acknowledged `IF` before dispatching, so Metroid Fusion's handler, which reads `IF` to see what happened, found nothing set and skipped its per-frame work: the game froze on its title screen, the picture never changed again, and the same freeze also took the SIO link driver with it once the acknowledge was removed (it enables the SIO interrupt, so it now installs its own acknowledge-and-return handler, which is what a program has to do). Metroid Fusion on the HLE now draws the same frames, pixel for pixel, as the official BIOS - the sequences line up at a fixed 35 frame offset, the HLE boot being the shorter one - and its audio matches to well under a decibel. |
| `HleBios.TheSwiNumbersAreTheOfficialOnes` (found by reading GBATEK's function list against the header) | The host-side BIOS calls had the whole sound driver at SWI 28h..2Fh and called 1Ah "DivArm2". GBATEK puts it at 1Ah..1Fh (`SoundDriverInit`, `Mode`, `Main`, `VSync`, `ChannelClear`, `MidiKey2Freq`) and 28h/29h (`VSyncOff`/`On`), so a game's `SoundDriverInit` was answered with a **division** and its music never started, `MidiKey2Freq` was read as `SoundDriverVSyncOff`, and the calls were reported as unimplemented sound slots. The numbers are the official ones now, and the driver's entry points are implemented (see the finding below). |
| `HleBios.HuffmanMatchesTheOfficialBios`, `HleBios.MidiKey2FreqMatchesTheOfficialBios`, `HleBios.TheSoundDriverMatchesTheOfficialBios` (the official BIOS image as the oracle) | `HuffUnComp` was a stub, so a game whose graphics are Huffman-compressed in the ROM could not boot without a real BIOS (Metroid Fusion was one: a black screen). It is implemented from GBATEK "SWI 13h" - and the differential test against the official BIOS found one thing the document leaves open: the hardware does **not** stop in the middle of a 32bit output unit, it keeps decoding until the unit that holds the last requested byte is full (with a 7 byte stream its last unit's fourth byte is a decoded symbol, not a zero). `MidiKey2Freq` was a double precision guess; it now uses the BIOS's own fixed point (a 16.16 semitone table, the octave as a shift, the product truncated, the fine value as a slope), which the oracle pins to within a unit per few thousand. The sound driver's entry points (`SoundDriverInit/Mode/ChannelClear/Main/VSync/VSyncOff/VSyncOn`) were stubs too; everything a probe could measure out of the real BIOS is implemented now: the work area's identifier 68736D53h and its 0FB0h byte size, the `pcmbuf` layout (two 0630h byte halves at +0350h and +0980h), the two FIFO DMAs (SAD at those halves, DAD at 40000A0h/40000A4h, CNT B600h - repeating with an incrementing source, which is what the DMA fix above is about), `SOUNDCNT_H` 210Eh, the mode fields in the header (+5 reverb, +6 the simultaneous channels, +7 the master volume) and the timer 0 reload for every playback frequency index. `SoundDriverMain` now mixes the sixteen virtual channels into `pcmbuf` as well (the envelope pipeline, the volume and pan levels, and the phase accumulator, verified against the official driver by a differential test), so a game without a real BIOS has its music and its direct sound effects; what is still missing is the reverb (the mode bits are kept, but the driver's own delay line is not modelled) and the exact fixed point of the pitch stepping. |

Everything else passes: the ARM7TDMI (66 tests), the Game Boy machine (95 - its CPU, LCD, cartridge,
boot ROM, the CGB WRAM/palette/HDMA bus paths and the sixteen APU tests), the LCD controller (41), the
sound (41: the GBA's 29 channel and mixer tests and the twelve of the mixer buffer between the
machine and the sound device), the settings (19), the cartridge (15), the disassemblers (13), the
boot ROM and the emitter (12), the DMA (9), the SIO (7), the timers/keypad/interrupts (10), the demo
machine (5), the BIOS harness (2) and the HLE calls against the official BIOS (14).

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
  0x3F27` and alpha blending around it. **This is written up as an open finding but it no longer
  is one**: running with the real image (`--bootrom --bios testing/gba_bench/bios/gba_bios.bin`,
  and `--run <cartridge> --bios ...`) draws the GAME BOY / Nintendo animation and hands over to
  the cartridge, so the mode 2 BG3 + OBJ window + alpha path below is what produced it (the OBJ
  window fix the sentence below mentions is what resolved it). The `Disasm`
  suite and `--trace` (see above) are what made this readable, and the OBJ window had no test until
  now: `Ppu.ObjWindowMasksTheLayers` covers it and found that the window stamp of one scanline
  survived into the next. `test_bios.cpp` still asserts what is certainly true and reports the rest.
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
