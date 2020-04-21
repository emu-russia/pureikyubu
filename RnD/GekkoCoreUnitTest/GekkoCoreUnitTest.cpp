// This module contains only basic tests.
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

		TEST_METHOD(TestOldDisasm)
		{
			FILE* f = nullptr;

			fopen_s(&f, "Data\\test.bin", "rb");
			assert(f);

			fseek(f, 0, SEEK_END);
			size_t size = ftell(f);
			fseek(f, 0, SEEK_SET);

			uint8_t* testCode = new uint8_t[size];

			fread(testCode, 1, size, f);
			fclose(f);

			fopen_s(&f, "test.txt", "wt");
			assert(f);

			uint32_t pc = 0x80000000;
			size_t instrCount = size / 4;
			uint32_t* instrPtr = (uint32_t*)testCode;

			while (instrCount--)
			{
				uint32_t instr = *instrPtr++;

				char * disasmText = PPCDisasmSimple((uint64_t)pc, instr);
				fprintf(f, "%s\n", disasmText);

				pc += 4;
			}

			delete[] testCode;
			fclose(f);
		}

		TEST_METHOD(TestNewDisasm)
		{
			FILE* f = nullptr;

			fopen_s(&f, "Data\\test.bin", "rb");
			assert(f);

			fseek(f, 0, SEEK_END);
			size_t size = ftell(f);
			fseek(f, 0, SEEK_SET);

			uint8_t* testCode = new uint8_t[size];

			fread(testCode, 1, size, f);
			fclose(f);

			fopen_s(&f, "testNew.txt", "wt");
			assert(f);

			uint32_t pc = 0x80000000;
			size_t instrCount = size / 4;
			uint32_t* instrPtr = (uint32_t*)testCode;

			while (instrCount--)
			{
				uint32_t instr = *instrPtr++;

				Gekko::AnalyzeInfo info = { 0 };

				Gekko::Analyzer::Analyze(pc, instr, &info);
				std::string disasmText = Gekko::GekkoDisasm::Disasm(pc, &info);

				fprintf(f, "%s\n", disasmText.c_str());

				pc += 4;
			}

			delete[] testCode;
			fclose(f);
		}

	};
}
