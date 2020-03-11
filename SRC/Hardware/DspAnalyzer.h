/// DSP analyzer.
/// Can be used by disassembler as well as by interpreter/jitc.

#pragma once

namespace DSP
{

	enum class DspInstruction
	{
		Unknown = -1,

		ADD,		///< Adds accumulator $ac(1-D) to accumulator register $acD
		ADDARN,		///< Adds indexing register $ixS to an addressing register $arD
		ADDAX,		///< Adds secondary accumulator $axS to accumulator register $acD
		ADDAXL,		///< Adds secondary accumulator $axS.l to accumulator register $acD
		ADDI,		///< Adds a 16-bit sign-extended immediate to mid accumulator $acD.hm
		ADDIS,		///< Adds an 8-bit sign-extended immediate to mid accumulator $acD.hm
		ADDP,		///< Adds the product register to the accumulator register
		ADDPAXZ,	///< Adds secondary accumulator $axS to product register and stores result in accumulator register. Low 16-bits of $acD ($acD.l) are set to 0
		ADDR,		///< Adds register $(0x18+S) to the accumulator $acD register

		ANDC,		///< Logic AND middle part of accumulator $acD.m with middle part of accumulator $ax(1-D).m. 
		ANDCF,		///< Logical AND operation involving the mid part of accumulator $acD.m and the immediate value I is equal to I
		ANDF,		///< Logic AND operation involving the mid part of accumulator $acD.m and the immediate value I is equal to zero
		ANDI,		///< Logical AND with the mid part of accumulator $acD.m and the immediate value I
		ANDR,		///< Logical AND with the middle part of accumulator $acD.m and the high part of secondary accumulator, $axS.h

		ASL,		///< Arithmetically left shifts the accumulator $acR by the amount speciﬁed by immediate I
		ASR,		///< Arithmetically right shifts accumulator $acR speciﬁed by the value calculated by negating sign-extended bits 0-6
		ASR16,		///< Arithmetically right shifts accumulator $acR by 16

		BLOOP,		///< Block loop (Counter in register)
		BLOOPI,		///< Block loop (Counter in immediate operand)
		CALL,		///< Call function
		CALLcc,		///< Call function if condition cc has been met
		CALLR,		///< Call function (register)

		CLR,		///< Clears accumulator $acR
		CLRL,		///< Clears $acR.l - low 16 bits of accumulator $acR
		CLRP,		///< Clears product register $prod

		CMP,		///< Compares accumulator $ac0 with accumulator $ac1
		CMPI,		///< Compares mid accumulator $acD.hm ($amD) with sign-extended immediate value I
		CMPIS,		///< Compares accumulator with short immediate

		DAR,		///< Decrement address register $arD. 
		DEC,		///< Decrements accumulator $acD. 
		DECM,		///< Decrements 24-bit mid-accumulator $acsD

		HALT,		///< Stops execution of DSP code. Sets bit DSP_CR_HALT in register DREG_CR

		IAR,		///< Increment address register $arD

		IFcc,		///< Executes the following opcode if the condition described by cccc has been met

		ILRR,		///< Move value from instruction memory pointed by addressing register $arS to mid accumulator register $acD.m
		ILRRD,		///< Move value from instruction memory pointed by addressing register $arS to mid accumulator register $acD.m. Decrement addressing register $arS
		ILRRI,		///< Move value from instruction memory pointed by addressing register $arS to mid accumulator register $acD.m. Increment addressing register $arS
		ILRRN,		///< Move value from instruction memory pointed by addressing register $arS to mid accumulator register $acD.m. Add corresponding indexing register $ixS to addressing register $arS

		INC,		///< Increments accumulator $acD
		INCM,		///< Increments 24-bit mid-accumulator $acsD

		JMP,		///< Jumps to addressA
		Jcc,		///< Jumps to addressA if condition cc has been met
		JMPR,		///< Jump to address (by register)
		LOOP,		///< Loop (by register)
		LOOPI,		///< Loop (immediate operand)

		LR,			///< Move value from data memory pointed by address M to register $D. 
		LRI,		///< Load immediate value I to register $D
		LRIS,		///< Load immediate value I (8-bit sign-extended) to accumulator register 
		LRR,		///< Move value from data memory pointed by addressing register $S to register $D
		LRRD,		///< Move value from data memory pointed by addressing register $S to register $D. Decrements register $S
		LRRI,		///< Move value from data memory pointed by addressing register $S to register $D. Increments register $S
		LRRN,		///< Move value from data memory pointed by addressing register $S to register $D. Add indexing register $(0x4+S) to register $S
		LRS,		///< Move value from data memory pointed by address M (8-bitsign-extended) to register $(0x18+D)

		LSL,		///< Logically left shifts accumulator $acR by the amount speciﬁed by value I. 
		LSL16,		///< Logically left shifts accumulator $acR by 16
		LSR,		///< Logically right shifts accumulator $acR by the amount calculated by negating sign-extended bits 0–6
		LSR16,		///< Logically right shifts accumulator $acR by 16

		MADD,		///< Multiply-add
		MADDC,
		MADDX,

		MOV,		///< Moves accumulator $ax(1-D) to accumulator $axD
		MOVAX,		///< Moves secondary accumulator $axS to accumulator $axD
		MOVNP,		///< Moves negated multiply product from the $prod register to the accumulator register $acD
		MOVP,		///< Moves multiply product from the $prod register to the accumulator register $acD
		MOVPZ,		///< Moves multiply product from the $prod register to the accumulator $acD and sets $acD.l to 0. 
		MOVR,		///< Moves register $(0x18+S) (sign-extended) to middle accumulator $acD.hm. Sets $acD.l to 0. 
		MRR,		///< Move value from register $S to register $D

		MSUB,		///< Multiply-sub
		MSUBC,
		MSUBX,

		MUL,		///< Multiply low part $axS.l of secondary accumulator $axS by high part $axS.h of secondary accumulator $axS (treat them both as signed). 
		MULAC,
		MULC,

		MULCAC,		///< 3 operands
		MULCMV,		///< 3 operands
		MULCMVZ,	///< 3 operands
		MULMV,		///< 3 operands
		MULMVZ,		///< 3 operands

		MULX,		///< Multiply one part $ax0 by one part $ax1 (treat them both as signed). 

		MULXAC,		///< 3 operands
		MULXMV,		///< 3 operands
		MULXMVZ,	///< 3 operands

		NEG,		///< Negates accumulator $acD.

		NOP,
		NX,			///< No operation, but can be extended with extended opcode

		ORC,		///< Logic OR middle part of accumulator $acD.m with middle part of accumulator $ax(1-D).m. 
		ORI,		///< Logical OR of accumulator mid part $acD.m with immediate value I. 
		ORR,		///< Logical OR middle part of accumulator $acD.m with high part of secondary accumulator $axS.h

		RET,		///< Return from subroutine
		RETcc,		///< Return from subroutine if condition cc has been met
		RTI,		///< Return from exception

		SBSET,		///< Set bit of status register $sr. 
		SBCLR,		///< Clear bit of status register $sr

		SI,			///< Store 16-bit immediate value I to a memory location pointed by address M 
		SR,			///< Store value from register $S to a memory pointed by address M. 
		SRR,		///< Store value from source register $S to a memory location pointed by addressing register $D
		SRRD,		///< Store value from source register $S to a memory location pointed by addressing register $D. Decrement register $D. 
		SRRI,		///< Store value from source register $S to a memory location pointed by addressing register $D. Increment register $D
		SRRN,		///< Store value from source register $S to a memory location pointed by addressing register $D. Add indexing register $(0x4+D) to register $D
		SRS,		///< Store value from register $(0x18+S) to a memory pointed by address M (8-bit sign-extended). 

		SUB,		///< Subtracts accumulator $ac(1-D) from accumulator register $acD
		SUBAX,		///< Subtracts secondary accumulator $axS from accumulator register $acD
		SUBP,		///< Subtracts product register from accumulator register
		SUBR,		///< Subtracts register $(0x18+S) from accumulator $acD register

		TST,		///< Test accumulator $acR
		TSTAXH,		///< Test hight part of secondary accumulator $axR.h. 

		XORI,		///< Logical XOR (exclusive OR) of accumulator mid part $acD.m with immediate value I. 
		XORR,		///< LogicalXOR(exclusiveOR)middlepartofaccumulator$acD.mwithhighpartofsecondaryaccumulator $axS.h. 

		// TODO: DIV (?)

		Max,
	};

	///< Extended opcodes

	/// Extended opcodes do not exist on their own. These opcodes can only be attached to opcodes that allow 
	/// extending (8 lower bits of opcode not used by opcode). Extended opcodes do not modify the program counter ($pc register.

	enum class DspInstructionEx
	{
		Unknown = -1,

		_DR,		///< Decrement addressing register $arR
		_IR,		///< Increment addressing register $arR. 
		_L,			///< Load register $(0x18+D) with value from memory pointed by register $S. Post increment register $S
		_LN,		///< Load register $(0x18+D) with value from memory pointed by register $S. Add indexing register register $(0x04+S) to register $S. 
		_LS,		///< Load register $(0x18+D) with value from memory pointed by register $ar0. Store value from register $acS.m to memory location pointed by register $ar3. Increment both $ar0 and $ar3
		_LSM,		///< Too long
		_LSNM,		///< Too long
		_LSN,		///< Too long
		_MV,		///< Move value of register $(0x1c+S) to the register $(0x18+D). 
		_NR,		///< Add corresponding indexing register $ixR to addressing register $arR. 
		_S,			///< Store value of register $(0x1c+S) in the memory pointed by register $D. Post increment register $D. 
		_SL,		///< Store value from register $acS.m to memory location pointed by register $ar0. Load register $(0x18+D) with value from memory pointed by register $ar3. Increment both $ar0 and $ar3. 
		_SLM,		///< Too long
		_SLMN,		///< Too long
		_SLN,		///< Too long
		_SN,		///< Store value of register $(0x1c+S) in the memory pointed by register $D. Add indexing register register $(0x04+D) to register $D. 

		// TODO: What about mentioned LD(?), LD2(?)
	};

	enum class DspHardwareRegs
	{
		CMBH = 0xFFFE,		///< CPU Mailbox H 
		CMBL = 0xFFFF,		///< CPU Mailbox L 
		DMBH = 0xFFFC,		///< DSP Mailbox H 
		DMBL = 0xFFFD,		///< DSP Mailbox L 

		DSMAH = 0xFFCE,		///< Memory address H 
		DSMAL = 0xFFCF,		///< Memory address L 
		DSPA = 0xFFCD,		///< DSP memory address 
		DSCR = 0xFFC9,		///< DMA control 
		DSBL = 0xFFCB,		///< Block size 

		ACSAH = 0xFFD4,		///< Accelerator start address H 
		ACSAL = 0xFFD5,		///< Accelerator start address L 
		ACEAH = 0xFFD6,		///< Accelerator end address H 
		ACEAL = 0xFFD7,		///< Accelerator end address L 
		ACCAH = 0xFFD8,		///< Accelerator current address H 
		ACCAL = 0xFFD9,		///< Accelerator current address L 
		ACDAT = 0xFFDD,		///< Accelerator data

		DIRQ = 0xFFFB,		///< IRQ request

		// TODO: What about sample-rate/ADPCM converter mentioned in patents/sdk?
	};

	enum class DspParameter
	{
		Unknown = -1,

		// Registers

		ar0 = 0,		///< Addressing register 0 
		ar1,			///< Addressing register 1 
		ar2,			///< Addressing register 2 
		ar3,			///< Addressing register 3 
		ix0,			///< Indexing register 0 
		ix1,			///< Indexing register 1
		ix2,			///< Indexing register 2
		ix3,			///< Indexing register 3
		r08,
		r09,
		r0a,
		r0b,
		st0,			///< Call stack register 
		st1,			///< Data stack register 
		st2,			///< Loop address stack register 
		st3,			///< Loop counter register 
		ac0h,			///< 40-bit Accumulator 0 (high) 
		ac1h,			///< 40-bit Accumulator 1 (high) 
		config,			///< Conﬁg register 
		sr,				///< Status register 
		prodl,			///< Product register (low) 
		prodm1,			///< Product register (mid 1) 
		prodh,			///< Product register (high) 
		prodm2,			///< Product register (mid 2) 
		ax0l,			///< 32-bit Accumulator 0 (low) 
		ax0h,			///< 32-bit Accumulator 0 (high) 
		ax1l,			///< 32-bit Accumulator 1 (low) 
		ax1h,			///< 32-bit Accumulator 1 (high
		ac0l,			///< 40-bit Accumulator 0 (low) 
		ac1l,			///< 40-bit Accumulator 1 (low) 
		ac0m,			///< 40-bit Accumulator 0 (mid)
		ac1m,			///< 40-bit Accumulator 1 (mid)

		// Immediates

		Max,
	};

	enum class ConditionCode
	{
		GE = 0b0000,		///< Greater than or equal 
		L = 0b0001,			///< Less than 
		G = 0b0010,			///< Greater than 
		LE = 0b0011,		///< Less than or equal 
		NE = 0b0100,		///< Not equal 
		EQ = 0b0101,		///< Equal 
		NC = 0b0110,		///< Not carry 
		C = 0b0111,			///< Carry 
		BelowS32 = 0b1000,	///< Below s32 
		AboveS32 = 0b1001,	///< Above s32 
		UnknownA = 0b1010,	///< TODO (?)
		UnknownB = 0b1011,	///< TODO (?)
		NZ = 0b1100,		///< Not zero 
		ZR = 0b1101,		///< Zero
		O = 0b1110,			///< Overﬂow
		Always = 0b1111,	///< Always
	};

	typedef struct _AnalyzeInfo
	{
		DspInstruction instr;		///< Processed instruction (ready to output)
		uint16_t instrBits;			///< Raw unprocessed opcode bits

		DspInstructionEx instrEx;	///< Processed extended opcode (ready to output)
		uint16_t instrExBits;		///< Raw unprocessed extended opcode bits
		bool extendedOpcodePresent;		///< Extended opcode present

		uint8_t bytes[0x10];	///< Saved instruction bytes, including immediate operands
		size_t sizeInBytes;		///< Total size of instruction, including immediate/address operands (in bytes)

		size_t numParameters;		///< Number of instruction parameters (0-3)
		size_t numParametersEx;		///< Number of extended instruction parameters (1-2)

		DspParameter params[3];		///< Processed parameters (ready to output)
		uint16_t paramBits[3];		///< Raw unprocessed parameter bits (order MSB->LSB)

		DspParameter paramsEx[2];		///< Processed extended opcode parameters (ready to output)
		uint16_t paramExBits[3];		///< Raw unprocessed extended opcode parameter bits (order MSB->LSB)

		bool flowControl;		///< Branch, jump or another flow control instruction
		bool logic;				///< Or, And and similar simple logic operation (non-arithmetic)
		bool madd;				///< Heavy MADD/MSUB operation

		///< Immediate/address operand, followed by instruction Word (or contained inside instruction)

		union ImmOperand
		{
			uint8_t		Byte;
			int8_t		SignedByte;
			uint16_t	UnsignedShort;
			int16_t		SignedShort;
			uint16_t	Address;		///< For bloop, call etc.
		};

		ConditionCode cc;		///< Some instructions has condition code

	} AnalyzeInfo;

	bool Analyze(uint8_t* instrPtr, size_t instrMaxSize, AnalyzeInfo& info);
}
