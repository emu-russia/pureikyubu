#include "pch.h"

namespace Debug
{
	ReportHub Msgs;			// Singletone

	void Halt(const char* text, ...)
	{
		va_list arg;
		char buf[0x1000] = { 0, };

		va_start(arg, text);
		vsprintf_s(buf, sizeof(buf) - 1, text, arg);
		va_end(arg);

		Msgs.AddReport(Channel::Error, true, buf);
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
		vsprintf_s (buf, sizeof(buf) - 1, text, arg);
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
	/// Get history of debug messages. Clear queue in progress.
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
			if (Gekko::Gekko->IsRunning())
			{
				Gekko::Gekko->Suspend();
			}
		}
	}

}
