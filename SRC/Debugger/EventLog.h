// Event log support

// The facility description is in \Docs\EMU\EventLog.md

#pragma once

#include "../Common/Spinlock.h"

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

		void TraceBegin(Channel chan, char * s);

		void TraceEnd(Channel chan);

		void TraceEvent(Channel chan, char * text);

		/// @brief Get event history as serialized Json text. Then you can save the text to a file or transfer it to the utility to display the history.
		void ToString(std::string & jsonText);
	};

	// Global instance. Available only after the emulation started, otherwise nullptr.
	// Event collection does not depend on whether the debugger is running or not.
	extern EventLog* Log;
}
