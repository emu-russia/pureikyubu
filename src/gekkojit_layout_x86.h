/*

Gekko -> 32-bit x86 layout constants (see jit_x86.h for the shared encoder).

The host register roles, the stack frame size and the offsets inside GekkoCore /
GekkoRegs that the generated code addresses. They are shared by the two Gekko
translators (gekkojit_x86.cpp and gekkojit_ps_x86.cpp) and are kept apart from the
encoder so that the DSPcore recompiler can use the encoder without inheriting
Gekko's layout.

## The register allocation

32-bit mode has eight general purpose registers and no more. cdecl keeps EBX, ESI,
EDI and EBP across a call, so the four values a block has to keep live - the core
pointer, the pc, the retired-instruction counter and the branch-target scratch -
live in exactly those four, and the two that only have to survive *one* helper call
(the effective address of an update-form access and the JitExit pointer) live in
frame slots.

`RegRegs` is the *same* register as `RegCore`: the emulated register file hangs off
the core at a fixed offset, so `GekkoRegs` is reached as `[RegCore + RegsOff + ...]`
and no second pointer register is needed. The offsets below therefore all include
`RegsOff`, which is what makes the translations in gekkojit_x86.cpp read like the
x86-64 ones.

The x86-64 layout this mirrors is in gekkojit_layout_x64.h.

*/

#pragma once

#include "jit_x86.h"

namespace Gekko
{

// Host register roles. The comment on each says how long the value stays live.
static const uint8_t RegCore = X86::EBX;		// GekkoCore*, kept for the whole block
static const uint8_t RegRegs = X86::EBX;		// == RegCore (see the note above)
static const uint8_t RegPc = X86::ESI;			// 32 bit pc, kept for the whole block
static const uint8_t RegCount = X86::EDI;		// instructions retired in this block

// Scratch GPRs for the translations (never live across a helper call).
static const uint8_t T0 = X86::EAX;
static const uint8_t T1 = X86::ECX;
static const uint8_t T2 = X86::EDX;
// EBP is the fourth scratch, and it is also what call_abs loads the helper address
// into. No translation keeps a T3 value live across a call, so the two roles cannot
// collide. There is no fifth scratch: COMPUTE_CR0 and the compare updates, the two
// sequences that need one, take their third condition bit out of AH instead (see
// setcc_ah in jit_x86.h and emitCR0 in gekkojit_x86.cpp).
static const uint8_t T3 = X86::EBP;

// ---------------------------------------------------------------------------
// Stack frame.
//
// Entry esp is 12 mod 16 (the return address the caller pushed from a 16 byte aligned
// call site) and the four saved registers keep it there, so a frame size of 0 mod 16
// keeps esp 12 mod 16 and a three-argument helper call - every call the Gekko core
// makes - lands on 16 bytes. A two-argument call (the DSP core makes those) pushes one
// padding argument to reach the same alignment.
//
// Slot 0 is the effective address of an update-form access (it has to survive a helper
// call and there is no callee-saved register left for it), slots 8 and 12 are the
// self-loop counters and the rest is staging space for the Paired-Single constants.
static const int32_t FrameSize = 48;
static_assert(FrameSize % 16 == 0, "esp must be 16 byte aligned at the helper calls");

static const int32_t EaSlot = 0;				// effective address across a helper call
static const int32_t LoopTicksSlot = 8;			// taken branches retired on a back edge
static const int32_t LoopBudgetSlot = 12;		// remaining iterations of a back edge
// How many ticks the block could still retire before the Flipper-side work is due, taken
// at the block entry. A self-looping block leaves when its own ticks catch up with it, so
// that the work - and every device register the loop may be polling - is up to date.
static const int32_t DeadlineSlot = 16;
static const int32_t ConstSlot = 24;			// 8 byte staging area (PS constants)

// The cdecl argument slots. The return address and the four saved registers sit
// between the frame and the arguments, so [esp + FrameSize] is the last saved
// register, [esp + FrameSize + 16] the return address and the arguments follow it.
static const int32_t Arg0Slot = FrameSize + 20;
static const int32_t Arg1Slot = FrameSize + 24;
static const int32_t Arg2Slot = FrameSize + 28;

// How many times a block may take its own back edge before it leaves and lets the
// dispatcher re-enter it. Small enough that the deferred time base update stays within a
// couple of microseconds of emulated time (16 iterations of a six instruction loop is
// under 100 ticks), large enough that the dispatcher is out of the loop's inner path.
static const uint32_t LoopBudget = 16;

// ---------------------------------------------------------------------------
// Offsets inside GekkoCore / GekkoRegs. They are all relative to RegCore.

static const int32_t RegsOff = offsetof(GekkoCore, regs);

static const int32_t GprOff = RegsOff + (int32_t)offsetof(GekkoRegs, gpr);
static const int32_t SprOff = RegsOff + (int32_t)offsetof(GekkoRegs, spr);
static const int32_t CrOff = RegsOff + (int32_t)offsetof(GekkoRegs, cr);
static const int32_t PcOff = RegsOff + (int32_t)offsetof(GekkoRegs, pc);
static const int32_t TbOff = RegsOff + (int32_t)offsetof(GekkoRegs, tb);
static const int32_t DecOff = SprOff + (int32_t)SPR::DEC * 4;
static const int32_t XerOff = SprOff + (int32_t)SPR::XER * 4;
static const int32_t LrOff = SprOff + (int32_t)SPR::LR * 4;
static const int32_t CtrOff = SprOff + (int32_t)SPR::CTR * 4;

// How the block stopped.
enum class JitExitKind : uint32_t
{
	Normal = 0,			// pc is in regs.pc, a plain fallthrough
	TakenBranch = 1,	// pc is the branch target, BranchCheck still has to run
	Exception = 2,		// a helper already set regs.pc to the exception vector

	// The self-loop ran out of budget or reached the Flipper deadline: pc is the head of the
	// loop, which is where the branch that was taken last points - but the branch that would
	// go back to it once more is one the guest has not executed. The block's `ticks` already
	// account for every branch that *was* taken, so the only tick left to pay is the one
	// BranchCheck gives (see Run).
	BackEdge = 3,
};

struct JitExit
{
	uint64_t count;
	uint32_t kind;

	// Taken branches the block retired on a back edge of its own, on top of `count`
	// instructions. The interpreter ticks every taken branch twice, so Run() has to
	// advance the time base by `count + ticks` (see emitBackEdge in gekkojit_x86.cpp).
	uint32_t ticks;
};

// Paired-Single register file. An FPR holds PS0 in fpr[n] and PS1 in ps1[n],
// both as doubles whose value is the one the guest stored (see the PS0/PS1
// macros in gekkoc.h), which is what the SSE translations in gekkojit_ps_x86.cpp
// widen into one 128 bit register.
static const int32_t FprOff = RegsOff + (int32_t)offsetof(GekkoRegs, fpr);
static const int32_t Ps1Off = RegsOff + (int32_t)offsetof(GekkoRegs, ps1);
static const int32_t FpscrOff = RegsOff + (int32_t)offsetof(GekkoRegs, fpscr);
}
