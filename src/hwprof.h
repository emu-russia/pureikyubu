/*

# HW interface profiler (issue #394)

The debugger has always had a few performance counters (the status bar shows the emulated Gekko
MIPS, the VI rate and the PE rate), but they answer only one question - "how fast is the CPU". The
reverse engineering work keeps asking another one: *is the data moving at all, and where*. A title
that boots into a black screen may be spinning on an interrupt that never arrives, may be pushing
an empty display list into the CP FIFO, or may be playing samples out of ARAM that nothing writes.
The instruction counter does not say any of that.

This module is the single place where the emulated hardware reports the traffic on its
information-exchange channels - the 60x bus, the Flipper->Splash bus, the DMAs, the CP FIFO, the
audio mixer input, the interrupts, the GFX primitives and the frame rate - and where that traffic
is turned into a table of rates.

## The counters

Every channel owns a plain, monotone counter. The emulated hardware bumps it on the path where
the transfer actually happens (`Count`), which compiles to one `add` on a global: the counters are
always on, and only the code that moves the data pays for them. A counter is never decremented and
is never reset by the emulated machine, so the profiler may read it at any moment.

The instructions retired and the interrupts asserted are not counted here: those already have
counters of their own (`GekkoCore::GetInstructionCounter`, `DspCore::GetInstructionCounter` and
the PI interrupt counters), and `Sample` picks them up from there.

## The sampling

`Sample` reads every counter, subtracts the value of the previous sample and divides by the
emulated time that passed in between. The window is measured in *emulated* seconds, not in
wall-clock ones: a channel that moves 4 MB per console frame is supposed to report the same rate
whether the host runs the emulator at 0.2x or at 5x. The "real time" factor is reported
separately, so a rate that looks impossible can still be read as "the emulator is not keeping up".

## The reports

Two forms are produced from the same rate table:

- a Markdown table (the native language of the debugger), which the `hwprofile` command returns.
  `hwprofile image` additionally renders it into a PNG picture next to the debugger session (the
  DebugUI2 panel), and `hwprofile osd` returns the fixed-width text the GFX overlay draws;
- the fixed-width text itself, drawn over the emulated picture by the GFX overlay (`hwsod 1`).

*/

#pragma once

namespace Debug
{

// ------------------------------------------------------------------------------------
// The counters
// ------------------------------------------------------------------------------------

namespace HwProfile
{

	//! The channels (one counter each) the profiler watches. The name says which side of the
	//! transfer the bytes are counted on.
	enum class Counter
	{
		Bus60xRead = 0,		//!< Bytes the CPU read over the 60x bus (registers, main memory, the EFB)
		Bus60xWrite,		//!< ... and wrote
		SplashRead,			//!< Bytes the Flipper read out of main memory (1T-SRAM, "Splash")
		SplashWrite,		//!< ... and wrote

		PiInterrupts,		//!< PI interrupt assertions (all sources)
		WriteGather,		//!< Bytes the CPU pushed into the Write Gather Buffer
		CpFifo,				//!< Bytes pushed into the PI -> CP command FIFO
		AudioMixer,			//!< Bytes fed into the audio mixer

		DmaExi,				//!< Bytes moved by the EXI DMA
		DmaDi,				//!< ... the DI DMA
		DmaDsp,				//!< ... the DSP memory DMA
		DmaAi,				//!< ... the AI DMA
		DmaAram,			//!< ... the ARAM DMA

		GfxPrimitives,		//!< Primitives (triangles, lines, points) submitted to the GFX pipeline
		GfxVertices,		//!< Vertices they were built from

		ViFrames,			//!< Frames scanned out by the video interface
		GekkoInstructions,	//!< Gekko instructions retired
		DspInstructions,	//!< DSP instructions retired (a paired instruction counts once)

		Max,
	};

	//! The live counters, indexed by `Counter`. The emulated hardware increments them in place
	//! (an inline variable, so a module that only counts does not have to pull the profiler's
	//! translation unit into the link).
	inline uint64_t counters[(size_t)Counter::Max] = {};

	//! Account `value` units of the channel. It is what the emulated hardware calls on the path
	//! where the transfer happens.
	inline void Count(Counter counter, uint64_t value)
	{
		counters[(size_t)counter] += value;
	}

	//! Put every counter (and the sampling baseline) back to zero. Called when a new machine is
	//! built, because the counters of the previous run say nothing about this one.
	void Reset();

	//! The name of a counter as it appears in a report, and whether it is a byte count (kB = 1024)
	//! rather than an event count (k = 1000).
	const char* CounterName(Counter counter);
	bool CounterIsBytes(Counter counter);

}

// ------------------------------------------------------------------------------------
// The rate table
// ------------------------------------------------------------------------------------

namespace HwProfile
{

	//! The window in emulated seconds a sample covers. One second makes the rates read as "per
	//! second" and matches the status bar, which the profiler subsumes.
	constexpr double DefaultWindow = 1.0;

	//! The rates of one window. `RateTable` is pure arithmetic over the raw counter values, so
	//! that the sampling can be tested without a running emulator.
	class RateTable
	{
		uint64_t previous[(size_t)Counter::Max] = {};
		uint64_t total[(size_t)Counter::Max] = {};
		uint64_t delta[(size_t)Counter::Max] = {};
		double window = 0.0;
		bool sampled = false;

	public:
		//! Fold a new set of raw counter values into the table. `emulatedSeconds` is the emulated
		//! time that passed since the previous call (the first call only records the baseline).
		void Add(const uint64_t* values, double emulatedSeconds);

		//! True once at least one window has been measured (before that every rate is zero).
		bool Sampled() const { return sampled; }

		//! The length of the last window in emulated seconds.
		double Window() const { return window; }

		uint64_t Total(Counter counter) const { return total[(size_t)counter]; }
		uint64_t Delta(Counter counter) const { return delta[(size_t)counter]; }

		//! The rate of the channel over the last window, per emulated second.
		double PerSecond(Counter counter) const;

		void Clear();
	};

}

// ------------------------------------------------------------------------------------
// The sampler the emulator drives
// ------------------------------------------------------------------------------------

namespace HwProfile
{

	//! Take a sample unless the current one is younger than `DefaultWindow` emulated seconds.
	//! Safe to call as often as a frame is drawn: it does nothing until the window has passed.
	//!
	//! The two instruction counts are handed in rather than read here: the Gekko and the DSP cores
	//! own those counters, and the profiler is a pure function of the machine's state - which is
	//! also what lets the sampling be tested without an emulator.
	//!
	//! \param gekkoInstructions Instructions retired by the Gekko (GekkoCore::GetInstructionCounter)
	//! \param dspInstructions Instructions retired by the DSP, a paired instruction counted once
	//! \param ticks The current emulated time base
	//! \param ticksPerSecond The ticks in one emulated second (GekkoCore::OneSecond)
	//! Returns true when a new window was measured.
	bool Sample(uint64_t gekkoInstructions, uint64_t dspInstructions, uint64_t ticks, uint64_t ticksPerSecond);

	//! The rates of the last measured window.
	const RateTable& Rates();

	//! The emulated time the last window covered, in seconds (0 before the first one).
	double WindowSeconds();

	//! The host time the last window took, in seconds. The ratio to `WindowSeconds` is how fast
	//! the emulator is running against the console ("x real").
	double WindowWallSeconds();

	//! The Markdown report (`hwprofile`).
	void ReportToMarkdown(std::string& text);

	//! The fixed-width report (`hwprofile osd`), one line per counter, plus a header line.
	void ReportToLines(std::vector<std::string>& lines);

}

// ------------------------------------------------------------------------------------
// Presentation helpers
// ------------------------------------------------------------------------------------

namespace HwProfile
{

	//! Format a byte count the way a report shows it: `12.35 MB`, `900.0 KB`, `512 B`.
	void FormatBytes(uint64_t value, std::string& text);

	//! Format an event count: `1.20 K`, `3.40 M`.
	void FormatCount(uint64_t value, std::string& text);

	//! Format a rate of a counter: `12.35 MB/s` or `1.20 K/s`.
	void FormatRate(double value, bool bytes, std::string& text);

}

}
