// Standalone benchmark stub for the pureikyubu Gekko core.
// Replaces src/pch.h so that gekko.cpp / gekkoc.cpp / gekkodec.cpp can be built
// without SDL / GL / ImGui and without the rest of Flipper.

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <atomic>
#include <cassert>
#include <cmath>
#include <ctime>
#include <limits.h>

extern bool g_verbose;

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

// ---------------------------------------------------------------------------
// Debug (stub)

namespace Debug
{
	enum class Channel
	{
		Void = 0,
		Norm, Info, Error, Header,
		CP, PE, VI, GP, PI, CPU, MI, DSP, DI, AR, AI, AIS, SI, EXI, MC, DVD, AX,
		Loader, HLE,
	};

	void Halt(const char* text, ...);
	void Report(Channel chan, const char* text, ...);
}

// ---------------------------------------------------------------------------
// Threading (stub - the benchmark never actually spawns emulator threads)

class SpinLock
{
	std::atomic_flag locked = ATOMIC_FLAG_INIT;
public:
	void Lock() { while (locked.test_and_set(std::memory_order_acquire)) {} }
	void Unlock() { locked.clear(std::memory_order_release); }
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
// Gekko core headers

#include "gekkodec.h"
#include "gekko.h"
#include "gekkoc.h"
#if defined(BENCH_WITH_JIT)
#include "gekkojit.h"
#endif
#include "gekkodisasm.h"

// ---------------------------------------------------------------------------
// Flipper PI / MEM (stub)

#define PI_MEMSPACE_BOOTROM     0xFFF0'0000
#define HW_BASE                 0x0C000000

namespace Flipper
{
	class ProcessorInterface
	{
	public:
		// Main memory (flat, backing store is provided by the harness)
		static uint8_t* ram;
		static uint32_t ramSize;

		// Optional boot ROM image, served for physical addresses >= 0xFFF00000.
		static uint8_t* bootrom;
		static uint32_t bootromSize;

		// Statistics used by the harness
		static uint64_t mmioReads;
		static uint64_t mmioWrites;

		BENCH_ALWAYS_INLINE static uint8_t* RamPtr(uint32_t pa)
		{
			return (pa < ramSize) ? &ram[pa] : nullptr;
		}

		// Returns a pointer into the boot ROM image, or nullptr when the address is
		// outside it (or outside the requested access size).
		BENCH_ALWAYS_INLINE static uint8_t* BootromPtr(uint32_t pa, uint32_t need)
		{
			if (bootrom == nullptr || pa < PI_MEMSPACE_BOOTROM) return nullptr;
			uint32_t off = pa - PI_MEMSPACE_BOOTROM;
			return (off + need <= bootromSize) ? &bootrom[off] : nullptr;
		}

		void PIReadByte(uint32_t pa, uint32_t* reg);
		void PIReadHalf(uint32_t pa, uint32_t* reg);
		void PIReadWord(uint32_t pa, uint32_t* reg);
		void PIReadDouble(uint32_t pa, uint64_t* reg);
		void PIWriteByte(uint32_t pa, uint32_t data);
		void PIWriteHalf(uint32_t pa, uint32_t data);
		void PIWriteWord(uint32_t pa, uint32_t data);
		void PIWriteDouble(uint32_t pa, uint64_t* data);
		void PIReadBurst(uint32_t phys_addr, uint8_t burstData[32]);
		void PIWriteBurst(uint32_t phys_addr, uint8_t burstData[32]);
	};

	// The harness has no Flipper ASIC: the CPU drives the periodic VI/serial work through this
	// hook (see GekkoCore::SyncFlipper), so the stub only has to provide the interface.
	static const int64_t FlipperTickStep = 100;

	struct FlipperStub
	{
		ProcessorInterface* pi;

		void Update(int64_t ticks) { (void)ticks; }
	};

	extern FlipperStub* HW;
}

extern Gekko::GekkoCore* Core;
