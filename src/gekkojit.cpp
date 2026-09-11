/*

Gekko -> x86-64 basic block recompiler. See gekkojit.h for the design notes.

The emitter below only implements the exact operand forms that the translator
needs, which keeps it small enough to be reviewable.

Division of labour: the generated code translates instructions and keeps the pc
in a register; everything that has to happen once per block - retiring the
instruction counter, advancing the time base and the decrementer, the branch
check - is done in C++ by Jit::Run when the block returns. That keeps the
generated code (and this file) much smaller and keeps the timing model in one
readable place.

*/

#include "pch.h"
#include "gekkojit.h"

#if GEKKO_JIT_SUPPORTED

#if defined(_WINDOWS)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

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

// ---------------------------------------------------------------------------
// Thin trampolines for the memory helpers. A pointer-to-member-function is not a
// code address, so the generated code calls these instead; at -O2 they are a tail
// jump into the real helper.

namespace
{
	void JitReadByte(GekkoCore* core, uint32_t addr, uint32_t* reg) { core->ReadByte(addr, reg); }
	void JitReadWord(GekkoCore* core, uint32_t addr, uint32_t* reg) { core->ReadWord(addr, reg); }
	void JitWriteByte(GekkoCore* core, uint32_t addr, uint32_t data) { core->WriteByte(addr, data); }
	void JitWriteHalf(GekkoCore* core, uint32_t addr, uint32_t data) { core->WriteHalf(addr, data); }
	void JitWriteWord(GekkoCore* core, uint32_t addr, uint32_t data) { core->WriteWord(addr, data); }
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

	X64::Emitter e(code + codeUsed, CodeArenaSize - codeUsed);
	size_t start = codeUsed;

	auto gprOff = [](uint32_t n) { return GprOff + (int32_t)n * 4; };

	auto loadGpr = [&](uint8_t host, uint32_t n) { e.mov_r32_m(host, RegRegs, gprOff(n)); };
	auto storeGpr = [&](uint32_t n, uint8_t host) { e.mov_m32_r(RegRegs, gprOff(n), host); };

	// COMPUTE_CR0, identical to the interpreter macro (including the SO copy).
	auto emitCR0 = [&](uint8_t src)
	{
		// All three condition bits come from one comparison, so the registers have to
		// be zeroed without touching the flags (`mov`, not `xor`).
		e.mov_r32_imm(X64::RCX, 0);
		e.mov_r32_imm(X64::RDX, 0);
		e.mov_r32_imm(TC, 0);
		e.test_r32_r32(src, src);
		e.setcc_r8(X64::CcS, X64::RCX);
		e.setcc_r8(X64::CcG, X64::RDX);
		e.setcc_r8(X64::CcE, TC);
		e.shl_r32_imm(X64::RCX, 31);
		e.shl_r32_imm(X64::RDX, 30);
		e.shl_r32_imm(TC, 29);
		e.or_r32_r32(X64::RCX, X64::RDX);
		e.or_r32_r32(X64::RCX, TC);

		e.mov_r32_m(X64::RAX, RegRegs, XerOff);
		e.and_r32_imm(X64::RAX, GEKKO_XER_SO);
		e.shr_r32_imm(X64::RAX, 3);						// bit 31 -> bit 28
		e.or_r32_r32(X64::RCX, X64::RAX);

		e.mov_r32_m(X64::RAX, RegRegs, CrOff);
		e.and_r32_imm(X64::RAX, 0x0fff'ffff);
		e.or_r32_r32(X64::RAX, X64::RCX);
		e.mov_m32_r(RegRegs, CrOff, X64::RAX);
	};

	// XER[CA] = carry of the previous ALU op / = 0.
	auto emitSetCa = [&]()
	{
		e.setcc_r8(X64::CcB, X64::RCX);
		e.movzx_r32_r8(X64::RCX, X64::RCX);
		e.shl_r32_imm(X64::RCX, 29);
		e.mov_r32_m(X64::RAX, RegRegs, XerOff);
		e.and_r32_imm(X64::RAX, ~(uint32_t)GEKKO_XER_CA);
		e.or_r32_r32(X64::RAX, X64::RCX);
		e.mov_m32_r(RegRegs, XerOff, X64::RAX);
	};

	// XER[CA] = the *inverted* carry of the previous ALU op. subfic computes
	// rD = SIMM - rA as an x86 `sub`, whose CF is the borrow, while the emulator
	// takes the carry out of ~rA + SIMM + 1 - the two are complements.
	auto emitSetCaInverted = [&]()
	{
		e.setcc_r8(X64::CcAE, X64::RCX);
		e.movzx_r32_r8(X64::RCX, X64::RCX);
		e.shl_r32_imm(X64::RCX, 29);
		e.mov_r32_m(X64::RAX, RegRegs, XerOff);
		e.and_r32_imm(X64::RAX, ~(uint32_t)GEKKO_XER_CA);
		e.or_r32_r32(X64::RAX, X64::RCX);
		e.mov_m32_r(RegRegs, XerOff, X64::RAX);
	};

	auto emitResetCa = [&]()
	{
		e.mov_r32_m(X64::RAX, RegRegs, XerOff);
		e.and_r32_imm(X64::RAX, ~(uint32_t)GEKKO_XER_CA);
		e.mov_m32_r(RegRegs, XerOff, X64::RAX);
	};

	// Effective address into EAX. D-form when 'imm' is given, otherwise indexed.
	auto emitEffectiveAddress = [&](uint32_t ra, uint32_t rb, bool indexed, bool zeroRa, int32_t imm)
	{
		if (zeroRa)
		{
			e.xor_r32_r32_same(X64::RAX);
		}
		else
		{
			loadGpr(X64::RAX, ra);
		}

		if (indexed)
		{
			e.alu_r32_m(X64::AluAdd, X64::RAX, RegRegs, gprOff(rb));
		}
		else
		{
			e.add_r32_imm(X64::RAX, (uint32_t)imm);
		}
	};

	auto emitExcCheck = [&](std::vector<size_t>& excJumps)
	{
		e.cmp_m8_imm(RegCore, exceptionOffset, 0);
		excJumps.push_back(e.jcc_rel32(X64::CcNE));
	};

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
	e.mov_r64_r64(RegRegs, Arg1);
	e.mov_r64_r64(RegExit, Arg2);
	e.mov_r32_m(RegPc, RegRegs, PcOff);
	e.xor_r32_r32_same(RegCount);

	std::vector<size_t> normalExits;		// jump to the normal epilogue
	std::vector<size_t> takenExits;			// jump to the taken branch epilogue
	std::vector<size_t> exceptionExits;		// jump to the exception epilogue

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

		e.inc_r64(RegCount);
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
			e.alu_r32_m(X64::AluAdd, T0, RegRegs, gprOff(rb));
			storeGpr(rd, T0);
			if (di.instr == Instruction::add_d) emitCR0(T0);
			break;

		case Instruction::subf:
		case Instruction::subf_d:
			loadGpr(T0, rb);
			e.alu_r32_m(X64::AluSub, T0, RegRegs, gprOff(ra));
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
			e.alu_r32_m(X64::AluOr, T0, RegRegs, gprOff(rb));
			storeGpr(rd, T0);
			if (di.instr == Instruction::or_d) emitCR0(T0);
			break;

		case Instruction::_and:
		case Instruction::and_d:
			loadGpr(T0, ra);
			e.alu_r32_m(X64::AluAnd, T0, RegRegs, gprOff(rb));
			storeGpr(rd, T0);
			if (di.instr == Instruction::and_d) emitCR0(T0);
			break;

		case Instruction::_xor:
		case Instruction::xor_d:
			loadGpr(T0, ra);
			e.alu_r32_m(X64::AluXor, T0, RegRegs, gprOff(rb));
			storeGpr(rd, T0);
			if (di.instr == Instruction::xor_d) emitCR0(T0);
			break;

		case Instruction::andc:
		case Instruction::andc_d:
			loadGpr(T0, rb);
			e.not_r32(T0);
			e.alu_r32_m(X64::AluAnd, T0, RegRegs, gprOff(ra));
			storeGpr(rd, T0);
			if (di.instr == Instruction::andc_d) emitCR0(T0);
			break;

		case Instruction::orc:
		case Instruction::orc_d:
			loadGpr(T0, rb);
			e.not_r32(T0);
			e.alu_r32_m(X64::AluOr, T0, RegRegs, gprOff(ra));
			storeGpr(rd, T0);
			if (di.instr == Instruction::orc_d) emitCR0(T0);
			break;

		case Instruction::nand:
		case Instruction::nand_d:
			loadGpr(T0, ra);
			e.alu_r32_m(X64::AluAnd, T0, RegRegs, gprOff(rb));
			e.not_r32(T0);
			storeGpr(rd, T0);
			if (di.instr == Instruction::nand_d) emitCR0(T0);
			break;

		case Instruction::nor:
		case Instruction::nor_d:
			loadGpr(T0, ra);
			e.alu_r32_m(X64::AluOr, T0, RegRegs, gprOff(rb));
			e.not_r32(T0);
			storeGpr(rd, T0);
			if (di.instr == Instruction::nor_d) emitCR0(T0);
			break;

		case Instruction::eqv:
		case Instruction::eqv_d:
			loadGpr(T0, ra);
			e.alu_r32_m(X64::AluXor, T0, RegRegs, gprOff(rb));
			e.not_r32(T0);
			storeGpr(rd, T0);
			if (di.instr == Instruction::eqv_d) emitCR0(T0);
			break;

		case Instruction::ori:
			loadGpr(T0, ra);
			e.alu_r32_imm(X64::AluOr, T0, di.Imm.Unsigned);
			storeGpr(rd, T0);
			break;

		case Instruction::oris:
			loadGpr(T0, ra);
			e.alu_r32_imm(X64::AluOr, T0, (uint32_t)di.Imm.Unsigned << 16);
			storeGpr(rd, T0);
			break;

		case Instruction::xori:
			loadGpr(T0, ra);
			e.alu_r32_imm(X64::AluXor, T0, di.Imm.Unsigned);
			storeGpr(rd, T0);
			break;

		case Instruction::xoris:
			loadGpr(T0, ra);
			e.alu_r32_imm(X64::AluXor, T0, (uint32_t)di.Imm.Unsigned << 16);
			storeGpr(rd, T0);
			break;

		case Instruction::andi_d:
			loadGpr(T0, ra);
			e.alu_r32_imm(X64::AluAnd, T0, di.Imm.Unsigned);
			storeGpr(rd, T0);
			emitCR0(T0);
			break;

		case Instruction::andis_d:
			loadGpr(T0, ra);
			e.alu_r32_imm(X64::AluAnd, T0, (uint32_t)di.Imm.Unsigned << 16);
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
			size_t zero = e.jcc_rel32(X64::CcE);
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

			// Zero the accumulators without disturbing the flags, then take all three
			// condition bits from the single comparison.
			e.mov_r32_imm(X64::RCX, 0);
			e.mov_r32_imm(X64::RDX, 0);
			e.mov_r32_imm(TC, 0);

			e.cmp_r32_r32(T0, T1);

			e.setcc_r8(isSigned ? X64::CcL : X64::CcB, X64::RCX);
			e.setcc_r8(isSigned ? X64::CcG : X64::CcA, X64::RDX);
			e.setcc_r8(X64::CcE, TC);

			e.shl_r32_imm(X64::RCX, 3);
			e.shl_r32_imm(X64::RDX, 2);
			e.shl_r32_imm(TC, 1);
			e.or_r32_r32(X64::RCX, X64::RDX);
			e.or_r32_r32(X64::RCX, TC);

			e.mov_r32_m(T3, RegRegs, XerOff);
			e.shr_r32_imm(T3, 31);						// XER[SO] -> bit 0
			e.or_r32_r32(X64::RCX, T3);

			int32_t shift = 28 - 4 * (int32_t)crfd;
			if (shift) e.shl_r32_imm(X64::RCX, (uint8_t)shift);
			e.mov_r32_m(T3, RegRegs, CrOff);
			e.and_r32_imm(T3, ~(0xfu << shift));
			e.or_r32_r32(T3, X64::RCX);
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
				loadGpr(X64::RCX, rb);
				e.and_r32_imm(X64::RCX, 0x1f);
				e.rol_r32_cl(T0);
			}
			else	// rlwimi
			{
				loadGpr(T1, rd);
				e.and_r32_imm(T1, ~mask);
				if (di.paramBits[2]) e.rol_r32_imm(T0, (uint8_t)di.paramBits[2]);
				e.alu_r32_imm(X64::AluAnd, T0, mask);
				e.or_r32_r32(T0, T1);
				storeGpr(rd, T0);
				if (di.instr == Instruction::rlwimi_d) emitCR0(T0);
				break;
			}

			e.alu_r32_imm(X64::AluAnd, T0, mask);
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
			size_t big = e.jcc_rel32(X64::CcNE);
			e.mov_r32_r32(X64::RCX, T1);
			e.and_r32_imm(X64::RCX, 0x1f);
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
				e.mov_r32_r32(T1, T0);
				e.sar_r32_imm(T1, (uint8_t)n);
				e.mov_r32_r32(T2, T0);
				e.mov_r32_r32(X64::RCX, T2);
				e.shl_r32_imm(T2, (uint8_t)(32 - n));
				e.test_r32_r32(T2, T2);					// ZF = (shifted == 0)
				e.setcc_r8(X64::CcNE, X64::RDX);
				e.shr_r32_imm(X64::RCX, 31);			// src < 0
				e.and_r32_r32(X64::RDX, X64::RCX);
				e.movzx_r32_r8(X64::RDX, X64::RDX);
				e.shl_r32_imm(X64::RDX, 29);			// -> XER[CA]

				e.mov_r32_m(T3, RegRegs, XerOff);
				e.and_r32_imm(T3, ~(uint32_t)GEKKO_XER_CA);
				e.or_r32_r32(T3, X64::RDX);
				e.mov_m32_r(RegRegs, XerOff, T3);
			}

			storeGpr(rd, T1);
			if (di.instr == Instruction::srawi_d) emitCR0(T1);
			break;
		}

		// ---- loads ------------------------------------------------------

		// Halfword loads (lhz/lha and their indexed and update forms) are left to the
		// interpreter. Random-program differential testing still found rare
		// disagreements between the two engines in exactly that family, so it is kept
		// on the interpreter until they are understood; the byte and word loads are
		// clean and stay translated.

		case Instruction::lbz: case Instruction::lbzu: case Instruction::lbzx: case Instruction::lbzux:
		case Instruction::lwz: case Instruction::lwzu: case Instruction::lwzx: case Instruction::lwzux:
		{
			bool indexed = (di.instr == Instruction::lbzx || di.instr == Instruction::lbzux ||
				di.instr == Instruction::lwzx || di.instr == Instruction::lwzux);
			bool update = (di.instr == Instruction::lbzu || di.instr == Instruction::lbzux ||
				di.instr == Instruction::lwzu || di.instr == Instruction::lwzux);
			bool zeroRa = !indexed && !update && ra == 0;

			emitEffectiveAddress(ra, rb, indexed, zeroRa, (int32_t)di.Imm.Signed);
			e.mov_r32_r32(RegEa, X64::RAX);

			// regs.pc has to be current before the helper runs: an access fault makes
			// the helper raise the exception, which stores regs.pc into SRR0.
			e.mov_m32_r(RegRegs, PcOff, RegPc);

			uint64_t fn = 0;
			switch (di.instr)
			{
			case Instruction::lbz: case Instruction::lbzu: case Instruction::lbzx: case Instruction::lbzux:
				fn = (uint64_t)(void*)&JitReadByte; break;
			default:
				fn = (uint64_t)(void*)&JitReadWord; break;
			}

			e.mov_r64_r64(Arg0, RegCore);
			e.mov_r32_r32(Arg1, X64::RAX);
			e.lea_r64_m(Arg2, RegRegs, gprOff(rd));
			e.call_abs(fn);

			emitExcCheck(exceptionExits);

			if (update)
			{
				e.mov_r32_r32(T0, RegEa);
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
			bool zeroRa = !indexed && !update && ra == 0;

			emitEffectiveAddress(ra, rb, indexed, zeroRa, (int32_t)di.Imm.Signed);
			e.mov_r32_r32(RegEa, X64::RAX);

			// regs.pc has to be current before the helper runs: an access fault makes
			// the helper raise the exception, which stores regs.pc into SRR0.
			e.mov_m32_r(RegRegs, PcOff, RegPc);

			uint64_t fn = 0;
			switch (di.instr)
			{
			case Instruction::stb: case Instruction::stbu: case Instruction::stbx: case Instruction::stbux:
				fn = (uint64_t)(void*)&JitWriteByte; break;
			case Instruction::sth: case Instruction::sthu: case Instruction::sthx: case Instruction::sthux:
				fn = (uint64_t)(void*)&JitWriteHalf; break;
			default:
				fn = (uint64_t)(void*)&JitWriteWord; break;
			}

			e.mov_r64_r64(Arg0, RegCore);
			e.mov_r32_r32(Arg1, X64::RAX);
			loadGpr(Arg2, rd);
			e.call_abs(fn);

			emitExcCheck(exceptionExits);

			if (update)
			{
				e.mov_r32_r32(T0, RegEa);
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
			e.mov_r64_r64(Arg0, RegCore);
			e.mov_r32_imm(Arg1, (uint32_t)di.paramBits[0]);
			e.mov_r32_imm(Arg2, (uint32_t)di.paramBits[1]);
			e.call_abs((uint64_t)(void*)&Jit::BcTest);
			e.movzx_r32_r8(X64::RAX, X64::RAX);		// bool comes back in AL only
			e.test_r32_r32(X64::RAX, X64::RAX);
			size_t notTaken = e.jcc_rel32(X64::CcE);

			if (di.instr == Instruction::bcl || di.instr == Instruction::bcla)
			{
				e.mov_r32_imm(T0, curPc + 4);
				e.mov_m32_r(RegRegs, LrOff, T0);
			}

			e.mov_r32_imm(RegPc, di.Imm.Address);
			takenExits.push_back(e.jmp_rel32());

			e.patch32(notTaken, e.rel(notTaken));
			e.add_r32_imm(RegPc, 4);				// not taken: pc += 4
			endBlock = true;
			break;
		}

		case Instruction::bclr:
		case Instruction::bclrl:
		{
			e.mov_r64_r64(Arg0, RegCore);
			e.mov_r32_imm(Arg1, (uint32_t)di.paramBits[0]);
			e.mov_r32_imm(Arg2, (uint32_t)di.paramBits[1]);
			e.call_abs((uint64_t)(void*)&Jit::BcTest);
			e.movzx_r32_r8(X64::RAX, X64::RAX);
			e.test_r32_r32(X64::RAX, X64::RAX);
			size_t notTaken = e.jcc_rel32(X64::CcE);

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

			e.patch32(notTaken, e.rel(notTaken));
			e.add_r32_imm(RegPc, 4);
			endBlock = true;
			break;
		}

		case Instruction::bcctr:
		case Instruction::bcctrl:
		{
			e.mov_r64_r64(Arg0, RegCore);
			e.mov_r32_imm(Arg1, (uint32_t)di.paramBits[0]);
			e.mov_r32_imm(Arg2, (uint32_t)di.paramBits[1]);
			e.call_abs((uint64_t)(void*)&Jit::BctrTest);
			e.movzx_r32_r8(X64::RAX, X64::RAX);
			e.test_r32_r32(X64::RAX, X64::RAX);
			size_t notTaken = e.jcc_rel32(X64::CcE);

			if (di.instr == Instruction::bcctrl)
			{
				e.mov_r32_imm(T0, curPc + 4);
				e.mov_m32_r(RegRegs, LrOff, T0);
			}

			e.mov_r32_m(RegPc, RegRegs, CtrOff);
			e.and_r32_imm(RegPc, ~3u);
			takenExits.push_back(e.jmp_rel32());

			e.patch32(notTaken, e.rel(notTaken));
			e.add_r32_imm(RegPc, 4);
			endBlock = true;
			break;
		}

		default:
			handled = false;
			break;
		}

		if (!handled)
		{
			// Run this one instruction through the interpreter. The handler owns its
			// exact semantics (including the timing side effects of mfspr/mtspr).
			e.mov_m32_r(RegRegs, PcOff, RegPc);
			e.mov_r64_r64(Arg0, RegCore);
			e.mov_r32_imm(Arg1, instr);
			e.mov_r32_imm(Arg2, curPc);
			e.call_abs((uint64_t)(void*)&Jit::Fallback);
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

		e.mov_r64_r64(T0, RegExit);
		e.mov_m64_r(T0, 0, RegCount);
		e.mov_m32_imm(T0, 8, (uint32_t)kind);
	};

	auto emitEpilogue = [&]()
	{
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

	return offset;
}

void Jit::Run()
{
	if (!supported)
	{
		core->interp->ExecuteOpcode();
		return;
	}

	uint32_t pc = core->regs.pc;

	// The boot ROM is fetched straight from the PI and never through the instruction
	// cache, so there is nothing to compile for it - and no reason to walk the block
	// cache for every instruction of it either.
	if (pc >= PI_MEMSPACE_BOOTROM)
	{
		core->interp->ExecuteOpcode();
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
		core->interp->ExecuteOpcode();
		return;
	}

	int WIMG = 0;
	uint32_t pa = core->EffectiveToPhysical(pc, MmuAccess::Execute, WIMG);
	if (pa == BadAddress || pa >= PI_MEMSPACE_BOOTROM || core->icache->GetCachePointer(pa) == nullptr ||
		(WIMG & WIMG_I) != 0)
	{
		core->interp->ExecuteOpcode();
		return;
	}

	Block* block = FindBlock(pc, pa);

	if (block == nullptr)
	{
		uint32_t count = 0;
		if (CompileBlock(pc, pa, count) == BadAddress)
		{
			core->interp->ExecuteOpcode();
			return;
		}
		block = FindBlock(pc, pa);
		if (block == nullptr)
		{
			core->interp->ExecuteOpcode();
			return;
		}
	}

	typedef void (*BlockFn)(GekkoCore*, GekkoRegs*, JitExit*);

	JitExit exit{};
	BlockFn fn = (BlockFn)(void*)(code + block->codeOffset);
	fn(core, &core->regs, &exit);

	uint32_t n = (uint32_t)exit.count;

	// Mirror Interpreter::ExecuteOpcode: an instruction that raises an exception
	// does not advance the tick, and a taken branch is ticked once by BranchCheck
	// and once more by the instruction loop.
	switch ((JitExitKind)exit.kind)
	{
	case JitExitKind::Exception:
		if (n > 0) core->TickN(n - 1);
		break;

	case JitExitKind::TakenBranch:
		// The n instructions include the taken branch itself, which the interpreter
		// ticks twice: once inside BranchCheck and once at the end of the loop.
		if (n > 0) core->TickN(n);
		interp->BranchCheck();
		break;

	default:
		if (n > 0) core->TickN(n);
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

#else // !GEKKO_JIT_SUPPORTED

// 32-bit x86 and non-x86 hosts keep the interpreter: the recompiler is a no-op that
// reports itself as unsupported, so GekkoCore::GekkoThreadProc uses the interpreter.

namespace Gekko
{
	Jit::Jit(GekkoCore* _core) : core(_core)
	{
		interp = core->interp;
		supported = false;
	}

	Jit::~Jit() {}

	void Jit::InvalidateAll() {}

	void Jit::Run()
	{
		core->interp->ExecuteOpcode();
	}

	bool Jit::FetchInstr(uint32_t pc, uint32_t& instr, uint32_t& pa) { pa = BadAddress; return false; }

	void* Jit::AllocCode(size_t size) { return nullptr; }

	void Jit::FreeCode(void* ptr, size_t size) {}

	uint32_t Jit::CompileBlock(uint32_t pc, uint32_t pa, uint32_t& instrCount) { return BadAddress; }

	Jit::Block* Jit::FindBlock(uint32_t pc, uint32_t pa) { return nullptr; }

	Jit::Block* Jit::AllocBlock(uint32_t pc, uint32_t pa) { return nullptr; }

	void Jit::Fallback(GekkoCore* core, uint32_t instr, uint32_t pc) {}

	bool Jit::BcTest(GekkoCore* core, uint32_t bo, uint32_t bi) { return false; }

	bool Jit::BctrTest(GekkoCore* core, uint32_t bo, uint32_t bi) { return false; }

	void Jit::BranchCheck(GekkoCore* core) {}
}

#endif // GEKKO_JIT_SUPPORTED
