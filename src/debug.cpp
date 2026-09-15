#include "pch.h"

namespace Debug
{

	ReportHub Msgs;			// Singletone

	bool ConsoleEcho = false;

	void Halt(const char* text, ...)
	{
		va_list arg;
		char buf[0x1000] = { 0, };

		va_start(arg, text);
		vsnprintf(buf, sizeof(buf), text, arg);
		va_end(arg);

		// The reason has to reach the report log (`EMU_LOG=<file>`) as well as the debugger's
		// message queue: a headless run has no debugger window open, and the log is the only
		// place the crash reason can be read from.
		Report(Channel::Error, "%s", buf);
		Msgs.AddReport(Channel::Error, true, buf);

		bool any_debugger_present = false;

		if (Debug::debugger != nullptr) {
			Debug::debugger->InvalidateAll();
			any_debugger_present = true;
		}

		if (!any_debugger_present) {

			Jdi->ExecuteCommand(
				std::string("UIReport \"The emulation is crashed. Details can be viewed in the debugger (Ctrl+D)\n\n" + std::string(buf) + "\"").c_str());
		}
	}

	void Report(Channel chan, const char* text, ...)
	{
		if (chan == Channel::Void)
		{
			return;
		}

		va_list arg;
		char buf[0x1000] = { 0, };
		static FILE* logFile = nullptr;
		static bool logChecked = false;

		va_start(arg, text);
		vsnprintf(buf, sizeof(buf), text, arg);
		va_end(arg);

		// Optional debug log (useful when no debugger window is open)
		if (!logChecked)
		{
			logChecked = true;
			const char* logName = getenv("EMU_LOG");
			if (logName != nullptr && logName[0] != 0)
			{
				logFile = fopen(logName, "w");
			}
		}

		if (logFile != nullptr)
		{
			fprintf(logFile, "[%s] %s", Msgs.DebugChannelToString(chan).c_str(), buf);
			fflush(logFile);
		}

		// A headless run has no debugger window to read the message queue from.
		if (ConsoleEcho)
		{
			fputs(buf, stdout);
			fflush(stdout);
		}

		Msgs.AddReport(chan, false, buf);
	}

	ReportHub::ReportHub()
	{
	}

	ReportHub::~ReportHub()
	{
		Flush(true);
	}

	void ReportHub::Flush(bool lockable)
	{
		if (lockable)
		{
			reportLock.Lock();
		}
		while (!reportQueue.empty())
		{
			ReportEntry* entry = reportQueue.back();
			reportQueue.pop_back();
			delete entry;
		}
		if (lockable)
		{
			reportLock.Unlock();
		}
	}

	/// <summary>
	/// Get the human-readable name of a debug channel
	/// </summary>
	/// <param name="chan"></param>
	/// <returns></returns>
	std::string ReportHub::DebugChannelToString(Channel chan)
	{
		switch (chan)
		{
			case Channel::Norm: return "";
			case Channel::Info: return "Info";
			case Channel::Error: return "Error";
			case Channel::Header: return "Header";

			case Channel::CP: return "CP";
			case Channel::PE: return "PE";
			case Channel::VI: return "VI";
			case Channel::GP: return "GP";
			case Channel::PI: return "PI";
			case Channel::CPU: return "CPU";
			case Channel::MI: return "MI";
			case Channel::DSP: return "DSP";
			case Channel::DI: return "DI";
			case Channel::AR: return "AR";
			case Channel::AI: return "AI";
			case Channel::AIS: return "AIS";
			case Channel::SI: return "SI";
			case Channel::EXI: return "EXI";
			case Channel::MC: return "MC";
			case Channel::DVD: return "DVD";
			case Channel::AX: return "AX";

			case Channel::Loader: return "Loader";
			case Channel::HLE: return "HLE";
		}

		return "Unknown";
	}

	/// <summary>
	/// Get history of debug messages (oldest first). Clear queue in progress.
	/// </summary>
	/// <param name="queue"></param>
	void ReportHub::QueryDebugMessages(std::list<std::pair<Channel, std::string>>& queue)
	{
		reportLock.Lock();
		if (reportQueue.size() != 0)
		{
			queue.clear();

			while (!reportQueue.empty())
			{
				ReportEntry* entry = reportQueue.back();
				reportQueue.pop_back();

				queue.push_front(std::pair<Channel, std::string>(entry->savedChan, entry->text));

				delete entry;
			}
		}
		reportLock.Unlock();
	}

	/// <summary>
	/// Add a debug message. Stop emulation if necessary.
	/// </summary>
	/// <param name="chan"></param>
	/// <param name="haltCpu"></param>
	/// <param name="text"></param>
	void ReportHub::AddReport(Channel chan, bool haltCpu, const std::string& text)
	{
		if (chan == Channel::Void)
		{
			return;
		}

		reportLock.Lock();

		// If no one reads the message history, it periodically cleans itself.

		if (reportQueue.size() >= MessageLimit)
		{
			// No need to lock, already locked
			Flush(false);
		}

		ReportEntry* entry = new ReportEntry(chan, text);
		reportQueue.push_back(entry);
		reportLock.Unlock();

		// Stop emulating the entire system on demand 

		if (haltCpu)
		{
			if (Core->IsRunning())
			{
				Core->Suspend();
			}
		}
	}

}


namespace Debug
{
	void SamplingProfiler::ThreadProc(void* Parameter)
	{
		SamplingProfiler* profiler = (SamplingProfiler*)Parameter;

		uint64_t ticks = Core->GetTicks();
		if (ticks >= (profiler->savedGekkoTbr + profiler->pollingInterval))
		{
			profiler->savedGekkoTbr = ticks;

			profiler->sampleData->AddUInt64(nullptr, ticks);
			profiler->sampleData->AddUInt32(nullptr, Core->regs.pc);
		}
	}

	SamplingProfiler::SamplingProfiler(const char* jsonFileName, int periodMs)
	{
		filename = jsonFileName;

		pollingInterval = periodMs * (Core->OneSecond() / 1000);
		savedGekkoTbr = Core->GetTicks();

		json = new Json();

		rootObj = json->root.AddObject(nullptr);
		assert(rootObj);

		sampleData = rootObj->AddArray("sampleData");
		assert(sampleData);

		thread = EMUCreateThread(ThreadProc, false, this, "SamplingProfiler");
	}

	SamplingProfiler::~SamplingProfiler()
	{
		EMUJoinThread(thread);

		size_t textSize = 0;

		json->GetSerializedTextSize(nullptr, -1, textSize);

		uint8_t* jsonText = new uint8_t[2 * textSize];
		assert(jsonText);

		json->Serialize(jsonText, 2 * textSize, textSize);

		auto buffer = std::vector<uint8_t>(jsonText, jsonText + textSize);
		Util::FileSave(filename, buffer);

		delete[] jsonText;
		delete json;
	}

}


// Debug commands


#define my_max(a,b) (((a) > (b)) ? (a) : (b))
#define my_min(a,b) (((a) < (b)) ? (a) : (b))

namespace Debug
{
	static bool testempty(char* str)
	{
		int len = (int)strlen(str);

		for (int i = 0; i < len; i++)
		{
			// Unsigned, like the trim above: a UTF-8 byte is never "blank", and a plain char is
			// signed on Windows, where the high bytes would all look like control characters.
			if ((uint8_t)str[i] > ' ') return false;
		}

		return true;
	}

	// Splits one command line into arguments. Returns false (instead of throwing) when a
	// quotation is not closed: an exception here would leave cmd_script, the JDI hub and the
	// loader with no handler at all and terminate the process.
	static bool Tokenize(char* line, std::vector<std::string>& args)
	{
#define endl    ( line[p] == 0 )
#define space   ( line[p] == 0x20 )
#define quot    ( line[p] == '\'' )
#define dquot   ( line[p] == '\"' )

		int p, start, end;
		p = start = end = 0;

		args.clear();

		// while not end line
		while (!endl)
		{
			// skip space first, if any
			while (space) p++;
			if (!endl && (quot || dquot))
			{   // quotation, need special case
				p++;
				start = p;
				while (1)
				{
					if (endl)
					{
						args.clear();
						return false;
					}

					if (quot || dquot)
					{
						end = p;
						p++;
						break;
					}
					else p++;
				}

				args.push_back(std::string(line + start, end - start));
			}
			else if (!endl)
			{
				start = p;
				while (1)
				{
					if (endl || space || quot || dquot)
					{
						end = p;
						break;
					}

					p++;
				}

				args.push_back(std::string(line + start, end - start));
			}
		}
#undef space
#undef quot
#undef dquot
#undef endl

		return true;
	}

	static Json::Value* cmd_script(std::vector<std::string>& args)
	{
		size_t i;

		// The JDI parameter check only enforces the minimum declared by the command spec, so a
		// direct call can still arrive without the file name.
		if (args.size() < 2)
		{
			Report(Channel::Error, "Script: file not specified.\n");
			return nullptr;
		}

		// A script that runs itself (directly, or through `load`, which re-runs autoexec.cmd)
		// would recurse once per line; every level also holds its own copy of the file.
		static int scriptDepth = 0;

		if (scriptDepth >= 8)
		{
			Report(Channel::Error, "Script '%s': too many nested scripts.\n", args[1].c_str());
			return nullptr;
		}

		// RAII, so the counter is restored on every early return below as well.
		struct DepthGuard
		{
			DepthGuard() { scriptDepth++; }
			~DepthGuard() { scriptDepth--; }
		} depthGuard;

		const char* file;
		std::vector<std::string> commandArgs;

		file = args[1].c_str();

		Report(Channel::Norm, "Loading script: %s\n", file);

		auto sbuf = Util::FileLoad((std::string)file);
		if (sbuf.empty())
		{
			Report(Channel::Norm, "Cannot open script file!\n");
			return nullptr;
		}
		sbuf.push_back(0);

		/* Remove all garbage, like tabs. CR-only files are split as well. */
		for (i = 0; i < sbuf.size(); i++)
		{
			if (sbuf[i] < ' ')
			{
				sbuf[i] = '\n';
			}
		}

		Report(Channel::Norm, "Executing script...\n");

		int cnt = 1;
		size_t position = 0;

		while (position < sbuf.size())
		{
			char line[1000];
			bool truncated = false;

			// The line is always bounded and NUL-terminated; at the end of the script the
			// loop ends instead of walking past the buffer looking for a '\n'.
			if (!Verify::ScriptLine(sbuf.data(), sbuf.size(), position, line, sizeof(line), truncated))
			{
				break;
			}

			if (truncated)
			{
				// The tail was dropped, so the fragment is not the command that was written.
				Report(Channel::Error, "%i: line is too long, skipped\n", cnt++);
				continue;
			}

			// remove comments
			char* p = line;
			while (*p)
			{
				if (p[0] == '/' && p[1] == '/')
				{
					*p = 0;
					break;
				}
				p++;
			}

			// Remove the spaces at the end and find the first non-blank character. A line that
			// is empty, blank or nothing but a comment comes back as nullptr and is skipped.
			p = Verify::ScriptTrim(line);
			if (p == nullptr) continue;

			// execute line
			if (testempty(line)) continue;
			Report(Channel::Norm, "%i: %s", cnt, line);
			int lineNumber = cnt++;

			commandArgs.clear();
			if (!Tokenize(line, commandArgs))
			{
				Report(Channel::Error, "Line %i has an open quotation, skipped\n", lineNumber);
				continue;
			}
			line[0] = 0;

			JDI::Hub.Execute(commandArgs);
		}

		Report(Channel::Norm, "\nDone execute script.\n");
		return nullptr;
	}

	// Echo
	static Json::Value* cmd_echo(std::vector<std::string>& args)
	{
		std::string text = "";

		for (size_t i = 1; i < args.size(); i++)
		{
			text += args[i] + " ";
		}

		Report(Channel::Norm, "%s\n", text.c_str());
		return nullptr;
	}

	static SamplingProfiler* profiler = nullptr;

	static Json::Value* StartProfiler(std::vector<std::string>& args)
	{
		if (args.size() < 2)
		{
			Report(Channel::Error, "StartProfiler: file not specified.\n");
			return nullptr;
		}

		if (profiler)
		{
			Report(Channel::Norm, "Already started.\n");
			return nullptr;
		}

		int period = 5;
		if (args.size() > 2)
		{
			period = atoi(args[2].c_str());
			period = my_max(2, my_min(period, 50));
		}

		profiler = new SamplingProfiler(args[1].c_str(), period);

		Report(Channel::Norm, "Profiler started.\n");

		return nullptr;
	}

	static Json::Value* StopProfiler(std::vector<std::string>& args)
	{
		if (profiler == nullptr)
		{
			Report(Channel::Norm, "Not started.\n");
			return nullptr;
		}

		delete profiler;
		profiler = nullptr;

		Report(Channel::Norm, "Profiler stopped.\n");

		return nullptr;
	}

	static Json::Value* GetChannelName(std::vector<std::string>& args)
	{
		// Keep the declared output type even when the argument is missing, so a caller that
		// dereferences the reply does not crash.
		if (args.size() < 2)
		{
			Report(Channel::Error, "GetChannelName: channel not specified.\n");
			Json::Value* empty = new Json::Value();
			empty->type = Json::ValueType::Array;
			return empty;
		}

		Channel chan = (Channel)atoi(args[1].c_str());

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		output->AddString(nullptr, Util::StringToWstring(Msgs.DebugChannelToString(chan)).c_str());

		return output;
	}

	static Json::Value* QueryDebugMessages(std::vector<std::string>& args)
	{
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		std::list<std::pair<Channel, std::string>> queue;
		Msgs.QueryDebugMessages(queue);

		for (auto it = queue.begin(); it != queue.end(); ++it)
		{
			output->AddInt(nullptr, (int)it->first);
			output->AddString(nullptr, Util::StringToWstring(it->second).c_str());
		}

		return output;
	}

	static Json::Value* ShowHelp(std::vector<std::string>& args)
	{
		JDI::Hub.Help();
		Report(Channel::Header, "## Debugger F-Keys\n");
		Report(Channel::Norm, "- F1: Registers (left/right arrows to select registers)\n");
		Report(Channel::Norm, "- F2: Memory dump\n");
		Report(Channel::Norm, "- F3: Instruction disassembly\n");
		Report(Channel::Norm, "- F5: Start emulation to breakpoint/pause emulation (break)\n");
		Report(Channel::Norm, "- F9: Toogle instruction breakpoint\n");
		Report(Channel::Norm, "- F10: Step over\n");
		Report(Channel::Norm, "- F11: Step into\n");
		Report(Channel::Norm, "- F12: Skip instruction\n");
		return nullptr;
	}

	static Json::Value* IsCommandExists(std::vector<std::string>& args)
	{
		if (args.size() < 2)
		{
			Report(Channel::Error, "IsCommandExists: command name not specified.\n");
			Json::Value* output = new Json::Value();
			output->type = Json::ValueType::Bool;
			output->value.AsBool = false;
			return output;
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Bool;
		output->value.AsBool = JDI::Hub.CommandExists(args[1]);
		return output;
	}

	// Get the value of the debug counter
	static Json::Value* CmdGetPerformanceCounter(std::vector<std::string>& args)
	{
		if (args.size() < 2)
		{
			Report(Channel::Error, "GetPerformanceCounter: counter not specified.\n");
			Json::Value* output = new Json::Value();
			output->type = Json::ValueType::Int;
			output->value.AsInt = 0;
			return output;
		}

		PerfCounter counter = (PerfCounter)strtoul(args[1].c_str(), nullptr, 0);
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;
		output->value.AsInt = g_PerfCounters->GetCounter(counter);
		return output;
	}

	// Reset the value of the debug counter
	static Json::Value* CmdResetPerformanceCounter(std::vector<std::string>& args)
	{
		if (args.size() < 2)
		{
			Report(Channel::Error, "ResetPerformanceCounter: counter not specified.\n");
			return nullptr;
		}

		PerfCounter counter = (PerfCounter)strtoul(args[1].c_str(), nullptr, 0);
		g_PerfCounters->ResetCounter(counter);
		return nullptr;
	}

	void Reflector()
	{
		JDI::Hub.AddCmd("script", cmd_script);
		JDI::Hub.AddCmd("echo", cmd_echo);
		JDI::Hub.AddCmd("StartProfiler", StartProfiler);
		JDI::Hub.AddCmd("StopProfiler", StopProfiler);
		JDI::Hub.AddCmd("GetChannelName", GetChannelName);
		JDI::Hub.AddCmd("qd", QueryDebugMessages);
		JDI::Hub.AddCmd("help", ShowHelp);
		JDI::Hub.AddCmd("IsCommandExists", IsCommandExists);
		JDI::Hub.AddCmd("GetPerformanceCounter", CmdGetPerformanceCounter);
		JDI::Hub.AddCmd("ResetPerformanceCounter", CmdResetPerformanceCounter);

		// The HW interface profiler (issue #394): `hwprofile` is the report, `hwsod` is the
		// overlay it draws over the emulated picture.
		HwOsd::Reflector();
	}
}


// Event log support

namespace Debug
{
	EventLog* Log;

	EventLog::EventLog()
	{
		traceEvents = eventHistory.root.AddArray(nullptr);
		assert(traceEvents);
	}

	EventLog::~EventLog()
	{
		// The memory will be cleared along with json root (eventHistory).
	}

	void EventLog::TraceBegin(Channel chan, char* s)
	{
		eventLock.Lock();

		Json::Value* entry = traceEvents->AddObject(nullptr);
		assert(entry);

		Json::Value* pid = entry->AddInt("pid", 1);
		assert(pid);

		Json::Value* tid = entry->AddInt("pid", (int)chan);
		assert(tid);

		Json::Value* ts = entry->AddUInt64("ts", Core->GetTicks());
		assert(ts);

		Json::Value* ph = entry->AddUtf8String("ph", "B");
		assert(ph);

		Json::Value* name = entry->AddUtf8String("name", s);
		assert(name);

		eventLock.Unlock();
	}

	void EventLog::TraceEnd(Channel chan)
	{
		eventLock.Lock();

		Json::Value* entry = traceEvents->AddObject(nullptr);
		assert(entry);

		Json::Value* pid = entry->AddInt("pid", 1);
		assert(pid);

		Json::Value* tid = entry->AddInt("pid", (int)chan);
		assert(tid);

		Json::Value* ts = entry->AddUInt64("ts", Core->GetTicks());
		assert(ts);

		Json::Value* ph = entry->AddUtf8String("ph", "E");
		assert(ph);

		eventLock.Unlock();
	}

	void EventLog::TraceEvent(Channel chan, char* text)
	{
		eventLock.Lock();

		Json::Value* entry = traceEvents->AddObject(nullptr);
		assert(entry);

		Json::Value* pid = entry->AddInt("pid", 1);
		assert(pid);

		Json::Value* tid = entry->AddInt("pid", (int)chan);
		assert(tid);

		Json::Value* ts = entry->AddUInt64("ts", Core->GetTicks());
		assert(ts);

		Json::Value* ph = entry->AddUtf8String("ph", "I");
		assert(ph);

		Json::Value* name = entry->AddUtf8String("name", text);
		assert(name);

		eventLock.Unlock();
	}

	void EventLog::ToString(std::string& jsonText)
	{
		size_t actualTextSize = 0;
		eventHistory.GetSerializedTextSize((void*)jsonText.data(), -1, actualTextSize);
		jsonText.resize(actualTextSize);
		eventHistory.Serialize((void*)jsonText.data(), actualTextSize, actualTextSize);
	}
}


namespace Debug
{
	PerfCounters* g_PerfCounters = nullptr;

	PerfCounters::PerfCounters()
	{
	}

	PerfCounters::~PerfCounters()
	{
	}

	int64_t PerfCounters::GetCounter(PerfCounter counter)
	{
		int64_t value = 0;

		switch (counter)
		{
			case PerfCounter::GekkoInstructions:
				return Core->GetInstructionCounter();
				break;
			case PerfCounter::DspInstructions:
				return Flipper::DSP->core->GetInstructionCounter();
				break;
			case PerfCounter::VIs:
				// The hardware counters belong to the emulated machine, which is built when an
				// image is loaded (EMUOpen) and freed when it is closed: before that there is
				// nothing to count, and walking the null Flipper used to crash the emulator.
				return (Flipper::HW != nullptr) ? Flipper::HW->pi->PIGetInterruptCounter(PIInterruptSource::VI) : 0;
				break;
			case PerfCounter::PEs:
				return (Flipper::HW != nullptr) ? Flipper::HW->pi->PIGetInterruptCounter(PIInterruptSource::PE_FINISH) : 0;
				break;

			case PerfCounter::GekkoCompiledSegments:
				return Gekko::stats.jitCompiles;
				break;
			case PerfCounter::GekkoExecutedSegments:
				return Gekko::stats.jitBlocks;
				break;
		}

		return value;
	}

	void PerfCounters::ResetCounter(PerfCounter counter)
	{
		switch (counter)
		{
			case PerfCounter::GekkoInstructions:
				Core->ResetInstructionCounter();
				break;
			case PerfCounter::DspInstructions:
				Flipper::DSP->core->ResetInstructionCounter();
				break;
			case PerfCounter::VIs:
				if (Flipper::HW != nullptr)
					Flipper::HW->pi->PIResetInterruptCounter(PIInterruptSource::VI);
				break;
			case PerfCounter::PEs:
				if (Flipper::HW != nullptr)
					Flipper::HW->pi->PIResetInterruptCounter(PIInterruptSource::PE_FINISH);
				break;

			case PerfCounter::GekkoCompiledSegments:
				Gekko::stats.jitCompiles = 0;
				break;
			case PerfCounter::GekkoExecutedSegments:
				Gekko::stats.jitBlocks = 0;
				break;
		}
	}

	void PerfCounters::ResetAllCounters()
	{
		for (int i = 0; i < (int)PerfCounter::Max; i++)
		{
			ResetCounter((PerfCounter)i);
		}
	}

}