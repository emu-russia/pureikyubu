// The module handles ALU / multiplier operations and flag setting, as well as other auxiliary operations.

#include "pch.h"

namespace DSP
{

	int64_t DspCore::SignExtend16(int16_t a)
	{
		int64_t res = a;
		if (res & 0x8000)
		{
			res |= 0xffff'ffff'ffff'0000;
		}
		return res;
	}

	int64_t DspCore::SignExtend32(int32_t a)
	{
		int64_t res = a;
		if (res & 0x8000'0000)
		{
			res |= 0xffff'ffff'0000'0000;
		}
		return res;
	}

	int64_t DspCore::SignExtend40(int64_t a)
	{
		if (a & 0x80'0000'0000)
		{
			a |= 0xffff'ff00'0000'0000;
		}
		else
		{
			a &= 0x0000'00ff'ffff'ffff;
		}
		return a;
	}

	// The current multiplication result (product) is stored as a set of "temporary" values (prod.h, prod.m1, prod.m2, prod.l).
	// The exact algorithm of the multiplier is unknown, but you can guess that these temporary results are collected from partial 
	// sums of multiplications between the upper and lower halves of the 16-bit operands.

	void DspCore::PackProd(DspProduct& prod)
	{
		uint64_t hi = (uint64_t)prod.h << 32;
		uint64_t mid = ((uint64_t)prod.m1 + (uint64_t)prod.m2) << 16;
		uint64_t low = prod.l;
		uint64_t res = hi + mid + low;
		res &= 0xff'ffff'ffff;
		prod.bitsPacked = res;
	}

	void DspCore::UnpackProd(DspProduct& prod)
	{
		prod.h = (prod.bitsPacked >> 32) & 0xff;
		prod.m1 = (prod.bitsPacked >> 16) & 0xffff;
		prod.m2 = 0;
		prod.l = prod.bitsPacked & 0xffff;
	}

	// Treat operands as signed 16-bit numbers and produce signed multiply product.

	DspProduct DspCore::Muls(int16_t a, int16_t b, bool scale)
	{
		DspProduct prod;
#if 0
		// P = A x B= (AH-AL) x (BH-BL) = AH x BH+AH x BL + AL x BH+ AL x BL 

		int32_t u = ((int32_t)(int16_t)(a & 0xff00)) * ((int32_t)(int16_t)(b & 0xff00));
		int32_t c1 = ((int32_t)(int16_t)(a & 0xff00)) * ((int32_t)(b & 0xff));
		int32_t c2 = ((int32_t)(a & 0xff)) * ((int32_t)(int16_t)(b & 0xff00));
		int32_t l = ((int32_t)(a & 0xff)) * ((int32_t)(b & 0xff));

		int32_t m = c1 + c2 + l;

		prod.h = ((a ^ b) & 0x8000) ? 0xff : 0;
		prod.m1 = u >> 16;
		prod.m2 = m >> 16;
		prod.l = m & 0xffff;
#else
		prod.bitsPacked = (int64_t)((int64_t)(int32_t)a * (int64_t)(int32_t)b);
		if (scale)
			prod.bitsPacked <<= 1;
		UnpackProd(prod);
#endif

		return prod;
	}

	// Treat operands as unsigned 16-bit numbers and produce unsigned multiply product.

	DspProduct DspCore::Mulu(uint16_t a, uint16_t b, bool scale)
	{
		DspProduct prod;
#if 0
		// P = A x B = (AH-AL) x (BH-BL) = AHxBH + AHxBL + ALxBH + ALxBL

		uint32_t u = ((uint32_t)a & 0xff00) * ((uint32_t)b & 0xff00);
		uint32_t c1 = ((uint32_t)a & 0xff00) * ((uint32_t)b & 0xff);
		uint32_t c2 = ((uint32_t)a & 0xff) * ((uint32_t)b & 0xff00);
		uint32_t l = ((uint32_t)a & 0xff) * ((uint32_t)b & 0xff);

		uint32_t m = c1 + c2 + l;

		prod.h = 0;
		prod.m1 = u >> 16;
		prod.m2 = m >> 16;
		prod.l = m & 0xffff;
#else
		prod.bitsPacked = (uint64_t)((uint64_t)(uint32_t)a * (uint64_t)(uint32_t)b);
		if (scale)
			prod.bitsPacked <<= 1;
		UnpackProd(prod);
#endif
		return prod;
	}

	// Treat operand `a` as unsigned 16-bit numbers and operand `b` as signed 16-bit number and produce signed multiply product.

	DspProduct DspCore::Mulus(uint16_t a, int16_t b, bool scale)
	{
		DspProduct prod;
		prod.bitsPacked = (int64_t)((int64_t)(int32_t)(uint32_t)a * (int64_t)(int32_t)b);
		if (scale)
			prod.bitsPacked <<= 1;
		UnpackProd(prod);
		return prod;
	}

	// Circular addressing logic

	uint16_t DspCore::CircularAddress(uint16_t r, uint16_t l, int16_t m)
	{
		if (m == 0 || l == 0)
		{
			return r;
		}

		if (l == 0xffff)
		{
			return (uint16_t)((int16_t)r + m);
		}
		else
		{
			int16_t abs_m = m > 0 ? m : -m;
			int16_t mm = abs_m % (l + 1);
			uint16_t base = (r / (l + 1)) * (l + 1);
			uint16_t next = 0;
			uint32_t sum = 0;

			if (m > 0)
			{
				sum = (uint32_t)((uint32_t)r + mm);
			}
			else
			{
				sum = (uint32_t)((uint32_t)r + l + 1 - mm);
			}

			next = base + (uint16_t)(sum % (l + 1));

			return next;
		}
	}

	void DspCore::ArAdvance(int r, int16_t step)
	{
		regs.r[r] = CircularAddress(regs.r[r], regs.l[r], step);
	}

	#define bit(n,b) (((n) >> (b)) & 1)

	void DspCore::ModifyFlags(uint64_t d, uint64_t s, uint64_t r, CFlagRules cf, VFlagRules vf, ZFlagRules zf, NFlagRules nf, EFlagRules ef, UFlagRules uf)
	{
		// Carry

		switch (cf)
		{
			case CFlagRules::Zero:
				regs.psr.c = 0;
				break;
			case CFlagRules::C1:
				regs.psr.c = (bit(d, 39) & bit(s, 39)) | (~bit(r, 39) & (bit(d, 39) | bit(s, 39)));
				break;
			case CFlagRules::C2:
				regs.psr.c = (bit(d, 39) & ~bit(s, 39)) | (~bit(r, 39) & (bit(d, 39) | ~bit(s, 39)));
				break;
			case CFlagRules::C3:
				regs.psr.c = (bit(d, 39) ^ bit(s, 15)) != 0 ? (bit(d, 39) & bit(s, 15)) | (~bit(r, 39) & (bit(d, 39) | bit(s, 15))) : (bit(d, 39) & ~bit(s, 15)) | (~bit(r, 39) & (bit(d, 39) | ~bit(s, 15)));
				break;
			case CFlagRules::C4:
				regs.psr.c = ~bit(d, 39) & ~bit(r, 39);
				break;
			case CFlagRules::C5:
				regs.psr.c = (bit(d, 39) ^ bit(s, 31)) == 0 ? (bit(d, 39) & bit(s, 39)) | (~bit(r, 39) & (bit(d, 39) | bit(s, 39))) : (bit(d, 39) & ~bit(s, 39)) | (~bit(r, 39) & (bit(d, 39) | ~bit(s, 39)));
				break;
			case CFlagRules::C6:
				regs.psr.c = bit(d, 39) & ~bit(r, 39);
				break;
			case CFlagRules::C7:
				regs.psr.c = bit(d, 39) & ~bit(r, 39);		// Prod
				break;
			case CFlagRules::C8:
				regs.psr.c = (bit(d, 39) & bit(s, 39)) | (~bit(r, 39) & (bit(d, 39) | bit(s, 39)));	// Prod
				break;
		}

		// Overflow

		switch (vf)
		{
			case VFlagRules::Zero:
				regs.psr.v = 0;
				break;
			case VFlagRules::V1:
				regs.psr.v = (bit(d, 39) & bit(s, 39) & ~bit(r, 39)) | (~bit(d, 39) & ~bit(s, 39) & bit(r, 39));
				break;
			case VFlagRules::V2:
				regs.psr.v = (bit(d, 39) & ~bit(s, 39) & ~bit(r, 39)) | (~bit(d, 39) & bit(s, 39) & bit(r, 39));
				break;
			case VFlagRules::V3:
				regs.psr.v = bit(d, 39) & bit(r, 39);
				break;
			case VFlagRules::V4:
				regs.psr.v = ~bit(d, 39) & bit(r, 39);
				break;
			case VFlagRules::V5:
				regs.psr.v = bit(r, 39);
				break;
			case VFlagRules::V6:
				regs.psr.v = ~bit(d, 39) & bit(r, 39);	// Prod
				break;
			case VFlagRules::V7:
				regs.psr.v = (bit(d, 39) & bit(s, 39) & ~bit(r, 39)) | (~bit(d, 39) & ~bit(s, 39) & bit(r, 39));	// Prod
				break;
			case VFlagRules::V8:
				regs.psr.v = bit(d, 39) & ~bit(r, 39);
				break;
		}

		// Sticky overflow

		if (regs.psr.v != 0)
		{
			regs.psr.sv = 1;
		}

		// Zero

		switch (zf)
		{
			case ZFlagRules::Z1:
				regs.psr.z = r == 0;
				break;
			case ZFlagRules::Z2:
				regs.psr.z = (r & 0xffff'0000) == 0;
				break;
			case ZFlagRules::Z3:
				regs.psr.z = (r & 0xff'ffff'ffff) == 0;
				break;
		}

		// Negative

		switch (nf)
		{
			case NFlagRules::N1:
				regs.psr.n = bit(r, 39);
				break;
			case NFlagRules::N2:
				regs.psr.n = bit(r, 31);
				break;
		}

		// Extension (above s32)

		switch (ef)
		{
			case EFlagRules::E1:
			{
				uint64_t ext = (r >> 31) & 0x1ff;
				regs.psr.e = !(ext == 0b0'0000'0000 || ext == 0b1'1111'1111);
				break;
			}
		}

		// Unnormalization

		switch (uf)
		{
			case UFlagRules::U1:
				regs.psr.u = ~(bit(r, 31) ^ bit(r, 30));
				break;
		}
	}

}
