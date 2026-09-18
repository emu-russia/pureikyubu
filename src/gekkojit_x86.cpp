/*

Gekko -> 32-bit x86 basic block recompiler. See gekkojit.h for the design notes;
gekkojit_x64.cpp is the x86-64 sibling of this file and gekkojit_ps_x86.cpp the
Paired-Single half.

The translator below is the x86-64 one, adapted to the register file 32-bit mode has.
Everything that has to happen once per block - retiring the instruction counter,
advancing the time base and the decrementer, the branch check - is still done in C++ by
Jit::Run when the block returns, and the instruction coverage is intentionally the same
as the x86-64 module's.

## What the 32-bit register file forces

Eight general purpose registers, four of them callee-saved. The allocation is in
gekkojit_layout_x86.h: the core pointer doubles as the pointer to the emulated register
file (GekkoRegs hangs off it at a fixed offset), the pc and the retired-instruction
counter take two more callee-saved registers and the remaining one (EBP) is the fourth
scratch. The effective address of an update-form access and the JitExit pointer live in
frame slots.

Two translations need a fourth and a fifth flag bit at once (COMPUTE_CR0 and the CR
updates of the compares). They take the three condition bits from the single comparison
and park one of them in AH, which is the one high byte register the ModRM encoding
reaches without REX.

*/

#include "pch.h"
#include "gekkojit.h"

#if GEKKO_JIT_X86

#include "jit_x86.h"
#include "gekkojit_layout_x86.h"
#include "gekkojit_ps.h"

#if defined(_WINDOWS)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace Gekko
{


// ---------------------------------------------------------------------------
// Thin trampolines for the memory helpers. A pointer-to-member-function is not a
// code address, so the generated code calls these instead; at -O2 they are a tail
// jump into the real helper.

namespace
{
	void JitReadByte(GekkoCore* core, uint32_t addr, uint32_t* reg)
	{
		stats.memHelperCalls++;
		CycleScope cycles(&stats.memHelperCycles);
		core->ReadByte(addr, reg);
	}
	void JitReadHalf(GekkoCore* core, uint32_t addr, uint32_t* reg)
	{
		stats.memHelperCalls++;
		CycleScope cycles(&stats.memHelperCycles);
		core->ReadHalf(addr, reg);
	}
	void JitReadWord(GekkoCore* core, uint32_t addr, uint32_t* reg)
	{
		stats.memHelperCalls++;
		CycleScope cycles(&stats.memHelperCycles);
		core->ReadWord(addr, reg);
	}
	void JitWriteByte(GekkoCore* core, uint32_t addr, uint32_t data)
	{
		stats.memHelperCalls++;
		CycleScope cycles(&stats.memHelperCycles);
		core->WriteByte(addr, data);
	}
	void JitWriteHalf(GekkoCore* core, uint32_t addr, uint32_t data)
	{
		stats.memHelperCalls++;
		CycleScope cycles(&stats.memHelperCycles);
		core->WriteHalf(addr, data);
	}
	void JitWriteWord(GekkoCore* core, uint32_t addr, uint32_t data)
	{
		stats.memHelperCalls++;
		CycleScope cycles(&stats.memHelperCycles);
		core->WriteWord(addr, data);
	}
}

// ---------------------------------------------------------------------------
// Interpreter callbacks used by the generated code.
// These are static members so that they inherit Jit's friendship.

// Run one instruction through the interpreter. Used for everything that the
// recompiler does not translate itself, which guarantees that the semantics stay
// in the interpreter and that the recompiler can never execute a wrong
// instruction - only a slower one.
void Jit::Fallback(GekkoCore* core, uint32_t instr, uint32_t pc)
{
	stats.jitFallbacks++;
	CycleScope cycles(&stats.fallbackCycles);
	core->interp->ExecuteDecoded(pc, instr);
}

bool Jit::BcTest(GekkoCore* core, uint32_t bo, uint32_t bi)
{
	return core->interp->BcTest(bo, bi);
}

bool Jit::BctrTest(GekkoCore* core, uint32_t bo, uint32_t bi)
{
	return core->interp->BctrTest(bo, bi);
}

void Jit::BranchCheck(GekkoCore* core)
{
	core->interp->BranchCheck();
}

// ---------------------------------------------------------------------------

Jit::Jit(GekkoCore* _core) : core(_core)
{
	interp = core->interp;

	for (size_t s = 0; s < BlockCacheSets; s++)
	{
		for (size_t w = 0; w < BlockCacheWays; w++)
		{
			blocks[s][w].gen = 0;
			blocks[s][w].pc = 0;
			blocks[s][w].pa = 0;
			blocks[s][w].instrCount = 0;
			blocks[s][w].codeOffset = 0;
		}
	}

	// Friend access: measure the offsets of the private core fields once, so that
	// the generated code can address them without an accessor call.
	exceptionOffset = (int32_t)((const uint8_t*)&core->exception - (const uint8_t*)core);
	decreqOffset = (int32_t)((const uint8_t*)&core->decreq - (const uint8_t*)core);
	opsOffset = (int32_t)((const uint8_t*)&core->ops - (const uint8_t*)core);
	resetCounterOffset = (int32_t)((const uint8_t*)&core->resetInstructionCounter - (const uint8_t*)core);

	code = (uint8_t*)AllocCode(CodeArenaSize);
	supported = (code != nullptr);
}

Jit::~Jit()
{
	if (code)
	{
		FreeCode(code, CodeArenaSize);
		code = nullptr;
	}
}

void* Jit::AllocCode(size_t size)
{
#if defined(_WINDOWS)
	return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
	void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	return (p == MAP_FAILED) ? nullptr : p;
#endif
}

void Jit::FreeCode(void* ptr, size_t size)
{
#if defined(_WINDOWS)
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	munmap(ptr, size);
#endif
}

void Jit::InvalidateAll()
{
	// O(1): every entry with an older generation is simply ignored. The arena is
	// reused from the beginning only when it is exhausted.
	stats.jitInvalidations++;
	generation++;
	if (generation == 0)
	{
		generation = 1;
	}
}

// Look a block up. The ways are filled round-robin, which is enough: once every
// block of a set is resident no further eviction happens.
//
// Both the effective address *and* the physical address it translated to at compile
// time have to match. That is what makes a block safe against a translation change
// (MSR[IR], the BATs, SDR1, the page tables) even if some future code path forgets
// to invalidate the cache.
Jit::Block* Jit::FindBlock(uint32_t pc, uint32_t pa)
{
	Block* set = blocks[(pc >> 2) & BlockCacheMask];

	for (size_t w = 0; w < BlockCacheWays; w++)
	{
		if (set[w].gen == generation && set[w].pc == pc && set[w].pa == pa)
		{
			return &set[w];
		}
	}

	return nullptr;
}

Jit::Block* Jit::AllocBlock(uint32_t pc, uint32_t pa)
{
	size_t s = (pc >> 2) & BlockCacheMask;
	size_t w = nextWay[s];
	nextWay[s] = (uint8_t)((w + 1) & (BlockCacheWays - 1));

	Block* block = &blocks[s][w];
	block->pc = pc;
	block->pa = pa;
	block->instrCount = 0;
	block->codeOffset = 0;

	return block;
}

// ---------------------------------------------------------------------------

// The emulated instruction fetch, without the exception side effects.
//
// Compilation is refused for anything that is not read through the emulated
// instruction cache: such code is coherent with the data path (a store is
// visible to the next fetch without an icbi), so a compiled block of it could go
// stale without any invalidation event. Keeping the interpreter for those
// regions preserves the emulator's behaviour exactly.
bool Jit::FetchInstr(uint32_t pc, uint32_t& instr, uint32_t& pa)
{
	int WIMG = 0;
	pa = core->EffectiveToPhysical(pc, MmuAccess::Execute, WIMG);

	// The instruction word has to come out of the emulated cache: outside its range
	// Cache::ReadWord leaves the destination untouched, and compiling a stale word
	// would be far worse than executing the interpreter.
	if (pa == BadAddress || pa >= PI_MEMSPACE_BOOTROM || core->icache->GetCachePointer(pa) == nullptr ||
		!core->icache->IsEnabled() || (WIMG & WIMG_I) != 0)
	{
		pa = BadAddress;
		return false;
	}

	core->icache->ReadWord(pa, &instr);
	return true;
}

static bool IsBranchInstr(Instruction instr)
{
	switch (instr)
	{
	case Instruction::b: case Instruction::ba: case Instruction::bl: case Instruction::bla:
	case Instruction::bc: case Instruction::bca: case Instruction::bcl: case Instruction::bcla:
	case Instruction::bclr: case Instruction::bclrl: case Instruction::bcctr: case Instruction::bcctrl:
		return true;
	default:
		return false;
	}
}

// ---------------------------------------------------------------------------
// The translator

uint32_t Jit::CompileBlock(uint32_t pc, uint32_t pa, uint32_t& instrCount)
{
	uint64_t compileStart = cycleProfile ? ReadCycleCounter() : 0;

	if (code == nullptr)
	{
		return BadAddress;
	}

	if (codeUsed + 65536 > CodeArenaSize)
	{
		// Start the arena over. Every live block is dropped along with it.
		InvalidateAll();
		codeUsed = 0;
		return BadAddress;
	}


	// Fetch the first instruction before emitting anything: a block whose entry
	// cannot be compiled (the boot ROM, an unmapped address) must not cost more than
	// the one failed fetch.
	{
		uint32_t probe = 0, probePa = 0;
		if (!FetchInstr(pc, probe, probePa) || probePa != pa)
		{
			return BadAddress;
		}
	}

	X86::Emitter e(code + codeUsed, CodeArenaSize - codeUsed);
	size_t start = codeUsed;

	auto gprOff = [](uint32_t n) { return GprOff + (int32_t)n * 4; };

	auto loadGpr = [&](uint8_t host, uint32_t n) { e.mov_r32_m(host, RegRegs, gprOff(n)); };
	auto storeGpr = [&](uint32_t n, uint8_t host) { e.mov_m32_r(RegRegs, gprOff(n), host); };

	// COMPUTE_CR0, identical to the interpreter macro (including the SO copy).
	auto emitCR0 = [&](uint8_t src)
	{
		// All three condition bits come from one comparison, so the registers have to
		// be zeroed without touching the flags (`mov`, not `xor`). 32-bit mode has three
		// scratch registers and three bits to keep, so one of them goes into AH
		// (setcc_ah / movzx_r32_ah, see jit_x86.h).
		e.test_r32_r32(src, src);
		e.mov_r32_imm(X86::EAX, 0);
		e.mov_r32_imm(X86::ECX, 0);
		e.mov_r32_imm(X86::EDX, 0);
		e.setcc_ah(X86::CcS);
		e.setcc_r8(X86::CcG, X86::ECX);
		e.setcc_r8(X86::CcE, X86::EDX);
		e.movzx_r32_ah(X86::EAX);
		e.shl_r32_imm(X86::EAX, 31);
		e.shl_r32_imm(X86::ECX, 30);
		e.shl_r32_imm(X86::EDX, 29);
		e.or_r32_r32(X86::EAX, X86::ECX);
		e.or_r32_r32(X86::EAX, X86::EDX);

		e.mov_r32_m(X86::ECX, RegRegs, XerOff);
		e.and_r32_imm(X86::ECX, GEKKO_XER_SO);
		e.shr_r32_imm(X86::ECX, 3);						// bit 31 -> bit 28
		e.or_r32_r32(X86::EAX, X86::ECX);

		e.mov_r32_m(X86::ECX, RegRegs, CrOff);
		e.and_r32_imm(X86::ECX, 0x0fff'ffff);
		e.or_r32_r32(X86::ECX, X86::EAX);
		e.mov_m32_r(RegRegs, CrOff, X86::ECX);
	};

	// XER[CA] = carry of the previous ALU op / = 0.
	//
	// EDX and EBP are the two scratch registers the ALU translations leave free while
	// they still hold a result: addic keeps it in EAX and subfic in ECX.
	auto emitSetCa = [&]()
	{
		e.setcc_r8(X86::CcB, X86::EDX);
		e.movzx_r32_r8(X86::EDX, X86::EDX);
		e.shl_r32_imm(X86::EDX, 29);
		e.mov_r32_m(T3, RegRegs, XerOff);
		e.and_r32_imm(T3, ~(uint32_t)GEKKO_XER_CA);
		e.or_r32_r32(T3, X86::EDX);
		e.mov_m32_r(RegRegs, XerOff, T3);
	};

	// XER[CA] = the *inverted* carry of the previous ALU op. subfic computes
	// rD = SIMM - rA as an x86 `sub`, whose CF is the borrow, while the emulator
	// takes the carry out of ~rA + SIMM + 1 - the two are complements.
	auto emitSetCaInverted = [&]()
	{
		e.setcc_r8(X86::CcAE, X86::EDX);
		e.movzx_r32_r8(X86::EDX, X86::EDX);
		e.shl_r32_imm(X86::EDX, 29);
		e.mov_r32_m(T3, RegRegs, XerOff);
		e.and_r32_imm(T3, ~(uint32_t)GEKKO_XER_CA);
		e.or_r32_r32(T3, X86::EDX);
		e.mov_m32_r(RegRegs, XerOff, T3);
	};

	auto emitResetCa = [&]()
	{
		e.mov_r32_m(X86::EAX, RegRegs, XerOff);
		e.and_r32_imm(X86::EAX, ~(uint32_t)GEKKO_XER_CA);
		e.mov_m32_r(RegRegs, XerOff, X86::EAX);
	};

	// Effective address into EAX. D-form when 'imm' is given, otherwise indexed.
	auto emitEffectiveAddress = [&](uint32_t ra, uint32_t rb, bool indexed, bool zeroRa, int32_t imm)
	{
		if (zeroRa)
		{
			e.xor_r32_r32_same(X86::EAX);
		}
		else
		{
			loadGpr(X86::EAX, ra);
		}

		if (indexed)
		{
			e.alu_r32_m(X86::AluAdd, X86::EAX, RegRegs, gprOff(rb));
		}
		else
		{
			e.add_r32_imm(X86::EAX, (uint32_t)imm);
		}
	};

	auto emitExcCheck = [&](std::vector<size_t>& excJumps)
	{
		e.cmp_m8_imm(RegCore, exceptionOffset, 0);
		excJumps.push_back(e.jcc_rel32(X86::CcNE));
	};

	// The branch condition of a bc / bclr / bcctr, inlined for the forms that are a plain
	// CR test or a plain CTR decrement - which is all a compiler emits in practice, and in
	// particular the `bdnz`/`blt` of a loop and the `bclr` of a return. The general BO/BI
	// combination still goes through BcTest / BctrTest.
	//
	// The BO fields are the ones Interpreter::BcTest decodes: bit 4 leaves CR alone, bit 3
	// takes the branch when CR[BI] is set, bit 2 leaves CTR alone, bit 1 takes it when CTR
	// is zero. `ctrBranch` is false for bcctr / bcctrl, whose test (Interpreter::BctrTest)
	// ignores CTR completely - it is not decremented there - so only the CR half is inlined
	// and a set bit 4 means "always taken". Returns the offset of a rel32 that lands on the
	// not-taken path, -1 when the branch is unconditional, or -2 when the caller has to
	// emit the helper call.
	auto emitBranchCondition = [&](uint32_t bo, uint32_t bi, bool ctrBranch) -> int64_t
	{
		if (!ctrBranch)
		{
			if (bo & 0x10)
			{
				return -1;								// CR ignored: always taken
			}
			e.test_m32_imm(RegRegs, CrOff, 0x8000'0000u >> bi);
			return (int64_t)e.jcc_rel32((bo & 0x08) ? X86::CcE : X86::CcNE);
		}

		if ((bo & 0x14) == 0x14)
		{
			return -1;									// CR and CTR both ignored
		}
		if ((bo & 0x14) == 0x10)
		{
			e.sub_m32_imm8(RegRegs, CtrOff, 1);			// bdnz / bdz
			return (int64_t)e.jcc_rel32((bo & 0x02) ? X86::CcNE : X86::CcE);
		}
		if ((bo & 0x14) == 0x04)
		{
			e.test_m32_imm(RegRegs, CrOff, 0x8000'0000u >> bi);
			return (int64_t)e.jcc_rel32((bo & 0x08) ? X86::CcE : X86::CcNE);
		}
		return -2;
	};

	//
	// Prologue
	//

	// cdecl entry: [esp] is the return address, [esp+4] the GekkoCore*, [esp+8] the
	// GekkoRegs* and [esp+12] the JitExit*. The four callee-saved registers take the
	// core/register-file pointer, the pc and the retired-instruction counter.
	e.push_r32(X86::EBX);
	e.push_r32(X86::ESI);
	e.push_r32(X86::EDI);
	e.push_r32(X86::EBP);
	e.sub_esp_imm8(FrameSize);

	e.mov_r32_m(RegCore, X86::ESP, Arg0Slot);
	e.mov_r32_m(RegPc, RegRegs, PcOff);
	e.xor_r32_r32_same(RegCount);

	// The frame slots a self-looping block uses (see emitBackEdge).
	e.mov_m32_imm(X86::ESP, LoopTicksSlot, 0);
	e.mov_m32_imm(X86::ESP, LoopBudgetSlot, LoopBudget);

	// Where a back edge has to return to: the first instruction of the block, after the
	// prologue, so that the loop body is not re-entered through the saves.
	size_t bodyStart = e.pos;

	std::vector<size_t> normalExits;		// jump to the normal epilogue
	std::vector<size_t> takenExits;			// jump to the taken branch epilogue
	std::vector<size_t> exceptionExits;		// jump to the exception epilogue

	// A taken branch that targets the start of the block it is in is translated as an
	// internal back edge instead of a block exit: the loop body is already in this block,
	// so jumping back to it keeps the loop out of the block cache and the dispatcher. The
	// budget bounds how many iterations run before the block leaves, so the deferred time
	// base update (and with it the Flipper-side scheduling) stays as fine grained as the
	// basic-block cut was designed to keep it, and `LoopTicksSlot` counts the taken
	// branches so that Run() can tick them exactly as the interpreter does.
	auto emitBackEdge = [&](uint32_t targetPc)
	{
		e.mov_r32_imm(RegPc, targetPc);
		e.add_m32_imm8(X86::ESP, LoopTicksSlot, 1);
		e.sub_m32_imm8(X86::ESP, LoopBudgetSlot, 1);

		size_t again = e.jcc_rel32(X86::CcNE);
		takenExits.push_back(e.jmp_rel32());		// budget exhausted: leave with pc = target
		e.patch32(again, e.rel(again));

		size_t back = e.jmp_rel32();
		e.patch32(back, (uint32_t)((int64_t)bodyStart - (int64_t)(back + 4)));
	};

	uint32_t curPc = pc;
	uint32_t count = 0;
	bool endBlock = false;

	while (count < MaxBlockInstrs && !endBlock)
	{
		uint32_t instr = 0, instrPa = 0;

		if (!FetchInstr(curPc, instr, instrPa))
		{
			break;							// let the interpreter raise the fetch exception
		}

		DecoderInfo di;
		Decoder::DecodeFast(curPc, instr, &di);

		// Instructions that the block cannot contain:
		// - rfi / sc change pc in ways that are not worth modelling,
		// - mftb / mfspr / mtspr of the time base and the decrementer would observe
		//   the deferred tick update of this block, so they are executed by the
		//   interpreter with the tick already applied.
		if (di.instr == Instruction::rfi || di.instr == Instruction::sc ||
			di.instr == Instruction::mftb ||
			(di.instr == Instruction::mfspr &&
				(di.paramBits[1] == SPR::TBL || di.paramBits[1] == SPR::TBU || di.paramBits[1] == SPR::DEC)) ||
			(di.instr == Instruction::mtspr &&
				(di.paramBits[0] == SPR::TBL || di.paramBits[0] == SPR::TBU || di.paramBits[0] == SPR::DEC)))
		{
			break;
		}

		e.inc_r32(RegCount);
		count++;

		uint32_t rd = (uint32_t)di.paramBits[0];
		uint32_t ra = (uint32_t)di.paramBits[1];
		uint32_t rb = (uint32_t)di.paramBits[2];
		bool handled = true;
		bool pcAdvanced = false;			// the fallback handler already advanced regs.pc

		switch (di.instr)
		{
		// ---- integer add / subtract -------------------------------------

		case Instruction::addi:
		case Instruction::addis:
		{
			int32_t imm = (di.instr == Instruction::addi)
				? (int32_t)di.Imm.Signed
				: ((int32_t)di.Imm.Signed << 16);

			if (ra) loadGpr(T0, ra); else e.xor_r32_r32_same(T0);
			e.add_r32_imm(T0, (uint32_t)imm);
			storeGpr(rd, T0);
			break;
		}

		case Instruction::add:
		case Instruction::add_d:
			loadGpr(T0, ra);
			e.alu_r32_m(X86::AluAdd, T0, RegRegs, gprOff(rb));
			storeGpr(rd, T0);
			if (di.instr == Instruction::add_d) emitCR0(T0);
			break;

		case Instruction::subf:
		case Instruction::subf_d:
			loadGpr(T0, rb);
			e.alu_r32_m(X86::AluSub, T0, RegRegs, gprOff(ra));
			storeGpr(rd, T0);
			if (di.instr == Instruction::subf_d) emitCR0(T0);
			break;

		case Instruction::neg:
		case Instruction::neg_d:
			loadGpr(T0, ra);
			e.neg_r32(T0);
			storeGpr(rd, T0);
			if (di.instr == Instruction::neg_d) emitCR0(T0);
			break;

		case Instruction::mulli:
			loadGpr(T0, ra);
			e.mov_r32_imm(T1, (uint32_t)di.Imm.Signed);
			e.imul_r32_r32(T0, T1);
			storeGpr(rd, T0);
			break;

		case Instruction::mullw:
		case Instruction::mullw_d:
			loadGpr(T0, ra);
			loadGpr(T1, rb);
			e.imul_r32_r32(T0, T1);
			storeGpr(rd, T0);
			if (di.instr == Instruction::mullw_d) emitCR0(T0);
			break;

		case Instruction::addic:
		case Instruction::addic_d:
			loadGpr(T0, ra);
			e.add_r32_imm(T0, (uint32_t)di.Imm.Signed);
			emitSetCa();
			storeGpr(rd, T0);
			if (di.instr == Instruction::addic_d) emitCR0(T0);
			break;

		case Instruction::subfic:
			// rd = ~ra + SIMM + 1, XER[CA]
			loadGpr(T0, ra);
			e.mov_r32_imm(T1, (uint32_t)di.Imm.Signed);
			e.sub_r32_r32(T1, T0);
			emitSetCaInverted();
			storeGpr(rd, T1);
			break;

		// ---- logical ----------------------------------------------------

		case Instruction::_or:
		case Instruction::or_d:
			loadGpr(T0, ra);
			e.alu_r32_m(X86::AluOr, T0, RegRegs, gprOff(rb));
			storeGpr(rd, T0);
			if (di.instr == Instruction::or_d) emitCR0(T0);
			break;

		case Instruction::_and:
		case Instruction::and_d:
			loadGpr(T0, ra);
			e.alu_r32_m(X86::AluAnd, T0, RegRegs, gprOff(rb));
			storeGpr(rd, T0);
			if (di.instr == Instruction::and_d) emitCR0(T0);
			break;

		case Instruction::_xor:
		case Instruction::xor_d:
			loadGpr(T0, ra);
			e.alu_r32_m(X86::AluXor, T0, RegRegs, gprOff(rb));
			storeGpr(rd, T0);
			if (di.instr == Instruction::xor_d) emitCR0(T0);
			break;

		case Instruction::andc:
		case Instruction::andc_d:
			loadGpr(T0, rb);
			e.not_r32(T0);
			e.alu_r32_m(X86::AluAnd, T0, RegRegs, gprOff(ra));
			storeGpr(rd, T0);
			if (di.instr == Instruction::andc_d) emitCR0(T0);
			break;

		case Instruction::orc:
		case Instruction::orc_d:
			loadGpr(T0, rb);
			e.not_r32(T0);
			e.alu_r32_m(X86::AluOr, T0, RegRegs, gprOff(ra));
			storeGpr(rd, T0);
			if (di.instr == Instruction::orc_d) emitCR0(T0);
			break;

		case Instruction::nand:
		case Instruction::nand_d:
			loadGpr(T0, ra);
			e.alu_r32_m(X86::AluAnd, T0, RegRegs, gprOff(rb));
			e.not_r32(T0);
			storeGpr(rd, T0);
			if (di.instr == Instruction::nand_d) emitCR0(T0);
			break;

		case Instruction::nor:
		case Instruction::nor_d:
			loadGpr(T0, ra);
			e.alu_r32_m(X86::AluOr, T0, RegRegs, gprOff(rb));
			e.not_r32(T0);
			storeGpr(rd, T0);
			if (di.instr == Instruction::nor_d) emitCR0(T0);
			break;

		case Instruction::eqv:
		case Instruction::eqv_d:
			loadGpr(T0, ra);
			e.alu_r32_m(X86::AluXor, T0, RegRegs, gprOff(rb));
			e.not_r32(T0);
			storeGpr(rd, T0);
			if (di.instr == Instruction::eqv_d) emitCR0(T0);
			break;

		case Instruction::ori:
			loadGpr(T0, ra);
			e.alu_r32_imm(X86::AluOr, T0, di.Imm.Unsigned);
			storeGpr(rd, T0);
			break;

		case Instruction::oris:
			loadGpr(T0, ra);
			e.alu_r32_imm(X86::AluOr, T0, (uint32_t)di.Imm.Unsigned << 16);
			storeGpr(rd, T0);
			break;

		case Instruction::xori:
			loadGpr(T0, ra);
			e.alu_r32_imm(X86::AluXor, T0, di.Imm.Unsigned);
			storeGpr(rd, T0);
			break;

		case Instruction::xoris:
			loadGpr(T0, ra);
			e.alu_r32_imm(X86::AluXor, T0, (uint32_t)di.Imm.Unsigned << 16);
			storeGpr(rd, T0);
			break;

		case Instruction::andi_d:
			loadGpr(T0, ra);
			e.alu_r32_imm(X86::AluAnd, T0, di.Imm.Unsigned);
			storeGpr(rd, T0);
			emitCR0(T0);
			break;

		case Instruction::andis_d:
			loadGpr(T0, ra);
			e.alu_r32_imm(X86::AluAnd, T0, (uint32_t)di.Imm.Unsigned << 16);
			storeGpr(rd, T0);
			emitCR0(T0);
			break;

		case Instruction::extsb:
		case Instruction::extsb_d:
			loadGpr(T0, ra);
			e.movsx_r32_r8(T0, T0);
			storeGpr(rd, T0);
			if (di.instr == Instruction::extsb_d) emitCR0(T0);
			break;

		case Instruction::extsh:
		case Instruction::extsh_d:
			loadGpr(T0, ra);
			e.shl_r32_imm(T0, 16);
			e.sar_r32_imm(T0, 16);
			storeGpr(rd, T0);
			if (di.instr == Instruction::extsh_d) emitCR0(T0);
			break;

		case Instruction::cntlzw:
		case Instruction::cntlzw_d:
		{
			// cntlzw(0) is 32, cntlzw(x) = 31 - bsr(x).
			e.mov_r32_m(T0, RegRegs, gprOff(ra));		// source
			e.mov_r32_imm(T1, 32);
			e.mov_r32_r32(T2, T0);
			e.test_r32_r32(T2, T2);
			size_t zero = e.jcc_rel32(X86::CcE);
			e.mov_r32_imm(T1, 31);
			e.bsr_r32_r32(T3, T2);
			e.sub_r32_r32(T1, T3);
			e.patch32(zero, e.rel(zero));
			storeGpr(rd, T1);
			if (di.instr == Instruction::cntlzw_d) emitCR0(T1);
			break;
		}

		// ---- compare ----------------------------------------------------

		case Instruction::cmpi:
		case Instruction::cmpli:
		case Instruction::cmp:
		case Instruction::cmpl:
		{
			uint32_t crfd = (uint32_t)di.paramBits[0];
			bool isSigned = (di.instr == Instruction::cmpi || di.instr == Instruction::cmp);
			bool isImm = (di.instr == Instruction::cmpi || di.instr == Instruction::cmpli);

			if (isImm)
			{
				loadGpr(T0, ra);
				e.mov_r32_imm(T1, isSigned ? (uint32_t)di.Imm.Signed : (uint32_t)di.Imm.Unsigned);
			}
			else
			{
				loadGpr(T0, ra);
				loadGpr(T1, rb);
			}

			// The two operands are dead once the comparison has been made, so their
			// registers take the three condition bits (one of them in AH - see emitCR0).
			e.cmp_r32_r32(T0, T1);

			e.mov_r32_imm(X86::EAX, 0);
			e.mov_r32_imm(X86::ECX, 0);
			e.mov_r32_imm(X86::EDX, 0);
			e.setcc_ah(isSigned ? X86::CcL : X86::CcB);
			e.setcc_r8(isSigned ? X86::CcG : X86::CcA, X86::ECX);
			e.setcc_r8(X86::CcE, X86::EDX);

			e.movzx_r32_ah(X86::EAX);
			e.shl_r32_imm(X86::EAX, 3);
			e.shl_r32_imm(X86::ECX, 2);
			e.shl_r32_imm(X86::EDX, 1);
			e.or_r32_r32(X86::EAX, X86::ECX);
			e.or_r32_r32(X86::EAX, X86::EDX);

			e.mov_r32_m(T3, RegRegs, XerOff);
			e.shr_r32_imm(T3, 31);						// XER[SO] -> bit 0
			e.or_r32_r32(X86::EAX, T3);

			int32_t shift = 28 - 4 * (int32_t)crfd;
			if (shift) e.shl_r32_imm(X86::EAX, (uint8_t)shift);
			e.mov_r32_m(T3, RegRegs, CrOff);
			e.and_r32_imm(T3, ~(0xfu << shift));
			e.or_r32_r32(T3, X86::EAX);
			e.mov_m32_r(RegRegs, CrOff, T3);
			break;
		}

		// ---- rotate -----------------------------------------------------

		case Instruction::rlwinm:
		case Instruction::rlwinm_d:
		case Instruction::rlwnm:
		case Instruction::rlwnm_d:
		case Instruction::rlwimi:
		case Instruction::rlwimi_d:
		{
			uint32_t mb = (uint32_t)di.paramBits[3], me = (uint32_t)di.paramBits[4];
			uint32_t mask = ((uint32_t)-1 >> mb) ^ ((me >= 31) ? 0 : ((uint32_t)-1) >> (me + 1));
			if (mb > me) mask = ~mask;

			loadGpr(T0, ra);

			if (di.instr == Instruction::rlwinm || di.instr == Instruction::rlwinm_d)
			{
				if (di.paramBits[2]) e.rol_r32_imm(T0, (uint8_t)di.paramBits[2]);
			}
			else if (di.instr == Instruction::rlwnm || di.instr == Instruction::rlwnm_d)
			{
				loadGpr(X86::ECX, rb);
				e.and_r32_imm(X86::ECX, 0x1f);
				e.rol_r32_cl(T0);
			}
			else	// rlwimi
			{
				loadGpr(T1, rd);
				e.and_r32_imm(T1, ~mask);
				if (di.paramBits[2]) e.rol_r32_imm(T0, (uint8_t)di.paramBits[2]);
				e.alu_r32_imm(X86::AluAnd, T0, mask);
				e.or_r32_r32(T0, T1);
				storeGpr(rd, T0);
				if (di.instr == Instruction::rlwimi_d) emitCR0(T0);
				break;
			}

			e.alu_r32_imm(X86::AluAnd, T0, mask);
			storeGpr(rd, T0);
			if (di.instr == Instruction::rlwinm_d || di.instr == Instruction::rlwnm_d) emitCR0(T0);
			break;
		}

		// ---- shifts -----------------------------------------------------

		case Instruction::slw:
		case Instruction::slw_d:
		case Instruction::srw:
		case Instruction::srw_d:
		{
			// The architecture returns 0 when the shift count has bit 5 set.
			loadGpr(T0, ra);
			loadGpr(T1, rb);
			e.mov_r32_imm(T2, 0);
			e.test_r32_imm(T1, 0x20);
			size_t big = e.jcc_rel32(X86::CcNE);
			e.mov_r32_r32(X86::ECX, T1);
			e.and_r32_imm(X86::ECX, 0x1f);
			if (di.instr == Instruction::slw || di.instr == Instruction::slw_d)
				e.shl_r32_cl(T0);
			else
				e.shr_r32_cl(T0);
			e.mov_r32_r32(T2, T0);
			e.patch32(big, e.rel(big));
			storeGpr(rd, T2);
			if (di.instr == Instruction::slw_d || di.instr == Instruction::srw_d) emitCR0(T2);
			break;
		}

		case Instruction::srawi:
		case Instruction::srawi_d:
		{
			uint32_t n = (uint32_t)di.paramBits[2];
			loadGpr(T0, ra);

			if (n == 0)
			{
				e.mov_r32_r32(T1, T0);
				emitResetCa();
			}
			else
			{
				// res = src >> n ; CA = (src < 0) && ((src << (32-n)) != 0)
				//
				// T1 holds the result and has to survive, so the shifted copy and the
				// flag byte both go through T2 and the CA bit through T3.
				e.mov_r32_r32(T1, T0);
				e.sar_r32_imm(T1, (uint8_t)n);
				e.mov_r32_r32(T2, T0);
				e.shl_r32_imm(T2, (uint8_t)(32 - n));
				e.test_r32_r32(T2, T2);					// ZF = (shifted == 0)
				e.mov_r32_imm(T2, 0);
				e.setcc_r8(X86::CcNE, X86::EDX);		// DL = (shifted != 0)
				e.mov_r32_r32(T3, T0);
				e.shr_r32_imm(T3, 31);					// src < 0
				e.and_r32_r32(T3, X86::EDX);
				e.shl_r32_imm(T3, 29);					// -> XER[CA]

				e.mov_r32_m(T2, RegRegs, XerOff);
				e.and_r32_imm(T2, ~(uint32_t)GEKKO_XER_CA);
				e.or_r32_r32(T2, T3);
				e.mov_m32_r(RegRegs, XerOff, T2);
			}

			storeGpr(rd, T1);
			if (di.instr == Instruction::srawi_d) emitCR0(T1);
			break;
		}

		// ---- loads ------------------------------------------------------

		case Instruction::lbz: case Instruction::lbzu: case Instruction::lbzx: case Instruction::lbzux:
		case Instruction::lhz: case Instruction::lhzu: case Instruction::lhzx: case Instruction::lhzux:
		case Instruction::lha: case Instruction::lhau: case Instruction::lhax: case Instruction::lhaux:
		case Instruction::lwz: case Instruction::lwzu: case Instruction::lwzx: case Instruction::lwzux:
		{
			bool indexed = (di.instr == Instruction::lbzx || di.instr == Instruction::lbzux ||
				di.instr == Instruction::lhzx || di.instr == Instruction::lhzux ||
				di.instr == Instruction::lhax || di.instr == Instruction::lhaux ||
				di.instr == Instruction::lwzx || di.instr == Instruction::lwzux);
			bool update = (di.instr == Instruction::lbzu || di.instr == Instruction::lbzux ||
				di.instr == Instruction::lhzu || di.instr == Instruction::lhzux ||
				di.instr == Instruction::lhau || di.instr == Instruction::lhaux ||
				di.instr == Instruction::lwzu || di.instr == Instruction::lwzux);
			bool arithmetic = (di.instr == Instruction::lha || di.instr == Instruction::lhau ||
				di.instr == Instruction::lhax || di.instr == Instruction::lhaux);
			bool zeroRa = !update && ra == 0;

			emitEffectiveAddress(ra, rb, indexed, zeroRa, (int32_t)di.Imm.Signed);
			e.mov_m32_r(X86::ESP, EaSlot, X86::EAX);

			// regs.pc has to be current before the helper runs: an access fault makes
			// the helper raise the exception, which stores regs.pc into SRR0.
			e.mov_m32_r(RegRegs, PcOff, RegPc);

			uint32_t fn = 0;
			switch (di.instr)
			{
			case Instruction::lbz: case Instruction::lbzu: case Instruction::lbzx: case Instruction::lbzux:
				fn = (uint32_t)(uintptr_t)&JitReadByte; break;
			case Instruction::lhz: case Instruction::lhzu: case Instruction::lhzx: case Instruction::lhzux:
			case Instruction::lha: case Instruction::lhau: case Instruction::lhax: case Instruction::lhaux:
				fn = (uint32_t)(uintptr_t)&JitReadHalf; break;
			default:
				fn = (uint32_t)(uintptr_t)&JitReadWord; break;
			}

			e.lea_r32_m(T2, RegRegs, gprOff(rd));		// arg2: &gpr[rd]
			e.push_r32(T2);
			e.push_r32(X86::EAX);						// arg1: the effective address
			e.push_r32(RegCore);						// arg0
			e.call_abs(fn);
			e.add_esp_imm8(12);

			emitExcCheck(exceptionExits);

			// lha/lhau/lhax/lhaux sign-extend the halfword the helper read zero-extended.
			// The extension is the interpreter's own expression - `if (rd & 0x8000)
			// rd |= 0xffff0000` - and not a movsx, because the two differ when the helper
			// does not write rd at all: Cache::ReadHalf returns without touching it when
			// the physical address is past the cache, and the interpreter then leaves the
			// *whole* old register in place (movsx would truncate it to the old low half).
			// The random-instruction fuzzer in testing/gekko_bench found exactly that as a
			// rare disagreement, which is why this family used to stay on the interpreter.
			if (arithmetic)
			{
				loadGpr(T0, rd);
				e.test_r32_imm(T0, 0x8000);
				size_t noSign = e.jcc_rel32(X86::CcE);
				e.alu_r32_imm(X86::AluOr, T0, 0xffff'0000);
				e.patch32(noSign, e.rel(noSign));
				storeGpr(rd, T0);
			}

			if (update)
			{
				e.mov_r32_m(T0, X86::ESP, EaSlot);
				storeGpr(ra, T0);
			}
			break;
		}

		// ---- stores -----------------------------------------------------

		case Instruction::stb: case Instruction::stbu: case Instruction::stbx: case Instruction::stbux:
		case Instruction::sth: case Instruction::sthu: case Instruction::sthx: case Instruction::sthux:
		case Instruction::stw: case Instruction::stwu: case Instruction::stwx: case Instruction::stwux:
		{
			bool indexed = (di.instr == Instruction::stbx || di.instr == Instruction::stbux ||
				di.instr == Instruction::sthx || di.instr == Instruction::sthux ||
				di.instr == Instruction::stwx || di.instr == Instruction::stwux);
			bool update = (di.instr == Instruction::stbu || di.instr == Instruction::stbux ||
				di.instr == Instruction::sthu || di.instr == Instruction::sthux ||
				di.instr == Instruction::stwu || di.instr == Instruction::stwux);
			bool zeroRa = !update && ra == 0;

			emitEffectiveAddress(ra, rb, indexed, zeroRa, (int32_t)di.Imm.Signed);
			e.mov_m32_r(X86::ESP, EaSlot, X86::EAX);

			// regs.pc has to be current before the helper runs: an access fault makes
			// the helper raise the exception, which stores regs.pc into SRR0.
			e.mov_m32_r(RegRegs, PcOff, RegPc);

			uint32_t fn = 0;
			switch (di.instr)
			{
			case Instruction::stb: case Instruction::stbu: case Instruction::stbx: case Instruction::stbux:
				fn = (uint32_t)(uintptr_t)&JitWriteByte; break;
			case Instruction::sth: case Instruction::sthu: case Instruction::sthx: case Instruction::sthux:
				fn = (uint32_t)(uintptr_t)&JitWriteHalf; break;
			default:
				fn = (uint32_t)(uintptr_t)&JitWriteWord; break;
			}

			loadGpr(T2, rd);							// arg2: the data
			e.push_r32(T2);
			e.push_r32(X86::EAX);						// arg1: the effective address
			e.push_r32(RegCore);						// arg0
			e.call_abs(fn);
			e.add_esp_imm8(12);

			emitExcCheck(exceptionExits);

			if (update)
			{
				e.mov_r32_m(T0, X86::ESP, EaSlot);
				storeGpr(ra, T0);
			}
			break;
		}

		// ---- branches ---------------------------------------------------

		case Instruction::b:
		case Instruction::ba:
			// The interpreter runs BranchCheck for an unconditional branch as well.
			e.mov_r32_imm(RegPc, di.Imm.Address);
			takenExits.push_back(e.jmp_rel32());
			endBlock = true;
			break;

		case Instruction::bl:
		case Instruction::bla:
			e.mov_r32_imm(T0, curPc + 4);
			e.mov_m32_r(RegRegs, LrOff, T0);
			e.mov_r32_imm(RegPc, di.Imm.Address);
			takenExits.push_back(e.jmp_rel32());
			endBlock = true;
			break;

		case Instruction::bc:
		case Instruction::bca:
		case Instruction::bcl:
		case Instruction::bcla:
		{
			uint32_t bo = (uint32_t)di.paramBits[0];
			uint32_t bi = (uint32_t)di.paramBits[1];
			int64_t notTaken = emitBranchCondition(bo, bi, true);

			if (notTaken == -2)
			{
				e.push_imm32(bi);						// arg2
				e.push_imm32(bo);						// arg1
				e.push_r32(RegCore);					// arg0
				e.call_abs((uint32_t)(uintptr_t)&Jit::BcTest);
				e.add_esp_imm8(12);
				e.movzx_r32_r8(X86::EAX, X86::EAX);		// bool comes back in AL only
				e.test_r32_r32(X86::EAX, X86::EAX);
				notTaken = (int64_t)e.jcc_rel32(X86::CcE);
			}

			if (di.instr == Instruction::bcl || di.instr == Instruction::bcla)
			{
				e.mov_r32_imm(T0, curPc + 4);
				e.mov_m32_r(RegRegs, LrOff, T0);
			}

			if (di.Imm.Address == pc && count > 1)
			{
				// A branch back to this block's own first instruction: the loop body is in
				// this block already, so keep the iteration here and leave only when the
				// budget runs out.
				emitBackEdge(di.Imm.Address);
				endBlock = true;
			}
			else
			{
				e.mov_r32_imm(RegPc, di.Imm.Address);
				takenExits.push_back(e.jmp_rel32());

				// A conditional branch does not end the block: the not-taken path is the
				// next instruction and belongs to the same straight-line run. That is what
				// lets a loop whose body contains a conditional branch - the search loops
				// the audio driver spends its time in are written that way - be compiled in
				// one piece, so that the backward branch at its end can become a back edge
				// instead of a block exit. An unconditional BO leaves nothing to fall into.
				if (notTaken == -1)
				{
					endBlock = true;
				}
			}

			if (notTaken >= 0)
			{
				e.patch32((size_t)notTaken, e.rel((size_t)notTaken));
			}
			e.add_r32_imm(RegPc, 4);				// not taken: pc += 4
			break;
		}

		case Instruction::bclr:
		case Instruction::bclrl:
		{
			uint32_t bo = (uint32_t)di.paramBits[0];
			uint32_t bi = (uint32_t)di.paramBits[1];
			int64_t notTaken = emitBranchCondition(bo, bi, true);

			if (notTaken == -2)
			{
				e.push_imm32(bi);						// arg2
				e.push_imm32(bo);						// arg1
				e.push_r32(RegCore);					// arg0
				e.call_abs((uint32_t)(uintptr_t)&Jit::BcTest);
				e.add_esp_imm8(12);
				e.movzx_r32_r8(X86::EAX, X86::EAX);
				e.test_r32_r32(X86::EAX, X86::EAX);
				notTaken = (int64_t)e.jcc_rel32(X86::CcE);
			}

			// The target is the *old* LR: it has to be read before the new one is
			// written (bclrl is how the IPL2 entry code calls subroutines).
			e.mov_r32_m(RegPc, RegRegs, LrOff);
			e.and_r32_imm(RegPc, ~3u);

			if (di.instr == Instruction::bclrl)
			{
				e.mov_r32_imm(T0, curPc + 4);
				e.mov_m32_r(RegRegs, LrOff, T0);
			}

			takenExits.push_back(e.jmp_rel32());

			if (notTaken >= 0)
			{
				e.patch32((size_t)notTaken, e.rel((size_t)notTaken));
			}
			e.add_r32_imm(RegPc, 4);
			endBlock = true;
			break;
		}

		case Instruction::bcctr:
		case Instruction::bcctrl:
		{
			uint32_t bo = (uint32_t)di.paramBits[0];
			uint32_t bi = (uint32_t)di.paramBits[1];
			int64_t notTaken = emitBranchCondition(bo, bi, false);

			if (notTaken == -2)
			{
				e.push_imm32(bi);						// arg2
				e.push_imm32(bo);						// arg1
				e.push_r32(RegCore);					// arg0
				e.call_abs((uint32_t)(uintptr_t)&Jit::BctrTest);
				e.add_esp_imm8(12);
				e.movzx_r32_r8(X86::EAX, X86::EAX);
				e.test_r32_r32(X86::EAX, X86::EAX);
				notTaken = (int64_t)e.jcc_rel32(X86::CcE);
			}

			if (di.instr == Instruction::bcctrl)
			{
				e.mov_r32_imm(T0, curPc + 4);
				e.mov_m32_r(RegRegs, LrOff, T0);
			}

			e.mov_r32_m(RegPc, RegRegs, CtrOff);
			e.and_r32_imm(RegPc, ~3u);
			takenExits.push_back(e.jmp_rel32());

			if (notTaken >= 0)
			{
				e.patch32((size_t)notTaken, e.rel((size_t)notTaken));
			}
			e.add_r32_imm(RegPc, 4);
			endBlock = true;
			break;
		}

		default:
			// Paired-Single instructions live in their own module (gekkojit_ps.cpp)
			// so that the SSE translations can be built out with GEKKO_JIT_PS=0.
			// When it declines the instruction, the interpreter runs it below.
			switch (JitPs::Translate(e, core, di))
			{
			case JitPs::PsResult::NotHandled:
				handled = false;
				break;
			case JitPs::PsResult::DoneMayExcept:
				// The helper may have raised a memory exception, so the block must
				// not run another instruction before the check.
				emitExcCheck(exceptionExits);
				break;
			default:
				break;
			}
			break;
		}

		if (!handled)
		{
			// Run this one instruction through the interpreter. The handler owns its
			// exact semantics (including the timing side effects of mfspr/mtspr).
			e.mov_m32_r(RegRegs, PcOff, RegPc);
			e.push_imm32(curPc);						// arg2
			e.push_imm32(instr);						// arg1
			e.push_r32(RegCore);						// arg0
			e.call_abs((uint32_t)(uintptr_t)&Jit::Fallback);
			e.add_esp_imm8(12);
			e.mov_r32_m(RegPc, RegRegs, PcOff);
			emitExcCheck(exceptionExits);
			pcAdvanced = true;
		}

		if (!IsBranchInstr(di.instr) && !pcAdvanced)
		{
			e.add_r32_imm(RegPc, 4);
		}

		curPc += 4;
	}

	if (count == 0 || e.overflow)
	{
		if (e.overflow)
		{
			InvalidateAll();
			codeUsed = 0;
		}
		return BadAddress;
	}

	//
	// Exit sequences. Three epilogues share the same body but differ in what they
	// leave behind: the pc store and the recorded exit kind.
	//

	auto emitFinish = [&](JitExitKind kind, bool storePc)
	{
		if (storePc)
		{
			e.mov_m32_r(RegRegs, PcOff, RegPc);
		}

		// JitExit { uint64 count; uint32 kind; uint32 ticks; }: the counter is 32 bit,
		// so the upper half is zeroed explicitly.
		e.mov_r32_m(T0, X86::ESP, Arg2Slot);
		e.mov_m32_r(T0, 0, RegCount);
		e.mov_m32_imm(T0, 4, 0);
		e.mov_m32_imm(T0, 8, (uint32_t)kind);
		e.mov_r32_m(T1, X86::ESP, LoopTicksSlot);
		e.mov_m32_r(T0, 12, T1);
	};

	auto emitEpilogue = [&]()
	{
		e.add_esp_imm8(FrameSize);
		e.pop_r32(X86::EBP);
		e.pop_r32(X86::EDI);
		e.pop_r32(X86::ESI);
		e.pop_r32(X86::EBX);
		e.ret();
	};

	// Normal fallthrough / unconditional branch.
	size_t normalJump = e.jmp_rel32();
	size_t normalBody = e.pos;
	e.patch32(normalJump, e.rel(normalJump));
	emitFinish(JitExitKind::Normal, true);
	emitEpilogue();

	// A taken conditional branch: BranchCheck is still pending.
	size_t takenJump = e.jmp_rel32();
	size_t takenBody = e.pos;
	e.patch32(takenJump, e.rel(takenJump));
	emitFinish(JitExitKind::TakenBranch, true);
	emitEpilogue();

	// An exception was raised: the helper has already set regs.pc to the vector.
	size_t exceptionBody = e.pos;
	emitFinish(JitExitKind::Exception, false);
	emitEpilogue();

	for (size_t at : normalExits) e.patch32(at, (uint32_t)(normalBody - (at + 4)));
	for (size_t at : takenExits) e.patch32(at, (uint32_t)(takenBody - (at + 4)));
	for (size_t at : exceptionExits) e.patch32(at, (uint32_t)(exceptionBody - (at + 4)));

	if (e.overflow)
	{
		InvalidateAll();
		codeUsed = 0;
		return BadAddress;
	}

	uint32_t offset = (uint32_t)start;
	codeUsed = start + e.pos;
	instrCount = count;

	Block* block = AllocBlock(pc, pa);
	block->gen = generation;
	block->instrCount = count;
	block->codeOffset = offset;

	stats.jitCompiles++;
	stats.compileCycles += compileStart ? (ReadCycleCounter() - compileStart) : 0;

	return offset;
}

// Every path of Run() that hands the instruction back to the interpreter goes through here, so
// that the CPU statistics can tell translated code and interpreted code apart.
void Jit::RunOneInterpreted()
{
	stats.interpInstrs++;
	CycleScope cycles(&stats.interpCycles);
	core->interp->ExecuteOpcode();
}

void Jit::Run()
{
	CycleScope cycles(&stats.jitRunCycles);
	RunInner();
}

void Jit::RunInner()
{
	if (!supported)
	{
		RunOneInterpreted();
		return;
	}

	uint32_t pc = core->regs.pc;

	// The boot ROM is fetched straight from the PI and never through the instruction
	// cache, so there is nothing to compile for it - and no reason to walk the block
	// cache for every instruction of it either.
	if (pc >= PI_MEMSPACE_BOOTROM)
	{
		RunOneInterpreted();
		return;
	}

	if (pc == 0x81300000)
	{
		core->cache->FlashInvalidate();
	}

	// Without the instruction cache the emulated fetch is coherent with the data
	// path, which a compiled block cannot model (see FetchInstr).
	if (!core->icache->IsEnabled())
	{
		RunOneInterpreted();
		return;
	}

	int WIMG = 0;
	uint32_t pa = core->EffectiveToPhysical(pc, MmuAccess::Execute, WIMG);
	if (pa == BadAddress || pa >= PI_MEMSPACE_BOOTROM || core->icache->GetCachePointer(pa) == nullptr ||
		(WIMG & WIMG_I) != 0)
	{
		RunOneInterpreted();
		return;
	}

	Block* block = FindBlock(pc, pa);

	if (block == nullptr)
	{
		uint32_t count = 0;
		if (CompileBlock(pc, pa, count) == BadAddress)
		{
			RunOneInterpreted();
			return;
		}
		block = FindBlock(pc, pa);
		if (block == nullptr)
		{
			RunOneInterpreted();
			return;
		}
	}

	typedef void (*BlockFn)(GekkoCore*, GekkoRegs*, JitExit*);

	JitExit exit{};
	BlockFn fn = (BlockFn)(void*)(code + block->codeOffset);
	{
		CycleScope cycles(&stats.blockCallCycles);
		fn(core, &core->regs, &exit);
	}

	uint32_t n = (uint32_t)exit.count;

	stats.jitBlocks++;
	stats.jitInstrs += n;

	// `exit.ticks` is the number of taken back edges the block retired on top of its
	// instructions. The interpreter ticks every taken branch twice (once in BranchCheck and
	// once in the instruction loop), so a block that looped on its own back edge owes one
	// extra tick per iteration even though it left the dispatcher only once.
	uint32_t ticks = n + exit.ticks;

	// Mirror Interpreter::ExecuteOpcode: an instruction that raises an exception
	// does not advance the tick, and a taken branch is ticked once by BranchCheck
	// and once more by the instruction loop.
	switch ((JitExitKind)exit.kind)
	{
	case JitExitKind::Exception:
		if (ticks > 0) core->TickN(ticks - 1);
		break;

	case JitExitKind::TakenBranch:
		if (ticks > 0) core->TickN(ticks);
		interp->BranchCheck();
		break;

	default:
		if (ticks > 0) core->TickN(ticks);
		break;
	}

	core->ops += n;

	if (core->resetInstructionCounter)
	{
		core->resetInstructionCounter = false;
		core->ops = 0;
	}

	if (core->exception)
	{
		core->exception = false;
	}
}

}

#endif
