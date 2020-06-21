// Paired Single Load and Store Instructions
#include "pch.h"

// Under normal conditions, GQR registers are set once and for a long time (__OSPSInit, OSInitFastCast)
// We use this fact here and do not get the Type/Scale value in runtime, but in compile time.

// When the GQR values change, the recompiler is invalidated.

namespace Gekko
{
	#define LD_SCALE(n) ((core->regs.spr[(int)SPR::GQRs + n] >> 24) & 0x3f)
	#define LD_TYPE(n)  (GEKKO_QUANT_TYPE)((core->regs.spr[(int)SPR::GQRs + n] >> 16) & 7)
	#define ST_SCALE(n) ((core->regs.spr[(int)SPR::GQRs + n] >>  8) & 0x3f)
	#define ST_TYPE(n)  (GEKKO_QUANT_TYPE)((core->regs.spr[(int)SPR::GQRs + n]      ) & 7)

	#define PS0(n)  (core->regs.fpr[n].dbl)
	#define PS1(n)  (core->regs.ps1[n].dbl)

	void Jitc::Dequantize(CodeSegment* seg, void *psReg, GEKKO_QUANT_TYPE type, uint8_t scale, bool secondReg)
	{
		// EA comes as ecx

		if (secondReg)
		{
			// add ecx, typeSize

			switch (type)
			{
				case GEKKO_QUANT_TYPE::U8:
				case GEKKO_QUANT_TYPE::S8:
					// 0:  ff c1                   inc    ecx
					seg->Write16(0xc1ff);
					break;

				case GEKKO_QUANT_TYPE::U16:
				case GEKKO_QUANT_TYPE::S16:
					// 0 : 83 c1 02                add    ecx, 0x2
					seg->Write8(0x83);
					seg->Write16(0x02c1);
					break;

				case GEKKO_QUANT_TYPE::SINGLE_FLOAT:
					// 0:  83 c1 04                add    ecx, 0x4
					seg->Write8(0x83);
					seg->Write16(0x04c1);
					break;

				default:
					break;
			}
		}

		// mov rdx, offset temp
		seg->Write16(0xba48);
		seg->Write64((uint64_t)&DequantizeTemp);

		switch (type)
		{
			case GEKKO_QUANT_TYPE::U8:

				// ReadByte (ecx, &temp);
				
				seg->Write16(0xb848);
				seg->Write64((uint64_t)Jitc::ReadByte);
				seg->Write16(0xd0ff);

				// flt = (float)(uint8_t)temp;

				// mov rdx, offset temp
				// mov ecx, dword ptr[rdx]
				seg->Write16(0xba48);
				seg->Write64((uint64_t)&DequantizeTemp);
				seg->Write16(0x0a8b);

				// 00045	0f b6 c1	 movzx	 eax, cl
				// 00048	66 0f 6e c0	 movd	 xmm0, eax
				// 0004c	0f 5b c0	 cvtdq2ps xmm0, xmm0

				seg->Write32(0x66c1b60f);
				seg->Write32(0x0fc06e0f);
				seg->Write16(0xc05b);

				break;

			case GEKKO_QUANT_TYPE::S8:

				// ReadByte (ecx, &temp);

				seg->Write16(0xb848);
				seg->Write64((uint64_t)Jitc::ReadByte);
				seg->Write16(0xd0ff);

				// flt = (float)(int8_t)temp;

				// mov rdx, offset temp
				// mov ecx, dword ptr[rdx]
				seg->Write16(0xba48);
				seg->Write64((uint64_t)&DequantizeTemp);
				seg->Write16(0x0a8b);

				// 0003b	0f be c1	 movsx	 eax, cl
				// 00048	66 0f 6e c0	 movd	 xmm0, eax
				// 0004c	0f 5b c0	 cvtdq2ps xmm0, xmm0

				seg->Write32(0x66c1be0f);
				seg->Write32(0x0fc06e0f);
				seg->Write16(0xc05b);

				break;

			case GEKKO_QUANT_TYPE::U16:

				// ReadHalf (ecx, &temp);

				seg->Write16(0xb848);
				seg->Write64((uint64_t)Jitc::ReadHalf);
				seg->Write16(0xd0ff);

				// flt = (float)(uint16_t)temp;

				// mov rdx, offset temp
				// mov ecx, dword ptr[rdx]
				seg->Write16(0xba48);
				seg->Write64((uint64_t)&DequantizeTemp);
				seg->Write16(0x0a8b);

				// 00040	0f b7 c1	 movzx	 eax, cx
				// 00048	66 0f 6e c0	 movd	 xmm0, eax
				// 0004c	0f 5b c0	 cvtdq2ps xmm0, xmm0

				seg->Write32(0x66c1b70f);
				seg->Write32(0x0fc06e0f);
				seg->Write16(0xc05b);

				break;

			case GEKKO_QUANT_TYPE::S16:

				// ReadHalf (ecx, &temp);

				seg->Write16(0xb848);
				seg->Write64((uint64_t)Jitc::ReadHalf);
				seg->Write16(0xd0ff);

				// flt = (float)(int16_t)temp;

				// 0002c	0f bf c1	 movsx	 eax, cx
				// 00048	66 0f 6e c0	 movd	 xmm0, eax
				// 0004c	0f 5b c0	 cvtdq2ps xmm0, xmm0

				seg->Write32(0x66c1bf0f);
				seg->Write32(0x0fc06e0f);
				seg->Write16(0xc05b);

				break;

			case GEKKO_QUANT_TYPE::SINGLE_FLOAT:

				// ReadWord (ecx, &temp);

				seg->Write16(0xb848);
				seg->Write64((uint64_t)Jitc::ReadWord);
				seg->Write16(0xd0ff);

				// mov rdx, offset temp
				seg->Write16(0xba48);
				seg->Write64((uint64_t)&DequantizeTemp);

				// 0:  f3 0f 10 02             movss  xmm0,DWORD PTR [rdx]
				seg->Write32(0x02100ff3);

				break;

			default:
				DBHalt("Jitc::Dequantize: Unknown type %i", type);
				break;
		}

		// flt = flt * Gekko->interp->ldScale[scale]

		// mov rdx, offset ldScale[scale]
		// 0:  f3 0f 59 02             mulss  xmm0, DWORD PTR[rdx]

		seg->Write16(0xba48);
		seg->Write64((uint64_t)&core->interp->ldScale[scale]);
		seg->Write32(0x02590ff3);

		// *psReg = flt

		//0:  0f 57 c9                xorps  xmm1,xmm1
		//3:  f3 0f 5a c8             cvtss2sd xmm1,xmm0
		//7:  48 ba 88 77 66 55 44    movabs rdx,0x1122334455667788
		//e:  33 22 11
		//11: f2 0f 11 0a             movsd  QWORD PTR [rdx],xmm1

		seg->Write16(0x570f);
		seg->Write8(0xc9);
		seg->Write32(0xc85a0ff3);
		seg->Write16(0xba48);
		seg->Write64((uint64_t)psReg);
		seg->Write32(0x0a110ff2);
	}

	void Jitc::PSQLoad(AnalyzeInfo* info, CodeSegment* seg)
	{
		// EA

		uint32_t ea = info->Imm.Unsigned;
		if (ea & 0x800) ea |= 0xfffff000;

		// if (ea)
		//		mov ecx, ea
		// else
		//		xor ecx, ecx
		// if (ra)
		//		add  ecx, [rsi + 4*ra]

		//0:  b9 dd cc bb aa          mov    ecx,0xaabbccdd
		//0:  31 c9					  xor ecx, ecx
		//5:  03 4e 20                add    ecx,DWORD PTR [rsi+0x20]

		if (ea)
		{
			seg->Write8(0xb9);
			seg->Write32(ea);
		}
		else
		{
			seg->Write16(0xc931);
		}
		if (info->paramBits[1] != 0)
		{
			seg->Write16(0x4e03);
			seg->Write8(info->paramBits[1] << 2);
		}

		// Type/Scale

		uint8_t scale = (uint8_t)LD_SCALE(info->paramBits[3]);
		GEKKO_QUANT_TYPE type = LD_TYPE(info->paramBits[3]);

		Dequantize(seg, &PS0(info->paramBits[0]), type, scale, false);

		if (info->paramBits[2])		// W
		{
			// mov  rdx, offset ps1(n)
			// mov  rcx, 0x3ff00000'00000000
			// mov	qword ptr [rdx], rcx

			//0:  48 ba 88 77 66 55 44    movabs rdx,0x1122334455667788
			//7:  33 22 11
			//a:  48 b9 00 00 00 00 00    movabs rcx,0x3ff0000000000000
			//11: 00 f0 3f
			//14: 48 89 0a                mov    QWORD PTR [rdx],rcx

			seg->Write16(0xba48);
			seg->Write64((uint64_t)&PS1(info->paramBits[0]));
			seg->Write16(0xb948);
			seg->Write64(0x3ff00000'00000000);
			seg->Write8(0x48);
			seg->Write16(0x0a89);
		}
		else
		{
			// if (ea)
			//		mov ecx, ea
			// else
			//		xor ecx, ecx
			// if (ra)
			//		add  ecx, [rsi + 4*ra]

			if (ea)
			{
				seg->Write8(0xb9);
				seg->Write32(ea);
			}
			else
			{
				seg->Write16(0xc931);
			}
			if (info->paramBits[1] != 0)
			{
				seg->Write16(0x4e03);
				seg->Write8(info->paramBits[1] << 2);
			}

			Dequantize(seg, &PS1(info->paramBits[0]), type, scale, true);
		}

		AddPc(seg);
		CallTick(seg);
	}

}
