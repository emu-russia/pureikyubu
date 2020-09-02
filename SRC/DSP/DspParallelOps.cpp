// All parallel instructions except multiplication.
#include "pch.h"

using namespace Debug;

namespace DSP
{

	void DspInterpreter::p_add(AnalyzeInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::x0:
				s = DspCore::SignExtend16(core->regs.x.l) << 16;
				break;
			case DspParameter::y0:
				s = DspCore::SignExtend16(core->regs.y.l) << 16;
				break;
			case DspParameter::x1:
				s = DspCore::SignExtend16(core->regs.x.h) << 16;
				break;
			case DspParameter::y1:
				s = DspCore::SignExtend16(core->regs.y.h) << 16;
				break;
			case DspParameter::x:
				s = DspCore::SignExtend32(core->regs.x.bits);
				break;
			case DspParameter::y:
				s = DspCore::SignExtend32(core->regs.y.bits);
				break;
			case DspParameter::a:
				s = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				s = DspCore::SignExtend40(core->regs.b.bits);
				break;
			case DspParameter::prod:
			{
				DspProduct prod;
				core->PackProd(prod);
				s = DspCore::SignExtend40(prod.bitsPacked);
				break;
			}
		}

		r = d + s;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C1, VFlagRules::V1, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_addl(AnalyzeInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::x0:
				s = core->regs.x.l;
				break;
			case DspParameter::y0:
				s = core->regs.y.l;
				break;
		}

		r = d + s;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C6, VFlagRules::V4, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_sub(AnalyzeInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::x0:
				s = DspCore::SignExtend16(core->regs.x.l) << 16;
				break;
			case DspParameter::y0:
				s = DspCore::SignExtend16(core->regs.y.l) << 16;
				break;
			case DspParameter::x1:
				s = DspCore::SignExtend16(core->regs.x.h) << 16;
				break;
			case DspParameter::y1:
				s = DspCore::SignExtend16(core->regs.y.h) << 16;
				break;
			case DspParameter::x:
				s = DspCore::SignExtend32(core->regs.x.bits);
				break;
			case DspParameter::y:
				s = DspCore::SignExtend32(core->regs.y.bits);
				break;
			case DspParameter::a:
				s = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				s = DspCore::SignExtend40(core->regs.b.bits);
				break;
			case DspParameter::prod:
			{
				DspProduct prod;
				core->PackProd(prod);
				s = DspCore::SignExtend40(prod.bitsPacked);
				break;
			}
		}

		r = d - s;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C2, VFlagRules::V2, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_amv(AnalyzeInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[1])
		{
			case DspParameter::x0:
				s = DspCore::SignExtend16(core->regs.x.l) << 16;
				break;
			case DspParameter::y0:
				s = DspCore::SignExtend16(core->regs.y.l) << 16;
				break;
			case DspParameter::x1:
				s = DspCore::SignExtend16(core->regs.x.h) << 16;
				break;
			case DspParameter::y1:
				s = DspCore::SignExtend16(core->regs.y.h) << 16;
				break;
			case DspParameter::x:
				s = DspCore::SignExtend32(core->regs.x.bits);
				break;
			case DspParameter::y:
				s = DspCore::SignExtend32(core->regs.y.bits);
				break;
			case DspParameter::a:
				s = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				s = DspCore::SignExtend40(core->regs.b.bits);
				break;
			case DspParameter::prod:
			{
				DspProduct prod;
				core->PackProd(prod);
				s = DspCore::SignExtend40(prod.bitsPacked);
				break;
			}
		}

		r = d + s;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_cmp(AnalyzeInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::x1:
				s = DspCore::SignExtend16(core->regs.x.h) << 16;
				break;
			case DspParameter::y1:
				s = DspCore::SignExtend16(core->regs.y.h) << 16;
				break;
			case DspParameter::a:
				s = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				s = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		r = d - s;

		core->ModifyFlags(d, s, r, CFlagRules::C2, VFlagRules::V2, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_inc(AnalyzeInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
			case DspParameter::a1:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
			case DspParameter::b1:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[0])
		{
			case DspParameter::a:
			case DspParameter::b:
				s = 1;
				break;
			case DspParameter::a1:
			case DspParameter::b1:
				s = 0x10000;
				break;
		}

		r = d + s;

		switch (info.params[0])
		{
			case DspParameter::a:
			case DspParameter::a1:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
			case DspParameter::b1:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C6, VFlagRules::V4, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_dec(AnalyzeInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
			case DspParameter::a1:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
			case DspParameter::b1:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[0])
		{
			case DspParameter::a:
			case DspParameter::b:
				s = 1;
				break;
			case DspParameter::a1:
			case DspParameter::b1:
				s = 0x10000;
				break;
		}

		r = d - s;

		switch (info.params[0])
		{
			case DspParameter::a:
			case DspParameter::a1:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
			case DspParameter::b1:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C4, VFlagRules::V8, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_abs(AnalyzeInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				s = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				s = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		if ((s & 0x80'0000'0000) == 0)
		{
			r = d + s;
		}
		else
		{
			r = d - s;
		}

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::V5, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_neg(AnalyzeInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		if (info.numParameters == 2)
		{
			switch (info.params[1])
			{
				case DspParameter::prod:
				{
					DspProduct prod;
					core->PackProd(prod);
					s = DspCore::SignExtend40(prod.bitsPacked);
					break;
				}
			}
		}
		else
		{
			switch (info.params[0])
			{
				case DspParameter::a:
					s = DspCore::SignExtend40(core->regs.a.bits);
					break;
				case DspParameter::b:
					s = DspCore::SignExtend40(core->regs.b.bits);
					break;
			}
		}

		r = d - s;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C4, VFlagRules::V3, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_clr(AnalyzeInfo& info)
	{
		switch (info.params[0])
		{
			case DspParameter::a: core->regs.a.bits = 0; break;
			case DspParameter::b: core->regs.b.bits = 0; break;

			case DspParameter::prod:
				core->regs.prod.l = 0x0000;
				core->regs.prod.m1 = 0xfff0;
				core->regs.prod.h = 0x00ff;
				core->regs.prod.m2 = 0x0010;
				break;

			case DspParameter::psr_im: core->regs.psr.im = 0; break;
			case DspParameter::psr_dp: core->regs.psr.dp = 0; break;
			case DspParameter::psr_xl: core->regs.psr.xl = 0; break;
		}
	}

	void DspInterpreter::p_rnd(AnalyzeInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		uint16_t d0 = d & 0xffff;

		if (d0 < 0x8000)
		{
			s = 0x00'0000'0000;
		}
		else if (d0 > 0x8000)
		{
			s = 0x00'0001'0000;
		}
		else 	// == 0x8000
		{
			if ((d & 0x10000) == 0) 		// lsb of d1
			{
				s = 0x00'0000'0000;
			}
			else
			{
				s = 0x00'0001'0000;
			}
		}

		r = d + s;
		r &= ~0xffff;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}
		
		core->ModifyFlags(d, s, r, CFlagRules::C6, VFlagRules::V4, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_rndp(AnalyzeInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		DspProduct prod;
		core->PackProd(prod);
		d = DspCore::SignExtend40(prod.bitsPacked);

		uint16_t d0 = d & 0xffff;

		if (d0 < 0x8000)
		{
			s = 0x00'0000'0000;
		}
		else if (d0 > 0x8000)
		{
			s = 0x00'0001'0000;
		}
		else 	// == 0x8000
		{
			if ((d & 0x10000) == 0) 		// lsb of d1
			{
				s = 0x00'0000'0000;
			}
			else
			{
				s = 0x00'0001'0000;
			}
		}

		r = d + s;
		r &= ~0xffff;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C7, VFlagRules::V6, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_tst(AnalyzeInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;
		
		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				s = 0;
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				s = 0;
				break;
			case DspParameter::x1:
				d = DspCore::SignExtend16(core->regs.x.h) << 16;
				s = 0;
				break;
			case DspParameter::y1:
				d = DspCore::SignExtend16(core->regs.y.h) << 16;
				s = 0;
				break;
			case DspParameter::prod:
			{
				DspProduct prod;
				core->PackProd(prod);
				d = ((uint64_t)prod.h << 32) | ((uint64_t)prod.m1 << 16) | prod.l;
				s = ((uint64_t)prod.m2 << 16);
				break;
			}
		}

		r = d + s;

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_lsl16(AnalyzeInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = core->regs.a.bits & 0x0000'00ff'ffff'ffff;
				break;
			case DspParameter::b:
				d = core->regs.b.bits & 0x0000'00ff'ffff'ffff;
				break;
		}

		r = (uint64_t)d << 16;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}
		
		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_lsr16(AnalyzeInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = core->regs.a.bits & 0x0000'00ff'ffff'ffff;
				break;
			case DspParameter::b:
				d = core->regs.b.bits & 0x0000'00ff'ffff'ffff;
				break;
		}

		r = (uint64_t)d >> 16;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_asr16(AnalyzeInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		r = d >> 16;		// Arithmetic

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_addp(AnalyzeInfo& info)
	{
		int64_t d = 0;
		int64_t s = 0;
		int64_t r = 0;

		switch (info.params[1])
		{
			case DspParameter::x1:
				d = DspCore::SignExtend16(core->regs.x.h) << 16;
				break;
			case DspParameter::y1:
				d = DspCore::SignExtend16(core->regs.y.h) << 16;
				break;
		}

		DspProduct prod;
		core->PackProd(prod);
		s = DspCore::SignExtend32((uint32_t)prod.bitsPacked);

		r = d + s;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C8, VFlagRules::V7, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_set(AnalyzeInfo& info)
	{
		switch (info.params[0])
		{
			case DspParameter::psr_im: core->regs.psr.im = 1; break;
			case DspParameter::psr_dp: core->regs.psr.dp = 1; break;
			case DspParameter::psr_xl: core->regs.psr.xl = 1; break;
		}
	}

	void DspInterpreter::p_not(AnalyzeInfo& info)
	{
		uint16_t d = 0;
		uint16_t s = 0;
		uint16_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a1:
				d = core->regs.a.m;
				break;
			case DspParameter::b1:
				d = core->regs.b.m;
				break;
		}

		r = ~d;

		switch (info.params[0])
		{
			case DspParameter::a1:
				core->regs.a.m = r;
				break;
			case DspParameter::b1:
				core->regs.b.m = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_xor(AnalyzeInfo& info)
	{
		uint16_t d = 0;
		uint16_t s = 0;
		uint16_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a1:
				d = core->regs.a.m;
				break;
			case DspParameter::b1:
				d = core->regs.b.m;
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::a1:
				s = core->regs.a.m;
				break;
			case DspParameter::b1:
				s = core->regs.b.m;
				break;
			case DspParameter::x1:
				s = core->regs.x.h;
				break;
			case DspParameter::y1:
				s = core->regs.y.h;
				break;
		}

		r = d ^ s;

		switch (info.params[0])
		{
			case DspParameter::a1:
				core->regs.a.m = r;
				break;
			case DspParameter::b1:
				core->regs.b.m = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_and(AnalyzeInfo& info)
	{
		uint16_t d = 0;
		uint16_t s = 0;
		uint16_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a1:
				d = core->regs.a.m;
				break;
			case DspParameter::b1:
				d = core->regs.b.m;
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::a1:
				s = core->regs.a.m;
				break;
			case DspParameter::b1:
				s = core->regs.b.m;
				break;
			case DspParameter::x1:
				s = core->regs.x.h;
				break;
			case DspParameter::y1:
				s = core->regs.y.h;
				break;
		}

		r = d & s;

		switch (info.params[0])
		{
			case DspParameter::a1:
				core->regs.a.m = r;
				break;
			case DspParameter::b1:
				core->regs.b.m = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_or(AnalyzeInfo& info)
	{
		uint16_t d = 0;
		uint16_t s = 0;
		uint16_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a1:
				d = core->regs.a.m;
				break;
			case DspParameter::b1:
				d = core->regs.b.m;
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::a1:
				s = core->regs.a.m;
				break;
			case DspParameter::b1:
				s = core->regs.b.m;
				break;
			case DspParameter::x1:
				s = core->regs.x.h;
				break;
			case DspParameter::y1:
				s = core->regs.y.h;
				break;
		}

		r = d | s;

		switch (info.params[0])
		{
			case DspParameter::a1:
				core->regs.a.m = r;
				break;
			case DspParameter::b1:
				core->regs.b.m = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_lsf(AnalyzeInfo& info)
	{
		int64_t d = 0;
		uint16_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = core->regs.a.bits & 0x0000'00ff'ffff'ffff;
				break;
			case DspParameter::b:
				d = core->regs.b.bits & 0x0000'00ff'ffff'ffff;
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::x1:
				s = core->regs.x.h;
				break;
			case DspParameter::y1:
				s = core->regs.y.h;
				break;
			case DspParameter::a1:
				s = core->regs.a.m;
				break;
			case DspParameter::b1:
				s = core->regs.b.m;
				break;
		}

		if (s & 0x8000)
		{
			r = (uint64_t)d << (~s + 1);
		}
		else
		{
			r = (uint64_t)d >> s;
		}

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::p_asf(AnalyzeInfo& info)
	{
		int64_t d = 0;
		uint16_t s = 0;
		int64_t r = 0;

		switch (info.params[0])
		{
			case DspParameter::a:
				d = DspCore::SignExtend40(core->regs.a.bits);
				break;
			case DspParameter::b:
				d = DspCore::SignExtend40(core->regs.b.bits);
				break;
		}

		switch (info.params[1])
		{
			case DspParameter::x1:
				s = core->regs.x.h;
				break;
			case DspParameter::y1:
				s = core->regs.y.h;
				break;
			case DspParameter::a1:
				s = core->regs.a.m;
				break;
			case DspParameter::b1:
				s = core->regs.b.m;
				break;
		}

		if (s & 0x8000)
		{
			r = d << (~s + 1);
		}
		else
		{
			r = d >> s;		// Arithmetic
		}

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z1, NFlagRules::N1, EFlagRules::E1, UFlagRules::U1);
	}

}
