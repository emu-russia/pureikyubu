/*

Shared x86-64 encoder and ABI layout for the Gekko recompiler.

The integer translator (gekkojit.cpp) and the Paired-Single translator
(gekkojit_ps.cpp, built when GEKKO_JIT_PS is enabled) emit through the same
Emitter and share the same host register roles, stack frame rules and
GekkoRegs offsets, so those live here to keep the two translation units
independent of each other.

*/

#pragma once

#include "pch.h"

namespace Gekko
{

// ---------------------------------------------------------------------------
// x86-64 encoder

namespace X64
{
	enum Reg : uint8_t
	{
		RAX = 0, RCX, RDX, RBX, RSP, RBP, RSI, RDI,
		R8, R9, R10, R11, R12, R13, R14, R15,
	};

	// XMM register numbers. The generated code only ever uses XMM0-XMM5: XMM6-XMM15
	// are callee-saved on Win64 and the Paired-Single translations need so few
	// registers that saving them would cost more than it is worth.
	enum Xmm : uint8_t
	{
		XMM0 = 0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
		XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15,
	};

	enum Cc : uint8_t
	{
		CcO = 0, CcNO, CcB, CcAE, CcE, CcNE, CcBE, CcA,
		CcS, CcNS, CcP, CcNP, CcL, CcGE, CcLE, CcG,
	};

	// Extension of the 0x81/0x83 group
	enum Alu : uint8_t
	{
		AluAdd = 0, AluOr, AluAdc, AluSbb, AluAnd, AluSub, AluXor, AluCmp,
	};

	class Emitter
	{
	public:
		uint8_t* buf = nullptr;
		size_t cap = 0;
		size_t pos = 0;
		bool overflow = false;

		Emitter(uint8_t* _buf, size_t _cap) : buf(_buf), cap(_cap) {}

		void u8(uint8_t v)
		{
			if (pos < cap) buf[pos] = v; else overflow = true;
			pos++;
		}

		void u32(uint32_t v)
		{
			u8((uint8_t)v); u8((uint8_t)(v >> 8)); u8((uint8_t)(v >> 16)); u8((uint8_t)(v >> 24));
		}

		void u64(uint64_t v)
		{
			u32((uint32_t)v); u32((uint32_t)(v >> 32));
		}

		void rex(bool w, uint8_t r, uint8_t x, uint8_t b)
		{
			uint8_t v = 0x40
				| (w ? 0x08 : 0)
				| (uint8_t)(((r >> 3) & 1) << 2)
				| (uint8_t)(((x >> 3) & 1) << 1)
				| (uint8_t)((b >> 3) & 1);
			if (v != 0x40) u8(v);
		}

		// Replaces REX.B? No - forces a REX prefix when the 8 bit operand is one
		// of spl/bpl/sil/dil, whose encoding without REX means ah/ch/dh/bh.
		void rex8(uint8_t r, uint8_t x, uint8_t b)
		{
			uint8_t v = 0x40
				| (uint8_t)(((r >> 3) & 1) << 2)
				| (uint8_t)(((x >> 3) & 1) << 1)
				| (uint8_t)((b >> 3) & 1);
			u8(v);
		}

		// ModRM + SIB for [base + index*scale + disp32]. index == RSP means "no index".
		void mem(uint8_t reg, uint8_t base, uint8_t index, uint8_t scale, int32_t disp)
		{
			u8(0x80 | (uint8_t)((reg & 7) << 3) | 4);		// mod=10, rm=100 -> SIB
			u8((uint8_t)((scale << 6) | ((index & 7) << 3) | (base & 7)));
			u32((uint32_t)disp);
		}

		void modrmReg(uint8_t reg, uint8_t rm)
		{
			u8(0xC0 | (uint8_t)((reg & 7) << 3) | (rm & 7));
		}

		// ---- moves ----------------------------------------------------------

		void mov_r32_m(uint8_t dst, uint8_t base, int32_t disp, uint8_t index = RSP, uint8_t scale = 0)
		{
			rex(false, dst, index, base);
			u8(0x8B);
			mem(dst, base, index, scale, disp);
		}

		void mov_r64_m(uint8_t dst, uint8_t base, int32_t disp)
		{
			rex(true, dst, RSP, base);
			u8(0x8B);
			mem(dst, base, RSP, 0, disp);
		}

		void mov_m32_r(uint8_t base, int32_t disp, uint8_t src)
		{
			rex(false, src, RSP, base);
			u8(0x89);
			mem(src, base, RSP, 0, disp);
		}

		void mov_m64_r(uint8_t base, int32_t disp, uint8_t src)
		{
			rex(true, src, RSP, base);
			u8(0x89);
			mem(src, base, RSP, 0, disp);
		}

		void mov_r32_r32(uint8_t dst, uint8_t src)
		{
			rex(false, src, RSP, dst);
			u8(0x89);
			modrmReg(src, dst);
		}

		void mov_r64_r64(uint8_t dst, uint8_t src)
		{
			rex(true, src, RSP, dst);
			u8(0x89);
			modrmReg(src, dst);
		}

		void mov_r32_imm(uint8_t dst, uint32_t imm)
		{
			rex(false, 0, RSP, dst);
			u8((uint8_t)(0xB8 + (dst & 7)));
			u32(imm);
		}

		void mov_r64_imm(uint8_t dst, uint64_t imm)
		{
			rex(true, 0, RSP, dst);
			u8((uint8_t)(0xB8 + (dst & 7)));
			u64(imm);
		}

		void mov_m32_imm(uint8_t base, int32_t disp, uint32_t imm)
		{
			rex(false, 0, RSP, base);
			u8(0xC7);
			mem(0, base, RSP, 0, disp);
			u32(imm);
		}

		void mov_m8_imm(uint8_t base, int32_t disp, uint8_t imm)
		{
			rex(false, 0, RSP, base);
			u8(0xC6);
			mem(0, base, RSP, 0, disp);
			u8(imm);
		}

		void lea_r64_m(uint8_t dst, uint8_t base, int32_t disp, uint8_t index = RSP, uint8_t scale = 0)
		{
			rex(true, dst, index, base);
			u8(0x8D);
			mem(dst, base, index, scale, disp);
		}

		void movsx_r64_r32(uint8_t dst, uint8_t src)
		{
			rex(true, dst, RSP, src);
			u8(0x63);
			modrmReg(dst, src);
		}

		// ---- ALU ------------------------------------------------------------

		void alu_r32_r32(uint8_t op, uint8_t dst, uint8_t src)
		{
			static const uint8_t opc[8] = { 0x03, 0x0B, 0x13, 0x1B, 0x23, 0x2B, 0x33, 0x3B };
			rex(false, dst, RSP, src);
			u8(opc[op]);
			modrmReg(dst, src);
		}

		void alu_r32_m(uint8_t op, uint8_t dst, uint8_t base, int32_t disp)		// NOLINT
		{
			static const uint8_t opc[8] = { 0x03, 0x0B, 0x13, 0x1B, 0x23, 0x2B, 0x33, 0x3B };
			rex(false, dst, RSP, base);
			u8(opc[op]);
			mem(dst, base, RSP, 0, disp);
		}

		void alu_r32_imm(uint8_t op, uint8_t dst, uint32_t imm)
		{
			rex(false, 0, RSP, dst);
			u8(0x81);
			modrmReg(op, dst);
			u32(imm);
		}

		void add_r32_imm(uint8_t dst, uint32_t imm) { alu_r32_imm(AluAdd, dst, imm); }
		void and_r32_imm(uint8_t dst, uint32_t imm) { alu_r32_imm(AluAnd, dst, imm); }
		void or_r32_r32(uint8_t dst, uint8_t src) { alu_r32_r32(AluOr, dst, src); }
		void and_r32_r32(uint8_t dst, uint8_t src) { alu_r32_r32(AluAnd, dst, src); }
		void sub_r32_r32(uint8_t dst, uint8_t src) { alu_r32_r32(AluSub, dst, src); }
		void xor_r32_r32_same(uint8_t r) { alu_r32_r32(AluXor, r, r); }

		void test_r32_r32(uint8_t a, uint8_t b)
		{
			rex(false, b, RSP, a);
			u8(0x85);
			modrmReg(b, a);
		}

		void test_r32_imm(uint8_t r, uint32_t imm)
		{
			rex(false, 0, RSP, r);
			u8(0xF7);
			modrmReg(0, r);
			u32(imm);
		}

		void test_m8_imm(uint8_t base, int32_t disp, uint8_t imm)
		{
			rex(false, 0, RSP, base);
			u8(0xF6);
			mem(0, base, RSP, 0, disp);
			u8(imm);
		}

		void cmp_m8_imm(uint8_t base, int32_t disp, uint8_t imm)
		{
			rex(false, 0, RSP, base);
			u8(0x80);
			mem(7, base, RSP, 0, disp);
			u8(imm);
		}

		void cmp_r32_r32(uint8_t a, uint8_t b) { alu_r32_r32(AluCmp, a, b); }

		void imul_r32_r32(uint8_t dst, uint8_t src)
		{
			rex(false, dst, RSP, src);
			u8(0x0F); u8(0xAF);
			modrmReg(dst, src);
		}

		void neg_r32(uint8_t r)
		{
			rex(false, 0, RSP, r);
			u8(0xF7);
			modrmReg(3, r);
		}

		void not_r32(uint8_t r)
		{
			rex(false, 0, RSP, r);
			u8(0xF7);
			modrmReg(2, r);
		}

		void inc_r64(uint8_t r)
		{
			rex(true, 0, RSP, r);
			u8(0xFF);
			modrmReg(0, r);
		}

		void dec_r64(uint8_t r)
		{
			rex(true, 0, RSP, r);
			u8(0xFF);
			modrmReg(1, r);
		}

		void shl_r32_imm(uint8_t r, uint8_t n) { rex(false, 0, RSP, r); u8(0xC1); modrmReg(4, r); u8(n); }
		void shr_r32_imm(uint8_t r, uint8_t n) { rex(false, 0, RSP, r); u8(0xC1); modrmReg(5, r); u8(n); }
		void sar_r32_imm(uint8_t r, uint8_t n) { rex(false, 0, RSP, r); u8(0xC1); modrmReg(7, r); u8(n); }
		void rol_r32_imm(uint8_t r, uint8_t n) { rex(false, 0, RSP, r); u8(0xC1); modrmReg(0, r); u8(n); }

		void shl_r32_cl(uint8_t r) { rex(false, 0, RSP, r); u8(0xD3); modrmReg(4, r); }
		void shr_r32_cl(uint8_t r) { rex(false, 0, RSP, r); u8(0xD3); modrmReg(5, r); }
		void rol_r32_cl(uint8_t r) { rex(false, 0, RSP, r); u8(0xD3); modrmReg(0, r); }

		// 8 bit destination, so the encoding has to be REX safe.
		void setcc_r8(uint8_t cc, uint8_t r)
		{
			rex8(0, RSP, r);
			u8(0x0F);
			u8((uint8_t)(0x90 + cc));
			modrmReg(0, r);
		}

		void movzx_r32_r8(uint8_t dst, uint8_t src)
		{
			rex8(dst, RSP, src);
			u8(0x0F); u8(0xB6);
			modrmReg(dst, src);
		}

		void movsx_r32_r8(uint8_t dst, uint8_t src)
		{
			rex8(dst, RSP, src);
			u8(0x0F); u8(0xBE);
			modrmReg(dst, src);
		}

		void bsr_r32_r32(uint8_t dst, uint8_t src)
		{
			rex(false, dst, RSP, src);
			u8(0x0F); u8(0xBD);
			modrmReg(dst, src);
		}

		void bswap_r32(uint8_t r)
		{
			rex(false, 0, RSP, r);
			u8(0x0F);
			u8((uint8_t)(0xC8 + (r & 7)));
		}

		// ---- stack / control ------------------------------------------------

		void push_r64(uint8_t r)
		{
			if (r >= 8) u8(0x41);
			u8((uint8_t)(0x50 + (r & 7)));
		}

		void pop_r64(uint8_t r)
		{
			if (r >= 8) u8(0x41);
			u8((uint8_t)(0x58 + (r & 7)));
		}

		void sub_rsp_imm8(uint8_t n)
		{
			rex(true, 0, RSP, RSP);
			u8(0x83);
			modrmReg(5, RSP);
			u8(n);
		}

		void add_rsp_imm8(uint8_t n)
		{
			rex(true, 0, RSP, RSP);
			u8(0x83);
			modrmReg(0, RSP);
			u8(n);
		}

		void ret() { u8(0xC3); }

		size_t jmp_rel32()
		{
			u8(0xE9);
			size_t at = pos;
			u32(0);
			return at;
		}

		size_t jcc_rel32(uint8_t cc)
		{
			u8(0x0F);
			u8((uint8_t)(0x80 + cc));
			size_t at = pos;
			u32(0);
			return at;
		}

		// ---- SSE / SSE2 ------------------------------------------------------
		// The Paired-Single translator (gekkojit_ps.cpp) moves a guest PS0/PS1 pair
		// - two doubles living in two separate arrays - through one XMM register and
		// then operates on both lanes at once, which is where the speedup comes from.
		//
		// Only the operand forms that translator needs are implemented, so that the
		// encoder stays reviewable. The suffix names where the memory operand goes,
		// as in the Intel manual: xmm_m loads into the register, m_xmm stores from it.

		enum SsePrefix : uint8_t { SseNone = 0x00, Sse66 = 0x66, SseF2 = 0xF2, SseF3 = 0xF3 };

		void sse_rr(uint8_t pfx, uint8_t op, uint8_t dst, uint8_t src)
		{
			if (pfx != SseNone) u8(pfx);
			rex(false, dst, RSP, src);
			u8(0x0F); u8(op);
			modrmReg(dst, src);
		}

		void sse_rr_imm(uint8_t pfx, uint8_t op, uint8_t dst, uint8_t src, uint8_t imm)
		{
			sse_rr(pfx, op, dst, src);
			u8(imm);
		}

		void sse_xmm_m(uint8_t pfx, uint8_t op, uint8_t dst, uint8_t base, int32_t disp)
		{
			if (pfx != SseNone) u8(pfx);
			rex(false, dst, RSP, base);
			u8(0x0F); u8(op);
			mem(dst, base, RSP, 0, disp);
		}

		void sse_m_xmm(uint8_t pfx, uint8_t op, uint8_t base, int32_t disp, uint8_t src)
		{
			if (pfx != SseNone) u8(pfx);
			rex(false, src, RSP, base);
			u8(0x0F); u8(op);
			mem(src, base, RSP, 0, disp);
		}

		// Half-register moves. These are what gather and scatter a PS0/PS1 pair:
		// movsd puts PS0 in the low half and clears the high one, movhpd puts PS1 in
		// the high half, and the same two in reverse write the pair back.
		void movsd_xmm_m(uint8_t x, uint8_t base, int32_t d) { sse_xmm_m(SseF2, 0x10, x, base, d); }
		void movsd_m_xmm(uint8_t base, int32_t d, uint8_t x) { sse_m_xmm(SseF2, 0x11, base, d, x); }
		void movhpd_xmm_m(uint8_t x, uint8_t base, int32_t d) { sse_xmm_m(Sse66, 0x16, x, base, d); }
		void movhpd_m_xmm(uint8_t base, int32_t d, uint8_t x) { sse_m_xmm(Sse66, 0x17, base, d, x); }
		void movlpd_xmm_m(uint8_t x, uint8_t base, int32_t d) { sse_xmm_m(Sse66, 0x12, x, base, d); }
		void movlpd_m_xmm(uint8_t base, int32_t d, uint8_t x) { sse_m_xmm(Sse66, 0x13, base, d, x); }

		void movapd_rr(uint8_t dst, uint8_t src) { sse_rr(Sse66, 0x28, dst, src); }
		void addpd_rr(uint8_t dst, uint8_t src) { sse_rr(Sse66, 0x58, dst, src); }
		void mulpd_rr(uint8_t dst, uint8_t src) { sse_rr(Sse66, 0x59, dst, src); }
		void subpd_rr(uint8_t dst, uint8_t src) { sse_rr(Sse66, 0x5C, dst, src); }
		void divpd_rr(uint8_t dst, uint8_t src) { sse_rr(Sse66, 0x5E, dst, src); }
		void sqrtpd_rr(uint8_t dst, uint8_t src) { sse_rr(Sse66, 0x51, dst, src); }
		void andpd_rr(uint8_t dst, uint8_t src) { sse_rr(Sse66, 0x54, dst, src); }
		void andnpd_rr(uint8_t dst, uint8_t src) { sse_rr(Sse66, 0x55, dst, src); }
		void orpd_rr(uint8_t dst, uint8_t src) { sse_rr(Sse66, 0x56, dst, src); }
		void xorpd_rr(uint8_t dst, uint8_t src) { sse_rr(Sse66, 0x57, dst, src); }
		void unpcklpd_rr(uint8_t dst, uint8_t src) { sse_rr(Sse66, 0x14, dst, src); }
		void unpckhpd_rr(uint8_t dst, uint8_t src) { sse_rr(Sse66, 0x15, dst, src); }
		void shufpd_rr(uint8_t dst, uint8_t src, uint8_t imm) { sse_rr_imm(Sse66, 0xC6, dst, src, imm); }
		void cmppd_rr(uint8_t dst, uint8_t src, uint8_t imm) { sse_rr_imm(Sse66, 0xC2, dst, src, imm); }
		// Integer lane shuffle, used to swap the two halves of a pair.
		void pshufd_rr(uint8_t dst, uint8_t src, uint8_t imm) { sse_rr_imm(Sse66, 0x70, dst, src, imm); }

		// movq r64 <-> xmm, used to materialise the constants the Paired-Single
		// translations need (1.0, the sign mask, the absolute-value mask).
		void movq_xmm_r64(uint8_t x, uint8_t gpr)
		{
			u8(Sse66);
			rex(true, x, RSP, gpr);
			u8(0x0F); u8(0x6E);
			modrmReg(x, gpr);
		}

		void movq_r64_xmm(uint8_t gpr, uint8_t x)
		{
			u8(Sse66);
			rex(true, x, RSP, gpr);
			u8(0x0F); u8(0x7E);
			modrmReg(x, gpr);
		}

		// mov r11, addr ; call r11
		// A Win64 callee is allowed to spill its four register arguments into the 32
		// bytes above the return address (the caller's "shadow space"), so a block may
		// not keep anything live there. SysV hosts have no such area, which makes the
		// mistake invisible on Linux; in the ABI regression build this poisons exactly
		// that area before every helper call, so the corruption is reproducible there
		// too. R11 is free: call_abs loads it with the target right afterwards.
		void poison_shadow_space()
		{
#ifdef GEKKO_JIT_TEST_WIN64_SHADOW
			mov_r64_imm(R11, 0x1122334455667788ull);
			mov_m64_r(RSP, 0, R11);
			mov_m64_r(RSP, 8, R11);
			mov_m64_r(RSP, 16, R11);
			mov_m64_r(RSP, 24, R11);
#endif
		}

		void call_abs(uint64_t addr)
		{
			poison_shadow_space();
			mov_r64_imm(R11, addr);
			u8(0x41); u8(0xFF);
			modrmReg(2, R11);
		}

		void patch32(size_t at, uint32_t value)
		{
			if (at + 4 > cap) { overflow = true; return; }
			buf[at + 0] = (uint8_t)value;
			buf[at + 1] = (uint8_t)(value >> 8);
			buf[at + 2] = (uint8_t)(value >> 16);
			buf[at + 3] = (uint8_t)(value >> 24);
		}

		// Offset from the end of a rel32 field at 'at' to the current position.
		uint32_t rel(size_t at) const { return (uint32_t)(pos - (at + 4)); }
	};
}

// ---------------------------------------------------------------------------
// Calling convention and register roles

#if defined(_WIN32)
	static const uint8_t Arg0 = X64::RCX;
	static const uint8_t Arg1 = X64::RDX;
	static const uint8_t Arg2 = X64::R8;
#else
	static const uint8_t Arg0 = X64::RDI;
	static const uint8_t Arg1 = X64::RSI;
	static const uint8_t Arg2 = X64::RDX;
#endif

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
