/*

# Guest frame profiler

The two profilers the emulator already has answer two questions, and neither of them is the one a
reverse engineer asks when a title is slow:

- the **performance counters** (`Debug::PerfCounter`, the status bar, `--bench`) say how many
  *guest instructions per second* the emulator retires. That is one number for the whole console;
- the **HW interface profiler** (`src/hwprof.h`, issue #394) says *how much data* moves on each
  information-exchange channel of the console, in one emulated second. That is a rate, not a cost,
  and one emulated second is a very long window when the interesting thing happens inside a single
  frame.

What is missing is the middle ground: **where the emulator's own time goes while one frame of the
guest is being emulated**, and which of the guest's blocks owns it. That is what this module
measures. It is the tool for "this title runs at 0.4x real time - which part of the machine is
eating the host".

## The units

The profiler attributes *host* cycles to a **unit**, and the unit table is the whole guest: every
Gekko path, every Flipper block, the drive, the DSP, the interfaces, and the host back ends that
present the frame and play the sound. `UnitName`/`UnitGroup` give the report its labels and let it
group the bars (Gekko, Flipper, DVD, Audio, GFX, Front end).

A unit is entered through `GuestProf::Scope`, an RAII object placed at the *outermost* entry point
of the block - the one call that means "this block is doing work now":

| Unit | Entered from |
|---|---|
| `GekkoJit` | `Jit::Run` - the recompiler's whole turn (translation and fallbacks carve themselves out) |
| `GekkoCompile` | `Jit::CompileBlock` |
| `GekkoInterp` | `Jit::RunOneInterpreted`, `Jit::Fallback` |
| `GekkoMemory` | the JIT's memory helpers (`JitReadWord` and friends) |
| `GekkoCache` | `Cache::CastIn` / `CastOut` |
| `GekkoTranslate` | the MMU/TLB/BAT translation paths |
| `GekkoIdle` | the idle-skip fast forward |
| `ProcessorInterface` / `MemoryInterface` | `PIRead*`/`PIWrite*`, `MIReadBurst`/`MIWriteBurst` |
| `VideoInterface` | `VIUpdate` (scan-out and the YUV blit) |
| `CommandProcessor` | `GxCommand` - one command of the display list |
| `CpVertexFetch` | `DrawPrimitive` - the vertex fetch and the XF kick |
| `GeometryEngine` | `XF::CPDrawBegin`/`CPVertex`/`CPDrawEnd`, the soft transform |
| `SetupUnit` | `SU::loadSUReg`, `SoftSetupTriangle` |
| `Rasterizer` | `RAS_Begin`/`SendVertex`/`End`, the soft rasterizer |
| `TextureUnit` | `TX::UpdateAndBindTextures`, `LoadTlut`, the soft sampler |
| `TextureEnv` | `TEV::SoftShade`, the TEV uniform upload |
| `PixelEngine` | `PE::loadPEReg`, `SoftWritePixel`, `TextureCopy`, `SoftDisplayCopy` |
| `AudioInterface` | `AITickSync`/`AIFeedMixer` and the DVD-audio sample callback |
| `DspCore` | `DspCore::Update` (a block of DSP instructions) |
| `DspDma` / `AramController` | the DSP memory DMA and the ARAM DMA/accelerator |
| `DiskInterface` / `DvdDrive` / `DvdAudio` | the DI DMA, the DDU protocol/read, the DVD-audio decode |
| `SerialInterface` / `ExternalInterface` / `HiSpeedPort` | `SIPoll`, `exi_write_cr`, the HSP transfer |
| `GfxShader` / `GfxPresent` / `GfxSoftware` | the OpenGL pipeline, the swap, the software rasterizer |
| `AudioOutput` | the audio output back end (`PushBytes` into the ring) |
| `EmulatorCore` | the emulator's own loop between the profiled blocks (see below) |

`EmulatorCore` is not entered by a scope: it is what is left of the frame's wall clock after every
other unit has been charged. That is the JIT's block-cache lookup and dispatch, the run loop's
thread hand-off, the profiler's own `rdtsc`, and the front end's work. A frame in which this entry
is large is a frame the emulator spent *not* emulating - which is itself a finding.

## Exclusive attribution

The units nest: a `CpVertexFetch` runs inside a `CommandProcessor`, a `PixelEngine` write runs
inside a `Rasterizer` triangle. A scope therefore charges its unit only with the time *it* owned -
the elapsed time minus the time its children reported - and adds its own elapsed time to its
parent's child budget. The sums stay equal to the wall clock of the frame, and no cycle is counted
twice. The nesting depth is a handful, and the whole mechanism is two `rdtsc` per scope, so it is
armed only while a capture is running.

The scopes are deliberately kept off the *per-pixel* and *per-vertex* inner loops: at that
granularity the `rdtsc` would cost more than the work it measures. Those paths are covered by their
outer scope (per draw, per triangle) and by the channel counters, which are plain increments.

## The frame

The frame boundary is the video interface's scan-out wrap (`VideoInterface::VIUpdate`, the same
point the `ViFrames` counter uses): one boundary per emulated console frame, whatever the display
mode. At each boundary the profiler closes the frame record, snapshots the `HwProfile` counters and
zeroes the frame accumulators.

A `FrameRecord` holds:

- the emulated time base at the boundary and the host wall time the frame took;
- the host cycles and call count per unit;
- the delta of every `HwProfile` counter (`src/hwprof.h`) - that is the guest traffic the frame
  moved, on every channel the emulator counts;
- the instructions retired by the Gekko and by the DSP;
- the guest's hot basic blocks as `(pc, blocks, instructions)` - a 4 KB bucket histogram resolved
  to its top entries (see "Reading the histogram" below);
- the same for the DSP program counter;
- whether the frame touched the movie stream, and what the drive read during it.

## Reading the histogram

The histogram is fed from two places, and what it says depends on which engine ran the guest.

- With the **recompiler on**, `Jit::RunInner` reports a whole compiled block at once: the block's
  *entry* address and the number of instructions it retired. The volume is exact - it is the count
  the recompiler keeps - but the address is the entry, so a loop the translator kept inside one
  block is reported at the block's first instruction rather than where it spins. The interpreter
  charges the instructions the recompiler handed back (`Jit::Fallback`,
  `Jit::RunOneInterpreted`) at their own addresses.
- With the **recompiler off** (`jit 0`, or `CORE.JIT` in the settings), every instruction goes
  through `Interpreter::ExecuteDecoded`, which charges it to its own address. That is the exact
  picture, and it is the one to take when the question is *which instruction* is hot. It is also
  the only mode in which an emulated program counter read from another thread (the debugger's
  `StartProfiler`) means what it says, because inside a compiled block the emulated PC only moves at
  the block's boundaries.

There is a second difference between the two modes worth knowing: the **idle-wait skip** is driven
from the compiled path alone (`Jit::RunInner` -> `GekkoCore::PollCheck`). In interpreter mode the
guest's poll loops are executed for real, so a capture taken with the recompiler off shows the wait
the title actually performs, and the `Gekko idle skip` unit stays empty.

## Telling a movie frame from a rendered one

The profiler cannot know what the guest is doing, and deliberately does not guess: it is *told*
where a movie stream lives on the disc (`CaptureOptions::movieOffset`/`movieLength`, from the
command line) and it watches the one place the bytes actually come off the image
(`GCMRead`, `src/dvd.cpp`). A frame in which any read overlapped that range is a movie frame, and
once the movie has started every later frame stays a movie frame: the drive reads the stream in
bursts, so the odd frame in between may not read at all.

That is what separates the frames the title draws from textures (the logo screens before the
intro movie) from the frames of the movie itself, without a single heuristic about the guest's
code.

## The capture and its report

`StartCapture` arms the profiler and `StopCapture` writes a JSON document
(`format: "pureikyubu-guestprof"`) that holds the unit table and the frame records. Nothing in the
emulator reads it back: the report is built outside the emulator
(`testing/mpfmv/`, which draws the distribution charts), so a question that the first report did
not ask can be answered from the same capture without running the game again.

`CaptureComplete` is the stop condition of an unattended run: the capture is over once the movie
has been running for `movieFrames` frames, or once `maxFrames` have been recorded.

## What it does not do

- It does not attribute time inside the audio *output* device's own thread: the SDL callback runs
  on a thread of its own, and the profiler keeps one accumulator per unit per thread budget rather
  than merging them. `AudioOutput` measures the producing side (the emulation thread filling the
  ring), which is the side the guest's clock waits for.
- It does not symbolize: the report resolves the hot PCs against the `.map` files the emulator
  already ships (`build/Data/*.map`, see `src/sym.h` at report time, not here).
- It is not free. A capture costs two `rdtsc` per scope plus one add per profiled block, which is
  a few percent of the emulation; the numbers it reports are host cycles and stay comparable
  between frames, but a capture run is not a benchmark run.

*/

#pragma once

namespace Debug
{

namespace GuestProf
{

// ------------------------------------------------------------------------------------
// The units
// ------------------------------------------------------------------------------------

//! Every block of the guest, and the host back ends, that the profiler can charge time to.
//! The order is the order of the report; `UnitGroup` groups them for the charts.
enum class Unit
{
	// Gekko
	GekkoJit = 0,			//!< A recompiled basic block (the generated code itself)
	GekkoCompile,			//!< Translating a basic block
	GekkoInterp,			//!< The interpreter (a fallback out of a block, or the whole CPU)
	GekkoMemory,			//!< The JIT's memory helpers (an access the block could not inline)
	GekkoCache,				//!< Cache line fills and writebacks
	GekkoTranslate,			//!< Address translation (BAT, the segment table, the hash page table)
	GekkoIdle,				//!< The idle-skip fast forward

	// Flipper
	ProcessorInterface,		//!< The 60x bus and the register space
	MemoryInterface,		//!< 1T-SRAM ("Splash") burst reads and writes
	VideoInterface,			//!< The scan-out and the YUV blit
	CommandProcessor,		//!< One command of the display list
	CpVertexFetch,			//!< The vertex fetch and the XF kick of a draw
	GeometryEngine,			//!< XF: the transform, the lighting, the clip
	SetupUnit,				//!< SU: the register file and the triangle setup
	Rasterizer,				//!< RAS: the draw call, or the software rasterizer's triangles
	TextureUnit,			//!< TX: the texture decode/upload and the soft sampler
	TextureEnv,				//!< TEV: the colour/alpha combiners
	PixelEngine,			//!< PE: the Z test, the blend and the copy engine
	AudioInterface,			//!< AI and the DSP-side AI DMA
	DspCore,				//!< The DSP core (a block of DSP instructions)
	DspDma,					//!< The DSP memory DMA
	AramController,			//!< The ARAM DMA engine and the ARAM accelerator
	DiskInterface,			//!< The DI register block and its DMA
	DvdDrive,				//!< The DDU: the command protocol, the seek and the image read
	DvdAudio,				//!< The DVD-audio stream decode
	SerialInterface,		//!< SI: the controller poll and transfers
	ExternalInterface,		//!< EXI: the channels and the memory card
	HiSpeedPort,			//!< The ARAM expansion port (the Game Boy Player)

	// Host back ends
	GfxShader,				//!< The OpenGL pipeline: state, uniforms, draw calls, uploads
	GfxPresent,				//!< Handing the finished frame to the display
	GfxSoftware,			//!< The software rasterizer (the CPU pixel loop)
	AudioOutput,			//!< The audio output back end
	EmulatorCore,			//!< The emulator's own loop: dispatch, thread wake-ups, the profiler

	Max,
};

//! The name of a unit as a report shows it.
const char* UnitName(Unit unit);

//! The group a unit belongs to: "Gekko", "Flipper", "DVD", "Audio", "GFX" or "Front end".
const char* UnitGroup(Unit unit);

//! A one-line description of what a unit covers (the report's legend).
const char* UnitDescription(Unit unit);

// ------------------------------------------------------------------------------------
// The scopes
// ------------------------------------------------------------------------------------

//! Charge the host cycles of the enclosing scope to `unit`. Cheap and inert while no capture is
//! running; keep it at the outermost entry point of the block, never in a per-pixel loop.
class Scope
{
	Unit unit;
	uint64_t start = 0;
	uint64_t childCycles = 0;
	Scope* parent = nullptr;
	bool live = false;			//!< The scope really opened (the capture was armed)

public:
	explicit Scope(Unit unit);
	~Scope();

	Scope(const Scope&) = delete;
	Scope& operator=(const Scope&) = delete;
};

//! Charge `cycles` host cycles to `unit` without opening a scope. For a path that already knows
//! how much time it spent (the DSP JIT, an idle skip).
void Charge(Unit unit, uint64_t cycles, uint64_t calls = 1);

// ------------------------------------------------------------------------------------
// What the emulated machine reports
// ------------------------------------------------------------------------------------

//! The video interface scanned out a frame: close the current frame record and start the next.
//! `ticks` is the emulated time base at the boundary.
void FrameBoundary(uint64_t ticks);

//! A disc read of `length` bytes at `discOffset` came off the image (src/dvd.cpp, `GCMRead`).
//! This is what tells a movie frame from a rendered one.
void NoteDiscRead(uint64_t discOffset, uint64_t length);

//! A guest basic block at `pc` retired `instructions` instructions.
void NoteBlock(uint32_t pc, uint32_t instructions);

//! A DSP block at `pc` retired `instructions` instructions.
void NoteDspBlock(uint32_t pc, uint32_t instructions);

// ------------------------------------------------------------------------------------
// The capture
// ------------------------------------------------------------------------------------

struct CaptureOptions
{
	std::string outputFile;			//!< The JSON document to write when the capture ends
	std::string image;				//!< The image that was run (for the report's header)

	//! The disc range of the movie stream. Reads that overlap it mark their frame as a movie
	//! frame. A length of zero turns the marking off (a plain "profile one frame" run).
	uint64_t movieOffset = 0;
	uint64_t movieLength = 0;

	//! How many frames of the movie to record before the capture is complete.
	size_t movieFrames = 60;

	//! The hard cap on the whole capture, so that a title that never reaches the movie (or a run
	//! where the range is wrong) still ends with a report instead of growing without a bound.
	size_t maxFrames = 6000;

	//! The emulated ticks in one second. `StartCapture` takes it from the running machine
	//! (`GekkoCore::OneSecond`); a caller that has no machine - a unit test driving the capture
	//! with synthetic boundaries - sets it here, and then the capture needs nothing else.
	uint64_t ticksPerSecond = 0;
};

//! Arm the profiler. The machine must already be built: the unit table is static, but the frame
//! accumulators are cleared here and the host cycle counter is calibrated against the wall clock.
void StartCapture(const CaptureOptions& options);

//! Disarm the profiler and write `options.outputFile`. Safe to call when no capture is running.
void StopCapture();

//! True while a capture is armed (the scopes are live).
bool Capturing();

//! True once the capture has everything it was asked for (or hit `maxFrames`).
bool CaptureComplete();

//! The options of the running capture (for the debug interface's report).
const CaptureOptions& Options();

// ------------------------------------------------------------------------------------
// The debug interface
// ------------------------------------------------------------------------------------

//! A Markdown report of the capture so far: the unit table of the last frames, the movie
//! boundary and the traffic of the frame that is being measured.
void ReportToMarkdown(std::string& text);

//! Register the `guestprof` command of the debug interface:
//!
//! ```
//! guestprof                                   # the Markdown report of the capture so far
//! guestprof start <capture.json> [off:len] [n] # arm a capture (n movie frames, 60 by default)
//! guestprof stop                              # disarm it and write the document
//! ```
void Reflector();

}

}
