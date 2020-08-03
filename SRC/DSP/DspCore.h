// Macronix DSP core

#pragma once

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
			uint16_t l;
			uint16_t m1;
			uint16_t h;
			uint16_t m2;
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
			unsigned sv : 1;	// Sticky overflow. Set together with the V overflow bit, can only be cleared by the CLRB instruction.
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
		ar0 = 0,		// Addressing register 0 
		ar1,			// Addressing register 1 
		ar2,			// Addressing register 2 
		ar3,			// Addressing register 3 
		indexRegs,
		ix0 = indexRegs,	// Indexing register 0 
		ix1,			// Indexing register 1
		ix2,			// Indexing register 2
		ix3,			// Indexing register 3
		limitRegs,
		lm0 = limitRegs, // Limit register 0 
		lm1,			// Limit register 1
		lm2,			// Limit register 2
		lm3,			// Limit register 3
		stackRegs,
		st0 = stackRegs,	// Call stack register 
		st1,			// Data stack register 
		st2,			// Loop address stack register 
		st3,			// Loop counter register 
		ac0h,			// 40-bit Accumulator 0 (high) 
		ac1h,			// 40-bit Accumulator 1 (high) 
		dpp,			// Used as high 8-bits of address for some load/store instructions
		psr,			// Processor Status register 
		prodl,			// Product register (low) 
		prodm1,			// Product register (mid 1) 
		prodh,			// Product register (high) 
		prodm2,			// Product register (mid 2) 
		ax0l,			// 32-bit Accumulator 0 (low) 
		ax1l,			// 32-bit Accumulator 1 (low) 
		ax0h,			// 32-bit Accumulator 0 (high) 
		ax1h,			// 32-bit Accumulator 1 (high)
		ac0l,			// 40-bit Accumulator 0 (low) 
		ac1l,			// 40-bit Accumulator 1 (low) 
		ac0m,			// 40-bit Accumulator 0 (mid)
		ac1m,			// 40-bit Accumulator 1 (mid)
	};

	struct DspRegs
	{
		uint16_t ar[4];		// Addressing registers
		uint16_t ix[4];		// Indexing registers
		uint16_t lm[4];	// Limit registers
		std::vector<DspAddress> st[4];	// Stack registers
		DspLongAccumulator ac[2];		// 40-bit Accumulators
		DspShortOperand ax[2];		// 32-bit operands
		DspProduct prod;		// Product register
		uint16_t dpp;		// Used as high 8-bits of address for some load/store instructions
		DspStatus psr;		// Processor status
		DspAddress pc;		// Program counter
	};

	// DSP interrupts

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
	class DspInterpreter;

	class DspCore
	{
		friend DspInterpreter;

	public:
		std::list<DspAddress> breakpoints;		// IMEM breakpoints
		SpinLock breakPointsSpinLock;
		DspAddress oneShotBreakpoint = 0xffff;

		std::map<DspAddress, std::string> canaries;		// When the PC is equal to the canary address, a debug message is displayed
		SpinLock canariesSpinLock;

		const uint32_t GekkoTicksPerDspInstruction = 5;		// How many Gekko ticks should pass so that we can execute one DSP instruction
		const uint32_t GekkoTicksPerDspSegment = 100;		// How many Gekko ticks should pass so that we can execute one DSP segment (in case of Jitc)

		DspInterpreter* interp = nullptr;

		Dsp16* dsp = nullptr;

		DspInterruptControl intr = { 0 };

		void CheckInterrupts();

	public:

		static const size_t MaxInstructionSizeInBytes = 4;		// max instruction size

		DspRegs regs;

		DspCore(Dsp16 *parent);
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
		void DumpRegs(DspRegs *prevState);

		// Register access

		void MoveToReg(int reg, uint16_t val);
		uint16_t MoveFromReg(int reg);

		// Multiplier and ALU utils
		
		static int64_t SignExtend40(int64_t);
		static int64_t SignExtend16(int16_t);

		static void PackProd(DspProduct& prod);
		static void UnpackProd(DspProduct& prod);
		static DspProduct Muls(int16_t a, int16_t b, bool scale);
		static DspProduct Mulu(uint16_t a, uint16_t b, bool scale);
		static DspProduct Mulus(uint16_t a, int16_t b, bool scale);

		void ArAdvance(int r, int16_t step);
	};

	#pragma warning (pop)		// warning C4201: nonstandard extension used: nameless struct/union

}
