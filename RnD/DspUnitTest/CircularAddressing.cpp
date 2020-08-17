// Testing the Circular Addressing Logic

#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace DSP
{
	namespace DspCore
	{
		uint16_t CircularAddress(uint16_t r, uint16_t l, int16_t m)
		{
			if (m == 0 || l == 0)
			{
				return r;
			}

			if (l == 0xffff)
			{
				return (uint16_t)((int16_t)r + m);
			}
			else
			{
				int16_t abs_m = m > 0 ? m : -m;
				int16_t mm = abs_m % (l + 1);
				uint16_t base = (r / (l + 1)) * (l + 1);
				uint16_t next = 0;
				uint32_t sum = 0;

				if (m > 0)
				{
					sum = (uint32_t)((uint32_t)r + mm);
				}
				else
				{
					sum = (uint32_t)((uint32_t)r + l + 1 - mm);
				}

				next = base + (uint16_t)(sum % (l + 1));

				return next;
			}
		}
	}
}

namespace DspUnitTest
{
	TEST_CLASS(DspUnitTest)
	{
	public:

		TEST_METHOD(TestCircularAddressing)
		{
			// Linear address 

			Assert::IsTrue(DSP::DspCore::CircularAddress(0, 0xffff, 1) == 1);
			Assert::IsTrue(DSP::DspCore::CircularAddress(1, 0xffff, -1) == 0);
			Assert::IsTrue(DSP::DspCore::CircularAddress(0, 0xffff, -1) == 0xffff);
			Assert::IsTrue(DSP::DspCore::CircularAddress(0x8000, 0xffff, -1) == 0x7fff);
			Assert::IsTrue(DSP::DspCore::CircularAddress(0x8000, 0xffff, 1) == 0x8001);

			// Buffer size 7 (l=6)

			Assert::IsTrue(DSP::DspCore::CircularAddress(5, 6, +1) == 6);
			Assert::IsTrue(DSP::DspCore::CircularAddress(6, 6, +1) == 0);
			Assert::IsTrue(DSP::DspCore::CircularAddress(0, 6, -1) == 6);
			Assert::IsTrue(DSP::DspCore::CircularAddress(0xffff, 6, -2) == 4);
			Assert::IsTrue(DSP::DspCore::CircularAddress(0xffff, 6, +6) == 0xfffe);

			Assert::IsTrue(DSP::DspCore::CircularAddress(11, 6, +3) == 7);
			Assert::IsTrue(DSP::DspCore::CircularAddress(11, 6, -5) == 13);
			Assert::IsTrue(DSP::DspCore::CircularAddress(11, 6, +14) == 11);
			Assert::IsTrue(DSP::DspCore::CircularAddress(11, 6, -11) == 7);

		}


	};
}
