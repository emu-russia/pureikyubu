# Guest frame profiler

`src/guestprof.h` / `src/guestprof.cpp`.

The emulator already had two profilers, and neither of them answers the question a slow title
raises:

| Tool | Question it answers | Window |
|---|---|---|
| `Debug::PerfCounter`, the status bar, `--bench` | how many guest instructions per second | a whole run |
| HW interface profiler (`hwprof`, issue #394) | how much data moves on each channel of the console | one emulated **second** |
| **guest frame profiler** (this page) | **where the host's time went while one frame of the guest was emulated, and which block of the machine owned it** | one console **frame** |

One emulated second is 30 frames. That is a very long window when the thing worth looking at
happens inside a single frame.

## The shape of a measurement

Every block of the guest reports the time it spent through an RAII scope:

```cpp
void TEV::UploadUniforms(GLProgram& program)
{
    Debug::GuestProf::Scope scope(Debug::GuestProf::Unit::GfxShader);
    ...
}
```

The scopes **nest**, and the accounting is *exclusive*: a scope charges its unit with the time it
owned - its own elapsed cycles minus what its children reported - and hands its own elapsed cycles
to its parent's budget. A `TEV` upload inside a `Rasterizer` draw inside a `CP` command each get
their own share, and the sums stay equal to the frame's wall clock. No cycle is counted twice.

The scopes sit only at the outermost entry point of a block. The per-vertex, per-pixel and per-byte
loops deliberately have none - at that granularity the two `rdtsc` of a scope would cost more than
the work it measures. Those paths are covered by their outer scope (per draw, per triangle) and by
the **channel counters**, which are plain increments and are always affordable.

## The units

`Unit` enumerates the whole guest and the host back ends. `UnitName`/`UnitGroup` give the report its
labels; the groups are `Gekko`, `Flipper`, `Audio`, `DVD`, `GFX` and `Host`.

| Group | Units |
|---|---|
| Gekko | `GekkoJit`, `GekkoCompile`, `GekkoInterp`, `GekkoMemory`, `GekkoCache`, `GekkoTranslate`, `GekkoIdle` |
| Flipper | `ProcessorInterface`, `MemoryInterface`, `VideoInterface`, `CommandProcessor`, `CpVertexFetch`, `GeometryEngine` (XF), `SetupUnit`, `Rasterizer` (RAS), `TextureUnit` (TX), `TextureEnv` (TEV), `PixelEngine` (PE), `SerialInterface`, `ExternalInterface`, `HiSpeedPort` |
| Audio | `AudioInterface` (AI + the DSP AI DMA), `DspCore`, `DspDma`, `AramController`, `AudioOutput` |
| DVD | `DiskInterface` (DI), `DvdDrive` (DDU), `DvdAudio` |
| GFX | `GfxShader` (the OpenGL backend), `GfxSoftware` (the software rasterizer), `GfxPresent` |
| Host | `EmulatorCore` - the frame's wall clock that no scope claimed |

`EmulatorCore` is not entered by a scope: it is the remainder. The JIT's block-cache lookup and
dispatch, the run loop's thread hand-off, the profiler's own overhead and the front end all land
there. A frame with a large `EmulatorCore` is a frame the emulator spent *not* emulating, which is
a finding of its own.

## The channels

The frame record carries the delta of every `HwProfile::Counter` (`src/hwprof.h`), so a frame
reports what the guest *moved* as well as what it *cost*. The counter table covers the whole
machine: the 60x bus, the Splash bus, the cache, the write-gather buffer, MMIO, the interrupts, the
VI, the CP and the BP chain, the GFX pipeline (primitives, vertices, draw calls, presents, texture
decodes and uploads, TEV fragments, PE pixels, copy-engine pixels), the audio (mixer bytes, sample
pairs, underruns), the DMA engines, the drive (commands, bytes, DVD-audio samples), SI/EXI/HSP, the
DSP, and the Gekko itself (instructions, blocks, translations, invalidations).

## The frame boundary

The boundary is the video interface's scan-out wrap in `VideoInterface::VIUpdate` - the same point
the `ViFrames` counter uses, one boundary per emulated console frame. At each boundary the profiler
closes the record, snapshots the channel counters and clears the frame accumulators.

## Which instruction is hot: run the recompiler off

The basic-block histogram is fed from two places, and what it says depends on the engine that ran
the guest.

- With the **recompiler on**, `Jit::RunInner` reports a whole compiled block at once - the block's
  *entry* address and the number of instructions it retired. The volume is exact (it is the count
  the recompiler keeps), but the address is the entry, so a loop the translator kept inside one
  block is reported at the block's first instruction rather than where it spins.
- With the **recompiler off** (`jit 0`, or `CORE.JIT` in the settings), every instruction reaches
  `Interpreter::ExecuteDecoded`, which charges it to its own address. That is the exact picture.

Two things follow, and both matter:

1. **A profile taken with the recompiler off is the one that names an instruction.** It is also the
   only mode in which a PC read from another thread means what it says: inside a compiled block the
   emulated `regs.pc` only moves at the block's boundaries, so the debugger's `StartProfiler`
   samples block entries exactly the way `Jit::RunInner` reports them.
2. **The idle-wait skip is driven from the compiled path alone** (`Jit::RunInner` ->
   `GekkoCore::PollCheck`). In interpreter mode the guest's poll loops are executed for real, so a
   capture taken that way shows the wait the title actually performs, and the `Gekko idle skip` unit
   stays empty. The guest also reaches the same state at a different console frame, because the skip
   changes the guest-visible timing.

The volume measured in the two modes agrees, which is what makes the region trustworthy: in the
Metroid Prime FMV capture below, the hottest 4 KB bucket holds 82.3% of the movie frames'
instructions with the recompiler on and 81.8% with it off.

## Telling a movie frame from a rendered one

The profiler does not guess what the guest is doing. It is *told* where a movie stream lives on the
disc and it watches the one place the bytes come off the image (`GCMRead`, `src/dvd.cpp`). A frame
in which a read overlapped that range is a movie frame; once the stream has started, every later
frame stays a movie frame (the drive reads it in bursts, so the odd frame in between may not read
at all).

That is what separates the frames of an FMV from the frames a title draws from textures - the logo
screens before the intro movie - with no heuristic about the guest's code. The disc range of a THP
stream comes from the emulator's own disc reader: `DumpFst` lists the file system and
`testing/mpfmv/tools/isofst.py` resolves and decodes it offline.

## The capture

```
pureikyubu_headless --guestprof "image.iso" capture.json --movie 0x2DD25600:7459244 --movieframes 60
```

The capture is armed after the image is loaded and stops by itself once the movie has run for the
requested number of frames (or once `--guestframes` frames have been recorded). The document
(`format: "pureikyubu-guestprof"`) holds the unit table, the channel table, one record per frame
and two aggregate histograms of the guest's basic blocks - one for the frames that did not read the
movie stream and one for the frames that did.

Nothing in the emulator reads the document back. The report is built outside
(`testing/mpfmv/mkreport.py`), so a question the first report did not ask can be answered from the
same capture without running the game again.

## The debug interface

```
guestprof                                     # the Markdown report of the capture so far
guestprof start fmv.json 0x2DD25600:7459244 60
guestprof stop
```

## What it costs

A capture is not free: two `rdtsc` per scope and one add per profiled block. The host cycle numbers
stay comparable between the frames of one capture, which is what the report draws; a capture run is
not a benchmark run.

## What it is next to `--bench`

`--bench` answers "how fast does this build run the title" and reports the host cycles of the
*whole run* through the eight coarse `Gekko::CpuStats` scopes. The guest frame profiler answers
"which block of the guest is eating the host", broken down by frame and by block, and it covers the
Flipper, the drive, the DSP and the back ends that `--bench` never looked at.
