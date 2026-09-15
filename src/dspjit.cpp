/*

DSPcore -> x86-64 basic block recompiler. See dspjit.h for the design notes.

A compiled block is a plain function

	uint32_t block(DspCore* core, DspInterpreter* interp)

that retires whole DSP instruction words and returns how many. Its body is a straight
sequence of direct calls: one per word (two for a parallel word). The trampoline runs the
handler and then `DspInterpreter::JitCommit`, the non-opcode part of `Dispatch`, and
returns the new pc. The decoded instruction is baked into the call (the trampoline is a
template instantiation over the handler method), so the recursion-heavy decoder and the
dispatch switch only run once per block, at compile time.

*/

#include "pch.h"
#include "dspjit.h"

#if DSP_JIT_SUPPORTED

#include "jit_x64.h"

#if defined(_WINDOWS)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace DSP
{

namespace
{
	// ---------------------------------------------------------------------------
	// Host register roles and stack frame.
	//
	// Registers the C++ ABI treats as callee-saved (RBX, RBP, R12-R15) survive a helper
	// call, so the block's live state lives there. The frame reserves the 32 byte Win64
	// shadow space that a callee may use and keeps rsp 16-byte aligned at every call: entry
	// rsp is 8 mod 16, the eight pushes keep that, so the frame size has to be 8 mod 16.

	static const uint8_t RegCore = X64::R15;		// DspCore*, kept for the whole block
	static const uint8_t RegInterp = X64::R14;		// DspInterpreter*, kept for the whole block
	static const uint8_t RegPc = X64::R13;			// 32 bit pc, kept for the whole block
	static const uint8_t RegCount = X64::RBX;		// words retired in this block

	static const uint8_t T0 = X64::R8;				// scratch (never live across a call)
	static const uint8_t T1 = X64::R9;
	static const uint8_t T2 = X64::R10;
	static const uint8_t T3 = X64::RAX;

	static const int32_t ShadowSpace = 32;
	static const int32_t FrameSize = 40;
	static_assert(FrameSize >= ShadowSpace, "the frame must cover the Win64 shadow space");
	static_assert(FrameSize % 16 == 8, "rsp must be 16 byte aligned at the helper calls");

	using BlockFn = uint32_t (*)(DspCore*, DspInterpreter*);
}

}		// namespace DSP

// The trampoline declarations live in the header; the definitions are member templates of
// Jit, so the instantiations are the only things that take the address of a private
// DspInterpreter handler.
namespace DSP
{

template <auto Fn>
uint32_t Jit::RegularTrampoline(DspInterpreter* interp, DecoderInfo* info, uint32_t pc)
{
	interp->info = info;
	interp->flowControl = info->flowControl;
	(interp->*Fn)();
	interp->core->instructionCounter++;
	return DspInterpreter::JitCommit(interp, pc);
}

template <auto Fn>
void Jit::ParallelUpperTrampoline(DspInterpreter* interp, DecoderInfo* info)
{
	interp->info = info;
	interp->flowControl = info->flowControl;
	interp->LatchPackedMemoryOperand();
	(interp->*Fn)();
}

template <auto Fn>
uint32_t Jit::ParallelLowerTrampoline(DspInterpreter* interp, uint32_t pc)
{
	(interp->*Fn)();
	// One instruction, not two: see the note in DspInterpreter::Dispatch (issue #394).
	interp->core->instructionCounter++;
	return DspInterpreter::JitCommit(interp, pc);
}

uint32_t Jit::NopTrampoline(DspInterpreter* interp, DecoderInfo* info, uint32_t pc)
{
	interp->info = info;
	interp->flowControl = info->flowControl;
	interp->core->instructionCounter++;
	return DspInterpreter::JitCommit(interp, pc);
}

void Jit::TraceWord(DspInterpreter* interp, uint32_t pc)
{
	TraceStep(pc, 1);
}

void Jit::ParallelNopUpperTrampoline(DspInterpreter* interp, DecoderInfo* info)
{
	interp->info = info;
	interp->flowControl = info->flowControl;
	interp->LatchPackedMemoryOperand();
}

uint32_t Jit::ParallelNopLowerTrampoline(DspInterpreter* interp, uint32_t pc)
{
	interp->core->instructionCounter++;
	return DspInterpreter::JitCommit(interp, pc);
}

// ---------------------------------------------------------------------------
// The instruction -> trampoline tables. They are built once; `Unknown` stays null and
// ends the block (the word is then interpreted, which owns its exact behaviour).

Jit::WordThunk Jit::RegularThunkFor(DspRegularInstruction instr)
{
	static WordThunk table[(size_t)DspRegularInstruction::btsth + 1] = { nullptr };
	static bool init = false;

	if (!init)
	{
		init = true;
#define REG(name, method) table[(size_t)DspRegularInstruction::name] = &Jit::RegularTrampoline<&DspInterpreter::method>
		REG(jmp, jmp);
		REG(call, call);
		REG(rets, rets);
		REG(reti, reti);
		REG(trap, trap);
		REG(wait, wait);
		REG(exec, exec);
		REG(loop, loop);
		REG(rep, rep);
		REG(pld, pld);
		table[(size_t)DspRegularInstruction::nop] = &Jit::NopTrampoline;
		REG(mr, mr);
		REG(adsi, adsi);
		REG(adli, adli);
		REG(cmpsi, cmpsi);
		REG(cmpli, cmpli);
		REG(lsfi, lsfi);
		REG(asfi, asfi);
		REG(xorli, xorli);
		REG(anli, anli);
		REG(orli, orli);
		REG(norm, norm);
		REG(div, div);
		REG(addc, addc);
		REG(subc, subc);
		REG(negc, negc);
		REG(max, _max);
		REG(lsf, lsf);
		REG(asf, asf);
		REG(ld, ld);
		REG(st, st);
		REG(ldsa, ldsa);
		REG(stsa, stsa);
		REG(ldla, ldla);
		REG(stla, stla);
		REG(mv, mv);
		REG(mvsi, mvsi);
		REG(mvli, mvli);
		REG(stli, stli);
		REG(clr, clr);
		REG(set, set);
		REG(btstl, btstl);
		REG(btsth, btsth);
#undef REG
	}

	if ((int)instr < 0) return nullptr;
	return table[(size_t)instr];
}

Jit::ParallelThunk Jit::ParallelUpperThunkFor(DspParallelInstruction instr)
{
	static ParallelThunk table[(size_t)DspParallelInstruction::asf + 1] = { nullptr };
	static bool init = false;

	if (!init)
	{
		init = true;
#define PAR(name, method) table[(size_t)DspParallelInstruction::name] = &Jit::ParallelUpperTrampoline<&DspInterpreter::method>
		PAR(add, p_add);
		PAR(addl, p_addl);
		PAR(sub, p_sub);
		PAR(amv, p_amv);
		PAR(cmp, p_cmp);
		PAR(inc, p_inc);
		PAR(dec, p_dec);
		PAR(abs, p_abs);
		PAR(neg, p_neg);
		PAR(clr, p_clr);
		PAR(rnd, p_rnd);
		PAR(rndp, p_rndp);
		PAR(tst, p_tst);
		PAR(lsl16, p_lsl16);
		PAR(lsr16, p_lsr16);
		PAR(asr16, p_asr16);
		PAR(addp, p_addp);
		table[(size_t)DspParallelInstruction::nop] = &Jit::ParallelNopUpperTrampoline;
		PAR(set, p_set);
		PAR(mpy, p_mpy);
		PAR(mac, p_mac);
		PAR(macn, p_macn);
		PAR(mvmpy, p_mvmpy);
		PAR(rnmpy, p_rnmpy);
		PAR(admpy, p_admpy);
		PAR(_not, p_not);
		PAR(_xor, p_xor);
		PAR(_and, p_and);
		PAR(_or, p_or);
		PAR(lsf, p_lsf);
		PAR(asf, p_asf);
#undef PAR
	}

	if ((int)instr < 0) return nullptr;
	return table[(size_t)instr];
}

Jit::ParallelMemThunk Jit::ParallelLowerThunkFor(DspParallelMemInstruction instr)
{
	static ParallelMemThunk table[(size_t)DspParallelMemInstruction::nop + 1] = { nullptr };
	static bool init = false;

	if (!init)
	{
		init = true;
#define PMEM(name, method) table[(size_t)DspParallelMemInstruction::name] = &Jit::ParallelLowerTrampoline<&DspInterpreter::method>
		PMEM(ldd, p_ldd);
		PMEM(ls, p_ls);
		PMEM(ld, p_ld);
		PMEM(st, p_st);
		PMEM(mv, p_mv);
		PMEM(mr, p_mr);
		table[(size_t)DspParallelMemInstruction::nop] = &Jit::ParallelNopLowerTrampoline;
#undef PMEM
	}

	if ((int)instr < 0) return nullptr;
	return table[(size_t)instr];
}

// A word can be compiled when every handler it needs is in the tables.
bool Jit::ThunkAvailable(const DecoderInfo& info)
{
	if (!info.parallel)
	{
		return RegularThunkFor(info.instr) != nullptr;
	}

	return ParallelUpperThunkFor(info.parallelInstr) != nullptr &&
		ParallelLowerThunkFor(info.parallelMemInstr) != nullptr;
}

// ---------------------------------------------------------------------------

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

Jit::Jit(DspCore* parent)
{
	core = parent;
	interp = parent->interp;

	// Compute the offsets the generated code addresses from a live instance, so that a class
	// layout change shows up as a wrong offset (and a failing test) rather than as silently
	// wrong code.
	corePcOffset = (int32_t)((uint8_t*)&core->regs.pc - (uint8_t*)core);
	pendingOffset = (int32_t)((uint8_t*)&core->intr.pendingSomething - (uint8_t*)core);
	dspOffset = (int32_t)((uint8_t*)&core->dsp - (uint8_t*)core);
	traceWords = getenv("DSP_TRACE_WORDS") != nullptr;
	cpuIntOffset = (core->dsp != nullptr)
		? (int32_t)((uint8_t*)core->dsp->JitCpuIntRequestFlag() - (uint8_t*)core->dsp)
		: -1;
	jitGenerationOffset = (int32_t)((uint8_t*)&core->jitGeneration - (uint8_t*)core);

	code = (uint8_t*)AllocCode(CodeArenaSize);
	supported = (code != nullptr);

	// The tables are built on the first lookup; do it now so that a missing entry is a
	// compile-time fact rather than a surprise on some code path.
	for (int i = 0; i <= (int)DspRegularInstruction::btsth; i++)
	{
		RegularThunkFor((DspRegularInstruction)i);
	}
	for (int i = 0; i <= (int)DspParallelInstruction::asf; i++)
	{
		ParallelUpperThunkFor((DspParallelInstruction)i);
	}
	for (int i = 0; i <= (int)DspParallelMemInstruction::nop; i++)
	{
		ParallelLowerThunkFor((DspParallelMemInstruction)i);
	}
}

Jit::~Jit()
{
	if (code != nullptr)
	{
		FreeCode(code, CodeArenaSize);
		code = nullptr;
	}
}

void Jit::SetMaxBlockInstrs(uint32_t count)
{
	maxBlockInstrs = (count < 1) ? 1 : (count > MaxBlockInstrs ? MaxBlockInstrs : count);
	InvalidateAll();
}

void Jit::InvalidateAll()
{
	// O(1): every entry with an older generation is simply ignored. The arena is reused from
	// the beginning only when it is exhausted.
	generation++;
	if (generation == 0)
	{
		generation = 1;
	}

	// The running block watches this one: bumping it makes the generated code leave at the next
	// word, so a DMA that rewrites instruction memory while a block executes cannot be missed.
	if (core != nullptr)
	{
		core->jitGeneration++;
	}
}

Jit::Block* Jit::FindBlock(uint32_t pc)
{
	Block* set = blocks[(pc >> 1) & (BlockCacheSets - 1)];
	for (size_t w = 0; w < BlockCacheWays; w++)
	{
		if (set[w].gen == generation && set[w].pc == pc)
		{
			return &set[w];
		}
	}
	return nullptr;
}

Jit::Block* Jit::AllocBlock(uint32_t pc)
{
	size_t index = (pc >> 1) & (BlockCacheSets - 1);
	size_t way = nextWay[index];
	nextWay[index] = (uint8_t)((way + 1) & (BlockCacheWays - 1));

	Block* b = &blocks[index][way];
	b->pc = pc;
	b->gen = generation;
	b->wordCount = 0;
	b->instrCount = 0;
	b->codeOffset = 0;
	b->infosOffset = 0;
	return b;
}

// The backstop for a missed invalidation: a block only runs while the words it was compiled
// from are still there. Comparing a few halfwords per entry costs less than the recompile it
// avoids on a changed line.
bool Jit::VerifyBlock(const Block* b)
{
	uint8_t* base = core->TranslateIMem(b->pc);
	if (base == nullptr)
	{
		return false;
	}

	const uint16_t* mem = (const uint16_t*)base;
	for (uint32_t i = 0; i < b->wordCount; i++)
	{
		if (_BYTESWAP_UINT16(mem[i]) != b->words[i])
		{
			return false;
		}
	}

	return true;
}

void* Jit::CompileBlock(uint32_t pc)
{
	if (code == nullptr)
	{
		return nullptr;
	}

	size_t arenaMark = codeUsed;

	if (codeUsed + 64 * 1024 > CodeArenaSize)
	{
		// Start the arena over. Every live block is dropped along with it.
		InvalidateAll();
		codeUsed = 0;
		arenaMark = 0;
		return nullptr;
	}

	uint32_t regionEnd = 0;
	if (pc < 0x1000)					// IRAM
	{
		regionEnd = 0x1000;
	}
	else if (pc >= 0x8000 && pc < 0x9000)	// IROM
	{
		regionEnd = 0x9000;
	}
	else
	{
		return nullptr;
	}

	//
	// Pass 1: decode the block. Two words is the longest instruction, so a block never
	// touches the last halfword of a memory (where a second word would read past the end).
	//

	DecoderInfo infos[Jit::MaxBlockInstrs];
	uint16_t words[Jit::MaxBlockWords];
	uint32_t instrPc[Jit::MaxBlockInstrs];
	uint32_t count = 0;
	uint32_t wordCount = 0;
	uint32_t curPc = pc;

	while (count < maxBlockInstrs && curPc + 2 <= regionEnd)
	{
		uint8_t* p = core->TranslateIMem(curPc);
		if (p == nullptr)
		{
			break;
		}

		DecoderInfo di = { 0 };
		Decoder::Decode(p, DspCore::MaxInstructionSizeInBytes, di);

		uint32_t sizeWords = (uint32_t)(di.sizeInBytes >> 1);
		if (sizeWords == 0 || curPc + sizeWords > regionEnd)
		{
			break;
		}

		if (!ThunkAvailable(di))
		{
			// The interpreter owns the semantics of a word the recompiler has no handler for
			// (including the Halt on an undefined opcode), so the block has to stop in front of
			// it. If it is the first word there is nothing to compile at all.
			break;
		}

		infos[count] = di;
		instrPc[count] = curPc;
		for (uint32_t k = 0; k < sizeWords; k++)
		{
			words[wordCount + k] = core->ReadIMem(curPc + k);
		}
		wordCount += sizeWords;
		count++;
		curPc += sizeWords;

		if (di.flowControl)
		{
			break;
		}
	}

	if (count == 0)
	{
		codeUsed = arenaMark;
		return nullptr;
	}

	//
	// Pass 2: reserve the decoded instructions in the arena (the generated code points at
	// them) and emit the code after them.
	//

	codeUsed = (codeUsed + 7) & ~(size_t)7;
	size_t infosStart = codeUsed;
	codeUsed += sizeof(DecoderInfo) * count;
	if (codeUsed + 4096 > CodeArenaSize)
	{
		InvalidateAll();
		codeUsed = 0;
		return nullptr;
	}
	DecoderInfo* arenaInfos = (DecoderInfo*)(code + infosStart);
	memcpy(arenaInfos, infos, sizeof(DecoderInfo) * count);

	X64::Emitter e(code + codeUsed, CodeArenaSize - codeUsed);
	size_t codeStart = codeUsed;

	//
	// Prologue
	//

	e.push_r64(X64::RBX);
	e.push_r64(X64::RBP);
	e.push_r64(X64::RSI);
	e.push_r64(X64::RDI);
	e.push_r64(X64::R12);
	e.push_r64(X64::R13);
	e.push_r64(X64::R14);
	e.push_r64(X64::R15);
	e.sub_rsp_imm8(FrameSize);

	e.mov_r64_r64(RegCore, Arg0);
	e.mov_r64_r64(RegInterp, Arg1);
	e.mov_r32_m(RegPc, RegCore, corePcOffset);
	e.xor_r32_r32_same(RegCount);

	std::vector<size_t> bailJumps;		// leave the block when an interrupt is pending
	bool lastFlow = false;
	const uint32_t expectedGeneration = core->jitGeneration;

	//
	// The words
	//

	for (uint32_t i = 0; i < count; i++)
	{
		const DecoderInfo& di = infos[i];

		// The block keeps the pc in a register; a control-transfer handler reads the pc out of
		// the core (a call pushes the return address, trap/wait/loop read it), so it has to be
		// current in memory before the handler runs. Only the block-ending flow-control words
		// need this - no data-path handler reads the pc.
		if (di.flowControl)
		{
			e.mov_m32_r(RegCore, corePcOffset, RegPc);
		}

		if (traceWords)
		{
			e.mov_r64_r64(Arg0, RegInterp);
			e.mov_r64_r64(Arg1, RegPc);
			e.call_abs((uint64_t)(void*)&Jit::TraceWord);
		}

		if (!di.parallel)
		{
			// regular trampoline(interp, info, pc) -> new pc (also retires the counter and
			// applies the repeat/loop rules)
			e.mov_r64_r64(Arg0, RegInterp);
			e.mov_r64_imm(Arg1, (uint64_t)(void*)&arenaInfos[i]);
			e.mov_r64_r64(Arg2, RegPc);
			e.call_abs((uint64_t)(void*)RegularThunkFor(di.instr));
		}
		else
		{
			e.mov_r64_r64(Arg0, RegInterp);
			e.mov_r64_imm(Arg1, (uint64_t)(void*)&arenaInfos[i]);
			e.call_abs((uint64_t)(void*)ParallelUpperThunkFor(di.parallelInstr));

			// lower trampoline(interp, pc) -> new pc (retires the cycle and commits)
			e.mov_r64_r64(Arg0, RegInterp);
			e.mov_r64_r64(Arg1, RegPc);
			e.call_abs((uint64_t)(void*)ParallelLowerThunkFor(di.parallelMemInstr));
		}

		e.inc_r64(RegCount);

		lastFlow = di.flowControl;

		if (!di.flowControl)
		{
			e.mov_r32_r32(RegPc, X64::RAX);

			// A block is a straight-line run, but the instruction advance is not always to the
			// next word: `rep` keeps the pc on the same instruction until its count is drained,
			// and the end address of a `loop` sends it back to the loop start. When the pc that
			// DspInterpreter::JitCommit produced is not the next instruction of this block,
			// leave - the next RunJitBlock dispatches at whatever the pc really is.
			if (i + 1 < count)
			{
				e.alu_r32_imm(X64::AluCmp, RegPc, instrPc[i + 1]);
				bailJumps.push_back(e.jcc_rel32(X64::CcNE));
			}

			// The instruction stream can change under a running block: the DSP-DMA that uploads
			// the microcode into IRAM is triggered by an instruction *inside* the block, and the
			// words after it were compiled from the old contents. Leave as soon as the code
			// generation moved, so the words that follow are compiled from what is really there.
			e.cmp_m32_imm(RegCore, jitGenerationOffset, expectedGeneration);
			bailJumps.push_back(e.jcc_rel32(X64::CcNE));

			// Interrupts are checked before every instruction on the interpreter; here the block
			// leaves as soon as one is pending, so Update() runs CheckInterrupts before the next
			// word, exactly as it would have.
			e.cmp_m8_imm(RegCore, pendingOffset, 0);
			bailJumps.push_back(e.jcc_rel32(X64::CcNE));

			if (cpuIntOffset >= 0)
			{
				e.mov_r64_m(T0, RegCore, dspOffset);
				e.cmp_m8_imm(T0, cpuIntOffset, 0);
				bailJumps.push_back(e.jcc_rel32(X64::CcNE));
			}
		}
	}

	//
	// Exit sequences. A block that ended on a control transfer must not touch the pc - the
	// handler already stored it; every other exit (the normal end and the interrupt bail-out)
	// writes the pc the block kept in a register.
	//

	size_t flowJump = (size_t)-1;
	if (lastFlow)
	{
		flowJump = e.jmp_rel32();
	}

	size_t normalBody = e.pos;
	e.mov_m32_r(RegCore, corePcOffset, RegPc);
	e.mov_r64_r64(X64::RAX, RegCount);
	e.add_rsp_imm8(FrameSize);
	e.pop_r64(X64::R15);
	e.pop_r64(X64::R14);
	e.pop_r64(X64::R13);
	e.pop_r64(X64::R12);
	e.pop_r64(X64::RDI);
	e.pop_r64(X64::RSI);
	e.pop_r64(X64::RBP);
	e.pop_r64(X64::RBX);
	e.ret();

	size_t flowBody = e.pos;
	if (lastFlow)
	{
		// Patched here, while e.pos is exactly the body start: e.rel() is the offset from the
		// end of the rel32 field to the current position.
		e.patch32(flowJump, e.rel(flowJump));
	}
	e.mov_r64_r64(X64::RAX, RegCount);
	e.add_rsp_imm8(FrameSize);
	e.pop_r64(X64::R15);
	e.pop_r64(X64::R14);
	e.pop_r64(X64::R13);
	e.pop_r64(X64::R12);
	e.pop_r64(X64::RDI);
	e.pop_r64(X64::RSI);
	e.pop_r64(X64::RBP);
	e.pop_r64(X64::RBX);
	e.ret();

	for (size_t at : bailJumps)
	{
		e.patch32(at, (uint32_t)(normalBody - (at + 4)));
	}

	if (e.overflow)
	{
		codeUsed = arenaMark;
		return nullptr;
	}

	codeUsed = codeStart + e.pos;

	Block* block = AllocBlock(pc);
	block->wordCount = wordCount;
	block->instrCount = count;
	block->codeOffset = (uint32_t)codeStart;
	block->infosOffset = (uint32_t)infosStart;
	for (uint32_t i = 0; i < wordCount; i++)
	{
		block->words[i] = words[i];
	}

	return code + codeStart;
}

void Jit::DumpBlock(uint32_t pc)
{
	for (size_t s = 0; s < BlockCacheSets; s++)
	{
		for (size_t w = 0; w < BlockCacheWays; w++)
		{
			Block* b = &blocks[s][w];
			if (b->pc != pc || b->instrCount == 0)
			{
				continue;
			}

			Debug::Report(Debug::Channel::DSP, "DSPBLOCK pc=%04X gen=%u words=%u instrs=%u codeOff=%u infosOff=%u\n",
				b->pc, b->gen, b->wordCount, b->instrCount, b->codeOffset, b->infosOffset);

			std::string line = "DSPWORDS ";
			for (uint32_t i = 0; i < b->wordCount; i++)
			{
				char buf[8];
				sprintf(buf, "%04X ", b->words[i]);
				line += buf;
			}
			Debug::Report(Debug::Channel::DSP, "%s\n", line.c_str());

			const DecoderInfo* infos = (const DecoderInfo*)(code + b->infosOffset);
			for (uint32_t i = 0; i < b->instrCount; i++)
			{
				const DecoderInfo& d = infos[i];
				Debug::Report(Debug::Channel::DSP, "DSPINFO %u par=%d instr=%d pInstr=%d pMem=%d flow=%d size=%zu cc=%d npar=%zu imm=%04X\n",
					i, (int)d.parallel, d.parallel ? -1 : (int)d.instr,
					d.parallel ? (int)d.parallelInstr : -1, d.parallel ? (int)d.parallelMemInstr : -1,
					(int)d.flowControl, d.sizeInBytes, (int)d.cc, d.numParameters, d.ImmOperand.Address);
			}

			if (b->codeOffset + 64 <= CodeArenaSize)
			{
				std::string bytes = "DSPCODE ";
				for (uint32_t i = 0; i < 64; i++)
				{
					char buf[4];
					sprintf(buf, "%02X", code[b->codeOffset + i]);
					bytes += buf;
				}
				Debug::Report(Debug::Channel::DSP, "%s\n", bytes.c_str());
			}
			return;
		}
	}

	Debug::Report(Debug::Channel::DSP, "DSPBLOCK pc=%04X not found\n", pc);
}

uint32_t Jit::Run()
{
	if (!supported)
	{
		interp->ExecuteInstr();
		return 1;
	}

	uint32_t pc = core->regs.pc;

	Block* block = FindBlock(pc);

	if (block != nullptr && !VerifyBlock(block))
	{
		// The instructions changed without an invalidation: drop the entry and recompile.
		block->gen = 0;
		block = nullptr;
	}

	if (block == nullptr)
	{
		if (CompileBlock(pc) == nullptr)
		{
			// Not compilable (an unmapped pc, an undefined opcode): the interpreter owns it.
			interp->ExecuteInstr();
			return 1;
		}

		block = FindBlock(pc);
		if (block == nullptr)
		{
			interp->ExecuteInstr();
			return 1;
		}
	}

	BlockFn fn = (BlockFn)(void*)(code + block->codeOffset);
	return fn(core, interp);
}

}		// namespace DSP

#else	// !DSP_JIT_SUPPORTED

namespace DSP
{

Jit::Jit(DspCore* parent)
{
	core = parent;
	interp = parent->interp;
}

Jit::~Jit() {}
void Jit::SetMaxBlockInstrs(uint32_t count) {}
void Jit::InvalidateAll() {}

uint32_t Jit::Run()
{
	interp->ExecuteInstr();
	return 1;
}

}		// namespace DSP

#endif
