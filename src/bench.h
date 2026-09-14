/*

# The `--bench` run

`--bench <file> [seconds]` loads the image, runs it unattended for the requested number of seconds
(30 by default) and prints the measured throughput together with the CPU-side performance counters.
This is the measurement tool for the performance work on the emulator: the counters say how the
emulated instructions split between translated blocks and the interpreter, how long the average
basic block is, how often a block has to be recompiled and how many memory accesses leave the
emulated cache for the PI/MEM. The whole point is to compare configurations (the recompiler against
the interpreter, one memory path against another) on the same workload.

The measurement itself is front-end independent, so it lives here and is used by both the windowed
SDL UI (`uisdl.cpp`) and the headless one (`uinull.cpp`). The only thing the front ends own is what
happens once per loop tick: the windowed one pumps its events, the headless one does nothing.

An interactive run shows the same throughput in the status bar one line per second (the metrics
thread of the front end); the benchmark always writes that line to the report log, so an unattended
run leaves a per-second timeline behind.

*/

#pragma once

namespace Bench
{
	/// <summary>
	/// Called from the measurement loop, so that a front end with a window can keep it responsive.
	/// Returns false to end the run early (the SDL UI answers false for `SDL_QUIT`).
	/// </summary>
	typedef bool (*PumpProc)();

	/// <summary>
	/// Measure the already running emulation and print the report. `file` is only used to label the
	/// report. A `seconds` of zero measures until the pump asks to stop (the plain headless run).
	/// </summary>
	void Measure(const std::wstring& file, uint32_t seconds, PumpProc pump);
}
