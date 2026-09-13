/*

# DSPcore basic block recompiler

The DSP interpreter spends most of its time on the fetch/decode/dispatch of a single
instruction: `DspInterpreter::ExecuteInstr` translates the pc to a memory pointer, runs
the recursive `Decoder::Decode` over the instruction word and then enters the big
`Dispatch` switch. The instruction handlers themselves are tiny (a 40-bit add, a move
between the register file and DMEM), so the surrounding machinery dominates.

This module compiles straight-line runs of DSP instruction words ("basic blocks") into
x86-64 machine code once and then reuses them.

## What is compiled and what is not

The recompiler does *not* reimplement the instruction semantics. Every decoded word is
turned into a direct call to the very handler the interpreter's `Dispatch` would call,
through a small trampoline that installs the word's `DecoderInfo` and reproduces the
non-opcode parts of `Dispatch` (the instruction counter and, in `DspInterpreter::
JitCommit`, the pc/repeat/loop advance). So the generated block is "threaded code": the
decode and the dispatch switch - the expensive part - are gone, while the semantics stay
in exactly one place (`dspcore.cpp`) and cannot drift.

A parallel word is two handlers in one cycle (`LatchPackedMemoryOperand`, then the ALU
half, then the memory half) and gets two trampolines, matching `Dispatch` exactly.

An instruction the decoder does not know, or one whose handler table entry is missing,
ends the block: the word is then executed by `DspInterpreter::ExecuteInstr`, which owns
its exact semantics (including the `Halt` the core raises for it).

## Correctness model

* A block is compiled from the words the emulated fetch returns at compile time, and the
  generated code bakes those words in (the `DecoderInfo` array lives in the code arena),
  so it must not run after the instruction stream changed. `InvalidateAll` is called from
  `DspCore::HardReset`, `LoadIrom`/`LoadDrom` and every DSP-DMA that wrote IMEM.

* As a backstop for any path that forgets to invalidate, the block carries a copy of the
  halfwords it was compiled from and `Jit::Run` re-checks them on every entry. A block
  whose words changed is simply recompiled - the DSP has no instruction cache, so there
  is no coherence state to model, just the memory contents.

* Interrupts are checked the way the interpreter checks them: before every instruction.
  A block runs under the same contract - after each word the generated code tests
  `intr.pendingSomething` and the latched CPU->DSP interrupt request, and leaves the
  block (letting `DspCore::Update` run `CheckInterrupts`) as soon as either is set. So an
  interrupt set by a handler is delivered before the next instruction, exactly as it is
  on the interpreter.

* The debug paths stay on the interpreter: `DspCore::Step`, breakpoints, canaries and the
  one-shot breakpoint all fall back (see `DspCore::RunJitBlock`).

## Switching it on

The recompiler is experimental and **off by default**: the emulator runs the interpreter
unless it is asked not to, so a plain run is never affected by this module. There are
three ways to turn it on:

* `--dspjit` on the command line,
* `dspjit 1` in the debugger (and `dspjit 0` back off, at runtime),
* `DspCore::JitEnabled = true` from code (this is what the tests and `dsp_bench` do).

`DspCore::JitEnabled` is the single switch; with it false `RunJitBlock` retires exactly one
instruction through the interpreter, so the two engines can be compared in one binary.
`-DDSP_JIT_DISABLED` removes the recompiler from the build entirely.

## x86-64 only

The recompiler needs x86-64 (the register roles and the calling convention are hard-coded
in `jit_x64.h`). On any other target `IsSupported()` returns false and the emulator runs
the interpreter exactly as before.

*/

#pragma once

#include "pch.h"
#include "dspdec.h"

// The recompiler is only built for 64-bit x86 hosts. DSP_JIT_DISABLED forces the
// interpreter-only build (used to check that path on a supported host).
#if (defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__)) && !defined(DSP_JIT_DISABLED)
#define DSP_JIT_SUPPORTED 1
#endif

namespace DSP
{
	class DspCore;
	class DspInterpreter;

	class Jit
	{
	public:
		/// <summary>
		/// Compiled blocks. The table is set associative on the block start address: a self
		/// looping program of a few thousand words holds a few hundred blocks, and a direct
		/// mapped table would evict and recompile them on every iteration.
		/// </summary>
		static const size_t BlockCacheSets = 512;
		static const size_t BlockCacheWays = 4;

		/// <summary>
		/// Machine code arena. When it fills up the whole cache is dropped and the arena is
		/// reused from the beginning.
		/// </summary>
		static const size_t CodeArenaSize = 4 * 1024 * 1024;

		/// <summary>
		/// A block is cut after this many words so that a single compiled run cannot hide an
		/// interrupt (or the emulated clock) for too long.
		/// </summary>
		static const uint32_t MaxBlockInstrs = 32;

		/// <summary>
		/// The most halfwords a block can cover (a two-word instruction needs two).
		/// </summary>
		static const uint32_t MaxBlockWords = MaxBlockInstrs * 2;

		struct Block
		{
			uint32_t pc;				// Start address (in halfwords)
			uint32_t gen;				// Generation the entry belongs to (0 = free)
			uint32_t wordCount;			// Halfwords covered by the block
			uint32_t instrCount;		// DSP instruction words the block retires
			uint32_t codeOffset;		// Offset of the machine code inside the arena
			uint32_t infosOffset;		// Offset of the DecoderInfo array inside the arena
			uint16_t words[MaxBlockWords];	// Verification copy of the words it was built from
		};

	private:
		DspCore* core = nullptr;
		DspInterpreter* interp = nullptr;

		uint8_t* code = nullptr;		// Executable arena
		size_t codeUsed = 0;
		bool supported = false;

		Block blocks[BlockCacheSets][BlockCacheWays];
		uint8_t nextWay[BlockCacheSets] = { 0 };

		// Invalidation is a generation bump rather than a walk over the table; an entry is live
		// only while its generation matches.
		uint32_t generation = 1;

		// How many words a single block may hold. Production uses MaxBlockInstrs; the
		// differential tests set it to 1 to compare one instruction at a time.
		uint32_t maxBlockInstrs = MaxBlockInstrs;

		// Development aid (DSP_TRACE_WORDS=1): emit a TraceWord call in front of every word, so
		// that the trace ring holds the pc of each individual instruction inside a block. It is
		// only compiled into the block when the environment asks for it.
		bool traceWords = false;

		// Byte offsets inside DspCore / Dsp16 that the generated code addresses. They are
		// computed from a live instance, so the class layouts can change without breaking the
		// recompiler silently.
		int32_t corePcOffset = 0;			// DspCore::regs.pc
		int32_t pendingOffset = 0;			// DspCore::intr.pendingSomething
		int32_t dspOffset = 0;				// DspCore::dsp
		int32_t cpuIntOffset = -1;			// Dsp16::intdspRequested
		int32_t jitGenerationOffset = 0;	// DspCore::jitGeneration

		void* AllocCode(size_t size);
		void FreeCode(void* ptr, size_t size);

		Block* FindBlock(uint32_t pc);
		Block* AllocBlock(uint32_t pc);
		bool VerifyBlock(const Block* b);

		// Compile the block that starts at `pc`. Returns the machine code entry point, or
		// nullptr when the address cannot be compiled (the caller then interprets one word).
		void* CompileBlock(uint32_t pc);

		// The trampolines the generated code calls. They are template instantiations over the
		// private handler methods, so a block word becomes one (regular) or two (parallel)
		// direct calls - no decode and no dispatch switch. The pc/repeat/loop advance is part
		// of the same call, which keeps the generated code to one indirect call per word.

		typedef uint32_t (*WordThunk)(DspInterpreter*, DecoderInfo*, uint32_t pc);
		typedef void (*ParallelThunk)(DspInterpreter*, DecoderInfo*);
		typedef uint32_t (*ParallelMemThunk)(DspInterpreter*, uint32_t pc);

		template <auto Fn> static uint32_t RegularTrampoline(DspInterpreter* interp, DecoderInfo* info, uint32_t pc);
		static uint32_t NopTrampoline(DspInterpreter* interp, DecoderInfo* info, uint32_t pc);

		template <auto Fn> static void ParallelUpperTrampoline(DspInterpreter* interp, DecoderInfo* info);
		static void ParallelNopUpperTrampoline(DspInterpreter* interp, DecoderInfo* info);

		template <auto Fn> static uint32_t ParallelLowerTrampoline(DspInterpreter* interp, uint32_t pc);
		static uint32_t ParallelNopLowerTrampoline(DspInterpreter* interp, uint32_t pc);

		static WordThunk RegularThunkFor(DspRegularInstruction instr);
		static ParallelThunk ParallelUpperThunkFor(DspParallelInstruction instr);
		static ParallelMemThunk ParallelLowerThunkFor(DspParallelMemInstruction instr);

		// Does every handler a word needs have a trampoline? A word that does not is left to
		// the interpreter (which also owns the Halt on an undefined opcode).
		static bool ThunkAvailable(const DecoderInfo& info);

		// Emitted in front of every word when traceWords is set (see CompileBlock).
		static void TraceWord(DspInterpreter* interp, uint32_t pc);

	public:
		Jit(DspCore* core);
		~Jit();

		bool IsSupported() const { return supported; }

		/// <summary>
		/// Change the block size limit. Only meaningful before the first block is compiled
		/// (the differential tests use a limit of 1); changing it drops the cache.
		/// </summary>
		void SetMaxBlockInstrs(uint32_t count);

		/// <summary>
		/// Development aid: report the compiled block that covers `pc` (its words, its decoded
		/// instructions and the first bytes of its code). Used by the trace dump when the core
		/// stops on a pc it cannot fetch.
		/// </summary>
		void DumpBlock(uint32_t pc);

		/// <summary>
		/// Drop every compiled block. Must be called whenever instruction memory can have
		/// changed; the per-entry word verification is only a backstop.
		/// </summary>
		void InvalidateAll();

		/// <summary>
		/// Run one basic block (or one interpreted instruction when the pc cannot be compiled).
		/// Returns the number of DSP instruction words retired.
		/// </summary>
		uint32_t Run();
	};
}
