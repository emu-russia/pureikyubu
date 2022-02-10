// Floating-Point Load and Store Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

	// ea = (ra | 0) + SIMM
	// fd = MEM(ea, 8)
	void Interpreter::lfd(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			if (info.paramBits[1]) core->ReadDouble(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, &FPRU(info.paramBits[0]));
			else core->ReadDouble((int32_t)info.Imm.Signed, &FPRU(info.paramBits[0]));
			if (core->exception) return;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// ea = ra + SIMM
	// fd = MEM(ea, 8)
	// ra = ea
	void Interpreter::lfdu(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
			core->ReadDouble(ea, &FPRU(info.paramBits[0]));
			if (core->exception) return;
			core->regs.gpr[info.paramBits[1]] = ea;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// ea = ra + rb
	// fd = MEM(ea, 8)
	// ra = ea
	void Interpreter::lfdux(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
			core->ReadDouble(ea, &FPRU(info.paramBits[0]));
			if (core->exception) return;
			core->regs.gpr[info.paramBits[1]] = ea;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// ea = (ra | 0) + rb
	// fd = MEM(ea, 8)
	void Interpreter::lfdx(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			if (info.paramBits[1]) core->ReadDouble(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], &FPRU(info.paramBits[0]));
			else core->ReadDouble(core->regs.gpr[info.paramBits[2]], &FPRU(info.paramBits[0]));
			if (core->exception) return;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// ea = (ra | 0) + SIMM
	// if HID2[PSE] = 0
	//      then fd = DOUBLE(MEM(ea, 4))
	//      else fd(ps0) = SINGLE(MEM(ea, 4))
	//           fd(ps1) = SINGLE(MEM(ea, 4))
	void Interpreter::lfs(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			float res;

			if (info.paramBits[1]) core->ReadWord(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, (uint32_t*)&res);
			else core->ReadWord((int32_t)info.Imm.Signed, (uint32_t*)&res);

			if (core->exception) return;

			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS0(info.paramBits[0]) = PS1(info.paramBits[0]) = (double)res;
			else FPRD(info.paramBits[0]) = (double)res;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// ea = ra + SIMM
	// if HID2[PSE] = 0
	//      then fd = DOUBLE(MEM(ea, 4))
	//      else fd(ps0) = SINGLE(MEM(ea, 4))
	//           fd(ps1) = SINGLE(MEM(ea, 4))
	// ra = ea
	void Interpreter::lfsu(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
			float res;

			core->ReadWord(ea, (uint32_t*)&res);

			if (core->exception) return;

			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS0(info.paramBits[0]) = PS1(info.paramBits[0]) = (double)res;
			else FPRD(info.paramBits[0]) = (double)res;

			core->regs.gpr[info.paramBits[1]] = ea;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// ea = ra + rb
	// if HID2[PSE] = 0
	//      then fd = DOUBLE(MEM(ea, 4))
	//      else fd(ps0) = SINGLE(MEM(ea, 4))
	//           fd(ps1) = SINGLE(MEM(ea, 4))
	// ra = ea
	void Interpreter::lfsux(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
			float res;

			core->ReadWord(ea, (uint32_t*)&res);

			if (core->exception) return;

			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS0(info.paramBits[0]) = PS1(info.paramBits[0]) = (double)res;
			else FPRD(info.paramBits[0]) = (double)res;

			core->regs.gpr[info.paramBits[1]] = ea;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// ea = (ra | 0) + rb
	// if HID2[PSE] = 0
	//      then fd = DOUBLE(MEM(ea, 4))
	//      else fd(ps0) = SINGLE(MEM(ea, 4))
	//           fd(ps1) = SINGLE(MEM(ea, 4))
	void Interpreter::lfsx(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			float res;

			if (info.paramBits[1]) core->ReadWord(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], (uint32_t*)&res);
			else core->ReadWord(core->regs.gpr[info.paramBits[2]], (uint32_t*)&res);

			if (core->exception) return;

			if (core->regs.spr[SPR::HID2] & HID2_PSE) PS0(info.paramBits[0]) = PS1(info.paramBits[0]) = (double)res;
			else FPRD(info.paramBits[0]) = (double)res;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// ea = (ra | 0) + SIMM
	// MEM(ea, 8) = fs
	void Interpreter::stfd(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			if (info.paramBits[1]) core->WriteDouble(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, &FPRU(info.paramBits[0]));
			else core->WriteDouble((int32_t)info.Imm.Signed, &FPRU(info.paramBits[0]));
			if (core->exception) return;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// ea = ra + SIMM
	// MEM(ea, 8) = fs
	// ra = ea
	void Interpreter::stfdu(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
			core->WriteDouble(ea, &FPRU(info.paramBits[0]));
			if (core->exception) return;
			core->regs.gpr[info.paramBits[1]] = ea;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// ea = ra + rb
	// MEM(ea, 8) = fs
	// ra = ea
	void Interpreter::stfdux(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
			core->WriteDouble(ea, &FPRU(info.paramBits[0]));
			if (core->exception) return;
			core->regs.gpr[info.paramBits[1]] = ea;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// ea = (ra | 0) + rb
	// MEM(ea, 8) = fs
	void Interpreter::stfdx(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			if (info.paramBits[1]) core->WriteDouble(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], &FPRU(info.paramBits[0]));
			else core->WriteDouble(core->regs.gpr[info.paramBits[2]], &FPRU(info.paramBits[0]));
			if (core->exception) return;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// ea = (ra | 0) + rb
	// MEM(ea, 4) = fs[32-63]
	void Interpreter::stfiwx(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			uint32_t val = (uint32_t)(FPRU(info.paramBits[0]) & 0x00000000ffffffff);
			if (info.paramBits[1]) core->WriteWord(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], val);
			else core->WriteWord(core->regs.gpr[info.paramBits[2]], val);
			if (core->exception) return;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// ea = (ra | 0) + SIMM
	// MEM(ea, 4) = SINGLE(fs)
	void Interpreter::stfs(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			float data = (float)FPRD(info.paramBits[0]);
			if (info.paramBits[1]) core->WriteWord(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, *(uint32_t*)&data);
			else core->WriteWord((int32_t)info.Imm.Signed, *(uint32_t*)&data);
			if (core->exception) return;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// ea = ra + SIMM
	// MEM(ea, 4) = SINGLE(fs)
	// ra = ea
	void Interpreter::stfsu(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			float data = (float)FPRD(info.paramBits[0]);
			uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
			core->WriteWord(ea, *(uint32_t*)&data);
			if (core->exception) return;
			core->regs.gpr[info.paramBits[1]] = ea;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// ea = ra + rb
	// MEM(ea, 4) = SINGLE(fs)
	// ra = ea
	void Interpreter::stfsux(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			float data = (float)FPRD(info.paramBits[0]);
			uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
			core->WriteWord(ea, *(uint32_t *)&data);
			if (core->exception) return;
			core->regs.gpr[info.paramBits[1]] = ea;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	// ea = (ra | 0) + rb
	// MEM(ea, 4) = SINGLE(fs)
	void Interpreter::stfsx(DecoderInfo& info)
	{
		if (core->regs.msr & MSR_FP)
		{
			float data = (float)FPRD(info.paramBits[0]);
			if (info.paramBits[1]) core->WriteWord(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], *(uint32_t*)&data);
			else core->WriteWord(core->regs.gpr[info.paramBits[2]], *(uint32_t*)&data);
			if (core->exception) return;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

}
