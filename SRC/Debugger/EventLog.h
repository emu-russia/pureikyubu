// Event log support

// The facility description is in \Docs\EMU\EventLog.md

#pragma once

#include "../Common/Json.h"
#include "../Common/Spinlock.h"
#include "Debugger.h"
#include <vector>

namespace Debug
{
	// As new types from different subsystems are added, this enumeration runs the risk of being too de-encapsulated, 
	// but since Event Log is virtually used by all subsystems, this is normal.

	enum class EventType : size_t
	{
		Unknown = 0,

		DebugReport,		// From DBReport2

	};

	class EventLog
	{
		Json eventHistory;
		Json::Value* rootObj;
		Json::Value* subsystems;
		// All enumerated collections are protected by a lock.
		SpinLock eventLock;

		Json::Value* GetSubsystem(DbgChannel chan);

	public:
		EventLog();
		~EventLog();

		/// @brief Add an event to the history.
		void Add(DbgChannel chan, EventType type, std::vector<uint8_t>& arbitraryData);

		/// @brief Get event history as serialized Json text. Then you can save the text to a file or transfer it to the utility to display the history.
		void ToString(std::string & jsonText);
	};

	// Global instance. Available only after the emulation started, otherwise nullptr.
	// Event collection does not depend on whether the debugger is running or not.
	extern EventLog* Log;
}
