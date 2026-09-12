/*

# Gekko basic block recompiler

The interpreter spends most of its time on the dispatch of a single instruction:
the indirect branch of the big `Dispatch` switch is taken once per emulated
instruction with an essentially unpredictable target, and a modern CPU pays the
full pipeline flush for it. Measured on a diverse synthetic workload, an
interpreter with the dispatch replaced by a perfect prediction runs ~3x faster,
so no amount of micro-optimisation of the interpreter itself can close the gap.

This module compiles straight-line runs of Gekko instructions ("basic blocks")
into x86-64 machine code once and then reuses them.

## What is compiled and what is not

Instructions that appear often and are simple (integer ALU, compare, rotate,
loads and stores, branches) are translated directly into machine code. Everything
else - the floating point and paired-single units, the system instructions, the
pair instructions with subtle flag behaviour - is *not* reimplemented: the block
calls back into the existing interpreter entry point for that one instruction.
That keeps the semantics in one place and makes the recompiler incrementally
extendable without ever being able to execute a wrong instruction.

Loads and stores are also not reimplemented - they call the same
`GekkoCore::ReadByte`/`WriteWord`/... helpers that the interpreter calls, so the
cache, the MMU, the write gather buffer and the DSI/ISI exceptions behave
identically.

## Correctness model

* A block is compiled from the instructions that the *emulated instruction fetch*
  returns at compile time, and the compiled code contains those instructions as
  constants, so it must be dropped as soon as the instruction stream at its
  address, or the address itself, can have changed. `InvalidateAll` is called from
  `icbi` (what a guest does after writing code), from a cache flash invalidate,
  from `mtspr` of BAT/SDR1/HID0/HID2, from `mtmsr`, from `rfi` and from
  `Exception` (both flip MSR[IR]/[DR]), from `tlbie`/`tlbsync` and from a CPU
  reset.

  Invalidation is a generation counter, not a walk over the table: a guest
  invalidates its caches one line at a time (`ICInvalidateRange` and friends, one
  `dcbst`+`icbi` per 32 bytes) and clearing 16K entries per line used to dominate
  the boot time. An entry is live only while its generation matches, and the code
  arena is restarted only when it runs out. `dcbi`/`dcbf`/`dcbst` deliberately do
  *not* invalidate: they do not change what the emulated instruction cache
  returns, so the interpreter keeps executing the old line until an `icbi` - and so
  does a compiled block.

* As a backstop for any path that forgets to invalidate, every lookup also compares
  the physical address the entry was compiled for. A block is used only when the pc
  *and* its translation match, so a translation change can never make the
  recompiler execute instructions the interpreter would not have executed.
* Exceptions raised by a helper set `regs.pc` themselves (to the exception
  vector). The generated code checks the flag after every operation that can
  fault and abandons the block without touching the pc.
* The instruction counter and the time base / decrementer are updated by the
  generated code at every exit, so the emulated timing is unchanged.
* Single stepping, breakpoints and the debugger keep using the interpreter:
  `GekkoCore::Step` and `GekkoThreadProc` fall back to it whenever breakpoints
  are armed.

## Switching it off

`GekkoCore::JitEnabled` (public, on by default) selects the engine; the debugger
command `jit 0` / `jit 1` toggles it at runtime and reports the current state.
Turning it off drops the compiled blocks and runs the plain interpreter.

## x86-64 only

The recompiler needs x86-64 (the register file and the calling convention are
hard-coded). On any other target `IsSupported()` returns false and the emulator
runs the interpreter exactly as before.

## What a generated block owes the C++ ABI

A block is ordinary code called from C++, and every memory access goes back out
through a C++ helper (`JitReadWord` and friends), so the block has to obey the
host calling convention exactly. The rules it follows:

* GPRs the C++ ABI treats as callee-saved are pushed in the prologue and popped
  in the epilogue, so the helpers may use them freely.
* Values that must survive a helper call (the effective address of an
  update-form access, the exit record pointer) are kept in callee-saved
  registers, never on the stack.
* Win64 is the subtle one: a callee may spill its four register arguments into
  the 32 bytes above the return address, so the caller must treat
  `[rsp, rsp+32)` as scratch and must not keep anything live there. SysV hosts
  have no such area, which means a violation is invisible on Linux and only
  shows up on Windows. The block therefore reserves 32 bytes of shadow space
  that it does not use, and keeps its own state in registers instead.
* Entry `rsp` is 8 mod 16 and the eight saved registers keep it that way, so the
  frame size is 8 mod 16; `rsp` is 16-byte aligned at every helper call on both
  conventions.

Build the ABI regression configuration (`-DGEKKO_JIT_TEST_WIN64_SHADOW`, see
`testing/gekko_bench`) to have the emitter poison `[rsp, rsp+32)` before every
helper call. That reproduces the Windows hazard on a Linux host, so a block that
breaks this contract fails the normal test suite there instead of only on the
user's machine.

*/

#pragma once

// The recompiler is only built for 64-bit x86 hosts. GEKKO_JIT_DISABLED forces the
// interpreter-only build (used to check that path on a supported host).
#if (defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__)) && !defined(GEKKO_JIT_DISABLED)
#define GEKKO_JIT_SUPPORTED 1
#endif

namespace Gekko
{
	class GekkoCore;
	class Interpreter;

	class Jit
	{
	public:
		// Compiled blocks. The table is set associative on (pc >> 2) because a
		// direct mapped table thrashes badly on code whose footprint is larger than
		// the table: the displaced blocks would then be recompiled over and over.
		static const size_t BlockCacheSets = 4096;
		static const size_t BlockCacheWays = 4;
		static const size_t BlockCacheMask = BlockCacheSets - 1;

		// Machine code arena. When it fills up the whole cache is dropped and the
		// arena is reused from the beginning.
		static const size_t CodeArenaSize = 32 * 1024 * 1024;

		// A basic block is cut after this many instructions so that the deferred
		// timer/decrementer update stays fine grained enough for the other threads.
#ifndef GEKKO_JIT_MAX_BLOCK
#define GEKKO_JIT_MAX_BLOCK 32
#endif
		static const uint32_t MaxBlockInstrs = GEKKO_JIT_MAX_BLOCK;

		struct Block
		{
			uint32_t pc;				// Effective address of the first instruction
			uint32_t pa;				// ... and its physical address at compile time
			uint32_t gen;				// Generation the entry belongs to
			uint32_t instrCount;
			uint32_t codeOffset;
		};

	private:
		GekkoCore* core = nullptr;
		Interpreter* interp = nullptr;

		uint8_t* code = nullptr;		// Executable arena
		size_t codeUsed = 0;
		bool codeFull = false;
		bool supported = false;

		Block blocks[BlockCacheSets][BlockCacheWays];
		uint8_t nextWay[BlockCacheSets] = { 0 };

		// Invalidation is a generation bump rather than a walk over the table: the
		// guest invalidates its caches one line at a time (ICInvalidateRange and
		// friends), and clearing 16K entries per line dominated the boot time.
		uint32_t generation = 1;

		// Byte offsets inside GekkoCore that the generated code addresses.
		int32_t exceptionOffset = 0;
		int32_t decreqOffset = 0;
		int32_t opsOffset = 0;
		int32_t resetCounterOffset = 0;

		// The emulated instruction fetch without the exception side effects, used at
		// compile time. Returns false when the address cannot be compiled (it does not
		// translate, it is the boot ROM, or the instruction cache is not in the path).
		bool FetchInstr(uint32_t pc, uint32_t& instr, uint32_t& pa);

		void* AllocCode(size_t size);
		void FreeCode(void* ptr, size_t size);

		uint32_t CompileBlock(uint32_t pc, uint32_t pa, uint32_t& instrCount);
		Block* FindBlock(uint32_t pc, uint32_t pa);
		Block* AllocBlock(uint32_t pc, uint32_t pa);

		// Run one instruction through the interpreter: every path of Run() that cannot use a
		// compiled block goes through here, so that the CPU statistics can tell translated code
		// and interpreted code apart.
		void RunOneInterpreted();

		// The body of Run(), so that Run() can wrap it in a cycle counter.
		void RunInner();

		// Called from generated code. Static members so that they inherit Jit's
		// friendship with GekkoCore / Interpreter.
		static void Fallback(GekkoCore* core, uint32_t instr, uint32_t pc);
		static bool BcTest(GekkoCore* core, uint32_t bo, uint32_t bi);
		static bool BctrTest(GekkoCore* core, uint32_t bo, uint32_t bi);
		static void BranchCheck(GekkoCore* core);

		// The quantised Paired-Single load and store helpers, defined in
		// gekkojit_ps.cpp and called from generated code. Static members for the
		// same reason as the four above.
		static void PsqLoad(GekkoCore* core, uint32_t ea, uint32_t packed);
		static void PsqStore(GekkoCore* core, uint32_t ea, uint32_t packed);

	public:
		// Addresses of the two Paired-Single quantised helpers, for the generated
		// code in gekkojit_ps.cpp. The helpers stay private so that nothing else
		// calls them.
		static uint64_t PsqLoadEntry() { return (uint64_t)(void*)&PsqLoad; }
		static uint64_t PsqStoreEntry() { return (uint64_t)(void*)&PsqStore; }

		Jit(GekkoCore* core);
		~Jit();

		bool IsSupported() const { return supported; }

		// Drop every compiled block. Must be called whenever the instruction stream or
		// the effective -> physical translation of a compiled block can have changed.
		void InvalidateAll();

		// Run one basic block, or one interpreter instruction when the pc cannot be
		// compiled (or when the recompiler is not available).
		void Run();
	};
}
