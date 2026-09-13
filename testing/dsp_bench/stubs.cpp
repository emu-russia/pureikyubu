// Standalone benchmark stubs for the pureikyubu DSP core.
//
// The DSP sources are compiled into this harness from a scratch copy (see build.sh), so
// the few emulator-wide entities they reference (console main memory, the PI register
// window, the Flipper tick source, JDI, Debug::Report/Halt) exist here instead of pulling
// in all of Flipper / Gekko.

#include "pch.h"

// ---------------------------------------------------------------------------
// Debug

bool g_verbose = false;
bool g_haltReturns = false;

namespace Debug
{
	void Halt(const char* text, ...)
	{
		va_list arg;
		char buf[0x1000];
		va_start(arg, text);
		vsprintf(buf, text, arg);
		va_end(arg);

		if (g_haltReturns)
		{
			return;
		}

		// The benchmark workloads are built from instructions the core really executes, so a
		// Halt means the harness (or the workload) is broken rather than the core. Report it
		// loudly and stop instead of silently measuring a no-op.
		fprintf(stderr, "HALT: %s\n", buf);
		fflush(stderr);
		exit(3);
	}

	void Report(Channel chan, const char* text, ...)
	{
		if (!g_verbose || chan == Channel::Void) return;

		va_list arg;
		char buf[0x1000];
		va_start(arg, text);
		vsprintf(buf, text, arg);
		va_end(arg);
		printf("%s", buf);
	}

	void CoreProfilerHalt() {}
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
// Gekko tick source

Gekko::GekkoCore* Core = nullptr;

namespace
{
	Gekko::GekkoCore tickSource;
}

namespace Gekko
{
	CpuStats stats;
	bool cycleProfile = false;

	int64_t GekkoCore::GetTicks()
	{
		return 0;
	}

	void GekkoCore::Suspend() {}
}

// ---------------------------------------------------------------------------
// Flipper / console memory

namespace Flipper
{
	Flipper* HW = nullptr;
	DSP::Dsp16* DSP = nullptr;

	namespace
	{
		const size_t MainMemorySize = 32 * 1024 * 1024;
		uint8_t* mainMemory = nullptr;

		Flipper hwInstance;
		MemoryInterface memInstance;
		ProcessorInterface piInstance;

		struct Binder
		{
			Binder()
			{
				hwInstance.mem = &memInstance;
				hwInstance.pi = &piInstance;
				HW = &hwInstance;
			}
		} binder;
	}

	void* MemoryInterface::MIGetMemoryPointerForDSP(uint32_t phys_addr)
	{
		if (mainMemory == nullptr)
		{
			mainMemory = new uint8_t[MainMemorySize];
			memset(mainMemory, 0, MainMemorySize);
		}
		if (phys_addr >= MainMemorySize) return nullptr;
		return mainMemory + phys_addr;
	}

	void ProcessorInterface::PIAssertInt(uint32_t mask) {}
	void ProcessorInterface::PIClearInt(uint32_t mask) {}
	void ProcessorInterface::PISetTrap(uint32_t, void (*)(uint32_t, uint32_t*, void*), void (*)(uint32_t, uint32_t, void*), void*) {}
}

// ---------------------------------------------------------------------------
// DSP device hooks (dspai.cpp is not part of the harness)

namespace DSP
{
	DspAIControl dsp_ai;

	// DSPUpdateInt lives in dsparam.cpp (it is the ARAM interrupt path), the rest in dspai.cpp,
	// which is not part of this harness.
	void DSPAssertInt() { dsp_ai.cdcr |= 1; }
	bool DSPGetInterruptStatus() { return (dsp_ai.cdcr & 1) != 0; }
	bool DSPGetResetModifier() { return false; }
	void DspSetAiDmaSampleRate(int32_t rate) { dsp_ai.dmaRate = rate; }
	void dsp_init_handlers() {}
}

// ---------------------------------------------------------------------------
// Util / JDI

namespace Util
{
	std::vector<uint8_t> FileLoad(const std::string& filename)
	{
		std::vector<uint8_t> data;
		FILE* f = fopen(filename.c_str(), "rb");
		if (f == nullptr) return data;
		fseek(f, 0, SEEK_END);
		long size = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (size > 0)
		{
			data.resize((size_t)size);
			if (fread(data.data(), 1, (size_t)size, f) != (size_t)size) data.clear();
		}
		fclose(f);
		return data;
	}

	bool FileSave(const std::string& filename, std::vector<uint8_t>& data)
	{
		FILE* f = fopen(filename.c_str(), "wb");
		if (f == nullptr) return false;
		bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
		fclose(f);
		return ok;
	}
}

namespace JDI
{
	HubClass Hub;

	void HubClass::AddNode(std::wstring filename, const char* jsonText, void (*reflector)()) {}
	void HubClass::RemoveNode(std::wstring filename) {}
}

namespace JdiSpecs
{
	const char* DspJdi = "{}";
}
