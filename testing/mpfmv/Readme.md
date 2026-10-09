# testing/mpfmv — profiling one frame of the Metroid Prime intro FMV

An investigation of the emulator's THP playback, built on the **guest frame profiler**
(`src/guestprof.h`) and the **HW interface profiler** (`src/hwprof.h`, issue #394).

The deliverable is [`report.html`](report.html): where the host time and the guest's work go while
the title plays `/Video/00_first_start.thp`, frame by frame.

## What is here

| Path | Contents |
|---|---|
| `report.html` | The generated report. One self-contained page (inline SVG, no network). |
| `capture.json` | The capture with the recompiler on: one record per emulated console frame. |
| `pcsample.json` | The same window sampled by the emulator's PC profiler, recompiler on. |
| `capture_interp.json` | The same run with the recompiler off (`jit 0`). |
| `pcsample_interp.json` | The PC profile of that run - the one the hot addresses come from. |
| `hotloops.txt` | The hot loop, disassembled by the emulator's own `GekkoDisasm`. |
| `capture.ps1` | The capture driver: resolves the disc range of a THP stream out of the image and runs the emulator. |
| `tools/isofst.py` | A minimal GameCube disc reader: the boot block, the file system table and the THP stream header. |
| `tools/discmap.py` | Resolves the per-frame disc reads of a capture against the file system table. |
| `tools/pcsample.py` | Keeps the samples of a PC profile that fall inside the movie window of a capture. |
| `tools/jdi.py` | A minimal MCP client for the emulator's debug interface (`--mcp`); `disasm` drives `GekkoDisasm`. |
| `tools/mkreport.py` | The analysis: turns a capture into the report's blocks. |
| `tools/report.js`, `tools/report.css` | The report's chart renderer and styling. |
| `tools/checkreport.js` | A self-check of a generated report: it runs the renderer against a minimal DOM and asserts that every block produced what it should (it caught the `code` blocks rendering as `[object ...]`). |

## Reproducing it

```
# build the headless emulator (VS2026)
MSBuild scripts\VS2026\pureikyubu_headless.vcxproj -p:Configuration=Release -p:Platform=x64

# capture (about two minutes: the title boots, loads its assets and then plays the stream)
.\testing\mpfmv\capture.ps1

# the same window at instruction resolution, and the disassembly of what it finds
cd testing\mpfmv\tools

#   ... with the recompiler off: this is the one that names an instruction
#   (put `jit 0` in build\autoexec.cmd, or set CORE.JIT to false in build\Data\Settings.json)
python jdi.py "<image>" profiler pcsample_interp.json 150
python pcsample.py ..\capture_interp.json ..\pcsample_interp.json
python jdi.py "<image>" disasm 0x8036C4D0 190 > ..\hotloops.txt

#   ... and with it on, which is what shows the block-entry bias
python jdi.py "<image>" profiler pcsample.json 240
python pcsample.py ..\capture.json ..\pcsample.json

# the report
python mkreport.py ..\capture.json -o ..\report.html
node checkreport.js ..\report.html
```

`capture.ps1` starts the emulator with `build/` as its working directory — that is where `Data/`
(and therefore the settings and the ROM images) lives; from anywhere else the emulator reports
"Default settings missing" and the machine comes up on garbage.

The equivalent command by hand:

```
cd build
pureikyubu_headless --guestprof "D:\Isos\Metroid Prime (v1.02)(USA).iso" capture.json ^
                    --movie 0x2DD25600:7459244 --movieframes 150 --guestframes 1000
```

## The stream

`/Video/00_first_start.thp` — the intro movie the title plays once, after the logo screens it
draws from textures. Read out of the image's own file system table by `tools/isofst.py`:

| | |
|---|---|
| disc offset | `0x2DD25600` |
| length | 7 459 244 bytes |
| version | `0x00010000` |
| frames | 301 |
| frame rate | 29.97 |
| duration | 10.04 s |
| video | 640x480 |
| audio | none (the stream carries no audio component) |

Every `/Video/*.thp` of the title is listed by `python tools/isofst.py "<image>" --find thp`.

## How "a frame of the FMV" is identified

The profiler is not told what the guest is doing. It is told the disc range of the stream and it
watches the one place the bytes come off the image (`GCMRead`, `src/dvd.cpp`). A frame in which a
read overlapped the range is a movie frame, and once the stream has started every later frame stays
a movie frame — the drive reads it in bursts, so the odd frame in between may not read at all.

That is what separates the frames of the FMV from the frames of the logo screens the title draws
from textures, without a single heuristic about the guest's code. In this capture the stream starts
at frame 522 and the report contrasts the frames before it with the frames of it.

The frames do not go straight from the logos to the movie: the title opens and inspects the headers
of eight different `/Video/*.thp` files between frames 522 and 573, and the last frame that
submitted a single primitive is 464. The picture stops 58 frames before the stream starts.

## What the numbers say

**The stream does not play — it trickles, and nothing is drawn.**

| | |
|---|---|
| Console frames captured | 672 (frames 0-521 before the stream, 522-671 of it) |
| Frames that read the stream's own disc range | 13 |
| Bytes of the stream consumed | 21 620 — about **4 of the 301 frames** the header declares |
| CPU commands submitted by the movie frames | 643 per 100 frames (the rendered frames before: 5 786) |
| Primitives submitted by the movie frames | 6.7 per 100 frames (before: 447.9) |
| Frames presented by the movie frames | 1.3 per 100 frames (before: 56.1) |
| Host time per movie frame | 20.0 ms (the frames before: 39.3 ms) — the emulator is *not* the bottleneck |
| Guest instructions per movie frame | 678 K, of which **82% sit in one 4 KB region at `0x8036C000`** |
| The DSP | 227 K instructions and 29% of the frame — audio keeps running while the video side waits |

## Which instruction is hot, and why the mode matters

The addresses had to be taken **with the recompiler off**. With it on, the emulated program counter
only moves at the boundaries of a compiled block, so the PC sampler reports the block's *entry* for
as long as the block runs — and so does the compiled path's own histogram. Comparing each sampler
against the deterministic instruction counts of its own capture shows the size of the effect:

| Execution mode | Hottest 4 KB bucket, counted | ... sampled | Distance |
|---|---|---|---|
| recompiler on | 82.25% | 60.31% | **0.274** |
| recompiler off | 81.83% | 84.44% | **0.031** |

With the recompiler on the sampled distribution is a quarter of the whole away from the counted
one, and it moves ~25 points of the time into the neighbouring 4 KB bucket. The region is the same
in both modes — which is what makes it trustworthy — but the address inside it is not.

With the recompiler off the hot code resolves to **six consecutive instructions at
`0x8036C7C8`–`0x8036C7DC`, 32.7 % of the movie window's samples**: a linear search over a
signed-halfword array, called from the cluster at `0x8036C4DC`–`0x8036C7E4` that holds about two
thirds of the frame together.

```
0x8036C7C8  lha    r0, 0(r4)        ; the next signed halfword of the array
0x8036C7CC  addi   r4, r4, 2
0x8036C7D0  cmpw   r3, r0           ; against the needle in r3
0x8036C7D4  blt+   0x8036C7E0       ; past it, stop
0x8036C7D8  addi   r6, r6, 1        ; else count it
0x8036C7DC  bdnz+  0x8036C7C8       ; and go round
```

The address the *compiled* run pointed at (`0x8036BEC0`, 27.9 % of its samples) is worth **0.92 %**
in the exact run: it is a block entry, not a hot instruction. This title's code around
`0x8036C000` sits in a gap in the shipped symbol map — between the SDK's memory-card block (last
named symbol `0x8035C834`) and its audio block (first named symbol `AIInitDMA` at `0x8036DBB0`) —
so the loop can be described but not named.

One more difference between the modes: the **idle-wait skip is driven from the compiled path
alone**, so the interpreter executes the guest's poll loops for real and the guest reaches the same
state at a different console frame (333 against 522).

In other words: the guest is not slow because the emulator is slow (a movie frame costs *less* host
time than a rendered one), and it is not slow because the THP decoder is expensive (the decoder,
`__THPDecompressiMCURow640x480` and friends, is about 9% of the frame's instructions). It is slow
because the movie player spends ~90% of its instructions in a small amount of unnamed code without
ever handing a display list to the CP.

## Where this sits next to the FMV work already in `master`

The title's movies have been through the emulator once already. The branches merged before this
capture was taken are:

| Branch (merged into `master`) | What it changed |
|---|---|
| `386-fix-metroid-prime-fmv-slowdown` | the recompiler falling back to the interpreter on the halfword loads this movie player is made of (`lha` / `lhz`), the block cache being dropped for address-translation events, and a CP FIFO repoint that lost entries the emulated reader had not fetched |
| `349-metroid-prime-black-screen-instead-fmv` | the frames staying on a back buffer nothing presented (the display-copy path) |
| `416-regress-on-fmvs` | a regression in the same area |
| `347-icaruga-garbage-on-title-fmv` | the same movie path for Ikaruga |

All four are ancestors of the `master` this capture was taken on: `lha` *is* translated
(`src/gekkojit_x64.cpp`, the halfword-load cases), and a movie frame is **cheaper** for the emulator
than a rendered one. What the capture shows is the other half of the story — the stream is read in a
trickle and the frames never reach the pixel engine — which is why the numbers are worth having
even though the slowdown itself is fixed.

## Where the emulator's own coverage was extended

This research also extended the HW interface profiler's channel table so that a capture describes
the whole guest rather than the CPU alone: the cache and its writebacks, MMIO, the CP command
decode and the BP register chain, the graphics back end (draw calls, presents, texture decodes and
uploads, TEV fragments, PE pixels, copy-engine pixels), the audio back end (sample pairs,
underruns), the drive (DDU commands, bytes, DVD-audio samples), SI/EXI/HSP, the ARAM ADPCM decoder,
and the Gekko's own blocks, translations and invalidations. See `src/hwprof.h`,
`testing/hwprof_test.cpp` and `wiki/guestprof.md`.
