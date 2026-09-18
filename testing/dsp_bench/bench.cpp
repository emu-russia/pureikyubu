/*

Standalone DSPcore benchmark. See Readme.md.

Usage:
	DSP_JIT=1  bench irom   [words]     real IROM boot path (mailbox handshake + command loop)
	           bench raw    <file> [words]
	           bench nop    [words]
	DSP_VERBOSE=1 prints the core's Debug::Report output.

The harness links the real DSP core (dsp.cpp, dspcore.cpp, dspdec.cpp, dspdma.cpp,
dsparam.cpp and dspjit_x64.cpp / dspjit_x86.cpp) against a stub pch.h and a flat console
main memory, so the
two execution engines can be measured and compared without SDL, GL or the rest of
Flipper.

It prints the number of DSP instruction words retired, the wall-clock MIPS and a
fingerprint of the whole observable core state (the register file, the four stacks and
data memory). `check.sh` uses the fingerprint to run the interpreter for exactly as many
instructions as a recompiler run retired and compare the two.

*/

#include "pch.h"
#include "dsp_golden_alu_vectors.h"

#include <chrono>


static const size_t IROM_BYTES = 8 * 1024;
static const size_t IRAM_BYTES = 8 * 1024;
static const size_t DRAM_BYTES = 8 * 1024;
static const size_t DROM_BYTES = 4 * 1024;

// The DSP core only reads the time base when it runs as a thread; the benchmark drives it by
// hand, so the tick source only has to exist.
static Gekko::GekkoCore tickSource;

// ---------------------------------------------------------------------------
// State fingerprint

static uint64_t Mix(uint64_t h, uint64_t v)
{
	for (int i = 0; i < 8; i++)
	{
		h ^= (uint8_t)v;
		h *= 1099511628211ULL;
		v >>= 8;
	}
	return h;
}

static uint64_t HashState(DSP::DspCore* core)
{
	uint64_t h = 1469598103934665603ULL;

	h = Mix(h, core->regs.a.bits);
	h = Mix(h, core->regs.b.bits);
	h = Mix(h, core->regs.prod.bitsPacked);
	h = Mix(h, core->regs.x.bits);
	h = Mix(h, core->regs.y.bits);
	h = Mix(h, core->regs.psr.bits);
	h = Mix(h, core->regs.pc);
	h = Mix(h, core->regs.dpp);
	h = Mix(h, (uint64_t)core->GetInstructionCounter());

	for (int i = 0; i < 4; i++)
	{
		h = Mix(h, core->regs.r[i]);
		h = Mix(h, core->regs.m[i]);
		h = Mix(h, core->regs.l[i]);
	}

	DSP::DspStack* stacks[4] = { core->regs.pcs, core->regs.pss, core->regs.eas, core->regs.lcs };
	for (DSP::DspStack* s : stacks)
	{
		h = Mix(h, (uint64_t)s->size());
		for (int i = 0; i < s->size(); i++)
		{
			h = Mix(h, s->at(i));
		}
	}

	// Data memory (DRAM + DROM), so that a memory-op difference is caught as well.
	uint8_t* dram = core->TranslateDMem(0);
	if (dram != nullptr)
	{
		for (size_t i = 0; i < DRAM_BYTES; i++) h = Mix(h, dram[i]);
	}
	uint8_t* drom = core->TranslateDMem(0x1000);
	if (drom != nullptr)
	{
		for (size_t i = 0; i < DROM_BYTES; i++) h = Mix(h, drom[i]);
	}

	return h;
}

static uint64_t HashMemory(DSP::DspCore* core, bool drom)
{
	uint64_t h = 1469598103934665603ULL;
	uint8_t* p = core->TranslateDMem(drom ? 0x1000 : 0);
	size_t n = drom ? DROM_BYTES : DRAM_BYTES;
	if (p) for (size_t i = 0; i < n; i++) h = Mix(h, p[i]);
	return h;
}

// ---------------------------------------------------------------------------

static bool LoadFile(const char* path, uint8_t* dst, size_t maxSize, size_t* loaded)
{
	FILE* f = fopen(path, "rb");
	if (f == nullptr) return false;

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (size < 0 || (size_t)size > maxSize)
	{
		fclose(f);
		return false;
	}

	bool ok = (size == 0) || (fread(dst, 1, (size_t)size, f) == (size_t)size);
	fclose(f);

	if (loaded) *loaded = (size > 0) ? (size_t)size : 0;
	return ok;
}

int main(int argc, char** argv)
{
	const char* mode = (argc > 1) ? argv[1] : "irom";
	bool useJit = getenv("DSP_JIT") != nullptr && atoi(getenv("DSP_JIT")) != 0;
	g_verbose = getenv("DSP_VERBOSE") != nullptr;

	// The DSP device (and with it the core) needs a Flipper to hang off. The stub HW is
	// enough: the core only reaches the PI register window and the flat main memory.
	DSP::Dsp16 dsp;
	Flipper::DSP = &dsp;
	DSP::DspCore* core = dsp.core;

	// Pin the tick source to a non-null stub so that nothing dereferences a null Core.
	Core = &tickSource;

	// The recompiler is off by default; this benchmark is the tool that measures it, so ask for
	// it explicitly (`DSP_JIT=1`), exactly as `--dspjit` does in the emulator.
	core->JitEnabled = useJit;

	// The ARAM accelerator is part of the DSP device and the sweep reaches its data port
	// through `ldsa`; without the ARAM image those reads dereference a null buffer.
	DSP::AROpen(Flipper::HW);


	uint64_t wanted = 1000000;
	DSP::DspAddress entry = 0x8000;

	if (strcmp(mode, "irom") == 0)
	{
		const char* path = getenv("DSP_IROM");
		if (path == nullptr) path = "build/Data/dsp_irom.bin";

		std::vector<uint8_t> image;
		FILE* f = fopen(path, "rb");
		if (f == nullptr)
		{
			fprintf(stderr, "cannot open the IROM image: %s\n", path);
			return 2;
		}
		fseek(f, 0, SEEK_END);
		long size = ftell(f);
		fseek(f, 0, SEEK_SET);
		image.resize((size > 0) ? (size_t)size : 0);
		if (!image.empty() && fread(image.data(), 1, image.size(), f) != image.size())
		{
			fclose(f);
			fprintf(stderr, "short read on the IROM image\n");
			return 2;
		}
		fclose(f);

		if (!core->LoadIrom(image))
		{
			fprintf(stderr, "the IROM image must be 8 KB\n");
			return 2;
		}
		entry = 0x8000;
		if (argc > 2) wanted = strtoull(argv[2], nullptr, 0);
	}
	else if (strcmp(mode, "raw") == 0)
	{
		if (argc < 3)
		{
			fprintf(stderr, "usage: bench raw <file> [words]\n");
			return 2;
		}

		size_t loaded = 0;
		if (!LoadFile(argv[2], core->TranslateIMem(0), IRAM_BYTES, &loaded))
		{
			fprintf(stderr, "cannot load %s into IRAM\n", argv[2]);
			return 2;
		}
		entry = 0;
		if (argc > 3) wanted = strtoull(argv[3], nullptr, 0);
	}
	else if (strcmp(mode, "golden") == 0)
	{
		// A synthetic data-path workload: a deterministic stream of instructions sampled from
		// the hardware golden vector table (see testing/dsp_golden_alu_vectors.h), so every
		// word is one the core really executes.
		uint32_t seed = (argc > 2) ? (uint32_t)strtoul(argv[2], nullptr, 0) : 0x12345678;
		if (argc > 3) wanted = strtoull(argv[3], nullptr, 0);

		// The table is the data-path corpus, but a few entries are flow control; a stream that
		// contains one leaves IRAM at some pc the jump picked (the core halts on the fetch and
		// the run is silently cut short). Decode every candidate and keep only the words that
		// advance the pc by their own size, so the program is a straight line into the `jmp 0`
		// (0x029F/0x0000 - the encoding the IROM uses in its own main loop) that closes it.
		size_t limit = IRAM_BYTES / 2 - 2;
		std::vector<uint16_t> words;
		size_t lastStart = 0;
		bool lastTwo = false;
		while (words.size() < limit)
		{
			seed = seed * 1103515245u + 12345u;
			const DspGolden::Vector& v = DspGolden::kAluVectors[(seed >> 8) % DspGolden::kAluVectorCount];

			uint8_t encoded[4] = { (uint8_t)(v.word >> 8), (uint8_t)v.word, 0, 0 };
			DSP::DecoderInfo info;
			DSP::Decoder::Decode(encoded, 2, info);
			if (info.flowControl) continue;

			lastStart = words.size();
			lastTwo = v.twoWord != 0;
			words.push_back(v.word);
			if (v.twoWord) words.push_back(v.word2);
		}
		// A two-word form must not be cut in half by the limit: the `jmp 0` that closes the
		// program has to start on an instruction boundary, otherwise its operand is executed as
		// an instruction and the pc walks off the end of IRAM.
		if (lastTwo) words.resize(lastStart);
		if (words.size() > limit) words.resize(limit);
		words.push_back(0x029F);
		words.push_back(0x0000);

		uint16_t* iram = (uint16_t*)core->TranslateIMem(0);
		for (size_t i = 0; i < words.size(); i++)
		{
			iram[i] = _BYTESWAP_UINT16(words[i]);
		}

		entry = 0;
	}
	else if (strcmp(mode, "sweep") == 0)
	{
		// Run every instruction word of [from, to] through the interpreter (and optionally the
		// recompiler) from a fixed state. Debug::Halt returns instead of stopping, because an
		// undefined word is part of the space. Used with -fsanitize=address to find the word
		// behind a memory error.
		uint32_t from = 0x0000;
		uint32_t to = 0xFFFF;
		if (argc > 2) from = (uint32_t)strtoul(argv[2], nullptr, 0);
		if (argc > 3) to = (uint32_t)strtoul(argv[3], nullptr, 0);

		g_haltReturns = true;
		core->SetJitMaxBlockInstrs(1);

		for (uint32_t word = from; word <= to; word++)
		{
			for (int engine = 0; engine < (useJit ? 2 : 1); engine++)
			{
				core->HardReset();
				memset(core->TranslateIMem(0), 0, IRAM_BYTES);
				memset(core->TranslateDMem(0), 0, DRAM_BYTES);
				memset(core->TranslateDMem(0x1000), 0, DROM_BYTES);

				core->regs.a.bits = 0x5A12345678ULL;
				core->regs.b.bits = 0xA5EDCBA987ULL;
				core->regs.x.h = 0x0005; core->regs.x.l = 0xFFFB;
				core->regs.y.h = 0x1234; core->regs.y.l = 0xEDCC;
				core->regs.r[0] = 0x0010;
				core->regs.r[1] = 0x0020;
				core->regs.r[2] = 0x0030;
				core->regs.r[3] = 0x0040;
				core->regs.m[0] = 0x0001;
				core->regs.m[1] = 0x0002;
				core->regs.m[2] = 0xFFFE;
				core->regs.m[3] = 0x0004;
				core->regs.l[0] = 0x000F;
				core->regs.l[1] = 0x001F;
				core->regs.l[2] = 0xFFFF;
				core->regs.l[3] = 0x003F;
				core->regs.dpp = 0x00FF;
				core->regs.psr.bits = 0;
				core->regs.pc = 0;

				uint16_t* iram = (uint16_t*)core->TranslateIMem(0);
				iram[0] = _BYTESWAP_UINT16((uint16_t)word);

				core->ResetInstructionCounter();
				if (engine == 0) core->Step(); else core->RunJitBlock();
			}
		}

		printf("sweep %04X..%04X done\n", from, to);
		return 0;
	}
	else if (strcmp(mode, "nop") == 0)
	{
		// IRAM full of nops with a `jmp 0` at the end, so the run can be arbitrarily long.
		memset(core->TranslateIMem(0), 0, IRAM_BYTES);
		uint16_t* iram = (uint16_t*)core->TranslateIMem(0);
		iram[(IRAM_BYTES / 2) - 2] = _BYTESWAP_UINT16(0x029F);
		iram[(IRAM_BYTES / 2) - 1] = _BYTESWAP_UINT16(0x0000);
		entry = 0;
		if (argc > 2) wanted = strtoull(argv[2], nullptr, 0);
	}
	else
	{
		fprintf(stderr, "unknown mode: %s\n", mode);
		return 2;
	}

	// The block limit is exposed for bisecting a codegen problem: DSP_JIT_BLOCK=1 makes every
	// RunJitBlock retire exactly one instruction. DSP_ENTRY overrides the start address.
	if (const char* limit = getenv("DSP_JIT_BLOCK"))
	{
		core->SetJitMaxBlockInstrs((uint32_t)atoi(limit));
	}
	if (const char* startPc = getenv("DSP_ENTRY"))
	{
		entry = (DSP::DspAddress)strtoul(startPc, nullptr, 0);
	}

	core->regs.pc = entry;
	core->ResetInstructionCounter();

	printf("Loaded %s, entry %04X, running %llu instructions with the %s...\n",
		mode, entry, (unsigned long long)wanted, useJit ? "recompiler" : "interpreter");

	auto start = std::chrono::steady_clock::now();

	bool trace = getenv("DSP_TRACE") != nullptr;

	uint64_t retired = 0;
	if (useJit)
	{
		while (retired < wanted)
		{
			DSP::DspAddress before = core->regs.pc;
			if (trace)
			{
				printf("block pc=%04X ...\n", before);
				fflush(stdout);
			}
			uint32_t count = core->RunJitBlock();
			if (trace)
			{
				printf("block pc=%04X retired=%u -> pc=%04X\n", before, count, core->regs.pc);
				fflush(stdout);
			}
			retired += count;
		}
	}
	else
	{
		while (retired < wanted)
		{
			core->Step();
			retired++;
		}
	}

	auto end = std::chrono::steady_clock::now();
	double seconds = std::chrono::duration<double>(end - start).count();
	uint64_t hash = HashState(core);

	printf("Executed %llu instructions in %.3f s (%.1f MIPS)\n",
		(unsigned long long)retired, seconds,
		seconds > 0 ? ((double)retired / seconds / 1e6) : 0.0);
	printf("state hash: %016llX\n", (unsigned long long)hash);
	printf("pc=%04X a=%010llX b=%010llX p=%010llX psr=%04X\n",
		core->regs.pc,
		(unsigned long long)(core->regs.a.bits & 0xFFFFFFFFFFULL),
		(unsigned long long)(core->regs.b.bits & 0xFFFFFFFFFFULL),
		(unsigned long long)(core->regs.prod.bitsPacked & 0xFFFFFFFFFFULL),
		core->regs.psr.bits);
	if (getenv("DSP_DETAIL"))
	{
		printf("counter=%lld dpp=%04X x=%08X y=%08X\n",
			(long long)core->GetInstructionCounter(), core->regs.dpp, core->regs.x.bits, core->regs.y.bits);
		printf("r=%04X,%04X,%04X,%04X m=%04X,%04X,%04X,%04X l=%04X,%04X,%04X,%04X\n",
			core->regs.r[0], core->regs.r[1], core->regs.r[2], core->regs.r[3],
			core->regs.m[0], core->regs.m[1], core->regs.m[2], core->regs.m[3],
			core->regs.l[0], core->regs.l[1], core->regs.l[2], core->regs.l[3]);
		printf("stacks pcs=%d pss=%d eas=%d lcs=%d dram=%016llX drom=%016llX\n",
			core->regs.pcs->size(), core->regs.pss->size(), core->regs.eas->size(), core->regs.lcs->size(),
			(unsigned long long)HashMemory(core, false), (unsigned long long)HashMemory(core, true));
	}

	return 0;
}
