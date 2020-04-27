#include "../pch.h"

namespace Gekko
{

	void Jitc::FallbackStub(AnalyzeInfo* info, CodeSegment* seg)
	{
		seg->Write8(0x90);		// nop

		// Call ExecuteInterpeterFallback

		//0:  48 b8 cd ab 78 56 34    movabs rax,0x12345678abcd  (ExecuteInterpeterFallback)
		//7:  12 00 00
		//17: ff d0                   call   rax

		uint64_t fnPtr = (uint64_t)GekkoCore::ExecuteInterpeterFallback;

		seg->Write16(0xb848);
		seg->Write64(fnPtr);
		seg->Write16(0xd0ff);

		//0:  84 c0                   test   al, al
		//2:  74 01                   je     5 <label>
		//4:  c3                      ret
		//0000000000000005 <label>:

		seg->Write16(0xc084);
		seg->Write16(0x0174);
		seg->Write8(0xc3);
	}

}
