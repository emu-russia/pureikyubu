// Integer Instructions
#include "../pch.h"

namespace Gekko
{

	void Jitc::Add(AnalyzeInfo* info, CodeSegment* seg)
	{
		// mov rsi, offset Gekko->regs.gpr
		// mov eax, [esi + 4*p1]
		// add eax, [esi + 4*p2]
		// mov [esi + 4*p0], eax

		//0:  48 be 88 77 66 55 44    movabs rsi,0x1122334455667788
		//7:  33 22 11
		//a:  67 8b 46 7c             mov    eax,DWORD PTR [esi+0x7c]
		//e:  67 03 46 20             add    eax,DWORD PTR [esi+0x20]
		//12: 67 89 46 30             mov    DWORD PTR [esi+0x30],eax

		seg->Write16(0xbe48);
		seg->Write64((uint64_t)core->regs.gpr);
		
		seg->Write8(0x67);
		seg->Write16(0x468b);
		seg->Write8(info->paramBits[1] << 2);
		
		seg->Write8(0x67);
		seg->Write16(0x4603);
		seg->Write8(info->paramBits[2] << 2);
		
		seg->Write8(0x67);
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
