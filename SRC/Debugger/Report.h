// Centralized hub for all debug messages

#pragma once

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
	void Halt (const char* text, ...);

	// do debugger output
	void Report (Channel chan, const char* text, ...);

	class ReportEntry
	{
	public:
		Channel savedChan;
		std::string text;

		ReportEntry(Channel chan, const std::string& message)
		{
			savedChan = chan;
			text = message;
		}
	};

	class ReportHub
	{
		static const size_t MessageLimit = 1000;

		// All enumerated collections are protected by a lock.
		SpinLock reportLock;

		std::list<ReportEntry *> reportQueue;

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
