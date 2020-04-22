// Test BitFactory

#include "../pch.h"
#include "CppUnitTest.h"
#include "BitFactory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GekkoCoreUnitTest
{
	TEST_CLASS(GekkoIsaUnitTest)
	{

		void PrintBitFactory(BitFactory& bf)
		{
			char text[0x20];
			sprintf_s(text, sizeof(text) - 1, "0x%llx", bf.GetBits64());
			Logger::WriteMessage(text);
		}

	public:

		TEST_METHOD(BitFactoryTest)
		{
			BitFactory bf;

			Assert::IsTrue(bf.GetBits64() == 0);

			// Test 0x12345678

			bf << Bits(1, 4);
			bf << Bits(2, 4);
			bf << Bits(3, 4);
			bf << Bits(4, 4);
			bf << Bits(5, 4);
			bf << Bits(6, 4);
			bf << Bits(7, 4);
			bf << Bits(8, 4);

			Assert::IsTrue(bf.GetBits32() == 0x12345678);
			bf.Clear();

			// Test 0b10100101

			bf << Bits(1, 1);
			bf << Bits(0, 1);
			bf << Bits(1, 1);
			bf << Bits(0, 1);
			bf << Bits(0, 1);
			bf << Bits(1, 1);
			bf << Bits(0, 1);
			bf << Bits(1, 1);

			Assert::IsTrue(bf.GetBits8() == 0xa5);

		}
	};
}
