// DSP analyzer

#include "pch.h"

namespace DSP
{
	void Analyzer::ResetInfo(AnalyzeInfo& info)
	{
		info.sizeInBytes = 0;

		info.extendedOpcodePresent = false;
		info.numParameters = 0;
		info.numParametersEx = 0;

		info.flowControl = false;
		info.logic = false;
		info.madd = false;
	}

	bool Analyzer::Group0(uint8_t* instrPtr, size_t instrMaxSize, AnalyzeInfo& info)
	{
		info.instr = DspInstruction::Unknown;

		if ((info.instrBits & 0b0000111100000000) == 0)
		{
			//NOP * 	0000 0000 [000]0 0000 
			//DAR * 	0000 0000 [000]0 01dd 
			//IAR * 	0000 0000 [000]0 10dd 
			//ADDARN * 	0000 0000 [000]1 ssdd

			//HALT * 	0000 0000 [001]0 0001 
			//LOOP * 	0000 0000 [010]r rrrr 
			//BLOOP * 	0000 0000 [011]r rrrr aaaa aaaa aaaa aaaa
			//LRI * 	0000 0000 [100]r rrrr iiii iiii iiii iiii 
			//LR * 		0000 0000 [110]r rrrr mmmm mmmm mmmm mmmm 
			//SR * 		0000 0000 [111]r rrrr mmmm mmmm mmmm mmmm 

			switch ((info.instrBits >> 5) & 7)
			{
				case 0b000:
					if (info.instrBits & 0b10000)
					{
						// ADDARN
						int dd = info.instrBits & 3;
						int ss = (info.instrBits >> 2) & 3;

						info.instr = DspInstruction::ADDARN;
						if (!AddParam(info, (DspParameter)((int)DspParameter::ar0 + dd), dd))
							return false;
						if (!AddParam(info, (DspParameter)((int)DspParameter::ix0 + ss), ss))
							return false;
					}
					else
					{
						switch ((info.instrBits >> 2) & 3)
						{
							case 0b00:		// NOP
								if ((info.instrBits & 3) == 0)
								{
									info.instr = DspInstruction::NOP;
								}
								break;
							case 0b01:		// DAR
							{
								int dd = info.instrBits & 3;

								info.instr = DspInstruction::DAR;
								if (!AddParam(info, (DspParameter)((int)DspParameter::ar0 + dd), dd))
									return false;
								break;
							}
							case 0b10:		// IAR
							{
								int dd = info.instrBits & 3;

								info.instr = DspInstruction::IAR;
								if (!AddParam(info, (DspParameter)((int)DspParameter::ar0 + dd), dd))
									return false;
								break;
							}

							default:
								break;
						}
					}
					break;
				case 0b001:		// HALT
					if ((info.instrBits & 0b11111) == 0b00001)
					{
						info.instr = DspInstruction::HALT;
						info.flowControl = true;
					}
					break;
				case 0b010:		// LOOP
				{
					uint16_t r = info.instrBits & 0b11111;
					info.instr = DspInstruction::LOOP;
					info.flowControl = true;
					if (!AddParam(info, (DspParameter)r, r))
						return false;
					break;
				}
				case 0b011:		// BLOOP
				{
					if (instrMaxSize < sizeof(uint16_t))
						return false;

					uint16_t r = info.instrBits & 0b11111;
					info.instr = DspInstruction::BLOOP;
					info.flowControl = true;
					if (!AddParam(info, (DspParameter)r, r))
						return false;

					uint16_t addr = MEMSwapHalf (*(uint16_t*)instrPtr);
					if (!AddBytes(instrPtr, sizeof(uint16_t), info))
						return false;
					if (!AddImmOperand(info, DspParameter::Address, (DspAddress)addr))
						return false;

					break;
				}
				case 0b100:		// LRI
				{
					if (instrMaxSize < sizeof(uint16_t))
						return false;

					uint16_t r = info.instrBits & 0b11111;
					info.instr = DspInstruction::LRI;
					if (!AddParam(info, (DspParameter)r, r))
						return false;

					uint16_t imm = MEMSwapHalf(*(uint16_t*)instrPtr);
					if (!AddBytes(instrPtr, sizeof(uint16_t), info))
						return false;
					if (!AddImmOperand(info, DspParameter::UnsignedShort, imm))
						return false;

					break;
				}
				case 0b110:		// LR
				{
					if (instrMaxSize < sizeof(uint16_t))
						return false;

					uint16_t r = info.instrBits & 0b11111;
					info.instr = DspInstruction::LR;
					if (!AddParam(info, (DspParameter)r, r))
						return false;

					uint16_t addr = MEMSwapHalf(*(uint16_t*)instrPtr);
					if (!AddBytes(instrPtr, sizeof(uint16_t), info))
						return false;
					if (!AddImmOperand(info, DspParameter::Address, (DspAddress)addr))
						return false;

					break;
				}
				case 0b111:		// SR
				{
					if (instrMaxSize < sizeof(uint16_t))
						return false;

					uint16_t addr = MEMSwapHalf(*(uint16_t*)instrPtr);
					if (!AddBytes(instrPtr, sizeof(uint16_t), info))
						return false;
					if (!AddImmOperand(info, DspParameter::Address, (DspAddress)addr))
						return false;

					uint16_t r = info.instrBits & 0b11111;
					info.instr = DspInstruction::SR;
					if (!AddParam(info, (DspParameter)r, r))
						return false;

					break;
				}

				default:
					break;
			}

			return true;
		}

		if ((info.instrBits & 0b0000111000000000) == 0b0000001000000000)
		{
			//IF cc * 	0000 00[1]0 0111 cccc 
			//JMP cc * 	0000 00[1]0 1001 cccc 
			//CALL cc * 0000 00[1]0 1011 cccc 
			//RET cc * 	0000 00[1]0 1101 cccc 
			//ADDI * 	0000 00[1]r 0000 0000 iiii iiii iiii iiii 
			//XORI * 	0000 00[1]r 0010 0000 iiii iiii iiii iiii 
			//ANDI * 	0000 00[1]r 0100 0000 iiii iiii iiii iiii 
			//ORI * 	0000 00[1]r 0110 0000 iiii iiii iiii iiii 
			//CMPI * 	0000 00[1]r 1000 0000 iiii iiii iiii iiii 
			//ANDCF * 	0000 00[1]r 1010 0000 iiii iiii iiii iiii 
			//ANDF * 	0000 00[1]r 1100 0000 iiii iiii iiii iiii 
			//ILRR * 	0000 00[1]r 0001 mmaa 

			return true;
		}

		if (info.instrBits & 0b0000100000000000)
		{
			//LRIS * 	0000 1rrr iiii iiii 

			return true;
		}

		if (info.instrBits & 0b0000010000000000)
		{
			//ADDIS *	0000 010d iiii iiii
			//CMPIS *	0000 011d iiii iiii

			return true;
		}

		return true;
	}

	bool Analyzer::AddParam(AnalyzeInfo& info, DspParameter param, uint16_t paramBits)
	{
		if (info.numParameters >= _countof(info.params))
			return false;

		info.params[info.numParameters] = param;
		info.paramBits[info.numParameters] = paramBits;

		info.numParameters++;

		return true;
	}

	bool Analyzer::AddBytes(uint8_t* instrPtr, size_t bytes, AnalyzeInfo& info)
	{
		if (info.sizeInBytes >= sizeof(info.bytes))
			return false;

		memcpy(&info.bytes[info.sizeInBytes], instrPtr, bytes);
		info.sizeInBytes += bytes;

		return true;
	}

	bool Analyzer::Analyze(uint8_t* instrPtr, size_t instrMaxSize, AnalyzeInfo& info)
	{
		// Reset info

		ResetInfo(info);

		// Get opcode and determine its group

		if (instrMaxSize < sizeof(uint16_t))
			return false;

		info.instrBits = MEMSwapHalf (*(uint16_t*)instrPtr);

		if (!AddBytes(instrPtr, sizeof(uint16_t), info))
			return false;

		instrPtr += sizeof(info.instrBits);
		instrMaxSize -= sizeof(info.instrBits);

		int group = (info.instrBits >> 12);

		// Select by group

		switch (group)
		{
			case 0:
				return Group0(instrPtr, instrMaxSize, info);
				break;

			default:
				info.instr = DspInstruction::Unknown;
				break;
		}

		return true;
	}

}
