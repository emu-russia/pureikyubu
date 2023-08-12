#include "pch.h"

namespace Debug
{
	ReportHub Msgs;			// Singletone

	void Halt(const char* text, ...)
	{
		va_list arg;
		char buf[0x1000] = { 0, };

		va_start(arg, text);
		vsprintf(buf, text, arg);
		va_end(arg);

		Msgs.AddReport(Channel::Error, true, buf);

		bool any_debugger_present = false;

		if (Debug::debugger != nullptr) {
			Debug::debugger->InvalidateAll();
			any_debugger_present = true;
		}

		// TODO: Jdi it

#ifdef _WINDOWS
		if (!any_debugger_present) {
			UI::Report(
				Util::StringToWstring(
					std::string("The emulation is crashed. Details can be viewed in the debugger (Ctrl+D)\n\n" + std::string(buf))).c_str());
		}
#endif
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
		vsprintf(buf, text, arg);
		va_end(arg);

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
			if (str[i] > ' ') return false;
		}

		return true;
	}

	static void Tokenize(char* line, std::vector<std::string>& args)
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
						throw "Open quotation";
						return;
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
	}

	static Json::Value* cmd_script(std::vector<std::string>& args)
	{
		size_t i;
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

		/* Remove all garbage, like tabs. */
		for (i = 0; i < sbuf.size(); i++)
		{
			char c = sbuf[i];

			if (c < ' ')
			{
				c = '\n';
			}
		}

		Report(Channel::Norm, "Executing script...\n");

		int cnt = 1;
		char* ptr = (char*)sbuf.data();
		while (*ptr)
		{
			char line[1000];
			line[i = 0] = 0;

			// Cut string
			while (*ptr == '\n') ptr++;
			if (!*ptr) break;
			while (*ptr != '\n') line[i++] = *ptr++;
			line[i++] = 0;

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

			// remove spaces at the end
			p = &line[strlen(line) - 1];
			while (*p <= ' ') p--;
			if (*p) p[1] = 0;

			// remove spaces at the beginning
			p = line;
			while (*p <= ' ' && *p) p++;

			// empty string ?
			if (!*p) continue;

			// execute line
			if (testempty(line)) continue;
			Report(Channel::Norm, "%i: %s", cnt++, line);

			commandArgs.clear();
			Tokenize(line, commandArgs);
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
		if (profiler)
		{
			Report(Channel::Norm, "Already started.\n");
			return nullptr;
		}

		int period = 5;
		if (args.size() > 2)
		{
			period = atoi(args[2].c_str());
			period = my_min(2, my_max(period, 50));
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
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Bool;
		output->value.AsBool = JDI::Hub.CommandExists(args[1]);
		return output;
	}

	// Get the value of the debug counter
	static Json::Value* CmdGetPerformanceCounter(std::vector<std::string>& args)
	{
		PerfCounter counter = (PerfCounter)strtoul(args[1].c_str(), nullptr, 0);
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;
		output->value.AsInt = g_PerfCounters->GetCounter(counter);
		return output;
	}

	// Reset the value of the debug counter
	static Json::Value* CmdResetPerformanceCounter(std::vector<std::string>& args)
	{
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

		Json::Value* ph = entry->AddAnsiString("ph", "B");
		assert(ph);

		Json::Value* name = entry->AddAnsiString("name", s);
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

		Json::Value* ph = entry->AddAnsiString("ph", "E");
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

		Json::Value* ph = entry->AddAnsiString("ph", "I");
		assert(ph);

		Json::Value* name = entry->AddAnsiString("name", text);
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
				return pi.intCounters[(size_t)PIInterruptSource::VI];
				break;
			case PerfCounter::PEs:
				return pi.intCounters[(size_t)PIInterruptSource::PE_FINISH];
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
				pi.intCounters[(size_t)PIInterruptSource::VI] = 0;
				break;
			case PerfCounter::PEs:
				pi.intCounters[(size_t)PIInterruptSource::PE_FINISH] = 0;
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
