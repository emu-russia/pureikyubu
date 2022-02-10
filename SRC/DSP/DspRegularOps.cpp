// Non-parallel instructions.
#include "pch.h"

using namespace Debug;

namespace DSP
{

	void DspInterpreter::jmp(DecoderInfo& info)
	{
		DspAddress address;

		if (info.params[0] == DspParameter::Address)
		{
			address = info.ImmOperand.Address;
		}
		else
		{
			address = core->MoveFromReg((int)info.params[0]);
		}

		if (ConditionTrue(info.cc))
		{
			if (core->dsp->logNonconditionalCallJmp && info.cc == ConditionCode::always)
			{
				Report(Channel::DSP, "0x%04X: jmp 0x%04X\n", core->regs.pc, address);
			}

			core->regs.pc = address;
		}
		else
		{
			core->regs.pc += (DspAddress)(info.sizeInBytes >> 1);
		}
	}

	void DspInterpreter::call(DecoderInfo& info)
	{
		DspAddress address;

		if (info.params[0] == DspParameter::Address)
		{
			address = info.ImmOperand.Address;
		}
		else
		{
			address = core->MoveFromReg((int)info.params[0]);
		}

		if (ConditionTrue(info.cc))
		{
			if (core->dsp->logNonconditionalCallJmp && info.cc == ConditionCode::always)
			{
				Report(Channel::DSP, "0x%04X: call 0x%04X\n", core->regs.pc, address);
			}

			if (core->regs.pcs->push((uint16_t)(core->regs.pc + (info.sizeInBytes >> 1))))
			{
				core->regs.pc = address;
			}
			else
			{
				core->AssertInterrupt(DspInterrupt::Error);
			}
		}
		else
		{
			core->regs.pc += (DspAddress)(info.sizeInBytes >> 1);
		}
	}

	void DspInterpreter::rets(DecoderInfo& info)
	{
		if (ConditionTrue(info.cc))
		{
			uint16_t pc;

			if (core->regs.pcs->pop(pc))
			{
				core->regs.pc = pc;
			}
			else
			{
				core->AssertInterrupt(DspInterrupt::Error);
			}
		}
		else
		{
			core->regs.pc++;
		}
	}

	void DspInterpreter::reti(DecoderInfo& info)
	{
		if (ConditionTrue(info.cc))
		{
			core->ReturnFromInterrupt();
		}
		else
		{
			core->regs.pc++;
		}
	}

	void DspInterpreter::trap(DecoderInfo& info)
	{
		core->AssertInterrupt(DspInterrupt::Trap);
	}

	void DspInterpreter::wait(DecoderInfo& info)
	{
		// In a real DSP, Clk is disabled and only the interrupt generation circuitry remains active. 
		// In the emulator, due to the fact that the instruction is flowControl, pc changes will not occur and the emulated DSP will "hang" on the execution of the `wait` instruction until an interrupt occurs.
	}

	void DspInterpreter::exec(DecoderInfo& info)
	{
		if (ConditionTrue(info.cc))
		{
			core->regs.pc++;
		}
		else
		{
			core->regs.pc += 2;
		}
	}

	void DspInterpreter::loop(DecoderInfo& info)
	{
		int lc;
		DspAddress end_addr;

		if (info.params[0] == DspParameter::Byte)
		{
			lc = info.ImmOperand.Byte;
			end_addr = info.ImmOperand2.Address;
		}
		else
		{
			lc = core->MoveFromReg((int)info.params[0]);
			end_addr = info.ImmOperand.Address;
		}

		if (lc != 0)
		{
			if (!core->regs.pcs->push(core->regs.pc + 2))
			{
				core->AssertInterrupt(DspInterrupt::Error);
				return;
			}
			if (!core->regs.lcs->push(lc))
			{
				core->AssertInterrupt(DspInterrupt::Error);
				return;
			}
			if (!core->regs.eas->push(end_addr))
			{
				core->AssertInterrupt(DspInterrupt::Error);
				return;
			}
			core->regs.pc += 2;
		}
		else
		{
			// If the parameter lc = 0, then accordingly no loop occurs, pc = end_address + 1 (the block is skipped)

			core->regs.pc = end_addr + 1;
		}
	}

	void DspInterpreter::rep(DecoderInfo& info)
	{
		int rc;

		if (info.params[0] == DspParameter::Byte)
		{
			rc = info.ImmOperand.Byte;
		}
		else
		{
			rc = core->MoveFromReg((int)info.params[0]);
		}

		if (rc != 0)
		{
			core->repeatCount = rc;
			core->regs.pc++;
		}
		else
		{
			core->regs.pc += 2;		// Skip next 1-cycle instruction
		}
	}

	void DspInterpreter::pld(DecoderInfo& info)
	{
		int r = (int)info.params[1];
		core->MoveToReg((int)info.params[0], core->dsp->ReadIMem(core->regs.r[r]) );
		AdvanceAddress(r, info.params[2]);
	}

	void DspInterpreter::mr(DecoderInfo& info)
	{
		int r = (int)info.params[0];
		AdvanceAddress(r, info.params[1]);
	}

	void DspInterpreter::adsi(DecoderInfo& info)
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

		s = DspCore::SignExtend16((int16_t)info.ImmOperand.SignedByte) << 16;

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

		core->ModifyFlags(d, s, r, CFlagRules::C1, VFlagRules::V1, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::adli(DecoderInfo& info)
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

		s = DspCore::SignExtend16(info.ImmOperand.UnsignedShort) << 16;

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

		core->ModifyFlags(d, s, r, CFlagRules::C1, VFlagRules::V1, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::cmpsi(DecoderInfo& info)
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

		s = DspCore::SignExtend16((int16_t)info.ImmOperand.SignedByte) << 16;

		r = d - s;

		core->ModifyFlags(d, s, r, CFlagRules::C2, VFlagRules::V2, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::cmpli(DecoderInfo& info)
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

		s = DspCore::SignExtend16(info.ImmOperand.UnsignedShort) << 16;

		r = d - s;

		core->ModifyFlags(d, s, r, CFlagRules::C2, VFlagRules::V2, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::lsfi(DecoderInfo& info)
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

		s = (int16_t)info.ImmOperand.SignedByte;

		if (s & 0x8000)
		{
			r = (uint64_t)d >> (~s + 1);
		}
		else
		{
			r = (uint64_t)d << s;
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

	void DspInterpreter::asfi(DecoderInfo& info)
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

		s = (int16_t)info.ImmOperand.SignedByte;

		if (s & 0x8000)
		{
			r = d >> (~s + 1);
		}
		else
		{
			r = d << s;		// Arithmetic
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

	void DspInterpreter::xorli(DecoderInfo& info)
	{
		uint16_t d = 0;
		uint16_t s = info.ImmOperand.UnsignedShort;
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

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::anli(DecoderInfo& info)
	{
		uint16_t d = 0;
		uint16_t s = info.ImmOperand.UnsignedShort;
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

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::orli(DecoderInfo& info)
	{
		uint16_t d = 0;
		uint16_t s = info.ImmOperand.UnsignedShort;
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

		core->ModifyFlags(d, s, r, CFlagRules::Zero, VFlagRules::Zero, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::norm(DecoderInfo& info)
	{
		Halt("DspInterpreter::norm\n");
	}

	void DspInterpreter::div(DecoderInfo& info)
	{
		Halt("DspInterpreter::div\n");
	}

	void DspInterpreter::addc(DecoderInfo& info)
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
			case DspParameter::x:
				s = DspCore::SignExtend32(core->regs.x.bits);
				break;
			case DspParameter::y:
				s = DspCore::SignExtend32(core->regs.y.bits);
				break;
		}

		r = d + s + core->regs.psr.c;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C1, VFlagRules::V1, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::subc(DecoderInfo& info)
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
			case DspParameter::x:
				s = DspCore::SignExtend32(core->regs.x.bits);
				break;
			case DspParameter::y:
				s = DspCore::SignExtend32(core->regs.y.bits);
				break;
		}

		r = d + (int64_t)(~s) + core->regs.psr.c;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C2, VFlagRules::V2, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::negc(DecoderInfo& info)
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

		r = d + (int64_t)(~s) + core->regs.psr.c;

		switch (info.params[0])
		{
			case DspParameter::a:
				core->regs.a.bits = r;
				break;
			case DspParameter::b:
				core->regs.b.bits = r;
				break;
		}

		core->ModifyFlags(d, s, r, CFlagRules::C4, VFlagRules::V3, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::_max(DecoderInfo& info)
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
		}

		// abs
		if (d < 0) d = -d;
		if (s < 0) s = -s;

		r = d - s;

		core->ModifyFlags(d, s, r, CFlagRules::C5, VFlagRules::Zero, ZFlagRules::Z2, NFlagRules::N2, EFlagRules::E1, UFlagRules::U1);
	}

	void DspInterpreter::lsf(DecoderInfo& info)
	{
		int64_t d = 0;
		int16_t s = 0;
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

		if (-s < 0)
		{
			r = (uint64_t)d >> s;
		}
		else
		{
			r = (uint64_t)d << (~s + 1);
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

	void DspInterpreter::asf(DecoderInfo& info)
	{
		int64_t d = 0;
		int16_t s = 0;
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

		if (-s < 0)
		{
			r = d >> s;		// Arithmetic
		}
		else
		{
			r = d << (~s + 1);
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

	void DspInterpreter::ld(DecoderInfo& info)
	{
		int r = (int)info.params[1];
		core->MoveToReg((int)info.params[0], core->dsp->ReadDMem(core->regs.r[r]) );
		AdvanceAddress(r, info.params[2]);
	}

	void DspInterpreter::st(DecoderInfo& info)
	{
		int r = (int)info.params[0];
		core->dsp->WriteDMem(core->regs.r[r], core->MoveFromReg((int)info.params[2]) );
		AdvanceAddress(r, info.params[1]);
	}

	void DspInterpreter::ldsa(DecoderInfo& info)
	{
		core->MoveToReg((int)info.params[0], 
			core->dsp->ReadDMem( (DspAddress)((core->regs.dpp << 8) | (info.ImmOperand.Address))) );
	}

	void DspInterpreter::stsa(DecoderInfo& info)
	{
		uint16_t s;

		switch (info.params[1])
		{
			case DspParameter::a2:
				s = core->regs.a.h & 0xff;
				if (s & 0x80) s |= 0xff00;
				break;
			case DspParameter::b2:
				s = core->regs.b.h & 0xff;
				if (s & 0x80) s |= 0xff00;
				break;
			default:
				s = core->MoveFromReg((int)info.params[1]);
				break;
		}

		core->dsp->WriteDMem((DspAddress)((core->regs.dpp << 8) | (info.ImmOperand.Address)), s);
	}

	void DspInterpreter::ldla(DecoderInfo& info)
	{
		core->MoveToReg((int)info.params[0], core->dsp->ReadDMem(info.ImmOperand.Address) );
	}

	void DspInterpreter::stla(DecoderInfo& info)
	{
		core->dsp->WriteDMem(info.ImmOperand.Address, core->MoveFromReg((int)info.params[1]) );
	}

	void DspInterpreter::mv(DecoderInfo& info)
	{
		core->MoveToReg((int)info.params[0], core->MoveFromReg((int)info.params[1]));
	}

	void DspInterpreter::mvsi(DecoderInfo& info)
	{
		core->MoveToReg((int)info.params[0], (int16_t)info.ImmOperand.SignedByte);
	}

	void DspInterpreter::mvli(DecoderInfo& info)
	{
		core->MoveToReg((int)info.params[0], info.ImmOperand.UnsignedShort);
	}

	void DspInterpreter::stli(DecoderInfo& info)
	{
		core->dsp->WriteDMem(info.ImmOperand.Address, info.ImmOperand2.UnsignedShort);
	}

	void DspInterpreter::clr(DecoderInfo& info)
	{
		switch (info.params[0])
		{
			case DspParameter::psr_tb: core->regs.psr.tb = 0; break;
			case DspParameter::psr_sv: core->regs.psr.sv = 0; break;
			case DspParameter::psr_te0: core->regs.psr.te0 = 0; break;
			case DspParameter::psr_te1: core->regs.psr.te1 = 0; break;
			case DspParameter::psr_te2: core->regs.psr.te2 = 0; break;
			case DspParameter::psr_te3: core->regs.psr.te3 = 0; break;
			case DspParameter::psr_et: core->regs.psr.et = 0; break;

			default:
				Halt("DspInterpreter::clr: Invalid parameter\n");
		}
	}

	void DspInterpreter::set(DecoderInfo& info)
	{
		switch (info.params[0])
		{
			case DspParameter::psr_tb: core->regs.psr.tb = 1; break;
			case DspParameter::psr_sv: core->regs.psr.sv = 1; break;
			case DspParameter::psr_te0: core->regs.psr.te0 = 1; break;
			case DspParameter::psr_te1: core->regs.psr.te1 = 1; break;
			case DspParameter::psr_te2: core->regs.psr.te2 = 1; break;
			case DspParameter::psr_te3: core->regs.psr.te3 = 1; break;
			case DspParameter::psr_et: core->regs.psr.et = 1; break;

			default:
				Halt("DspInterpreter::set: Invalid parameter\n");
		}
	}

	void DspInterpreter::btstl(DecoderInfo& info)
	{
		uint16_t val = 0;

		switch (info.params[0])
		{
			case DspParameter::a1:
				val = core->regs.a.m;
				break;
			case DspParameter::b1:
				val = core->regs.b.m;
				break;

			default:
				Halt("DspInterpreter::btstl: Invalid parameter\n");
		}

		core->regs.psr.tb = (val & info.ImmOperand.UnsignedShort) == 0;
	}
	
	void DspInterpreter::btsth(DecoderInfo& info)
	{
		uint16_t val = 0;

		switch (info.params[0])
		{
			case DspParameter::a1:
				val = core->regs.a.m;
				break;
			case DspParameter::b1:
				val = core->regs.b.m;
				break;

			default:
				Halt("DspInterpreter::btsth: Invalid parameter\n");
		}

		core->regs.psr.tb = (val & info.ImmOperand.UnsignedShort) == info.ImmOperand.UnsignedShort;
	}

}
