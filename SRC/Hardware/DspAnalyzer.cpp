// DSP analyzer

#include "pch.h"

namespace DSP
{
	void Analyzer::ResetInfo(AnalyzeInfo& info)
	{
		info.sizeInBytes = 0;

		info.instr = DspInstruction::Unknown;

		info.extendedOpcodePresent = false;
		info.numParameters = 0;
		info.numParametersEx = 0;

		info.flowControl = false;
		info.logic = false;
		info.madd = false;
	}

	bool Analyzer::Group0_Logic(uint8_t* instrPtr, size_t instrMaxSize, AnalyzeInfo& info, DspInstruction instr, bool logic)
	{
		if ((info.instrBits & 0xf) == 0)
		{
			if (instrMaxSize < sizeof(uint16_t))
				return false;

			int dd = ((info.instrBits & 0b100000000) != 0) ? 1 : 0;

			info.instr = instr;
			info.logic = logic;

			if (!AddParam(info, (DspParameter)((int)DspParameter::ac0m + dd), dd))
				return false;

			uint16_t imm = MEMSwapHalf(*(uint16_t*)instrPtr);
			if (!AddBytes(instrPtr, sizeof(uint16_t), info))
				return false;
			if (!AddImmOperand(info, DspParameter::UnsignedShort, imm))
				return false;
		}

		return true;
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

		else if ((info.instrBits & 0b0000111000000000) == 0b0000001000000000)
		{
			//IF cc * 	0000 00[1]0 [0111] cccc 
			//JMP cc * 	0000 00[1]0 [1001] cccc aaaa aaaa aaaa aaaa
			//CALL cc * 0000 00[1]0 [1011] cccc aaaa aaaa aaaa aaaa
			//RET cc * 	0000 00[1]0 [1101] cccc 
			//ADDI * 	0000 00[1]d [0000] 0000 iiii iiii iiii iiii 
			//XORI * 	0000 00[1]d [0010] 0000 iiii iiii iiii iiii 
			//ANDI * 	0000 00[1]d [0100] 0000 iiii iiii iiii iiii 
			//ORI * 	0000 00[1]d [0110] 0000 iiii iiii iiii iiii 
			//CMPI * 	0000 00[1]d [1000] 0000 iiii iiii iiii iiii 
			//ANDCF * 	0000 00[1]d [1010] 0000 iiii iiii iiii iiii 
			//ANDF * 	0000 00[1]d [1100] 0000 iiii iiii iiii iiii 
			//ILRR * 	0000 00[1]d [0001] 00ss
			//ILRRD * 	0000 00[1]d [0001] 01ss
			//ILRRI * 	0000 00[1]d [0001] 10ss
			//ILRRN * 	0000 00[1]d [0001] 11ss

			switch ((info.instrBits >> 4) & 0xf)
			{
				case 0b0111:		// IF cc
				{
					info.instr = DspInstruction::IFcc;
					info.cc = (ConditionCode)(info.instrBits & 0xf);
					info.flowControl = true;
					break;
				}
				case 0b1001:		// JMP cc
				{
					if (instrMaxSize < sizeof(uint16_t))
						return false;

					info.instr = DspInstruction::Jcc;
					info.cc = (ConditionCode)(info.instrBits & 0xf);
					info.flowControl = true;

					uint16_t addr = MEMSwapHalf(*(uint16_t*)instrPtr);
					if (!AddBytes(instrPtr, sizeof(uint16_t), info))
						return false;
					if (!AddImmOperand(info, DspParameter::Address, (DspAddress)addr))
						return false;

					break;
				}
				case 0b1011:		// CALL cc
				{
					if (instrMaxSize < sizeof(uint16_t))
						return false;

					info.instr = DspInstruction::CALLcc;
					info.cc = (ConditionCode)(info.instrBits & 0xf);
					info.flowControl = true;

					uint16_t addr = MEMSwapHalf(*(uint16_t*)instrPtr);
					if (!AddBytes(instrPtr, sizeof(uint16_t), info))
						return false;
					if (!AddImmOperand(info, DspParameter::Address, (DspAddress)addr))
						return false;

					break;
				}
				case 0b1101:		// RET cc
				{
					info.instr = DspInstruction::RETcc;
					info.cc = (ConditionCode)(info.instrBits & 0xf);
					info.flowControl = true;
					break;
				}
				case 0b0000:		// ADDI
					return Group0_Logic(instrPtr, instrMaxSize, info, DspInstruction::ADDI, true);
					break;
				case 0b0010:		// XORI
					return Group0_Logic(instrPtr, instrMaxSize, info, DspInstruction::XORI, true);
					break;
				case 0b0100:		// ANDI
					return Group0_Logic(instrPtr, instrMaxSize, info, DspInstruction::ANDI, true);
					break;
				case 0b0110:		// ORI
					return Group0_Logic(instrPtr, instrMaxSize, info, DspInstruction::ORI, true);
					break;
				case 0b1000:		// CMPI
					return Group0_Logic(instrPtr, instrMaxSize, info, DspInstruction::CMPI, false);
					break;
				case 0b1010:		// ANDCF
					return Group0_Logic(instrPtr, instrMaxSize, info, DspInstruction::ANDCF, true);
					break;
				case 0b1100:		// ANDF
					return Group0_Logic(instrPtr, instrMaxSize, info, DspInstruction::ANDF, true);
					break;
				case 0b0001:		// ILRR's
				{
					switch ((info.instrBits >> 2) & 3)
					{
						case 0:
							info.instr = DspInstruction::ILRR;
							break;
						case 1:
							info.instr = DspInstruction::ILRRD;
							break;
						case 2:
							info.instr = DspInstruction::ILRRI;
							break;
						case 3:
							info.instr = DspInstruction::ILRRN;
							break;
					}

					int dd = ((info.instrBits & 0b100000000) != 0) ? 1 : 0;
					int ss = info.instrBits & 3;

					if (!AddParam(info, (DspParameter)((int)DspParameter::ac0m + dd), dd))
						return false;
					if (!AddParam(info, (DspParameter)((int)DspParameter::ar0 + ss), ss))
						return false;

					break;
				}
			}

			return true;
		}

		else if (info.instrBits & 0b0000100000000000)
		{
			//LRIS * 	0000 1ddd iiii iiii 

			info.instr = DspInstruction::LRIS;

			int dd = (info.instrBits >> 8) & 7;
			int8_t ii = info.instrBits & 0xff;
			if (!AddParam(info, (DspParameter)(0x18 + dd), dd))
				return false;
			if (!AddImmOperand(info, DspParameter::SignedByte, ii))
				return false;

			return true;
		}

		else if (info.instrBits & 0b0000010000000000)
		{
			//ADDIS *	0000 01[0]d iiii iiii
			//CMPIS *	0000 01[1]d iiii iiii

			if (info.instrBits & 0b1000000000)
			{
				info.instr = DspInstruction::CMPIS;
			}
			else
			{
				info.instr = DspInstruction::ADDIS;
			}

			int dd = (info.instrBits >> 8) & 1;
			int8_t ii = info.instrBits & 0xff;
			if (!AddParam(info, (DspParameter)((int)DspParameter::ac0m + dd), dd))
				return false;
			if (!AddImmOperand(info, DspParameter::SignedByte, ii))
				return false;

			return true;
		}

		return true;
	}

	bool Analyzer::Group1(uint8_t* instrPtr, size_t instrMaxSize, AnalyzeInfo& info)
	{
		return true;
	}

	bool Analyzer::Group2(AnalyzeInfo& info)
	{
		return true;
	}

	bool Analyzer::Group3(AnalyzeInfo& info)
	{
		return true;
	}

	bool Analyzer::Group4(AnalyzeInfo& info)
	{
		return true;
	}

	bool Analyzer::Group5(AnalyzeInfo& info)
	{
		return true;
	}

	bool Analyzer::Group6(AnalyzeInfo& info)
	{
		return true;
	}

	bool Analyzer::Group7(AnalyzeInfo& info)
	{
		return true;
	}

	bool Analyzer::Group8(AnalyzeInfo& info)
	{
		return true;
	}

	bool Analyzer::Group9(AnalyzeInfo& info)
	{
		return true;
	}

	bool Analyzer::GroupAB(AnalyzeInfo& info)
	{
		return true;
	}

	bool Analyzer::GroupCD(AnalyzeInfo& info)
	{
		return true;
	}

	bool Analyzer::GroupE(AnalyzeInfo& info)
	{
		return true;
	}

	bool Analyzer::GroupF(AnalyzeInfo& info)
	{
		return true;
	}

	bool Analyzer::GroupPacked(AnalyzeInfo& info)
	{
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

		// Select by group. Groups 3-F in packed format.

		switch (group)
		{
			case 0:
				return Group0(instrPtr, instrMaxSize, info);
			case 1:
				return Group1(instrPtr, instrMaxSize, info);
			case 2:
				return Group2(info);
			case 3:
				if (!Group3(info))
					return false;
				return GroupPacked(info);
			case 4:
				if (!Group4(info))
					return false;
				return GroupPacked(info);
			case 5:
				if (!Group5(info))
					return false;
				return GroupPacked(info);
			case 6:
				if (!Group6(info))
					return false;
				return GroupPacked(info);
			case 7:
				if (!Group7(info))
					return false;
				return GroupPacked(info);
			case 8:
				if (!Group8(info))
					return false;
				return GroupPacked(info);
			case 9:
				if (!Group9(info))
					return false;
				return GroupPacked(info);
			case 0xa:
			case 0xb:
				if (!GroupAB(info))
					return false;
				return GroupPacked(info);
			case 0xc:
			case 0xd:
				if (!GroupCD(info))
					return false;
				return GroupPacked(info);
			case 0xe:
				if (!GroupE(info))
					return false;
				return GroupPacked(info);
			case 0xf:
				if (!GroupF(info))
					return false;
				return GroupPacked(info);
		}

		return true;
	}

}
