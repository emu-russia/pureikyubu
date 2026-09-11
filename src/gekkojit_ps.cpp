/*

Paired-Single -> SSE/SSE2 recompiler. See gekkojit_ps.h for the design notes.

The operand order is the one trap in this file. The decoder stores the fields in
the encoding order frD, frA, frC, frB (paramBits[0..3]), which is *not* the order
the two-operand forms use: for those paramBits[2] is frB and paramBits[3] is
unused. So every case below is written against the interpreter's own expression,
with p1/p2/p3 named after the paramBits index rather than after a role.

*/

#include "pch.h"
#include "gekkojit.h"
#include "gekkojit_ps.h"

#if GEKKO_JIT_SUPPORTED && GEKKO_JIT_PS

#include "gekkojit_x64.h"

namespace Gekko
{

namespace JitPs
{

// The XMM registers a translation works in. Only XMM0-XMM5 are used anywhere in
// the generated code: XMM6-XMM15 are callee-saved on Win64.
static const uint8_t V0 = X64::XMM0;		// first operand, and the result
static const uint8_t V1 = X64::XMM1;		// second operand
static const uint8_t V2 = X64::XMM2;		// third operand
static const uint8_t V3 = X64::XMM3;		// scratch
static const uint8_t V4 = X64::XMM4;		// scratch
static const uint8_t V5 = X64::XMM5;		// scratch

// PS0 lives in fpr[n], PS1 in ps1[n]; each entry is one 8 byte FPREG.
static int32_t Ps0Disp(uint32_t n) { return FprOff + (int32_t)n * 8; }
static int32_t Ps1Disp(uint32_t n) { return Ps1Off + (int32_t)n * 8; }

// The recording forms differ from the plain ones only by a trailing CR1 update.
static bool IsRecordForm(Instruction instr)
{
	switch (instr)
	{
	case Instruction::ps_add_d:
	case Instruction::ps_sub_d:
	case Instruction::ps_mul_d:
	case Instruction::ps_div_d:
	case Instruction::ps_res_d:
	case Instruction::ps_rsqrte_d:
	case Instruction::ps_sel_d:
	case Instruction::ps_muls0_d:
	case Instruction::ps_muls1_d:
	case Instruction::ps_sum0_d:
	case Instruction::ps_sum1_d:
	case Instruction::ps_madd_d:
	case Instruction::ps_msub_d:
	case Instruction::ps_nmadd_d:
	case Instruction::ps_nmsub_d:
	case Instruction::ps_madds0_d:
	case Instruction::ps_madds1_d:
	case Instruction::ps_mr_d:
	case Instruction::ps_neg_d:
	case Instruction::ps_abs_d:
	case Instruction::ps_nabs_d:
	case Instruction::ps_merge00_d:
	case Instruction::ps_merge01_d:
	case Instruction::ps_merge10_d:
	case Instruction::ps_merge11_d:
		return true;
	default:
		return false;
	}
}

// COMPUTE_CR1() from gekkoc.h, which every recording form runs after its result
// is written: cr = (cr & 0xf0ffffff) | ((fpscr & 0xf0000000) >> 4).
static void EmitComputeCr1(X64::Emitter& e)
{
	e.mov_r32_m(T0, RegRegs, FpscrOff);
	e.shr_r32_imm(T0, 4);
	e.and_r32_imm(T0, 0x0f000000);
	e.mov_r32_m(T1, RegRegs, CrOff);
	e.and_r32_imm(T1, 0xf0ffffff);
	e.or_r32_r32(T0, T1);
	e.mov_m32_r(RegRegs, CrOff, T0);
}

// Pull a guest PS register into one XMM register: lane 0 = PS0, lane 1 = PS1.
// The movsd load clears the upper half, so the pair ends up exactly in place.
static void Gather(X64::Emitter& e, uint8_t x, uint32_t n)
{
	e.movsd_xmm_m(x, RegRegs, Ps0Disp(n));
	e.movhpd_xmm_m(x, RegRegs, Ps1Disp(n));
}

// The reverse: write both halves of the pair back to the guest register file.
static void Scatter(X64::Emitter& e, uint32_t n, uint8_t x)
{
	e.movsd_m_xmm(RegRegs, Ps0Disp(n), x);
	e.movhpd_m_xmm(RegRegs, Ps1Disp(n), x);
}

// Broadcast a 64 bit constant to both lanes. movq clears bits 127:64, so the
// unpcklpd that follows duplicates the low half into the high one. RAX is a
// translation scratch that no instruction keeps live across itself.
static void BroadcastConst(X64::Emitter& e, uint8_t x, uint64_t bits)
{
	e.mov_r64_imm(X64::RAX, bits);
	e.movq_xmm_r64(x, X64::RAX);
	e.unpcklpd_rr(x, x);
}

// Negate both lanes by flipping the sign bits. The interpreter uses unary minus,
// which does the same thing including for zeroes and NaNs.
static void Negate(X64::Emitter& e, uint8_t x)
{
	e.mov_r64_imm(X64::RAX, 0x8000'0000'0000'0000ull);
	e.movq_xmm_r64(V3, X64::RAX);
	e.unpcklpd_rr(V3, V3);
	e.xorpd_rr(x, V3);
}

bool Translate(X64::Emitter& e, GekkoCore* core, const DecoderInfo& di)
{
	// Every PS instruction raises the FP-unavailable exception when MSR[FP] is
	// clear. MSR is constant within a block (see gekkojit_ps.h), so it is decided
	// here and the interpreter keeps that case.
	if ((core->regs.msr & MSR_FP) == 0)
	{
		return false;
	}

	uint32_t rd = (uint32_t)di.paramBits[0];		// frD
	uint32_t p1 = (uint32_t)di.paramBits[1];		// frA (or frB for the two operand forms)
	uint32_t p2 = (uint32_t)di.paramBits[2];		// frC (or frB for the two operand forms)
	uint32_t p3 = (uint32_t)di.paramBits[3];		// frB, three operand forms only

	switch (di.instr)
	{
	// ---- two operand arithmetic -----------------------------------------
	// rd = p1 OP p2

	case Instruction::ps_add:
	case Instruction::ps_add_d:
		Gather(e, V0, p1);
		Gather(e, V1, p2);
		e.addpd_rr(V0, V1);
		Scatter(e, rd, V0);
		break;

	case Instruction::ps_sub:
	case Instruction::ps_sub_d:
		Gather(e, V0, p1);
		Gather(e, V1, p2);
		e.subpd_rr(V0, V1);
		Scatter(e, rd, V0);
		break;

	case Instruction::ps_mul:
	case Instruction::ps_mul_d:
		Gather(e, V0, p1);
		Gather(e, V1, p2);
		e.mulpd_rr(V0, V1);
		Scatter(e, rd, V0);
		break;

	case Instruction::ps_div:
	case Instruction::ps_div_d:
		Gather(e, V0, p1);
		Gather(e, V1, p2);
		e.divpd_rr(V0, V1);
		Scatter(e, rd, V0);
		break;

	// rd = 1.0 / p1 and rd = 1.0 / sqrt(p1). Both are spelled with a single
	// operand, so the operand is paramBits[1]. The interpreter evaluates them in
	// double (the 1.0f literal promotes), so the packed double operations match.
	case Instruction::ps_res:
	case Instruction::ps_res_d:
		Gather(e, V0, p1);
		BroadcastConst(e, V1, 0x3ff0'0000'0000'0000ull);		// 1.0
		e.divpd_rr(V1, V0);
		Scatter(e, rd, V1);
		break;

	case Instruction::ps_rsqrte:
	case Instruction::ps_rsqrte_d:
		Gather(e, V0, p1);
		BroadcastConst(e, V1, 0x3ff0'0000'0000'0000ull);		// 1.0
		e.sqrtpd_rr(V0, V0);
		e.divpd_rr(V1, V0);
		Scatter(e, rd, V1);
		break;

	// ---- fused multiply-add ---------------------------------------------
	// rd = p1 * p2 +/- p3, or its negation. The multiply and the add are kept
	// separate so that the intermediate is rounded exactly like the interpreter's
	// (a * c) + b.

	case Instruction::ps_madd:
	case Instruction::ps_madd_d:
		Gather(e, V0, p1);
		Gather(e, V1, p2);
		Gather(e, V2, p3);
		e.mulpd_rr(V0, V1);
		e.addpd_rr(V0, V2);
		Scatter(e, rd, V0);
		break;

	case Instruction::ps_msub:
	case Instruction::ps_msub_d:
		Gather(e, V0, p1);
		Gather(e, V1, p2);
		Gather(e, V2, p3);
		e.mulpd_rr(V0, V1);
		e.subpd_rr(V0, V2);
		Scatter(e, rd, V0);
		break;

	case Instruction::ps_nmadd:
	case Instruction::ps_nmadd_d:
		Gather(e, V0, p1);
		Gather(e, V1, p2);
		Gather(e, V2, p3);
		e.mulpd_rr(V0, V1);
		e.addpd_rr(V0, V2);
		Negate(e, V0);
		Scatter(e, rd, V0);
		break;

	case Instruction::ps_nmsub:
	case Instruction::ps_nmsub_d:
		Gather(e, V0, p1);
		Gather(e, V1, p2);
		Gather(e, V2, p3);
		e.mulpd_rr(V0, V1);
		e.subpd_rr(V0, V2);
		Negate(e, V0);
		Scatter(e, rd, V0);
		break;

	// ---- multiply by one lane of the second operand ---------------------
	// rd = p1 * broadcast(PS0 or PS1 of p2), and madds forms add p3 as well. The
	// broadcast is unpcklpd (low half) or unpckhpd (high half) of p2 with itself.

	case Instruction::ps_muls0:
	case Instruction::ps_muls0_d:
		Gather(e, V0, p1);
		Gather(e, V2, p2);
		e.unpcklpd_rr(V2, V2);
		e.mulpd_rr(V0, V2);
		Scatter(e, rd, V0);
		break;

	case Instruction::ps_muls1:
	case Instruction::ps_muls1_d:
		Gather(e, V0, p1);
		Gather(e, V2, p2);
		e.unpckhpd_rr(V2, V2);
		e.mulpd_rr(V0, V2);
		Scatter(e, rd, V0);
		break;

	case Instruction::ps_madds0:
	case Instruction::ps_madds0_d:
		Gather(e, V0, p1);
		Gather(e, V2, p2);
		Gather(e, V1, p3);
		e.unpcklpd_rr(V2, V2);
		e.mulpd_rr(V0, V2);
		e.addpd_rr(V0, V1);
		Scatter(e, rd, V0);
		break;

	case Instruction::ps_madds1:
	case Instruction::ps_madds1_d:
		Gather(e, V0, p1);
		Gather(e, V2, p2);
		Gather(e, V1, p3);
		e.unpckhpd_rr(V2, V2);
		e.mulpd_rr(V0, V2);
		e.addpd_rr(V0, V1);
		Scatter(e, rd, V0);
		break;

	// ---- horizontal adds -------------------------------------------------
	// ps_sum0: rd = { PS0(p1) + PS1(p3), PS1(p2) }
	// ps_sum1: rd = { PS0(p2), PS0(p1) + PS1(p3) }
	//
	// pshufd 0x4e swaps the two halves of p3 so that the wanted lanes line up
	// under addpd, and shufpd then takes one half from each source:
	// dest.lo = imm[0] ? src.lo : dest.lo, dest.hi = imm[1] ? src.hi : dest.hi.

	case Instruction::ps_sum0:
	case Instruction::ps_sum0_d:
		Gather(e, V0, p1);
		Gather(e, V2, p2);
		Gather(e, V1, p3);
		e.pshufd_rr(V1, V1, 0x4e);			// V1 = { p3.ps1, p3.ps0 }
		e.addpd_rr(V0, V1);					// V0.lo = p1.ps0 + p3.ps1
		e.shufpd_rr(V0, V2, 0x2);			// V0.hi = V2.hi = p2.ps1
		Scatter(e, rd, V0);
		break;

	case Instruction::ps_sum1:
	case Instruction::ps_sum1_d:
		Gather(e, V0, p1);
		Gather(e, V2, p2);
		Gather(e, V1, p3);
		e.pshufd_rr(V0, V0, 0x4e);			// V0 = { p1.ps1, p1.ps0 }
		e.addpd_rr(V0, V1);					// V0.hi = p1.ps0 + p3.ps1
		e.shufpd_rr(V2, V0, 0x2);			// V2 = { p2.ps0, V0.hi }
		Scatter(e, rd, V2);
		break;

	// ---- merges ----------------------------------------------------------
	// Each takes one lane from p1 and the other from p2, so the pair is built
	// straight from the two halves without gathering a whole register first.

	case Instruction::ps_merge00:
	case Instruction::ps_merge00_d:
		e.movsd_xmm_m(V0, RegRegs, Ps0Disp(p1));
		e.movhpd_xmm_m(V0, RegRegs, Ps0Disp(p2));
		Scatter(e, rd, V0);
		break;

	case Instruction::ps_merge01:
	case Instruction::ps_merge01_d:
		e.movsd_xmm_m(V0, RegRegs, Ps0Disp(p1));
		e.movhpd_xmm_m(V0, RegRegs, Ps1Disp(p2));
		Scatter(e, rd, V0);
		break;

	case Instruction::ps_merge10:
	case Instruction::ps_merge10_d:
		e.movsd_xmm_m(V0, RegRegs, Ps1Disp(p1));
		e.movhpd_xmm_m(V0, RegRegs, Ps0Disp(p2));
		Scatter(e, rd, V0);
		break;

	case Instruction::ps_merge11:
	case Instruction::ps_merge11_d:
		e.movsd_xmm_m(V0, RegRegs, Ps1Disp(p1));
		e.movhpd_xmm_m(V0, RegRegs, Ps1Disp(p2));
		Scatter(e, rd, V0);
		break;

	// ---- moves and sign manipulation -------------------------------------
	// All of these are single operand forms, so the source is paramBits[1].

	case Instruction::ps_mr:
	case Instruction::ps_mr_d:
		Gather(e, V0, p1);
		Scatter(e, rd, V0);
		break;

	case Instruction::ps_neg:
	case Instruction::ps_neg_d:
		Gather(e, V0, p1);
		Negate(e, V0);
		Scatter(e, rd, V0);
		break;

	case Instruction::ps_abs:
	case Instruction::ps_abs_d:
		Gather(e, V0, p1);
		BroadcastConst(e, V3, 0x7fff'ffff'ffff'ffffull);
		e.andpd_rr(V0, V3);
		Scatter(e, rd, V0);
		break;

	case Instruction::ps_nabs:
	case Instruction::ps_nabs_d:
		Gather(e, V0, p1);
		BroadcastConst(e, V3, 0x8000'0000'0000'0000ull);
		e.orpd_rr(V0, V3);
		Scatter(e, rd, V0);
		break;

	// ---- select ----------------------------------------------------------
	// rd.lane = (p1.lane >= 0) ? p2.lane : p3.lane
	//
	// The comparison has to be an *ordered* "greater or equal": the interpreter
	// uses C's >=, which is false for NaN and true for -0.0, while SSE2's NLT
	// predicate (5) is true for NaN. So the mask is built as "not (a < 0) and not
	// unordered": cmppd LT is false for NaN, cmppd UNORD catches it, and the two
	// are or-ed and inverted. The select is b ^ ((b ^ c) & mask) with b = p2.

	case Instruction::ps_sel:
	case Instruction::ps_sel_d:
		Gather(e, V0, p1);					// V0 = a
		Gather(e, V1, p2);					// V1 = b, taken when a >= 0
		Gather(e, V2, p3);					// V2 = c, taken otherwise
		BroadcastConst(e, V3, 0);			// V3 = 0.0
		e.movapd_rr(V4, V0);
		e.cmppd_rr(V4, V3, 0x1);			// V4 = (a < 0), false for NaN
		e.movapd_rr(V5, V0);
		e.cmppd_rr(V5, V0, 0x3);			// V5 = unordered(a)
		e.orpd_rr(V4, V5);					// V4 = !(a >= 0)
		BroadcastConst(e, V5, 0xffff'ffff'ffff'ffffull);
		e.andnpd_rr(V4, V5);				// V4 = (a >= 0)
		e.movapd_rr(V5, V2);
		e.xorpd_rr(V5, V1);					// V5 = c ^ b
		e.andpd_rr(V5, V4);
		e.xorpd_rr(V2, V5);					// V2 = mask ? b : c
		Scatter(e, rd, V2);
		break;

	// Everything else - the comparison forms and the quantised loads and stores -
	// is left to the interpreter fallback.
	default:
		return false;
	}

	if (IsRecordForm(di.instr))
	{
		EmitComputeCr1(e);
	}

	return true;
}

}

}

#else

// Built without GEKKO_JIT_PS (or on a host without a recompiler): every PS
// instruction stays on the interpreter fallback.
namespace Gekko
{
	namespace JitPs
	{
		bool Translate(X64::Emitter&, GekkoCore*, const DecoderInfo&) { return false; }
	}
}

#endif
