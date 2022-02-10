// DSP analyzer
#include "pch.h"

namespace DSP
{
	void Decoder::ResetInfo(DecoderInfo& info)
	{
		info.sizeInBytes = 0;

		info.parallel = false;
		info.parallelMemInstr = DspParallelMemInstruction::Unknown;
		info.instr = DspRegularInstruction::Unknown;

		info.numParameters = 0;
		info.numParametersEx = 0;

		info.flowControl = false;
	}

	void Decoder::AddParam(DecoderInfo& info, DspParameter param)
	{
		assert(info.numParameters < DspDecoderNumParam);

		info.params[info.numParameters] = param;
		info.numParameters++;
	}

	void Decoder::AddParamEx(DecoderInfo& info, DspParameter param)
	{
		assert(info.numParametersEx < DspDecoderNumParam);

		info.paramsEx[info.numParametersEx] = param;
		info.numParametersEx++;
	}

	void Decoder::AddImmOperand(DecoderInfo& info, DspParameter param, uint8_t imm)
	{
		AddParam(info, param);
		if (param == DspParameter::Byte)
			info.ImmOperand.Byte = imm;
		else if (param == DspParameter::Byte2)
			info.ImmOperand2.Byte = imm;
	}

	void Decoder::AddImmOperand(DecoderInfo& info, DspParameter param, int8_t imm)
	{
		AddParam(info, param);
		if (param == DspParameter::SignedByte)
			info.ImmOperand.SignedByte = imm;
		else if (param == DspParameter::SignedByte2)
			info.ImmOperand2.SignedByte = imm;
	}

	void Decoder::AddImmOperand(DecoderInfo& info, DspParameter param, uint16_t imm)
	{
		AddParam(info, param);
		if (param == DspParameter::UnsignedShort)
			info.ImmOperand.UnsignedShort = imm;
		else if (param == DspParameter::UnsignedShort2)
			info.ImmOperand2.UnsignedShort = imm;
	}

	void Decoder::AddImmOperand(DecoderInfo& info, DspParameter param, int16_t imm)
	{
		AddParam(info, param);
		if (param == DspParameter::SignedShort)
			info.ImmOperand.SignedShort = imm;
		else if (param == DspParameter::SignedShort2)
			info.ImmOperand2.SignedShort = imm;
	}

	void Decoder::AddImmOperand(DecoderInfo& info, DspParameter param, DspAddress imm)
	{
		AddParam(info, param);
		if (param == DspParameter::Address)
			info.ImmOperand.Address = imm;
		else if (param == DspParameter::Address2)
			info.ImmOperand2.Address = imm;
	}

	void Decoder::AddBytes(uint8_t* instrPtr, size_t bytes, DecoderInfo& info)
	{
		assert(info.sizeInBytes < sizeof(info.bytes));

		memcpy(&info.bytes[info.sizeInBytes], instrPtr, bytes);
		info.sizeInBytes += bytes;
	}

	void Decoder::Group0(uint8_t* instrPtr, size_t instrMaxSize, DecoderInfo& info, uint16_t instrBits)
	{
		static DspParameter addressreg[] = {
			DspParameter::r0,
			DspParameter::r1,
			DspParameter::r2,
			DspParameter::r3,
		};

		static DspParameter topreg[] = {
			DspParameter::x0,
			DspParameter::y0,
			DspParameter::x1,
			DspParameter::y1,
			DspParameter::a0,
			DspParameter::b0,
			DspParameter::a1,
			DspParameter::b1,
		};

		static DspParameter modifier[] = {	// pld
			DspParameter::mod_none,
			DspParameter::mod_dec,
			DspParameter::mod_inc,
			DspParameter::mod_plus_m,
		};

		static DspParameter modifier_n[] = {
			DspParameter::mod_plus_m0,
			DspParameter::mod_plus_m1,
			DspParameter::mod_plus_m2,
			DspParameter::mod_plus_m3,
		};

		switch ((instrBits >> 8) & 0xf)
		{
			//|nop         |0000 0000 0[00]0 0000|
			//|mr rn,mn    |0000 0000 0[00]m mmrr|
			//|trap        |0000 0000 0[01]0 0000|
			//|wait        |0000 0000 0[01]0 0001|
			//|repr reg    |0000 0000 0[10]r rrrr|
			//|loop reg,ea |0000 0000 0[11]r rrrr aaaa aaaa aaaa aaaa|
			//|mvli d,li   |0000 0000 1[00]d dddd iiii iiii iiii iiii|
			//|ldla d,la   |0000 0000 1[10]d dddd aaaa aaaa aaaa aaaa|
			//|stla la,s   |0000 0000 1[11]s ssss aaaa aaaa aaaa aaaa|

			case 0:
				if ((instrBits & 0x80) == 0)
				{
					switch ((instrBits >> 5) & 3)
					{
						case 0:		// nop, mr
						{
							int r = instrBits & 3;
							int m = (instrBits >> 2) & 7;
							if ((r == 0) && (m == 0))
							{
								info.instr = DspRegularInstruction::nop;
							}
							else
							{
								info.instr = DspRegularInstruction::mr;
								AddParam(info, addressreg[r]);
								AddParam(info, (DspParameter)((int)DspParameter::mod_base + m));
							}
							break;
						}
						case 1:		// trap, wait
						{
							info.instr = (instrBits & 1) == 0 ? DspRegularInstruction::trap : DspRegularInstruction::wait;
							info.flowControl = true;
							break;
						}
						case 2:		// rep r
						{
							info.instr = DspRegularInstruction::rep;
							info.flowControl = true;
							int r = instrBits & 0x1f;
							AddParam(info, (DspParameter)((int)DspParameter::regs + r));
							break;
						}
						case 3:		// loop r
						{
							info.instr = DspRegularInstruction::loop;
							info.flowControl = true;
							int r = instrBits & 0x1f;
							AddParam(info, (DspParameter)((int)DspParameter::regs + r));
							DspAddress addr = _BYTESWAP_UINT16(*(uint16_t*)instrPtr);
							AddBytes(instrPtr, sizeof(uint16_t), info);
							AddImmOperand(info, DspParameter::Address, addr);
							break;
						}
					}
				}
				else
				{
					switch ((instrBits >> 5) & 3)
					{
						case 0:		// mvli
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
						case 2:		// ldla
						{
							info.instr = DspRegularInstruction::ldla;
							int r = instrBits & 0x1f;
							DspAddress addr = _BYTESWAP_UINT16(*(uint16_t*)instrPtr);
							AddBytes(instrPtr, sizeof(uint16_t), info);
							AddParam(info, (DspParameter)((int)DspParameter::regs + r));
							AddImmOperand(info, DspParameter::Address, addr);
							break;
						}
						case 3:		// stla
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
				switch ((instrBits >> 4) & 0xf)
				{
					case 0:		// adli, norm, negc + div, max
						switch (instrBits & 0xf)
						{
							case 0:		// adli
							{
								info.instr = DspRegularInstruction::adli;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int16_t imm = _BYTESWAP_UINT16(*(uint16_t*)instrPtr);
								AddBytes(instrPtr, sizeof(uint16_t), info);
								AddImmOperand(info, DspParameter::SignedShort, imm);
								break;
							}
							case 4:		// norm
							case 5:
							case 6:
							case 7:
							{
								info.instr = DspRegularInstruction::norm;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int r = instrBits & 3;
								AddParam(info, addressreg[r]);
								break;
							}
							case 8:		// div
							{
								info.instr = DspRegularInstruction::div;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int s = (instrBits >> 5) & 3;
								AddParam(info, topreg[s]);
								break;
							}
							case 9:		// max
							{
								info.instr = DspRegularInstruction::max;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int s = (instrBits >> 5) & 3;
								AddParam(info, topreg[s]);
								break;
							}
							case 0xd:	// negc
							{
								info.instr = DspRegularInstruction::negc;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								break;
							}
						}
						break;
					case 1:		// pld
					{
						info.instr = DspRegularInstruction::pld;
						int d = (instrBits & 0x100) ? 1 : 0;
						AddParam(info, d == 0 ? DspParameter::a1 : DspParameter::b1);
						int r = instrBits & 3;
						AddParam(info, addressreg[r]);
						int m = (instrBits >> 2) & 3;
						AddParam(info, modifier[m] == DspParameter::mod_plus_m ? modifier_n[r] : modifier[m]);
						break;
					}
					case 2:		// xorli + div, max
						switch (instrBits & 0xf)
						{
							case 0:		// xorli
							{
								info.instr = DspRegularInstruction::xorli;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a1 : DspParameter::b1);
								uint16_t imm = _BYTESWAP_UINT16(*(uint16_t*)instrPtr);
								AddBytes(instrPtr, sizeof(uint16_t), info);
								AddImmOperand(info, DspParameter::UnsignedShort, imm);
								break;
							}
							case 8:		// div
							{
								info.instr = DspRegularInstruction::div;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int s = (instrBits >> 5) & 3;
								AddParam(info, topreg[s]);
								break;
							}
							case 9:		// max
							{
								info.instr = DspRegularInstruction::max;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int s = (instrBits >> 5) & 3;
								AddParam(info, topreg[s]);
								break;
							}
						}
						break;
					case 3:
					case 5:
						// Reserved
						break;

					case 4:		// anli + div, max + lsf, asf
						switch (instrBits & 0xf)
						{
							case 0:		// anli
							{
								info.instr = DspRegularInstruction::anli;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a1 : DspParameter::b1);
								uint16_t imm = _BYTESWAP_UINT16(*(uint16_t*)instrPtr);
								AddBytes(instrPtr, sizeof(uint16_t), info);
								AddImmOperand(info, DspParameter::UnsignedShort, imm);
								break;
							}
							case 8:		// div
							{
								info.instr = DspRegularInstruction::div;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int s = (instrBits >> 5) & 3;
								AddParam(info, topreg[s]);
								break;
							}
							case 9:		// max
							{
								info.instr = DspRegularInstruction::max;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int s = (instrBits >> 5) & 3;
								AddParam(info, topreg[s]);
								break;
							}
							case 0xa:		// lsf
							{
								info.instr = DspRegularInstruction::lsf;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int s = (instrBits & 0x20) ? 1 : 0;
								AddParam(info, s == 0 ? DspParameter::x1 : DspParameter::y1);
								break;
							}
							case 0xb:		// asf
							{
								info.instr = DspRegularInstruction::asf;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int s = (instrBits & 0x20) ? 1 : 0;
								AddParam(info, s == 0 ? DspParameter::x1 : DspParameter::y1);
								break;
							}
						}
						break;
					case 6:		// orli + div, max + lsf, asf
						switch (instrBits & 0xf)
						{
							case 0:		// orli
							{
								info.instr = DspRegularInstruction::orli;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a1 : DspParameter::b1);
								uint16_t imm = _BYTESWAP_UINT16(*(uint16_t*)instrPtr);
								AddBytes(instrPtr, sizeof(uint16_t), info);
								AddImmOperand(info, DspParameter::UnsignedShort, imm);
								break;
							}
							case 8:		// div
							{
								info.instr = DspRegularInstruction::div;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int s = (instrBits >> 5) & 3;
								AddParam(info, topreg[s]);
								break;
							}
							case 9:		// max
							{
								info.instr = DspRegularInstruction::max;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int s = (instrBits >> 5) & 3;
								AddParam(info, topreg[s]);
								break;
							}
							case 0xa:		// lsf
							{
								info.instr = DspRegularInstruction::lsf;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int s = (instrBits & 0x20) ? 1 : 0;
								AddParam(info, s == 0 ? DspParameter::x1 : DspParameter::y1);
								break;
							}
							case 0xb:		// asf
							{
								info.instr = DspRegularInstruction::asf;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int s = (instrBits & 0x20) ? 1 : 0;
								AddParam(info, s == 0 ? DspParameter::x1 : DspParameter::y1);
								break;
							}
						}
						break;

					case 7:		// exec
					{
						info.instr = DspRegularInstruction::exec;
						info.flowControl = true;
						info.cc = (ConditionCode)(instrBits & 0xf);
						break;
					}
					case 8:		// cmpli + addc, subc
						switch (instrBits & 0xf)
						{
							case 0:		// cmpli
							{
								info.instr = DspRegularInstruction::cmpli;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int16_t imm = _BYTESWAP_UINT16(*(uint16_t*)instrPtr);
								AddBytes(instrPtr, sizeof(uint16_t), info);
								AddImmOperand(info, DspParameter::SignedShort, imm);
								break;
							}
							case 0xc:		// addc
							{
								info.instr = DspRegularInstruction::addc;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int s = (instrBits & 0x20) ? 1 : 0;
								AddParam(info, s == 0 ? DspParameter::x : DspParameter::y);
								break;
							}
							case 0xd:		// subc
							{
								info.instr = DspRegularInstruction::subc;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int s = (instrBits & 0x20) ? 1 : 0;
								AddParam(info, s == 0 ? DspParameter::x : DspParameter::y);
								break;
							}
						}
						break;
					case 9:		// jmp
					{
						info.instr = DspRegularInstruction::jmp;
						info.flowControl = true;
						info.cc = (ConditionCode)(instrBits & 0xf);
						DspAddress addr = _BYTESWAP_UINT16(*(uint16_t*)instrPtr);
						AddBytes(instrPtr, sizeof(uint16_t), info);
						AddImmOperand(info, DspParameter::Address, addr);
						break;
					}
					case 0xa:	// btstl + addc, subc
						switch (instrBits & 0xf)
						{
							case 0:		// btstl
							{
								info.instr = DspRegularInstruction::btstl;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a1 : DspParameter::b1);
								uint16_t imm = _BYTESWAP_UINT16(*(uint16_t*)instrPtr);
								AddBytes(instrPtr, sizeof(uint16_t), info);
								AddImmOperand(info, DspParameter::UnsignedShort, imm);
								break;
							}
							case 0xc:		// addc
							{
								info.instr = DspRegularInstruction::addc;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int s = (instrBits & 0x20) ? 1 : 0;
								AddParam(info, s == 0 ? DspParameter::x : DspParameter::y);
								break;
							}
							case 0xd:		// subc
							{
								info.instr = DspRegularInstruction::subc;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								int s = (instrBits & 0x20) ? 1 : 0;
								AddParam(info, s == 0 ? DspParameter::x : DspParameter::y);
								break;
							}
						}
						break;
					case 0xb:	// call
					{
						info.instr = DspRegularInstruction::call;
						info.flowControl = true;
						info.cc = (ConditionCode)(instrBits & 0xf);
						DspAddress addr = _BYTESWAP_UINT16(*(uint16_t*)instrPtr);
						AddBytes(instrPtr, sizeof(uint16_t), info);
						AddImmOperand(info, DspParameter::Address, addr);
						break;
					}
					case 0xc:	// btsth, lsf, asf
						switch (instrBits & 0xf)
						{
							case 0:		// btsth
							{
								info.instr = DspRegularInstruction::btsth;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a1 : DspParameter::b1);
								uint16_t imm = _BYTESWAP_UINT16(*(uint16_t*)instrPtr);
								AddBytes(instrPtr, sizeof(uint16_t), info);
								AddImmOperand(info, DspParameter::UnsignedShort, imm);
								break;
							}
							case 0xa:		// lsf
							{
								info.instr = DspRegularInstruction::lsf;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								AddParam(info, d == 0 ? DspParameter::b1 : DspParameter::a1);
								break;
							}
							case 0xb:		// asf
							{
								info.instr = DspRegularInstruction::asf;
								int d = (instrBits & 0x100) ? 1 : 0;
								AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
								AddParam(info, d == 0 ? DspParameter::b1 : DspParameter::a1);
								break;
							}
						}
						break;
					case 0xd:	// rets
					{
						info.instr = DspRegularInstruction::rets;
						info.flowControl = true;
						info.cc = (ConditionCode)(instrBits & 0xf);
						break;
					}
					case 0xe:
						// Reserved
						break;
					case 0xf:	// reti
					{
						info.instr = DspRegularInstruction::reti;
						info.flowControl = true;
						info.cc = (ConditionCode)(instrBits & 0xf);
						break;
					}
				}
				break;

			//|adsi d,si   |0000 010d iiii iiii|

			case 4:
			case 5:
			{
				info.instr = DspRegularInstruction::adsi;
				int r = (instrBits & 0x100) ? 1 : 0;
				AddParam(info, r == 0 ? DspParameter::a : DspParameter::b);
				AddImmOperand(info, DspParameter::SignedByte, (int8_t)instrBits);
				break;
			}

			//|cmpsi s,si  |0000 011s iiii iiii|

			case 6:
			case 7:
			{
				info.instr = DspRegularInstruction::cmpsi;
				int r = (instrBits & 0x100) ? 1 : 0;
				AddParam(info, r == 0 ? DspParameter::a : DspParameter::b);
				AddImmOperand(info, DspParameter::SignedByte, (int8_t)instrBits);
				break;
			}

			//|mvsi d,si   |0000 1ddd iiii iiii|

			case 8:
			case 9:
			case 0xa:
			case 0xb:
			case 0xc:
			case 0xd:
			case 0xe:
			case 0xf:
			{
				info.instr = DspRegularInstruction::mvsi;
				AddParam(info, topreg[(instrBits >> 8) & 7]);
				AddImmOperand(info, DspParameter::SignedByte, (int8_t)instrBits);
				break;
			}
		}
	}

	void Decoder::Group1(uint8_t* instrPtr, size_t instrMaxSize, DecoderInfo& info, uint16_t instrBits)
	{
		static DspParameter addressreg[] = {
			DspParameter::r0,
			DspParameter::r1,
			DspParameter::r2,
			DspParameter::r3,
		};

		static DspParameter modifier[] = {
			DspParameter::mod_none,
			DspParameter::mod_dec,
			DspParameter::mod_inc,
			DspParameter::mod_plus_m,
		};

		static DspParameter modifier_n[] = {
			DspParameter::mod_plus_m0,
			DspParameter::mod_plus_m1,
			DspParameter::mod_plus_m2,
			DspParameter::mod_plus_m3,
		};

		//|rep rc      |0001 0000 cccc cccc|
		//|loop lc,ea  |0001 0001 cccc cccc aaaa aaaa aaaa aaaa|
		//|clr b       |0001 0010 0000 0bbb|
		//|set b       |0001 0011 0000 0bbb|
		//|lsfi d,si   |0001 010d 0iii iiii|
		//|asfi d,si   |0001 010d 1iii iiii|
		//|stli sa,li  |0001 0110 aaaa aaaa iiii iiii iiii iiii|
		//|jmp(cc) rn  |0001 0111 0rr0 cccc|
		//|call(cc) rn |0001 0111 0rr1 cccc|
		//|ld d,r,m    |0001 100m mrrd dddd|
		//|st r,m,s    |0001 101m mrrs ssss|
		//|mv d,s      |0001 11dd ddds ssss|

		switch ((instrBits >> 8) & 0xf)
		{
			case 0:		// rep
			{
				info.instr = DspRegularInstruction::rep;
				info.flowControl = true;
				AddImmOperand(info, DspParameter::Byte, (uint8_t)instrBits);
				break;
			}
			case 1:		// loop
			{
				info.instr = DspRegularInstruction::loop;
				info.flowControl = true;
				AddImmOperand(info, DspParameter::Byte, (uint8_t)instrBits);
				DspAddress addr = _BYTESWAP_UINT16(*(uint16_t*)instrPtr);
				AddBytes(instrPtr, sizeof(uint16_t), info);
				AddImmOperand(info, DspParameter::Address2, addr);
				break;
			}
			case 2:		// clr
			{
				int b = instrBits & 7;
				if (b != 7)
				{
					info.instr = DspRegularInstruction::clr;
					AddParam(info, (DspParameter)((int)DspParameter::psr_tb + b));
				}
				break;
			}
			case 3:		// set
			{
				int b = instrBits & 7;
				if (b != 7)
				{
					info.instr = DspRegularInstruction::set;
					AddParam(info, (DspParameter)((int)DspParameter::psr_tb + b));
				}
				break;
			}
			case 4:		// lsfi, asfi
			case 5:
			{
				int d = (instrBits & 0x100) ? 1 : 0;

				int8_t si = instrBits & 0x7f;
				if (si & 0x40)
				{
					si |= 0x80;
				}

				info.instr = (instrBits & 0x80) == 0 ? DspRegularInstruction::lsfi : DspRegularInstruction::asfi;
				AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
				AddImmOperand(info, DspParameter::SignedByte, si);
				break;
			}
			case 6:		// stli
			{
				info.instr = DspRegularInstruction::stli;
				DspAddress addr = 0xff00 | (instrBits & 0xff);
				AddImmOperand(info, DspParameter::Address, addr);
				uint16_t imm = _BYTESWAP_UINT16(*(uint16_t*)instrPtr);
				AddBytes(instrPtr, sizeof(uint16_t), info);
				AddImmOperand(info, DspParameter::UnsignedShort2, imm);
				break;
			}
			case 7:		// jmp, call
			{
				info.instr = (instrBits & 0x10) == 0 ? DspRegularInstruction::jmp : DspRegularInstruction::call;
				info.flowControl = true;
				info.cc = (ConditionCode)(instrBits & 0xf);
				int r = (instrBits >> 5) & 3;
				AddParam(info, addressreg[r]);
				break;
			}
			case 8:		// ld
			case 9:
			{
				info.instr = DspRegularInstruction::ld;
				int d = instrBits & 0x1f;
				AddParam(info, (DspParameter)((int)DspParameter::regs + d));
				int r = (instrBits >> 5) & 3;
				AddParam(info, addressreg[r]);
				int m = (instrBits >> 7) & 3;
				AddParam(info, modifier[m] == DspParameter::mod_plus_m ? modifier_n[r] : modifier[m]);
				break;
			}
			case 0xa:	// st
			case 0xb:
			{
				info.instr = DspRegularInstruction::st;
				int r = (instrBits >> 5) & 3;
				AddParam(info, addressreg[r]);
				int m = (instrBits >> 7) & 3;
				AddParam(info, modifier[m] == DspParameter::mod_plus_m ? modifier_n[r] : modifier[m]);
				int s = instrBits & 0x1f;
				AddParam(info, (DspParameter)((int)DspParameter::regs + s));
				break;
			}
			case 0xc:	// mv
			case 0xd:
			case 0xe:
			case 0xf:
			{
				info.instr = DspRegularInstruction::mv;
				int d = (instrBits >> 5) & 0x1f;
				AddParam(info, (DspParameter)((int)DspParameter::regs + d));
				int s = instrBits & 0x1f;
				AddParam(info, (DspParameter)((int)DspParameter::regs + s));
				break;
			}
		}
	}

	void Decoder::Group2(DecoderInfo& info, uint16_t instrBits)
	{
		static DspParameter ldsa_reg[] = {
			DspParameter::x0,
			DspParameter::y0,
			DspParameter::x1,
			DspParameter::y1,
			DspParameter::a0,
			DspParameter::b0,
			DspParameter::a1,
			DspParameter::b1,
		};

		static DspParameter stsa_reg[] = {
			DspParameter::a2,
			DspParameter::b2,
			DspParameter::Unknown,
			DspParameter::Unknown,
			DspParameter::a0,
			DspParameter::b0,
			DspParameter::a1,
			DspParameter::b1,
		};

		//|ldsa d,sa   |0010 0ddd aaaa aaaa|
		//|stsa sa,s   |0010 1sss aaaa aaaa|

		if ((instrBits & 0x800) == 0)
		{
			// ldsa
			info.instr = DspRegularInstruction::ldsa;
			int d = (instrBits >> 8) & 7;
			AddParam(info, ldsa_reg[d]);
			AddImmOperand(info, DspParameter::Address, (DspAddress)(uint8_t)instrBits);
		}
		else
		{
			// stsa
			info.instr = DspRegularInstruction::stsa;
			AddImmOperand(info, DspParameter::Address, (DspAddress)(uint8_t)instrBits);
			int s = (instrBits >> 8) & 7;
			AddParam(info, stsa_reg[s]);
		}
	}

	void Decoder::Group3(DecoderInfo& info, uint16_t instrBits)
	{
		//|xor d,s      |0011 00sd 0xxx xxxx|
		//|xor d,s      |0011 000d 1xxx xxxx|
		//|not d        |0011 001d 1xxx xxxx|
		//|and d,s      |0011 01sd 0xxx xxxx|
		//|lsf d,s      |0011 01sd 1xxx xxxx|
		//|or d,s       |0011 10sd 0xxx xxxx|
		//|asf d,s      |0011 10sd 1xxx xxxx|
		//|and d,s      |0011 110d 0xxx xxxx|
		//|lsf d,s      |0011 110d 1xxx xxxx|
		//|or d,s       |0011 111d 0xxx xxxx|
		//|asf d,s      |0011 111d 1xxx xxxx|

		bool msb = (instrBits & 0x80) ? true : false;

		switch ((instrBits >> 9) & 7)
		{
			case 0:		// xor
			{
				info.parallelInstr = DspParallelInstruction::_xor;
				if (!msb)
				{
					int s = (instrBits >> 9) & 1;
					int d = (instrBits >> 8) & 1;
					AddParam(info, d == 0 ? DspParameter::a1 : DspParameter::b1);
					AddParam(info, s == 0 ? DspParameter::x1 : DspParameter::y1);
				}
				else
				{
					int d = (instrBits >> 8) & 1;
					AddParam(info, d == 0 ? DspParameter::a1 : DspParameter::b1);
					AddParam(info, d == 0 ? DspParameter::b1 : DspParameter::a1);
				}
				break;
			}
			case 1:		// not + xor
			{
				if (!msb)
				{
					// xor
					info.parallelInstr = DspParallelInstruction::_xor;
					int s = (instrBits >> 9) & 1;
					int d = (instrBits >> 8) & 1;
					AddParam(info, d == 0 ? DspParameter::a1 : DspParameter::b1);
					AddParam(info, s == 0 ? DspParameter::x1 : DspParameter::y1);
				}
				else
				{
					// not
					info.parallelInstr = DspParallelInstruction::_not;
					int d = (instrBits >> 8) & 1;
					AddParam(info, d == 0 ? DspParameter::a1 : DspParameter::b1);
				}
				break;
			}
			case 2:		// and, lsf
			case 3:
			{
				if (!msb)
				{
					// and
					info.parallelInstr = DspParallelInstruction::_and;
					int s = (instrBits >> 9) & 1;
					int d = (instrBits >> 8) & 1;
					AddParam(info, d == 0 ? DspParameter::a1 : DspParameter::b1);
					AddParam(info, s == 0 ? DspParameter::x1 : DspParameter::y1);
				}
				else
				{
					// lsf
					info.parallelInstr = DspParallelInstruction::lsf;
					int s = (instrBits >> 9) & 1;
					int d = (instrBits >> 8) & 1;
					AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
					AddParam(info, s == 0 ? DspParameter::x1 : DspParameter::y1);
				}
				break;
			}
			case 4:		// or, asf
			case 5:
			{
				if (!msb)
				{
					// or
					info.parallelInstr = DspParallelInstruction::_or;
					int s = (instrBits >> 9) & 1;
					int d = (instrBits >> 8) & 1;
					AddParam(info, d == 0 ? DspParameter::a1 : DspParameter::b1);
					AddParam(info, s == 0 ? DspParameter::x1 : DspParameter::y1);
				}
				else
				{
					// asf
					info.parallelInstr = DspParallelInstruction::asf;
					int s = (instrBits >> 9) & 1;
					int d = (instrBits >> 8) & 1;
					AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
					AddParam(info, s == 0 ? DspParameter::x1 : DspParameter::y1);
				}
				break;
			}
			case 6:		// and, lsf
			{
				if (!msb)
				{
					// and
					info.parallelInstr = DspParallelInstruction::_and;
					int d = (instrBits >> 8) & 1;
					AddParam(info, d == 0 ? DspParameter::a1 : DspParameter::b1);
					AddParam(info, d == 0 ? DspParameter::b1 : DspParameter::a1);
				}
				else
				{
					// lsf
					info.parallelInstr = DspParallelInstruction::lsf;
					int d = (instrBits >> 8) & 1;
					AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
					AddParam(info, d == 0 ? DspParameter::x1 : DspParameter::y1);
				}
				break;
			}
			case 7:		// or, asf
			{
				if (!msb)
				{
					// or
					info.parallelInstr = DspParallelInstruction::_or;
					int d = (instrBits >> 8) & 1;
					AddParam(info, d == 0 ? DspParameter::a1 : DspParameter::b1);
					AddParam(info, d == 0 ? DspParameter::b1 : DspParameter::a1);
				}
				else
				{
					// asf
					info.parallelInstr = DspParallelInstruction::asf;
					int d = (instrBits >> 8) & 1;
					AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
					AddParam(info, d == 0 ? DspParameter::x1 : DspParameter::y1);
				}
				break;
			}
		}

		GroupMemOps3(info, instrBits);
	}

	void Decoder::Group4_6(DecoderInfo& info, uint16_t instrBits)
	{
		//|add d,s      |0100 sssd xxxx xxxx|
		//|sub d,s      |0101 sssd xxxx xxxx|
		//|amv d,s      |0110 sssd xxxx xxxx|

		static DspParameter srcreg[] = {
			DspParameter::x0,
			DspParameter::y0,
			DspParameter::x1,
			DspParameter::y1,
			DspParameter::x,
			DspParameter::y,
			DspParameter::a,
			DspParameter::prod,
		};

		switch (instrBits >> 12)
		{
			case 4:
				info.parallelInstr = DspParallelInstruction::add;
				break;
			case 5:
				info.parallelInstr = DspParallelInstruction::sub;
				break;
			case 6:
				info.parallelInstr = DspParallelInstruction::amv;
				break;
		}

		int d = (instrBits & 0x100) ? 1 : 0;
		int s = (instrBits >> 9) & 7;

		AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
		AddParam(info, srcreg[s] == DspParameter::a ? (d == 0 ? DspParameter::b : DspParameter::a) : srcreg[s]);

		GroupMemOps4_F(info, instrBits);
	}

	void Decoder::Group7(DecoderInfo& info, uint16_t instrBits)
	{
		//|addl d,s     |0111 00sd xxxx xxxx|
		//|inc d        |0111 01dd xxxx xxxx|
		//|dec d        |0111 10dd xxxx xxxx|
		//|neg d        |0111 110d xxxx xxxx|
		//|neg d,p      |0111 111d xxxx xxxx|

		static DspParameter increg[] = {
			DspParameter::a1,
			DspParameter::b1,
			DspParameter::a,
			DspParameter::b
		};

		int s = (instrBits & 0x200) ? 1 : 0;
		int d = (instrBits & 0x100) ? 1 : 0;
		int dd = (instrBits >> 8) & 3;

		switch ((instrBits >> 9) & 7)
		{
			case 0:
			case 1:
				info.parallelInstr = DspParallelInstruction::addl;
				AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
				AddParam(info, s == 0 ? DspParameter::x0 : DspParameter::y0);
				break;
			case 2:
			case 3:
				info.parallelInstr = DspParallelInstruction::inc;
				AddParam(info, increg[dd]);
				break;
			case 4:
			case 5:
				info.parallelInstr = DspParallelInstruction::dec;
				AddParam(info, increg[dd]);
				break;
			case 6:
				info.parallelInstr = DspParallelInstruction::neg;
				AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
				break;
			case 7:
				info.parallelInstr = DspParallelInstruction::neg;
				AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
				AddParam(info, DspParameter::prod);
				break;
		}

		GroupMemOps4_F(info, instrBits);
	}

	void Decoder::Group8(DecoderInfo& info, uint16_t instrBits)
	{
		//|nop          |1000 0000 xxxx xxxx|
		//|clr d        |1000 d001 xxxx xxxx|
		//|cmp a,b      |1000 0010 xxxx xxxx|
		//|mpy x1,x1    |1000 0011 xxxx xxxx|
		//|clr p        |1000 0100 xxxx xxxx|
		//|tst p        |1000 0101 xxxx xxxx|
		//|tst s        |1000 011s xxxx xxxx|
		//|clr im       |1000 1010 xxxx xxxx|
		//|set im       |1000 1011 xxxx xxxx|
		//|clr dp       |1000 1100 xxxx xxxx|
		//|set dp       |1000 1101 xxxx xxxx|
		//|clr xl       |1000 1110 xxxx xxxx|
		//|set xl       |1000 1111 xxxx xxxx|

		switch ((instrBits >> 8) & 0xf)
		{
			case 0:		// nop
				info.parallelInstr = DspParallelInstruction::nop;
				break;
			case 1:		// clr d
			case 9:
			{
				int d = (instrBits >> 11) & 1;
				info.parallelInstr = DspParallelInstruction::clr;
				AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
				break;
			}
			case 2:		// cmp a,b
				info.parallelInstr = DspParallelInstruction::cmp;
				AddParam(info, DspParameter::a);
				AddParam(info, DspParameter::b);
				break;
			case 3:		// mpy x1,x1
				info.parallelInstr = DspParallelInstruction::mpy;
				AddParam(info, DspParameter::x1);
				AddParam(info, DspParameter::x1);
				break;
			case 4:		// clr p
				info.parallelInstr = DspParallelInstruction::clr;
				AddParam(info, DspParameter::prod);
				break;
			case 5:		// tst p
				info.parallelInstr = DspParallelInstruction::tst;
				AddParam(info, DspParameter::prod);
				break;
			case 6:		// tst s (Form 2)
			case 7:
			{
				int s = (instrBits >> 8) & 1;
				info.parallelInstr = DspParallelInstruction::tst;
				AddParam(info, s == 0 ? DspParameter::x1 : DspParameter::y1);
				break;
			}
			case 8:
				// Reserved
				break;
			case 0xa:	// clr im
				info.parallelInstr = DspParallelInstruction::clr;
				AddParam(info, DspParameter::psr_im);
				break;
			case 0xb:	// set im
				info.parallelInstr = DspParallelInstruction::set;
				AddParam(info, DspParameter::psr_im);
				break;
			case 0xc:	// clr dp
				info.parallelInstr = DspParallelInstruction::clr;
				AddParam(info, DspParameter::psr_dp);
				break;
			case 0xd:	// set dp
				info.parallelInstr = DspParallelInstruction::set;
				AddParam(info, DspParameter::psr_dp);
				break;
			case 0xe:	// clr xl
				info.parallelInstr = DspParallelInstruction::clr;
				AddParam(info, DspParameter::psr_xl);
				break;
			case 0xf:	// set xl
				info.parallelInstr = DspParallelInstruction::set;
				AddParam(info, DspParameter::psr_xl);
				break;
		}

		GroupMemOps4_F(info, instrBits);
	}

	void Decoder::Group9_B(DecoderInfo& info, uint16_t instrBits)
	{
		//|asr16 d      |1001 d001 xxxx xxxx|
		//|abs d        |1010 d001 xxxx xxxx|
		//|tst s        |1011 s001 xxxx xxxx|

		if (((instrBits >> 8) & 7) == 1)
		{
			switch (instrBits >> 12)
			{
				case 9:
					info.parallelInstr = DspParallelInstruction::asr16;
					break;
				case 0xa:
					info.parallelInstr = DspParallelInstruction::abs;
					break;
				case 0xb:
					info.parallelInstr = DspParallelInstruction::tst;
					break;
			}

			int d = (instrBits & 0x800) ? 1 : 0;
			AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
		}

		GroupMemOps4_F(info, instrBits);
	}

	void Decoder::GroupCD(DecoderInfo& info, uint16_t instrBits)
	{
		//|cmp d,s      |110s d001 xxxx xxxx|

		if (((instrBits >> 8) & 7) == 1)
		{
			info.parallelInstr = DspParallelInstruction::cmp;

			int s = (instrBits & 0x1000) ? 1 : 0;
			int d = (instrBits & 0x800) ? 1 : 0;

			AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
			AddParam(info, s == 0 ? DspParameter::x1 : DspParameter::y1);
		}

		GroupMemOps4_F(info, instrBits);
	}

	void Decoder::GroupE(DecoderInfo& info, uint16_t instrBits)
	{
		static DspParameter form1_regs[][2] = {
			{ DspParameter::x0, DspParameter::y0 },
			{ DspParameter::x0, DspParameter::y1 },
			{ DspParameter::x1, DspParameter::y0 },
			{ DspParameter::x1, DspParameter::y1 },
		};

		static DspParameter form2_regs[][2] = {
			{ DspParameter::a1, DspParameter::x1 },
			{ DspParameter::a1, DspParameter::y1 },
			{ DspParameter::b1, DspParameter::x1 },
			{ DspParameter::b1, DspParameter::y1 },
		};

		//|mac s1,s2    |1110 00ss xxxx xxxx|
		//|mac s1,s2    |1110 10ss xxxx xxxx|
		//|macn s1,s2   |1110 01ss xxxx xxxx|
		//|macn s1,s2   |1110 11ss xxxx xxxx|

		info.parallelInstr = (instrBits & 0x400) == 0 ? DspParallelInstruction::mac : DspParallelInstruction::macn;

		int ss = (instrBits >> 8) & 3;
		bool form1 = (instrBits & 0x800) == 0;

		AddParam(info, form1 ? form1_regs[ss][0] : form2_regs[ss][0]);
		AddParam(info, form1 ? form1_regs[ss][1] : form2_regs[ss][1]);

		GroupMemOps4_F(info, instrBits);
	}

	void Decoder::GroupF(DecoderInfo& info, uint16_t instrBits)
	{
		//|lsl16 d      |1111 000d xxxx xxxx|
		//|mac s1,s2    |1111 001s xxxx xxxx|
		//|lsr16 d      |1111 010d xxxx xxxx|
		//|macn s1,s2   |1111 011s xxxx xxxx|
		//|addp d,s     |1111 10sd xxxx xxxx|
		//|rnd d        |1111 110d xxxx xxxx|
		//|rndp d       |1111 111d xxxx xxxx|

		int s = (instrBits & 0x200) ? 1 : 0;
		int d = (instrBits & 0x100) ? 1 : 0;

		switch ((instrBits >> 9) & 7)
		{
			case 0:
				info.parallelInstr = DspParallelInstruction::lsl16;
				AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
				break;
			case 1:
				info.parallelInstr = DspParallelInstruction::mac;
				AddParam(info, d == 0 ? DspParameter::x1 : DspParameter::y1);
				AddParam(info, d == 0 ? DspParameter::x0 : DspParameter::y0);
				break;
			case 2:
				info.parallelInstr = DspParallelInstruction::lsr16;
				AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
				break;
			case 3:
				info.parallelInstr = DspParallelInstruction::macn;
				AddParam(info, d == 0 ? DspParameter::x1 : DspParameter::y1);
				AddParam(info, d == 0 ? DspParameter::x0 : DspParameter::y0);
				break;
			case 4:
			case 5:
				info.parallelInstr = DspParallelInstruction::addp;
				AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
				AddParam(info, s == 0 ? DspParameter::x1 : DspParameter::y1);
				break;
			case 6:
				info.parallelInstr = DspParallelInstruction::rnd;
				AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
				break;
			case 7:
				info.parallelInstr = DspParallelInstruction::rndp;
				AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
				break;
		}

		GroupMemOps4_F(info, instrBits);
	}

	void Decoder::GroupMpy(DecoderInfo& info, uint16_t instrBits)
	{
		static DspParameter mpy_regs[][2] = {
			{ DspParameter::Unknown, DspParameter::Unknown },
			{ DspParameter::Unknown, DspParameter::Unknown },
			{ DspParameter::x1, DspParameter::x0 },
			{ DspParameter::y1, DspParameter::y0 },
			{ DspParameter::x0, DspParameter::y0 },
			{ DspParameter::x0, DspParameter::y1 },
			{ DspParameter::x1, DspParameter::y0 },
			{ DspParameter::x1, DspParameter::y1 },
			{ DspParameter::a1, DspParameter::x1 },
			{ DspParameter::a1, DspParameter::y1 },
			{ DspParameter::b1, DspParameter::x1 },
			{ DspParameter::b1, DspParameter::y1 },
			{ DspParameter::Unknown, DspParameter::Unknown },
			{ DspParameter::Unknown, DspParameter::Unknown },
			{ DspParameter::Unknown, DspParameter::Unknown },
			{ DspParameter::Unknown, DspParameter::Unknown },
		};

		//|mpy s1,s2    |1sss s000 xxxx xxxx|
		//|rnmpy d,s1,s2|1sss s01d xxxx xxxx|
		//|admpy d,s1,s2|1sss s10d xxxx xxxx|
		//|mvmpy d,s1,s2|1sss s11d xxxx xxxx|

		int d = (instrBits & 0x100) ? 1 : 0;
		int s = (instrBits >> 11) & 0xf;

		switch ((instrBits >> 9) & 3)
		{
			case 0:
				if (d == 0)
				{
					info.parallelInstr = DspParallelInstruction::mpy;
					AddParam(info, mpy_regs[s][0]);
					AddParam(info, mpy_regs[s][1]);
				}
				break;
			case 1:
				info.parallelInstr = DspParallelInstruction::rnmpy;
				AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
				AddParam(info, mpy_regs[s][0]);
				AddParam(info, mpy_regs[s][1]);
				break;
			case 2:
				info.parallelInstr = DspParallelInstruction::admpy;
				AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
				AddParam(info, mpy_regs[s][0]);
				AddParam(info, mpy_regs[s][1]);
				break;
			case 3:
				info.parallelInstr = DspParallelInstruction::mvmpy;
				AddParam(info, d == 0 ? DspParameter::a : DspParameter::b);
				AddParam(info, mpy_regs[s][0]);
				AddParam(info, mpy_regs[s][1]);
				break;
		}

		GroupMemOps4_F(info, instrBits);
	}

	void Decoder::GroupMemOps3(DecoderInfo& info, uint16_t instrBits)
	{
		static DspParameter addressreg[] = {
			DspParameter::r0,
			DspParameter::r1,
			DspParameter::r2,
			DspParameter::r3,
		};

		static DspParameter modifier[] = {
			DspParameter::mod_none,
			DspParameter::mod_dec,
			DspParameter::mod_inc,
			DspParameter::mod_plus_m,
		};

		static DspParameter modifier_n[] = {
			DspParameter::mod_plus_m0,
			DspParameter::mod_plus_m1,
			DspParameter::mod_plus_m2,
			DspParameter::mod_plus_m3,
		};

		static DspParameter dstreg[] = {
			DspParameter::x0,
			DspParameter::y0,
			DspParameter::x1,
			DspParameter::y1,
		};

		static DspParameter srcreg[] = {
			DspParameter::a0,
			DspParameter::b0,
			DspParameter::a1,
			DspParameter::b1,
		};

		static DspParameter topreg[] = {
			DspParameter::x0,
			DspParameter::y0,
			DspParameter::x1,
			DspParameter::y1,
			DspParameter::a0,
			DspParameter::b0,
			DspParameter::a1,
			DspParameter::b1,
		};

		//|mr rn,mn               |0011 xxxx x000 mmrr|
		//|mv d,s                 |0011 xxxx x001 ddss|
		//|st rn,mn,s             |0011 xxxx x01s smrr|
		//|ld d,rn,mn             |0011 xxxx x1dd dmrr|

		switch ((instrBits >> 4) & 7)
		{
			case 0:		// mr
			{
				int r = instrBits & 3;
				int m = (instrBits >> 2) & 3;

				if (r == 0 && m == 0)
				{
					info.parallelMemInstr = DspParallelMemInstruction::nop;
				}
				else
				{
					info.parallelMemInstr = DspParallelMemInstruction::mr;
					AddParamEx(info, addressreg[r]);
					AddParamEx(info, modifier[m] == DspParameter::mod_plus_m ? modifier_n[r] : modifier[m]);
				}
				break;
			}
			case 1:		// mv
			{
				info.parallelMemInstr = DspParallelMemInstruction::mv;
				int d = (instrBits >> 2) & 3;
				AddParamEx(info, dstreg[d]);
				int s = instrBits & 3;
				AddParamEx(info, srcreg[s]);
				break;
			}
			case 2:		// st
			case 3:
			{
				info.parallelMemInstr = DspParallelMemInstruction::st;
				int r = instrBits & 3;
				AddParamEx(info, addressreg[r]);
				int m = (instrBits & 4) != 0 ? 1 : 0;
				AddParamEx(info, m == 0 ? DspParameter::mod_inc : modifier_n[r]);
				int s = (instrBits >> 3) & 3;
				AddParamEx(info, srcreg[s]);
				break;
			}
			case 4:		// ld
			case 5:
			case 6:
			case 7:
			{
				info.parallelMemInstr = DspParallelMemInstruction::ld;
				int d = (instrBits >> 3) & 7;
				AddParamEx(info, topreg[d]);
				int r = instrBits & 3;
				AddParamEx(info, addressreg[r]);
				int m = (instrBits & 4) != 0 ? 1 : 0;
				AddParamEx(info, m == 0 ? DspParameter::mod_inc : modifier_n[r]);
				break;
			}
		}
	}

	void Decoder::GroupMemOps4_F(DecoderInfo& info, uint16_t instrBits)
	{
		static DspParameter addressreg[] = {
			DspParameter::r0,
			DspParameter::r1,
			DspParameter::r2,
			DspParameter::r3,
		};

		static DspParameter modifier[] = {
			DspParameter::mod_plus_m0,
			DspParameter::mod_plus_m1,
			DspParameter::mod_plus_m2,
			DspParameter::mod_plus_m3,
		};

		static DspParameter dstreg[] = {
			DspParameter::x0,
			DspParameter::y0,
			DspParameter::x1,
			DspParameter::y1,
		};

		static DspParameter ldd_regs[][2] = {
			{ DspParameter::x0, DspParameter::y0 },
			{ DspParameter::x0, DspParameter::y1 },
			{ DspParameter::x1, DspParameter::y0 },
			{ DspParameter::x1, DspParameter::y1 },
		};

		if (instrBits & 0x80)
		{
			//|ls d,r,m r,m,s         |01xx xxxx 10dd mn0s|
			//|ls d,r,m r,m,s         |1xxx xxxx 10dd mn0s|
			//|ls2 d,r,m r,m,s        |01xx xxxx 10dd mn1s|
			//|ls2 d,r,m r,m,s        |1xxx xxxx 10dd mn1s|

			if ((instrBits & 0x40) == 0)
			{
				int d = (instrBits >> 4) & 3;
				int m = (instrBits & 8) ? 1 : 0;
				int n = (instrBits & 4) ? 1 : 0;
				int s = instrBits & 1;

				info.parallelMemInstr = DspParallelMemInstruction::ls;

				if ((instrBits & 2) == 0)		// ls
				{
					// Load by r0, store by r3
					// d, r0, n, r3, m, s

					AddParamEx(info, dstreg[d]);
					AddParamEx(info, DspParameter::r0);
					AddParamEx(info, n == 0 ? DspParameter::mod_inc : DspParameter::mod_plus_m0);

					AddParamEx(info, DspParameter::r3);
					AddParamEx(info, m == 0 ? DspParameter::mod_inc : DspParameter::mod_plus_m3);
					AddParamEx(info, s == 0 ? DspParameter::a1 : DspParameter::b1);
				}
				else		// ls2
				{
					// Load by r3, store by r0
					// d, r3, m, r0, n, s

					AddParamEx(info, dstreg[d]);
					AddParamEx(info, DspParameter::r3);
					AddParamEx(info, m == 0 ? DspParameter::mod_inc : DspParameter::mod_plus_m3);

					AddParamEx(info, DspParameter::r0);
					AddParamEx(info, n == 0 ? DspParameter::mod_inc : DspParameter::mod_plus_m0);
					AddParamEx(info, s == 0 ? DspParameter::a1 : DspParameter::b1);
				}
			}
			else
			{
				//|ldd d1,rn,mn d2,r3,m3|1xxx xxxx 11dd mnrr|-|-|-|-|-|-|Load dual data (Form 1a)|1|
				//|ldd d1,rn,mn d2,r3,m3|01xx xxxx 11dd mnrr|-|-|-|-|-|-|Load dual data (Form 1b)|1|
				//|ldd2 d1,rn,mn d2,r3,m3|1xxx xxxx 11rd mn11|-|-|-|-|-|-|Load dual data (Form 2)|1|

				int d = (instrBits >> 4) & 3;
				int m = (instrBits & 8) ? 1 : 0;
				int n = (instrBits & 4) ? 1 : 0;
				int r = instrBits & 3;

				info.parallelMemInstr = DspParallelMemInstruction::ldd;

				if (r == 3 && (instrBits & 0x8000) != 0)
				{
					// ldd2

					r = (instrBits & 0x20) ? 1 : 0;
					d = (instrBits & 0x10) ? 1 : 0;

					AddParamEx(info, d == 0 ? DspParameter::x1 : DspParameter::y1);
					AddParamEx(info, addressreg[r]);
					AddParamEx(info, n == 0 ? DspParameter::mod_inc : modifier[r]);
					AddParamEx(info, d == 0 ? DspParameter::x0 : DspParameter::y0);
					AddParamEx(info, DspParameter::r3);
					AddParamEx(info, m == 0 ? DspParameter::mod_inc : DspParameter::mod_plus_m3);
				}
				else
				{
					// ldd

					AddParamEx(info, ldd_regs[d][0]);
					AddParamEx(info, addressreg[r]);
					AddParamEx(info, n == 0 ? DspParameter::mod_inc : modifier[r]);
					AddParamEx(info, ldd_regs[d][1]);
					AddParamEx(info, DspParameter::r3);
					AddParamEx(info, m == 0 ? DspParameter::mod_inc : DspParameter::mod_plus_m3);
				}
			}
		}
		else
		{
			//|mr rn,mn               |01xx xxxx 0000 mmrr|
			//|mv d,s                 |01xx xxxx 0001 ddss|
			//|st rn,mn,s             |01xx xxxx 001s smrr|
			//|ld d,rn,mn             |01xx xxxx 01dd dmrr|

			GroupMemOps3(info, instrBits);
		}
	}

	void Decoder::Decode(uint8_t* instrPtr, size_t instrMaxSize, DecoderInfo& info)
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

		// Group mixed multiply (8-F)?

		if ((instrBits & 0x8000) != 0)
		{
			int s = (instrBits >> 11) & 0xf;

			if (s >= 2 && s <= 0xb)
			{
				switch ((instrBits >> 8) & 7)
				{
					case 0:
					case 2:
					case 3:
					case 4:
					case 5:
					case 6:
					case 7:
						GroupMpy(info, instrBits);
						return;
				}
			}
		}

		// Other

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
			case 5:
			case 6:
				Group4_6(info, instrBits);
				break;
			case 7:
				Group7(info, instrBits);
				break;
			case 8:
				Group8(info, instrBits);
				break;
			case 9:
			case 0xa:
			case 0xb:
				Group9_B(info, instrBits);
				break;
			case 0xc:
			case 0xd:
				GroupCD(info, instrBits);
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
