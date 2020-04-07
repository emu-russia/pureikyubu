// The analyzer is a key component of the processor module.
// It is used by all other interested components (disassembler, interpreter, etc.)

#include "pch.h"

namespace DVD
{
	bool MnAnalyze::Main(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info)
	{
		*instrPtr++ = *instrPtr++;
		info->instrSize++;
		instrMaxSize--;

		int nn = (info->instrBytes[0] >> 2) & 3;
		int mm = info->instrBytes[0] & 3;

		if (info->instrBytes[0] >= 0xF0)
		{
			switch (info->instrBytes[0])
			{
				case 0xF6:		// NOP
					info->instr = MnInstruction::NOP;
					break;
				case 0xF8:		// MOV imm16, Dm
				case 0xF9:
				case 0xFA:
				case 0xFB:
					info->instr = MnInstruction::MOV;
					if (!AddOp(info, MnOperand::Imm16, 0))
						return false;
					if (!FetchImm16(instrPtr, instrMaxSize, info))
						return false;
					if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + mm), mm))
						return false;
					break;
				case 0xFC:		// JMP label16
					info->instr = MnInstruction::JMP;
					info->flow = true;
					if (!AddOp(info, MnOperand::Imm16, 0))
						return false;
					if (!FetchImm16(instrPtr, instrMaxSize, info))
						return false;
					break;
				case 0xFD:		// JSR label16
					info->instr = MnInstruction::JSR;
					info->flow = true;
					if (!AddOp(info, MnOperand::Imm16, 0))
						return false;
					if (!FetchImm16(instrPtr, instrMaxSize, info))
						return false;
					break;
				case 0xFE:		// RTS
					info->instr = MnInstruction::RTS;
					info->flow = true;
					break;
			}
			return true;
		}
		else if (info->instrBytes[0] >= 0xE0)
		{
			switch (info->instrBytes[0])
			{
				case 0xE0:		// Bxx
				case 0xE1:
				case 0xE2:
				case 0xE3:
				case 0xE4:
				case 0xE5:
				case 0xE6:
				case 0xE7:
				case 0xE8:
				case 0xE9:
				case 0xEA:
					info->instr = MnInstruction::Bcc;
					info->cc = (MnCond)(info->instrBytes[0] & 0xf);
					info->flow = true;
					if (!AddOp(info, MnOperand::Imm8, 0))
						return false;
					if (!FetchImm8(instrPtr, instrMaxSize, info))
						return false;
					break;
				case 0xEB:		// RTI
					info->instr = MnInstruction::RTI;
					info->flow = true;
					break;
				case 0xEC:		// CMP imm16, Am
				case 0xED:
				case 0xEE:
				case 0xEF:
					info->instr = MnInstruction::CMP;
					if (!AddOp(info, MnOperand::Imm16, 0))
						return false;
					if (!FetchImm16(instrPtr, instrMaxSize, info))
						return false;
					if (!AddOp(info, (MnOperand)((int)MnOperand::A0 + mm), mm))
						return false;
					break;
			}
			return true;
		}

		switch (info->instrBytes[0] >> 4)
		{
			case 0:			// MOV Dm, (An)
				info->instr = MnInstruction::MOV;
				if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + mm), mm))
					return false;
				if (!AddOp(info, (MnOperand)((int)MnOperand::Ind_A0 + nn), nn))
					return false;
				break;
			case 1:			// MOVB Dm, (An)
				info->instr = MnInstruction::MOVB;
				if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + mm), mm))
					return false;
				if (!AddOp(info, (MnOperand)((int)MnOperand::Ind_A0 + nn), nn))
					return false;
				break;
			case 2:			// MOV (An), Dm
				info->instr = MnInstruction::MOV;
				if (!AddOp(info, (MnOperand)((int)MnOperand::Ind_A0 + nn), nn))
					return false;
				if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + mm), mm))
					return false;
				break;
			case 3:			// MOVBU (An), Dm
				info->instr = MnInstruction::MOVBU;
				if (!AddOp(info, (MnOperand)((int)MnOperand::Ind_A0 + nn), nn))
					return false;
				if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + mm), mm))
					return false;
				break;
			case 4:			// MOV Dm, (d8, An)
				info->instr = MnInstruction::MOV;
				if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + mm), mm))
					return false;
				if (!AddOp(info, (MnOperand)((int)MnOperand::D8_A0 + nn), nn))
					return false;
				if (!FetchImm8(instrPtr, instrMaxSize, info))
					return false;
				break;
			case 5:			// MOV Am, (d8, An)
				info->instr = MnInstruction::MOV;
				if (!AddOp(info, (MnOperand)((int)MnOperand::A0 + mm), mm))
					return false;
				if (!AddOp(info, (MnOperand)((int)MnOperand::D8_A0 + nn), nn))
					return false;
				if (!FetchImm8(instrPtr, instrMaxSize, info))
					return false;
				break;
			case 6:			// MOV (d8, An), Dm
				info->instr = MnInstruction::MOV;
				if (!AddOp(info, (MnOperand)((int)MnOperand::D8_A0 + nn), nn))
					return false;
				if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + mm), mm))
					return false;
				if (!FetchImm8(instrPtr, instrMaxSize, info))
					return false;
				break;
			case 7:				// MOV (d8, An), Am
				info->instr = MnInstruction::MOV;
				if (!AddOp(info, (MnOperand)((int)MnOperand::D8_A0 + nn), nn))
					return false;
				if (!AddOp(info, (MnOperand)((int)MnOperand::A0 + mm), mm))
					return false;
				if (!FetchImm8(instrPtr, instrMaxSize, info))
					return false;
				break;
			case 8:				// MOV Dn, Dm (n==m: MOV imm8, Dn)
				info->instr = MnInstruction::MOV;
				if (nn == mm)
				{
					if (!AddOp(info, MnOperand::Imm8, 0))
						return false;
					if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + nn), nn))
						return false;
					if (!FetchImm8(instrPtr, instrMaxSize, info))
						return false;
				}
				else
				{
					if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + nn), nn))
						return false;
					if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + mm), mm))
						return false;
				}
				break;
			case 9:				// ADD Dn, Dm
				info->instr = MnInstruction::ADD;
				if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + nn), nn))
					return false;
				if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + mm), mm))
					return false;
				break;
			case 0xA:				// SUB Dn, Dm
				info->instr = MnInstruction::SUB;
				if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + nn), nn))
					return false;
				if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + mm), mm))
					return false;
				break;
			case 0xB:			// EXTXxx Dm
				switch (nn)
				{
					case 0: info->instr = MnInstruction::EXTX; break;
					case 1: info->instr = MnInstruction::EXTXU; break;
					case 2: info->instr = MnInstruction::EXTXB; break;
					case 3: info->instr = MnInstruction::EXTXBU; break;
				}
				if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + mm), mm))
					return false;
				break;
			case 0xC:
				switch (nn)
				{
					case 0:		// MOV Dm, (abs16)
						info->instr = MnInstruction::MOV;
						if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + mm), mm))
							return false;
						if (!AddOp(info, MnOperand::Abs16, 0))
							return false;
						if (!FetchImm16(instrPtr, instrMaxSize, info))
							return false;
						break;
					case 1:		// MOVB Dm, (abs16)
						info->instr = MnInstruction::MOVB;
						if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + mm), mm))
							return false;
						if (!AddOp(info, MnOperand::Abs16, 0))
							return false;
						if (!FetchImm16(instrPtr, instrMaxSize, info))
							return false;
						break;
					case 2:		// MOV  (abs16), Dm
						info->instr = MnInstruction::MOV;
						if (!AddOp(info, MnOperand::Abs16, 0))
							return false;
						if (!FetchImm16(instrPtr, instrMaxSize, info))
							return false;
						if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + mm), mm))
							return false;
						break;
					case 3:		// MOVBU  (abs16), Dm
						info->instr = MnInstruction::MOVBU;
						if (!AddOp(info, MnOperand::Abs16, 0))
							return false;
						if (!FetchImm16(instrPtr, instrMaxSize, info))
							return false;
						if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + mm), mm))
							return false;
						break;
				}
				break;
			case 0xD:
				switch (nn)
				{
					case 0:				// ADD imm8, Am
						info->instr = MnInstruction::ADD;
						if (!AddOp(info, MnOperand::Imm8, 0))
							return false;
						if (!FetchImm8(instrPtr, instrMaxSize, info))
							return false;
						if (!AddOp(info, (MnOperand)((int)MnOperand::A0 + mm), mm))
							return false;
						break;
					case 1:				// ADD imm8, Dm
						info->instr = MnInstruction::ADD;
						if (!AddOp(info, MnOperand::Imm8, 0))
							return false;
						if (!FetchImm8(instrPtr, instrMaxSize, info))
							return false;
						if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + mm), mm))
							return false;
						break;
					case 2:				// CMP imm8, Dm
						info->instr = MnInstruction::CMP;
						if (!AddOp(info, MnOperand::Imm8, 0))
							return false;
						if (!FetchImm8(instrPtr, instrMaxSize, info))
							return false;
						if (!AddOp(info, (MnOperand)((int)MnOperand::D0 + mm), mm))
							return false;
						break;
					case 3:				// MOV imm16, Am
						info->instr = MnInstruction::MOV;
						if (!AddOp(info, MnOperand::Imm16, 0))
							return false;
						if (!FetchImm16(instrPtr, instrMaxSize, info))
							return false;
						if (!AddOp(info, (MnOperand)((int)MnOperand::A0 + mm), mm))
							return false;
						break;
				}
				break;
		}

		return true;
	}

	bool MnAnalyze::F0(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info)
	{
		return false;
	}

	bool MnAnalyze::F1(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info)
	{
		return false;
	}

	bool MnAnalyze::F2(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info)
	{
		return false;
	}

	bool MnAnalyze::F3(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info)
	{
		return false;
	}

	bool MnAnalyze::F4(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info)
	{
		return false;
	}

	bool MnAnalyze::F5(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info)
	{
		return false;
	}

	bool MnAnalyze::F7(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info)
	{
		return false;
	}

	bool MnAnalyze::FetchImm8(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info)
	{
		if (instrMaxSize < sizeof(uint8_t))
			return false;
		*instrPtr++ = instrPtr[0];
		info->instrSize++;
		info->imm.Uint8 = instrPtr[0];
		return true;
	}

	bool MnAnalyze::FetchImm16(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info)
	{
		if (instrMaxSize < 2 * sizeof(uint8_t))
			return false;
		*instrPtr++ = instrPtr[0];
		info->instrSize++;
		*instrPtr++ = instrPtr[1];
		info->instrSize++;
		info->imm.Uint16 = ((uint16_t)instrPtr[1] << 8) | instrPtr[0];
		return true;
	}

	bool MnAnalyze::FetchImm24(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info)
	{
		if (instrMaxSize < 3 * sizeof(uint8_t))
			return false;
		*instrPtr++ = instrPtr[0];
		info->instrSize++;
		*instrPtr++ = instrPtr[1];
		info->instrSize++;
		*instrPtr++ = instrPtr[2];
		info->instrSize++;
		info->imm.Uint24 = ((uint32_t)instrPtr[2] << 16) | ((uint32_t)instrPtr[1] << 8) | instrPtr[0];
		return true;
	}

	bool MnAnalyze::AddOp(MnInstrInfo* info, MnOperand op, int bits)
	{
		if (info->numOp >= _countof(info->op))
			return false;

		info->op[info->numOp] = op;
		info->opBits[info->numOp] = bits;

		info->numOp++;

		return true;
	}

	bool MnAnalyze::Analyze(uint8_t* instrPtr, size_t instrMaxSize, MnInstrInfo* info)
	{
		assert(instrPtr);
		assert(info);

		if (instrMaxSize == 0)
			return false;

		info->instr = MnInstruction::Unknown;
		instrPtr = info->instrBytes;
		info->instrSize = 0;
		info->numOp = 0;
		info->flow = false;

		if (instrPtr[0] >= 0xF0)
		{
			switch (instrPtr[0])
			{
				case 0xF0: return F0(instrPtr, instrMaxSize, info);
				case 0xF1: return F1(instrPtr, instrMaxSize, info);
				case 0xF2: return F2(instrPtr, instrMaxSize, info);
				case 0xF3: return F3(instrPtr, instrMaxSize, info);
				case 0xF4: return F4(instrPtr, instrMaxSize, info);
				case 0xF5: return F5(instrPtr, instrMaxSize, info);
				case 0xF6: return Main(instrPtr, instrMaxSize, info);
				case 0xF7: return F7(instrPtr, instrMaxSize, info);
				case 0xF8: return Main(instrPtr, instrMaxSize, info);
				case 0xF9: return Main(instrPtr, instrMaxSize, info);
				case 0xFA: return Main(instrPtr, instrMaxSize, info);
				case 0xFB: return Main(instrPtr, instrMaxSize, info);
				case 0xFC: return Main(instrPtr, instrMaxSize, info);
				case 0xFD: return Main(instrPtr, instrMaxSize, info);
				case 0xFE: return Main(instrPtr, instrMaxSize, info);
				case 0xFF: break;
			}
		}
		else
		{
			return Main(instrPtr, instrMaxSize, info);
		}

		return true;
	}

}
