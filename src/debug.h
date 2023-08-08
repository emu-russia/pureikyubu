/*

# Debugger

This component deals with the usual debugging tasks that all developers want to do:

- Collecting debug messages
- Tracing
- Code profiling
- Performance counters

This component no longer has anything to do with the debug console. The debug console code has been moved to the category of user interfaces and moved to the UI\\Legacy folder.

## Debug report notes

Previously, debug messages would pass into the internals of the debug console (cmd_print).

Debug messages are now a self-contained entity and are stored in the debug message queue.

Debug UI (or whoever claims it), when active, should periodically ask - "Is there something?". If there is, the current messages from the queue are transferred to the debug UI and the queue is cleared.

Also, in order not to occupy all the user's memory, debug messages are cleared by themselves after reaching a certain limit of messages.

*/

#pragma once

// Centralized hub for all debug messages

namespace Debug
{
	// Debug messages output channel

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

	// always breaks emulation
	void Halt(const char* text, ...);

	// do debugger output
	void Report(Channel chan, const char* text, ...);

	class ReportEntry
	{
	public:
		Channel savedChan;
		std::string text;

		ReportEntry(Channel chan, const std::string& message) : savedChan(chan), text(message)
		{
		}
	};

	class ReportHub
	{
		static const size_t MessageLimit = 1000;

		// All enumerated collections are protected by a lock.
		SpinLock reportLock;

		std::list<ReportEntry*> reportQueue;

		void Flush(bool lockable);

	public:
		ReportHub();
		~ReportHub();

		std::string DebugChannelToString(Channel chan);

		void QueryDebugMessages(std::list<std::pair<Channel, std::string>>& queue);

		void AddReport(Channel chan, bool haltCpu, const std::string& text);
	};

	extern ReportHub Msgs;
}


// Sampling Profiler.
// Description in Docs\EMU\SamplingProfiler.md

namespace Debug
{

	class SamplingProfiler
	{
		std::string filename;

		Thread* thread = nullptr;
		static void ThreadProc(void* Parameter);

		uint64_t pollingInterval = 100;			// Program Counter polling frequency
		uint64_t savedGekkoTbr = 0;

		Json* json = nullptr;
		Json::Value* rootObj = nullptr;
		Json::Value* sampleData = nullptr;

	public:
		SamplingProfiler(const char* jsonFileName, int periodMs);
		~SamplingProfiler();
	};

}

// Debugger Jdi

#define DEBUGGER_JDI_JSON L"./Data/Json/DebuggerJdi.json"

namespace Debug
{
	void Reflector();
}



// Event log support

// The facility description is in \Docs\EMU\EventLog.md

namespace Debug
{
	class EventLog
	{
		Json eventHistory;
		Json::Value* traceEvents;
		// All enumerated collections are protected by a lock.
		SpinLock eventLock;

	public:
		EventLog();
		~EventLog();

		void TraceBegin(Channel chan, char* s);

		void TraceEnd(Channel chan);

		void TraceEvent(Channel chan, char* text);

		/// @brief Get event history as serialized Json text. Then you can save the text to a file or transfer it to the utility to display the history.
		void ToString(std::string& jsonText);
	};

	// Global instance. Available only after the emulation started, otherwise nullptr.
	// Event collection does not depend on whether the debugger is running or not.
	extern EventLog* Log;
}


// Performance Counters.

// Statistics are sent to the UI through the GetPerformanceCounter / ResetPerformanceCounter Jdi commands.

namespace Debug
{

	enum class PerfCounter
	{
		GekkoInstructions = 0,		// Number of Gekko instructions executed
		DspInstructions,		// Number of DSP instructions executed
		VIs,				// Number of VI VBlank interrupts (based on PI interrupt counters)
		PEs,				// Number of PE DRAW_DONE operations (based on PI interrupt counters)

		Max,
	};

	class PerfCounters
	{
	public:
		PerfCounters();
		~PerfCounters();

		int64_t GetCounter(PerfCounter counter);
		void ResetCounter(PerfCounter counter);
		void ResetAllCounters();
	};

	// A global instance, created by the emulator in the EMUCtor method and is available throughout the life of the emulator
	// (another thing is that statistics are updated only when the emulator is running).
	extern PerfCounters* g_PerfCounters;
}
