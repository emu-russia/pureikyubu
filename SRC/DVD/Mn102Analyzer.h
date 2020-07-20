// Matsushita MN102 instruction analyzer.

#pragma once

namespace DVD
{

	enum class MnInstruction
	{
		Unknown = 0,

		MOV,
		MOVX,
		MOVB,
		MOVBU,
		EXT,
		EXTX,
		EXTXU,
		EXTXB,
		EXTXBU,
		ADD,
		ADDC,
		ADDNF,
		SUB,
		SUBC,
		MUL,
		MULU,
		DIVU,
		CMP,
		AND,
		OR,
		XOR,
		NOT,
		ASR,
		LSR,
		ROR,
		ROL,
		BTST,
		BSET,
		BCLR,
		Bcc,
		BccX,
		JMP,
		JSR,
		NOP,
		RTS,
		RTI
	};

	enum class MnOperand
	{
		Unknown = 0,

		// Registers
		D0,
		D1,
		D2,
		D3,
		A0,
		A1,
		A2,
		A3,

		// Immediate
		Imm8,
		Imm16,
		Imm24,

		// Register indirect
		Ind_A0,
		Ind_A1,
		Ind_A2,
		Ind_A3,

		// Register relative indirect
		D8_A0,
		D8_A1,
		D8_A2,
		D8_A3,
		D16_A0,
		D16_A1,
		D16_A2,
		D16_A3,
		D24_A0,
		D24_A1,
		D24_A2,
		D24_A3,
		D8_PC,
		D16_PC,
		D24_PC,

		// Absolute
		Abs16,
		Abs24,

		// Indirect Register
		Ind_D0_A0,
		Ind_D0_A1,
		Ind_D0_A2,
		Ind_D0_A3,
		Ind_D1_A0,
		Ind_D1_A1,
		Ind_D1_A2,
		Ind_D1_A3,
		Ind_D2_A0,
		Ind_D2_A1,
		Ind_D2_A2,
		Ind_D2_A3,
		Ind_D3_A0,
		Ind_D3_A1,
		Ind_D3_A2,
		Ind_D3_A3,

		MDR,
		PSW,

	};

	// Condition code for branch instructions
	enum class MnCond
	{
		Unknown = -1,

		LT = 0,
		GT = 1,
		GE = 2,
		LE = 3,
		CS = 4,
		HI = 5,
		CC = 6,
		LS = 7,
		EQ = 8,
		NE = 9,
		RA = 0xa,

		VC = 0xc,
		VS = 0xd,
		NC = 0xe,
		NS = 0xf,
	};

	union MnInstrImmed
	{
		uint8_t		Uint8;
		uint16_t	Uint16;
		uint32_t	Uint24;
	};

	struct MnInstrInfo
	{
		size_t instrSize;			// In bytes
		uint8_t instrBytes[5];		// Saved instruction bytes

		MnInstruction instr;

		size_t numOp;
		MnOperand op[2];
		int opBits[2];			// If one of the operands is in the form (Di, An), then Di and An saved as one

		MnInstrImmed imm;

		bool	flow;				// Kind of branch (can break instruction flow)

		MnCond	cc;				// Condition code for branches

	};

	class MnAnalyze
	{
		uint8_t* instrPtr = nullptr;		// Temporary pointer

		bool Main(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info);
		bool F0(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info);
		bool F1(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info);
		bool F2(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info);
		bool F3(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info);
		bool F4(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info);
		bool F5(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info);
		bool F7(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info);

		bool FetchImm8(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info);
		bool FetchImm16(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info);
		bool FetchImm24(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info);

		bool AddOp(MnInstrInfo* info, MnOperand op, int bits);

	public:

		bool Analyze(uint8_t * instrPtr, size_t instrMaxSize, MnInstrInfo * info);

	};
}
