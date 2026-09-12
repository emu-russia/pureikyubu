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
| `build.sh` | Builds the harness from the current working tree (GCC/Clang) |
| `build_win.bat` | The same build with MSVC, so that the two compilers can be compared on one workload |
| `bench_win_prof.cpp` | Empty `ProfStart`/`ProfStop` for the MSVC build (the sampler is POSIX) |
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

## The emulator's background threads

The harness runs the CPU core alone: `main` calls `Core->Step()` or
`Core->jit->Run()` in a tight loop. The emulator does not - it also runs the
Flipper-side device threads, and until recently every one of them waited for its
own deadline by reading `Core->GetTicks()` in a tight loop. The time base is the
same location the CPU thread writes on every tick, so each write had to take the
cache line back from every poller, and every poller then pulled it back again.
That is not a micro-optimisation problem: the pollers cost more than the work
they perform.

`BENCH_SPIN=<n>` reproduces it faithfully - it starts n threads that poll
`Core->GetTicks()` exactly like the device threads did - so the effect can be
measured on a workload that does not change underneath the measurement:

```
$ BENCH_JIT=1 BENCH_RAW=workload_alu.bin ./bench dummy 30000000 1000000
Executed 30000018 instructions in 0.103 s = 291.74 MIPS
$ BENCH_SPIN=2 BENCH_JIT=1 BENCH_RAW=workload_alu.bin ./bench dummy 30000000 1000000
Executed 30000025 instructions in 0.214 s = 140.34 MIPS
spinner polls: 16117259 (75.4M/s)
```

Two pollers cost **2.1x** on the same instruction stream, one costs about 1.85x.
The knobs below narrow down what part of it matters:

| Variable | Effect on the poller |
|---|---|
| `BENCH_SPIN_PAUSE=1` | Execute the CPU's spin-wait hint (`pause`/`YieldProcessor`) in the loop |
| `BENCH_SPIN_SLEEP=<ms>` | Sleep between polls instead of spinning (the fully blocked case) |
| `BENCH_SPIN_DIV=<n>` | Touch the time base only every n-th iteration (`0` = never) |

Measured on the `workload_alu` mix at 30M instructions (~292 MIPS without
pollers):

| Configuration | MIPS |
|---|---|
| No pollers | 291.7 |
| 1 poller | 157.7 |
| 2 pollers | 140.3 |
| 2 pollers, `BENCH_SPIN_PAUSE=1` | 152.4 |
| 2 pollers, `BENCH_SPIN_DIV=1024` | 240.4 |
| 2 pollers, `BENCH_SPIN_SLEEP=1` | 288.5 |
| 2 pollers, `BENCH_SPIN_DIV=0` (pure spin, no time base reads) | 130.2 |

Two things follow. Reducing the polling rate helps but does not fix it -
`BENCH_SPIN_DIV=1024` still costs 18%, and a spinner that never touches the time
base at all costs 1.85x, so the damage is the spinning itself (the machine's SMT
siblings and its power budget) and not only the cache-line transfers. And a
blocked wait costs nothing: the remedy is to *not* busy-wait at all.

The emulator was fixed accordingly: the VI/serial update is now executed by the
CPU thread from the tick it already advances (`Flipper::Update`), and the CP
thread blocks on an `Event` that the CPU thread signals when a batch of FIFO
entries is due (`CommandProcessor::TickSync`).

Measured with two binaries built from the same tooling, one with the periodic
work on its own polling thread and one without, on the same command line and the
same wall time. "Real time" is the emulated time the run covered (`tb` divided
by `CPU_TIMER_CLOCK`), which is the number to compare: a faster build reaches a
later point of the game in the same second, so its MIPS figure alone is not the
whole story.

| Workload | Polling threads | Fixed | Speedup |
|---|---|---|---|
| `pong.dol` (interpreted, no caches) | 22.0 MIPS, 1.47x | 55.0 MIPS, 3.66x | **2.50x** |
| Ikaruga (disc, recompiler) | 40.3 MIPS, 2.16x | 49.6 MIPS, 2.66x | **1.23x** |
| Luigi's Mansion (disc, recompiler) | 27.8 MIPS, 1.72x | 36.2 MIPS, 2.25x | **1.30x** |

The uncached case is the one the issue was written about - every Gekko memory
access goes through the PI there - and it is also where the pollers hurt most,
because the interpreter writes the time base on every instruction. A real disc
game gains less: its Gekko thread is only part of the emulator's total load, and
the same pollers are also competing with the graphics work.

## Measuring the emulator as a whole

`--bench <file> [seconds]` runs the emulator without a user interface for the
requested number of seconds and prints the throughput together with the CPU
statistics (`Gekko::CpuStats`). `set BENCH_PROFILE=1` additionally turns on the
host cycle counters, which cost about 20% of the throughput themselves, so the
plain run is the one to quote:

```
> set EMU_LOG=bench.log
> pureikyubu.exe --bench "C:\Isos\NGC\Ikaruga.iso" 15
```

The report answers the question this harness keeps raising - how much of the
emulated work is the CPU, and how much is the rest of the console:

```
instructions       : 602482411
throughput         : 43.08 MIPS
basic blocks run   : 96642645 (6.96 instructions per block)
blocks translated  : 2902777 (193428/s, 3.07% of the blocks run)
block invalidations: 163889 (10921/s)
  mtmsr            : 107959
  rfi              : 26050
  exception entry  : 16248
jit fallbacks      : 22767860 (3.52% of the instructions)
data cache fills   : 932071
pi reads           : 78024 (mmio 77968)
```

The PI/MEM interface is not the bottleneck. On this run the CPU reached it 78
thousand times against 602 million retired instructions (0.013%), and with
`BENCH_PROFILE=1` the whole memory-helper path - every translated load and
store, including the address translation and the cache probe - accounted for
about a tenth of the host cycles, of which filling lines from the PI/MEM was
0.1%. Everything else is the recompiler's own bookkeeping:

```
host cycles (TSC 3.00 GHz):
  jit total        : 13.901 s (92.6% of the wall)
    generated block:  7.319 s (52.7% of jit), 252 cycles/block
    translating    :  2.056 s (14.8% of jit), 2263 cycles/block
    dispatch       :  4.526 s (32.6% of jit)
    interp fallback:  0.460 s ( 3.3% of jit)
    cache fills    :  0.011 s ( 0.1% of jit)
```

(A caveat: `BENCH_PROFILE` costs about 20% of the throughput, because two
`rdtsc` per basic block is not free, so read the *shares* above rather than the
absolute seconds, and quote the plain run when comparing configurations.)

Two thirds of the block-cache drops came from `mtmsr` - the register the OS
toggles around every critical section to enable and disable interrupts, which
cannot change what a compiled block translates to - and one sixth from `rfi`.
Both now only drop the cache when MSR[IR]/[DR] actually change; that removes
about 85,000 cache drops per second on this workload. It does *not* reduce the
re-translation rate by the same factor, because the rate is set by how much of
the guest code is live at once rather than by the drops (the compiles stay at
about 200,000/s, and quadrupling the block cache changes them by a few percent).
The average basic block is only seven instructions long, so the per-block
dispatch and translation are where the remaining time goes.

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

## Paired-Single

The PS translations live in `src/gekkojit_ps.cpp` and are switched independently
of the rest of the recompiler:

```bash
OPT="-O2 -DGEKKO_JIT_PS=0" bash build.sh   # every PS instruction back on the fallback
```

```bash
# Targeted differential test: every translated form over the operand patterns
# that break floating point (NaN, infinity, denormal, both zeroes).
BENCH_PS_TEST=1 ./build/bench pong.dol

# The fuzzer's PS mode: lets the PS arithmetic through and seeds the FPRs with
# raw bit patterns.
BENCH_FUZZ_PS=1 BENCH_FUZZ=600 BENCH_FUZZ_N=96 ./build/bench pong.dol

# A PS-dominated workload (the shape of an SDK matrix kernel).
python3 gen_workload.py workload_ps.bin --body 2000 --mix ps
./check.sh workload_ps.bin 20000000   # for the MIPS figures, see below

# The quantised paired loads and stores, with only the arithmetic that cannot
# make a NaN out of zeroed registers, so the bit-exact comparison holds.
python3 gen_workload.py workload_psq.bin --body 2000 --mix psq
./check.sh workload_psq.bin 20000000
```

Both mixes program HID2[PSE] (and LSQE) themselves with an mtspr in their
prologue: the raw harness leaves HID2 clear, and without those bits the guest
takes an illegal-instruction exception on the first `psq_*`.

The arithmetic workload is a *performance* benchmark and a smoke test, not a
correctness one. It runs random PS operations over registers that the raw mode
starts at zero, so division and rsqrt against zero produce infinities, `inf -
inf` turns them into NaNs and the state ends up full of them - at which point the
bit-exact `check.sh` comparison reports the same NaN-payload difference described
below. The `psq` mix stays finite and its hash does match. The authoritative PS
correctness tests are `BENCH_PS_TEST` and `BENCH_FUZZ_PS`, which now also covers
the quantised forms.

Measured at 20M instructions: 59 MIPS for the interpreter, 114 with the
recompiler and PS built out and 435 with PS translated on the arithmetic mix; 57,
105 and 191 on the `psq` mix. The gap between the last two of each pair is the
point of the module - a PS instruction left on the fallback also ends the basic
block, so it costs a block cache lookup on top of the decode and the dispatch.

The quantised forms call into a C++ helper (`Jit::PsqLoad` / `Jit::PsqStore`)
instead of emitting the conversion in machine code: the generated code computes
the effective address and makes one call, which is still far cheaper than the
fallback. The GQR is read by the helper at run time, so writing one does not have
to invalidate any compiled block.

`BENCH_PS_TEST` reports a second number next to the failures: NaN payload cases.
When both operands of an operation are NaN, which one gets propagated is not
specified by C++ and depends on how the host compiler allocated registers for the
interpreter's own expression, so the two engines can disagree on the sign or the
payload of a NaN result - never on NaN against a number. Those cases are counted
instead of failed because they cannot be fixed from this side. Everything else
must match bit for bit. See the notes at the top of `src/gekkojit_ps.h`.

`BENCH_JIT` selects the recompiler, and an *empty* value counts as "not
selected". That matters: `BENCH_JIT= ./build/bench ...` used to run the
recompiler twice, which makes an interpreter-versus-recompiler comparison look
green while it compares nothing.

## Known gap

The random fuzzer covers everything except branches (those are covered by the
traced workloads) and skips iterations whose random program raises a guest
exception that cannot be resumed. `BENCH_FUZZ_NOJIT=1` runs the interpreter
against itself and is the control: it must report zero failures.
