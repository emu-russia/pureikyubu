// Sampling Profiler.
// Description in Docs\EMU\SamplingProfiler.md

#pragma once

namespace Debug
{

	class SamplingProfiler
	{
		char filename[0x1000] = { 0, };

		Thread* thread = nullptr;
		static void ThreadProc(void* Parameter);

		uint64_t pollingInterval = 100;			// Program Counter polling frequency
		uint64_t savedGekkoTbr = 0;

		Json* json = nullptr;
		Json::Value* rootObj = nullptr;
		Json::Value* sampleData = nullptr;

	public:
		SamplingProfiler(const char* jsonFileName, int periodMs);
		~SamplingProfiler();
	};

}
