// DSP analyzer
#include "pch.h"

namespace DSP
{
	void Analyzer::ResetInfo(AnalyzeInfo& info)
	{
		info.sizeInBytes = 0;

		info.instr = DspInstruction::Unknown;
		info.instrEx = DspInstructionEx::Unknown;

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

			uint16_t imm = _byteswap_ushort(*(uint16_t*)instrPtr);
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

					uint16_t addr = _byteswap_ushort(*(uint16_t*)instrPtr);
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

					uint16_t imm = _byteswap_ushort(*(uint16_t*)instrPtr);
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

					uint16_t addr = _byteswap_ushort(*(uint16_t*)instrPtr);
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

					uint16_t addr = _byteswap_ushort(*(uint16_t*)instrPtr);
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
			//RTI		0000 00[1]0 [1111] 1111
			//ADDI * 	0000 00[1]d [0000] 0000 iiii iiii iiii iiii 
			//XORI * 	0000 00[1]d [0010] 0000 iiii iiii iiii iiii 
			//ANDI * 	0000 00[1]d [0100] 0000 iiii iiii iiii iiii 
			//ORI * 	0000 00[1]d [0110] 0000 iiii iiii iiii iiii 
			//CMPI * 	0000 00[1]d [1000] 0000 iiii iiii iiii iiii 
			//TCLR * 	0000 00[1]d [1010] 0000 iiii iiii iiii iiii 
			//TSET * 	0000 00[1]d [1100] 0000 iiii iiii iiii iiii 
			//LSN		0000 00[1]0 [1100] 1010
			//ASN		0000 00[1]0 [1100] 1011
			//ILRR * 	0000 00[1]d [0001] 00ss
			//ILRRD * 	0000 00[1]d [0001] 01ss
			//ILRRI * 	0000 00[1]d [0001] 10ss
			//ILRRN * 	0000 00[1]d [0001] 11ss

			if (info.instrBits == 0x02ca)
			{
				info.logic = true;
				info.instr = DspInstruction::LSN;
				return true;
			}
			else if (info.instrBits == 0x02cb)
			{
				info.logic = true;
				info.instr = DspInstruction::ASN;
				return true;
			}

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

					uint16_t addr = _byteswap_ushort(*(uint16_t*)instrPtr);
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

					uint16_t addr = _byteswap_ushort(*(uint16_t*)instrPtr);
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
				case 0b1111:		// RTI
				{
					if ((info.instrBits & 0xf) == 0xf)
					{
						info.instr = DspInstruction::RTI;
						info.flowControl = true;
					}
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
				case 0b1010:		// TCLR
					return Group0_Logic(instrPtr, instrMaxSize, info, DspInstruction::TCLR, true);
					break;
				case 0b1100:		// TSET
					return Group0_Logic(instrPtr, instrMaxSize, info, DspInstruction::TSET, true);
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
					if (!AddParam(info, (DspParameter)((int)DspParameter::Indexed_ar0 + ss), ss))
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
			if (!AddParam(info, (DspParameter)(0x18 + dd), 0x18 + dd))
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
		//LOOPI *	0001 [00]00 iiii iiii
		//BLOOPI	0001 [00]01 iiii iiii aaaa aaaa aaaa aaaa 
		//SBSET *	0001 [00]10 0000 0iii 
		//SBCLR *	0001 [00]11 0000 0iii 

		//LSL 		0001 [01]0r 00ii iiii
		//LSR 		0001 [01]0r 01ii iiii
		//ASL 		0001 [01]0r 10ii iiii
		//ASR 		0001 [01]0r 11ii iiii
		//SI * 		0001 [01]10 mmmm mmmm iiii iiii iiii iiii
		//CALLR *	0001 [01]11 rrr1 1111 
		//JMPR * 	0001 [01]11 rrr0 1111 

		//LRR * 	0001 [10]00 0ssd dddd 
		//LRRD * 	0001 [10]00 1ssd dddd 
		//LRRI * 	0001 [10]01 0ssd dddd 
		//LRRN * 	0001 [10]01 1ssd dddd 
		//SRR * 	0001 [10]10 0ssd dddd 
		//SRRD * 	0001 [10]10 1ssd dddd 
		//SRRI * 	0001 [10]11 0ssd dddd 
		//SRRN * 	0001 [10]11 1ssd dddd 

		//MRR * 	0001 [11]dd ddds ssss 

		switch ((info.instrBits >> 10) & 3)
		{
			case 0:
				switch ((info.instrBits >> 8) & 3)
				{
					case 0:		// LOOPI
					{
						info.instr = DspInstruction::LOOPI;
						info.flowControl = true;

						if (!AddImmOperand(info, DspParameter::Byte, (uint8_t)(info.instrBits & 0xff)))
							return false;

						break;
					}
					case 1:		// BLOOPI
					{
						if (instrMaxSize < sizeof(uint16_t))
							return false;

						info.instr = DspInstruction::BLOOPI;
						info.flowControl = true;

						uint16_t addr = _byteswap_ushort(*(uint16_t*)instrPtr);
						if (!AddBytes(instrPtr, sizeof(uint16_t), info))
							return false;
						if (!AddImmOperand(info, DspParameter::Byte, (uint8_t)(info.instrBits & 0xff)))
							return false;
						if (!AddImmOperand(info, DspParameter::Address2, (DspAddress)addr))
							return false;
						break;
					}
					case 2:		// SBSET
						if ((info.instrBits & 0b11111000) == 0)
						{
							info.instr = DspInstruction::SBSET;

							int ii = 6 + (info.instrBits & 7);
							if (!AddImmOperand(info, DspParameter::SignedByte, (int8_t)ii))
								return false;
						}
						break;
					case 3:		// SBCLR
						if ((info.instrBits & 0b11111000) == 0)
						{
							info.instr = DspInstruction::SBCLR;

							int ii = 6 + (info.instrBits & 7);
							if (!AddImmOperand(info, DspParameter::SignedByte, (int8_t)ii))
								return false;
						}
						break;
				}
				break;
			case 1:
				if ((info.instrBits & 0b1000000000) != 0)
				{
					if ((info.instrBits & 0b100000000) != 0)
					{
						if ((info.instrBits & 0xf) == 0xf)
						{
							// CALLR, JMPR
							if ((info.instrBits & 0b10000) != 0)
							{
								info.instr = DspInstruction::CALLR;
							}
							else
							{
								info.instr = DspInstruction::JMPR;
							}
							info.flowControl = true;

							int rr = (info.instrBits >> 5) & 7;
							if (!AddParam(info, (DspParameter)rr, rr))
								return false;
						}
					}
					else
					{
						// SI
						if (instrMaxSize < sizeof(uint16_t))
							return false;

						info.instr = DspInstruction::SI;

						DspAddress mm = (DspAddress)(uint16_t)(int16_t)(int8_t)(uint8_t)(info.instrBits & 0xff);
						uint16_t imm = _byteswap_ushort(*(uint16_t*)instrPtr);
						if (!AddBytes(instrPtr, sizeof(uint16_t), info))
							return false;
						if (!AddImmOperand(info, DspParameter::Address, mm))
							return false;
						if (!AddImmOperand(info, DspParameter::UnsignedShort2, imm))
							return false;
					}
				}
				else
				{
					// LSL, LSR, ASL, ASR
					bool rightShift = false;

					switch ((info.instrBits >> 6) & 3)
					{
						case 0:
							info.instr = DspInstruction::LSL;
							break;
						case 1:
							info.instr = DspInstruction::LSR;
							rightShift = true;
							break;
						case 2:
							info.instr = DspInstruction::ASL;
							break;
						case 3:
							info.instr = DspInstruction::ASR;
							rightShift = true;
							break;
					}
					info.logic = true;

					uint16_t rr = ((info.instrBits & 0b100000000) != 0) ? 1 : 0;
					uint8_t ii = info.instrBits & 0x3f;

					// This strange shift behavior is suspiciously looks like bit rotation, but let it be as it is
					if (rightShift)
					{
						if (ii & 0b100000)
							ii |= 0b11000000;
					}

					if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + rr), rr))
						return false;
					if (rightShift)
					{
						if (!AddImmOperand(info, DspParameter::SignedByte, (int8_t)ii))
							return false;
					}
					else
					{
						if (!AddImmOperand(info, DspParameter::Byte, ii))
							return false;
					}
				}
				break;
			case 2:			// LRR / SRR
			{
				bool load = false;

				switch ((info.instrBits >> 7) & 7)
				{
					case 0b000:
						info.instr = DspInstruction::LRR;
						load = true;
						break;
					case 0b001:
						info.instr = DspInstruction::LRRD;
						load = true;
						break;
					case 0b010:
						info.instr = DspInstruction::LRRI;
						load = true;
						break;
					case 0b011:
						info.instr = DspInstruction::LRRN;
						load = true;
						break;
					case 0b100:
						info.instr = DspInstruction::SRR;
						load = false;
						break;
					case 0b101:
						info.instr = DspInstruction::SRRD;
						load = false;
						break;
					case 0b110:
						info.instr = DspInstruction::SRRI;
						load = false;
						break;
					case 0b111:
						info.instr = DspInstruction::SRRN;
						load = false;
						break;
				}

				if (load)
				{
					int dd = (info.instrBits & 0x1f);
					int ss = (info.instrBits >> 5) & 3;

					if (!AddParam(info, (DspParameter)dd, dd))
						return false;
					if (!AddParam(info, (DspParameter)((int)DspParameter::Indexed_regs + ss), ss))
						return false;
				}
				else
				{
					int ss = (info.instrBits & 0x1f);
					int dd = (info.instrBits >> 5) & 3;

					if (!AddParam(info, (DspParameter)((int)DspParameter::Indexed_regs + dd), dd))
						return false;
					if (!AddParam(info, (DspParameter)ss, ss))
						return false;
				}
				break;
			}
			case 3:		// MRR
			{
				int dd = (info.instrBits >> 5) & 0x1f;
				int ss = (info.instrBits & 0x1f);

				info.instr = DspInstruction::MRR;

				if (!AddParam(info, (DspParameter)dd, dd))
					return false;
				if (!AddParam(info, (DspParameter)ss, ss))
					return false;
				break;
			}
		}

		return true;
	}

	bool Analyzer::Group2(AnalyzeInfo& info)
	{
		//LRS * 	0010 0ddd mmmm mmmm 
		//SRS * 	0010 1sss mmmm mmmm 

		if ((info.instrBits & 0b100000000000) != 0)
		{
			info.instr = DspInstruction::SRS;
			int ss = (info.instrBits >> 8) & 7;
			DspAddress addr = (DspAddress)(0xFF00 | (info.instrBits & 0xff));		// By default bank reg = 0xFF

			if (!AddImmOperand(info, DspParameter::Address, addr))
				return false;
			if (!AddParam(info, (DspParameter)(0x18 + ss), 0x18 + ss))
				return false;
		}
		else
		{
			info.instr = DspInstruction::LRS;
			int dd = (info.instrBits >> 8) & 7;
			DspAddress addr = (DspAddress)(0xFF00 | (info.instrBits & 0xff));		// By default bank reg = 0xFF

			if (!AddParam(info, (DspParameter)(0x18 + dd), 0x18 + dd))
				return false;
			if (!AddImmOperand(info, DspParameter::Address, addr))
				return false;
		}

		return true;
	}

	bool Analyzer::Group3(AnalyzeInfo& info)
	{
		//XORR *      0011 00sd xxxx xxxx         // XORR $acD.m, $axS.h 
		//ANDR *      0011 01sd xxxx xxxx         // ANDR $acD.m, $axS.h 
		//ORR *       0011 10sd xxxx xxxx         // ORR $acD.m, $axS.h 
		//ANDC *      0011 110d xxxx xxxx         // ANDC $acD.m, $ac(1-D).m 
		//ORC *       0011 111d xxxx xxxx         // ORC $acD.m, $ac(1-D).m 

		info.logic = true;

		if (((info.instrBits >> 10) & 3) == 0b11)		// ANDC, ORC
		{
			int dd = (info.instrBits >> 8) & 1;

			if (info.instrBits & 0b1000000000)
			{
				info.instr = DspInstruction::ORC;
			}
			else
			{
				info.instr = DspInstruction::ANDC;
			}

			if (!AddParam(info, (DspParameter)((int)DspParameter::ac0m + dd), dd))
				return false;
			if (!AddParam(info, (DspParameter)((int)DspParameter::ac0m + (1 - dd)), (1 - dd)))
				return false;
		}
		else	// XORR, ANDR, ORR
		{
			int ss = (info.instrBits >> 9) & 1;
			int dd = (info.instrBits >> 8) & 1;

			switch ((info.instrBits >> 10) & 3)
			{
				case 0:
					info.instr = DspInstruction::XORR;
					break;
				case 1:
					info.instr = DspInstruction::ANDR;
					break;
				case 2:
					info.instr = DspInstruction::ORR;
					break;
			}

			if (!AddParam(info, (DspParameter)((int)DspParameter::ac0m + dd), dd))
				return false;
			if (!AddParam(info, ss ? DspParameter::ax1h : DspParameter::ax0h, ss))
				return false;
		}

		return true;
	}

	bool Analyzer::Group4(AnalyzeInfo& info)
	{
		//ADDR *      0100 0ssd xxxx xxxx         // ADDR $acD, $(0x18+S) 
		//ADDAX *     0100 10sd xxxx xxxx         // ADDAX $acD, $axS 
		//ADD *       0100 110d xxxx xxxx         // ADD $acD, $ac(1-D) 
		//ADDP *      0100 111d xxxx xxxx         // ADDP $acD 

		int dd = (info.instrBits >> 8) & 1;

		if ((info.instrBits & 0b100000000000) != 0)
		{
			if ((info.instrBits & 0b010000000000) != 0)
			{
				if ((info.instrBits & 0b001000000000) != 0)
				{
					// ADDP
					info.instr = DspInstruction::ADDP;

					if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
						return false;
				}
				else
				{
					// ADD
					info.instr = DspInstruction::ADD;

					if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
						return false;
					if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + (1-dd)), (1-dd)))
						return false;
				}
			}
			else
			{
				// ADDAX
				int ss = (info.instrBits >> 9) & 1;
				info.instr = DspInstruction::ADDAX;

				if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
					return false;
				if (!AddParam(info, (DspParameter)((int)DspParameter::ax0 + ss), ss))
					return false;
			}
		}
		else
		{
			// ADDR
			int ss = (info.instrBits >> 9) & 3;
			info.instr = DspInstruction::ADDR;

			if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
				return false;
			if (!AddParam(info, (DspParameter)(0x18 + ss), 0x18 + ss))
				return false;
		}

		return true;
	}

	bool Analyzer::Group5(AnalyzeInfo& info)
	{
		//SUBR *      0101 0ssd xxxx xxxx         // SUBR $acD, $(0x18+S) 
		//SUBAX *     0101 10sd xxxx xxxx         // SUBAX $acD, $axS 
		//SUB *       0101 110d xxxx xxxx         // SUB $acD, $ac(1-D) 
		//SUBP *      0101 111d xxxx xxxx         // SUBP $acD 

		int dd = (info.instrBits >> 8) & 1;

		if ((info.instrBits & 0b100000000000) != 0)
		{
			if ((info.instrBits & 0b010000000000) != 0)
			{
				if ((info.instrBits & 0b001000000000) != 0)
				{
					// SUBP
					info.instr = DspInstruction::SUBP;

					if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
						return false;
				}
				else
				{
					// SUB
					info.instr = DspInstruction::SUB;

					if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
						return false;
					if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + (1 - dd)), (1 - dd)))
						return false;
				}
			}
			else
			{
				// SUBAX
				int ss = (info.instrBits >> 9) & 1;
				info.instr = DspInstruction::SUBAX;

				if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
					return false;
				if (!AddParam(info, (DspParameter)((int)DspParameter::ax0 + ss), ss))
					return false;
			}
		}
		else
		{
			// SUBR
			int ss = (info.instrBits >> 9) & 3;
			info.instr = DspInstruction::SUBR;

			if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
				return false;
			if (!AddParam(info, (DspParameter)(0x18 + ss), 0x18 + ss))
				return false;
		}

		return true;
	}

	bool Analyzer::Group6(AnalyzeInfo& info)
	{
		//MOVR *      0110 0ssd xxxx xxxx         // MOVR $acD, $(0x18+S) 
		//MOVAX *     0110 10sd xxxx xxxx         // MOVAX $acD, $axS 
		//MOV *       0110 110d xxxx xxxx         // MOV $acD, $ac(1-D) 
		//MOVP *      0110 111d xxxx xxxx         // MOVP $acD 

		int dd = (info.instrBits >> 8) & 1;

		if ((info.instrBits & 0b100000000000) != 0)
		{
			if ((info.instrBits & 0b010000000000) != 0)
			{
				if ((info.instrBits & 0b001000000000) != 0)
				{
					// MOVP
					info.instr = DspInstruction::MOVP;

					if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
						return false;
				}
				else
				{
					// MOV
					info.instr = DspInstruction::MOV;

					if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
						return false;
					if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + (1 - dd)), (1 - dd)))
						return false;
				}
			}
			else
			{
				// MOVAX
				int ss = (info.instrBits >> 9) & 1;
				info.instr = DspInstruction::MOVAX;

				if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
					return false;
				if (!AddParam(info, (DspParameter)((int)DspParameter::ax0 + ss), ss))
					return false;
			}
		}
		else
		{
			// MOVR
			int ss = (info.instrBits >> 9) & 3;
			info.instr = DspInstruction::MOVR;

			if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
				return false;
			if (!AddParam(info, (DspParameter)(0x18 + ss), 0x18 + ss))
				return false;
		}

		return true;
	}

	bool Analyzer::Group7(AnalyzeInfo& info)
	{
		//ADDAXL *    0111 00sd xxxx xxxx         // ADDAXL $acD, $axS.l 
		//INCM *      0111 010d xxxx xxxx         // INCM $acsD  (mid accum)
		//INC *       0111 011d xxxx xxxx         // INC $acD 
		//DECM *      0111 100d xxxx xxxx         // DECM $acsD  (mid)
		//DEC *       0111 101d xxxx xxxx         // DEC $acD 
		//NEG *       0111 110d xxxx xxxx         // NEG $acD 
		//MOVNP *     0111 111d xxxx xxxx         // MOVNP $acD 

		int dd = (info.instrBits >> 8) & 1;

		if ((info.instrBits & 0b110000000000) != 0)
		{
			// Others

			switch ((info.instrBits >> 9) & 7)
			{
				case 0b010:
					info.instr = DspInstruction::INCM;
					break;
				case 0b011:
					info.instr = DspInstruction::INC;
					break;
				case 0b100:
					info.instr = DspInstruction::DECM;
					break;
				case 0b101:
					info.instr = DspInstruction::DEC;
					break;
				case 0b110:
					info.instr = DspInstruction::NEG;
					break;
				case 0b111:
					info.instr = DspInstruction::MOVNP;
					break;
			}

			if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
				return false;
		}
		else
		{
			// ADDAXL
			int ss = (info.instrBits >> 9) & 1;

			info.instr = DspInstruction::ADDAXL;

			if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
				return false;
			if (!AddParam(info, ss ? DspParameter::ax1l : DspParameter::ax0l, ss))
				return false;
		}

		return true;
	}

	bool Analyzer::Group8(AnalyzeInfo& info)
	{
		//NX * 			1000 r000 xxxx xxxx			(possibly mov r, r)
		//CLR * 		1000 r001 xxxx xxxx 
		//CMP * 		1000 0010 xxxx xxxx 
		//UNUSED*		1000 0011 xxxx xxxx 
		//CLRP * 		1000 0100 xxxx xxxx 
		//TSTAXH 		1000 011r xxxx xxxx 
		//M2/M0 		1000 101x xxxx xxxx 
		//CLR15/SET15	1000 110x xxxx xxxx 
		//CLR40/SET40	1000 111x xxxx xxxx 

		switch ((info.instrBits >> 8) & 0xf)
		{
			case 0b0000:		// NX
			case 0b1000:
				info.instr = DspInstruction::NX;
				break;

			case 0b0001:		// CLR acR
			case 0b1001:
			{
				int r = (info.instrBits >> 11) & 1;
				info.instr = DspInstruction::CLR;
				if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + r), r))
					return false;
				break;
			}

			case 0b0010:		// CMP
				info.instr = DspInstruction::CMP;
				break;

			case 0b0100:		// CLRP
				info.instr = DspInstruction::CLRP;
				break;

			case 0b0110:		// TSTAXH axR.h 
			case 0b0111:
			{
				int r = (info.instrBits >> 8) & 1;
				info.instr = DspInstruction::TSTAXH;
				if (!AddParam(info, r ? DspParameter::ax1h : DspParameter::ax0h, r))
					return false;
				break;
			}

			case 0b1010:		// M2
				info.instr = DspInstruction::M2;
				break;
			case 0b1011:		// M0
				info.instr = DspInstruction::M0;
				break;

			case 0b1100:		// CLR15
				info.instr = DspInstruction::CLR15;
				break;
			case 0b1101:		// SET15
				info.instr = DspInstruction::SET15;
				break;

			case 0b1110:		// CLR40
				info.instr = DspInstruction::CLR40;
				break;
			case 0b1111:		// SET40
				info.instr = DspInstruction::SET40;
				break;
		}

		return true;
	}

	bool Analyzer::Group9(AnalyzeInfo& info)
	{
		//MUL *       1001 s000 xxxx xxxx         // MUL $axS.l, $axS.h 
		//ASR16 *     1001 s001 xxxx xxxx         // ASR16 $acS 
		//MULMVZ *    1001 s01r xxxx xxxx         // MULMVZ $axS.l, $axS.h, $acR 
		//MULAC *     1001 s10r xxxx xxxx         // MULAC $axS.l, $axS.h, $acR 
		//MULMV *     1001 s11r xxxx xxxx         // MULMV $axS.l, $axS.h, $acR 

		int ss = (info.instrBits >> 11) & 1;
		int rr = (info.instrBits >> 8) & 1;
		info.madd = true;
		bool hasR = false;

		switch ((info.instrBits >> 9) & 3)
		{
			case 0:
				if ((info.instrBits & 0b100000000) != 0)
				{
					// ASR16
					info.madd = false;
					info.instr = DspInstruction::ASR16;
					if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + ss), ss))
						return false;
				}
				else
				{
					// MUL
					info.instr = DspInstruction::MUL;
					hasR = false;
				}
				break;
			case 1:		// MULMVZ
				info.instr = DspInstruction::MULMVZ;
				hasR = true;
				break;
			case 2:		// MULAC
				info.instr = DspInstruction::MULAC;
				hasR = true;
				break;
			case 3:		// MULMV
				info.instr = DspInstruction::MULMV;
				hasR = true;
				break;
		}

		if (info.madd)
		{
			if (!AddParam(info, ss ? DspParameter::ax1l : DspParameter::ax0l, ss))
				return false;
			if (!AddParam(info, ss ? DspParameter::ax1h : DspParameter::ax0h, ss))
				return false;

			if (hasR)
			{
				if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + rr), rr))
					return false;
			}
		}

		return true;
	}

	bool Analyzer::GroupAB(AnalyzeInfo& info)
	{
		//MULX *      101s t000 xxxx xxxx         // MULX $ax0.S, $ax1.T 
		//ABS		  1010 t001 xxxx xxxx         // ABS $acT 
		//TST		  1011 t001 xxxx xxxx		  // TST $acT
		//MULXMVZ *   101s t01r xxxx xxxx         // MULXMVZ $ax0.S, $ax1.T, $acR 
		//MULXAC *    101s t10r xxxx xxxx         // MULXAC $ax0.S, $ax1.T, $acR 
		//MULXMV *    101s t11r xxxx xxxx         // MULXMV $ax0.S, $ax1.T, $acR

		info.madd = true;
		int ss = (info.instrBits >> 12) & 1;
		int tt = (info.instrBits >> 11) & 1;
		int rr = (info.instrBits >> 8) & 1;
		bool hasR = false;

		switch ((info.instrBits >> 9) & 3)
		{
			case 0:
				if ((info.instrBits & 0b100000000) != 0)
				{
					info.madd = false;

					if ((info.instrBits & 0b1000000000000) != 0)
					{
						// TST
						info.instr = DspInstruction::TST;
					}
					else
					{
						// ABS
						info.instr = DspInstruction::ABS;
					}
					if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + tt), tt))
						return false;
				}
				else
				{
					// MULX
					info.instr = DspInstruction::MULX;
					hasR = false;
				}
				break;
			case 1:		// MULXMVZ
				info.instr = DspInstruction::MULXMVZ;
				hasR = true;
				break;
			case 2:		// MULXAC
				info.instr = DspInstruction::MULXAC;
				hasR = true;
				break;
			case 3:		// MULXMV
				info.instr = DspInstruction::MULXMV;
				hasR = true;
				break;
		}

		if (info.madd)
		{
			if (!AddParam(info, (DspParameter)((int)DspParameter::ax0l + ss), ss))
				return false;
			if (!AddParam(info, (DspParameter)((int)DspParameter::ax1l + tt), tt))
				return false;

			if (hasR)
			{
				if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + rr), rr))
					return false;
			}
		}

		return true;
	}

	bool Analyzer::GroupCD(AnalyzeInfo& info)
	{
		//MULC *      110s t000 xxxx xxxx         // MULC $acS.m, $axT.h 
		//CMPAR *	  110r s001 xxxx xxxx         // CMPAR $acS.m, $ax1.l/h  (r=0: low, r=1: high) 
		//MULCMVZ *   110s t01r xxxx xxxx         // MULCMVZ $acS.m, $axT.h, $acR 
		//MULCAC *    110s t10r xxxx xxxx         // MULCAC $acS.m, $axT.h, $acR 
		//MULCMV *    110s t11r xxxx xxxx         // MULCMV $acS.m, $axT.h, $acR 

		info.madd = true;
		int ss = (info.instrBits >> 12) & 1;
		int tt = (info.instrBits >> 11) & 1;
		int rr = (info.instrBits >> 8) & 1;
		bool hasR = false;

		switch ((info.instrBits >> 9) & 3)
		{
			case 0:
				if ((info.instrBits & 0b100000000) != 0)
				{
					// CMPAR
					info.madd = false;

					info.instr = DspInstruction::CMPAR;

					ss = (info.instrBits >> 11) & 1;
					rr = (info.instrBits >> 12) & 1;

					if (!AddParam(info, (DspParameter)((int)DspParameter::ac0m + ss), ss))
						return false;
					if (!AddParam(info, rr ? DspParameter::ax1h : DspParameter::ax1l, rr))
						return false;
				}
				else
				{
					// MULC
					info.instr = DspInstruction::MULC;
					hasR = false;
				}
				break;
			case 1:		// MULCMVZ
				info.instr = DspInstruction::MULCMVZ;
				hasR = true;
				break;
			case 2:		// MULCAC
				info.instr = DspInstruction::MULCAC;
				hasR = true;
				break;
			case 3:		// MULCMV
				info.instr = DspInstruction::MULCMV;
				hasR = true;
				break;
		}

		if (info.madd)
		{
			if (!AddParam(info, ss ? DspParameter::ac1m : DspParameter::ac0m, ss))
				return false;
			if (!AddParam(info, tt ? DspParameter::ax1h : DspParameter::ax0h, tt))
				return false;

			if (hasR)
			{
				if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + rr), rr))
					return false;
			}
		}

		return true;
	}

	bool Analyzer::GroupE(AnalyzeInfo& info)
	{
		//MADDX **    1110 00st xxxx xxxx         // MADDX $(0x18+S*2), $(0x19+T*2) 
		//MSUBX **    1110 01st xxxx xxxx         // MSUBX $(0x18+S*2), $(0x19+T*2) 
		//MADDC **    1110 10st xxxx xxxx         // MADDC $acS.m, $axT.h 
		//MSUBC **    1110 11st xxxx xxxx         // MSUBC $acS.m, $axT.h 

		info.madd = true;
		int ss = (info.instrBits >> 9) & 1;
		int tt = (info.instrBits >> 8) & 1;

		if ((info.instrBits & 0b100000000000) != 0)
		{
			// MADDC, MSUBC
			info.instr = (info.instrBits & 0b010000000000) ?
				DspInstruction::MSUBC : DspInstruction::MADDC;

			if (!AddParam(info, ss ? DspParameter::ac1m : DspParameter::ac0m, ss))
				return false;
			if (!AddParam(info, tt ? DspParameter::ax1h : DspParameter::ax0h, tt))
				return false;
		}
		else
		{
			// MADDX, MSUBX
			info.instr = (info.instrBits & 0b010000000000) ?
				DspInstruction::MSUBX : DspInstruction::MADDX;

			if (!AddParam(info, (DspParameter)(0x18 + ss * 2), 0x18 + ss * 2))
				return false;
			if (!AddParam(info, (DspParameter)(0x19 + tt * 2), 0x19 + tt * 2))
				return false;
		}

		return true;
	}

	bool Analyzer::GroupF(AnalyzeInfo& info)
	{
		//LSL16 *     1111 000d xxxx xxxx         // LSL16 $acD
		//MADD *      1111 001d xxxx xxxx         // MADD $axD.l, $axD.h 
		//LSR16 *     1111 010d xxxx xxxx         // LSR16 $acD
		//MSUB *      1111 011d xxxx xxxx         // MSUB $axD.l, $axD.h 
		//ADDPAXZ *   1111 10sd xxxx xxxx         // ADDPAXZ $acD, $ax1.[l|h] 
		//CLRL *      1111 110d xxxx xxxx         // CLRL $acD.l 
		//MOVPZ *     1111 111d xxxx xxxx         // MOVPZ $acD 

		int dd = (info.instrBits >> 8) & 1;
		int ss = (info.instrBits >> 9) & 1;

		switch ((info.instrBits >> 9) & 7)
		{
			case 0b000:		// LSL16
				info.logic = true;
				info.instr = DspInstruction::LSL16;
				if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
					return false;
				break;
			case 0b001:		// MADD
				info.madd = true;
				info.instr = DspInstruction::MADD;
				if (!AddParam(info, dd ? DspParameter::ax1l : DspParameter::ax0l, dd))
					return false;
				if (!AddParam(info, dd ? DspParameter::ax1h : DspParameter::ax0h, dd))
					return false;
				break;
			case 0b010:		// LSR16
				info.logic = true;
				info.instr = DspInstruction::LSR16;
				if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
					return false;
				break;
			case 0b011:		// MSUB
				info.madd = true;
				info.instr = DspInstruction::MSUB;
				if (!AddParam(info, dd ? DspParameter::ax1l : DspParameter::ax0l, dd))
					return false;
				if (!AddParam(info, dd ? DspParameter::ax1h : DspParameter::ax0h, dd))
					return false;
				break;
			case 0b100:		// ADDPAXZ
			case 0b101:
				info.instr = DspInstruction::ADDPAXZ;
				if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
					return false;
				if (!AddParam(info, ss ? DspParameter::ax1h : DspParameter::ax1l, ss))
					return false;
				break;
			case 0b110:		// CLRL
				info.instr = DspInstruction::CLRL;
				if (!AddParam(info, dd ? DspParameter::ac1l : DspParameter::ac0l, dd))
					return false;
				break;
			case 0b111:		// MOVPZ
				info.instr = DspInstruction::MOVPZ;
				if (!AddParam(info, (DspParameter)((int)DspParameter::ac0 + dd), dd))
					return false;
				break;
		}

		return true;
	}

	// DSP instructions are in a hybrid format: some instructions occupy a full 16-bit word, and some can be packed as two 8-bit instructions per word.
	// Extended opcodes represents lower-part of instruction pair.

	bool Analyzer::GroupPacked(AnalyzeInfo& info)
	{
		// x is used as part of Group3. Otherwise x=0.

		//?? * 		xxxx xxxx x000 00rr		// NOP2 (effectivly $arR = $arR + 0)
		//DR * 		xxxx xxxx x000 01rr		// DR $arR 
		//IR * 		xxxx xxxx x000 10rr		// IR $arR 
		//NR * 		xxxx xxxx x000 11rr		// NR $arR, ixR
		//MV * 		xxxx xxxx x001 ddss 	// MV $(0x18+D), $(0x1c+S) 
		//S * 		xxxx xxxx x01s s0dd		// S @$rD, $(0x1c+s)  
		//SN * 		xxxx xxxx x01s s1dd		// SN @$rD, $(0x1c+s)
		//L * 		xxxx xxxx x1dd d0ss 	// L $(0x18+D), @$rS 
		//LN * 		xxxx xxxx x1dd d1ss 	// LN $(0x18+D), @$rS 

		// Cannot be used with Group3 (bit7 is used as part of Group3 upper instruction)

		//LS * 		xxxx xxxx 10dd 000s 	// LS $(0x18+D), $acS.m 
		//SL * 		xxxx xxxx 10dd 001s		// SL $acS.m, $(0x18+D)  
		//LSN * 	xxxx xxxx 10dd 010s		// LSN $(0x18+D), $acS.m 
		//SLN * 	xxxx xxxx 10dd 011s		// SLN $acS.m, $(0x18+D)
		//LSM * 	xxxx xxxx 10dd 100s		// LSM $(0x18+D), $acS.m 
		//SLM * 	xxxx xxxx 10dd 101s		// SLM $acS.m, $(0x18+D)
		//LSNM * 	xxxx xxxx 10dd 110s		// LSNM $(0x18+D), $acS.m 
		//SLNM * 	xxxx xxxx 10dd 111s		// SLNM $acS.m, $(0x18+D)

		//LD 		xxxx xxxx 11dr 00ss		// LD $ax0.d, $ax1.r, @$arS 
		//LDN 		xxxx xxxx 11dr 01ss		// LDN $ax0.d, $ax1.r, @$arS
		//LDM 		xxxx xxxx 11dr 10ss		// LDM $ax0.d, $ax1.r, @$arS
		//LDNM 		xxxx xxxx 11dr 11ss		// LDNM $ax0.d, $ax1.r, @$arS

		//LDAX 		xxxx xxxx 11sr 0011		// LDAX $axR, @$arS
		//LDAXN 	xxxx xxxx 11sr 0111		// LDAXN $axR, @$arS
		//LDAXM 	xxxx xxxx 11sr 1011		// LDAXM $axR, @$arS
		//LDAXNM 	xxxx xxxx 11sr 1111		// LDAXNM $axR, @$arS

		info.extendedOpcodePresent = true;

		info.instrExBits = info.instrBits & 0xff;

		// Not all can be used with Group3 (paired logic)

		if ((info.instrBits >> 12) == 3)
		{
			info.instrExBits &= 0x7f;
		}

		switch (info.instrExBits >> 6)
		{
			case 0:
			{
				if ((info.instrExBits & 0b100000) != 0)		// S, SN
				{
					int ss = (info.instrExBits >> 3) & 3;
					int dd = info.instrExBits & 3;

					if ((info.instrExBits & 0b100) != 0)
					{
						info.instrEx = DspInstructionEx::SN;
					}
					else
					{
						info.instrEx = DspInstructionEx::S;
					}

					if (!AddParamEx(info, (DspParameter)((int)DspParameter::Indexed_regs + dd), dd))
						return false;
					if (!AddParamEx(info, (DspParameter)(0x1c + ss), 0x1c + ss))
						return false;
				}
				else if ((info.instrExBits & 0b10000) != 0)		// MV
				{
					int dd = (info.instrExBits >> 2) & 3;
					int ss = info.instrExBits & 3;

					info.instrEx = DspInstructionEx::MV;

					if (!AddParamEx(info, (DspParameter)(0x18 + dd), 0x18 + dd))
						return false;
					if (!AddParamEx(info, (DspParameter)(0x1c + ss), 0x1c + ss))
						return false;
				}
				else	// NOP2, DR, IR, NR
				{
					int rr = info.instrExBits & 3;

					switch ((info.instrExBits >> 2) & 3)
					{
						case 0:
							info.instrEx = DspInstructionEx::NOP2;
							break;
						case 1:
							info.instrEx = DspInstructionEx::DR;
							if (!AddParamEx(info, (DspParameter)((int)DspParameter::ar0 + rr), rr))
								return false;
							break;
						case 2:
							info.instrEx = DspInstructionEx::IR;
							if (!AddParamEx(info, (DspParameter)((int)DspParameter::ar0 + rr), rr))
								return false;
							break;
						case 3:
							info.instrEx = DspInstructionEx::NR;
							if (!AddParamEx(info, (DspParameter)((int)DspParameter::ar0 + rr), rr))
								return false;
							if (!AddParamEx(info, (DspParameter)((int)DspParameter::ix0 + rr), rr))
								return false;
							break;
					}
				}

				break;
			}
			case 1:		// L, LN
			{
				int dd = (info.instrExBits >> 3) & 7;
				int ss = info.instrExBits & 3;

				if (info.instrExBits & 0b100)
				{
					info.instrEx = DspInstructionEx::LN;
				}
				else
				{
					info.instrEx = DspInstructionEx::L;
				}

				if (!AddParamEx(info, (DspParameter)(0x18 + dd), 0x18 + dd))
					return false;
				if (!AddParamEx(info, (DspParameter)((int)DspParameter::Indexed_regs + ss), ss))
					return false;

				break;
			}
			case 2:		// LSx, SLx
			{
				int dd = (info.instrExBits >> 4) & 3;
				int ss = info.instrExBits & 1;
				bool load = false;

				switch ((info.instrExBits >> 1) & 7)
				{
					case 0b000:
						info.instrEx = DspInstructionEx::LS;
						load = true;
						break;
					case 0b001:
						info.instrEx = DspInstructionEx::SL;
						load = false;
						break;
					case 0b010:
						info.instrEx = DspInstructionEx::LSN;
						load = true;
						break;
					case 0b011:
						info.instrEx = DspInstructionEx::SLN;
						load = false;
						break;
					case 0b100:
						info.instrEx = DspInstructionEx::LSM;
						load = true;
						break;
					case 0b101:
						info.instrEx = DspInstructionEx::SLM;
						load = false;
						break;
					case 0b110:
						info.instrEx = DspInstructionEx::LSNM;
						load = true;
						break;
					case 0b111:
						info.instrEx = DspInstructionEx::SLNM;
						load = false;
						break;
				}

				if (load)
				{
					if (!AddParamEx(info, (DspParameter)(0x18 + dd), 0x18 + dd))
						return false;
					if (!AddParamEx(info, (DspParameter)((int)DspParameter::ac0m + ss), ss))
						return false;
				}
				else
				{
					if (!AddParamEx(info, (DspParameter)((int)DspParameter::ac0m + ss), ss))
						return false;
					if (!AddParamEx(info, (DspParameter)(0x18 + dd), 0x18 + dd))
						return false;
				}

				break;
			}
			case 3:		// LDx
			{
				if ((info.instrExBits & 3) == 0b11)
				{
					int ss = (info.instrExBits >> 5) & 1;
					int rr = (info.instrExBits >> 4) & 1;

					switch ((info.instrExBits >> 2) & 3)
					{
						case 0:
							info.instrEx = DspInstructionEx::LDAX;
							break;
						case 1:
							info.instrEx = DspInstructionEx::LDAXN;
							break;
						case 2:
							info.instrEx = DspInstructionEx::LDAXM;
							break;
						case 3:
							info.instrEx = DspInstructionEx::LDAXNM;
							break;
					}

					if (!AddParamEx(info, (DspParameter)((int)DspParameter::ax0 + rr), rr))
						return false;
					if (!AddParamEx(info, (DspParameter)((int)DspParameter::Indexed_ar0 + ss), ss))
						return false;
				}
				else
				{
					int dd = (info.instrExBits >> 5) & 1;
					int rr = (info.instrExBits >> 4) & 1;
					int ss = info.instrExBits & 3;

					switch ((info.instrExBits >> 2) & 3)
					{
						case 0:
							info.instrEx = DspInstructionEx::LD;
							break;
						case 1:
							info.instrEx = DspInstructionEx::LDN;
							break;
						case 2:
							info.instrEx = DspInstructionEx::LDM;
							break;
						case 3:
							info.instrEx = DspInstructionEx::LDNM;
							break;
					}

					if (!AddParamEx(info, (DspParameter)((int)DspParameter::ax0l + dd), dd))
						return false;
					if (!AddParamEx(info, (DspParameter)((int)DspParameter::ax1l + rr), rr))
						return false;
					if (!AddParamEx(info, (DspParameter)((int)DspParameter::Indexed_ar0 + ss), ss))
						return false;
				}
				break;
			}
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

	bool Analyzer::AddParamEx(AnalyzeInfo& info, DspParameter param, uint16_t paramBits)
	{
		if (info.numParametersEx >= _countof(info.paramsEx))
			return false;

		info.paramsEx[info.numParametersEx] = param;
		info.paramExBits[info.numParametersEx] = paramBits;

		info.numParametersEx++;

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

		info.instrBits = _byteswap_ushort(*(uint16_t*)instrPtr);

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
