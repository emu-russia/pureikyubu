// addx

#include "../pch.h"
#include "CppUnitTest.h"
#include "BitFactory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GekkoCoreUnitTest
{
	TEST_CLASS(GekkoIsaUnitTest)
	{
	public:

		TEST_METHOD(Add)
		{
			Gekko::GekkoCore* core = new Gekko::GekkoCore();

			BitFactory bf;

			bf << Bits(31, 6);	// Primary opcode
			bf << Bits(1, 5);
			bf << Bits(2, 5);
			bf << Bits(3, 5);
			bf << Bits(0, 1);	// OE
			bf << Bits(266, 9);		// Secondary opcode
			bf << Bits(0, 1);	// Rc

			Gekko::AnalyzeInfo info = { 0 };

			Gekko::Analyzer::Analyze(0x8000'0000, bf.GetBits32(), &info);

			std::string text = Gekko::GekkoDisasm::Disasm(0x8000'0000, &info);

			Logger::WriteMessage(text.c_str());

			delete core;
		}
	};
}
