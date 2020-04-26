#include "../pch.h"

namespace Gekko
{

	void Jitc::FallbackStub(AnalyzeInfo* info, CodeSegment* seg)
	{
		seg->Write8(0x90);		// nop
		seg->Write8(0xc3);		// ret
	}

}
