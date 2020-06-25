// Event log support

#include "pch.h"

namespace Debug
{
	EventLog* Log;

	EventLog::EventLog()
	{
		rootObj = eventHistory.root.AddObject(nullptr);
		assert(rootObj);

		subsystems = rootObj->AddObject("subsystems");
		assert(subsystems);
	}

	EventLog::~EventLog()
	{
		// The memory will be cleared along with json root (eventHistory).
	}

	Json::Value* EventLog::GetSubsystem(DbgChannel chan)
	{
		Json::Value* out = nullptr;

		switch (chan)
		{
            case DbgChannel::CP:
				out = subsystems->ByName("CP");
				if (!out)
					out = subsystems->AddObject("CP");
                break;
            case DbgChannel::PE:
				out = subsystems->ByName("PE");
				if (!out)
					out = subsystems->AddObject("PE");
                break;
            case DbgChannel::VI:
				out = subsystems->ByName("VI");
				if (!out)
					out = subsystems->AddObject("VI");
                break;
            case DbgChannel::GP:
				out = subsystems->ByName("GP");
				if (!out)
					out = subsystems->AddObject("GP");
                break;
            case DbgChannel::PI:
				out = subsystems->ByName("PI");
				if (!out)
					out = subsystems->AddObject("PI");
                break;
            case DbgChannel::CPU:
				out = subsystems->ByName("CPU");
				if (!out)
					out = subsystems->AddObject("CPU");
                break;
            case DbgChannel::MI:
				out = subsystems->ByName("MI");
				if (!out)
					out = subsystems->AddObject("MI");
                break;
            case DbgChannel::DSP:
				out = subsystems->ByName("DSP");
				if (!out)
					out = subsystems->AddObject("DSP");
                break;
            case DbgChannel::DI:
				out = subsystems->ByName("DI");
				if (!out)
					out = subsystems->AddObject("DI");
                break;
            case DbgChannel::AR:
				out = subsystems->ByName("AR");
				if (!out)
					out = subsystems->AddObject("AR");
                break;
            case DbgChannel::AI:
				out = subsystems->ByName("AI");
				if (!out)
					out = subsystems->AddObject("AI");
                break;
            case DbgChannel::AIS:
				out = subsystems->ByName("AIStream");
				if (!out)
					out = subsystems->AddObject("AIStream");
                break;
            case DbgChannel::SI:
				out = subsystems->ByName("SI");
				if (!out)
					out = subsystems->AddObject("SI");
                break;
            case DbgChannel::EXI:
				out = subsystems->ByName("EXI");
				if (!out)
					out = subsystems->AddObject("EXI");
                break;
            case DbgChannel::MC:
				out = subsystems->ByName("MemCards");
				if (!out)
					out = subsystems->AddObject("MemCards");
                break;
            case DbgChannel::DVD:
				out = subsystems->ByName("DVD");
				if (!out)
					out = subsystems->AddObject("DVD");
                break;
            case DbgChannel::AX:
				out = subsystems->ByName("AudioDAC");
				if (!out)
					out = subsystems->AddObject("AudioDAC");
                break;
		}

		return out;
	}

	void EventLog::Add(DbgChannel chan, EventType type, std::vector<uint8_t>& arbitraryData)
	{
		char entryName[0x100] = { 0, };

		eventLock.Lock();

		Json::Value* subsystem = GetSubsystem(chan);
		if (!subsystem)
		{
			eventLock.Unlock();
			return;
		}

		sprintf_s(entryName, sizeof(entryName) - 1, "%lld", Gekko::Gekko->GetTicks());

		Json::Value * entry = subsystem->AddObject(entryName);
		assert(entry);

		Json::Value* typeVal = entry->AddInt("type", (int)type);
		assert(typeVal);
		Json::Value* data = entry->AddArray("data");
		assert(data);

		for (size_t i = 0; i < arbitraryData.size(); i++)
		{
			data->AddInt(nullptr, arbitraryData.data()[i]);
		}

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
