# Gekko interpreter / recompiler benchmark

A standalone harness that links the real Gekko core (`gekko.cpp`, `gekkoc.cpp`,
`gekkodec.cpp`, `gekkojit.cpp`) with a stub `pch.h` and a flat 24 MB main
memory, so that the speed of the core can be measured without SDL, OpenGL or the
rest of Flipper.

It is a development tool, not part of any build.

## Layout

| File | Contents |
|---|---|
| `pch.h`, `stubs.cpp` | Minimal replacement for `src/pch.h`, `Debug::Report`/`Halt`, `Thread`, and a flat PI |
| `bench.cpp` | The harness: loads a DOL or a raw workload, runs the interpreter and/or the recompiler, prints MIPS and a state fingerprint |
| `prof.cpp` | SIGPROF sampling profiler (host instruction pointer) |
| `gen_workload.py` | Generates synthetic Gekko workloads (instruction mix, code size, data window) |
| `gen_selfmod.py` | Self-modifying-code probe (does the execution engine pick up rewritten instructions?) |
| `gen_ipl.py` | Synthetic boot ROM + game + IPL2: BAT setup, cache enable, `mtmsr`, a per-cache-line `dcbst`/`icbi` flush, `bctr` into the game, a deliberate DSI whose handler returns with `rfi`, and an IPL2 stand-in that calls a `bclrl` thunk |
| `build.sh` | Builds the harness from the current working tree |
| `build_base.sh` | Builds a reference harness from `git HEAD` (used as the differential oracle) |
| `check.sh` | Compares the state fingerprint of the reference, the interpreter and the recompiler |
| `compare.sh` | Throughput table for every workload |

## Building and running

```bash
cd testing/gekko_bench
bash gen_workload.py /tmp/pkbench/workload.bin --body 4000
bash build.sh                              # current working tree, -O2
bash build_base.sh                         # reference build from git HEAD

# interpreter
BENCH_RAW=/tmp/pkbench/workload.bin /tmp/pkbench/build/bench dummy 30000000 1000000

# recompiler
BENCH_JIT=1 BENCH_RAW=/tmp/pkbench/workload.bin /tmp/pkbench/build/bench dummy 30000000 1000000

# differential test against the reference interpreter
bash check.sh /tmp/pkbench/workload.bin 20000000

# random instruction fuzzer (recompiler vs interpreter)
BENCH_FUZZ=5000 BENCH_FUZZ_N=32 /tmp/pkbench/build/bench pong.dol 1

# exhaustive branch test: every branch form x BO/BI x several LR values
BENCH_BRANCH_TEST=1 /tmp/pkbench/build/bench pong.dol 1

# the real IPL2 scenario: real boot ROM, then IPL2 copied into main memory behind
# the CPU's back (EXI DMA) and entered at 0x81300000, with stale data cache lines
python3 gen_ipl.py /tmp/pkbench/ipl2test 8
BENCH_REAL_IPL=$REPO/build/Data/gc-ntsc-11.bin \
BENCH_IPL2_BIN=/tmp/pkbench/ipl2test_ipl2.bin \
BENCH_IPL_STEPS=2000000 BENCH_IPL2_STEPS=200000 /tmp/pkbench/build/bench pong.dol 1
# ... and the same with BENCH_JIT=1. The recompiler retires whole blocks, so it
# overshoots by up to a block; compare against the interpreter at the printed count.

# synthetic boot path (both engines must print the same pc/cr/tb/hash)
python3 gen_ipl.py /tmp/pkbench/ipl 65536
BENCH_IPL=/tmp/pkbench/ipl BENCH_IPL_STEPS=600000 BENCH_GAME_STEPS=1000 \
    /tmp/pkbench/build/bench pong.dol 1
BENCH_JIT=1 BENCH_IPL=/tmp/pkbench/ipl BENCH_IPL_STEPS=600000 BENCH_GAME_STEPS=1000 \
    /tmp/pkbench/build/bench pong.dol 1
```

`BENCH` overrides the scratch directory (`/tmp/pkbench` by default), `OPT`
overrides the compiler flags. `OPT` is how the host-ABI regression build is
selected:

```bash
OPT="-O2 -DGEKKO_JIT_TEST_WIN64_SHADOW" bash build.sh
```

That build makes the emitter poison the 32 bytes above `rsp` before every helper
call, which is the area a Win64 callee is allowed to spill its register arguments
into. SysV hosts have no such area, so without this flag a block that keeps live
values there passes every test on Linux and corrupts them only on Windows. The
whole suite must stay green with it; see "Host ABI" below.

## What the numbers mean

* `bench` runs `GekkoCore::Step()` (the interpreter) or `Jit::Run()` (one basic
  block) in a tight loop, which is what the Windows build's thread procedure does.
* The MIPS figure is host-side: emulated instructions retired per wall-clock second.
* The `state hash` covers the register file and a sample of main memory, so two
  builds that agree on it have executed the same instructions with the same
  results.
* The self-modifying-code probes must produce r5 = 0x1111 (with a proper
  `dcbst`/`icbi`/`isync` flush) and r5 = 0 (without one).
* The boot-path test must end with the same pc, cr, tb and state hash on both
  engines. It also measures the invalidation cost: the `dcbst`+`icbi` loop over
  65536 cache lines takes 0.20 s with a table-walking invalidation and 0.03 s with
  the generation counter.
* The branch test must report 0 failures. It exists because the random fuzzer
  almost never produces a `bclrl` (about one word in 131072): a wrong `bclrl`
  translation made the recompiler derail on the real IPL2 entry and was invisible
  to every other test here.

## Host ABI

A generated block is called from C++ and calls back into C++ for every memory
access, so it has to obey the host calling convention. The one rule that is not
visible on Linux is the Win64 shadow space: a callee may write its four register
arguments into `[rsp, rsp+32)`, so a block may not keep live values there. The
Windows build crashed on the first game because the block kept the exit record
pointer and the update-form effective address in exactly that area; every helper
call overwrote them and the block then wrote its exit record through garbage.
On Linux the same code passed everything, because SysV has no shadow space.

Build with `-DGEKKO_JIT_TEST_WIN64_SHADOW` to make the emitter poison that area
before each helper call. That reproduces the Windows hazard on a Linux host:
with a live value in the shadow space `check.sh` fails immediately (the JIT run
prints no instruction count at all, so the comparison has no hash), and with the
block keeping its state in callee-saved registers the whole suite is green.
Re-run the suite in this configuration after touching the prologue, the epilogue
or the register allocation.

Note what this does *not* cover: the 16-byte alignment of `rsp` at the helper
calls. Deliberately misaligning the frame (48 instead of 40 bytes) still passes
every test here, because the helpers happen to be compiled without aligned SSE
stack spills, so nothing faults. Alignment is therefore guaranteed by the
`static_assert` on the frame size in `gekkojit.cpp` plus the ABI-guaranteed entry
`rsp`, not by this test.

## Known gap

The random fuzzer covers everything except branches (those are covered by the
traced workloads) and skips iterations whose random program raises a guest
exception that cannot be resumed. `BENCH_FUZZ_NOJIT=1` runs the interpreter
against itself and is the control: it must report zero failures.
