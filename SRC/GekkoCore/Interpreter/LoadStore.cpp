// Integer Load and Store Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

	// ea = (ra | 0) + SIMM
	// rd = 0x000000 || MEM(ea, 1)
	void Interpreter::lbz(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->ReadByte(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, &core->regs.gpr[info.paramBits[0]]);
		else core->ReadByte((int32_t)info.Imm.Signed, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = ra + SIMM
	// rd = 0x000000 || MEM(ea, 1)
	// ra = ea
	void Interpreter::lbzu(AnalyzeInfo& info)
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		core->ReadByte(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = ra + rb
	// rd = 0x000000 || MEM(ea, 1)
	// ra = ea
	void Interpreter::lbzux(AnalyzeInfo& info)
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
		core->ReadByte(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// rd = 0x000000 || MEM(ea, 1)
	void Interpreter::lbzx(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->ReadByte(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], &core->regs.gpr[info.paramBits[0]]);
		else core->ReadByte(core->regs.gpr[info.paramBits[2]], &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + SIMM
	// rd = (signed)MEM(ea, 2)
	void Interpreter::lha(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->ReadHalf(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, &core->regs.gpr[info.paramBits[0]]);
		else core->ReadHalf((int32_t)info.Imm.Signed, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		if (core->regs.gpr[info.paramBits[0]] & 0x8000) core->regs.gpr[info.paramBits[0]] |= 0xffff0000;
		core->regs.pc += 4;
	}

	// ea = ra + SIMM
	// rd = (signed)MEM(ea, 2)
	// ra = ea
	void Interpreter::lhau(AnalyzeInfo& info)
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		core->ReadHalf(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		if (core->regs.gpr[info.paramBits[0]] & 0x8000) core->regs.gpr[info.paramBits[0]] |= 0xffff0000;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = ra + rb
	// rd = (signed)MEM(ea, 2)
	// ra = ea
	void Interpreter::lhaux(AnalyzeInfo& info)
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
		core->ReadHalf(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		if (core->regs.gpr[info.paramBits[0]] & 0x8000) core->regs.gpr[info.paramBits[0]] |= 0xffff0000;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// rd = (signed)MEM(ea, 2)
	void Interpreter::lhax(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->ReadHalf(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], &core->regs.gpr[info.paramBits[0]]);
		else core->ReadHalf(core->regs.gpr[info.paramBits[2]], &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		if (core->regs.gpr[info.paramBits[0]] & 0x8000) core->regs.gpr[info.paramBits[0]] |= 0xffff0000;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + SIMM
	// rd = 0x0000 || MEM(ea, 2)
	void Interpreter::lhz(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->ReadHalf(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, &core->regs.gpr[info.paramBits[0]]);
		else core->ReadHalf((int32_t)info.Imm.Signed, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = ra + SIMM
	// rd = 0x0000 || MEM(ea, 2)
	// ra = ea
	void Interpreter::lhzu(AnalyzeInfo& info)
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		core->ReadHalf(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = ra + rb
	// rd = 0x0000 || MEM(ea, 2)
	// ra = ea
	void Interpreter::lhzux(AnalyzeInfo& info)
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
		core->ReadHalf(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// rd = 0x0000 || MEM(ea, 2)
	void Interpreter::lhzx(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->ReadHalf(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], &core->regs.gpr[info.paramBits[0]]);
		else core->ReadHalf(core->regs.gpr[info.paramBits[2]], &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + SIMM
	// rd = MEM(ea, 4)
	void Interpreter::lwz(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->ReadWord(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, &core->regs.gpr[info.paramBits[0]]);
		else core->ReadWord((int32_t)info.Imm.Signed, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = ra + SIMM
	// rd = MEM(ea, 4)
	// ra = ea
	void Interpreter::lwzu(AnalyzeInfo& info)
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		core->ReadWord(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = ra + rb
	// rd = MEM(ea, 4)
	// ra = ea
	void Interpreter::lwzux(AnalyzeInfo& info)
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
		core->ReadWord(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// rd = MEM(ea, 4)
	void Interpreter::lwzx(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->ReadWord(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], &core->regs.gpr[info.paramBits[0]]);
		else core->ReadWord(core->regs.gpr[info.paramBits[2]], &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + SIMM
	// MEM(ea, 1) = rs[24-31]
	void Interpreter::stb(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->WriteByte(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, core->regs.gpr[info.paramBits[0]]);
		else core->WriteByte((int32_t)info.Imm.Signed, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = ra + SIMM
	// MEM(ea, 1) = rs[24-31]
	// ra = ea
	void Interpreter::stbu(AnalyzeInfo& info)
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		core->WriteByte(ea, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = ra + rb
	// MEM(ea, 1) = rs[24-31]
	// ra = ea
	void Interpreter::stbux(AnalyzeInfo& info)
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
		core->WriteByte(ea, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// MEM(ea, 1) = rs[24-31]
	void Interpreter::stbx(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->WriteByte(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], core->regs.gpr[info.paramBits[0]]);
		else core->WriteByte(core->regs.gpr[info.paramBits[2]], core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + SIMM
	// MEM(ea, 2) = rs[16-31]
	void Interpreter::sth(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->WriteHalf(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, core->regs.gpr[info.paramBits[0]]);
		else core->WriteHalf((int32_t)info.Imm.Signed, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = ra + SIMM
	// MEM(ea, 2) = rs[16-31]
	// ra = ea
	void Interpreter::sthu(AnalyzeInfo& info)
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		core->WriteHalf(ea, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = ra + rb
	// MEM(ea, 2) = rs[16-31]
	// ra = ea
	void Interpreter::sthux(AnalyzeInfo& info)
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
		core->WriteHalf(ea, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// MEM(ea, 2) = rs[16-31]
	void Interpreter::sthx(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->WriteHalf(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], core->regs.gpr[info.paramBits[0]]);
		else core->WriteHalf(core->regs.gpr[info.paramBits[2]], core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + SIMM
	// MEM(ea, 4) = rs
	void Interpreter::stw(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->WriteWord(core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed, core->regs.gpr[info.paramBits[0]]);
		else core->WriteWord((int32_t)info.Imm.Signed, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = ra + SIMM
	// MEM(ea, 4) = rs
	// ra = ea
	void Interpreter::stwu(AnalyzeInfo& info)
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		core->WriteWord(ea, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = ra + rb
	// MEM(ea, 4) = rs
	// ra = ea
	void Interpreter::stwux(AnalyzeInfo& info)
	{
		uint32_t ea = core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]];
		core->WriteWord(ea, core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[1]] = ea;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// MEM(ea, 4) = rs
	void Interpreter::stwx(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->WriteWord(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], core->regs.gpr[info.paramBits[0]]);
		else core->WriteWord(core->regs.gpr[info.paramBits[2]], core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// rd = 0x0000 || MEM(ea+1, 1) || MEM(EA, 1)
	void Interpreter::lhbrx(AnalyzeInfo& info)
	{
		uint32_t val;
		if (info.paramBits[1]) core->ReadHalf(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], &val);
		else core->ReadHalf(core->regs.gpr[info.paramBits[2]], &val);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[0]] = _BYTESWAP_UINT16((uint16_t)val);
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// rd = MEM(ea+3, 1) || MEM(ea+2, 1) || MEM(ea+1, 1) || MEM(ea, 1)
	void Interpreter::lwbrx(AnalyzeInfo& info)
	{
		uint32_t val;
		if (info.paramBits[1]) core->ReadWord(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], &val);
		else core->ReadWord(core->regs.gpr[info.paramBits[2]], &val);
		if (core->exception) return;
		core->regs.gpr[info.paramBits[0]] = _BYTESWAP_UINT32(val);
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// MEM(ea, 2) = rs[24-31] || rs[16-23]
	void Interpreter::sthbrx(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->WriteHalf(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], _BYTESWAP_UINT16((uint16_t)core->regs.gpr[info.paramBits[0]]));
		else core->WriteHalf(core->regs.gpr[info.paramBits[2]], _BYTESWAP_UINT16((uint16_t)core->regs.gpr[info.paramBits[0]]));
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// MEM(ea, 4) = rs[24-31] || rs[16-23] || rs[8-15] || rs[0-7]
	void Interpreter::stwbrx(AnalyzeInfo& info)
	{
		if (info.paramBits[1]) core->WriteWord(core->regs.gpr[info.paramBits[1]] + core->regs.gpr[info.paramBits[2]], _BYTESWAP_UINT32(core->regs.gpr[info.paramBits[0]]));
		else core->WriteWord(core->regs.gpr[info.paramBits[2]], _BYTESWAP_UINT32(core->regs.gpr[info.paramBits[0]]));
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + SIMM
	// r = rd
	// while r <= 31
	//      GPR(r) = MEM(ea, 4)
	//      r = r + 1
	//      ea = ea + 4
	void Interpreter::lmw(AnalyzeInfo& info)
	{
		uint32_t ea;
		if (info.paramBits[1]) ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		else ea = (int32_t)info.Imm.Signed;

		for (int r = info.paramBits[0]; r < 32; r++, ea += 4)
		{
			core->ReadWord(ea, &core->regs.gpr[r]);
			if (core->exception) return;
		}
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + SIMM
	// r = rs
	// while r <= 31
	//      MEM(ea, 4) = GPR(r)
	//      r = r + 1
	//      ea = ea + 4
	void Interpreter::stmw(AnalyzeInfo& info)
	{
		uint32_t ea;
		if (info.paramBits[1]) ea = core->regs.gpr[info.paramBits[1]] + (int32_t)info.Imm.Signed;
		else ea = (int32_t)info.Imm.Signed;

		for (int r = info.paramBits[0]; r < 32; r++, ea += 4)
		{
			core->WriteWord(ea, core->regs.gpr[r]);
			if (core->exception) return;
		}
		core->regs.pc += 4;
	}

	// ea = (ra | 0)
	// n = NB ? NB : 32
	// r = rd - 1
	// i = 0
	// while n > 0
	//      if i = 0 then
	//          r = (r + 1) % 32
	//          GPR(r) = 0
	//      GPR(r)[i...i+7] = MEM(ea, 1)
	//      i = i + 8
	//      if i = 32 then i = 0
	//      ea = ea + 1
	//      n = n -1
	void Interpreter::lswi(AnalyzeInfo& info)
	{
		int32_t rd = info.paramBits[0], n = (info.paramBits[2]) ? (info.paramBits[2]) : 32, i = 4;
		uint32_t ea = (info.paramBits[1]) ? (core->regs.gpr[info.paramBits[1]]) : 0;
		uint32_t r = 0, val;

		while (n > 0)
		{
			if (i == 0)
			{
				i = 4;
				core->regs.gpr[rd] = r;
				rd++;
				rd %= 32;
				r = 0;
			}
			core->ReadByte(ea, &val);
			if (core->exception) return;
			r <<= 8;
			r |= (uint8_t)val;
			ea++;
			i--;
			n--;
		}

		if (i != 0)
		{
			while (i)
			{
				r <<= 8;
				i--;
			}
			core->regs.gpr[rd] = r;
		}

		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// n = XER[25-31]
	// r = rd - 1
	// i = 0
	// while n > 0
	//      if i = 0 then
	//          r = (r + 1) % 32
	//          GPR(r) = 0
	//      GPR(r)[i...i+7] = MEM(ea, 1)
	//      i = i + 8
	//      if i = 32 then i = 0
	//      ea = ea + 1
	//      n = n -1
	void Interpreter::lswx(AnalyzeInfo& info)
	{
		int32_t rd = info.paramBits[0], n = core->regs.spr[SPR::XER] & 0x7f, i = 4;
		uint32_t ea = ((info.paramBits[1]) ? (core->regs.gpr[info.paramBits[1]]) : 0) + core->regs.gpr[info.paramBits[2]];
		uint32_t r = 0, val;

		while (n > 0)
		{
			if (i == 0)
			{
				i = 4;
				core->regs.gpr[rd] = r;
				rd++;
				rd %= 32;
				r = 0;
			}
			core->ReadByte(ea, &val);
			if (core->exception) return;
			r <<= 8;
			r |= (uint8_t)val;
			ea++;
			i--;
			n--;
		}

		if (i != 0)
		{
			while (i)
			{
				r <<= 8;
				i--;
			}
			core->regs.gpr[rd] = r;
		}

		core->regs.pc += 4;
	}

	// ea = (ra | 0)
	// n = NB ? NB : 32
	// r = rs - 1
	// i = 0
	// while n > 0
	//      if i = 0 then r = (r + 1) % 32
	//      MEM(ea, 1) = GPR(r)[i...i+7]
	//      i = i + 8
	//      if i = 32 then i = 0;
	//      ea = ea + 1
	//      n = n -1
	void Interpreter::stswi(AnalyzeInfo& info)
	{
		int32_t rs = info.paramBits[0], n = (info.paramBits[2]) ? (info.paramBits[2]) : 32, i = 0;
		uint32_t ea = (info.paramBits[1]) ? (core->regs.gpr[info.paramBits[1]]) : 0;
		uint32_t r = 0;

		while (n > 0)
		{
			if (i == 0)
			{
				r = core->regs.gpr[rs];
				rs++;
				rs %= 32;
				i = 4;
			}
			core->WriteByte(ea, r >> 24);
			if (core->exception) return;
			r <<= 8;
			ea++;
			i--;
			n--;
		}
		core->regs.pc += 4;
	}

	// ea = (ra | 0)
	// n = XER[25-31]
	// r = rs - 1
	// i = 0
	// while n > 0
	//      if i = 0 then r = (r + 1) % 32
	//      MEM(ea, 1) = GPR(r)[i...i+7]
	//      i = i + 8
	//      if i = 32 then i = 0;
	//      ea = ea + 1
	//      n = n -1
	void Interpreter::stswx(AnalyzeInfo& info)
	{
		int32_t rs = info.paramBits[0], n = core->regs.spr[SPR::XER] & 0x7f, i = 0;
		uint32_t ea = ((info.paramBits[1]) ? (core->regs.gpr[info.paramBits[1]]) : 0) + core->regs.gpr[info.paramBits[2]];
		uint32_t r = 0;

		while (n > 0)
		{
			if (i == 0)
			{
				r = core->regs.gpr[rs];
				rs++;
				rs %= 32;
				i = 4;
			}
			core->WriteByte(ea, r >> 24);
			if (core->exception) return;
			r <<= 8;
			ea++;
			i--;
			n--;
		}
		core->regs.pc += 4;
	}

}
