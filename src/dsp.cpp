#include "pch.h"

// The real DSP is designed as follows: [DSP PI Interface] <-> [DSPCore] <-> [ARAM Controller/Accelerator]
// The DSPCore includes the actual DSP instruction execution core ("Engine"), as well as the IMEM/DMEM and the register decoder (0xff00...)
// DSP PI I/F deals with PI communication (interrupts, Mailbox) and also contains 3 DMA controllers: DSP IMEM/DMEM DMA, AI DMA, ARAM DMA
// ARAM(SDRAM) Controller controls access to external SDRAM memory (ARAM) and also contains the "Accelerator"

using namespace Debug;

namespace DSP
{

	Dsp16::Dsp16()
	{
		dspThread = EMUCreateThread(DspThreadProc, true, this, "DspCore");

		core = new DspCore(this);

		JDI::Hub.AddNode(L"DSP_JDI_JSON", JdiSpecs::DspJdi, dsp_init_handlers);
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

		// Block until the CPU thread says that a batch of DSP time is due (see DspCore::TickSync).
		// The thread used to poll the shared time base in a tight loop, which cost far more than the
		// DSP work itself (see the benchmark notes in `testing/gekko_bench`).
		dsp->core->WaitForWork();

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
			core->wakeTick = savedGekkoTicks;
			core->workEvent.Signal();
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
		DspToCpuSnapshot = 0;
		DspToCpuSnapshotValid = false;
		CpuToDspMailbox[0] = 0;
		CpuToDspMailbox[1] = 0;
		CpuToDspSnapshot = 0;
		CpuToDspSnapshotValid = false;

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

				case (DspAddress)DspHardwareRegs::ADM:
					return Accel.Fmt;
				case (DspAddress)DspHardwareRegs::ACPDS:
					return Accel.AdpcmPds;
				case (DspAddress)DspHardwareRegs::ACYN1:
					return Accel.AdpcmYn1;
				case (DspAddress)DspHardwareRegs::ACYN2:
					return Accel.AdpcmYn2;
				case (DspAddress)DspHardwareRegs::ACGAN:
					return Accel.AdpcmGan;

				case (DspAddress)DspHardwareRegs::ACXN:
					return Accel.AdpcmXn;

				case (DspAddress)DspHardwareRegs::AMDM:
					return aram.masked ? 1 : 0;

				case (DspAddress)DspHardwareRegs::ACDL:
					return AccelReadData(true);
				case (DspAddress)DspHardwareRegs::ACYN:
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
					Accel.StartAddress.h = value & 0x07ff;
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
					Accel.EndAddress.h = value & 0x07ff;
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
					Accel.CurrAddress.h = value & 0x87ff;		// bit 15 = direction, bits 10:0 = address
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
				case (DspAddress)DspHardwareRegs::ACDL:
					AccelWriteData(value);
					if (logAccel)
					{
						Report(Channel::DSP, "ACDL = 0x%04X\n", value);
					}
					break;

				case (DspAddress)DspHardwareRegs::ADM:
					Accel.Fmt = value;
					if (logAccel || logAdpcm)
					{
						Report(Channel::DSP, "ADM = 0x%04X\n", value);
					}
					break;

				case (DspAddress)DspHardwareRegs::AMDM:
					// The DSP can mask ARAM-DMA requests, dedicating ARAM to the accelerator
					aram.masked = (value & 1) != 0;
					if (logAccel)
					{
						Report(Channel::DSP, "AMDM = 0x%04X\n", value);
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
				case (DspAddress)DspHardwareRegs::ACXN:
					Accel.AdpcmXn = value;
					if (logAdpcm)
					{
						Report(Channel::DSP, "ACXN = 0x%04X\n", value);
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
			// CDCR bit 1 is a *request*, not a pulse: the hardware clears it when the core takes
			// the interrupt through the TE3/ET gate (dsp.md section 4.2). Latch it so that a
			// request which arrives while the core has the interrupt masked is delivered as soon
			// as the gate opens - the loader hands control to a microcode that closes the window
			// at its entry and opens it again a few instructions later, and both Zelda and the
			// AX microcode rely on the request surviving that window.
			intdspRequested = true;
			core->AssertInterrupt(DspInterrupt::CpuInt);
		}
	}

	bool Dsp16::GetIntBit()
	{
		return intdspRequested || core->IsInterruptPending(DspInterrupt::CpuInt);
	}

	bool Dsp16::CpuIntRequested() const
	{
		return intdspRequested;
	}

	void Dsp16::ClearCpuIntRequest()
	{
		intdspRequested = false;
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
//
// The mailbox protocol is defined by dsp.md section 4.1: both halves of a 32-bit message are
// transferred high word first and low word second; writing the high word clears the valid
// flag (bit 15 of the high word register), writing the low word sets it, and the receiver
// clears it again by reading the low word.
//
// Two properties follow from that and are implemented below:
//
//   1. The two halves of one mailbox are protected by a single lock, so a sender can never
//      update the low word of a pair while a receiver is picking up the pair.
//   2. Reading the high word also snapshots the low word. The receiver reads the high word
//      first and the low word second; if the sender posts its next message in between, the
//      receiver must still get the low word that belongs to the message whose valid flag it
//      observed, not a mixture of two messages.

namespace DSP
{
	// CPU->DSP Mailbox

	// Write by processor only.

	void Dsp16::CpuToDspWriteHi(uint16_t value)
	{
		CpuToDspLock.Lock();

		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "CpuToDspWriteHi: 0x%04X\n", value);
		}

		core->HoldMailbox();

		// Bit 15 carries the valid flag, so the sender's bit 15 is discarded here
		// (writing the high word clears the flag).
		CpuToDspMailbox[0] = value & 0x7FFF;
		CpuToDspLock.Unlock();
	}

	void Dsp16::CpuToDspWriteLo(uint16_t value)
	{
		CpuToDspLock.Lock();

		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "CpuToDspWriteLo: 0x%04X\n", value);
		}

		core->HoldMailbox();

		CpuToDspMailbox[1] = value;
		CpuToDspMailbox[0] |= 0x8000;

		if (logMailbox)
		{
			Report(Channel::DSP, "CPU Write Message: 0x%04X_%04X\n", CpuToDspMailbox[0], CpuToDspMailbox[1]);
		}
		CpuToDspLock.Unlock();
	}

	uint16_t Dsp16::CpuToDspReadHi(bool ReadByDsp)
	{
		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "CpuToDspReadHi\n");
		}

		CpuToDspLock.Lock();
		uint16_t value = CpuToDspMailbox[0];
		// A high word with the valid flag set is the head of a complete message: latch its
		// low word so that the following low word read returns that message, even when the
		// sender has posted the next one in the meantime.
		CpuToDspSnapshotValid = (value & 0x8000) != 0;
		if (CpuToDspSnapshotValid)
		{
			CpuToDspSnapshot = CpuToDspMailbox[1];
		}
		CpuToDspLock.Unlock();
		return value;
	}

	uint16_t Dsp16::CpuToDspReadLo(bool ReadByDsp)
	{
		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "CpuToDspReadLo\n");
		}

		CpuToDspLock.Lock();
		uint16_t value = CpuToDspSnapshotValid ? CpuToDspSnapshot : CpuToDspMailbox[1];
		CpuToDspSnapshotValid = false;
		if (ReadByDsp)
		{
			if (logMailbox)
			{
				Report(Channel::DSP, "DSP Read Message: 0x%04X_%04X\n", CpuToDspMailbox[0], CpuToDspMailbox[1]);
			}
			CpuToDspMailbox[0] &= ~0x8000;				// When DSP read
		}
		CpuToDspLock.Unlock();
		return value;
	}

	// DSP->CPU Mailbox

	// Write by DSP only.

	void Dsp16::DspToCpuWriteHi(uint16_t value)
	{
		DspToCpuLock.Lock();

		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "DspToCpuWriteHi: 0x%04X\n", value);
		}

		DspToCpuMailbox[0] = value & 0x7FFF;
		DspToCpuLock.Unlock();
	}

	void Dsp16::DspToCpuWriteLo(uint16_t value)
	{
		DspToCpuLock.Lock();

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

		DspToCpuLock.Unlock();
	}

	uint16_t Dsp16::DspToCpuReadHi(bool ReadByDsp)
	{
		DspToCpuLock.Lock();
		uint16_t value = DspToCpuMailbox[0];
		DspToCpuSnapshotValid = (value & 0x8000) != 0;
		if (DspToCpuSnapshotValid)
		{
			DspToCpuSnapshot = DspToCpuMailbox[1];
		}
		DspToCpuLock.Unlock();
		return value;
	}

	uint16_t Dsp16::DspToCpuReadLo(bool ReadByDsp)
	{
		DspToCpuLock.Lock();
		uint16_t value = DspToCpuSnapshotValid ? DspToCpuSnapshot : DspToCpuMailbox[1];
		DspToCpuSnapshotValid = false;
		if (!ReadByDsp)
		{
			if (logMailbox)
			{
				Report(Channel::DSP, "CPU Read Message: 0x%04X_%04X\n", DspToCpuMailbox[0], DspToCpuMailbox[1]);
			}
			DspToCpuMailbox[0] &= ~0x8000;					// When CPU read
		}
		DspToCpuLock.Unlock();
		return value;
	}
}
