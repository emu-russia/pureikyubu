// Support layer for the DSP core unit tests.
//
// The DSP sources (dspcore.cpp, dspdec.cpp, dspdisasm.cpp, dsp.cpp, dspdma.cpp, dsparam.cpp)
// are pulled into this project as file links (see pureikyubu_test.vcxproj), so the few
// external entities they reference have to exist at link time as well.
//
// The tests deliberately exercise the DSP in isolation: the core is stepped by hand, one
// instruction at a time, and the ARAM (SDRAM) and main memory of the console are replaced by
// plain buffers. This file supplies that environment:
//
//   * a console main memory buffer behind MemoryInterface::MIGetMemoryPointerForDSP, so that
//     the real DSP-DMA code (dspdma.cpp) can be exercised;
//   * an ARAM buffer used by the real ARAM accelerator / deADPCM decoder (dsparam.cpp);
//   * a minimal Gekko tick source (the DSP core only reads ticks when it runs as a thread,
//     which the unit tests never do);
//   * Debug::Report / Debug::Halt routed into the test process instead of the emulator GUI.
//
// Nothing here implements DSP behaviour: it only replaces the surrounding hardware.

#include "pch.h"

namespace
{
	// ------------------------------------------------------------------
	// Console main memory (Splash). The GameCube has 24 MB of it; the audio system
	// stages its bootstrap block at physical 0x0100_0000, so the buffer is a bit larger.
	// ------------------------------------------------------------------

	const size_t TestMainMemorySize = 32 * 1024 * 1024;
	uint8_t* testMainMemory = nullptr;

	// Gekko time base for the few places that consult it.
	volatile int64_t testGekkoTicks = 0;
}

// ----------------------------------------------------------------------
// Test environment accessors (used by the test machine)
// ----------------------------------------------------------------------

/// <summary>
/// Physical pointer into the simulated console main memory (same contract as the real MI).
/// </summary>
uint8_t* DspTestMainMemory(uint32_t physAddr, size_t size)
{
	if (testMainMemory == nullptr)
	{
		testMainMemory = new uint8_t[TestMainMemorySize];
		memset(testMainMemory, 0, TestMainMemorySize);
	}

	if ((size_t)physAddr + size > TestMainMemorySize)
	{
		return nullptr;
	}

	return testMainMemory + physAddr;
}

uint8_t* DspTestMainMemoryBase()
{
	DspTestMainMemory(0, 0);
	return testMainMemory;
}

/// <summary>
/// Allocate the ARAM SDRAM image. In the emulator this is done by AROpen() on the Flipper side.
/// </summary>
void DspTestInitAram()
{
	if (DSP::aram.mem == nullptr)
	{
		DSP::aram.mem = new uint8_t[ARAMSIZE];
		memset(DSP::aram.mem, 0, ARAMSIZE);
	}

	// The ARAM driver normally owns this thread; it is not started in unit tests.
	DSP::aram.dmaThread = nullptr;
	DSP::aram.mmaddr = 0;
	DSP::aram.araddr = 0;
	DSP::aram.cnt = 0;
	DSP::aram.size = 0x5000;
}

void DspTestSetGekkoTicks(int64_t ticks)
{
	testGekkoTicks = ticks;
}

int64_t DspTestGetGekkoTicks()
{
	return testGekkoTicks;
}

// ----------------------------------------------------------------------
// Emulator threads
//
// The emulator wraps thread creation in EMUCreateThread / EMUJoinThread (main.cpp) so that
// the `threads` debug command can enumerate them. The unit tests only need the wrappers.
// ----------------------------------------------------------------------

namespace
{
	std::list<Thread*> testThreads;
}

Thread* EMUCreateThread(ThreadProc threadProc, bool suspended, void* context, const char* name)
{
	Thread* thread = new Thread(threadProc, suspended, context, name);
	testThreads.push_back(thread);
	return thread;
}

void EMUJoinThread(Thread* thread)
{
	testThreads.remove(thread);
	delete thread;
}

// ----------------------------------------------------------------------
// Debug output
// ----------------------------------------------------------------------

namespace
{
	CRITICAL_SECTION testLogLock;
	bool testLogInit = false;
	std::string testLog;
	std::string lastHalt;
	int haltCount = 0;
	bool reportEnabled = false;

	void LogInit()
	{
		if (!testLogInit)
		{
			InitializeCriticalSection(&testLogLock);
			testLogInit = true;
		}
	}
}

void DspTestLogClear()
{
	LogInit();
	EnterCriticalSection(&testLogLock);
	testLog.clear();
	lastHalt.clear();
	haltCount = 0;
	LeaveCriticalSection(&testLogLock);
}

void DspTestLogEnable(bool enable)
{
	reportEnabled = enable;
}

std::string DspTestLogText()
{
	LogInit();
	EnterCriticalSection(&testLogLock);
	std::string text = testLog;
	LeaveCriticalSection(&testLogLock);
	return text;
}

std::string DspTestLastHalt()
{
	LogInit();
	EnterCriticalSection(&testLogLock);
	std::string text = lastHalt;
	LeaveCriticalSection(&testLogLock);
	return text;
}

int DspTestHaltCount()
{
	LogInit();
	EnterCriticalSection(&testLogLock);
	int count = haltCount;
	LeaveCriticalSection(&testLogLock);
	return count;
}

namespace Debug
{
	/// <summary>
	/// The emulator uses Halt() to abort emulation on an unrecoverable condition.
	/// In the tests the report is recorded instead (a test asserts on it if the condition
	/// is expected), so that a fault does not kill the whole test run.
	/// </summary>
	void Halt(const char* text, ...)
	{
		va_list arg;
		char buf[0x1000] = { 0, };

		va_start(arg, text);
		vsprintf_s(buf, sizeof(buf), text, arg);
		va_end(arg);

		LogInit();
		EnterCriticalSection(&testLogLock);
		lastHalt = buf;
		haltCount++;
		testLog += "[HALT] ";
		testLog += buf;
		LeaveCriticalSection(&testLogLock);

		OutputDebugStringA("[DSP HALT] ");
		OutputDebugStringA(buf);
	}

	void Report(Channel chan, const char* text, ...)
	{
		if (chan == Channel::Void)
		{
			return;
		}

		va_list arg;
		char buf[0x1000] = { 0, };

		va_start(arg, text);
		vsprintf_s(buf, sizeof(buf), text, arg);
		va_end(arg);

		LogInit();
		EnterCriticalSection(&testLogLock);
		if (reportEnabled)
		{
			testLog += buf;
		}
		LeaveCriticalSection(&testLogLock);
	}
}

// ----------------------------------------------------------------------
// Gekko core
// ----------------------------------------------------------------------

// The DSP core dereferences Core only from DspCore::Update / Dsp16::Run (tick counting and
// thread suspension). Unit tests drive the core with DspCore::Step(), which never touches it,
// and DspCore::HardReset() tolerates a null Core. The tick accessor is still defined because
// the linker needs it.

Gekko::GekkoCore* Core = nullptr;

namespace Gekko
{
	int64_t GekkoCore::GetTicks()
	{
		return testGekkoTicks;
	}
}

namespace Debug
{
	// Some of the emulator paths that the DSP sources reference pull in the profiler hooks.
	void CoreProfilerHalt() {}
}

// ----------------------------------------------------------------------
// Flipper peripherals that the DSP subsystem talks to
// ----------------------------------------------------------------------

namespace Flipper
{
	Flipper* HW = nullptr;
	DSP::Dsp16* DSP = nullptr;

	// PI interrupt line, so that tests can observe the DSP -> CPU interrupt.
	uint32_t TestPIAssertedInts = 0;

	void ProcessorInterface::PIAssertInt(uint32_t mask)
	{
		TestPIAssertedInts |= mask;
	}

	void ProcessorInterface::PIClearInt(uint32_t mask)
	{
		TestPIAssertedInts &= ~mask;
	}

	void ProcessorInterface::PISetTrap(
		uint32_t addr,
		void (*rdTrap)(uint32_t, uint32_t*, void*),
		void (*wrTrap)(uint32_t, uint32_t, void*),
		void* context)
	{
		// The DSP tests do not go through the CPU register window.
	}

	void* MemoryInterface::MIGetMemoryPointerForDSP(uint32_t phys_addr)
	{
		if (phys_addr >= TestMainMemorySize)
		{
			return nullptr;
		}
		return DspTestMainMemoryBase() + phys_addr;
	}
}

// ----------------------------------------------------------------------
// DSP device: control hooks and JDI
// ----------------------------------------------------------------------

namespace DSP
{
	// DSP/AI control register block (see dspai.cpp in the main project). Only the
	// Mailbox-relevant hooks are needed: the AI DMA engine itself is Flipper-side.
	DspAIControl dsp_ai;

	void DSPAssertInt()
	{
		dsp_ai.dcr |= AIDCR_DSPINT;
	}

	bool DSPGetInterruptStatus()
	{
		return (dsp_ai.dcr & AIDCR_DSPINT) != 0;
	}

	bool DSPGetResetModifier()
	{
		return (dsp_ai.dcr & AIDCR_RESETMOD) != 0;
	}

	void DspSetAiDmaSampleRate(int32_t rate)
	{
		dsp_ai.dmaRate = rate;
	}

	// JDI command handlers (dspdebug.cpp) are not part of the core test scope; the node is
	// still registered so that Dsp16 construction behaves exactly as in the emulator.
	void dsp_init_handlers()
	{
	}
}
