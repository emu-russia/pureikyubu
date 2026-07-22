#include "pch.h"

// The real DSP is designed as follows: [DSP PI Interface] <-> [DSPCore] <-> [ARAM Controller/Accelerator]
// The DSPCore includes the actual DSP instruction execution core ("Engine"), as well as the IMEM/DMEM and the register decoder (0xff00...)
// DSP PI I/F deals with PI communication (interrupts, Mailbox) and also contains 3 DMA controllers: DSP IMEM/DMEM DMA, AI DMA, ARAM DMA
// ARAM(SDRAM) Controller controls access to external SDRAM memory (ARAM) and also contains the "Accelerator"

using namespace Debug;

namespace DSP
{
	const char* DspJdi = R"json(
{
	"info":
	{
		"description": "DSP JDI",
		"helpGroup": "DSP Debug Commands"
	},

	"can": {
		"dspdisa": {
			"help": "Disassemble DSP code into text file",
			"args": 1,
			"usage": [
				"Syntax: dspdisa <dsp_ucode.bin> [start_addr]\n",
				"disassemble dsp ucode from binary file and dump it into dspdisa.txt\n",
				"start_addr in DSP slots;\n",
				"Example of use: dspdisa Data/dsp_irom.bin 0x8000\n"
			]
		},

		"dregs": {
			"help": "Show DSP registers",
			"output": "Array [] of all DSP registers contents"
		},

		"dreg": {
			"help": "Modify DSP register",
			"hints": "<reg> <value>",
			"args": 2,
			"usage": [
				"Syntax: dreg <register> <value>\n",
				"Register names: ar0 ar1 ar2 ar3 ix0 ix1 ix2 ix3",
				"r8 r9 r10 r11 st0 st1 st2 st3",
				"ac0h ac1h config sr prodl prodm1 prodh prodm2",
				"ax0l ax0h ax1l ax1h ac0l ac1l ac0m ac1m",
				"Example of use: dreg ar0 0x300\n"
			]
		},

		"dmem": {
			"help": "Dump DSP DMEM",
			"args": 1,
			"usage": [
				"Syntax: dmem <dsp_addr>, dmem .\n",
				"Dump 32 bytes of DMEM at dsp_addr. dsp_addr in halfword DSP slots.\n",
				"dmem . will dump 0x800 bytes at dmem address 0\n",
				"Example of use: dmem 0x8000\n"
			],
			"output": "Array [] of memory data"
		},

		"imem": {
			"help": "Dump DSP IMEM",
			"args": 1,
			"usage": [
				"Syntax: imem <dsp_addr>, imem .\n",
				"Dump 32 bytes of IMEM at dsp_addr. dsp_addr in halfword DSP slots.\n",
				"imem . will dump 32 bytes of imem at program counter address.\n",
				"Example of use: imem 0\n"
			],
			"output": "Array [] of memory data"
		},

		"drun": {
			"help": "Run DSP thread until break, halt or dstop"
		},

		"dstop": {
			"help": "Stop DSP thread"
		},

		"dstep": {
			"help": "Step DSP instruction(s)",
			"hints": "[n]"
		},

		"dbrk": {
			"help": "Add IMEM breakpoint",
			"args": 1,
			"usage": [
				"Syntax: dbrk <dsp_addr>\n",
				"Add breakpoint at dsp_addr. dsp_addr in halfword DSP slots.\n",
				"Example of use: dbrk 0x8020\n"
			]
		},

		"dcan": {
			"help": "Add IMEM canary",
			"args": 2,
			"usage": [
				"Syntax: dcan <dsp_addr> <message>\n",
				"Add canary at dsp_addr. dsp_addr in halfword DSP slots.\n",
				"When the PC is equal to the canary address, a debug message is displayed\n",
				"Example of use: dcan 0x10 \"Ucode entrypoint\"\n"
			]
		},

		"dlist": {
			"help": "List IMEM breakpoints / canaries"
		},

		"dbrkclr": {
			"help": "Clear all IMEM breakpoints"
		},

		"dcanclr": {
			"help": "Clear all IMEM canaries"
		},

		"dpc": {
			"help": "Set DSP program counter",
			"args": 1,
			"usage": [
				"Syntax: dpc <dsp_addr>\n",
				"Set DSP program counter to dsp_addr. dsp_addr in halfword DSP slots.\n",
				"Example of use: dpc 0x8000\n"
			]
		},

		"dreset": {
			"help": "Issue DSP reset"
		},

		"du": {
			"help": "Disassemble some DSP instructions at pc / address",
			"hints": "[addr] [count]"
		},

		"dst": {
			"help": "Dump DSP call stack"
		},

		"difx": {
			"help": "Dump DSP IFX"
		},

		"cpumbox": {
			"help": "Write message to CPU Mailbox",
			"args": 1,
			"usage": [
				"Syntax: cpumbox <value>\n",
				"Example of use: cpumbox 0x8001FEED\n"
			]
		},

		"dspmbox": {
			"help": "Read message from DSP Mailbox"
		},

		"cpudspint": {
			"help": "Send CPU->DSP interrupt"
		},

		"dspcpuint": {
			"help": "Send DSP->CPU interrupt"
		},

		"DspIsRunning": {
			"internal": true,
			"output": "Bool"
		},

		"DspRun": {
			"internal": true
		},

		"DspSuspend": {
			"internal": true
		},

		"DspStep": {
			"internal": true
		},

		"DspGetReg": {
			"internal": true,
			"args": 1,
			"output": "UInt16"
		},

		"DspGetPsr": {
			"internal": true,
			"output": "UInt16"
		},

		"DspGetPc": {
			"internal": true,
			"output": "UInt16"
		},

		"DspPackProd": {
			"internal": true,
			"output": "UInt64 (crazy packed multiply product)"
		},

		"DspTranslateDMem": {
			"internal": true,
			"args": 1,
			"output": "UInt64 (pointer). nullptr if cannot be translated."
		},

		"DspTranslateIMem": {
			"internal": true,
			"args": 1,
			"output": "UInt64 (pointer). nullptr if cannot be translated."
		},

		"DspTestBreakpoint": {
			"internal": true,
			"args": 1,
			"output": "Bool"
		},

		"DspToggleBreakpoint": {
			"internal": true,
			"args": 1
		},

		"DspAddOneShotBreakpoint": {
			"internal": true,
			"args": 1
		},

		"DspIsCall": {
			"internal": true,
			"args": 1,
			"output": "Array: [Bool, UInt32 targetAddress]"
		},

		"DspIsCallOrJump": {
			"internal": true,
			"args": 1,
			"output": "Array: [Bool, UInt32 targetAddress]"
		},

		"DspDisasm": {
			"internal": true,
			"args": 1,
			"output": "Array: [Bool flowControl, Int instrSizeWords, String]. Empty string (\"\") mean disasm error"
		},

		"DspWatch": {
			"help": "Adds DSP DMEM address for tracking",
			"args": 1,
			"hint": "<addr>",
			"usage": [
				"Syntax: DspUnwatch <dsp_addr>\n",
				"Adds DSP DMEM address for tracking\n",
				"Example of use: DspUnwatch 0x0BE5\n"
			]
		},

		"DspUnwatch": {
			"help": "Removes DSP DMEM address tracking",
			"args": 1,
			"hint": "<addr>",
			"usage": [
				"Syntax: DspUnwatch <dsp_addr>\n",
				"Removes DSP DMEM address tracking\n",
				"Example of use: DspUnwatch 0x0BE5\n"
			]
		},

		"DspUnwatchAll": {
			"help": "Remove all DSP DMEM tracking addresses"
		},

		"DspWatchList": {
			"help": "List DSP DMEM addresses for tracking",
			"hint": "[hide]",
			"output": "Array: [address1, address2, ...]"
		}

	}
}
)json";

	Dsp16::Dsp16()
	{
		dspThread = EMUCreateThread(DspThreadProc, true, this, "DspCore");

		core = new DspCore(this);

		JDI::Hub.AddNode(L"DSP_JDI_JSON", DspJdi, dsp_init_handlers);
	}

	Dsp16::~Dsp16()
	{
		EMUJoinThread(dspThread);
		delete core;

		JDI::Hub.RemoveNode(L"DSP_JDI_JSON");
	}

	void Dsp16::DspThreadProc(void* Parameter)
	{
		Dsp16* dsp = (Dsp16*)Parameter;

		// Do DSP actions
		dsp->core->Update();
	}

	void Dsp16::Run()
	{
		if (!dspThread->IsRunning())
		{
			dspThread->Resume();
			if (logDspControlBits)
			{
				Report(Channel::DSP, "Run\n");
			}
			savedGekkoTicks = Core->GetTicks();
		}
	}

	void Dsp16::Suspend()
	{
		if (dspThread->IsRunning())
		{
			if (logDspControlBits)
			{
				Report(Channel::DSP, "Suspend\n");
			}
			dspThread->Suspend();
		}
	}

	void Dsp16::ResetIfx()
	{
		DspToCpuMailbox[0] = 0;
		DspToCpuMailbox[1] = 0;
		CpuToDspMailbox[0] = 0;
		CpuToDspMailbox[1] = 0;

		memset(&DmaRegs, 0, sizeof(DmaRegs));
		memset(&Accel, 0, sizeof(Accel));
	}

	// Dump IFX State
	void Dsp16::DumpIfx()
	{
		Report(Channel::Norm, "Cpu2Dsp Mailbox: Hi: 0x%04X, Lo: 0x%04X\n",
			(uint16_t)CpuToDspMailbox[0], (uint16_t)CpuToDspMailbox[1]);
		Report(Channel::Norm, "Dsp2Cpu Mailbox: Hi: 0x%04X, Lo: 0x%04X\n",
			(uint16_t)DspToCpuMailbox[0], (uint16_t)DspToCpuMailbox[1]);
		Report(Channel::Norm, "Dma: MmemAddr: 0x%08X, DspAddr: 0x%04X, Size: 0x%04X, Ctrl: %i\n",
			DmaRegs.mmemAddr.bits, DmaRegs.dspAddr, DmaRegs.blockSize, DmaRegs.control.bits);
		for (int i = 0; i < 16; i++)
		{
			Report(Channel::Norm, "Dsp Coef[%i]: 0x%04X\n", i, Accel.AdpcmCoef[i]);
		}
	}

#pragma region "Memory Engine"

	uint16_t Dsp16::ReadDMem(DspAddress addr)
	{
		if (core->TestWatch(addr))
		{
			Report(Channel::DSP, "ReadDMem 0x%04X, pc: 0x%04X\n", addr, core->regs.pc);
		}

		if (addr >= core->IFX_START_ADDRESS)
		{
			switch (addr)
			{
				case (DspAddress)DspHardwareRegs::DSMAH:
					return DmaRegs.mmemAddr.h;
				case (DspAddress)DspHardwareRegs::DSMAL:
					return DmaRegs.mmemAddr.l;
				case (DspAddress)DspHardwareRegs::DSPA:
					return DmaRegs.dspAddr;
				case (DspAddress)DspHardwareRegs::DSCR:
					return DmaRegs.control.bits;
				case (DspAddress)DspHardwareRegs::DSBL:
					return DmaRegs.blockSize;

				case (DspAddress)DspHardwareRegs::CMBH:
					return CpuToDspReadHi(true);
				case (DspAddress)DspHardwareRegs::CMBL:
					return CpuToDspReadLo(true);
				case (DspAddress)DspHardwareRegs::DMBH:
					return DspToCpuReadHi(true);
				case (DspAddress)DspHardwareRegs::DMBL:
					return DspToCpuReadLo(true);

				case (DspAddress)DspHardwareRegs::DIRQ:
					return DSPGetInterruptStatus() ? 1 : 0;

				case (DspAddress)DspHardwareRegs::ACSAH:
					return Accel.StartAddress.h;
				case (DspAddress)DspHardwareRegs::ACSAL:
					return Accel.StartAddress.l;
				case (DspAddress)DspHardwareRegs::ACEAH:
					return Accel.EndAddress.h;
				case (DspAddress)DspHardwareRegs::ACEAL:
					return Accel.EndAddress.l;
				case (DspAddress)DspHardwareRegs::ACCAH:
					return Accel.CurrAddress.h;
				case (DspAddress)DspHardwareRegs::ACCAL:
					return Accel.CurrAddress.l;

				case (DspAddress)DspHardwareRegs::ACFMT:
					return Accel.Fmt;
				case (DspAddress)DspHardwareRegs::ACPDS:
					return Accel.AdpcmPds;
				case (DspAddress)DspHardwareRegs::ACYN1:
					return Accel.AdpcmYn1;
				case (DspAddress)DspHardwareRegs::ACYN2:
					return Accel.AdpcmYn2;
				case (DspAddress)DspHardwareRegs::ACGAN:
					return Accel.AdpcmGan;

				case (DspAddress)DspHardwareRegs::ACDAT2:
					return AccelReadData(true);
				case (DspAddress)DspHardwareRegs::ACDAT:
					return AccelReadData(false);

				default:
					Report(Channel::DSP, "Unknown HW read 0x%04X\n", addr);
					break;
			}

			return 0;
		}

		uint8_t* ptr = core->TranslateDMem(addr);

		if (ptr)
		{
			return _BYTESWAP_UINT16(*(uint16_t*)ptr);
		}
		else
		{
			if (haltOnUnmappedMemAccess)
			{
				Halt("DSP Unmapped DMEM read 0x%04X\n", addr);
				Suspend();
			}
		}

		return 0xFFFF;
	}

	void Dsp16::WriteDMem(DspAddress addr, uint16_t value)
	{
		if (core->TestWatch(addr))
		{
			Report(Channel::DSP, "WriteDMem 0x%04X = 0x%04X, pc: 0x%04X\n", addr, value, core->regs.pc);
		}

		if (addr >= core->IFX_START_ADDRESS)
		{
			switch (addr)
			{
				case (DspAddress)DspHardwareRegs::DSMAH:
					DmaRegs.mmemAddr.h = value & 0x03ff;
					if (logDspDma)
					{
						Report(Channel::DSP, "DSMAH: 0x%04X\n", DmaRegs.mmemAddr.h);
					}
					break;
				case (DspAddress)DspHardwareRegs::DSMAL:
					DmaRegs.mmemAddr.l = value & ~3;
					if (logDspDma)
					{
						Report(Channel::DSP, "DSMAL: 0x%04X\n", DmaRegs.mmemAddr.l);
					}
					break;
				case (DspAddress)DspHardwareRegs::DSPA:
					DmaRegs.dspAddr = value & ~1;
					if (logDspDma)
					{
						Report(Channel::DSP, "DSPA: 0x%04X\n", DmaRegs.dspAddr);
					}
					break;
				case (DspAddress)DspHardwareRegs::DSCR:
					DmaRegs.control.bits = value & 3;
					if (logDspDma)
					{
						Report(Channel::DSP, "DSCR: 0x%04X\n", DmaRegs.control.bits);
					}
					break;
				case (DspAddress)DspHardwareRegs::DSBL:
					DmaRegs.blockSize = value & ~3;
					if (logDspDma)
					{
						Report(Channel::DSP, "DSBL: 0x%04X\n", DmaRegs.blockSize);
					}
					DoDma();
					break;

				case (DspAddress)DspHardwareRegs::CMBH:
					Halt("DSP is not allowed to write processor Mailbox!\n");
					Suspend();
					break;
				case (DspAddress)DspHardwareRegs::CMBL:
					Halt("DSP is not allowed to write processor Mailbox!\n");
					Suspend();
					break;
				case (DspAddress)DspHardwareRegs::DMBH:
					DspToCpuWriteHi(value);
					break;
				case (DspAddress)DspHardwareRegs::DMBL:
					DspToCpuWriteLo(value);
					break;

				case (DspAddress)DspHardwareRegs::DIRQ:
					if (value & 1)
					{
						if (logDspInterrupts)
						{
							Report(Channel::DSP, "DspHardwareRegs::DIRQ\n");
						}
						DSPAssertInt();
					}
					break;

				case (DspAddress)DspHardwareRegs::ACSAH:
					Accel.StartAddress.h = value;
					if (logAccel)
					{
						Report(Channel::DSP, "ACSAH = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACSAL:
					Accel.StartAddress.l = value;
					if (logAccel)
					{
						Report(Channel::DSP, "ACSAL = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACEAH:
					Accel.EndAddress.h = value;
					if (logAccel)
					{
						Report(Channel::DSP, "ACEAH = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACEAL:
					Accel.EndAddress.l = value;
					if (logAccel)
					{
						Report(Channel::DSP, "ACEAL = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACCAH:
					Accel.CurrAddress.h = value;
					if (logAccel)
					{
						Report(Channel::DSP, "ACCAH = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACCAL:
					Accel.CurrAddress.l = value;
					if (logAccel)
					{
						Report(Channel::DSP, "ACCAL = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACDAT2:
					AccelWriteData(value);
					if (logAccel)
					{
						Report(Channel::DSP, "ACDAT2 = 0x%04X\n", value);
					}
					break;

				case (DspAddress)DspHardwareRegs::ACFMT:
					Accel.Fmt = value;
					if (logAccel || logAdpcm)
					{
						Report(Channel::DSP, "ACFMT = 0x%04X\n", value);
					}
					break;

				case (DspAddress)DspHardwareRegs::ACPDS:
					Accel.AdpcmPds = value;
					if (logAdpcm)
					{
						Report(Channel::DSP, "ACPDS = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACYN1:
					Accel.AdpcmYn1 = value;
					if (logAdpcm)
					{
						Report(Channel::DSP, "ACYN1 = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACYN2:
					Accel.AdpcmYn2 = value;
					if (logAdpcm)
					{
						Report(Channel::DSP, "ACYN2 = 0x%04X\n", value);
					}
					break;
				case (DspAddress)DspHardwareRegs::ACGAN:
					Accel.AdpcmGan = value;
					if (logAdpcm)
					{
						Report(Channel::DSP, "ACGAN = 0x%04X\n", value);
					}
					break;

				case (DspAddress)DspHardwareRegs::ADPCM_A00:
					Accel.AdpcmCoef[0] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A10:
					Accel.AdpcmCoef[1] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A20:
					Accel.AdpcmCoef[2] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A30:
					Accel.AdpcmCoef[3] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A40:
					Accel.AdpcmCoef[4] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A50:
					Accel.AdpcmCoef[5] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A60:
					Accel.AdpcmCoef[6] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A70:
					Accel.AdpcmCoef[7] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A01:
					Accel.AdpcmCoef[8] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A11:
					Accel.AdpcmCoef[9] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A21:
					Accel.AdpcmCoef[10] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A31:
					Accel.AdpcmCoef[11] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A41:
					Accel.AdpcmCoef[12] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A51:
					Accel.AdpcmCoef[13] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A61:
					Accel.AdpcmCoef[14] = value;
					break;
				case (DspAddress)DspHardwareRegs::ADPCM_A71:
					Accel.AdpcmCoef[15] = value;
					break;

				case (DspAddress)DspHardwareRegs::UNKNOWN_FFB0:
				case (DspAddress)DspHardwareRegs::UNKNOWN_FFB1:
					Report(Channel::DSP, "Known unknown HW write 0x%04X = 0x%04X\n", addr, value);
					break;

				default:
					Report(Channel::DSP, "Unknown HW write 0x%04X = 0x%04X\n", addr, value);
					break;
			}
			return;
		}

		if (addr < (core->DRAM_SIZE / 2))
		{
			uint8_t* ptr = core->TranslateDMem(addr);

			if (ptr)
			{
				*(uint16_t*)ptr = _BYTESWAP_UINT16(value);
				return;
			}
		}

		if (haltOnUnmappedMemAccess)
		{
			Halt("DSP Unmapped DMEM write 0x%04X = 0x%04X\n", addr, value);
			Suspend();
		}
	}

#pragma endregion "Memory Engine"


#pragma region "Flipper interface"

	void Dsp16::SetResetBit(bool val)
	{
		if (val)
		{
			core->AssertInterrupt(DspInterrupt::Reset);
		}
	}

	bool Dsp16::GetResetBit()
	{
		return false;
	}

	void Dsp16::SetIntBit(bool val)
	{
		if (val)
		{
			core->AssertInterrupt(DspInterrupt::CpuInt);
		}
	}

	bool Dsp16::GetIntBit()
	{
		return core->IsInterruptPending(DspInterrupt::CpuInt);
	}

	void Dsp16::SetHaltBit(bool val)
	{
		val ? Suspend() : Run();
	}

	bool Dsp16::GetHaltBit()
	{
		return !dspThread->IsRunning();
	}

#pragma endregion "Flipper interface"

}

// DSP Mailbox processing

namespace DSP
{
	// CPU->DSP Mailbox

	// Write by processor only.

	void Dsp16::CpuToDspWriteHi(uint16_t value)
	{
		CpuToDspLock[0].Lock();

		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "CpuToDspWriteHi: 0x%04X\n", value);
		}

		core->delay_mailbox_reasons = 4;

		CpuToDspMailbox[0] = value & 0x7FFF;
		CpuToDspLock[0].Unlock();
	}

	void Dsp16::CpuToDspWriteLo(uint16_t value)
	{
		CpuToDspLock[1].Lock();

		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "CpuToDspWriteLo: 0x%04X\n", value);
		}

		core->delay_mailbox_reasons = 4;

		CpuToDspMailbox[1] = value;
		CpuToDspMailbox[0] |= 0x8000;

		if (logMailbox)
		{
			Report(Channel::DSP, "CPU Write Message: 0x%04X_%04X\n", CpuToDspMailbox[0], CpuToDspMailbox[1]);
		}
		CpuToDspLock[1].Unlock();
	}

	uint16_t Dsp16::CpuToDspReadHi(bool ReadByDsp)
	{
		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "CpuToDspReadHi\n");
		}

		CpuToDspLock[0].Lock();
		uint16_t value = CpuToDspMailbox[0];
		CpuToDspLock[0].Unlock();
		return value;
	}

	uint16_t Dsp16::CpuToDspReadLo(bool ReadByDsp)
	{
		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "CpuToDspReadLo\n");
		}

		CpuToDspLock[1].Lock();
		uint16_t value = CpuToDspMailbox[1];
		if (ReadByDsp)
		{
			if (logMailbox)
			{
				Report(Channel::DSP, "DSP Read Message: 0x%04X_%04X\n", CpuToDspMailbox[0], CpuToDspMailbox[1]);
			}
			CpuToDspMailbox[0] &= ~0x8000;				// When DSP read
		}
		CpuToDspLock[1].Unlock();
		return value;
	}

	// DSP->CPU Mailbox

	// Write by DSP only.

	void Dsp16::DspToCpuWriteHi(uint16_t value)
	{
		DspToCpuLock[0].Lock();

		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "DspToCpuWriteHi: 0x%04X\n", value);
		}

		DspToCpuMailbox[0] = value & 0x7FFF;
		DspToCpuLock[0].Unlock();
	}

	void Dsp16::DspToCpuWriteLo(uint16_t value)
	{
		DspToCpuLock[1].Lock();

		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "DspToCpuWriteLo: 0x%04X\n", value);
		}

		DspToCpuMailbox[1] = value;
		DspToCpuMailbox[0] |= 0x8000;

		if (logMailbox)
		{
			Report(Channel::DSP, "DSP Write Message: 0x%04X_%04X\n", DspToCpuMailbox[0], DspToCpuMailbox[1]);
		}

		DspToCpuLock[1].Unlock();
	}

	uint16_t Dsp16::DspToCpuReadHi(bool ReadByDsp)
	{
		DspToCpuLock[0].Lock();
		uint16_t value = DspToCpuMailbox[0];
		DspToCpuLock[0].Unlock();
		return value;
	}

	uint16_t Dsp16::DspToCpuReadLo(bool ReadByDsp)
	{
		DspToCpuLock[1].Lock();
		uint16_t value = DspToCpuMailbox[1];
		if (!ReadByDsp)
		{
			if (logMailbox)
			{
				Report(Channel::DSP, "CPU Read Message: 0x%04X_%04X\n", DspToCpuMailbox[0], DspToCpuMailbox[1]);
			}
			DspToCpuMailbox[0] &= ~0x8000;					// When CPU read
		}
		DspToCpuLock[1].Unlock();
		return value;
	}
}