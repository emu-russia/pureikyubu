#include "pch.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace IntelCore;

namespace IntelAssemblerUnitTest
{
	TEST_CLASS(IntelAssemblerUnitTest)
	{
		void Check(AnalyzeInfo& info, uint8_t* bytes, size_t size)
		{
			uint8_t compiledInstr[32] = { 0 };
			size_t n = 0;

			for (size_t i = 0; i < info.prefixSize; i++)
			{
				compiledInstr[n++] = info.prefixBytes[i];
			}

			for (size_t i = 0; i < info.instrSize; i++)
			{
				compiledInstr[n++] = info.instrBytes[i];
			}

			Assert::IsTrue(memcmp(compiledInstr, bytes, size) == 0);
		}

	public:

		TEST_METHOD(SIB_mechanism)
		{
			Param groups[] = { 
				Param::MemSib32Scale1Start, Param::MemSib32Scale2Start, Param::MemSib32Scale4Start, Param::MemSib32Scale8Start, 
				Param::MemSib32Scale1Disp8Start, Param::MemSib32Scale2Disp8Start, Param::MemSib32Scale4Disp8Start, Param::MemSib32Scale8Disp8Start, 
				Param::MemSib32Scale1Disp32Start, Param::MemSib32Scale2Disp32Start, Param::MemSib32Scale4Disp32Start, Param::MemSib32Scale8Disp32Start, 
				Param::MemSib64Scale1Start, Param::MemSib64Scale2Start, Param::MemSib64Scale4Start, Param::MemSib64Scale8Start, 
				Param::MemSib64Scale1Disp8Start, Param::MemSib64Scale2Disp8Start, Param::MemSib64Scale4Disp8Start, Param::MemSib64Scale8Disp8Start, 
				Param::MemSib64Scale1Disp32Start, Param::MemSib64Scale2Disp32Start, Param::MemSib64Scale4Disp32Start, Param::MemSib64Scale8Disp32Start, };
			
			for (size_t g = 0; g < _countof(groups); g++)
			{
				size_t expected_scale = g % 4;

				for (int n = 0; n < 0x100; n++)
				{
					size_t scale, index, base;

					size_t expected_index = (n >> 4) & 0xf;
					size_t expected_base = n & 0xf;

					IntelAssembler::GetSS((Param)((size_t)groups[g] + n + 1), scale);
					IntelAssembler::GetIndex((Param)((size_t)groups[g] + n + 1), index);
					IntelAssembler::GetBase((Param)((size_t)groups[g] + n + 1), base);

					Assert::IsTrue(scale < 4);
					Assert::IsTrue(index < 16);
					Assert::IsTrue(base < 16);

					Assert::IsTrue(scale == expected_scale);
					Assert::IsTrue(index == expected_index);
					Assert::IsTrue(base == expected_base);
				}
			}
		}

		//TEST_METHOD(adc)
		//{
		//	uint8_t adcBytes1[] = { 0x14, 0xaa };
		//	Check(IntelAssembler::adc<16>(Param::al, Param::imm8, 0, 0xaa), adcBytes1, sizeof(adcBytes1));
		//}

		TEST_METHOD(nop)
		{
			uint8_t nopBytes[] = { 0x90 };
			Check(IntelAssembler::nop<16>(), nopBytes, sizeof(nopBytes));
		}
	};
}
