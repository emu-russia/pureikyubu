// Event log support

#include "pch.h"

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

	void EventLog::TraceBegin(Channel chan, char * s)
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

	void EventLog::TraceEvent(Channel chan, char * text)
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

	void EventLog::ToString(std::string & jsonText)
	{
		size_t actualTextSize = 0;
		eventHistory.GetSerializedTextSize((void *)jsonText.data(), -1, actualTextSize);
		jsonText.resize(actualTextSize);
		eventHistory.Serialize((void*)jsonText.data(), actualTextSize, actualTextSize);
	}
}
