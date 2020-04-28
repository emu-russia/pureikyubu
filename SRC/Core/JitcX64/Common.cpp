#include "../pch.h"

namespace Gekko
{
	// Special sections of code that are executed at the beginning and end of each translated segment.

	void Jitc::Prolog(CodeSegment* seg)
	{
		//0:  48 89 5c 24 08          mov    QWORD PTR [rsp+0x8],rbx
		//5:  48 89 6c 24 10          mov    QWORD PTR [rsp+0x10],rbp
		//a:  48 89 74 24 18          mov    QWORD PTR [rsp+0x18],rsi
		//f:  57                      push   rdi
		//10: 48 83 ec 40             sub    rsp,0x40

		seg->Write8(0x48);
		seg->Write32(0x08245c89);
		seg->Write8(0x48);
		seg->Write32(0x10246c89);
		seg->Write8(0x48);
		seg->Write32(0x18247489);
		seg->Write8(0x57);
		seg->Write16(0x8348);
		seg->Write16(0x40ec);

	}

	void Jitc::Epilog(CodeSegment* seg)
	{
		//14: 48 8b 5c 24 50          mov    rbx,QWORD PTR [rsp+0x50]
		//19: 48 8b 6c 24 58          mov    rbp,QWORD PTR [rsp+0x58]
		//1e: 48 8b 74 24 60          mov    rsi,QWORD PTR [rsp+0x60]
		//23: 48 83 c4 40             add    rsp,0x40
		//27: 5f                      pop    rdi
		//28: c3                      ret

		seg->Write8(0x48);
		seg->Write32(0x50245c8b);
		seg->Write8(0x48);
		seg->Write32(0x58246c8b);
		seg->Write8(0x48);
		seg->Write32(0x6024748b);
		seg->Write16(0x8348);
		seg->Write16(0x40c4);
		seg->Write8(0x5f);
		seg->Write8(0xc3);
	}

	size_t Jitc::EpilogSize()
	{
		return 21;
	}

}
