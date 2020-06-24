// Integer Instructions
#include "../pch.h"

namespace Gekko
{

	void Jitc::Add(AnalyzeInfo* info, CodeSegment* seg)
	{
		// mov eax, [rsi + 4*p1]
		// add eax, [rsi + 4*p2]
		// mov [rsi + 4*p0], eax

		//0:  8b 46 10                mov    eax,DWORD PTR [rsi+0x10]
		//3:  03 46 20                add    eax,DWORD PTR [rsi+0x20]
		//6:  89 46 30                mov    DWORD PTR [rsi+0x30],eax
		
		seg->Write16(0x468b);
		seg->Write8(info->paramBits[1] << 2);
		
		seg->Write16(0x4603);
		seg->Write8(info->paramBits[2] << 2);
		
		seg->Write16(0x4689);
		seg->Write8(info->paramBits[0] << 2);
		
		AddPc(seg);
		CallTick(seg);
	}

	void Jitc::Addd(AnalyzeInfo* info, CodeSegment* seg)
	{
	}

	void Jitc::Addo(AnalyzeInfo* info, CodeSegment* seg)
	{
	}

	void Jitc::Addod(AnalyzeInfo* info, CodeSegment* seg)
	{
	}

}
