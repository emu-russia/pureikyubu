/*

The guest frame profiler: the unit table, the exclusive host cycle attribution, the frame
records and the capture document. See guestprof.h for what the module is for and how the pieces
fit together.

*/

#include "pch.h"

#include <chrono>
#include <thread>

namespace Debug
{

namespace GuestProf
{

// ------------------------------------------------------------------------------------
// The unit table
// ------------------------------------------------------------------------------------

	struct UnitInfo
	{
		Unit unit;
		const char* name;
		const char* group;
		const char* description;
	};

	// The order is the order of the report. It is also what `UnitName` walks, and what the JSON
	// document's unit table is built from.
	static const UnitInfo unitInfo[] =
	{
		{ Unit::GekkoJit,			"Gekko JIT",			"Gekko",	"The recompiler's turn: dispatch and the generated block code" },
		{ Unit::GekkoCompile,		"Gekko translate",		"Gekko",	"Translating a basic block the JIT has not seen" },
		{ Unit::GekkoInterp,		"Gekko interpreter",	"Gekko",	"An instruction run by the interpreter (a fallback out of a block)" },
		{ Unit::GekkoMemory,		"Gekko memory helper",	"Gekko",	"A load/store the block could not inline" },
		{ Unit::GekkoCache,			"Gekko cache",			"Gekko",	"Cache line fills and writebacks" },
		{ Unit::GekkoTranslate,		"Gekko MMU",			"Gekko",	"Address translation (BAT, segment table, hash page table)" },
		{ Unit::GekkoIdle,			"Gekko idle skip",		"Gekko",	"The idle-skip fast forward" },

		{ Unit::ProcessorInterface,	"PI (60x bus)",			"Flipper",	"The 60x bus and the register space" },
		{ Unit::MemoryInterface,	"MI (Splash)",			"Flipper",	"1T-SRAM burst reads and writes" },
		{ Unit::VideoInterface,		"VI (scan-out)",		"Flipper",	"The scan-out and the YUV blit" },
		{ Unit::CommandProcessor,	"CP (command)",			"Flipper",	"One command of the display list" },
		{ Unit::CpVertexFetch,		"CP (vertex fetch)",	"Flipper",	"The vertex fetch and the XF kick of a draw" },
		{ Unit::GeometryEngine,		"XF (transform)",		"Flipper",	"XF: the transform, the lighting and the clip" },
		{ Unit::SetupUnit,			"SU (setup)",			"Flipper",	"SU: the register file and the triangle setup" },
		{ Unit::Rasterizer,			"RAS (raster)",			"Flipper",	"RAS: the draw call, or the software rasterizer's triangles" },
		{ Unit::TextureUnit,		"TX (texture)",			"Flipper",	"TX: the texture decode/upload and the soft sampler" },
		{ Unit::TextureEnv,			"TEV (combine)",		"Flipper",	"TEV: the colour and alpha combiners" },
		{ Unit::PixelEngine,		"PE (pixel)",			"Flipper",	"PE: the Z test, the blend and the copy engine" },
		{ Unit::AudioInterface,		"AI (audio in)",		"Audio",	"AI and the DSP-side AI DMA" },
		{ Unit::DspCore,			"DSP core",				"Audio",	"The DSP core (a block of DSP instructions)" },
		{ Unit::DspDma,				"DSP DMA",				"Audio",	"The DSP memory DMA" },
		{ Unit::AramController,		"ARAM",					"Audio",	"The ARAM DMA engine and the ARAM accelerator" },
		{ Unit::DiskInterface,		"DI (drive if)",		"DVD",		"The DI register block and its DMA" },
		{ Unit::DvdDrive,			"DDU (drive)",			"DVD",		"The DDU: the command protocol, the seek and the image read" },
		{ Unit::DvdAudio,			"DVD audio",			"DVD",		"The DVD-audio stream decode" },
		{ Unit::SerialInterface,	"SI (controllers)",		"Flipper",	"SI: the controller poll and the transfers" },
		{ Unit::ExternalInterface,	"EXI (channels)",		"Flipper",	"EXI: the channels and the memory card" },
		{ Unit::HiSpeedPort,		"HSP (expansion)",		"Flipper",	"The ARAM expansion port (the Game Boy Player)" },

		{ Unit::GfxShader,			"GFX shader",			"GFX",		"The OpenGL pipeline: state, uniforms, draws, uploads" },
		{ Unit::GfxPresent,			"GFX present",			"GFX",		"Handing the finished frame to the display" },
		{ Unit::GfxSoftware,		"GFX software",			"GFX",		"The software rasterizer's pixel loop" },
		{ Unit::AudioOutput,		"Audio output",			"Audio",	"The audio output back end" },
		{ Unit::EmulatorCore,		"Emulator loop",		"Host",		"The emulator's own loop: dispatch, thread hand-off, the profiler" },
	};

	static const size_t unitInfoCount = sizeof(unitInfo) / sizeof(unitInfo[0]);

	static const UnitInfo* FindUnit(Unit unit)
	{
		for (size_t i = 0; i < unitInfoCount; i++)
		{
			if (unitInfo[i].unit == unit)
				return &unitInfo[i];
		}

		return nullptr;
	}

	const char* UnitName(Unit unit)
	{
		const UnitInfo* info = FindUnit(unit);
		return info ? info->name : "?";
	}

	const char* UnitGroup(Unit unit)
	{
		const UnitInfo* info = FindUnit(unit);
		return info ? info->group : "?";
	}

	const char* UnitDescription(Unit unit)
	{
		const UnitInfo* info = FindUnit(unit);
		return info ? info->description : "";
	}

// ------------------------------------------------------------------------------------
// The capture state
// ------------------------------------------------------------------------------------

	// The resolution of the guest basic-block histogram: 4 KB buckets over the whole 32-bit
	// effective address space. Fine enough that a bucket start resolves to the function that is
	// running (a GameCube function is a few hundred bytes, and the shipped maps are dense), and
	// coarse enough that a frame touches a couple of thousand of them.
	//
	// The histogram is sparse in practice, so the frame accumulator also keeps the list of the
	// buckets it touched: the frame boundary walks that list instead of the whole array, and
	// clears the same entries. A frame that touched nothing costs nothing.
	static const size_t pcBucketShift = 12;
	static const size_t pcBucketCount = (size_t)1 << (32 - pcBucketShift);
	static const size_t maxTouchedBuckets = 1 << 16;

	// The DSP program counter is 12 bits, so its histogram is exact rather than bucketed.
	static const size_t dspPcCount = 1 << 12;

	//! The hot basic blocks a frame record keeps. The frame's whole histogram is aggregated into
	//! the phase tables; the record keeps the head of it, which is what a per-frame chart draws.
	static const size_t topBlocks = 16;
	static const size_t topDspBlocks = 8;

	struct PcEntry
	{
		uint32_t pc;
		uint32_t instructions;
	};

	struct FrameRecord
	{
		uint64_t ticks = 0;				//!< The emulated time base at the boundary
		double emulated = 0.0;			//!< Emulated seconds since the capture started
		double wall = 0.0;				//!< Host seconds this frame took

		uint64_t hostCycles[(size_t)Unit::Max] = {};
		uint32_t calls[(size_t)Unit::Max] = {};

		uint64_t counters[(size_t)HwProfile::Counter::Max] = {};
		uint64_t gekkoInstructions = 0;
		uint64_t dspInstructions = 0;

		bool movie = false;				//!< The drive read the movie stream during this frame
		uint64_t discBytes = 0;			//!< Bytes read off the image
		uint32_t discReads = 0;
		uint64_t discFirstOffset = 0;
		uint64_t discLastOffset = 0;
		uint64_t movieBytes = 0;		//!< ... of those, the bytes that were the movie stream

		uint32_t pcBlocksTotal = 0;
		size_t pcTopCount = 0;
		PcEntry pcTop[topBlocks] = {};

		uint32_t dspPcTotal = 0;
		size_t dspTopCount = 0;
		PcEntry dspTop[topDspBlocks] = {};
	};

	// The accumulators of the frame that is being measured. Every scope writes here; the frame
	// boundary reads them, builds a record and clears them.
	static uint64_t frameCycles[(size_t)Unit::Max] = {};
	static uint64_t frameCalls[(size_t)Unit::Max] = {};
	static uint32_t framePc[pcBucketCount] = {};
	static uint32_t touchedPc[maxTouchedBuckets] = {};
	static size_t touchedPcCount = 0;
	static uint32_t frameDspPc[dspPcCount] = {};

	// The per-phase aggregate histograms: phase 0 is every frame that did not read the movie,
	// phase 1 is every frame that did. They are what the report's "where does the guest live"
	// chart is drawn from, and they are exact (the frame records only keep the top entries).
	static uint32_t phasePc[2][pcBucketCount] = {};
	static uint32_t phaseDspPc[2][dspPcCount] = {};
	static uint64_t phaseFrames[2] = {};

	static CaptureOptions captureOptions;
	static std::vector<FrameRecord> frames;
	static std::atomic<bool> armed{ false };

	static bool haveBaseline = false;
	static uint64_t baselineTicks = 0;
	static uint64_t baselineWall = 0;
	static uint64_t lastCounters[(size_t)HwProfile::Counter::Max] = {};
	static uint64_t lastGekkoInstructions = 0;
	static uint64_t lastDspInstructions = 0;

	// The frame that is being filled right now.
	static bool movieStarted = false;
	static bool movieStartRecorded = false;
	static size_t movieFrameCount = 0;
	static size_t movieStartIndex = 0;
	static uint64_t frameDiscBytes = 0;
	static uint32_t frameDiscReads = 0;
	static uint64_t frameDiscFirst = 0;
	static uint64_t frameDiscLast = 0;
	static uint64_t frameMovieBytes = 0;
	static std::atomic<bool> complete{ false };

	static double tscHz = 1.0;
	static uint64_t ticksPerSecond = 0;
	static uint64_t captureStartTicks = 0;

	static uint64_t ReadCycleCounter()
	{
#if defined(_MSC_VER)
		return __rdtsc();
#else
		return __builtin_ia32_rdtsc();
#endif
	}

	static uint64_t NowMs()
	{
		static const auto origin = std::chrono::steady_clock::now();
		auto now = std::chrono::steady_clock::now();
		return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - origin).count();
	}

	static double MeasureTscFrequency()
	{
		auto wall0 = std::chrono::steady_clock::now();
		uint64_t tsc0 = ReadCycleCounter();
		Thread::Sleep(100);
		auto wall1 = std::chrono::steady_clock::now();
		uint64_t tsc1 = ReadCycleCounter();

		double seconds = std::chrono::duration<double>(wall1 - wall0).count();
		if (seconds <= 0.0)
			return 1.0;

		return (double)(tsc1 - tsc0) / seconds;
	}

// ------------------------------------------------------------------------------------
// The scopes
// ------------------------------------------------------------------------------------

	// The innermost open scope of this thread. A scope charges its unit with the time it owned -
	// its own elapsed cycles minus what its children already reported - and hands its own elapsed
	// cycles to its parent's child budget. The sums therefore stay equal to the wall clock, and
	// nothing is counted twice.
	static thread_local Scope* innermost = nullptr;

	Scope::Scope(Unit u) : unit(u)
	{
		if (!armed.load(std::memory_order_relaxed))
			return;

		start = ReadCycleCounter();
		parent = innermost;
		childCycles = 0;
		innermost = this;
		live = true;
	}

	Scope::~Scope()
	{
		if (!live)
			return;

		uint64_t elapsed = ReadCycleCounter() - start;
		uint64_t self = (elapsed > childCycles) ? (elapsed - childCycles) : 0;

		frameCycles[(size_t)unit] += self;
		frameCalls[(size_t)unit]++;

		if (parent != nullptr)
		{
			parent->childCycles += elapsed;
		}

		innermost = parent;
	}

	void Charge(Unit unit, uint64_t cycles, uint64_t calls)
	{
		if (!armed.load(std::memory_order_relaxed))
			return;

		frameCycles[(size_t)unit] += cycles;
		frameCalls[(size_t)unit] += (uint32_t)calls;
	}

// ------------------------------------------------------------------------------------
// What the emulated machine reports
// ------------------------------------------------------------------------------------

	void NoteBlock(uint32_t pc, uint32_t instructions)
	{
		if (!armed.load(std::memory_order_relaxed))
			return;

		size_t bucket = (size_t)(pc >> pcBucketShift);

		// The first instruction in a bucket is what puts it on the touched list; the boundary
		// walks that list rather than the whole (4 MB) array.
		if (framePc[bucket] == 0 && touchedPcCount < maxTouchedBuckets)
		{
			touchedPc[touchedPcCount++] = (uint32_t)bucket;
		}

		framePc[bucket] += instructions ? instructions : 1;
	}

	void NoteDspBlock(uint32_t pc, uint32_t instructions)
	{
		if (!armed.load(std::memory_order_relaxed))
			return;

		frameDspPc[pc & (dspPcCount - 1)] += instructions ? instructions : 1;
	}

	void NoteDiscRead(uint64_t discOffset, uint64_t length)
	{
		if (!armed.load(std::memory_order_relaxed) || length == 0)
			return;

		frameDiscBytes += length;
		frameDiscReads++;

		if (frameDiscReads == 1 || discOffset < frameDiscFirst)
			frameDiscFirst = discOffset;
		if (discOffset + length > frameDiscLast)
			frameDiscLast = discOffset + length;

		if (captureOptions.movieLength == 0)
			return;

		uint64_t movieEnd = captureOptions.movieOffset + captureOptions.movieLength;
		uint64_t readEnd = discOffset + length;

		if (discOffset < movieEnd && readEnd > captureOptions.movieOffset)
		{
			// The overlap, not the whole read: the first burst of the stream can carry the tail of
			// whatever the title read before the movie.
			uint64_t from = (discOffset > captureOptions.movieOffset) ? discOffset : captureOptions.movieOffset;
			uint64_t to = (readEnd < movieEnd) ? readEnd : movieEnd;

			frameMovieBytes += to - from;
			movieStarted = true;
		}
	}

// ------------------------------------------------------------------------------------
// The frame boundary
// ------------------------------------------------------------------------------------

	//! Extract the `count` heaviest buckets of a sparse histogram and return how many entries were
	//! found. `touched` is the list of the buckets that hold something, which is what keeps this a
	//! walk over a couple of thousand entries rather than over the whole 4 MB array. The histogram
	//! itself is left alone: the caller clears the touched entries.
	static void TopTouched(const uint32_t* histogram, const uint32_t* touched, size_t touchedCount,
		size_t shift, PcEntry* out, size_t count, size_t& found, uint32_t& total)
	{
		found = 0;
		total = 0;

		for (size_t t = 0; t < touchedCount; t++)
		{
			size_t i = touched[t];
			uint32_t value = histogram[i];
			if (value == 0)
				continue;

			total += value;

			// The list is short (16 entries), so a linear insertion is cheaper than a sort.
			size_t at = found;
			while (at > 0 && out[at - 1].instructions < value)
				at--;

			if (at >= count)
				continue;

			size_t last = (found < count) ? found : count - 1;
			for (size_t j = last; j > at; j--)
				out[j] = out[j - 1];

			out[at].pc = (uint32_t)((uint64_t)i << shift);
			out[at].instructions = value;

			if (found < count)
				found++;
		}
	}

	//! Throw away the frame's accumulators. The basic-block histogram is cleared through its
	//! touched list, which is why the list exists.
	static void ClearFrameAccumulators()
	{
		for (size_t t = 0; t < touchedPcCount; t++)
		{
			framePc[touchedPc[t]] = 0;
		}
		touchedPcCount = 0;

		memset(frameCycles, 0, sizeof(frameCycles));
		memset(frameCalls, 0, sizeof(frameCalls));
		memset(frameDspPc, 0, sizeof(frameDspPc));

		frameDiscBytes = 0;
		frameDiscReads = 0;
		frameDiscFirst = 0;
		frameDiscLast = 0;
		frameMovieBytes = 0;
	}

	void FrameBoundary(uint64_t ticks)
	{
		if (!armed.load(std::memory_order_relaxed))
			return;

		uint64_t wall = NowMs();

		if (ticksPerSecond == 0)
		{
			// Nothing to measure against: the capture was armed without a machine and without a
			// time base (see StartCapture).
			return;
		}

		if (!haveBaseline)
		{
			// The first boundary only records where the counters start: a delta needs two readings.
			haveBaseline = true;
			baselineTicks = ticks;
			baselineWall = wall;

			for (size_t i = 0; i < (size_t)HwProfile::Counter::Max; i++)
				lastCounters[i] = HwProfile::counters[i];

			lastGekkoInstructions = (Core != nullptr) ? (uint64_t)Core->GetInstructionCounter() : 0;
			lastDspInstructions = Gekko::stats.dspInstrs;

			ClearFrameAccumulators();
			return;
		}

		if (ticks <= baselineTicks)
		{
			// The machine was reset under us; start the window over rather than report a negative
			// one.
			baselineTicks = ticks;
			baselineWall = wall;
			ClearFrameAccumulators();
			return;
		}

		FrameRecord record;
		record.ticks = ticks;
		record.emulated = (double)(ticks - captureStartTicks) / (double)ticksPerSecond;
		record.wall = (double)(wall - baselineWall) / 1000.0;

		memcpy(record.hostCycles, frameCycles, sizeof(frameCycles));
		for (size_t i = 0; i < (size_t)Unit::Max; i++)
			record.calls[i] = (uint32_t)frameCalls[i];

		// Where the wall clock went that no scope claimed: the emulator's own loop between the
		// profiled blocks, the thread hand-off and the front end. The record is built without this
		// entry, so adding it here cannot feed back on itself.
		uint64_t claimed = 0;
		for (size_t i = 0; i < (size_t)Unit::Max; i++)
		{
			if (i == (size_t)Unit::EmulatorCore)
				continue;
			claimed += record.hostCycles[i];
		}

		uint64_t wallCycles = (uint64_t)(record.wall * tscHz);
		record.hostCycles[(size_t)Unit::EmulatorCore] = (wallCycles > claimed) ? (wallCycles - claimed) : 0;

		for (size_t i = 0; i < (size_t)HwProfile::Counter::Max; i++)
		{
			uint64_t value = HwProfile::counters[i];
			record.counters[i] = (value >= lastCounters[i]) ? (value - lastCounters[i]) : value;
			lastCounters[i] = value;
		}

		uint64_t gekko = (Core != nullptr) ? (uint64_t)Core->GetInstructionCounter() : 0;
		uint64_t dsp = Gekko::stats.dspInstrs;
		record.gekkoInstructions = (gekko >= lastGekkoInstructions) ? (gekko - lastGekkoInstructions) : gekko;
		record.dspInstructions = (dsp >= lastDspInstructions) ? (dsp - lastDspInstructions) : dsp;
		lastGekkoInstructions = gekko;
		lastDspInstructions = dsp;

		record.movie = movieStarted;
		record.discBytes = frameDiscBytes;
		record.discReads = frameDiscReads;
		record.discFirstOffset = frameDiscFirst;
		record.discLastOffset = frameDiscLast;
		record.movieBytes = frameMovieBytes;

		// The index of the first record that read the stream: the record being built right now.
		if (movieStarted && !movieStartRecorded)
		{
			movieStartIndex = frames.size();
			movieStartRecorded = true;
		}

		TopTouched(framePc, touchedPc, touchedPcCount, pcBucketShift, record.pcTop, topBlocks,
			record.pcTopCount, record.pcBlocksTotal);

		// The DSP histogram is small and dense, so it is walked whole (the touched list above is
		// for the guest's 4 MB one only).
		record.dspPcTotal = 0;
		record.dspTopCount = 0;
		for (size_t i = 0; i < dspPcCount; i++)
		{
			uint32_t value = frameDspPc[i];
			if (value == 0)
				continue;

			record.dspPcTotal += value;

			size_t at = record.dspTopCount;
			while (at > 0 && record.dspTop[at - 1].instructions < value)
				at--;

			if (at >= topDspBlocks)
				continue;

			size_t last = (record.dspTopCount < topDspBlocks) ? record.dspTopCount : topDspBlocks - 1;
			for (size_t j = last; j > at; j--)
				record.dspTop[j] = record.dspTop[j - 1];

			record.dspTop[at].pc = (uint32_t)i;
			record.dspTop[at].instructions = value;

			if (record.dspTopCount < topDspBlocks)
				record.dspTopCount++;
		}

		size_t phase = record.movie ? 1 : 0;
		phaseFrames[phase]++;

		// Fold the frame's histogram into the phase's and clear the touched entries in the same
		// walk.
		for (size_t t = 0; t < touchedPcCount; t++)
		{
			uint32_t bucket = touchedPc[t];
			uint32_t value = framePc[bucket];
			if (value != 0)
				phasePc[phase][bucket] += value;
		}

		for (size_t i = 0; i < dspPcCount; i++)
		{
			if (frameDspPc[i] != 0)
				phaseDspPc[phase][i] += frameDspPc[i];
		}

		if (record.movie)
			movieFrameCount++;

		frames.push_back(record);

		ClearFrameAccumulators();

		baselineTicks = ticks;
		baselineWall = wall;

		if (frames.size() >= captureOptions.maxFrames ||
			movieFrameCount >= captureOptions.movieFrames)
		{
			complete.store(true, std::memory_order_relaxed);
		}
	}

// ------------------------------------------------------------------------------------
// The capture
// ------------------------------------------------------------------------------------

	void StartCapture(const CaptureOptions& options)
	{
		captureOptions = options;

		frames.clear();
		frames.reserve((options.maxFrames < 8192) ? options.maxFrames : 8192);

		memset(frameCycles, 0, sizeof(frameCycles));
		memset(frameCalls, 0, sizeof(frameCalls));
		memset(framePc, 0, sizeof(framePc));
		touchedPcCount = 0;
		memset(frameDspPc, 0, sizeof(frameDspPc));
		memset(phasePc, 0, sizeof(phasePc));
		memset(phaseDspPc, 0, sizeof(phaseDspPc));
		memset(phaseFrames, 0, sizeof(phaseFrames));
		memset(lastCounters, 0, sizeof(lastCounters));

		haveBaseline = false;
		baselineTicks = 0;
		baselineWall = 0;
		lastGekkoInstructions = 0;
		lastDspInstructions = 0;

		movieStarted = false;
		movieStartRecorded = false;
		movieFrameCount = 0;
		movieStartIndex = 0;
		ClearFrameAccumulators();

		complete.store(false, std::memory_order_relaxed);
		captureStartTicks = 0;

		// The time base comes from the machine the capture was armed on; a caller without one (a
		// unit test) hands it in, and a capture that has neither records nothing.
		if (options.ticksPerSecond != 0)
			ticksPerSecond = options.ticksPerSecond;
		else if (Core != nullptr)
			ticksPerSecond = Core->OneSecond();
		else
			ticksPerSecond = 0;

		tscHz = MeasureTscFrequency();
		innermost = nullptr;

		Report(Channel::Info, "GuestProf: capture armed (TSC %.2f GHz, movie range 0x%llX + %llu bytes)\n",
			tscHz / 1e9,
			(unsigned long long)options.movieOffset,
			(unsigned long long)options.movieLength);

		armed.store(true, std::memory_order_relaxed);
	}

// ------------------------------------------------------------------------------------
// The document
// ------------------------------------------------------------------------------------

	static void JsonString(FILE* file, const std::string& text)
	{
		fputc('"', file);

		for (char c : text)
		{
			switch (c)
			{
				case '"': fputs("\\\"", file); break;
				case '\\': fputs("\\\\", file); break;
				case '\n': fputs("\\n", file); break;
				case '\r': fputs("\\r", file); break;
				case '\t': fputs("\\t", file); break;
				default:
					if ((unsigned char)c < 0x20)
						fprintf(file, "\\u%04X", (unsigned char)c);
					else
						fputc(c, file);
					break;
			}
		}

		fputc('"', file);
	}

	static void WriteDocument(FILE* file)
	{
		fprintf(file, "{\n");
		fprintf(file, "  \"format\": \"pureikyubu-guestprof\",\n");
		fprintf(file, "  \"version\": 1,\n");
		fprintf(file, "  \"image\": ");
		JsonString(file, captureOptions.image);
		fprintf(file, ",\n");

		fprintf(file, "  \"tscHz\": %.0f,\n", tscHz);
		fprintf(file, "  \"ticksPerSecond\": %llu,\n", (unsigned long long)ticksPerSecond);
		fprintf(file, "  \"hostCyclesPerEmulatedSecond\": %.0f,\n",
			(ticksPerSecond != 0) ? (tscHz / (double)ticksPerSecond) : 0.0);

		fprintf(file, "  \"movie\": { \"offset\": %llu, \"length\": %llu, \"startFrame\": %llu, \"frames\": %llu },\n",
			(unsigned long long)captureOptions.movieOffset,
			(unsigned long long)captureOptions.movieLength,
			(unsigned long long)movieStartIndex,
			(unsigned long long)movieFrameCount);

		// The unit table.
		fprintf(file, "  \"units\": [\n");
		for (size_t i = 0; i < unitInfoCount; i++)
		{
			fprintf(file, "    { \"id\": %d, \"name\": ", (int)unitInfo[i].unit);
			JsonString(file, unitInfo[i].name);
			fprintf(file, ", \"group\": ");
			JsonString(file, unitInfo[i].group);
			fprintf(file, ", \"about\": ");
			JsonString(file, unitInfo[i].description);
			fprintf(file, " }%s\n", (i + 1 < unitInfoCount) ? "," : "");
		}
		fprintf(file, "  ],\n");

		// The HwProfile channel table, so the report can label the traffic it shows.
		fprintf(file, "  \"channels\": [\n");
		for (size_t i = 0; i < (size_t)HwProfile::Counter::Max; i++)
		{
			fprintf(file, "    { \"id\": %d, \"name\": ", (int)i);
			JsonString(file, HwProfile::CounterName((HwProfile::Counter)i));
			fprintf(file, ", \"bytes\": %s }%s\n",
				HwProfile::CounterIsBytes((HwProfile::Counter)i) ? "true" : "false",
				(i + 1 < (size_t)HwProfile::Counter::Max) ? "," : "");
		}
		fprintf(file, "  ],\n");

		// The frames.
		fprintf(file, "  \"frames\": [\n");
		for (size_t f = 0; f < frames.size(); f++)
		{
			const FrameRecord& r = frames[f];

			fprintf(file, "    { \"i\": %llu, \"t\": %llu, \"emulated\": %.9f, \"wall\": %.6f, \"movie\": %s,\n",
				(unsigned long long)f, (unsigned long long)r.ticks, r.emulated, r.wall,
				r.movie ? "true" : "false");

			fprintf(file, "      \"gekko\": %llu, \"dsp\": %llu,\n",
				(unsigned long long)r.gekkoInstructions, (unsigned long long)r.dspInstructions);

			fprintf(file, "      \"disc\": { \"bytes\": %llu, \"reads\": %u, \"first\": %llu, \"last\": %llu, \"movieBytes\": %llu },\n",
				(unsigned long long)r.discBytes, r.discReads,
				(unsigned long long)r.discFirstOffset, (unsigned long long)r.discLastOffset,
				(unsigned long long)r.movieBytes);

			fprintf(file, "      \"host\": [");
			for (size_t i = 0; i < (size_t)Unit::Max; i++)
				fprintf(file, "%s%llu", (i == 0) ? "" : ", ", (unsigned long long)r.hostCycles[i]);
			fprintf(file, "],\n");

			fprintf(file, "      \"calls\": [");
			for (size_t i = 0; i < (size_t)Unit::Max; i++)
				fprintf(file, "%s%u", (i == 0) ? "" : ", ", r.calls[i]);
			fprintf(file, "],\n");

			fprintf(file, "      \"counters\": [");
			for (size_t i = 0; i < (size_t)HwProfile::Counter::Max; i++)
				fprintf(file, "%s%llu", (i == 0) ? "" : ", ", (unsigned long long)r.counters[i]);
			fprintf(file, "],\n");

			fprintf(file, "      \"pcTotal\": %u,\n", r.pcBlocksTotal);
			fprintf(file, "      \"pcTop\": [");
			for (size_t i = 0; i < r.pcTopCount; i++)
				fprintf(file, "%s[%u, %u]", (i == 0) ? "" : ", ", r.pcTop[i].pc, r.pcTop[i].instructions);
			fprintf(file, "],\n");

			fprintf(file, "      \"dspPcTotal\": %u,\n", r.dspPcTotal);
			fprintf(file, "      \"dspPcTop\": [");
			for (size_t i = 0; i < r.dspTopCount; i++)
				fprintf(file, "%s[%u, %u]", (i == 0) ? "" : ", ", r.dspTop[i].pc, r.dspTop[i].instructions);
			fprintf(file, "] }%s\n", (f + 1 < frames.size()) ? "," : "");
		}
		fprintf(file, "  ],\n");

		// The per-phase aggregate histograms, as sparse lists of nonzero buckets.
		for (int phase = 0; phase < 2; phase++)
		{
			fprintf(file, "  \"phase%d\": { \"frames\": %llu, \"pc\": [",
				phase, (unsigned long long)phaseFrames[phase]);

			bool first = true;
			for (size_t i = 0; i < pcBucketCount; i++)
			{
				if (phasePc[phase][i] == 0)
					continue;
				fprintf(file, "%s[%llu, %u]", first ? "" : ", ",
					(unsigned long long)((uint64_t)i << pcBucketShift), phasePc[phase][i]);
				first = false;
			}

			fprintf(file, "], \"dspPc\": [");
			first = true;
			for (size_t i = 0; i < dspPcCount; i++)
			{
				if (phaseDspPc[phase][i] == 0)
					continue;
				fprintf(file, "%s[%llu, %u]", first ? "" : ", ", (unsigned long long)i, phaseDspPc[phase][i]);
				first = false;
			}

			fprintf(file, "] }%s\n", (phase == 0) ? "," : "");
		}

		fprintf(file, "}\n");
	}

	void StopCapture()
	{
		if (!armed.load(std::memory_order_relaxed))
			return;

		armed.store(false, std::memory_order_relaxed);
		innermost = nullptr;

		Report(Channel::Info, "GuestProf: capture stopped after %llu frames (%llu of them movie frames)\n",
			(unsigned long long)frames.size(), (unsigned long long)movieFrameCount);

		if (captureOptions.outputFile.empty())
			return;

		FILE* file = fopen(captureOptions.outputFile.c_str(), "wb");
		if (file == nullptr)
		{
			Report(Channel::Error, "GuestProf: cannot write %s\n", captureOptions.outputFile.c_str());
			return;
		}

		WriteDocument(file);
		fclose(file);

		Report(Channel::Info, "GuestProf: wrote %s (%llu frames)\n",
			captureOptions.outputFile.c_str(), (unsigned long long)frames.size());
	}

	bool Capturing()
	{
		return armed.load(std::memory_order_relaxed);
	}

	bool CaptureComplete()
	{
		return complete.load(std::memory_order_relaxed);
	}

	const CaptureOptions& Options()
	{
		return captureOptions;
	}

// ------------------------------------------------------------------------------------
// The debug interface's report
// ------------------------------------------------------------------------------------

	void ReportToMarkdown(std::string& text)
	{
		char line[0x200];

		text.clear();
		text += "# Guest frame profile\n\n";

		if (!armed.load(std::memory_order_relaxed) && frames.empty())
		{
			text += "No capture has been run. Start one with the `guestprof start <file> [movieOffset:length]` command.\n";
			return;
		}

		sprintf(line, "%llu frames recorded, %llu of them movie frames",
			(unsigned long long)frames.size(), (unsigned long long)movieFrameCount);
		text += line;
		text += (Capturing() ? " (capture running)\n\n" : " (capture finished)\n\n");

		if (!frames.empty())
		{
			const FrameRecord& last = frames.back();

			text += "## The last frame\n\n";
			text += "| Unit | Group | Host cycles | Share | Calls |\n";
			text += "|---|---|---:|---:|---:|\n";

			uint64_t total = 0;
			for (size_t i = 0; i < (size_t)Unit::Max; i++)
				total += last.hostCycles[i];

			for (size_t i = 0; i < unitInfoCount; i++)
			{
				size_t id = (size_t)unitInfo[i].unit;
				uint64_t cycles = last.hostCycles[id];

				sprintf(line, "| %s | %s | %llu | %.1f%% | %u |\n",
					unitInfo[i].name, unitInfo[i].group,
					(unsigned long long)cycles,
					total ? (double)cycles * 100.0 / (double)total : 0.0,
					last.calls[id]);
				text += line;
			}

			sprintf(line, "\nThe frame covered %.6f s of emulated time in %.4f s of host time (%.2fx real), "
				"%llu Gekko instructions and %llu DSP instructions.\n",
				(last.ticks > 0) ? (double)last.ticks / (double)(ticksPerSecond ? ticksPerSecond : 1) : 0.0,
				last.wall,
				(last.wall > 0.0) ? ((double)last.ticks / (double)(ticksPerSecond ? ticksPerSecond : 1)) / last.wall : 0.0,
				(unsigned long long)last.gekkoInstructions,
				(unsigned long long)last.dspInstructions);
			text += line;

			text += "\n## The heaviest guest blocks of the last frame\n\n";
			text += "| Address | Instructions |\n|---|---:|\n";
			for (size_t i = 0; i < last.pcTopCount; i++)
			{
				sprintf(line, "| 0x%08X | %u |\n", last.pcTop[i].pc, last.pcTop[i].instructions);
				text += line;
			}

			if (last.dspTopCount != 0)
			{
				text += "\n## The heaviest DSP words of the last frame\n\n";
				text += "| Address | Instructions |\n|---|---:|\n";
				for (size_t i = 0; i < last.dspTopCount; i++)
				{
					sprintf(line, "| 0x%04X | %u |\n", last.dspTop[i].pc, last.dspTop[i].instructions);
					text += line;
				}
			}

			text += "\n## The drive, in the last frame\n\n";
			sprintf(line, "%llu bytes in %u reads (0x%llX .. 0x%llX), %llu bytes of them the movie stream.\n",
				(unsigned long long)last.discBytes, last.discReads,
				(unsigned long long)last.discFirstOffset, (unsigned long long)last.discLastOffset,
				(unsigned long long)last.movieBytes);
			text += line;
		}

		text += "\n## The traffic channels of the last frame\n\n";
		text += "| Channel | Amount |\n|---|---:|\n";
		if (!frames.empty())
		{
			const FrameRecord& last = frames.back();
			for (size_t i = 0; i < (size_t)HwProfile::Counter::Max; i++)
			{
				std::string value;
				if (HwProfile::CounterIsBytes((HwProfile::Counter)i))
					HwProfile::FormatBytes(last.counters[i], value);
				else
					HwProfile::FormatCount(last.counters[i], value);

				sprintf(line, "| %s | %s |\n", HwProfile::CounterName((HwProfile::Counter)i), value.c_str());
				text += line;
			}
		}
	}

// ------------------------------------------------------------------------------------
// The debug interface
// ------------------------------------------------------------------------------------

	// guestprof [start <file> [off:len] [n] | stop] - the guest frame profiler.
	static Json::Value* CmdGuestProf(std::vector<std::string>& args)
	{
		std::string mode = (args.size() > 1) ? args[1] : "text";

		if (mode == "start")
		{
			if (args.size() < 3)
			{
				Report(Channel::Error, "guestprof start needs the capture file name\n");
				return nullptr;
			}

			CaptureOptions options;
			options.outputFile = args[2];
			options.image = Util::WstringToString(emu.lastLoaded);
			options.maxFrames = cmdline.guestFrames;

			if (args.size() > 3)
			{
				size_t colon = args[3].find(':');
				if (colon != std::string::npos)
				{
					options.movieOffset = strtoull(args[3].substr(0, colon).c_str(), nullptr, 0);
					options.movieLength = strtoull(args[3].substr(colon + 1).c_str(), nullptr, 0);
				}
			}

			options.movieFrames = (args.size() > 4) ? (size_t)strtoul(args[4].c_str(), nullptr, 0) : cmdline.movieFrames;

			StartCapture(options);

			Json::Value* output = new Json::Value();
			output->type = Json::ValueType::Bool;
			output->value.AsBool = true;
			return output;
		}

		if (mode == "stop")
		{
			StopCapture();
			return nullptr;
		}

		std::string markdown;
		ReportToMarkdown(markdown);

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Object;
		output->AddUtf8String("markdown", markdown.c_str());
		return output;
	}

	void Reflector()
	{
		JDI::Hub.AddCmd("guestprof", CmdGuestProf);
	}

}

}
