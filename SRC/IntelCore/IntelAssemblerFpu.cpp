// x87 (FPU) instructions assembler.

#include "pch.h"

namespace IntelCore
{

	bool IntelAssembler::IsFpuReg(Param p)
	{
		return Param::X87Start < p && p < Param::X87End;
	}

	void IntelAssembler::GetFpuReg(Param p, size_t& reg)
	{
		switch (p)
		{
			case Param::st0: reg = 0; break;
			case Param::st1: reg = 1; break;
			case Param::st2: reg = 2; break;
			case Param::st3: reg = 3; break;
			case Param::st4: reg = 4; break;
			case Param::st5: reg = 5; break;
			case Param::st6: reg = 6; break;
			case Param::st7: reg = 7; break;

			default:
				throw "Invalid parameter";
		}
	}

	bool IntelAssembler::IsFpuInstr(Instruction instr)
	{
		return Instruction::FpuInstrStart < instr && instr < Instruction::FpuInstrEnd;
	}

	void IntelAssembler::FpuAssemble16(AnalyzeInfo& info)
	{
		switch (info.instr)
		{

			default:
				throw "Invalid instruction";
		}
	}

	void IntelAssembler::FpuAssemble32(AnalyzeInfo& info)
	{
		switch (info.instr)
		{

			default:
				throw "Invalid instruction";
		}
	}

	void IntelAssembler::FpuAssemble64(AnalyzeInfo& info)
	{
		switch (info.instr)
		{

			default:
				throw "Invalid instruction";
		}
	}

}
