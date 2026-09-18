// Standalone benchmark stub for the pureikyubu DSP core.
//
// Replaces src/pch.h so that dsp.cpp / dspcore.cpp / dspdec.cpp / dspdma.cpp /
// dsparam.cpp / dspjit_x64.cpp / dspjit_x86.cpp can be built without SDL, GL, ImGui, the Gekko core or
// the rest of Flipper. It is the DSP counterpart of testing/gekko_bench/pch.h: the
// same idea, but the DSP core's dependency on the outside world is only the console
// main memory, the PI register window and the Flipper tick source.

#pragma once

// The range verifiers the copied DSP sources use (dsparam.cpp checks its DMA windows
// with them). The header is header-only and self-contained, so the stub can take the
// real one instead of a copy that goes stale.
#include "../../src/verify.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <atomic>
#include <cassert>
#include <cmath>
#include <ctime>
#include <limits.h>

extern bool g_verbose;

// When set, Debug::Halt records the condition and returns instead of stopping the process
// (the opcode sweep needs it: undefined words are part of the space under test).
extern bool g_haltReturns;

#if defined(_MSC_VER)
#define BENCH_ALWAYS_INLINE __forceinline
#else
#define BENCH_ALWAYS_INLINE inline __attribute__((always_inline))
#endif

#ifdef _LINUX
#include <byteswap.h>
#define _BYTESWAP_UINT16 __bswap_16
#define _BYTESWAP_UINT32 __bswap_32
#define _BYTESWAP_UINT64 __bswap_64
#else
#include <intrin.h>
#define _BYTESWAP_UINT16 _byteswap_ushort
#define _BYTESWAP_UINT32 _byteswap_ulong
#define _BYTESWAP_UINT64 _byteswap_uint64
#endif

#ifdef _LINUX
#define CNTLZ(mask) __builtin_clz(mask)
#else
#define CNTLZ(mask) __lzcnt(mask)
#endif

#define my_max(a,b) (((a) > (b)) ? (a) : (b))
#define my_min(a,b) (((a) < (b)) ? (a) : (b))

// From src/pi.h: the DSP register window and the aggregate DSP interrupt line. The DSP
// sources install ARAM register traps and re-evaluate the PI line through these.
struct HWConfig;
#define PI_REGSPACE_DSP         0x0C00'5000
#define PI_INTERRUPT_DSP        0x0040

// ---------------------------------------------------------------------------
// Debug (stub)

namespace Debug
{
	enum class Channel
	{
		Void = 0,

		Norm,
		Info,
		Error,
		Header,

		CP,
		PE,
		VI,
		GP,
		PI,
		CPU,
		MI,
		DSP,
		DI,
		AR,
		AI,
		AIS,
		SI,
		EXI,
		MC,
		DVD,
		AX,

		Loader,
		HLE,
	};

	void Halt(const char* text, ...);
	void Report(Channel chan, const char* text, ...);
	void CoreProfilerHalt();

	// The DSP sources count their DMA traffic in the hardware profiler (issue #394). The
	// benchmark does not profile, so the counters are a no-op here - but the enum has to
	// name every counter a copied source mentions, or the harness does not build.
	namespace HwProfile
	{
		enum class Counter
		{
			SplashRead, SplashWrite, DmaDsp, DmaAram, DmaAi, AudioMixer,
		};

		inline void Count(Counter counter, size_t value) {}
	}
}

// ---------------------------------------------------------------------------
// Threading (stub - the benchmark drives the core by hand and never starts the
// emulator's DSP thread)

class SpinLock
{
	std::atomic_flag locked = ATOMIC_FLAG_INIT;
public:
	void Lock() { while (locked.test_and_set(std::memory_order_acquire)) {} }
	void Unlock() { locked.clear(std::memory_order_release); }
};

class Event
{
public:
	void Signal() {}
	void Wait(int ms) {}
	void Reset() {}
};

typedef void (*ThreadProc)(void* param);

class Thread
{
public:
	Thread(ThreadProc proc, bool suspended, void* context, const char* name) {}
	~Thread() {}
	void Resume() {}
	void Suspend() {}
	bool IsRunning() { return false; }
	const char* GetName() { return "stub"; }
	static void Sleep(size_t ms) {}
};

Thread* EMUCreateThread(ThreadProc threadProc, bool suspended, void* context, const char* name);
void EMUJoinThread(Thread* thread);

// ---------------------------------------------------------------------------
// The Gekko core is only a tick source for the DSP (and the DSP thread is never
// started here), so a two-method stub is enough.

namespace Gekko
{
	struct CpuStats
	{
		uint64_t dspInstrs = 0;		// DSP instructions executed
		uint64_t dspWakes = 0;		// Times the DSP thread was woken
	};

	extern CpuStats stats;
	extern bool cycleProfile;

	class GekkoCore
	{
	public:
		int64_t GetTicks();
		void Suspend();
	};
}

extern Gekko::GekkoCore* Core;

// ---------------------------------------------------------------------------
// Flipper (stub)

namespace Flipper
{
	class MemoryInterface
	{
	public:
		void* MIGetMemoryPointerForDSP(uint32_t phys_addr);
		void* MIGetMemoryPointerForDSP(uint32_t phys_addr, size_t size);
		size_t MIGetMemorySize();
	};

	class ProcessorInterface
	{
	public:
		void PIAssertInt(uint32_t mask);
		void PIClearInt(uint32_t mask);
		void PISetTrap(
			uint32_t addr,
			void (*rdTrap)(uint32_t, uint32_t*, void*) = nullptr,
			void (*wrTrap)(uint32_t, uint32_t, void*) = nullptr,
			void* context = nullptr);
	};

	class Flipper
	{
	public:
		MemoryInterface* mem = nullptr;
		ProcessorInterface* pi = nullptr;
	};

	extern Flipper* HW;
}

// ---------------------------------------------------------------------------
// Util / JDI (stub)

namespace Util
{
	std::vector<uint8_t> FileLoad(const std::string& filename);
	bool FileSave(const std::string& filename, std::vector<uint8_t>& data);
}

namespace JDI
{
	class HubClass
	{
	public:
		void AddNode(std::wstring filename, const char* jsonText, void (*reflector)());
		void RemoveNode(std::wstring filename);
	};

	extern HubClass Hub;
}

namespace JdiSpecs
{
	extern const char* DspJdi;
}

// ---------------------------------------------------------------------------
// DSP headers

namespace DSP
{
	class Dsp16;

	// From src/dspdebug.h: Dsp16's constructor registers the JDI command node.
	void dsp_init_handlers();
}

namespace Flipper
{
	extern DSP::Dsp16* DSP;
}

#include "dspdec.h"
#include "dspcore.h"
#include "dspai.h"
#include "dsparam.h"
#include "dspdma.h"
#include "dsp.h"
#include "dspjit.h"
