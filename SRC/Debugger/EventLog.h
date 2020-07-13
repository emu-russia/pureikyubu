// Event log support

// The facility description is in \Docs\EMU\EventLog.md

#pragma once

#include "../Common/Json.h"
#include "../Common/Spinlock.h"
#include "Debugger.h"

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

		void TraceBegin(DbgChannel chan, std::string s);

		void TraceEnd(DbgChannel chan);

		void TraceEvent(DbgChannel chan, std::string text);

		/// @brief Get event history as serialized Json text. Then you can save the text to a file or transfer it to the utility to display the history.
		void ToString(std::string & jsonText);
	};

	// Global instance. Available only after the emulation started, otherwise nullptr.
	// Event collection does not depend on whether the debugger is running or not.
	extern EventLog* Log;
}
