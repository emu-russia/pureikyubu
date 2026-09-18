/*

Shared 32-bit x86 encoder.

The 32-bit sibling of jit_x64.h. The x86 recompilers of the Gekko core
(gekkojit_x86.cpp / gekkojit_ps_x86.cpp) and of DSPcore (dspjit_x86.cpp) both emit
32-bit x86 machine code through this Emitter, so it lives here. The host register
roles, the stack frame and the offsets into the emulated register files stay next
to the core that uses them (gekkojit_layout_x86.h and the layout section of
dspjit.h).

What is different from the x86-64 encoder:

* There are no REX prefixes. Eight general purpose registers are all the ISA has,
  and four of them (EBX, ESI, EDI, EBP) are the ones the C++ ABI keeps across a
  call, which is what the translators build their register allocation around.
* cdecl passes every argument on the stack, so the generated code pushes them and
  there are no Arg0..Arg2 registers to fill (see push_r32 and push_imm32). The
  caller also cleans the arguments up, which is why a call site is always a push
  sequence, the call and an add esp.
* An absolute call clobbers EAX (`mov eax, target; call eax`). EAX is a scratch
  register that no translation keeps live across a helper call - the one thing
  that is live there is push-ed as an argument first - and a helper that returns
  a value returns it in EAX anyway, so the target load cannot be observed.

The parts that are not specific to 32-bit mode - the ModRM/SIB forms, the ALU
opcodes, the condition codes, the SSE encodings - are the same bytes the x86-64
encoder emits, so the two files are deliberately similar and can be diffed.

*/

#pragma once

#include "pch.h"

// ---------------------------------------------------------------------------
// 32-bit x86 encoder

namespace X86
{
enum Reg : uint8_t
{
	EAX = 0, ECX, EDX, EBX, ESP, EBP, ESI, EDI,
};

// The XMM registers a translation works in. The generated code only ever uses
// XMM0-XMM5: XMM6-XMM15 are callee-saved on Win32 and on the i386 SysV ABI are
// not argument registers either, and the Paired-Single translations need so few
// registers that saving them would cost more than it is worth. The numbers are
// the same as on x86-64 (no REX is needed for XMM0-XMM7).
enum Xmm : uint8_t
{
	XMM0 = 0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
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

	// ModRM + SIB for [base + index*scale + disp32]. index == ESP means "no index".
	// The SIB byte is always emitted; in 32-bit mode that is the simplest form that
	// covers ESP as a base as well.
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

	void mov_r32_m(uint8_t dst, uint8_t base, int32_t disp, uint8_t index = ESP, uint8_t scale = 0)
	{
		u8(0x8B);
		mem(dst, base, index, scale, disp);
	}

	void mov_m32_r(uint8_t base, int32_t disp, uint8_t src)
	{
		u8(0x89);
		mem(src, base, ESP, 0, disp);
	}

	void mov_r32_r32(uint8_t dst, uint8_t src)
	{
		u8(0x89);
		modrmReg(src, dst);
	}

	void mov_r32_imm(uint8_t dst, uint32_t imm)
	{
		u8((uint8_t)(0xB8 + (dst & 7)));
		u32(imm);
	}

	void mov_m32_imm(uint8_t base, int32_t disp, uint32_t imm)
	{
		u8(0xC7);
		mem(0, base, ESP, 0, disp);
		u32(imm);
	}

	void mov_m8_imm(uint8_t base, int32_t disp, uint8_t imm)
	{
		u8(0xC6);
		mem(0, base, ESP, 0, disp);
		u8(imm);
	}

	void lea_r32_m(uint8_t dst, uint8_t base, int32_t disp, uint8_t index = ESP, uint8_t scale = 0)
	{
		u8(0x8D);
		mem(dst, base, index, scale, disp);
	}

	void movsx_r32_r8(uint8_t dst, uint8_t src)
	{
		u8(0x0F); u8(0xBE);
		modrmReg(dst, src);
	}

	// ---- ALU ------------------------------------------------------------

	void alu_r32_r32(uint8_t op, uint8_t dst, uint8_t src)
	{
		static const uint8_t opc[8] = { 0x03, 0x0B, 0x13, 0x1B, 0x23, 0x2B, 0x33, 0x3B };
		u8(opc[op]);
		modrmReg(dst, src);
	}

	void alu_r32_m(uint8_t op, uint8_t dst, uint8_t base, int32_t disp)		// NOLINT
	{
		static const uint8_t opc[8] = { 0x03, 0x0B, 0x13, 0x1B, 0x23, 0x2B, 0x33, 0x3B };
		u8(opc[op]);
		mem(dst, base, ESP, 0, disp);
	}

	void alu_r32_imm(uint8_t op, uint8_t dst, uint32_t imm)
	{
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
		u8(0x85);
		modrmReg(b, a);
	}

	void test_r32_imm(uint8_t r, uint32_t imm)
	{
		u8(0xF7);
		modrmReg(0, r);
		u32(imm);
	}

	void test_m8_imm(uint8_t base, int32_t disp, uint8_t imm)
	{
		u8(0xF6);
		mem(0, base, ESP, 0, disp);
		u8(imm);
	}

	void cmp_m8_imm(uint8_t base, int32_t disp, uint8_t imm)
	{
		u8(0x80);
		mem(7, base, ESP, 0, disp);
		u8(imm);
	}

	void cmp_m32_imm(uint8_t base, int32_t disp, uint32_t imm)
	{
		u8(0x81);
		mem(7, base, ESP, 0, disp);
		u32(imm);
	}

	// test r/m32, imm32 - the inlined branch condition tests a CR bit with it (see the
	// branch translations in gekkojit_x86.cpp).
	void test_m32_imm(uint8_t base, int32_t disp, uint32_t imm)
	{
		u8(0xF7);
		mem(0, base, ESP, 0, disp);
		u32(imm);
	}

	// sub r/m32, imm8 - the CTR decrement of a `bdnz`/`bdz` the translator inlines.
	void sub_m32_imm8(uint8_t base, int32_t disp, uint8_t imm)
	{
		u8(0x83);
		mem(5, base, ESP, 0, disp);
		u8(imm);
	}

	// add r/m32, imm8 - the taken-branch counter of a block that loops on its back edge.
	void add_m32_imm8(uint8_t base, int32_t disp, uint8_t imm)
	{
		u8(0x83);
		mem(0, base, ESP, 0, disp);
		u8(imm);
	}

	void cmp_r32_r32(uint8_t a, uint8_t b) { alu_r32_r32(AluCmp, a, b); }

	void imul_r32_r32(uint8_t dst, uint8_t src)
	{
		u8(0x0F); u8(0xAF);
		modrmReg(dst, src);
	}

	void neg_r32(uint8_t r)
	{
		u8(0xF7);
		modrmReg(3, r);
	}

	void not_r32(uint8_t r)
	{
		u8(0xF7);
		modrmReg(2, r);
	}

	void inc_r32(uint8_t r)
	{
		u8(0xFF);
		modrmReg(0, r);
	}

	void dec_r32(uint8_t r)
	{
		u8(0xFF);
		modrmReg(1, r);
	}

	void shl_r32_imm(uint8_t r, uint8_t n) { u8(0xC1); modrmReg(4, r); u8(n); }
	void shr_r32_imm(uint8_t r, uint8_t n) { u8(0xC1); modrmReg(5, r); u8(n); }
	void sar_r32_imm(uint8_t r, uint8_t n) { u8(0xC1); modrmReg(7, r); u8(n); }
	void rol_r32_imm(uint8_t r, uint8_t n) { u8(0xC1); modrmReg(0, r); u8(n); }

	void shl_r32_cl(uint8_t r) { u8(0xD3); modrmReg(4, r); }
	void shr_r32_cl(uint8_t r) { u8(0xD3); modrmReg(5, r); }
	void sar_r32_cl(uint8_t r) { u8(0xD3); modrmReg(7, r); }
	void rol_r32_cl(uint8_t r) { u8(0xD3); modrmReg(0, r); }

	// setcc r/m8. There is no REX register in 32-bit mode, so the low eight registers
	// are directly encodable as byte operands.
	void setcc_r8(uint8_t cc, uint8_t r)
	{
		u8(0x0F);
		u8((uint8_t)(0x90 + cc));
		modrmReg(0, r);
	}

	void movzx_r32_r8(uint8_t dst, uint8_t src)
	{
		u8(0x0F); u8(0xB6);
		modrmReg(dst, src);
	}

	// setcc into AH / movzx from AH. In 32-bit mode the low byte of EBP is not
	// encodable (mod=11 rm=101 without REX is CH, not BPL), so a translation that
	// needs three flag bits at once - the CR0 and compare updates do - puts one of
	// them in AH. The only other option is spilling to the frame, and this runs on
	// every recording-form instruction.
	void setcc_ah(uint8_t cc)
	{
		u8(0x0F);
		u8((uint8_t)(0x90 + cc));
		u8(0xC4);
	}

	void movzx_r32_ah(uint8_t dst)
	{
		u8(0x0F); u8(0xB6);
		modrmReg(dst, 4);
	}

	void bsr_r32_r32(uint8_t dst, uint8_t src)
	{
		u8(0x0F); u8(0xBD);
		modrmReg(dst, src);
	}

	void bswap_r32(uint8_t r)
	{
		u8(0x0F);
		u8((uint8_t)(0xC8 + (r & 7)));
	}

	// ---- stack / control ------------------------------------------------

	void push_r32(uint8_t r) { u8((uint8_t)(0x50 + (r & 7))); }
	void pop_r32(uint8_t r) { u8((uint8_t)(0x58 + (r & 7))); }

	// A whole 32-bit argument, so that the caller does not have to materialise it in a
	// register first (which 32-bit mode has none of to spare).
	void push_imm32(uint32_t imm) { u8(0x68); u32(imm); }

	void sub_esp_imm8(uint8_t n)
	{
		u8(0x83);
		modrmReg(5, ESP);
		u8(n);
	}

	void add_esp_imm8(uint8_t n)
	{
		u8(0x83);
		modrmReg(0, ESP);
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

	// mov eax, target ; call eax. See the note at the top of the file: EAX is a
	// translation scratch that is never live across a call.
	void call_abs(uint32_t addr)
	{
		mov_r32_imm(EAX, addr);
		u8(0xFF);
		modrmReg(2, EAX);
	}

	// ---- SSE / SSE2 ------------------------------------------------------
	// The Paired-Single translator (gekkojit_ps_x86.cpp) moves a guest PS0/PS1 pair
	// - two doubles living in two separate arrays - through one XMM register and then
	// operates on both lanes at once. The encodings are the x86-64 ones without REX.

	enum SsePrefix : uint8_t { SseNone = 0x00, Sse66 = 0x66, SseF2 = 0xF2, SseF3 = 0xF3 };

	void sse_rr(uint8_t pfx, uint8_t op, uint8_t dst, uint8_t src)
	{
		if (pfx != SseNone) u8(pfx);
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
		u8(0x0F); u8(op);
		mem(dst, base, ESP, 0, disp);
	}

	void sse_m_xmm(uint8_t pfx, uint8_t op, uint8_t base, int32_t disp, uint8_t src)
	{
		if (pfx != SseNone) u8(pfx);
		u8(0x0F); u8(op);
		mem(src, base, ESP, 0, disp);
	}

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
	void pshufd_rr(uint8_t dst, uint8_t src, uint8_t imm) { sse_rr_imm(Sse66, 0x70, dst, src, imm); }

	// movq xmm, m64. The x86-64 translator materialises a 64 bit constant in a GPR
	// (mov rax, imm64 / movq xmm, rax); 32-bit mode has neither the register width nor
	// the encoding, so a constant is written to a frame slot and loaded from there.
	void movq_xmm_m64(uint8_t x, uint8_t base, int32_t d) { sse_xmm_m(SseF3, 0x7E, x, base, d); }

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
