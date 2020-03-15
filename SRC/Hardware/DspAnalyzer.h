/// DSP analyzer.
/// Can be used by disassembler as well as by interpreter/jitc.

#pragma once

namespace DSP
{

	enum class DspInstruction
	{
		Unknown = -1,

		ABS,		///< 0xA100

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
		TCLR,		///< Test bit clear
		TSET,		///< Test bit set
		ANDI,		///< Logical AND with the mid part of accumulator $acD.m and the immediate value I
		ANDR,		///< Logical AND with the middle part of accumulator $acD.m and the high part of secondary accumulator, $axS.h

		ASL,		///< Arithmetically left shifts the accumulator $acR by the amount specified by immediate I
		ASR,		///< Arithmetically right shifts accumulator $acR specified by the value calculated by negating sign-extended bits 0-6
		ASR16,		///< Arithmetically right shifts accumulator $acR by 16

		BLOOP,		///< Block loop (Counter in register)
		BLOOPI,		///< Block loop (Counter in immediate operand)
		CALLcc,		///< Call function if condition cc has been met
		CALLR,		///< Call function (register)

		CLR,		///< Clears accumulator $acR
		CLRL,		///< Clears $acR.l - low 16 bits of accumulator $acR
		CLRP,		///< Clears product register $prod

		CMP,		///< Compares accumulator $ac0 with accumulator $ac1
		CMPI,		///< Compares mid accumulator $acD.hm ($amD) with sign-extended immediate value I
		CMPIS,		///< Compares accumulator with short immediate
		CMPAR,		///< Compares accumulator $acS with accumulator axR.h.

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
		LRS,		///< Move value from data memory pointed by address M (8-bit sign-extended) to register $(0x18+D)

		LSL,		///< Logically left shifts accumulator $acR by the amount specified by value I. 
		LSL16,		///< Logically left shifts accumulator $acR by 16
		LSR,		///< Logically right shifts accumulator $acR by the amount calculated by negating sign-extended bits 0–6
		LSR16,		///< Logically right shifts accumulator $acR by 16

		M2,			///< Clear SR_MUL_MODIFY
		M0,			///< Set SR_MUL_MODIFY
		CLR15,		///< Clear SR_MUL_UNSIGNED
		SET15,		///< Set SR_MUL_UNSIGNED
		CLR40,		///< Clear SR_40_MODE_BIT
		SET40,		///< Set SR_40_MODE_BIT

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

		// Weird

		LSN,		///< Logically shifts right accumulator $ACC0 by lower 7-bit (signed) value in $AC1.M (if value negative, becomes left shift)
		ASN,		///< Arithmetically shifts right accumulator $ACC0 by lower 7-bit (signed) value in $AC1.M (if value negative, becomes left shift)

		// TODO: DIV (?)

		Max,
	};

	///< Extended opcodes

	// DSP instructions are in a hybrid format: some instructions occupy a full 16-bit word, and some can be packed as two 8-bit instructions per word.
	// Extended opcodes represents lower-part of instruction pair.

	enum class DspInstructionEx
	{
		Unknown = -1,

		NOP2,	// 0x00
		DR,		// DR $arR 
		IR,		// IR $arR 
		NR,		// NR $arR, ixR
		MV,		// MV $(0x18+D), $(0x1c+S) 
		S,		// S @$D, $(0x1c+D)  
		SN,		// SN @$D, $(0x1c+D)  
		L,		// L $(0x18+D), @$S 
		LN,		// LN $(0x18+D), @$S 

		LS,		// LS $(0x18+D), $acS.m 
		SL,		// SL $acS.m, $(0x18+D)  
		LSN,	// LSN $(0x18+D), $acS.m 
		SLN,	// SLN $acS.m, $(0x18+D)
		LSM,	// LSM $(0x18+D), $acS.m 
		SLM,	// SLM $acS.m, $(0x18+D)
		LSNM,	// LSNM $(0x18+D), $acS.m 
		SLNM,	// SLNM $acS.m, $(0x18+D)

		LD,		// LD $ax0.d, $ax1.r, @$arS 
		LDN,	// LDN $ax0.d, $ax1.r, @$arS
		LDM,	// LDM $ax0.d, $ax1.r, @$arS
		LDNM,	// LDNM $ax0.d, $ax1.r, @$arS

		LDAX,	// LDAX $axR, @$arS
		LDAXN,	// LDAXN $axR, @$arS
		LDAXM,	// LDAXM $axR, @$arS
		LDAXNM,	// LDAXNM $axR, @$arS
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
		config,			///< Config register 
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

		// Accumulator (40-bit)

		ac0,
		ac1,

		// Small accumulator (32-bit)

		ax0,
		ax1,

		// Indexed by register

		Indexed_regs,
		Indexed_ar0 = Indexed_regs,	///< @ Addressing register 0 
		Indexed_ar1,			///< @ Addressing register 1 
		Indexed_ar2,			///< @ Addressing register 2 
		Indexed_ar3,			///< @ Addressing register 3 
		Indexed_ix0,			///< @ Indexing register 0 
		Indexed_ix1,			///< @ Indexing register 1
		Indexed_ix2,			///< @ Indexing register 2
		Indexed_ix3,			///< @ Indexing register 3

		// Immediates

		Byte,
		SignedByte,
		UnsignedShort,
		Address,

		Byte2,
		SignedByte2,
		UnsignedShort2,
		Address2,

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
		NOK = 0b1100,		///< Bit Test Not OK
		OK = 0b1101,		///< Bit Test OK
		O = 0b1110,			///< Overﬂow
		Always = 0b1111,	///< Always
	};

	typedef struct _AnalyzeInfo
	{
		uint32_t clearingPaddy;		///< To use {0} on structure

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

		DspParameter paramsEx[3];		///< Processed extended opcode parameters (ready to output)
		uint16_t paramExBits[3];		///< Raw unprocessed extended opcode parameter bits (order MSB->LSB)

		bool flowControl;		///< Branch, jump or another flow control instruction
		bool logic;				///< Or, And and similar simple logic operation (non-arithmetic)
		bool madd;				///< Heavy MADD/MSUB operation

		///< Immediate/address operand, followed by instruction Word (or contained inside instruction)

		union
		{
			uint8_t		Byte;
			int8_t		SignedByte;
			uint16_t	UnsignedShort;
			DspAddress	Address;		///< For bloop, call etc.
		} ImmOperand;

		///< Second immediate/address operand (required by small amount of instructions)

		union
		{
			uint8_t		Byte;
			int8_t		SignedByte;		///< For SI
			uint16_t	UnsignedShort;
			DspAddress	Address;		///< For BLOOPI
		} ImmOperand2;

		ConditionCode cc;		///< Some instructions has condition code

	} AnalyzeInfo;

	class Analyzer
	{
		// Internal helpers

		static void ResetInfo(AnalyzeInfo& info);

		static bool Group0_Logic(uint8_t* instrPtr, size_t instrMaxSize, AnalyzeInfo& info, DspInstruction instr, bool logic);

		static bool Group0(uint8_t* instrPtr, size_t instrMaxSize, AnalyzeInfo& info);
		static bool Group1(uint8_t* instrPtr, size_t instrMaxSize, AnalyzeInfo& info);
		static bool Group2(AnalyzeInfo& info);
		static bool Group3(AnalyzeInfo& info);
		static bool Group4(AnalyzeInfo& info);
		static bool Group5(AnalyzeInfo& info);
		static bool Group6(AnalyzeInfo& info);
		static bool Group7(AnalyzeInfo& info);
		static bool Group8(AnalyzeInfo& info);
		static bool Group9(AnalyzeInfo& info);
		static bool GroupAB(AnalyzeInfo& info);
		static bool GroupCD(AnalyzeInfo& info);
		static bool GroupE(AnalyzeInfo& info);
		static bool GroupF(AnalyzeInfo& info);

		static bool GroupPacked(AnalyzeInfo& info);

		template<typename T>
		static bool AddImmOperand(AnalyzeInfo& info, DspParameter param, T imm);
		static bool AddParam(AnalyzeInfo& info, DspParameter param, uint16_t paramBits);
		static bool AddParamEx(AnalyzeInfo& info, DspParameter param, uint16_t paramBits);

		// c++ commitete should try harder. Allowed only in headers..

		static bool inline AddImmOperand(AnalyzeInfo& info, DspParameter param, uint8_t imm)
		{
			if (!AddParam(info, param, imm))
				return false;
			if (param == DspParameter::Byte)
				info.ImmOperand.Byte = imm;
			else if (param == DspParameter::Byte2)
				info.ImmOperand2.Byte = imm;
			return true;
		}

		static bool inline AddImmOperand(AnalyzeInfo& info, DspParameter param, int8_t imm)
		{
			if (!AddParam(info, param, imm))
				return false;
			if (param == DspParameter::SignedByte)
				info.ImmOperand.SignedByte = imm;
			else if (param == DspParameter::SignedByte2)
				info.ImmOperand2.SignedByte = imm;
			return true;
		}

		static bool inline AddImmOperand(AnalyzeInfo& info, DspParameter param, uint16_t imm)
		{
			if (!AddParam(info, param, imm))
				return false;
			if (param == DspParameter::UnsignedShort)
				info.ImmOperand.UnsignedShort = imm;
			else if (param == DspParameter::UnsignedShort2)
				info.ImmOperand2.UnsignedShort = imm;
			return true;
		}

		static bool inline AddImmOperand(AnalyzeInfo& info, DspParameter param, DspAddress imm)
		{
			if (!AddParam(info, param, (uint16_t)imm))
				return false;
			if (param == DspParameter::Address)
				info.ImmOperand.Address = imm;
			else if (param == DspParameter::Address2)
				info.ImmOperand2.Address = imm;
			return true;
		}

		static bool AddBytes(uint8_t* instrPtr, size_t bytes, AnalyzeInfo& info);

	public:

		static bool Analyze(uint8_t* instrPtr, size_t instrMaxSize, AnalyzeInfo& info);
	};
}
