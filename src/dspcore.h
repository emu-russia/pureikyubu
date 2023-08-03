
#pragma once

namespace DSP
{
	// Flag modify rules

	enum class CFlagRules
	{
		None = -1,
		Zero,
		C1,		// (Ds(39) & S(39)) | (~Dd(39) & (Ds(39) | S(39)))
		C2,		// (Ds(39) & ~S(39)) | (~Dd(39) & (Ds(39) | ~S(39)))
		C3,		// Ds(39) ^ S(15) != 0 ? (Ds(39) & S(15)) | (~Dd(39) & (Ds(39) | S(15))) : (Ds(39) & ~S(15)) | (~Dd(39) & (Ds(39) | ~S(15)))
		C4,		// ~Ds(39) & ~Dd(39)
		C5,		// Ds(39) ^ S(31) == 0 ? (Ds(39) & S(39)) | (~Dd(39) & (Ds(39) | S(39))) : (Ds(39) & ~S(39)) | (~Dd(39) & (Ds(39) | ~S(39)))
		C6,		// Ds(39) & ~Dd(39)
		C7,		// P(39) & ~D(39)
		C8,		// (P(39) & S(39)) | (~D(39) & (P(39) | S(39)))
	};

	enum class VFlagRules
	{
		None = -1,
		Zero,
		V1,		// (Ds(39) & S(39) & ~Dd(39)) | (~Ds(39) & ~S(39) & Dd(39))
		V2,		// (Ds(39) & ~S(39) & ~Dd(39)) | (~Ds(39) & S(39) & Dd(39))
		V3,		// Ds(39)& Dd(39)
		V4,		// ~Ds(39) & Dd(39)
		V5,		// Dd(39)
		V6,		// ~P(39) & D(39)
		V7,		// (P(39) & S(39) & ~D(39)) | (~P(39) & ~S(39) & D(39))
		V8,		// Ds(39) & ~Dd(39)
	};

	enum class ZFlagRules
	{
		None = -1,
		Z1,		// Dd == 0
		Z2,		// Dd(31 - 16) == 0
		Z3,		// Dd(39 - 0) == 0
	};

	enum class NFlagRules
	{
		None = -1,
		N1,		// Dd(39)
		N2,		// Dd(31)
	};

	enum class EFlagRules
	{
		None = -1,
		E1,		// Dd(39 - 31) != (0b0'0000'0000 || 0b1'1111'1111)
	};

	enum class UFlagRules
	{
		None = -1,
		U1,		// ~(Dd(31) ^ Dd(30))
	};

}


// DSPcore stack implementation.

namespace DSP
{

	class DspStack
	{
		uint16_t* stack;
		int ptr = 0;
		int depth;

	public:
		DspStack(size_t _depth);
		~DspStack();

		bool push(uint16_t val);
		bool pop(uint16_t& val);
		uint16_t top();
		uint16_t at(int pos);
		bool empty();
		int size();
		void clear();
	};

}

// GameCube DSP interpreter

namespace DSP
{
	class DspCore;
}

namespace DSP
{
	class DspInterpreter
	{
		DspCore* core;

		// Regular instructions (single-word)

		void jmp();
		void call();
		void rets();
		void reti();
		void trap();
		void wait();
		void exec();
		void loop();
		void rep();
		void pld();
		void mr();
		void adsi();
		void adli();
		void cmpsi();
		void cmpli();
		void lsfi();
		void asfi();
		void xorli();
		void anli();
		void orli();
		void norm();
		void div();
		void addc();
		void subc();
		void negc();
		void _max();
		void lsf();
		void asf();
		void ld();
		void st();
		void ldsa();
		void stsa();
		void ldla();
		void stla();
		void mv();
		void mvsi();
		void mvli();
		void stli();
		void clr();
		void set();
		void btstl();
		void btsth();

		// Parallel instructions that occupy the upper part (in the lower part there is a parallel Load / Store / Move instruction)

		void p_add();
		void p_addl();
		void p_sub();
		void p_amv();
		void p_cmp();
		void p_inc();
		void p_dec();
		void p_abs();
		void p_neg();
		void p_clr();
		void p_rnd();
		void p_rndp();
		void p_tst();
		void p_lsl16();
		void p_lsr16();
		void p_asr16();
		void p_addp();
		void p_set();
		void p_mpy();
		void p_mac();
		void p_macn();
		void p_mvmpy();
		void p_rnmpy();
		void p_admpy();
		void p_not();
		void p_xor();
		void p_and();
		void p_or();
		void p_lsf();
		void p_asf();

		// Parallel mem opcodes (low part)

		void p_ldd();
		void p_ls();
		void p_ld();
		void p_st();
		void p_mv();
		void p_mr();

		// Helpers

		void FetchMpyParams(DspParameter s1p, DspParameter s2p, int64_t& s1, int64_t& s2, bool checkDp);
		void AdvanceAddress(int r, DspParameter param);
		bool ConditionTrue(ConditionCode cc);
		void Dispatch();

		/// <summary>
		/// Current decoded instruction.
		/// </summary>
		DecoderInfo info = { 0 };

		bool flowControl = false;

	public:
		DspInterpreter(DspCore* parent);
		~DspInterpreter();

		void ExecuteInstr();

	};
}



// Macronix DSP core


namespace DSP
{
#pragma warning (push)
#pragma warning (disable: 4201)

#pragma pack (push, 1)

	union DspLongAccumulator
	{
		struct
		{
			uint16_t	l;
			union
			{
				struct
				{
					uint16_t	m;
					uint16_t	h;
				};
				uint32_t hm;
				int32_t shm;
			};
		};
		uint64_t	bits;
		int64_t		sbits;
	};

	union DspShortOperand
	{
		struct
		{
			uint16_t	l;
			uint16_t	h;
		};
		uint32_t	bits;
		int32_t		sbits;
	};

	struct DspProduct
	{
		struct
		{
			uint16_t l;			// ps0
			uint16_t m1;		// ps1  (Duddie m1)
			uint16_t h;			// ps2
			uint16_t m2;		// pc1	(Duddie m2)
		};
		uint64_t bitsPacked;
	};

	union DspStatus
	{
		struct
		{
			unsigned c : 1;		// Carry
			unsigned v : 1;		// Overflow 
			unsigned z : 1;		// Zero
			unsigned n : 1;		// Negative
			unsigned e : 1;		// Extension (above s32)
			unsigned u : 1;		// Unnormalization
			unsigned tb : 1;	// Test bit (btstl/btsth instructions)
			unsigned sv : 1;	// Sticky overflow. Set together with the V overflow bit, can only be cleared by the `clr sv` instruction.
			unsigned te0 : 1;	// Interrupt enable 0 (Not used)
			unsigned te1 : 1;	// Interrupt enable 1 (Acrs, Acwe, Dcre)
			unsigned te2 : 1;	// Interrupt enable 2 (AiDma, not used by ucodes)
			unsigned te3 : 1;	// Interrupt enable 3 (CpuInt)
			unsigned et : 1;	// Global interrupt enable
			unsigned im : 1;	// Integer/fraction mode. 0: fraction mode, 1: integer mode. In fraction mode, the output of the multiplier is shifted left 1 bit to remove the sign.
			unsigned xl : 1;	// Extension limit mode. Affects the loading and saving of a/b operands.
			unsigned dp : 1;	// Double precision mode. Affects mixed multiply (xxxMPY) instructions. When DP = 1, some of the operands of these instructions are signed and some are unsigned.
		};

		uint16_t bits;
	};

#pragma pack (pop)

	enum class DspRegister
	{
		r0,		// Address register 0 (circular addressing)
		r1,		// Address register 1 (circular addressing)
		r2,		// Address register 2 (circular addressing)
		r3,		// Address register 3 (circular addressing)
		m0,		// Modifier value 0 (circular addressing)
		m1,		// Modifier value 1 (circular addressing)
		m2,		// Modifier value 2 (circular addressing)
		m3,		// Modifier value 3 (circular addressing)
		l0,		// Buffer length 0 (circular addressing)
		l1,		// Buffer length 1 (circular addressing)
		l2,		// Buffer length 2 (circular addressing)
		l3,		// Buffer length 3 (circular addressing)
		pcs,	// Program counter stack
		pss,	// Program status stack
		eas,	// End address stack
		lcs,	// Loop count stack
		a2,		// 40 - bit accumulator `a` high 8 bits
		b2,		// 40 - bit accumulator `b` high 8 bits
		dpp,	// Used as high 8 - bits of address for some load / store instructions
		psr,	// Program status register
		ps0,	// Product partial sum low part
		ps1,	// Product partial sum middle part
		ps2,	// Product partial sum high part (8 bits)
		pc1,	// Product partial carry 1 middle part
		x0,		// ALU / Multiplier input operand `x` low part
		y0,		// ALU / Multiplier input operand `y` low part
		x1,		// ALU / Multiplier input operand `x` high part
		y1,		// ALU / Multiplier input operand `y` high part
		a0,		// 40 - bit accumulator `a` low 16 bits
		b0,		// 40 - bit accumulator `b` low 16 bits
		a1,		// 40 - bit accumulator `a` middle 16 bits / Whole `a` accumulator
		b1,		// 40 - bit accumulator `b` middle 16 bits / Whole `b` accumulator
	};

	/// <summary>
	/// DSPcore registers.
	/// </summary>
	struct DspRegs
	{
		uint16_t r[4];		// Addressing registers
		uint16_t m[4];		// Modifier value registers
		uint16_t l[4];		// Buffer length registers
		DspStack* pcs;		// Program counter stack
		DspStack* pss;		// Program status stack
		DspStack* eas;		// End address stack
		DspStack* lcs;		// Loop count stack
		DspLongAccumulator a, b;	// 40-bit Accumulators
		DspShortOperand x, y;		// 32-bit operands
		DspProduct prod;			// Product register
		uint16_t dpp;		// Used as high 8-bits of address for some load/store instructions
		DspStatus psr;		// Processor status
		DspAddress pc;		// Program counter
	};

	/// <summary>
	/// DSP interrupts.
	/// </summary>
	enum class DspInterrupt
	{
		Reset = 0,	// Soft reset
		Error,		// Stack underflow/overflow
		Trap,		// Trap instruction
		Acrs,		// Accelerator read start (TE1)
		Acwe,		// Accelerator write end (TE1)
		Dcre,		// Decoder read end (TE1)
		AiDma,		// Not used (TE2)
		CpuInt,		// External interrupt (from CPU) (TE3)

		Max,
	};

	struct DspInterruptControl
	{
		bool pendingSomething;
		int pendingDelay[(size_t)DspInterrupt::Max];
		bool pending[(size_t)DspInterrupt::Max];
	};

	class Dsp16;

	/// <summary>
	/// Macronix DSP core.
	/// </summary>
	class DspCore
	{
		friend DspInterpreter;
		friend DspUnitTest::DspUnitTest;

		std::list<DspAddress> breakpoints;		// IMEM breakpoints
		SpinLock breakPointsSpinLock;
		DspAddress oneShotBreakpoint = 0xffff;

		std::map<DspAddress, std::string> canaries;		// When the PC is equal to the canary address, a debug message is displayed
		SpinLock canariesSpinLock;

		std::list<DspAddress> watches;		// DMEM watches
		SpinLock watchesSpinLock;

		const uint32_t GekkoTicksPerDspInstruction = 5;		// How many Gekko ticks should pass so that we can execute one DSP instruction
		const uint32_t GekkoTicksPerDspSegment = 100;		// How many Gekko ticks should pass so that we can execute one DSP segment (in case of Jitc)

		DspInterpreter* interp = nullptr;

		Dsp16* dsp = nullptr;

		DspInterruptControl intr = { 0 };

		void CheckInterrupts();
		uint16_t CircularAddress(uint16_t r, uint16_t l, int16_t m);

		int repeatCount = 0;		// Internal register for the `rep` instruction.

		int64_t instructionCounter = 0;
		bool resetInstructionCounter = false;

	public:

		static const size_t MaxInstructionSizeInBytes = 4;		// max instruction size

		DspRegs regs;

		DspCore(Dsp16* parent);
		~DspCore();

		void AssertInterrupt(DspInterrupt id);
		bool IsInterruptPending(DspInterrupt id);
		void ReturnFromInterrupt();
		void HardReset();

		void Update();

		// Debug methods

		void AddBreakpoint(DspAddress imemAddress);
		void RemoveBreakpoint(DspAddress imemAddress);
		void ListBreakpoints();
		void ClearBreakpoints();
		bool TestBreakpoint(DspAddress imemAddress);
		void ToggleBreakpoint(DspAddress imemAddress);
		void AddOneShotBreakpoint(DspAddress imemAddress);
		void AddCanary(DspAddress imemAddress, std::string text);
		void ListCanaries();
		void ClearCanaries();
		bool TestCanary(DspAddress imemAddress);
		void Step();
		void DumpRegs(DspRegs* prevState);
		void AddWatch(DspAddress dmemAddress);
		void RemoveWatch(DspAddress dmemAddress);
		void RemoveAllWatches();
		void ListWatches(std::list<DspAddress>& watches);
		bool TestWatch(DspAddress dmemAddress);
		int64_t GetInstructionCounter();
		void ResetInstructionCounter();

		// Register access

		void MoveToReg(int reg, uint16_t val);
		uint16_t MoveFromReg(int reg);

		// Multiplier and ALU utils

		static int64_t SignExtend16(int16_t);
		static int64_t SignExtend32(int32_t);
		static int64_t SignExtend40(int64_t);

		static void PackProd(DspProduct& prod);
		static void UnpackProd(DspProduct& prod);

		void ArAdvance(int r, int16_t step);

		void ModifyFlags(uint64_t d, uint64_t s, uint64_t r, CFlagRules, VFlagRules, ZFlagRules, NFlagRules, EFlagRules, UFlagRules);

		static int64_t RndFactor(int64_t d);
	};

#pragma warning (pop)		// warning C4201: nonstandard extension used: nameless struct/union

}
