// Branch Instructions
#include "../pch.h"

namespace Gekko
{
	void Jitc::Branch(AnalyzeInfo* info, CodeSegment* seg, bool link)
	{
		// Address is prepared in advance by the analyzer.

		// mov rsi, offset Gekko::PC
		// if (link)
		//		mov rax, offset Gekko::LR
		//		mov ecx, dword [rsi]
		//		add ecx, 4
		//		mov dword ptr [rax], ecx
		// mov dword ptr [rsi], Imm::Address

		//0:  48 be 88 77 66 55 44    movabs rsi,0x1122334455667788
		//7:  33 22 11
		//a:  48 b8 88 77 66 55 44    movabs rax,0x1122334455667788
		//11: 33 22 11
		//14: 8b 0e                   mov    ecx,DWORD PTR [rsi]
		//16: 83 c1 04                add    ecx,0x4
		//19: 89 08                   mov    DWORD PTR [rax],ecx
		//1b: c7 06 dd cc bb aa       mov    DWORD PTR [rsi],0xaabbccdd

		seg->Write16(0xbe48);
		seg->Write64((uint64_t)&core->regs.pc);

		if (link)
		{
			seg->Write16(0xb848);
			seg->Write64((uint64_t)&core->regs.spr[(int)SPR::LR]);
			seg->Write16(0x0e8b);
			seg->Write8(0x83);
			seg->Write16(0x04c1);
			seg->Write16(0x0889);
		}

		seg->Write16(0x06c7);
		seg->Write32(info->Imm.Address);

		CallTick(seg);
	}

}
