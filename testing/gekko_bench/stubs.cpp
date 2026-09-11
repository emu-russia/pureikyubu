// Standalone benchmark stubs for the pureikyubu Gekko core.

#include "pch.h"
#include <csetjmp>

jmp_buf g_fuzzJmp;
bool g_fuzzActive = false;

// ---------------------------------------------------------------------------
// Debug

bool g_verbose = false;

namespace Debug
{
	void Halt(const char* text, ...)
	{
		va_list arg;
		char buf[0x1000];
		va_start(arg, text);
		vsprintf(buf, text, arg);
		va_end(arg);

		printf("HALT: %s\n", buf);
		if (Core)
		{
			printf("  pc=%08X r3=%08X r4=%08X r10=%08X msr=%08X sr0=%08X\n",
				Core->regs.pc, Core->regs.gpr[3], Core->regs.gpr[4], Core->regs.gpr[10],
				Core->regs.msr, Core->regs.sr[0]);
			printf("  SRR1=%08X IBAT1U=%08X IBAT1L=%08X\n",
				Core->regs.spr[(int)Gekko::SPR::SRR1],
				Core->regs.spr[(int)Gekko::SPR::IBAT1U], Core->regs.spr[(int)Gekko::SPR::IBAT1L]);
			printf("  SRR0=%08X DAR=%08X DSISR=%08X\n",
				Core->regs.spr[(int)Gekko::SPR::SRR0], Core->regs.spr[(int)Gekko::SPR::DAR],
				Core->regs.spr[(int)Gekko::SPR::DSISR]);
			printf("  DBAT0U=%08X DBAT0L=%08X DBAT1U=%08X DBAT1L=%08X IBAT0U=%08X IBAT0L=%08X\n",
				Core->regs.spr[(int)Gekko::SPR::DBAT0U], Core->regs.spr[(int)Gekko::SPR::DBAT0L],
				Core->regs.spr[(int)Gekko::SPR::DBAT1U], Core->regs.spr[(int)Gekko::SPR::DBAT1L],
				Core->regs.spr[(int)Gekko::SPR::IBAT0U], Core->regs.spr[(int)Gekko::SPR::IBAT0L]);
		}
		fflush(stdout);
		if (g_fuzzActive) longjmp(g_fuzzJmp, 1);
		exit(3);
	}

	void Report(Channel chan, const char* text, ...)
	{
		// Silent by default: the harness measures, it does not log.
		if (!g_verbose) return;
		va_list arg;
		char buf[0x1000];
		va_start(arg, text);
		vsprintf(buf, text, arg);
		va_end(arg);
		printf("%s", buf);
	}
}

// ---------------------------------------------------------------------------
// Threading

Thread* EMUCreateThread(ThreadProc threadProc, bool suspended, void* context, const char* name)
{
	return new Thread(threadProc, suspended, context, name);
}

void EMUJoinThread(Thread* thread)
{
	delete thread;
}

// ---------------------------------------------------------------------------
// Flipper HW

namespace Flipper
{
	uint8_t* ProcessorInterface::ram = nullptr;
	uint32_t ProcessorInterface::ramSize = 0;
	uint8_t* ProcessorInterface::bootrom = nullptr;
	uint32_t ProcessorInterface::bootromSize = 0;
	uint64_t ProcessorInterface::mmioReads = 0;
	uint64_t ProcessorInterface::mmioWrites = 0;

	FlipperStub hwInstance{};
	FlipperStub* HW = &hwInstance;

	static ProcessorInterface piInstance;
	struct PiBinder { PiBinder() { HW->pi = &piInstance; } } piBinder;

	// Unmapped / device reads return 0xFFFFFFFF for the boot ROM window and 0 for
	// everything else - close enough for a CPU-only benchmark.

	void ProcessorInterface::PIReadByte(uint32_t pa, uint32_t* reg)
	{
		uint8_t* ptr = RamPtr(pa);
		if (ptr) { *reg = *ptr; return; }
		if (BootromPtr(pa, 1)) { *reg = *BootromPtr(pa, 1); return; }
		mmioReads++;
		*reg = (pa >= PI_MEMSPACE_BOOTROM) ? (bootrom ? 0 : 0xFF) : 0;
	}

	void ProcessorInterface::PIReadHalf(uint32_t pa, uint32_t* reg)
	{
		uint8_t* ptr = RamPtr(pa);
		if (ptr) { *reg = _BYTESWAP_UINT16(*(uint16_t*)ptr); return; }
		if (BootromPtr(pa, 2)) { *reg = _BYTESWAP_UINT16(*(uint16_t*)BootromPtr(pa, 2)); return; }
		mmioReads++;
		*reg = (pa >= PI_MEMSPACE_BOOTROM) ? (bootrom ? 0 : 0xFFFF) : 0;
	}

	void ProcessorInterface::PIReadWord(uint32_t pa, uint32_t* reg)
	{
		uint8_t* ptr = RamPtr(pa);
		if (ptr) { *reg = _BYTESWAP_UINT32(*(uint32_t*)ptr); return; }
		if (BootromPtr(pa, 4)) { *reg = _BYTESWAP_UINT32(*(uint32_t*)BootromPtr(pa, 4)); return; }
		mmioReads++;
		*reg = (pa >= PI_MEMSPACE_BOOTROM) ? (bootrom ? 0 : 0xFFFFFFFF) : 0;
	}

	void ProcessorInterface::PIReadDouble(uint32_t pa, uint64_t* reg)
	{
		uint8_t* ptr = RamPtr(pa);
		if (ptr) { *reg = _BYTESWAP_UINT64(*(uint64_t*)ptr); return; }
		mmioReads++;
		*reg = 0;
	}

	void ProcessorInterface::PIWriteByte(uint32_t pa, uint32_t data)
	{
		uint8_t* ptr = RamPtr(pa);
		if (ptr) { *ptr = (uint8_t)data; return; }
		mmioWrites++;
	}

	void ProcessorInterface::PIWriteHalf(uint32_t pa, uint32_t data)
	{
		uint8_t* ptr = RamPtr(pa);
		if (ptr) { *(uint16_t*)ptr = _BYTESWAP_UINT16((uint16_t)data); return; }
		mmioWrites++;
	}

	void ProcessorInterface::PIWriteWord(uint32_t pa, uint32_t data)
	{
		uint8_t* ptr = RamPtr(pa);
		if (ptr) { *(uint32_t*)ptr = _BYTESWAP_UINT32(data); return; }
		mmioWrites++;
	}

	void ProcessorInterface::PIWriteDouble(uint32_t pa, uint64_t* data)
	{
		uint8_t* ptr = RamPtr(pa);
		if (ptr) { *(uint64_t*)ptr = _BYTESWAP_UINT64(*data); return; }
		mmioWrites++;
	}

	void ProcessorInterface::PIReadBurst(uint32_t phys_addr, uint8_t burstData[32])
	{
		uint8_t* ptr = RamPtr(phys_addr);
		if (ptr) { memcpy(burstData, ptr, 32); return; }
		if (BootromPtr(phys_addr, 32)) { memcpy(burstData, BootromPtr(phys_addr, 32), 32); return; }
		mmioReads++;
		memset(burstData, 0, 32);
	}

	void ProcessorInterface::PIWriteBurst(uint32_t phys_addr, uint8_t burstData[32])
	{
		uint8_t* ptr = RamPtr(phys_addr);
		if (ptr) { memcpy(ptr, burstData, 32); return; }
		mmioWrites++;
	}
}
