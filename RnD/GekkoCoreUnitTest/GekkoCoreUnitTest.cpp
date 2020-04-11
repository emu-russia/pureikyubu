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
	};
}
