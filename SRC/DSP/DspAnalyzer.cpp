// DSP analyzer
#include "pch.h"

namespace DSP
{
	void Analyzer::ResetInfo(AnalyzeInfo& info)
	{
		info.sizeInBytes = 0;

		info.parallel = false;
		info.instr = DspRegularInstruction::Unknown;

		info.numParameters = 0;
		info.numParametersEx = 0;

		info.flowControl = false;
	}

	void Analyzer::AddParam(AnalyzeInfo& info, DspParameter param)
	{
		assert(info.numParameters < DspAnalyzeNumParam);

		info.params[info.numParameters] = param;
		info.numParameters++;
	}

	void Analyzer::AddParamEx(AnalyzeInfo& info, DspParameter param)
	{
		assert(info.numParametersEx < DspAnalyzeNumParam);

		info.paramsEx[info.numParametersEx] = param;
		info.numParametersEx++;
	}

	void Analyzer::AddImmOperand(AnalyzeInfo& info, DspParameter param, uint8_t imm)
	{
		AddParam(info, param);
		if (param == DspParameter::Byte)
			info.ImmOperand.Byte = imm;
		else if (param == DspParameter::Byte2)
			info.ImmOperand2.Byte = imm;
	}

	void Analyzer::AddImmOperand(AnalyzeInfo& info, DspParameter param, int8_t imm)
	{
		AddParam(info, param);
		if (param == DspParameter::SignedByte)
			info.ImmOperand.SignedByte = imm;
		else if (param == DspParameter::SignedByte2)
			info.ImmOperand2.SignedByte = imm;
	}

	void Analyzer::AddImmOperand(AnalyzeInfo& info, DspParameter param, uint16_t imm)
	{
		AddParam(info, param);
		if (param == DspParameter::UnsignedShort)
			info.ImmOperand.UnsignedShort = imm;
		else if (param == DspParameter::UnsignedShort2)
			info.ImmOperand2.UnsignedShort = imm;
	}

	void Analyzer::AddImmOperand(AnalyzeInfo& info, DspParameter param, DspAddress imm)
	{
		AddParam(info, param);
		if (param == DspParameter::Address)
			info.ImmOperand.Address = imm;
		else if (param == DspParameter::Address2)
			info.ImmOperand2.Address = imm;
	}

	void Analyzer::AddBytes(uint8_t* instrPtr, size_t bytes, AnalyzeInfo& info)
	{
		assert(info.sizeInBytes < sizeof(info.bytes));

		memcpy(&info.bytes[info.sizeInBytes], instrPtr, bytes);
		info.sizeInBytes += bytes;
	}

	void Analyzer::Group0(uint8_t* instrPtr, size_t instrMaxSize, AnalyzeInfo& info, uint16_t instrBits)
	{
		switch ((instrBits >> 8) & 0xf)
		{
			//|nop         |0000 0000 0000 0000|
			//|mr rn,mn    |0000 0000 000m mmrr|
			//|trap        |0000 0000 0010 0000|
			//|wait        |0000 0000 0010 0001|
			//|repr reg    |0000 0000 010r rrrr|
			//|loopr reg,ea|0000 0000 011r rrrr aaaa aaaa aaaa aaaa|
			//|mvli d,li   |0000 0000 100d dddd iiii iiii iiii iiii|
			//|ldla d,la   |0000 0000 110d dddd aaaa aaaa aaaa aaaa|
			//|stla la,s   |0000 0000 111s ssss aaaa aaaa aaaa aaaa|

			case 0:
				if ((instrBits & 0x80) == 0)
				{

				}
				else
				{
					switch ((instrBits >> 5) & 3)
					{
						case 0:
						{
							info.instr = DspRegularInstruction::mvli;

							int r = instrBits & 0x1f;
							uint16_t imm = _BYTESWAP_UINT16(*(uint16_t*)instrPtr);
							AddBytes(instrPtr, sizeof(uint16_t), info);
							AddParam(info, (DspParameter)((int)DspParameter::regs + r));
							AddImmOperand(info, DspParameter::UnsignedShort, imm);
							break;
						}
						case 1:
							// Reserved
							break;
						case 2:
						{
							info.instr = DspRegularInstruction::ldla;

							int r = instrBits & 0x1f;
							DspAddress addr = _BYTESWAP_UINT16(*(uint16_t*)instrPtr);
							AddBytes(instrPtr, sizeof(uint16_t), info);
							AddParam(info, (DspParameter)((int)DspParameter::regs + r));
							AddImmOperand(info, DspParameter::Address, addr);
							break;
						}
						case 3:
						{
							info.instr = DspRegularInstruction::stla;

							int r = instrBits & 0x1f;
							DspAddress addr = _BYTESWAP_UINT16(*(uint16_t*)instrPtr);
							AddBytes(instrPtr, sizeof(uint16_t), info);
							AddImmOperand(info, DspParameter::Address, addr);
							AddParam(info, (DspParameter)((int)DspParameter::regs + r));
							break;
						}
					}
				}
				break;

			case 1:
				// Reserved
				break;

			//|adli d,li   |0000 001d 0000 0000 iiii iiii iiii iiii|
			//|norm d,rn   |0000 001d 0000 01rr|
			//|negc d      |0000 001d 0000 1101|
			//|pld d,rn,mn |0000 001d 0001 mmrr|
			//|xorli d,li  |0000 001d 0010 0000 iiii iiii iiii iiii|
			//|anli d,li   |0000 001d 0100 0000 iiii iiii iiii iiii|
			//|orli d,li   |0000 001d 0110 0000 iiii iiii iiii iiii|
			//|div d,s     |0000 001d 0ss0 1000|
			//|max d,s     |0000 001d 0ss0 1001|
			//|lsf d,s     |0000 001d 01s0 1010|
			//|asf d,s     |0000 001d 01s0 1011|
			//|exec(cc)    |0000 0010 0111 cccc|
			//|cmpli s,li  |0000 001s 1000 0000 iiii iiii iiii iiii|
			//|addc d,s    |0000 001d 10s0 1100|
			//|subc d,s    |0000 001d 10s0 1101|
			//|jmp(cc) ta  |0000 0010 1001 cccc aaaa aaaa aaaa aaaa|
			//|btstl d,bs  |0000 001d 1010 0000 bbbb bbbb bbbb bbbb|
			//|call(cc) ta |0000 0010 1011 cccc aaaa aaaa aaaa aaaa|
			//|btsth d,bs  |0000 001d 1100 0000 bbbb bbbb bbbb bbbb|
			//|lsf d,s     |0000 001d 1100 1010|
			//|asf d,s     |0000 001d 1100 1011|
			//|rets(cc)    |0000 0010 1101 cccc|
			//|reti(cc)    |0000 0010 1111 cccc|

			case 2:
			case 3:
				break;

			//|adsi d,si   |0000 010d iiii iiii|

			case 4:
			case 5:
				break;

			//|cmpsi s,si  |0000 011s iiii iiii|

			case 6:
			case 7:
				break;

			//|mvsi d,si   |0000 1ddd iiii iiii|

			case 8:
			case 9:
			case 0xa:
			case 0xb:
			case 0xc:
			case 0xd:
			case 0xe:
			case 0xf:
				break;
		}
	}

	void Analyzer::Group1(uint8_t* instrPtr, size_t instrMaxSize, AnalyzeInfo& info, uint16_t instrBits)
	{

	}

	void Analyzer::Group2(AnalyzeInfo& info, uint16_t instrBits)
	{

	}

	void Analyzer::Group3(AnalyzeInfo& info, uint16_t instrBits)
	{

	}

	void Analyzer::Group4(AnalyzeInfo& info, uint16_t instrBits)
	{

	}

	void Analyzer::Group5(AnalyzeInfo& info, uint16_t instrBits)
	{

	}

	void Analyzer::Group6(AnalyzeInfo& info, uint16_t instrBits)
	{

	}

	void Analyzer::Group7(AnalyzeInfo& info, uint16_t instrBits)
	{

	}

	void Analyzer::Group8(AnalyzeInfo& info, uint16_t instrBits)
	{

	}

	void Analyzer::Group9(AnalyzeInfo& info, uint16_t instrBits)
	{

	}

	void Analyzer::GroupA(AnalyzeInfo& info, uint16_t instrBits)
	{

	}

	void Analyzer::GroupB(AnalyzeInfo& info, uint16_t instrBits)
	{

	}

	void Analyzer::GroupC(AnalyzeInfo& info, uint16_t instrBits)
	{

	}

	void Analyzer::GroupD(AnalyzeInfo& info, uint16_t instrBits)
	{

	}

	void Analyzer::GroupE(AnalyzeInfo& info, uint16_t instrBits)
	{

	}

	void Analyzer::GroupF(AnalyzeInfo& info, uint16_t instrBits)
	{

	}

	void Analyzer::Analyze(uint8_t* instrPtr, size_t instrMaxSize, AnalyzeInfo& info)
	{
		// Reset info

		ResetInfo(info);

		// Get opcode and determine its group

		uint16_t instrBits = _BYTESWAP_UINT16(*(uint16_t*)instrPtr);

		AddBytes(instrPtr, sizeof(uint16_t), info);

		instrPtr += sizeof(instrBits);
		instrMaxSize -= sizeof(instrBits);

		int group = (instrBits >> 12);

		// Select by group. Groups 3-F are parallel instructions.

		info.parallel = group >= 3;

		switch (group)
		{
			case 0:
				Group0(instrPtr, instrMaxSize, info, instrBits);
				break;
			case 1:
				Group1(instrPtr, instrMaxSize, info, instrBits);
				break;
			case 2:
				Group2(info, instrBits);
				break;
			case 3:
				Group3(info, instrBits);
				break;
			case 4:
				Group4(info, instrBits);
				break;
			case 5:
				Group5(info, instrBits);
				break;
			case 6:
				Group6(info, instrBits);
				break;
			case 7:
				Group7(info, instrBits);
				break;
			case 8:
				Group8(info, instrBits);
				break;
			case 9:
				Group9(info, instrBits);
				break;
			case 0xa:
				GroupA(info, instrBits);
				break;
			case 0xb:
				GroupB(info, instrBits);
				break;
			case 0xc:
				GroupC(info, instrBits);
				break;
			case 0xd:
				GroupD(info, instrBits);
				break;
			case 0xe:
				GroupE(info, instrBits);
				break;
			case 0xf:
				GroupF(info, instrBits);
				break;
		}
	}

}
