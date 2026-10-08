# Idle wait skipping

A GameCube title spends a large part of every frame waiting, and the emulator spends just as much
of the host's time emulating that wait. This note describes what the wait looks like in the guest
code, how the recompiler recognises it, how much of a run it accounts for, and what it costs to
skip it.

## What the guest does at the end of a frame

`VIWaitForRetrace` is the SDK call every title makes when it is done with a frame. Its body (from
the Dolphin SDK, and byte for byte the same in the retail Metroid Prime image, `GM8E`,
`0x803890A4`) is a *sleep*, not a spin:

```
803890B8  bl   OSDisableInterrupts
803890BC  lwz  r30, -0x5234(r13)   ; count = __VIRetraceCount
803890C4  addi r3, r13, -0x522C    ; loop:
803890C8  bl   OSSleepThread       ;   park the calling thread on the retrace queue
803890CC  lwz  r0, -0x5234(r13)
803890D0  cmpw r0, r30
803890D4  beq  0x803890C4          ;   until the counter moved
803890DC  bl   OSRestoreInterrupts
```

So the wait itself is the OS: the thread is parked, the scheduler picks the next runnable thread,
and the VI interrupt wakes the parked one. The CPU time is not spent inside `VIWaitForRetrace` -
it is spent in the scheduler round trips that the *other* waits of the title generate while the
frame thread is parked.

Metroid Prime's own end-of-frame loop is where the time actually goes (`0x8030C538`):

```
8030C538  addi  r29, r13, -0x57D4  ; &counter
8030C53C  b     0x8030C544
8030C540  bl    OSYieldThread      ; 0x80385B34
8030C544  lwz   r0, 0(r29)
8030C548  cmpwi r0, 0
8030C54C  bgt+  0x8030C540         ; while (*counter > 0) OSYieldThread();
```

Every iteration of that loop runs the whole scheduler: save the context, pick the next thread,
load the context, restore. Measured over a 300 000 block trace (1 322 583 instructions) taken in
the loading phase, **every instruction of the window was inside this loop**:

| | |
|---|---|
| iterations in the trace | 5 475 |
| instructions per iteration | 242 (min 225, max 46 262) |
| blocks per iteration | 54.8 |
| emulated time per iteration | 6.0 us |
| of it in page `0x80380000` (the DolphinOS image: `SetEffectivePriority`, `OSSaveContext`, `OSLoadContext`, `OSYieldThread`, `OSDisableInterrupts`, `OSRestoreInterrupts`) | 93.3 % |

Over a whole 20 second run the share depends on the phase - the boot and intro screens spend
almost all of it waiting, the first seconds much less - but by page, `0x80380000` is **51 % of all
the instructions the machine retires**, and the wait loop is what those instructions are.

## How it is skipped

The recompiler marks a block as an *idle poll* when it is short (at most 8 instructions), writes
nothing to memory, loads a value and then compares that same register - the `lwz / cmpwi / b`
shape of a wait loop. `Block::pollReg` carries the register the wait is watching.

At run time `GekkoCore::PollCheck` keeps one streak per poll block (64 slots, direct mapped on the
block's pc, because a machine has several waits in flight at once). A poll whose value is unchanged
and which came round within `idlePollPeriodMax` extends the streak; anything else starts it over. A
value that moves is the progress the guest asked for, and a poll that took long to come round again
had real work in between.

When the streak reaches `idlePollThreshold` the CPU is waiting for something outside itself, and
`GekkoCore::SkipIdleWait` covers the wait by advancing the emulated clock instead of executing it:

* one Flipper deadline at a time, so the devices are stepped exactly as they would have been (the
  VI scan-out, the audio DMA, the serial poll, the DSP);
* stopping as soon as an interrupt is pending (`intFlag || decreq` with `MSR[EE]`), because that
  interrupt is what the guest is waiting for and the guest's own handler is what the wait is for;
* stopping after `idleSkipMax` ticks, so a mis-detected loop cannot throw the machine far ahead.

With skipping off the detector still runs, which is what makes the measurements below comparable.

## What it is worth

Both tables are measured with the benchmark's fixed-guest-time mode
(`BENCH_UNTIL_EMULATED`, see `src/bench.cpp`): every configuration executes exactly the same guest
work and only the wall clock differs. The host is noisy, so each configuration is run several times
and the smallest wall time is reported. `fps` is `30 * emulated / wall` - the retrace the emulator
generates runs at 30 Hz in an NTSC interlaced mode.

The skip budget, at 8.06 s of guest time (min of 3 runs):

| `IDLE_SKIP_MAX_MS` | wall | x real | fps | instructions | VI interrupts | skipped |
|---|---|---|---|---|---|---|
| 0 (off) | 13.65 s | 0.590 | 17.7 | 280 M | 231 | - |
| 33 | 7.55 s | 1.069 | 32.1 | 173 M | 232 | 2.99 s (37 %) |
| 100 | 4.07 s | 1.988 | 59.7 | 117 M | 233 | 4.58 s (57 %) |

100 ms is already "until the next interrupt": raising it to 1000 ms changes nothing at all (the
same 245 skips and the same 4.577 s). The cap is a backstop, not the thing that normally ends a
skip.

The detector variants, at 6.3 s of guest time (min of 2 runs):

| variant | wall | x real | speedup | instructions | VI | skipped | AI DMA feeds |
|---|---|---|---|---|---|---|---|
| off | 8.30 s | 0.729 | 1.00x | 208 M | 172 | - | 21 804 |
| `33`, value must not move | 3.15 s | 1.935 | **2.64x** | 117 M | 173 | 2.59 s | 12 741 |
| `33`, value may move | 1.76 s | 3.675 | 4.73x | 25 M | 170 | 5.64 s | 6 720 |
| `33`, stop on `PI_INTSR` | 3.26 s | 1.870 | 2.55x | 116 M | 173 | 2.59 s | 12 713 |
| `100` | 2.13 s | 2.917 | 3.90x | 54 M | 177 | 4.41 s | 5 315 |

* Letting the polled value move keeps the streak alive across the counter's own decrement. It is
  the fastest of the variants and it is **not safe**: the run retires only 25 M instructions over
  6.3 emulated seconds where the hardware would retire about 250 M, so the machine is being starved
  rather than idled. The value-unchanged rule is what keeps the skip on the wait.
* Stopping the skip at a latched interrupt cause (`PI_INTSR`, enabled or not) changes nothing
  measurable on this title: the causes are cleared long before a skip reaches them. It is kept as an
  option because a title that leaves a source masked and polls its handler's memory would need it.
* The frame count is identical in every variant (172-177 VI interrupts over the same guest time),
  and the disc-access path is byte for byte the same (`DVD Read` addresses match the unskipped run),
  which is what says the guest is running the same program.

## What it costs

Skipping a wait removes guest *execution* time, not just guest idling, and the share of the
machine's work that lives in that time comes off the top. The cleanest evidence is the audio
pipeline, which in this emulator is driven by instructions the guest retires (the DSP mailbox
protocol):

| `IDLE_SKIP_MAX_MS` | AI DMA blocks | AIDINTs |
|---|---|---|
| 0 | 29 749 | 1 416 |
| 33 | 20 549 | 978 |
| 100 | 12 796 | 609 |

The audio rate falls in proportion to the skipped time. The frame rate does not: the retrace is
generated by the Flipper on the emulated clock, so a title whose frame loop is retrace-driven keeps
its cadence - which is why the technique is worth having - but the emulated CPU delivers fewer
instructions per emulated second, and anything that competes for them (the audio driver above all)
notices.

**This is accepted for now.** The budget stays at one frame (33 ms), which is the conservative end
of the table above, and the audio pipeline is left alone: making the skip hold back while the DSP or
the AI has work outstanding is the obvious refinement, but it is a change to the audio path rather
than to the idle detector and it is not attempted here. A title whose sound matters more than its
speed can be run with `IDLE_SKIP_MAX_MS=0`, which restores the plain emulator exactly.

## Tuning and reproducing

The feature is on by default with a budget of one frame - 33 ms of the 30 Hz NTSC retrace, the
`strict33` row of the variant table (2.64x on Metroid Prime), with the value-unchanged rule on and
the audio cost described above accepted. The tuning lives in the emulator's settings, in the `core`
section of `Data/DefaultSettings.json` overridden by `Data/Settings.json`:

| setting | meaning | default |
|---|---|---|
| `JIT` | run the Gekko on the basic block recompiler (`false` = the plain interpreter) | true |
| `IDLE_SKIP` | milliseconds of emulated time one skip may cover; `0` turns the feature off | 33 |
| `IDLE_SKIP_POLLS` | polls in a row before the wait is skipped | 16 |
| `IDLE_SKIP_PERIOD_US` | the longest gap between two polls of the same wait, in microseconds | 500 |
| `IDLE_SKIP_STRICT` | `false` keeps the streak across a value that moves (see above: not safe) | true |
| `IDLE_SKIP_STOP_ON_CAUSE` | `true` also stops at an interrupt the guest has not enabled | false |

The settings window edits them on its "Core" page (the JIT toggle was the debugger's `jit` command
alone before), and it applies a change to the running machine at once: the page stores the value and
then drives the core through the debug interface, with `jit` and the `idleskip` command that
re-reads the idle skip settings.

**Skipping needs the recompiler.** The poll a wait is recognised by is a property of a compiled
basic block (`Block::pollReg`), and a machine running on the interpreter has no blocks, so with
`JIT` off the feature does nothing - `idle skips` in the benchmark report stays 0.

The environment variables of the same names (`IDLE_SKIP_MAX_MS`, `IDLE_POLL_COUNT`,
`IDLE_POLL_PERIOD_TICKS`, `IDLE_REQUIRE_SAME_VALUE`, `IDLE_STOP_ON_CAUSE`) are read *over* the
settings, which is how the benchmark scripts sweep the numbers without rewriting the settings of the
machine they run on. Two more variables belong to the benchmark itself:

| variable | meaning | default |
|---|---|---|
| `BENCH_UNTIL_EMULATED` | stop `--bench` when the guest clock reaches this many seconds | off |
| `BENCH_HOTBLOCKS` | print the hottest compiled blocks with their disassembly | off |

The `--bench` report carries `idle skips` (how many waits were covered and how much emulated time
they were) and `idle wait` (how much of the run the detector recognised as waiting).
