// Auxiliary definitions for x87 instruction assembler.

#pragma once

namespace IntelCore
{

	/// <summary>
	/// Bitmask that determines which forms of encoding are supported by the specified FPU instruction.
	/// </summary>
	enum FpuInstrForm : uint32_t
	{
		None = 0,

		FpuForm_M32FP = 0x1,
		FpuForm_M64FP = 0x2,
		FpuForm_M80FP = 0x4,
		FpuForm_M80BCD = 0x8,
		FpuForm_M16INT = 0x10,
		FpuForm_M32INT = 0x20,
		FpuForm_M64INT = 0x40,
		FpuForm_ToST0 = 0x80,
		FpuForm_FromST0 = 0x100,
		FpuForm_STn = 0x200,
		FpuForm_M2Byte = 0x400,
		FpuForm_M14_28Byte = 0x800,
		FpuForm_M94_108Byte = 0x1000,
		FpuForm_M512Byte = 0x2000,
		FpuForm_AX = 0x4000,
	};

	/// <summary>
	/// Definitions of FPU instruction format and opcodes used.
	/// </summary>
	struct FpuInstrFeatures
	{
		uint32_t forms;
		uint8_t FpuForm_M32FP_Opcode;
		uint8_t FpuForm_M32FP_RegOpcode;
		uint8_t FpuForm_M64FP_Opcode;
		uint8_t FpuForm_M64FP_RegOpcode;
		uint8_t FpuForm_M80FP_Opcode;
		uint8_t FpuForm_M80FP_RegOpcode;
		uint8_t FpuForm_M80BCD_Opcode;
		uint8_t FpuForm_M80BCD_RegOpcode;
		uint8_t FpuForm_M16INT_Opcode;
		uint8_t FpuForm_M16INT_RegOpcode;
		uint8_t FpuForm_M32INT_Opcode;
		uint8_t FpuForm_M32INT_RegOpcode;
		uint8_t FpuForm_M64INT_Opcode;
		uint8_t FpuForm_M64INT_RegOpcode;
		uint8_t FpuForm_ToST0_Opcode1;
		uint8_t FpuForm_ToST0_Opcode2;
		uint8_t FpuForm_FromST0_Opcode1;
		uint8_t FpuForm_FromST0_Opcode2;
		uint8_t FpuForm_STn_Opcode1;
		uint8_t FpuForm_STn_Opcode2;
	};

}
