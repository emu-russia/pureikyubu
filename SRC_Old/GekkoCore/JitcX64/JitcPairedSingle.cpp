// Paired Single Instructions
#include "../pch.h"

// HID2[PSE] verification is not performed because it is not used by programs.

namespace Gekko
{
	#define PS0(n)  (core->regs.fpr[n].dbl)
	#define PS1(n)  (core->regs.ps1[n].dbl)
	
	void Jitc::PsAdd(DecoderInfo* info, CodeSegment* seg)
	{
		// mov  rcx, offset ps0
		// mov  rdx, offset ps1

		// movsd xmm0, qword ptr [rcx + 8 * ra]
		// addsd xmm0, qword ptr [rcx + 8 * rb]
		// movsd qword ptr [rcx + 8 * rd], xmm0

		// movsd xmm0, qword ptr [rdx + 8 * ra]
		// addsd xmm0, qword ptr [rdx + 8 * rb]
		// movsd qword ptr [rdx + 8 * rd], xmm0

		seg->Write16(0xb948);
		seg->Write64((uint64_t)&PS0(0));
		seg->Write16(0xba48);
		seg->Write64((uint64_t)&PS1(0));

		seg->Write32(0x81100ff2);
		seg->Write32(info->paramBits[1] << 3);
		seg->Write32(0x81580ff2);
		seg->Write32(info->paramBits[2] << 3);
		seg->Write32(0x81110ff2);
		seg->Write32(info->paramBits[0] << 3);

		seg->Write32(0x82100ff2);
		seg->Write32(info->paramBits[1] << 3);
		seg->Write32(0x82580ff2);
		seg->Write32(info->paramBits[2] << 3);
		seg->Write32(0x82110ff2);
		seg->Write32(info->paramBits[0] << 3);

		AddPc(seg);
		CallTick(seg);
	}

	void Jitc::PsSub(DecoderInfo* info, CodeSegment* seg)
	{
		// mov  rcx, offset ps0
		// mov  rdx, offset ps1

		// movsd xmm0, qword ptr [rcx + 8 * ra]
		// subsd xmm0, qword ptr [rcx + 8 * rb]
		// movsd qword ptr [rcx + 8 * rd], xmm0

		// movsd xmm0, qword ptr [rdx + 8 * ra]
		// subsd xmm0, qword ptr [rdx + 8 * rb]
		// movsd qword ptr [rdx + 8 * rd], xmm0

		seg->Write16(0xb948);
		seg->Write64((uint64_t)&PS0(0));
		seg->Write16(0xba48);
		seg->Write64((uint64_t)&PS1(0));

		seg->Write32(0x81100ff2);
		seg->Write32(info->paramBits[1] << 3);
		seg->Write32(0x815c0ff2);
		seg->Write32(info->paramBits[2] << 3);
		seg->Write32(0x81110ff2);
		seg->Write32(info->paramBits[0] << 3);

		seg->Write32(0x82100ff2);
		seg->Write32(info->paramBits[1] << 3);
		seg->Write32(0x825c0ff2);
		seg->Write32(info->paramBits[2] << 3);
		seg->Write32(0x82110ff2);
		seg->Write32(info->paramBits[0] << 3);

		AddPc(seg);
		CallTick(seg);
	}

	void Jitc::PsMerge00(DecoderInfo* info, CodeSegment* seg)
	{
		// mov  rcx, offset ps0
		// mov  rdx, offset ps1
		// mov  r8, qword ptr [rcx + 8*ra]
		// mov  r9, qword ptr [rcx + 8*rb]
		// mov  qword ptr [rcx + 8*rd], r8
		// mov  qword ptr [rdx + 8*rd], r9

		//0:  48 b9 88 77 66 55 44    movabs rcx,0x1122334455667788
		//7:  33 22 11
		//a:  48 ba 88 77 66 55 44    movabs rdx,0x1122334455667788
		//11: 33 22 11
		//14: 4c 8b 81 00 01 00 00    mov    r8,QWORD PTR [rcx+0x100]
		//1b: 4c 8b 89 00 02 00 00    mov    r9,QWORD PTR [rcx+0x200]
		//22: 4c 89 81 00 03 00 00    mov    QWORD PTR [rcx+0x300],r8
		//29: 4c 89 8a 00 04 00 00    mov    QWORD PTR [rdx+0x400],r9

		seg->Write16(0xb948);
		seg->Write64((uint64_t)&PS0(0));
		seg->Write16(0xba48);
		seg->Write64((uint64_t)&PS1(0));

		seg->Write8(0x4c);
		seg->Write16(0x818b);
		seg->Write32(info->paramBits[1] << 3);
		seg->Write8(0x4c);
		seg->Write16(0x898b);
		seg->Write32(info->paramBits[2] << 3);

		seg->Write8(0x4c);
		seg->Write16(0x8189);
		seg->Write32(info->paramBits[0] << 3);
		seg->Write8(0x4c);
		seg->Write16(0x8a89);
		seg->Write32(info->paramBits[0] << 3);

		AddPc(seg);
		CallTick(seg);
	}

	void Jitc::PsMerge01(DecoderInfo* info, CodeSegment* seg)
	{
		// mov  rcx, offset ps0
		// mov  rdx, offset ps1
		// mov  r8, qword ptr [rcx + 8*ra]
		// mov  r9, qword ptr [rdx + 8*rb]
		// mov  qword ptr [rcx + 8*rd], r8
		// mov  qword ptr [rdx + 8*rd], r9

		seg->Write16(0xb948);
		seg->Write64((uint64_t)&PS0(0));
		seg->Write16(0xba48);
		seg->Write64((uint64_t)&PS1(0));

		seg->Write8(0x4c);
		seg->Write16(0x818b);
		seg->Write32(info->paramBits[1] << 3);
		seg->Write8(0x4c);
		seg->Write16(0x8a8b);
		seg->Write32(info->paramBits[2] << 3);

		seg->Write8(0x4c);
		seg->Write16(0x8189);
		seg->Write32(info->paramBits[0] << 3);
		seg->Write8(0x4c);
		seg->Write16(0x8a89);
		seg->Write32(info->paramBits[0] << 3);

		AddPc(seg);
		CallTick(seg);
	}

	void Jitc::PsMerge10(DecoderInfo* info, CodeSegment* seg)
	{
		// mov  rcx, offset ps0
		// mov  rdx, offset ps1
		// mov  r8, qword ptr [rdx + 8*ra]
		// mov  r9, qword ptr [rcx + 8*rb]
		// mov  qword ptr [rcx + 8*rd], r8
		// mov  qword ptr [rdx + 8*rd], r9

		seg->Write16(0xb948);
		seg->Write64((uint64_t)&PS0(0));
		seg->Write16(0xba48);
		seg->Write64((uint64_t)&PS1(0));

		seg->Write8(0x4c);
		seg->Write16(0x828b);
		seg->Write32(info->paramBits[1] << 3);
		seg->Write8(0x4c);
		seg->Write16(0x898b);
		seg->Write32(info->paramBits[2] << 3);

		seg->Write8(0x4c);
		seg->Write16(0x8189);
		seg->Write32(info->paramBits[0] << 3);
		seg->Write8(0x4c);
		seg->Write16(0x8a89);
		seg->Write32(info->paramBits[0] << 3);

		AddPc(seg);
		CallTick(seg);
	}

	void Jitc::PsMerge11(DecoderInfo* info, CodeSegment* seg)
	{
		// mov  rcx, offset ps0
		// mov  rdx, offset ps1
		// mov  r8, qword ptr [rdx + 8*ra]
		// mov  r9, qword ptr [rdx + 8*rb]
		// mov  qword ptr [rcx + 8*rd], r8
		// mov  qword ptr [rdx + 8*rd], r9

		seg->Write16(0xb948);
		seg->Write64((uint64_t)&PS0(0));
		seg->Write16(0xba48);
		seg->Write64((uint64_t)&PS1(0));

		seg->Write8(0x4c);
		seg->Write16(0x828b);
		seg->Write32(info->paramBits[1] << 3);
		seg->Write8(0x4c);
		seg->Write16(0x8a8b);
		seg->Write32(info->paramBits[2] << 3);

		seg->Write8(0x4c);
		seg->Write16(0x8189);
		seg->Write32(info->paramBits[0] << 3);
		seg->Write8(0x4c);
		seg->Write16(0x8a89);
		seg->Write32(info->paramBits[0] << 3);

		AddPc(seg);
		CallTick(seg);
	}

}
