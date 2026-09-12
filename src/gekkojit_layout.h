/*

Gekko -> x86-64 layout constants (see jit_x64.h for the shared encoder).

The host register roles, the stack frame size and the offsets inside GekkoRegs that the
generated code addresses. They are shared by the two Gekko translators (gekkojit.cpp
and gekkojit_ps.cpp) and are kept apart from the generic encoder so that the DSPcore
recompiler can use the encoder without inheriting Gekko's layout.

*/

#pragma once

#include "jit_x64.h"

namespace Gekko
{

static const uint8_t RegCore = X64::R15;		// GekkoCore*, kept for the whole block
static const uint8_t RegRegs = X64::R14;		// GekkoRegs*, kept for the whole block
static const uint8_t RegPc = X64::R13;			// 32 bit pc, kept for the whole block
static const uint8_t RegCount = X64::RBX;		// instructions retired in this block

// Scratch GPRs for the translations (never live across a helper call).
static const uint8_t T0 = X64::R8;
static const uint8_t T1 = X64::R9;
static const uint8_t T2 = X64::R10;
static const uint8_t T3 = X64::RAX;
// Scratch for the condition-register helpers. It must not be one of T0..T3: the
// value a helper derives the flags from is frequently held in one of those.
static const uint8_t TC = X64::R11;

// Values that have to survive a helper call. They live in callee-saved registers
// rather than on the stack on purpose: the Win64 ABI lets a callee spill its four
// register arguments into the 32 bytes above rsp ("shadow space"), so anything the
// block keeps in memory below that mark would be silently overwritten by the
// helper. Keeping them in RBP/R12 also saves a store/load per memory instruction.
static const uint8_t RegEa = X64::RBP;			// effective address across a helper call
static const uint8_t RegExit = X64::R12;		// JitExit* for the whole block

// Stack frame. The only requirement is the 32 byte Win64 shadow space at [rsp,rsp+32);
// nothing of ours lives there, and the saved registers sit above it. Entry rsp is
// 8 mod 16 (the calling C++ code is ABI compliant), the eight pushes keep that, so
// the frame size must be 8 mod 16 to leave rsp 16 byte aligned at the helper calls.
// Alignment is enforced by the static_assert rather than by the runtime ABI test:
// the helpers are plain C++ functions that this build compiles without aligned SSE
// stack spills, so a misaligned frame happens not to fail anything, while a wrong
// shadow space layout does.
static const int32_t ShadowSpace = 32;
static const int32_t FrameSize = 40;
static_assert(FrameSize >= ShadowSpace, "the frame must cover the Win64 shadow space");
static_assert(FrameSize % 16 == 8, "rsp must be 16 byte aligned at the helper calls");

static const int32_t GprOff = offsetof(GekkoRegs, gpr);
static const int32_t SprOff = offsetof(GekkoRegs, spr);
static const int32_t CrOff = offsetof(GekkoRegs, cr);
static const int32_t PcOff = offsetof(GekkoRegs, pc);
static const int32_t TbOff = offsetof(GekkoRegs, tb);
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
};

struct JitExit
{
	uint64_t count;
	uint32_t kind;
	uint32_t pad;
};

// Paired-Single register file. An FPR holds PS0 in fpr[n] and PS1 in ps1[n],
// both as doubles whose value is the one the guest stored (see the PS0/PS1
// macros in gekkoc.h), which is what the SSE translations in gekkojit_ps.cpp
// widen into one 128 bit register.
static const int32_t FprOff = offsetof(GekkoRegs, fpr);
static const int32_t Ps1Off = offsetof(GekkoRegs, ps1);
static const int32_t FpscrOff = offsetof(GekkoRegs, fpscr);
}
