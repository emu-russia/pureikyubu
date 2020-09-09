#include "pch.h"

namespace Debug
{
	void SamplingProfiler::ThreadProc(void* Parameter)
	{
		SamplingProfiler* profiler = (SamplingProfiler *)Parameter;

		uint64_t ticks = Gekko::Gekko->GetTicks();
		if (ticks >= (profiler->savedGekkoTbr + profiler->pollingInterval))
		{
			profiler->savedGekkoTbr = ticks;

			profiler->sampleData->AddUInt64(nullptr, ticks);
			profiler->sampleData->AddUInt32(nullptr, Gekko::Gekko->regs.pc);
		}
	}

	SamplingProfiler::SamplingProfiler(const char* jsonFileName, int periodMs)
	{
		filename = jsonFileName;

		pollingInterval = periodMs * (Gekko::Gekko->OneSecond() / 1000);
		savedGekkoTbr = Gekko::Gekko->GetTicks();

		json = new Json();

		rootObj = json->root.AddObject(nullptr);
		assert(rootObj);

		sampleData = rootObj->AddArray("sampleData");
		assert(sampleData);

		thread = new Thread(ThreadProc, false, this, "SamplingProfiler");
	}

	SamplingProfiler::~SamplingProfiler()
	{
		delete thread;

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
