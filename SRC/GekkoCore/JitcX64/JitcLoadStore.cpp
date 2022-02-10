// Integer Load and Store Instructions
#include "../pch.h"

namespace Gekko
{
	void Jitc::LoadImm(DecoderInfo* info, CodeSegment* seg, LoadDelegate loadProc)
	{
		// mov  ecx, SIMM
		// if (RA)
		//		add		ecx, dword ptr [rsi + 4 *ra]
		// lea rdx, [rsi + 4 * rd]
		// mov rax, Jitc::ReadByte
		// call rax
		// mov rax, offset GekkoCore->exception
		// movzx  ecx, byte ptr [rax]
		// test	cl, cl
		// je	AddPc
		// Epilog()
		// AddPc: AddPc()

		//a:  b9 dd cc bb aa          mov    ecx,0xaabbccdd
				//f:  03 4e 10                add    ecx,DWORD PTR [rsi+0x10]
		//12: 48 8d 56 20             lea    rdx,[rsi+0x20]
		//16: 48 b8 88 77 66 55 44    movabs rax,0x1122334455667788
		//1d: 33 22 11
		//20: ff d0                   call   rax
		//22: 48 b8 88 77 66 55 44    movabs rax,0x1122334455667788
		//29: 33 22 11
		//2c: 0f b6 08                movzx  ecx,BYTE PTR [rax]
		//2f: 84 c9                   test   cl,cl
		//31: 74 01                   je     34 <AddPc>
		//33: c3                      Epilog()

		seg->Write8(0xb9);
		seg->Write32((uint32_t)(int32_t)info->Imm.Signed);

		if (info->paramBits[1] != 0)	// RA != 0
		{
			seg->Write16(0x4e03);
			seg->Write8(info->paramBits[1] << 2);
		}

		seg->Write8(0x48);
		seg->Write16(0x568d);
		seg->Write8(info->paramBits[0] << 2);

		seg->Write16(0xb848);
		seg->Write64((uint64_t)loadProc);
		seg->Write16(0xd0ff);

		// if (exception) return;

		seg->Write16(0xb848);
		seg->Write64((uint64_t)&core->exception);
		seg->Write8(0x0f);
		seg->Write16(0x08b6);
		seg->Write16(0xc984);
		seg->Write8(0x74);
		seg->Write8((uint8_t)EpilogSize());
		Epilog(seg);

		AddPc(seg);
		CallTick(seg);
	}
}
