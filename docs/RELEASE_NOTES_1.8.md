# pureikyubu 1.8

**Release date:** September 2026
**Previous release:** 1.7 (September 2026)

Version 1.8 is the performance release, and most of the speed came from removing work rather than
adding it. Gekko gained an x86-64 recompiler that lifts the emulated CPU to over 150 MIPS, the
periodic device threads stopped polling the time base in tight loops, and the interrupt lines of the
DSP, the disk interface and the serial interface were re-derived from the hardware documentation.
The last part matters more than the numbers: with the interrupt storms gone, Zelda: The Wind Waker
boots to its title screen and Animal Crossing finally leaves its exception handler. The release also
adds compressed RVZ disc images, a full controller-settings dialog for the SDL build, and a new
application icon.

![The Legend of Zelda: The Wind Waker](imgstore/zelda_wind_waker.png)
![Metroid Prime](imgstore/metroid_prime.png)

## Highlights

- **Gekko recompiler.** A new x86-64 basic-block compiler (`src/gekkojit.*`) keeps the guest GPRs in
  host registers and shares the interpreter's decode cache for everything it does not translate.
  Measured on the differential harness: 39.7 MIPS for the original interpreter, 58.6 for the inlined
  interpreter and 155.9 with the recompiler; ALU-heavy code gains 8.6x. `jit 0` / `jit 1` in the
  debugger switches engines at run time.
- **Paired-Single in SSE.** Every arithmetic PS instruction and both quantised loads and stores
  (`psq_l` / `psq_st`) are translated to SSE/SSE2. The PS-heavy workload goes from 59 MIPS
  (interpreter) to 435 MIPS, the quantised-load workload from 57 to 191.
- **No more busy-waiting.** The VI scan-out and the serial poll now run on the CPU thread from the
  tick it already advances, and the CP, AI DMA and DSP threads block on events instead of spinning on
  `Core->GetTicks()`. `pong.dol` went from 1.47x to 3.66x real time, Ikaruga from 2.16x to 4.58x and
  Luigi's Mansion from 0.62x (stalled) to 4.26x.
- **The aggregate interrupt lines now work.** The DSP and disk-interface Processor-Interface lines
  ANDed the cause bits with their own mask bits, which is always zero; both now follow the OR of the
  latched, unmasked causes. The serial interface polls on the video line schedule instead of a fixed
  tick, which removes an interrupt storm that pinned titles inside their handlers.
- **Games.** Zelda: The Wind Waker boots to its title screen and Metroid Prime presents its title
  screen; Animal Crossing gets past the audio ARQ handshake, reads its disc and displays the Nintendo
  logo.
- **RVZ disc images.** Compressed GameCube images in Dolphin's RVZ container (Zstandard) can be
  mounted read-only, the same way an ISO or GCM is.
- **SDL controller settings.** The SDL build has a pad backend of its own and the full
  "Configure Controller N" dialog: keyboard and game controller bindings side by side, analog sticks
  and triggers, Clear / Default / OK / Cancel.
- **New icon.** The cube icon was recoloured with the blue palette of the logo and regenerated as
  `pureikyubu.ico` (16–256 px, 32 bpp); the SDL builds embed the same pixels and call
  `SDL_SetWindowIcon`.

![Controller settings in the SDL UI](imgstore/controller_settings.png)

## Added

### Gekko

- x86-64 recompiler for Gekko basic blocks (`src/gekkojit.h`, `src/gekkojit.cpp`). Every block
  records the translation state (generation counter) it was compiled under, so a write to a BAT,
  `SDR1`, `HID0`/`HID2` or `MSR`, or an `rfi`, `tlbie`, `icbi`, `dcbst` or cache flush, drops all
  blocks in O(1). Untranslated instructions run through `Interpreter::ExecuteDecoded` and share the
  interpreter's decode cache, so both engines decode identically.
- 8192-entry decode cache keyed on the pc and the fetched instruction word, and a direct-mapped TLB
  of 2048 page translations instead of an `unordered_map` that allocated a `TLBEntry` per page.
- Paired-Single translation (`src/gekkojit_ps.cpp`, `-DGEKKO_JIT_PS=0` builds it out): `add`, `sub`,
  `mul`, `div`, `res`, `rsqrte`, `madd`, `msub`, `nmadd`, `nmsub`, `muls0/1`, `madds0/1`, `sum0/1`,
  `sel`, `mr`, `neg`, `abs`, `nabs`, `merge00/01/10/11` and the `_d` recording forms. The x86-64
  encoder, host register roles and `GekkoRegs` offsets moved to `src/gekkojit_x64.h`.
- Quantised paired loads and stores (`psq_l` / `psq_st`): all eight indexed/non-indexed,
  update/non-update and W = 0/1 forms, with `HID2[PSE]`, `HID2[LSQE]` and the `RA != 0` update rule
  decided at compile time. The GQR is read at run time by `Jit::PsqLoad` / `Jit::PsqStore`, so
  writing one invalidates nothing.
- Debugger command `jit 0` / `jit 1`; 32-bit and non-x86 hosts, and `-DGEKKO_JIT_DISABLED` builds,
  fall back to the interpreter.

### Disc images

- Read-only support for Dolphin's RVZ container (`src/rvz.cpp`): the container header and its SHA-1
  checksums, the data-entry and group tables, the `None` and `Zstandard` compression methods and the
  "packed" representation that regenerates runs of pseudo-random disc junk from an LFG seed.
  bzip2/LZMA groups and Wii images are reported as unsupported. Zstandard is bundled under
  `thirdparty/zstd`.
- `.rvz` is accepted everywhere a disc image was accepted: the command line, the game selector and
  the file dialogs.

### Input and user interface

- SDL pad backend (`src/padsdl.cpp`): a pad can be driven by the keyboard (`SDL_GetKeyboardState`,
  SDL scancodes in `VKEY_FOR_*`) and by an SDL game controller (`GCKEY_FOR_*` button and axis
  identifiers) at the same time; the Nth connected controller drives the Nth port. The left stick
  drives the main stick, the right stick the C stick and the triggers the analog L/R (digital L/R
  fire at 50%); the keyboard 50%/100% keys still work, and both sources are summed and clamped
  instead of overflowing `int8_t`.
- The controller-settings dialog in the SDL UI (`uisdl.cpp`, ported from the Win32
  `PADConfigDialogProc`): Options → Controllers → Port 1..4 opens "Configure Controller N" with the
  Buttons table on the left and the Control Stick / C Stick tables on the right, each with its own
  Control | Keyboard | Gamepad columns. Clicking a binding arms capture, the next key, controller
  button or stick/trigger deflection becomes the binding and the event is swallowed; Esc cancels and
  modifier keys and F1–F12 are skipped for the keyboard.
- Game controllers are opened and closed by the thread that pumps SDL events
  (`PADUpdateControllers`) and read by the emulation thread through a spin-locked snapshot;
  `PADLoadConfig` is public so the dialog can apply the bindings without restarting emulation.
- New application icon: the cube artwork recoloured to the logo palette
  (`src/res/pureikyubu_icon.svg`) and embedded in `src/res/pureikyubu_icon.h` for SDL_SetWindowIcon
  (the SDL port has no `.rc` resources); `pureikyubu.ico` regenerated at 16–256 px, 32 bpp.

### Command line and tooling

- `pureikyubu <file>` (or `--image <file>`) loads and runs the image immediately instead of stopping
  in the game selector, and `--help` prints the accepted options to the console and to the report
  log, so `EMU_LOG=<file> pureikyubu --help` works too.
- `--bench <file> [seconds]` runs the emulator unattended and reports throughput plus
  `Gekko::CpuStats` (translated vs interpreted instructions, basic-block length, re-translations and
  their causes, cache-line fills and every access that leaves the cache for the PI/MEM);
  `BENCH_PROFILE=1` adds a host cycle breakdown and `BENCH_STATS=1` a guest instruction histogram.
- The Gekko differential harness gained `BENCH_SPIN*` modes (which reproduce and dissect the thread
  interference on the time base), `BENCH_PS_TEST`, `BENCH_FUZZ_PS` and the `psq` workload mix;
  `build_win.bat` builds the harness with MSVC so the two compilers can be compared on one workload.

### Testing

- Serial-interface spec tests (`testing/`): 17 new cases covering the SIPOLL round trip and its
  independent halves, per-channel output buffers, read-only input buffers, COMCSR bit positions and
  TSTART, the poll interval / budget / vertical-blank anchor, per-channel enables and the read-status
  interrupt with its mask.
- Command Processor spec tests (`testing/gfx_cp_spec_test.cpp`, 19 cases) written from
  `specs/architecture/command-processor.md`: the VCD/VAT field positions, the VAT component and
  colour byte counts, the vertex attribute order and `gx_vtxsize`.
- Five aggregation tests for the DSP interrupt line (each cause raises the line only through its own
  mask, a real ARAM DMA completion reaches the PI, a masked cause latches without a line, an
  acknowledgement drops it, and the mailbox interrupt reaches the PI), plus the corrected DSP vector
  test and the latched CPU→DSP request test. The suite is at 367 tests.

## Changed

### Performance and threading

- The VI scan-out and the serial poll are executed by the CPU thread from the tick it already
  advances (`GekkoCore::Tick` / `TickN` call `Flipper::Update`), so VI timing is exact instead of
  sampled by a thread.
- The Command Processor thread blocks on a new portable `Event` (`utils.h`) that the CPU thread
  signals when a batch of FIFO entries is due (`CommandProcessor::TickSync`); the drain stays at the
  emulated `tickPerFifo` rate.
- The AI DMA thread is woken by `DSP::AITickSync` (about every 6750 ticks at 48 kHz) and parks itself
  with `Suspend()` when no DMA is armed. The DSP thread is woken by `DspCore::TickSync` once per
  `DspWakeTicks` and drains a batch, so the emulated DSP no longer self-throttles by re-anchoring its
  deadline on every poll; the mailbox hold-off is now a tick deadline
  (`DspCore::MailboxHoldTicks`).
- `mtmsr` and `rfi` drop the compiled blocks only when `MSR[IR]`/`[DR]` actually change; the OS
  toggles `MSR[EE]` around every critical section and dropping the whole cache for it cost ~85,000
  needless re-translations per second.
- Address translation, the tick and the branch-condition helpers are really inlined now
  (`GEKKO_INLINE`).

### Behaviour and configuration

- The Windows and SDL builds keep separate settings: `DefaultSettingsWin.json` / `SettingsWin.json`
  and `DefaultSettingsSdl.json` / `SettingsSdl.json`. The two ports run from the same directory and
  used to overwrite each other's configuration; each executable now embeds its own file names.
- The serial interface follows the documented register layout: `SICnOUTBUF` is the CPU-visible
  register with the shadow read by the transfer engine, `SICOMCSR` latches CHANNEL and INLNGTH on an
  ordinary write and `TSTART` reads back as a pending bit, and the poll register drives `SIPoll(line)`
  directly.
- The Flipper device threads and the emulator use the same `Event` abstraction, so the timing code is
  portable between Win32 and SDL.

## Fixed

### Interrupts and timers

- **DSP/DI aggregate interrupt lines.** Every cause in CDCR and DI_SR has its own mask bit, and the
  pairs are not adjacent; both re-evaluation helpers ANDed the group of causes with the group of
  masks, so the lines were never asserted at all. Animal Crossing hung in its ARQ wait because the
  ARAM DMA completion never reached the CPU. Both lines now follow the OR of `(cause && its own
  mask)` and are re-evaluated from `DSPUpdateInt()` / `DIUpdateInt()` whenever a cause or a mask
  changes.
- **A cleared cause can drop the line.** The lines were only ever dropped inside one `write_cdcr` /
  `write_sr` branch; clearing one cause while another was still latched left the line high with
  nothing left to clear it, so the CPU re-entered the external-interrupt handler on every `rfi`.
  Animal Crossing was executing ~11,500 exceptions a second.
- **Serial-interface poll storm.** The emulator polled on a fixed 0x10000-tick interval (about ten
  times the hardware frame rate) instead of the SIPOLL video-line schedule, so `RDSTINT` was
  re-raised continuously. On Animal Crossing the emulated time for a fixed wall clock went from 3.8 s
  to 74 s, exception entries from 165,000 to 2,213 and VI interrupts from 0 to 2,178; Zelda's
  exception count dropped from 49,635 to 2,157.
- **DSP microcode vectors.** The interrupt vector base was taken from the live CDCR use-rom bit
  instead of the program that is running; Zelda's microcode installs a CPU→DSP vector at 0x000E in
  IRAM and the loader's vectors made it bounce the command. CDCR bit 1 is now latched as a request
  and re-tried until the core accepts it through the TE3/ET gate, so a request that arrives while
  `te3` is closed is no longer lost.
- **DSP/AI threads.** `DspAIOpen` cleared its state with `memset`, which also wiped the `Event` the AI
  thread waits on (a wait on a zeroed handle returns immediately, so the thread silently went back to
  spinning); the clear is now an explicit `DspAIControl::Reset`.

### Graphics

- **Presentation (issue #361).** `pe.cpp` switched the VI output off after the first
  `GXDrawDone`/PE token, which stopped scan-out for the rest of the run in titles that keep
  presenting; the gate had no hardware meaning and is gone. In the SDL build the VI picture was
  written into the same window that carries the OpenGL context, so the two presenters alternated; the
  GL backend owns the window while it runs and the VI still decodes the XFB and counts frames.
- **Command Processor.** `CP_FIFO_COUNT` was a stale software register and is now the live FIFO
  occupancy ("the distance between the write and the read pointer"). The break-point status bit was
  gated on `CP_ENABLE[FIFOBRKINT]` instead of `[FIFOBRK]`, so a title that armed the break point
  without the interrupt never saw the flag. `CommandProcessor::ResetFifoProcessor()` drops the
  CP-side command buffer so a new stream starts clean.
- **VI.** `VI_DTV_STATUS` (0xCC00206E) is implemented instead of falling through to the default case,
  where it returned 0 and made the SDK's `VIGetTvFormat()` always report NTSC.
- **Texture decoder (issue #359).** IA4 walked the tiles column first while the tiles of an image are
  stored row by row, so a map wider than one tile came out transposed (and the report encoder mirrored
  the same walk, which is why the gallery never showed it); the debugger's `gxtexdump` read the colour
  fields after the decoder had serialised the texels into the R,G,B,A byte order GL wants, which
  rotated the channels of every non-grey format; C14X2 masked its palette index with 0x3ff instead of
  0x3fff; and the transparent fourth colour of the CMPR three-colour mode lost the RGB of the
  endpoint average.
- **Texture uploads.** `DecodeTexture` compares the description and an FNV-1a hash of the raw texture
  bytes (plus a TLUT generation for the palette) and keeps the GL image when nothing changed; the
  upload reuses the image storage with `glTexSubImage2D`, the sampler parameters and the mip chain
  are only re-applied when `TX_SETMODE0/1` really change, and `TX_INVTAGS` (`GXInvalidateTexAll`)
  marks every map for a decode again.

### Interface and platform

- `File → Reopen`, a dead menu entry, is wired to the last loaded image (the `LASTFILE` setting the
  selector already maintains) with F3 as its accelerator.
- Writes to the SI input buffer registers no longer halt the emulation.
- The Gekko sources are valid UTF-8 (two CP1252 comment bytes in `gekko.cpp` / `gekkoc.cpp` were
  normalised).
- The DSP unit tests link a counter double (`testing/dsp_test_support.cpp`) now that the DSP sources
  are built without `gekko.cpp`, and the mailbox hold-off tolerates the null `Core` the tests use.

## Known issues

- Bump mapping, indirect texturing, the Z-texture environment and Cpu2Efb are not emulated yet.
- PE dither and a few `SU` flag fields are deliberately unimplemented and documented in
  `wiki/gfx.md`.
- Full JAudio microcode support (issue #71) is still in progress; the remaining DSP divergences from
  the hardware are recorded in `testing/Readme.md`.
- RVZ support is read-only: bzip2/LZMA groups and Wii images are not implemented.
- The recompiler needs a 64-bit x86 host; 32-bit and non-x86 builds, and `GEKKO_JIT_DISABLED`, fall
  back to the interpreter.
- The Linux build still has no sound and no input.
- Some titles still fail to render or hang; compatibility is a work in progress.

## Building

**Windows.** Open `scripts/VS2026/pureikyubu.sln` in Visual Studio 2026 and build. SDL and Win32 front
ends both have Debug and Release configurations.

**Linux.**

```sh
sudo apt install libglew-dev
git clone https://github.com/emu-russia/pureikyubu.git
cd pureikyubu
git submodule update --init
cd build && cmake .. && make
./pureikyubu pong.dol
```

**Tests.**

```
MSBuild scripts/VS2026/pureikyubu_test.slnx -p:Configuration=Release -p:Platform=x64
vstest.console.exe scripts/VS2026/x64/Release/pureikyubu_test.dll /Platform:x64
```

---

*Nintendo GameCube is a trademark of Nintendo. This emulator is not affiliated with Nintendo.*
