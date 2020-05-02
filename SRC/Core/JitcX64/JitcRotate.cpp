// Integer Rotate Instructions
#include "../pch.h"

namespace Gekko
{

	// rlwinm rA,rS,SH,MB,ME 
	void Jitc::Rlwinm(AnalyzeInfo* info, CodeSegment* seg)
	{
		uint32_t mask = core->interp->GetRotMask(info->paramBits[3], info->paramBits[4]);

		// mov rsi, offset gpr
		// xor rax, rax
		// mov eax, [rsi + 4*rS]
		// if (cl != 0 )
		//		mov cl, SH
		//		rol eax, cl
		// and eax, 0xaabbccdd
		// mov [rsi + 4*rA], eax

		//0:  48 be 88 77 66 55 44    movabs rsi,0x1122334455667788
		//7:  33 22 11
		//a:  48 31 c0                xor    rax,rax
		//d:  8b 46 10                mov    eax,DWORD PTR [rsi+0x10]
		//10: b1 1f                   mov    cl,0x1f
		//12: d3 c0                   rol    eax,cl
		//14: 25 dd cc bb aa          and    eax,0xaabbccdd
		//19: 89 46 20                mov    DWORD PTR [rsi+0x20],eax

		seg->Write16(0xbe48);
		seg->Write64((uint64_t)core->regs.gpr);
		seg->Write8(0x48);
		seg->Write16(0xc031);
		seg->Write16(0x468b);
		seg->Write8(info->paramBits[1] << 2);

		if (info->paramBits[2] != 0)
		{
			seg->Write8(0xb1);
			seg->Write8(info->paramBits[2]);
			seg->Write16(0xc0d3);
		}

		seg->Write8(0x25);
		seg->Write32(mask);

		seg->Write16(0x4689);
		seg->Write8(info->paramBits[0] << 2);

		AddPc(seg);
		CallTick(seg);
	}

}
