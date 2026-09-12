# DSPcore interpreter / recompiler benchmark

A standalone harness that links the real DSP core (`dsp.cpp`, `dspcore.cpp`,
`dspdec.cpp`, `dspdma.cpp`, `dsparam.cpp` and `dspjit.cpp`) with a stub `pch.h` and a
flat console main memory, so that the cost of the two execution engines can be measured
and compared without SDL, OpenGL or the rest of Flipper.

It is a development tool, not part of any build. It is the DSP counterpart of
`testing/gekko_bench`.

| File | Contents |
|---|---|
| `pch.h`, `stubs.cpp` | Minimal replacement for `src/pch.h` (Debug, the threading types, the Gekko tick source, Flipper's memory/PI stubs, JDI, Util) |
| `bench.cpp` | The harness: loads the IROM or a workload into IRAM, runs the interpreter and/or the recompiler, prints MIPS and a state fingerprint |
| `gen_workload.py` | Generates a synthetic IRAM image from the hardware golden vector table (`testing/dsp_golden_alu_vectors.h`) |
| `build.sh` | Builds the harness with GCC/Clang |
| `check.sh` | Differential test: the recompiler against the interpreter |

## Building and running

```bash
cd testing/dsp_bench
bash build.sh                              # current working tree, -O2
python3 gen_workload.py /tmp/dsp.bin --words 3900 --seed 4242

# interpreter
/tmp/pkdspbench/build/bench irom 1000000
# recompiler
DSP_JIT=1 /tmp/pkdspbench/build/bench irom 1000000

# synthetic data-path workload (a self-looping stream of golden vector words)
/tmp/pkdspbench/build/bench golden 4242 300000
DSP_JIT=1 /tmp/pkdspbench/build/bench golden 4242 300000

# differential test against the interpreter
bash check.sh irom 500000
bash check.sh golden 4242 300000
bash check.sh raw /tmp/dsp.bin 300000
```

The harness prints the number of DSP instruction words retired, the wall-clock MIPS and a
fingerprint of the whole observable core state (the register file, the four stacks and
data memory). `check.sh` parses the count the recompiler actually retired (a compiled
block overshoots the requested total) and runs the interpreter for exactly that many
instructions, so the two fingerprints have to match.

## Modes

| Mode | Contents |
|---|---|
| `irom [N]` | The real 8 KB IROM (`build/Data/dsp_irom.bin`, override with `DSP_IROM`): the mailbox handshake, the command dispatcher and its wait loop. Loops forever, so `N` can be large. |
| `golden <seed> [N]` | A deterministic stream of words sampled from the hardware golden vector table, ending in a `jmp 0` self-loop. Every word is one the real core executes. |
| `raw <file> [N]` | A raw big-endian 16-bit IRAM image (what `gen_workload.py` writes). |
| `nop [N]` | IRAM full of `nop`s (with a `jmp 0` loop at the end) - the pure block-dispatch throughput. |
| `sweep [from] [to]` | Every instruction word in the range, from a fixed state, through the interpreter and (with `DSP_JIT=1`) the recompiler. `Debug::Halt` returns instead of stopping, so undefined words are covered. Built with `-fsanitize=address` it is how a memory error is traced to the exact word. |

## Environment

| Variable | Effect |
|---|---|
| `DSP_JIT=1` | Run the recompiler instead of the interpreter |
| `DSP_VERBOSE=1` | Print the core's `Debug::Report` output |
| `DSP_IROM=<path>` | The IROM image to load (default `build/Data/dsp_irom.bin`) |
| `DSP_JIT_BLOCK=<n>` | Limit a compiled block to `n` words (bisecting a codegen problem) |
| `DSP_ENTRY=<pc>` | Start at an address other than the mode's entry point |
| `DSP_TRACE=1` | Print every basic block the recompiler runs (start pc and retired count) |
| `DSP_DETAIL=1` | Print the extended register/stack/memory state after the run |

## ABI regression build

`src/jit_x64.h` is shared with the Gekko recompiler. Build with
`OPT="-DDSP_JIT_TEST_WIN64_SHADOW"` to have the emitter poison the Win64 shadow space
before every helper call, which reproduces the Windows-only hazard on a Linux host.

`OPT="-DDSP_JIT_DISABLED"` builds the interpreter-only configuration.

## Results

Measured with `-O2` on one x86-64 host, median of three runs, 10 million instructions
each (`interp` = `DspCore::Step`, `jit` = `DspCore::RunJitBlock`):

| Workload | Interpreter | Recompiler | Speedup |
|---|---|---|---|
| `irom` - real boot microcode | 97.2 MIPS | 150.7 MIPS | 1.55x |
| `nop` - pure block dispatch | 138.7 MIPS | 234.8 MIPS | 1.69x |
| `golden` - synthetic data path | 35.8 MIPS | 61.3 MIPS | 1.71x |
| `raw` - generated workload | 34.9 MIPS | 63.3 MIPS | 1.81x |

The speedup is bounded by how much of the interpreter's time is fetch/decode/dispatch -
the part the recompiler removes by baking the decoded instruction into the generated call.
The handlers themselves are shared, so the two engines can never disagree about what an
instruction *means*; `check.sh` and `testing/dsp_jit_test.cpp` pin that down.
