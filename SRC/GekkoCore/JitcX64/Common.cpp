#include "../pch.h"

// X64 Register usage:
// rsi: offset Gekko::regs.gpr

namespace Gekko
{
	// Special sections of code that are executed at the beginning and end of each translated segment.

	void Jitc::Prolog(CodeSegment* seg)
	{
		// mov    QWORD PTR [rsp+0x8],rbx
		// mov    QWORD PTR [rsp+0x10],rbp
		// mov    QWORD PTR [rsp+0x18],rsi
		// push   rdi
		// sub    rsp,0x40

		seg->Write(IntelAssembler::mov<64>(IntelCore::Param::sib_none_rsp_disp8, IntelCore::Param::rbx, 0x8));
		seg->Write(IntelAssembler::mov<64>(IntelCore::Param::sib_none_rsp_disp8, IntelCore::Param::rbp, 0x10));
		seg->Write(IntelAssembler::mov<64>(IntelCore::Param::sib_none_rsp_disp8, IntelCore::Param::rsi, 0x18));
		seg->Write(IntelAssembler::push<64>(IntelCore::Param::rdi));
		seg->Write(IntelAssembler::sub<64>(IntelCore::Param::rsp, IntelCore::Param::imm8, 0, 0x40));

		// mov rsi, Gekko::regs.gpr

		seg->Write(IntelAssembler::mov<64>(IntelCore::Param::rsi, IntelCore::Param::imm64, 0, (int64_t)core->regs.gpr));
	}

	void Jitc::Epilog(CodeSegment* seg)
	{
		// mov    rbx,QWORD PTR [rsp+0x50]
		// mov    rbp,QWORD PTR [rsp+0x58]
		// mov    rsi,QWORD PTR [rsp+0x60]
		// add    rsp,0x40
		// pop    rdi
		// ret

		seg->Write(IntelAssembler::mov<64>(IntelCore::Param::rbx, IntelCore::Param::sib_none_rsp_disp8, 0x50));
		seg->Write(IntelAssembler::mov<64>(IntelCore::Param::rbp, IntelCore::Param::sib_none_rsp_disp8, 0x58));
		seg->Write(IntelAssembler::mov<64>(IntelCore::Param::rsi, IntelCore::Param::sib_none_rsp_disp8, 0x60));
		seg->Write(IntelAssembler::add<64>(IntelCore::Param::rsp, IntelCore::Param::imm8, 0, 0x40));
		seg->Write(IntelAssembler::pop<64>(IntelCore::Param::rdi));
		seg->Write(IntelAssembler::ret<64>());
	}

	size_t Jitc::EpilogSize()
	{
		static size_t size = 0;

		if (size == 0)
		{
			CodeSegment seg;
			Epilog(&seg);
			size = seg.code.size();
		}

		return size;
	}

	// PC = PC + 4
	void Jitc::AddPc(CodeSegment* seg)
	{
		// mov rax, offset regs.pc
		// add dword ptr [rax], 4

		seg->Write(IntelAssembler::mov<64>(IntelCore::Param::rax, IntelCore::Param::imm64, 0, (int64_t)&core->regs.pc));
		seg->Write(IntelAssembler::add<64>(IntelCore::Param::m_rax, IntelCore::Param::simm8_as32, 0, 4));
	}

	void Jitc::CallTick(CodeSegment* seg)
	{
		// Call Jitc::Tick

		seg->Write(IntelAssembler::mov<64>(IntelCore::Param::rax, IntelCore::Param::imm64, 0, (int64_t)Jitc::Tick));
		seg->Write(IntelAssembler::call<64>(IntelCore::Param::rax));
	}

}
