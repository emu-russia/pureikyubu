// Integer Rotate Instructions
#include "../pch.h"

namespace Gekko
{

	// rlwinm rA,rS,SH,MB,ME 
	void Jitc::Rlwinm(AnalyzeInfo* info, CodeSegment* seg)
	{
		uint32_t mask = core->interp->GetRotMask(info->paramBits[3], info->paramBits[4]);

		// Place rS in eax

		seg->Write(IntelAssembler::_xor<64>(IntelCore::Param::rax, IntelCore::Param::rax));
		seg->Write(IntelAssembler::mov<64>(IntelCore::Param::eax, IntelCore::Param::m_rsi_disp8, (uint64_t)info->paramBits[1] << 2));

		// If SH != 0, then rotate the bits

		if (info->paramBits[2] != 0)
		{
			seg->Write(IntelAssembler::mov<64>(IntelCore::Param::cl, IntelCore::Param::imm8, 0, info->paramBits[2]));
			seg->Write(IntelAssembler::rol<64>(IntelCore::Param::eax, IntelCore::Param::cl));
		}

		// Apply the [MB;ME] mask and write the result to rA

		seg->Write(IntelAssembler::_and<64>(IntelCore::Param::eax, IntelCore::Param::imm32, 0, mask));
		seg->Write(IntelAssembler::mov<64>(IntelCore::Param::m_rsi_disp8, IntelCore::Param::eax, (uint64_t)info->paramBits[0] << 2));

		AddPc(seg);
		CallTick(seg);
	}

}
