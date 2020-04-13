#include "pch.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GekkoCoreUnitTest
{
	TEST_CLASS(GekkoCoreUnitTest)
	{
	public:
		
		TEST_METHOD(TestMethod1)
		{
			Gekko::GekkoCore* core = new Gekko::GekkoCore();

			delete core;
		}

		void DumpGekkoAnalyzeInfo(Gekko::AnalyzeInfo* info)
		{
			char text[0x100] = { 0, };
			
			sprintf_s(text, sizeof(text) - 1, "instr: %i, numParam: %zi, p0: %i, p1: %i, p2: %i",
				(int)info->instr,
				info->numParam,
				info->paramBits[0],
				info->paramBits[1],
				info->paramBits[2]);

			Logger::WriteMessage(text);
		}

		TEST_METHOD(SimpleAnalyzeInfo)
		{
			Gekko::AnalyzeInfo info = { 0 };

			uint32_t instr = (31 << 26) | (266 << 1);

			Gekko::Analyzer::Analyze(0, instr, &info);

			DumpGekkoAnalyzeInfo(&info);
		}

	};
}
