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

	void Analyzer::AddBytes(uint8_t* instrPtr, size_t bytes, AnalyzeInfo& info)
	{
		assert(info.sizeInBytes < sizeof(info.bytes));

		memcpy(&info.bytes[info.sizeInBytes], instrPtr, bytes);
		info.sizeInBytes += bytes;
	}

	void Analyzer::Group0(uint8_t* instrPtr, size_t instrMaxSize, AnalyzeInfo& info)
	{

	}

	void Analyzer::Group1(uint8_t* instrPtr, size_t instrMaxSize, AnalyzeInfo& info)
	{

	}

	void Analyzer::Group2(AnalyzeInfo& info)
	{

	}

	void Analyzer::Group3(AnalyzeInfo& info)
	{

	}

	void Analyzer::Group4(AnalyzeInfo& info)
	{

	}

	void Analyzer::Group5(AnalyzeInfo& info)
	{

	}

	void Analyzer::Group6(AnalyzeInfo& info)
	{

	}

	void Analyzer::Group7(AnalyzeInfo& info)
	{

	}

	void Analyzer::Group8(AnalyzeInfo& info)
	{

	}

	void Analyzer::Group9(AnalyzeInfo& info)
	{

	}

	void Analyzer::GroupA(AnalyzeInfo& info)
	{

	}

	void Analyzer::GroupB(AnalyzeInfo& info)
	{

	}

	void Analyzer::GroupC(AnalyzeInfo& info)
	{

	}

	void Analyzer::GroupD(AnalyzeInfo& info)
	{

	}

	void Analyzer::GroupE(AnalyzeInfo& info)
	{

	}

	void Analyzer::GroupF(AnalyzeInfo& info)
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
				Group0(instrPtr, instrMaxSize, info);
				break;
			case 1:
				Group1(instrPtr, instrMaxSize, info);
				break;
			case 2:
				Group2(info);
				break;
			case 3:
				Group3(info);
				break;
			case 4:
				Group4(info);
				break;
			case 5:
				Group5(info);
				break;
			case 6:
				Group6(info);
				break;
			case 7:
				Group7(info);
				break;
			case 8:
				Group8(info);
				break;
			case 9:
				Group9(info);
				break;
			case 0xa:
				GroupA(info);
				break;
			case 0xb:
				GroupB(info);
				break;
			case 0xc:
				GroupC(info);
				break;
			case 0xd:
				GroupD(info);
				break;
			case 0xe:
				GroupE(info);
				break;
			case 0xf:
				GroupF(info);
				break;
		}
	}

}
