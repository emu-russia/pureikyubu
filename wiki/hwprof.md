# HW interface profiler (issue #394)

The debugger has always had a few performance counters. The status bar shows the emulated Gekko
MIPS, the VI rate and the PE rate, and the benchmark (`--bench`) prints a table of its own. They
answer one question - *how fast is the CPU* - and the reverse engineering work keeps asking
another one: **is the data moving at all, and where**.

A title that boots into a black screen may be spinning on an interrupt that never arrives, may be
pushing an empty display list into the CP FIFO, or may be streaming audio out of an ARAM buffer
that nothing ever fills. The instruction counter cannot tell those cases apart. The profiler
watches the console's information-exchange channels instead, so that "it does nothing" becomes
"the CP FIFO moved 0 bytes in the last second".

## The channels

| Channel | What is counted | Where it is counted |
|---|---|---|
| 60x bus read / write | The bytes of every CPU access that crosses the bus between Gekko and Flipper | `ProcessorInterface::PIRead*` / `PIWrite*` (1, 2, 4, 8 bytes per beat, 32 per burst) |
| Flipper/Splash read / write | The traffic on the Flipper side of main memory (1T-SRAM, "Splash") | `MemoryInterface::MIReadBurst` / `MIWriteBurst` (32 bytes each), and the single-beat PI accesses that land in RAM |
| PI interrupts | The interrupt assertions of the processor interface, every source together | `ProcessorInterface::PIAssertInt` (the per-source breakdown is the existing `PIGetInterruptCounter`) |
| Write gather buffer | The bytes the CPU pushes into the Write Gather pipe (WGP) before they burst out to the FIFO | `GatherBuffer::WriteBytes` |
| PI/CP FIFO | The bytes written into the command FIFO window, i.e. the display list traffic that reaches the CP | `ProcessorInterface::PIWriteBurst` (the `PI_REGSPACE_GFX_FIFO` branch) |
| Audio mixer input | The bytes the mixer is fed with, from both of its inputs | the AI DMA feed (`AIFeedMixer`) and the DVD audio stream (`AIStreamCallback`) |
| DMA EXI / DI / DSP / AI / ARAM | The bytes each DMA engine moved | `ExternalInterface::exi_write_cr`, the DI callbacks, `Dsp16::DoDma`, `AIFeedMixer`, `ARAMDmaRun` |
| GFX primitives / vertices | The primitives (triangles, lines, points) and the vertices the CP submitted | `CommandProcessor::DrawPrimitive` |
| VI frames | The frames the video interface scanned out | `VideoInterface::VIUpdate` |
| Gekko / DSP instructions | The instructions the two cores retired | `GekkoCore::GetInstructionCounter`, `DspCore::GetInstructionCounter` |

Two notes on the counters:

* The instruction counters are not incremented by the profiler. The cores already keep them, and a
  second increment on the Gekko's hot path would be paid by every emulated instruction. The
  profile is a pure function of a *reading*, which is also what makes it testable without an
  emulator (`testing/hwprof_test.cpp`).
* A DSP **paired** instruction counts as **one**. The upper and the lower half of a parallel word
  are two opcodes of the same cycle, so counting them separately made the core look twice as fast
  as the clock model - which advances the emulated time base by the words a block retired - says
  it is. The DSP thread's own clock arithmetic is untouched.

## The rates

`Debug::HwProfile::Sample` reads every counter, subtracts the previous reading and divides by the
emulated time that passed in between. The window is one *emulated* second, not a wall-clock one: a
channel that moves 4 MB per console frame reports the same rate whether the host runs the emulator
at 0.2x or at 5x. The ratio to the real time is reported with the table, so a rate that looks
impossible can still be read as "the emulator is not keeping up".

The counters are always on and are never reset by the emulated machine; `HwProfile::Reset` clears
them when a new machine is built (`EMUOpen`).

## The two presentations

The same rate table is published in two forms.

### The overlay in the emulated picture

```
hwsod 1
```

switches on an overlay that draws the report over the picture. The choice is stored in the
settings (`HW_OSD`, off by default).

The overlay does not read the hardware. It asks the **debug interface** for the report
(`hwprofile osd`) once a second, rasterizes the lines into a picture with the debugger's TrueType
font (`Data/DebugUiMono.ttf`, the same one DebugUI2 uses) and hands the picture to whichever back
end is presenting the frame:

```
Debug::HwProfile  ->  hwprofile  ->  Debug::HwOsd  ->  the picture  ->  the frame
```

* the OpenGL pipeline uploads the picture as a texture and draws it over the finished frame
  (`GFX::OsdDraw`, called from `GFXCore::GL_EndFrame`, after the frame dump so that the dumped
  frames stay free of debug text);
* the software pipeline hands its XFB to the video interface, so `VideoInterface::YUVBlit` blits
  the same picture into the RGB output buffer the video back end presents.

### The panel in the new debugger

```
hwprofile          # the Markdown table (the default)
hwprofile image    # ... and the same table as a PNG next to the session
hwprofile osd      # the fixed-width text the overlay draws
hwprofile reset    # start a new measuring window
```

DebugUI2 has a **Profiler** tab next to Registers, Disassembly and Memory; it runs
`hwprofile image` once a second, so the panel shows the picture an offline look at the session
folder would show (`hwprofile.png`).

## Where it stands next to the old counters

The `PerfCounter` enumeration and the status bar stay as they are: they are the cheap, always-on
subset of the same idea (the emulated MIPS, the VI and PE rates, the JIT statistics). The profiler
subsumes them - it reports the instructions per second and the VI frame rate from the same sources
- and adds the channels the status bar never had. The `--bench` report keeps its own richer
breakdown of the host cycles, which is a different measurement (what the *host* spends, not what
the *console* moves).
