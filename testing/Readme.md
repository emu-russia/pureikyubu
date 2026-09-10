# testing

`CppUnitTest` unit tests for the emulator. The test project
(`scripts/VS2026/pureikyubu_test.slnx`) links the emulator sources **as file references** —
the sources are never copied, so the tests always exercise exactly the code that the emulator
is built from.

## Layout

| File | Contents |
|---|---|
| `pch.h`, `pch.cpp` | The test precompiled header; it simply reuses the emulator's `src/pch.h` |
| `dsp_test_common.h` | Test machine (`DspTestMachine`), flag reference model (`FlagRules`), instruction encoders (`Enc`), file helpers |
| `dsp_test_support.cpp` | Link-time support: the DSP sources are compiled into the test DLL, so the few emulator-wide entities they reference (console main memory, ARAM, PI/MI hooks, JDI node, `Debug::Report`/`Halt`) are supplied here instead of dragging in all of Flipper/Gekko |
| `dsp_control_test.cpp` | Control transfer, `loop`/`rep`/`pld`/`mr`, PSR bit ops, bit tests, stacks, data moves |
| `dsp_core_test.cpp` | Every non-parallel computing instruction (immediate ALU, step ops, accumulator ALU, logic, multiply family) |
| `dsp_parallel_test.cpp` | Packed (parallel) words, `ldd`/`ls`, circular addressing, data-memory addressing modes, XL mode |
| `dsp_flags_test.cpp` | The `C V Z N E U` flags: `ModifyFlags` against an independent reference model, plus end-to-end flag checks per instruction |
| `dsp_mailbox_test.cpp` | The CPU↔DSP mailbox protocol |
| `dsp_irom_test.cpp` | The `build/Data/dsp_irom.bin` boot ROM: disassembly, decode coverage and execution of its boot path |
| `dsp_golden_alu_test.cpp` | Differential test of the data path against the golden vectors in `dsp_golden_alu_vectors.h` |
| `dsp_golden_alu_vectors.h` | Generated: results and flags of ~180 instruction words from the hardware core (see below) |
| `DspIrom.md` | The IROM disassembly and analysis report |

## Building and running

From the repository root (MSBuild and the VS test host are used directly so the tests can also
be run from a shell):

```
cd scripts/VS2026
MSBuild pureikyubu_test.slnx -p:Configuration=Debug -p:Platform=x64
"$VS/Common7/IDE/CommonExtensions/Microsoft/TestWindow/vstest.console.exe" x64/Debug/pureikyubu_test.dll
```

`x64/Debug/pureikyubu_test.dll` is a VS "native unit test" DLL, so the same tests can be run
from the Visual Studio Test Explorer after opening `pureikyubu_test.slnx`.

To run a single test, pass a filter, e.g.

```
vstest.console x64/Debug/pureikyubu_test.dll /Tests:Irom_DumpDisassemblyForReview
```

## Notes

* The tests drive the DSP core through its public API (`DspCore::Step`, `regs`,
  `TranslateIMem`/`TranslateDMem`, `Dsp16::ReadDMem`/`WriteDMem`, the mailbox accessors),
  exactly as the debugger does. Nothing is poked around behind the emulator's back.
* Instruction words are produced by the `Enc` builders, which follow the operand encodings of
  dsp-isa.md section 3 and the opcode spaces of section 6. They are written by hand, so they
  also act as an independent check of `src/dspdec.cpp`.
* Expected flag values come from an independent implementation of the specification's flag
  rules (`DspUnitTest::FlagRules`), not from the emulator's own `ModifyFlags`.
* `Irom_DumpDisassemblyForReview` regenerates `build/Data/dsp_irom_disasm.txt`. It is part of
  the suite on purpose so that the published analysis can be re-checked with one command.

## The golden vectors

`dsp_golden_alu_vectors.h` holds 1822 vectors captured from the hardware core (a gate-level
simulation of the DSP), one per instruction word and operand set: the instruction word, the
operand registers to load, and the resulting `a`, `b`, `p` and the low six PSR bits
(`C V Z N E U`). Both the words and the expected values come from outside the emulator, so the
table also catches rules that the specifications describe wrongly — the carry of `max` is one
such case, and the shift family turned out to work quite differently from dsp-isa.md 4.6.

The table covers the data-path instruction space: the immediate and accumulator ALU families,
the logic family, the shift family in all its register forms, `addc`/`subc`/`negc`, `norm`,
`div`, `max`, the multiply/accumulate family, and the two-word long immediates. Memory
operands are not part of the sweep (they need the address-unit state, and are covered by
`dsp_parallel_test.cpp` instead).

`DataPath_GoldenResultsAndFlags` runs every vector through the emulator and compares the four
observable values. The handful of vectors that still disagree are listed in the test as
`kOpenDivergences`: the test fails both when a *new* disagreement appears and when a listed one
stops reproducing, so the list can only shrink.

## What is not covered (and why)

* **ARAM accelerator and the hardware deADPCM/IIR decoder.** `src/dsparam.cpp` is linked into
  the test DLL (so its code is compiled and linked against the test doubles), but the tests do
  not drive `ACYN`/`ACDL`/`ADM`/`ACPDS`, because those registers need the ARAM SDRAM
  controller and the Flipper memory interface rather than the DSP core. The DSP-DMA path *is*
  covered: `src/dspdma.cpp` runs against a simulated console main memory.
* **`ld d,*a1,m` / `st *a1,m,s` (opcodes `0x2A00`/`0x2B00`).** dsp-isa.md section 4.12 lists
  these accumulator-indirect forms but does not define the effective address they use, so the
  decoder still treats that opcode space as `stsa` with an invalid source code. The emulator
  reports it (through `Debug::Halt`) instead of executing something wrong; implementing it
  needs the ARAM accelerator semantics.
* **`pst` (program store, `0x0230`/`0x0330`).** This core does not have it: dsp-isa.md
  section 4.3 ("`pst` (program store) was removed from this core's list — program memory is
  written only by DSP-DMA") and dsp.md section 2.9 both say so, so the decoder keeps that
  opcode space reserved. The reserved-word tests keep
  the emitted space honest).
* **The DSP `wait` state and the interrupt latency model.** Only the flag/vector side is
  covered (the reset vector, `trap`, the enable bits).

## Known deviations that the tests pin down but do not fix

These are places where the emulator deliberately simplifies the hardware. They are recorded
here (and asserted where possible) so that they are not mistaken for verified behaviour:

* **`eas` and `lcs` have independent stack pointers.** dsp.md section 2.5 says the two stacks
  share one pointer; the interpreter keeps them separate because `Dispatch` reads `lcs->top()`
  and `eas->top()` as two independent stack tops. The `loop`/`rep` machinery is consistent
  either way; only microcode that explicitly pushes/pops `eas`/`lcs` through `ld`/`st`/`mv`
  (which the IROM does not) could tell the difference.
* **The DSP-side AI-DMA registers (`0xFFBB`/`0xFFBD`/`0xFFBE`/`0xFFBF`) and `AMDM` (`0xFFEF`)
  are not emulated.** Writes to them are reported and dropped. They belong to the AI-DMA
  engine, which lives on the Flipper side (`src/dspai.cpp`) and is outside this test scope.
* **A two-word instruction placed in the last word of IRAM (`0x0FFF`) or IROM (`0x8FFF`)**
  reads its second word past the end of the memory array; `Decoder::Decode` is handed a raw
  pointer and never checks `instrMaxSize`. The IROM image never does this (its last code word
  is a `rets` at `0x88EA`).
* **`ls` performs the load before the store.** dsp.md section 3.2 describes the hardware page
  order as "store of the old value, then load"; the two orders only differ when the load and
  the store address registers alias.
* **The XL store clamp recomputes the extension condition from the value** instead of testing
  the `E`/`V` flags directly (equivalent while `V` implies `E`).

## Open findings from the golden vectors

These are real disagreements with the hardware core that the sweep found and that are not
fixed yet. They are listed in `DataPath_GoldenResultsAndFlags` as `kOpenDivergences`, so the
suite stays green while the list is accurate and fails as soon as it changes.

* **`addp d,s` (`0xf800`-`0xfbff`) computes a different sum.** For most operands the core's
  result is `d + (s << 16) + (P & ~0xffff)` — the product's low word is dropped and the source
  is added at bit 16 — but that formula does not reproduce every operand set, so the exact rule
  (probably a 16-bit-sliced addition of the product's carry-save form) still needs to be
  pinned down. 64 vectors.
* **`sub d,p` (`0x5e00`/`0x5f00`) reports a different carry.** The core reports the borrow of
  `d - p` itself (`C = 1` while `d < p`), the emulator the complemented borrow of the C2 rule.
  Implemented for the product *source* in `p_sub`, but the word still takes another path, so
  the fix does not reach it yet. 20 vectors.
* **The "other accumulator half" shift forms (`0x3c80`-`0x3f80`, `0x02ca`-`0x03cb`) with a
  source whose low byte differs from the full value** shift by the full 16-bit value instead of
  by its low byte (e.g. `b1 = 0x7fff` must shift right by one, the emulator shifts by 32767 and
  ends up with zero). 16 vectors.

Two smaller findings that the sweep produced and that *are* fixed, for the record: `div` took
its carry from a wrongly sized mask (so the low quotient bit was always zero), and every
handler whose 40-bit result was computed in a wider intermediate reported flags for the wide
value — `ModifyFlags` now masks its inputs to 40 bits, which brought the `add`/`sub`/`admpy`
product-source flag rows and the immediate shifts in line.

# GFX (Flipper graphics) tests

The graphics subsystem has its own suite (`gfx_*_test.cpp`, `gfx_test_common.h`,
`gfx_test_support.cpp`). The emulator sources are linked into the test DLL exactly like the DSP
ones, and the tests drive the **real OpenGL backend**: a hidden window, the real `GFXCore`, the real
XF vertex shader and TEV fragment shader. Nothing is stubbed out except the Flipper devices
(`MemoryInterface`, `ProcessorInterface`, `VideoInterface`), which the support file replaces.

| File | Contents |
|---|---|
| `gfx_test_common.h` | The test machine (`GfxTestMachine`), the hidden-window OpenGL backend, the PNG/report helpers |
| `gfx_test_support.cpp` | Link-time doubles (main memory, the PI register window, the VI/CP hooks) and the machine implementation |
| `gfx_rig_test.cpp` | Smoke tests for the rig itself (the GL context, the shaders, the frame clear, transform feedback) |
| `gfx_su_ras_test.cpp` | `GEN_MODE`/`GEN_MSLOC`, the scissor box, the line/point size, the 0x30-0x3F texture sizes, `RAS1_TREF`/`SS`/`IREF` |
| `gfx_xf_test.cpp` | The XF register file, the CP read-back handshake, the geometry/projection transform, lighting, every texgen type, the dual transform |
| `gfx_tev_test.cpp` | The TEV register file, the combine against an independent reference model, bias/shift/sub/clamp, the Rev B K constants, the alpha function and the render gallery |
| `gfx_pe_test.cpp` | The PE register file, the Z test/write mask/`zfreeze`, blending, the logic op, the write masks, the copy engine's clear |
| `gfx_bp_map_test.cpp` | The BP register ownership map: every register 0x00-0xFF must be decoded by the block the specification assigns it to |
| `gfx_cp_test.cpp` | The CP FIFO: the register window, a display list executed burst by burst, the frame counters |
| `gfx_bump_test.cpp` | The indirect (bump) registers and the coordinate offset arithmetic, checked by rendering a ramp texture with and without the offset |
| `gfx_texture_test.cpp` | The texture formats (I4/I8/IA4/IA8/RGB565/RGB5A3/RGBA8/CMPR) and the palette (TLUT) lookup, with the alpha expansions checked through the alpha function |

## How the tests observe the pipeline

* **Transform feedback.** `GfxTestMachine::RunVertexShader` links the XF vertex shader into a
  program with the transform feedback varyings enabled, feeds it vertices and returns the exact
  clip position, texture coordinates and colours it produced. This is how the XF is tested.
* **Rendering.** `BeginFrame` / `DrawQuad` / `ReadColorPixel` run a draw command through
  XF → SU → RAS → TEV → PE and read the EFB back with `glReadPixels`; `ReadDepthPixel` reads the
  depth buffer, which is how the Z path is tested.
* **The debug log.** `Debug::Report` is captured (`EnableTestLog` / `TestLogText`), so a register
  that no pipeline block decodes is caught by asserting that no "Unknown reg load" was reported.

The rendering tests also publish their images into `x64/<Config>/gfx_test_out/gfx_report.html`
(half-size PNG screenshots of the EFB, generated by the `Util::SavePng` writer in `src/utils.cpp`).

The tests need an OpenGL 3.3 capable driver; when no context can be created they fail with a
readable message instead of crashing.

## What the GFX tests do not cover (and why)

* **The CP's vertex fetch (VCD/VAT/FIFO walk)**. The display list itself is executed burst by burst
  (`CommandProcessor::PumpFifo`, the deterministic equivalent of one CP thread tick), but the vertex
  array fetch needs a full VCD/VAT setup and a game-like vertex layout; the pipeline is tested with
  hand-built vertex rows instead.
* **Texture formats.** The texture engine is exercised end to end (a texture is decoded and sampled
  by the TEV in the render tests), but there is no per-format golden test: the specifications in
  `gamecube-specs` describe the tile shapes and the component expansion, but not the (s, t) to
  main-memory offset formula or the texel order inside a 32-byte tile, so such a test would only
  pin down whatever the decoder already does.
* **Bump mapping and indirect texturing.** The register file is emulated and dumped, but the
  indirect coordinate arithmetic is not: see `wiki/gfx.md`.

