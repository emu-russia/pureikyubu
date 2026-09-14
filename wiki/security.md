# Security review

This page is the report for [issue #381](https://github.com/emu-russia/pureikyubu/issues/381):
the emulator's sources were audited for vulnerabilities reachable from the data it loads, every
confirmed defect was fixed, and the checks that were missing are now a small, tested layer
(`src/verify.h`).

The audit and the fixes were done with this source tree alone (no other emulator was consulted for
either the findings or the repairs).

## Threat model

An emulator is a program that parses files. A GameCube emulator parses files that come from third
parties (game images, homebrew, save files), runs code written by those third parties, and is
normally run with the rights of the user. The realistic attacker is therefore:

* a **malformed file** the user is asked to open - a "demo" `.dol`, a patched `.gcm`/`.rvz`, a
  `.map` file shipped next to it, a `.cmd` script, a memory card save, a ROM dump - and
* **guest code** running inside the emulator, which drives every DMA engine in the machine (EXI,
  DSP, DI, AI, PI/CP) with values the emulator must not trust.

A crash is the common outcome of the defects below; several of them were heap or stack buffer
overflows with attacker-chosen content, which is worse than a crash.

## What the emulator takes from the outside

| Artifact | Read by | Entry point |
|---|---|---|
| Settings JSON (`Data/DefaultSettings*.json`, `Data/Settings*.json`) | `src/config.cpp` -> `src/json.cpp` | `GetConfig*` (first use, i.e. startup) |
| Bootrom image (`Data/bootrom.bin`) | `src/bootrtc.cpp` | `BootROM()` on every load |
| DSP IROM/DROM (`Data/dsp_irom.bin`, `Data/dsp_drom.bin`) | `src/flipper.cpp` -> `src/dspcore.cpp` | `Flipper::Flipper()` |
| Executables (`.dol`, `.elf`) | `src/main.cpp` | `LoadFile()` (command line, selector, `load`) |
| Disc images (`.iso`, `.gcm`, `.rvz`) | `src/dvd.cpp`, `src/rvz.cpp`, `src/dvddebug.cpp` | `DVD::MountFile()`, `dvd_fs_init()` |
| Memory card saves | `src/memcard.cpp` | `MCConnect()` at startup, guest EXI transfers |
| Command line | `src/main.cpp` | `EMUParseCmdLine()` |
| Emulated IPL ROM and the guest's device registers | `src/pi.cpp`, `src/mem.cpp`, `src/exi.cpp`, `src/bootrtc.cpp`, `src/dsparam.cpp`, `src/dspdma.cpp`, `src/cp.cpp` | memory traps, the CP FIFO and every DMA engine |
| Console scripts (`autoexec.cmd`, any `script <file>.cmd`) | `src/debug.cpp` | `CallJdi("script autoexec.cmd")` on every load |
| Symbol maps (`*.map`, `Data/makemap.dat`) | `src/sym.cpp` | `AutoloadMap()` on every load |

## Method

* The input surfaces were audited one by one, and every reported defect was re-checked by a second
  pass that tried to refute it (line numbers, reachability and severity were all verified against
  the actual source).
* Findings that exist only because a bound was written as `assert()` are real defects here: the
  Release configurations define `NDEBUG`, so the check is not compiled in, and in a Debug build the
  assert pops a blocking dialog. Most of what was found was exactly that.
* Every fix is a check where the untrusted value is used, and the checks that repeat (windows in
  main memory, image sections, FST entries, card transfers, script lines) were moved into
  `src/verify.h`, so the valid range of each field is written down once.
* `testing/security_test.cpp` turns each of those rules into a unit test, including the malformed
  artifacts that used to overflow, spin or exhaust the stack.

## The verifiers

`src/verify.h` holds the rules, and every input path uses them:

| Verifier | What it decides |
|---|---|
| `Verify::Range(offset, size, limit)` | the only place where the two untrusted values are combined; it subtracts instead of adding, so no pair of 32-bit fields can wrap the test |
| `Verify::MainMemory(phys, size, ramSize)` | a window in main memory, with the address masked the way the MI decodes it (the mask allows 64 MB, the allocation is 24 or 48 MB) |
| `Verify::ImageSection(...)` | a section of an executable image: present in the file **and** inside main memory |
| `Verify::DiscRead(position, length, imageSize)` | a disc read, with the signed seek the guest can drive |
| `Verify::FstRoot / FstEntry / FstName` | the disc file system table, whose entries come from the image |
| `Verify::MemcardWindow(cardSize, offset, length)` | a memory card transfer, the length included |
| `Verify::ScriptLine / Verify::ScriptTrim` | the bounded reader for `autoexec.cmd` and the other console scripts |

The memory interface gained length-aware accessors next to the old start-only ones
(`MIGetMemoryPointerForIO(phys, size)`, `...ForDSP`, `...ForPI`, `...ForDebug`, `MIGetMemorySize()`),
so a block copy can no longer start inside RAM and end outside it.

## Findings and fixes

### Settings JSON (`src/json.cpp`, `src/config.cpp`)

The settings parser is a hand-written JSON reader, and the settings file is the first thing the
emulator reads.

| Defect | Kind | Severity | Fix |
|---|---|---|---|
| String token copied into `wchar_t str[0x1000]`, bound as an `assert()` | stack overflow | critical | a runtime limit; the terminator is written inside the array |
| Numeric token copied into `char number[0x100]`, bound as an `assert()` | stack overflow | high | a runtime limit in both the integer and the float reader |
| No recursion depth limit while parsing; the depth limit in the writer was an assert | stack exhaustion | high | a depth counter in the parse context, enforced on both paths |
| `DeserializeObject` spun forever when the document ended after a comma | infinite loop | high | the missing end-of-stream/default cases report a syntax error |
| Literal look-ahead computed `maxSize - 4` in `size_t`, which wraps for a 1-4 byte file | out-of-bounds read | low | a non-wrapping form of the test |
| UTF-8 continuation bytes read past the end of a truncated string | out-of-bounds read | medium | a real bound, including the escape fetch |
| The number readers recomputed "remaining bytes" and wrapped it after an overrun | out-of-bounds read | low | the remaining length is computed once, the invalid state is rejected |
| Element-count cap was `assert()`-only | resource exhaustion | medium | a real runaway-memory guard (high enough that no shipped JDI specification is affected) |
| `strtoull` accepted `-1` and silently wrapped out-of-range numbers | integer overflow | low | canonical form and range are validated before the value is stored |
| A member name could be null when a document was serialized back | null dereference | low | an absent name serializes as an empty name |
| Missing colon was an `assert()`, so a malformed document aborted a Debug build | abort / mis-parse | high | a real check that rejects the document |
| Config accessors dereferenced a missing section (assert-only) and reinterpreted mistyped values | null dereference / wild pointer | high | a real section lookup and a real type check, failing closed with a report |
| A corrupt *user* settings file aborted the emulator | availability | medium | it is reported and ignored; the shipped defaults are kept |

### Executable images and the command line (`src/main.cpp`, `src/utils.cpp`)

| Defect | Kind | Severity | Fix |
|---|---|---|---|
| DOL section size was never checked against the end of the 24 MB RAM buffer | heap overflow | critical | the file range and the RAM window are both verified before the copy |
| DOL section destination pointer could be null (the address mask is 64 MB, the allocation 24 MB) | null dereference | high | the pointer is tested, and the RAM size comes from the memory interface |
| The same two defects in `LoadDOLFromMemory` | heap overflow | critical | the same verifier, with the buffer size supplied by the caller |
| ELF `p_filesz` copied to `p_vaddr` with no RAM or file bound, and as a signed length | heap overflow | critical | unsigned length, the program-header table is validated, every section is verified |
| `wcsrchr` result passed to `_wcsicmp` when the file name has no extension | null dereference | high | the extension is tested in both front ends |
| A disk image that failed to mount was ignored, so the boot sequence read zeroes from an empty drive (a hang) | hang / availability | high | the mount result is checked and the load fails cleanly |
| A failed load left the half-built Flipper object allocated, so the shutdown path crashed | crash on startup failure | high | `EMUOpen` releases the partially built machine and rethrows |
| `Report`/`Halt` formatted into `char buf[0x1000]` with unbounded `vsprintf` | stack overflow | high | `vsnprintf` with the real buffer size |
| `sprintf` into 4 KB/512 B stack buffers when the UI builds a console command from a file name | stack overflow | high | bounded formatting; an over-long value is refused, not truncated |
| `Util::SplitPath` copied path components with unsized copies | stack overflow | low | the copy takes the destination size; a long path fails the map autoload cleanly |
| `Util::FileSave` opened files read-only on Linux; `FileLoad`/`FileSize` ignored failures | logic error | medium | the open mode and the error paths are correct |
| File/dump commands read `args[n]` without checking the argument count | out-of-bounds access | medium | every handler validates its arguments |

### Disc images (`src/dvd.cpp`, `src/rvz.cpp`, `src/dvddebug.cpp`)

| Defect | Kind | Severity | Fix |
|---|---|---|---|
| FST root `nextOffset` drove a byte-swap loop with no bound against the buffer read from the image | heap overflow | critical | the table must fit in the buffer before any entry is touched |
| Negative DVD seek: the sign passed the start-only checks and the length wrapped into a multi-gigabyte `fread` | heap overflow | critical | the seek is rejected and the length is clamped in 64-bit arithmetic |
| FST size from the disc's boot info was read into an address near the end of RAM (up to ~255 MB past the allocation) | heap overflow | critical | the destination window is verified in the boot loader and the size is capped |
| FST entry walk and name-table pointer used image values with no bound | out-of-bounds read | medium | entry and name offsets are verified against the validated table size |
| RVZ: the compressor-data byte was read from a header of exactly minimum size | out-of-bounds read | low | the field is tested for existence before it is read, like every other field |
| RVZ: the chunk size was never compared with the disc, so a crafted header could force a ~4 GB allocation | resource exhaustion | medium | the chunk size is bounded by the disc size and by an absolute cap |
| RVZ: the image file handle leaked on a failed open | resource leak | low | every failure path closes the handle |
| Host paths copied into fixed `wchar_t[0x1000]` fields with unbounded copies | stack/heap overflow | medium | the length is checked against the destination |
| `DumpFst` walked the image's FST byte-swapping 12 bytes per entry with no bound, and copied the name table into `char name[0x200]` | heap and stack overflow | high | entry indices are verified, the FST is capped, names are built from a bounded range |

### Memory cards and the EXI bus (`src/memcard.cpp`, `src/bootrtc.cpp`)

| Defect | Kind | Severity | Fix |
|---|---|---|---|
| Page program tested `offset >= size + size` (a wrapped, start-only check) with an unbounded length | heap overflow | critical | the whole window is verified and the length is bounded by the card |
| Read array had the same check, copying host heap into guest DMA memory | out-of-bounds read (info leak) | high | the same window check on the card and on the main-memory side |
| Sector erase checked only the start of an 8 KB block | heap overflow | high | the whole erased block is verified |
| The DMA pointer was used without a null test, and a valid start could still overrun main memory | null dereference / heap overflow | high | the length-aware accessor is used instead of the start-only one |
| Immediate read shifted by a negative count | undefined behaviour | low | the same result without a negative shift |
| The "extra bytes" address form reported an error and then computed an offset anyway | logic error | medium | the helper fails closed and the callers reject the result |
| A card file larger than 4 GiB was truncated to 32 bits and accepted as valid | integer overflow | medium | the size comparison is done in 64 bits |
| A failing slot A short-circuited the connect (and the flush) of slot B | logic error | low | both slots are attempted |
| MX chip DMA used the transfer length raw for the font/ROM windows and for main memory | heap overflow | critical | both windows are verified at every copy site |
| The SRAM DMA read validated the length and then always copied 64 bytes | heap overflow | medium | it copies exactly what was requested, inside the verified window |
| The SRAM immediate read indexed the 64-byte SRAM with an 8-bit mask | out-of-bounds read | low | the same 6-bit index as the write path |
| The UART receive buffer index was never bounded | out-of-bounds write | high | the index is clamped and the buffer is reported when full |

### DMA engines and the command processor (`src/dsparam.cpp`, `src/dspdma.cpp`, `src/pi.cpp`, `src/cp.cpp`)

| Defect | Kind | Severity | Fix |
|---|---|---|---|
| ARAM DMA checked the start of the transfer only, against a 16 MB buffer | heap overflow | critical | both the ARAM and the main-memory window are verified |
| DSP DMA clipped the DSP side but not the main-memory side | heap overflow | high | the main-memory window is verified with the length-aware accessor |
| The CP FIFO write pointer was compared with nothing; a 32-byte burst could land anywhere in 64 MB | heap overflow | critical | the burst target and the other FIFO pointers are verified, reads included |
| The CP read pointer was used without any bound when feeding the GX | out-of-bounds read | high | the 32-byte burst is verified against main memory |
| A `CALL_DL` display list used the guest address and length with no bound | out-of-bounds read | high | the window is verified and clamped to what the memory holds |
| Vertex array fetches used the guest base/stride with no bound, and a null pointer was dereferenced | out-of-bounds read / null dereference | high | the whole component window is verified; an indirect attribute outside memory is skipped |

### Console scripts and debugger commands (`src/debug.cpp`, `src/jdiserver.cpp`, `src/gekkodebug.cpp`)

| Defect | Kind | Severity | Fix |
|---|---|---|---|
| The script line buffer was filled with no bound, and a script without a trailing newline ran off the end of the file | stack overflow | critical | `Verify::ScriptLine`: bounded, NUL-safe, and an over-long line is skipped |
| Trimming an empty or all-blank line walked a pointer below the buffer | out-of-bounds read/write | medium | `Verify::ScriptTrim` handles the empty line first |
| A script could re-enter itself through `script`/`load` without limit | stack exhaustion | medium | a recursion depth guard |
| An unterminated quote threw out of the tokenizer with no handler anywhere up to the loader | availability | medium | tokenizing reports the bad line and the script continues |
| The same throw in the JDI server for a console-supplied line | availability | medium | the tokenizer reports the failure; the callers answer with an empty reply |
| The control-character pre-pass normalised a local copy instead of the buffer | logic error | medium | the normalisation writes back |
| Profiler interval clamp was inverted | logic error | low | the documented 2-50 ms range is honoured |
| Register commands indexed `gpr/fpr/ps1/spr/sr` with an unvalidated argument | out-of-bounds access | high | the index is verified against the register file |
| `r r0 << 40` shifted by an unvalidated count | undefined behaviour | low | the count is masked to the operand width |
| Disassembly commands dereferenced absent hardware after `unload` | null dereference | medium | they return early when no machine is loaded |
| `sprintf` of a 64-bit register/parameter number into `char def[0x10]`/`def[8]` | stack overflow | high | a wide enough buffer and `snprintf` |

### Symbol maps and banner text (`src/sym.cpp`, `src/ui.cpp`, `src/uisdl.cpp`)

| Defect | Kind | Severity | Fix |
|---|---|---|---|
| RAW map reader copied an unbounded line into `char line[0x1000]` | stack overflow | critical | the copy is bounded and an over-long line is skipped |
| Empty/comment-only map lines trimmed below the buffer | out-of-bounds read/write | high | the empty case is handled before the walk |
| A map line with an address but no symbol name walked past the end of the line | out-of-bounds read | low | the walk stops at the terminator and the line is skipped |
| CodeWarrior and GCC map readers used `sscanf("%s")` without field widths | stack overflow | high | field widths on every conversion |
| `makemap.dat`'s function count was used to index the file with no size validation | out-of-bounds read | medium | the count and the name offsets are validated against the loaded file |
| Function names from `makemap.dat` were widened into `wchar_t[0x100]` with no bound | stack overflow | medium | the copy is bounded; the dead status block was removed |
| Saving a map cast a wide path to `char*`, creating a wrongly named file or failing silently | logic error | low | the path stays wide through the save |
| The DVD banner title was copied with an unbounded loop into fixed buffers in both front ends | stack/heap overflow | high | the copies are bounded by the destination |
| The SJIS-to-Unicode conversion kept reading past a title that ended with a lead byte | out-of-bounds read | medium | the second byte is checked before it is consumed; `malloc` failure is handled |
| Recent-file names shorter than three characters underflowed a length | out-of-bounds read | high | the short case is handled explicitly |

## Catching startup crashes

A startup failure used to look the same from the outside as any other crash: the process
disappeared. Two things were added so that it can be seen and reproduced:

* **`--selftest`.** The emulator runs its whole startup sequence - the debug interface
  specifications, the settings, the emulated hardware, the ROM and memory card files - without
  creating a window, prints a step-by-step report and exits with the number of failed steps as its
  status code. Every step runs in its own catch-all frame, so a broken settings file or a damaged
  image is reported instead of taking the process down. The file on the command line (if any) is
  loaded as part of the check.

  ```
  $ ./pureikyubu --selftest
  [..] emulator core, debug interface specifications
  [ok] emulator core, debug interface specifications
  [..] settings
  [ok] settings
  [..] emulated hardware, ROM and memory card files
  [ok] emulated hardware, ROM and memory card files
  [..] shutdown
  [ok] shutdown
  selftest: the emulator starts, 0 failed step(s)
  ```

* **`testing/startup_cases.sh`.** The same check run against deliberately broken inputs: a settings
  file truncated after `{`, a settings file with a missing colon, an over-long string, a corrupt
  default settings file, and random files renamed to `.dol` and `.rvz`. Each case must be reported
  and must not crash the process (before the fixes, the `.rvz` case hung forever and the `.dol`
  case dumped core while shutting down).

For the unit tests, `vstest.console <test.dll> /Blame` names the test that crashed or hung instead
of leaving the run without a summary.

## Tests

`testing/security_test.cpp` is part of the normal unit-test suite
(`scripts/VS2026/pureikyubu_test.slnx`) and covers:

* **the verifiers** - every predicate against the wrap-around cases the old hand-written checks got
  wrong (a 64 MB mask with a 24 MB allocation, an FST root pointing past the buffer, a card
  transfer starting exactly at the end of the card), plus a property test that compares
  `Verify::Range` with carry-based arithmetic on 200 000 random inputs;
* **the script reader** - ordinary scripts, a last line without a newline, a 5000-byte line, an
  embedded NUL, blank lines, and a property test that reads 3000 random binary buffers with
  canaries around the line buffer;
* **the settings parser** - the malformed documents that used to overflow, spin or exhaust the
  stack must now be rejected (an oversized string, an oversized number, 20000 nested arrays, a
  document truncated after a comma, a one-byte file, a truncated UTF-8 sequence, a stray comma, a
  missing colon), while a well-formed document still yields the same values;
* **the shipped JDI specifications** - all eleven specification texts are parsed through the
  hardened parser, so an over-strict limit cannot take the debug interface down with it.

## What is not covered

* **Deliberate simplifications.** `testing/Readme.md` lists the places where the emulator knowingly
  simplifies the hardware (the DSP stacks, the missing DSP-side AI-DMA registers, the `ls` order).
  None of them is a memory-safety issue and none was changed here.
* **Guest code is trusted with its own console memory.** Guest code can still ask the emulator to
  fill main memory with DMA data, which is what the hardware does; the checks above ensure that a
  transfer cannot leave the memory the hardware has, not that the data is meaningful.
* **The JDI console is not a boundary between local users.** A console command can read and write
  guest memory and files by design; the fixes only remove the cases where a short or malformed
  command could corrupt the emulator instead of doing what it says.
* **Still on `assert()`.** A few internal invariants in the graphics pipeline (`FifoProcessor`'s
  "enough bytes left" checks, the FIFO overflow/underflow paths) are asserts, so a Debug build can
  still abort on a malformed display list. In a Release build those reads stay inside the buffer
  that was validated for the display list, so they are not memory-unsafe; making them runtime
  checks is left for a follow-up.
* **`PITranslatePhysicalAddress` ignores its length argument**, so a debugger command that walks a
  structure near the end of RAM can read past it. The path is debugger-only.

## Reproducing the review

* The verifier rules are documented in `src/verify.h`, and each one is exercised by
  `testing/security_test.cpp`.
* The startup checks are `--selftest` and `testing/startup_cases.sh`.
* An AddressSanitizer build can be made with
  `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"` in the CMake build; the malformed artifacts
  described here are a good starting corpus.
