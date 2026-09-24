# Save states

A save state is the whole emulated console in one file: `savestate` writes it, `loadstate` puts it
back, and the front end's File menu and quick keys are the same two operations with a slot attached.
It is the GameCube side of what the integrated GBA emulator has had since `src/gba/gba_savestate.*`;
the two share the shape of the image and the two cursors that write it, but never each other's
states.

The engine is `src/savestate.cpp` (the image, the sections and the files) and `src/savestate.h` (the
format, the cursors and the machine-level API). Everything else in this article is what those two
files decide.

## The keys and the menu

The front end (`src/uisdl.cpp`) offers the ten slots in its File menu and on the keyboard. A slot is
numbered `0`..`9`, which is the number the commands take and the number in the file's suffix.

| Key | What it does |
|---|---|
| `F5` | write the state of the current slot |
| `F7` | read the state of the current slot back |
| `Shift`+`F5` / `Shift`+`F7` | step the current slot forward / backward (it wraps; the status bar shows where it is) |

| Menu item | What it does |
|---|---|
| File -> Quick Save State / Quick Load State | the two keys |
| File -> Save State to Slot / Load State from Slot | a submenu per slot, the current one marked |
| File -> Next State Slot / Previous State Slot | the stepping keys |

Unlike the `F3` reopen shortcut the keys work while a game is running, which is the point of a
state. The front end knows nothing about the format or the file names: it sends the command and
shows the file the answer names in the status bar, so the menu and the console do the same thing. A
*successful* save or load puts the file into the status bar, and a failed one opens the modal box
the core uses for its own errors - the status bar is owned by the performance counters, which
rewrite all four of its fields every second, so a message left there is gone before it can be read.

## The commands

The debug interface has three commands (the `EMU_JDI_JSON` node, `src/jdispecs.cpp`), and each
answers Markdown next to the fields a client that does not want to read prose can use (`file`,
`slot`, `result`, and `error` or `size`).

| Command | What it does |
|---|---|
| `savestate [slot]` | write the machine into the state file of the slot (`0` when it is left out) |
| `loadstate [slot]` | read that file back into the machine |
| `states` | the slots that hold a state, with their files and sizes |

The machine is stopped for the duration of either call and started again afterwards the way it was
(`SaveStates::CorePause`): the Gekko core runs on a thread of its own, and a state of a machine that
is running is a state of nothing.

## The files

A state lives next to the image the console is running and is named after it, the way the GBA module
names its states after the cartridge:

| Image | States |
|---|---|
| `Data/game.iso` | `Data/game.st0` .. `Data/game.st9` |
| `pong.dol` | `pong.st0` .. `pong.st9` |
| the boot ROM (the loader calls it `Bootrom`) | `Bootrom.st0` .. `Bootrom.st9` |

A state is 45 MB or so, and about 51 MB when the software pipeline is running (which also carries
the EFB). The two things that dominate it are main memory (24 MB) and ARAM (16 MB) - the same two
things a real console is mostly made of. Nothing is compressed and nothing is skipped: what a state
does not carry is listed below, and it is all state that belongs to the front end or to an external
file rather than to the machine.

## The format

The image is a header, a checksum of the payload, and then one section per subsystem:

| Offset | Field |
|---|---|
| `0x00` | the magic `PSAVEST` and a NUL |
| `0x08` | the format version (`SaveStates::FormatVersion`) |
| `0x0C` | reserved (0) |
| `0x10` | the size of the payload, as a 64-bit integer |
| `0x18` | the FNV-1a 64 checksum of the payload |
| `0x20` | the payload: the sections |

A section is a four character tag, a 32-bit length and the bytes of the section. The reader walks
the sections in order and dispatches on the tag, so a build that grows a section can still read an
older state (it steps over the one it does not know) and a state whose layout this build does not
share is refused *by the name of the section that did not fit* rather than loaded as a machine that
is quietly wrong. Every section has to be consumed exactly to its end, which is what catches a member
added to a device but left out of its `SaveState`/`LoadState` pair.

`SaveStates::StateWriter` and `SaveStates::StateReader` are the whole interface a device sees, and
they live in `src/savestate.h` (a block that carries a state pair needs nothing else, which is also
what lets the unit tests compile a handful of blocks without the machine behind them). A writer has
`Fields` for a run of members, `Array` for a C array, `Values` for a `std::vector`, `Raw` for a block
of memory and `Text` for a string; a reader has the same calls the other way round, plus `Fail` for a
value it cannot accept. Only a bool, an integer, an enum, a float or a string maps to a value - a
struct or a union is expanded field by field - and the byte order is little endian and explicit, so
the format does not depend on the compiler that wrote it.

## The sections

| Tag | Contents |
|---|---|
| `META` | the machine kind, the size of main memory, the console revision, whether the boot ROM is running, the four character code of the disc and the name of the loaded image |
| `FLPR` | the deadline of the Flipper's periodic work (the anchor the scan-out and the serial poll hang off) |
| `CPU ` | the Gekko register file (general, floating-point/paired-single, the 1024 SPRs, the segments, CR/MSR/FPSCR/PC, the whole 64-bit time base), the sticky latches (the decrementer request, the interrupt line, the pending exception, the `lwarx`/`stwcx` reservation, the reason of the last PROGRAM exception), the write gather buffer, and the locked L1 data cache with its window |
| `MEM ` | the size of main memory and all of it, the four MARR registers with their control word, the two MI interrupt registers and the eight access counters |
| `PI  ` | the interrupt cause and mask, the chip id and the block's own copy of the CP FIFO window (base, top, write pointer, wrap bit) |
| `VI  ` | the display configuration and timing registers, both field bases, the raster position, the INT0 compare/status, both light-gun latches and the line timer |
| `AI  ` | the four streaming registers and the 32-byte FIFO of samples that have not reached the mixer yet |
| `DI  ` | the status/cover/control registers, the DMA address and length, the 12-byte command buffer, the immediate buffer (where the drive latches its sense code) and the 32-byte DMA staging FIFO with both byte counters |
| `SI  ` | the four channel latches and their shadows, POLL/COMCSR/SR, the 160-byte COM buffer and the poll schedule |
| `EXI ` | the three channels (CSR, DMA address and length, CR, data), the console's 64 bytes of SRAM settings and the MX/AD16 latches |
| `CP  ` | the command processor's register file (both ring pointers, the water marks, the status latches, the XF read-back), the parser's vertex format state (VCD, the three VATs, the array bases and strides), the BP write mask and the 1 MB decoded stream buffer with its cursors |
| `DSP ` | the DSP machine: whether it runs and its clock anchor, both mailbox pairs with their snapshots, the DMA registers, the ADPCM/IIR filter history and the accelerator window, the DSP-side AI DMA, ARAM (16 MB) with its registers, and the core (both memories, the register file, the four stacks, the interrupt control and the countdowns) |
| `DVD ` | the drive's state machine, the command and immediate buffers, the read pointer and the transaction size, and the DVD-audio streaming state (position, rate, the decoded PCM buffer and the ADPCM filter history) |
| `GX  ` | the graphics pipeline's shared registers (GEN_MODE and the four quad/sample locations), where the frame loop stands and the size of the render target |
| `PE  ` | the BP register file of the pixel engine, the CPU-side status register, and the software EFB (the colour plane and the Z plane, about 6 MB at 640x480) when the software pipeline is running |
| `XF  ` | every transform register: the three matrix memories, the eight lights, the channel controls, the viewport, the projection and the texture coordinate generation |
| `SU  ` | the scissor registers, the line/point size, the eight texture size pairs and the latches that say which of them were programmed |
| `RAS ` | the eight texture references, the two scale/bias pairs and IREF |
| `TEV ` | the sixteen colour and alpha environments, the four colour and K-constant register pairs, the range adjustment, the fog parameters and the alpha function |
| `TX  ` | the texture registers, the palette generation, the 1 MB palette (TLUT), the 1 MB of TMEM and the tag cache of its hardware-managed images |
| `BP  ` | the three indirect (bump) matrices, IMASK and the sixteen indirect commands |
| `HLE ` | the two OS context mirrors the host-side OS calls keep |

## What a state does not carry

Everything below is deliberately out, and each one has its reason in the code next to the section it
belongs to:

* **the disc image, the executables, the boot ROM** and the DSP ROMs. They belong to the front end
  and the settings and outlive any state; a state names the image it was taken from instead (see the
  refusals below).
* **the memory cards and the other devices of the peripheral pool.** A card's contents live in its
  own file and are written through to it as the guest writes them; the controllers are the user's
  hands, re-read before every poll. The pool is opened with the emulator and outlives every machine.
* **the samples already handed to the host audio device.** They are a host buffer, not machine state.
  A load therefore always has a sub-frame audio discontinuity; the FIFO of samples that the AI has
  not pushed yet *is* in the state, so the discontinuity is a fraction of a frame.
* **the EFB of the shader pipeline**, which lives in an OpenGL render target rather than in a memory
  array. A state of a machine that runs the software pipeline carries its EFB; a state of one that
  runs the shader pipeline resumes with whatever the title draws next, and the frame it was in the
  middle of is not shown again (the guest redraws it).
* **the GL objects, the window, the threads and the callbacks**: the host's, recreated or re-armed
  when the emulator was built.
* **the JIT's compiled blocks** and the DSP recompiler's. Loading a state drops both: the code they
  were translated from has been replaced under them.
* **the debugger's breakpoints** and the profiler's counters. A breakpoint armed on the machine the
  debugger is watching stays armed across a load.
* **the frame counter** the front end keeps (`gfx_frame_counter`) and everything derived from it.
* **the cache residency**: the data and instruction caches are written back to main memory and
  dropped before anything is stored, and a load throws them away as well. The locked L1 data cache
  is the exception and travels in the state, because it is scratch-pad memory that is deliberately
  not coherent with main memory.

## A state is refused when it does not belong here

Nothing is applied before the machine has been checked, and each refusal names what did not fit:

| The state | The answer |
|---|---|
| is not a state at all | `the file is not a save state` |
| is a format this build does not know | `the save state is format version N and this build reads M` |
| was damaged on the way | `the save state is corrupt (its checksum does not match)` |
| is a different length than it says | `the save state says it is a different size than it is` |
| is a truncated file | `the save state says it is a different size than it is` |
| belongs to another game | `the save state belongs to "pang.dol" (no disc) and this console is running "pong.dol" (no disc)` |
| was taken on a console with other memory | `the save state was taken on a console with 48 MBytes of main memory and this one has 24` |
| was written by another build of the emulator | `the image has the section "XXXX" where "YYYY" belongs`, or `a section has more bytes than this build reads` |
| is a state of the other machine (a GBA or Game Boy one) | `the save state is a Game Boy Advance state, and this machine is a GameCube` |
| is a slot with nothing in it | `no save state in pong.st7` |

A state that fails half way leaves the machine half loaded - the sections are applied as they are
read, and the alternative (a whole second machine built off to the side) is not what this emulator is
shaped for. The failures that can be checked without touching the machine - the header, the checksum,
the identity, the section layout - are all checked first; a state that gets past them and then fails
is a state from a build whose sections have changed under it, and the guest is expected to be reset
rather than resumed.

## Adding a block to a state

A block that has machine state of its own adds a pair of members:

```cpp
void SaveState(SaveStates::StateWriter& writer) const;
void LoadState(SaveStates::StateReader& reader);
```

and `src/savestate.cpp` gives it a tag, opens the section, calls the pair and closes the section -
`writer.Begin("XXX ")` / `writer.End()` on the way out and a branch of the tag dispatch on the way
back. Inside the pair the fields go out in one fixed order and come back in exactly the same one,
which is what the section length check enforces.

`src/savestate.h` is the only header such a block needs. The image and the orchestrator stay in
`src/savestate.cpp`, and the blocks do not know about each other.

## The tests

There is no unit test for the GameCube side yet. What was checked, with a headless build
(`cmake -B build_headless -DHEADLESS=ON`, and the `pureikyubu_headless` project of `pureikyubu.sln`)
driven through the MCP transport (`pureikyubu --mcp --image pong.dol` and `--image Bootrom`) on
2026-09-24, on Linux/GCC and on Windows/MSVC (Visual Studio 2026):

* **the round trip is exact.** With the core stopped, a state was written, read back and written
  again: the two images are identical byte for byte, both at a machine state ~173 million ticks into
  a run of `pong.dol` and at one ~264 million ticks into a run of the boot ROM with the software
  pipeline live (a 51,468,936 byte state, its EFB included). That is the same completeness property
  the GBA suite asserts - it can only hold if every block writes everything it has. Two asymmetries
  were found by it and fixed: the memory interface's counters were left holding what the *load* had
  counted of its own (the video interface translates its XFB pointer out of the restored register),
  and the DSP recompiler's generation was bumped by the invalidation the load performs.
* **a loaded state resumes the run that never stopped.** A state was taken (`A`), the machine was
  stepped on 20,000 instructions and a second state was written (`B`); then `A` was loaded again and
  the machine was stepped the same 20,000 instructions, giving a third state (`C`). `B` and `C` are
  identical byte for byte - on `pong.dol` and on the boot ROM, whose run has the display, the audio
  and the peripherals all busy. This is what says a state carries everything the *next* instruction
  reads and not merely everything the next save would write. The first run of this test failed in two
  sections, and the reason was a real one: the tick at which the Flipper-side periodic work (the
  scan-out, the serial poll, the device steps) is next due had been treated as a value derived from
  the clock instead of a latch of the run, which shifted every interrupt the devices raise by up to
  100 ticks. It travels in the `CPU ` section now.
* **a load that lands in a machine which has run on.** The three checks above all load a state into
  a machine that has not moved since the state was written (the round trip stops the core first, and
  the two runs of the resume test are only a few thousand instructions apart). The way a state is
  really used is the opposite: the save key is pressed, the game is played for a while, and the load
  key is pressed. A fourth check does exactly that - sleep, save `A`, step 20,000 instructions and
  save `B`, run free for five seconds, load `A`, step the same 20,000 instructions and save `C` - and
  `B` has to equal `C`. It failed the first time it was run, and the reason was the emulated caches:
  a load restored main memory but left the data and instruction caches holding the lines of the run
  in between, so the CPU executed code the restored machine never had (the boot ROM died on an
  unimplemented opcode). `GekkoCore::LoadState` throws both caches away now - *invalidates* them
  rather than writing them back, because the dirty lines are the newer bytes and casting them out
  would overwrite the memory the load has just restored.
* **the refusals work**: a corrupted image, a truncated one, one that belongs to another image, and
  an empty slot were each refused with the sentence in the table above, and a good state still
  loaded afterwards.

Both runs were made twice: once with the CMake/GCC build of the emulator and once with the VS2026
build (`msbuild scripts/VS2026/pureikyubu.vcxproj`, `pureikyubu_headless.vcxproj` and
`pureikyubu_test.vcxproj`, `Release|x64`), which compiles the same code with MSVC and produced
byte-identical answers and files. The project's own unit test suite (`vstest.console.exe
pureikyubu_test.dll`) runs 531 tests with three failures - `Pe_ZWriteMaskControlsTheDepthUpdate`,
`Pe_ZFreezeDisablesTheDepthWrites` and `Report_TevFog` - and the same three fail on a DLL built
before this work, so they are the test harness' own (the two depth ones also pass when they are run
on their own) and not a regression of the save states.

The front end itself was checked by driving the windowed build from the outside
(`pureikyubu.exe --ipl`, Visual Studio 2026, Release x64): the File menu was opened with the mouse
and shows the quick items, the two slot submenus and the two stepping items; a slot was written and
read from those submenus; the quick keys (`F5`, `F7`) and the stepping keys (`Shift`+`F5`,
`Shift`+`F7`) were sent to the window and each wrote or read the slot the log names; the boot ROM
was saved, played for 25 seconds and resumed from the state without a single CPU error, with the
picture still on the screen; and a load from an empty slot raised the modal report.

## The files

| File | Contents |
|---|---|
| `src/savestate.h` | the format, the two cursors (header-inline), the machine API and the slot files |
| `src/savestate.cpp` | the image header and its checksum, the sections of the whole machine, the files and the reports |
| `src/<block>.h/.cpp` | a `SaveState`/`LoadState` pair per block: `gekko`, `flipper`, `mem`, `pi`, `vi`, `ai`, `di`, `si`, `exi`, `cp`, `dsp`/`dspcore`/`dsparam`/`dspai`, `dvd`, `gfx`, `pe`, `xf`, `su`, `ras`, `tev`, `tx`, `bump`, `os` |
| `src/uisdl.cpp` | the File menu items and the quick keys |
| `src/uijdi.h/.cpp` | the client side of the two commands, for the menu |
| `src/jdispecs.cpp`, `src/main.cpp` | the `savestate`, `loadstate` and `states` commands |
