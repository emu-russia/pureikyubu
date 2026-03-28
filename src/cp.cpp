// CP - command processor
#include "pch.h"

// TODO: It's a bit crooked right now after refactoring, but will settle with time

// CP is architecturally NOT part of the graphics Pipeline, but is the initiator of drawing primitives and updating the internal context of GFX registers (XF, SU, PE, etc. load reg commands)

using namespace Debug;

namespace Flipper
{
	// init

	CommandProcessor::CommandProcessor(HWConfig* config)
	{
		Report(Channel::CP, "Command processor (for GFX)\n");

		memset(&cpregs, 0, sizeof(cpregs));
		cpregs.cr |= CP_CR_WPINC;		// 1 on reset

		// Command Processor
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_STATUS, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_ENABLE, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_CLR, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_MEMPERF_SEL, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_STM_LOW, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_BASEL, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_BASEH, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_TOPL, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_TOPH, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_HICNTL, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_HICNTH, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_LOCNTL, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_LOCNTH, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_COUNTL, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_COUNTH, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_WPTRL, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_WPTRH, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_RPTRL, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_RPTRH, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_BRKL, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_BRKH, CPRegRead, CPRegWrite, this);

		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_COUNTER0L, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_COUNTER0H, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_COUNTER1L, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_COUNTER1H, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_COUNTER2L, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_COUNTER2H, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_COUNTER3L, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_COUNTER3H, CPRegRead, CPRegWrite, this);

		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_VC_CHKCNTL, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_VC_CHKCNTH, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_VC_MISSL, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_VC_MISSH, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_VC_STALLL, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_VC_STALLH, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FRCLK_CNTL, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_FRCLK_CNTH, CPRegRead, CPRegWrite, this);

		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_XF_ADDR, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_XF_DATAL, CPRegRead, CPRegWrite, this);
		HW->pi->PISetTrap(PI_REGSPACE_CP | CP_XF_DATAH, CPRegRead, CPRegWrite, this);

		fifo = new FifoProcessor();
		fifo->Reset();

		tickPerFifo = 100;
		updateTbrValue = Core->GetTicks() + tickPerFifo;

		cp_thread = EMUCreateThread(CPThread, false, this, "CPThread");

		// clear counters
		tris = pts = lines = 0;
	}

	CommandProcessor::~CommandProcessor()
	{
		if (cp_thread)
		{
			EMUJoinThread(cp_thread);
			cp_thread = nullptr;
		}

		delete fifo;
	}

	#pragma region "Dealing with registers"

	//
	// Stubs
	//

	void CommandProcessor::CPRegRead(uint32_t addr, uint32_t* reg, void* context)
	{
		CommandProcessor* cp = (CommandProcessor*)context;
		*reg = cp->CpReadReg(addr & 0xFF);
	}

	void CommandProcessor::CPRegWrite(uint32_t addr, uint32_t data, void* context)
	{
		CommandProcessor* cp = (CommandProcessor*)context;
		cp->CpWriteReg(addr & 0xFF, data);
	}

	void CommandProcessor::CP_BREAK()
	{
		if (cpregs.cr & CP_CR_BPINTEN && (cpregs.sr & CP_SR_BPINT) == 0)
		{
			cpregs.sr |= CP_SR_BPINT;
			HW->pi->PIAssertInt(PI_INTERRUPT_CP);
			Report(Channel::CP, "BREAK\n");
		}
	}

	void CommandProcessor::CP_OVF()
	{
		if (cpregs.cr & CP_CR_OVFEN && (cpregs.sr & CP_SR_OVF) == 0)
		{
			cpregs.sr |= CP_SR_OVF;
			HW->pi->PIAssertInt(PI_INTERRUPT_CP);
			Report(Channel::CP, "OVF\n");
		}
	}

	void CommandProcessor::CP_UVF()
	{
		if (cpregs.cr & CP_CR_UVFEN && (cpregs.sr & CP_SR_UVF) == 0)
		{
			cpregs.sr |= CP_SR_UVF;
			HW->pi->PIAssertInt(PI_INTERRUPT_CP);
			Report(Channel::CP, "UVF\n");
		}
	}

	void CommandProcessor::GXWriteFifo(uint8_t dataPtr[32])
	{
		fifo->PushBytes(dataPtr);

		while (fifo->EnoughToExecute())
		{
			GxCommand(fifo);
		}
	}

	void CommandProcessor::CPThread(void* Param)
	{
		CommandProcessor* cp = (CommandProcessor*)Param;

		int64_t ticks = Core->GetTicks();
		if (ticks < cp->updateTbrValue)
		{
			return;
		}
		cp->updateTbrValue = ticks + cp->tickPerFifo;

		// Calculate count
		if (cp->cpregs.wrptr >= cp->cpregs.rdptr)
		{
			cp->cpregs.cnt = cp->cpregs.wrptr - cp->cpregs.rdptr;
		}
		else
		{
			cp->cpregs.cnt = (cp->cpregs.top - cp->cpregs.rdptr) + (cp->cpregs.wrptr - cp->cpregs.base);
		}

		// Watermarks logic. Active only in linked-mode (?).
		if (cp->cpregs.cnt > cp->cpregs.himark)
		{
			cp->CP_OVF();
		}
		if (cp->cpregs.cnt < cp->cpregs.lomark)
		{
			cp->CP_UVF();
		}

		// Breakpoint
		if ((cp->cpregs.rdptr & ~0x1f) == (cp->cpregs.bpptr & ~0x1f))
		{
			cp->CP_BREAK();
		}

		// Advance read pointer.
		if (cp->cpregs.cnt != 0 && cp->cpregs.cr & CP_CR_RDEN && (cp->cpregs.sr & (CP_SR_OVF | CP_SR_UVF | CP_SR_BPINT)) == 0)
		{
			cp->cpregs.sr &= ~(CP_SR_RD_IDLE | CP_SR_CMD_IDLE);

			cp->GXWriteFifo( (uint8_t*)HW->mem->MIGetMemoryPointerForCP(cp->cpregs.rdptr) );

			cp->cpregs.rdptr += 32;
			if (cp->cpregs.rdptr == cp->cpregs.top)
			{
				cp->cpregs.rdptr = cp->cpregs.base;
			}
		}
		else
		{
			cp->cpregs.sr |= (CP_SR_RD_IDLE | CP_SR_CMD_IDLE);
		}
	}

	void CommandProcessor::CPAbortFifo()
	{
		Report(Channel::GP, "CP Abort FIFO\n");
		fifo->Reset();
	}

	uint16_t CommandProcessor::CpReadReg(uint32_t addr)
	{
		switch (addr)
		{
			case CP_STATUS:
				return cpregs.sr;
			case CP_ENABLE:
				return cpregs.cr;
			case CP_CLR:
				return 0;
			case CP_MEMPERF_SEL:
				return 0;
			case CP_STM_LOW:
				return 0;
			case CP_FIFO_BASEL:
				return cpregs.basel & 0xffe0;
			case CP_FIFO_BASEH:
				return cpregs.baseh;
			case CP_FIFO_TOPL:
				return cpregs.topl & 0xffe0;
			case CP_FIFO_TOPH:
				return cpregs.toph;
			case CP_FIFO_HICNTL:
				return cpregs.himarkl & 0xffe0;
			case CP_FIFO_HICNTH:
				return cpregs.himarkh;
			case CP_FIFO_LOCNTL:
				return cpregs.lomarkl & 0xffe0;
			case CP_FIFO_LOCNTH:
				return cpregs.lomarkh;
			case CP_FIFO_COUNTL:
				return cpregs.cntl & 0xffe0;
			case CP_FIFO_COUNTH:
				return cpregs.cnth;
			case CP_FIFO_WPTRL:
				return cpregs.wrptrl & 0xffe0;
			case CP_FIFO_WPTRH:
				return cpregs.wrptrh;
			case CP_FIFO_RPTRL:
				return cpregs.rdptrl & 0xffe0;
			case CP_FIFO_RPTRH:
				return cpregs.rdptrh;
			case CP_FIFO_BRKL:
				return cpregs.bpptrl & 0xffe0;
			case CP_FIFO_BRKH:
				return cpregs.bpptrh;
			case CP_COUNTER0L:
				return 0;
			case CP_COUNTER0H:
				return 0;
			case CP_COUNTER1L:
				return 0;
			case CP_COUNTER1H:
				return 0;
			case CP_COUNTER2L:
				return 0;
			case CP_COUNTER2H:
				return 0;
			case CP_COUNTER3L:
				return 0;
			case CP_COUNTER3H:
				return 0;
			case CP_VC_CHKCNTL:
				return 0;
			case CP_VC_CHKCNTH:
				return 0;
			case CP_VC_MISSL:
				return 0;
			case CP_VC_MISSH:
				return 0;
			case CP_VC_STALLL:
				return 0;
			case CP_VC_STALLH:
				return 0;
			case CP_FRCLK_CNTL:
				return 0;
			case CP_FRCLK_CNTH:
				return 0;
			case CP_XF_ADDR:
				return 0;
			case CP_XF_DATAL:
				return 0;
			case CP_XF_DATAH:
				return 0;
		}

		return 0;
	}

	void CommandProcessor::CpWriteReg(uint32_t addr, uint16_t value)
	{
		switch (addr)
		{
			case CP_STATUS:
				break;
			case CP_ENABLE:
				cpregs.cr = (uint16_t)value;

				// clear breakpoint
				if ((value & CP_CR_BPINTEN) == 0)
				{
					cpregs.sr &= ~CP_SR_BPINT;
				}

				if ((cpregs.sr & CP_SR_BPINT) == 0 && (cpregs.sr & CP_SR_OVF) == 0 && (cpregs.sr & CP_SR_UVF) == 0)
				{
					HW->pi->PIClearInt(PI_INTERRUPT_CP);
				}
				break;
			case CP_CLR:
				// clear watermark conditions
				if (value & CP_CLR_OVFCLR)
				{
					cpregs.sr &= ~CP_SR_OVF;
				}
				if (value & CP_CLR_UVFCLR)
				{
					cpregs.sr &= ~CP_SR_UVF;
				}

				if ((cpregs.sr & CP_SR_BPINT) == 0 && (cpregs.sr & CP_SR_OVF) == 0 && (cpregs.sr & CP_SR_UVF) == 0)
				{
					HW->pi->PIClearInt(PI_INTERRUPT_CP);
				}
				break;
			case CP_MEMPERF_SEL:
				break;
			case CP_STM_LOW:
				break;
			case CP_FIFO_BASEL:
				cpregs.basel = value & 0xffe0;
				break;
			case CP_FIFO_BASEH:
				cpregs.baseh = value;
				break;
			case CP_FIFO_TOPL:
				cpregs.topl = value & 0xffe0;
				break;
			case CP_FIFO_TOPH:
				cpregs.toph = value;
				break;
			case CP_FIFO_HICNTL:
				cpregs.himarkl = value & 0xffe0;
				break;
			case CP_FIFO_HICNTH:
				cpregs.himarkh = value;
				break;
			case CP_FIFO_LOCNTL:
				cpregs.lomarkl = value & 0xffe0;
				break;
			case CP_FIFO_LOCNTH:
				cpregs.lomarkh = value;
				break;
			case CP_FIFO_COUNTL:
				cpregs.cntl = value & 0xffe0;
				break;
			case CP_FIFO_COUNTH:
				cpregs.cnth = value;
				break;
			case CP_FIFO_WPTRL:
				cpregs.wrptrl = value & 0xffe0;
				break;
			case CP_FIFO_WPTRH:
				cpregs.wrptrh = value;
				break;
			case CP_FIFO_RPTRL:
				cpregs.rdptrl = value & 0xffe0;
				break;
			case CP_FIFO_RPTRH:
				cpregs.rdptrh = value;
				break;
			case CP_FIFO_BRKL:
				cpregs.bpptrl = value & 0xffe0;
				break;
			case CP_FIFO_BRKH:
				cpregs.bpptrh = value;
				break;
			case CP_COUNTER0L:
				break;
			case CP_COUNTER0H:
				break;
			case CP_COUNTER1L:
				break;
			case CP_COUNTER1H:
				break;
			case CP_COUNTER2L:
				break;
			case CP_COUNTER2H:
				break;
			case CP_COUNTER3L:
				break;
			case CP_COUNTER3H:
				break;
			case CP_VC_CHKCNTL:
				break;
			case CP_VC_CHKCNTH:
				break;
			case CP_VC_MISSL:
				break;
			case CP_VC_MISSH:
				break;
			case CP_VC_STALLL:
				break;
			case CP_VC_STALLH:
				break;
			case CP_FRCLK_CNTL:
				break;
			case CP_FRCLK_CNTH:
				break;
			case CP_XF_ADDR:
				break;
			case CP_XF_DATAL:
				break;
			case CP_XF_DATAH:
				break;
		}
	}

	void CommandProcessor::FifoWriteBurst()
	{
		// CP FIFO

		if (cpregs.cr & CP_CR_WPINC)
		{
			cpregs.wrptr += 32;

			if (cpregs.wrptr == cpregs.top)
			{
				cpregs.wrptr = cpregs.base;
			}

			// All other work is done by CommandProcessor thread.
		}
	}

	// show CP fifo configuration
	void CommandProcessor::DumpCPFIFO()
	{
		// fifo modes
		char* md = (cpregs.cr & CP_CR_WPINC) ? ((char*)"immediate ") : ((char*)"multi-");
		char bp = (cpregs.cr & CP_CR_BPEN) ? ('B') : ('b');    // breakpoint
		char lw = (cpregs.cr & CP_CR_UVFEN) ? ('U') : ('u');    // low-wmark
		char hw = (cpregs.cr & CP_CR_OVFEN) ? ('O') : ('o');    // high-wmark

		Report(Channel::Norm, "CP %sfifo configuration:%c%c%c\n", md, bp, lw, hw);
		Report(Channel::Norm, " status :0x%08X\n", cpregs.sr);
		Report(Channel::Norm, " enable :0x%08X\n", cpregs.cr);
		Report(Channel::Norm, "   base :0x%08X\n", cpregs.base);
		Report(Channel::Norm, "   top  :0x%08X\n", cpregs.top);
		Report(Channel::Norm, "   low  :0x%08X\n", cpregs.lomark);
		Report(Channel::Norm, "   high :0x%08X\n", cpregs.himark);
		Report(Channel::Norm, "   cnt  :0x%08X\n", cpregs.cnt);
		Report(Channel::Norm, "   wrptr:0x%08X\n", cpregs.wrptr);
		Report(Channel::Norm, "   rdptr:0x%08X\n", cpregs.rdptr);
		Report(Channel::Norm, "   break:0x%08X\n", cpregs.bpptr);
	}

	// index range = 00..FF
	// reg size = 32 bit
	void CommandProcessor::loadCPReg(size_t index, uint32_t value, FifoProcessor* gxfifo)
	{
		cpLoads++;

		if (GpRegsLog)
		{
			Report(Channel::GP, "Load CP: index: 0x%02X, data: 0x%08X\n", index, value);
		}

		switch(index)
		{
			case CP_MATINDEX_A_ID:
			{
				cp.matIndexA.bits = value;
			}
			return;

			case CP_MATINDEX_B_ID:
			{
				cp.matIndexB.bits = value;
			}
			return;

			case CP_VCD_LO_ID:
			{
				cp.vcdLo.bits = value;
				FifoReconfigure(gxfifo);
			}
			return;

			case CP_VCD_HI_ID:
			{
				cp.vcdHi.bits = value;
				FifoReconfigure(gxfifo);
			}
			return;

			case CP_VAT_A_ID | 0:
			case CP_VAT_A_ID | 1:
			case CP_VAT_A_ID | 2:
			case CP_VAT_A_ID | 3:
			case CP_VAT_A_ID | 4:
			case CP_VAT_A_ID | 5:
			case CP_VAT_A_ID | 6:
			case CP_VAT_A_ID | 7:
			{
				cp.vatA[index & 7].bits = value;
				FifoReconfigure(gxfifo);
			}
			return;

			case CP_VAT_B_ID | 0:
			case CP_VAT_B_ID | 1:
			case CP_VAT_B_ID | 2:
			case CP_VAT_B_ID | 3:
			case CP_VAT_B_ID | 4:
			case CP_VAT_B_ID | 5:
			case CP_VAT_B_ID | 6:
			case CP_VAT_B_ID | 7:
			{
				cp.vatB[index & 7].bits = value;
				FifoReconfigure(gxfifo);
			}
			return;

			case CP_VAT_C_ID | 0:
			case CP_VAT_C_ID | 1:
			case CP_VAT_C_ID | 2:
			case CP_VAT_C_ID | 3:
			case CP_VAT_C_ID | 4:
			case CP_VAT_C_ID | 5:
			case CP_VAT_C_ID | 6:
			case CP_VAT_C_ID | 7:
			{
				cp.vatC[index & 7].bits = value;
				FifoReconfigure(gxfifo);
			}
			return;

			case CP_ARRAY_BASE_ID | 0:
			case CP_ARRAY_BASE_ID | 1:
			case CP_ARRAY_BASE_ID | 2:
			case CP_ARRAY_BASE_ID | 3:
			case CP_ARRAY_BASE_ID | 4:
			case CP_ARRAY_BASE_ID | 5:
			case CP_ARRAY_BASE_ID | 6:
			case CP_ARRAY_BASE_ID | 7:
			case CP_ARRAY_BASE_ID | 8:
			case CP_ARRAY_BASE_ID | 9:
			case CP_ARRAY_BASE_ID | 0xa:
			case CP_ARRAY_BASE_ID | 0xb:
			case CP_ARRAY_BASE_ID | 0xc:
			case CP_ARRAY_BASE_ID | 0xd:
			case CP_ARRAY_BASE_ID | 0xe:
			case CP_ARRAY_BASE_ID | 0xf:
			{
				cp.arrayBase[index & 0xf].bits = value;
			}
			return;

			case CP_ARRAY_STRIDE_ID | 0:
			case CP_ARRAY_STRIDE_ID | 1:
			case CP_ARRAY_STRIDE_ID | 2:
			case CP_ARRAY_STRIDE_ID | 3:
			case CP_ARRAY_STRIDE_ID | 4:
			case CP_ARRAY_STRIDE_ID | 5:
			case CP_ARRAY_STRIDE_ID | 6:
			case CP_ARRAY_STRIDE_ID | 7:
			case CP_ARRAY_STRIDE_ID | 8:
			case CP_ARRAY_STRIDE_ID | 9:
			case CP_ARRAY_STRIDE_ID | 0xa:
			case CP_ARRAY_STRIDE_ID | 0xb:
			case CP_ARRAY_STRIDE_ID | 0xc:
			case CP_ARRAY_STRIDE_ID | 0xd:
			case CP_ARRAY_STRIDE_ID | 0xe:
			case CP_ARRAY_STRIDE_ID | 0xf:
			{
				cp.arrayStride[index & 0xf].bits = value & 0xFF;
			}
			return;

			default:
			{
				Report(Channel::GP, "Unknown CP load, index: 0x%02X\n", index);
			}
		}
	}

	#pragma endregion "Dealing with registers"


	#pragma region "FIFO Processing"

	FifoProcessor::FifoProcessor()
	{
		fifo = new uint8_t[fifoSize];
		memset(fifo, 0, fifoSize);
		allocated = true;
	}

	FifoProcessor::FifoProcessor(uint8_t* fifoPtr, size_t size)
	{
		fifo = fifoPtr;
		fifoSize = size + 1;
		writePtr = fifoSize - 1;
		allocated = false;
	}

	FifoProcessor::~FifoProcessor()
	{
		if (allocated)
		{
			delete[] fifo;
		}
	}

	void FifoProcessor::Reset()
	{
		readPtr = writePtr = 0;
	}

	void FifoProcessor::PushBytes(uint8_t dataPtr[32])
	{
		lock.Lock();

		if ((writePtr + 32) < fifoSize)
		{
			memcpy(&fifo[writePtr], dataPtr, 32);
			writePtr += 32;
		}
		else
		{
			size_t part1Size = fifoSize - writePtr;
			memcpy(&fifo[writePtr], dataPtr, part1Size);
			writePtr = 32 - part1Size;
			memcpy(fifo, dataPtr + part1Size, writePtr);

			Report(Channel::GP, "FifoProcessor: fifo wrapped\n");
		}

		lock.Unlock();

		//while (EnoughToExecute())
		//{
		//	ExecuteCommand();
		//}
	}

	size_t FifoProcessor::GetSize()
	{
		if (writePtr >= readPtr)
		{
			return writePtr - readPtr;
		}
		else
		{
			return (fifoSize - readPtr) + writePtr;
		}
	}

	bool FifoProcessor::EnoughToExecute()
	{
		if (GetSize() < 1)
			return false;

		CPCommand cmd = (CPCommand)Peek8(0);

		switch(cmd)
		{
			case CPCommand::CP_CMD_NOP | 0:
			case CPCommand::CP_CMD_NOP | 1:
			case CPCommand::CP_CMD_NOP | 2:
			case CPCommand::CP_CMD_NOP | 3:
			case CPCommand::CP_CMD_NOP | 4:
			case CPCommand::CP_CMD_NOP | 5:
			case CPCommand::CP_CMD_NOP | 6:
			case CPCommand::CP_CMD_NOP | 7:
				return true;

			case CPCommand::CP_CMD_VCACHE_INVD | 0:
			case CPCommand::CP_CMD_VCACHE_INVD | 1:
			case CPCommand::CP_CMD_VCACHE_INVD | 2:
			case CPCommand::CP_CMD_VCACHE_INVD | 3:
			case CPCommand::CP_CMD_VCACHE_INVD | 4:
			case CPCommand::CP_CMD_VCACHE_INVD | 5:
			case CPCommand::CP_CMD_VCACHE_INVD | 6:
			case CPCommand::CP_CMD_VCACHE_INVD | 7:
				return true;

			case CPCommand::CP_CMD_CALL_DL | 0:
			case CPCommand::CP_CMD_CALL_DL | 1:
			case CPCommand::CP_CMD_CALL_DL | 2:
			case CPCommand::CP_CMD_CALL_DL | 3:
			case CPCommand::CP_CMD_CALL_DL | 4:
			case CPCommand::CP_CMD_CALL_DL | 5:
			case CPCommand::CP_CMD_CALL_DL | 6:
			case CPCommand::CP_CMD_CALL_DL | 7:
				return GetSize() >= 9;

			case CPCommand::CP_CMD_LOAD_BPREG | 0:
			case CPCommand::CP_CMD_LOAD_BPREG | 1:
			case CPCommand::CP_CMD_LOAD_BPREG | 2:
			case CPCommand::CP_CMD_LOAD_BPREG | 3:
			case CPCommand::CP_CMD_LOAD_BPREG | 4:
			case CPCommand::CP_CMD_LOAD_BPREG | 5:
			case CPCommand::CP_CMD_LOAD_BPREG | 6:
			case CPCommand::CP_CMD_LOAD_BPREG | 7:
			case CPCommand::CP_CMD_LOAD_BPREG | 8:
			case CPCommand::CP_CMD_LOAD_BPREG | 0xa:
			case CPCommand::CP_CMD_LOAD_BPREG | 0xb:
			case CPCommand::CP_CMD_LOAD_BPREG | 0xc:
			case CPCommand::CP_CMD_LOAD_BPREG | 0xd:
			case CPCommand::CP_CMD_LOAD_BPREG | 0xe:
			case CPCommand::CP_CMD_LOAD_BPREG | 0xf:
				return GetSize() >= 5;

			case CPCommand::CP_CMD_LOAD_CPREG | 0:
			case CPCommand::CP_CMD_LOAD_CPREG | 1:
			case CPCommand::CP_CMD_LOAD_CPREG | 2:
			case CPCommand::CP_CMD_LOAD_CPREG | 3:
			case CPCommand::CP_CMD_LOAD_CPREG | 4:
			case CPCommand::CP_CMD_LOAD_CPREG | 5:
			case CPCommand::CP_CMD_LOAD_CPREG | 6:
			case CPCommand::CP_CMD_LOAD_CPREG | 7:
				return GetSize() >= 6;
			
			case CPCommand::CP_CMD_LOAD_XFREG | 0:
			case CPCommand::CP_CMD_LOAD_XFREG | 1:
			case CPCommand::CP_CMD_LOAD_XFREG | 2:
			case CPCommand::CP_CMD_LOAD_XFREG | 3:
			case CPCommand::CP_CMD_LOAD_XFREG | 4:
			case CPCommand::CP_CMD_LOAD_XFREG | 5:
			case CPCommand::CP_CMD_LOAD_XFREG | 6:
			case CPCommand::CP_CMD_LOAD_XFREG | 7:
			{
				if (GetSize() < 5)
					return false;

				uint16_t len = Peek16(1) + 1;
				return GetSize() >= (len * 4 + 5);
			}

			case CPCommand::CP_CMD_LOAD_INDXA | 0:
			case CPCommand::CP_CMD_LOAD_INDXA | 1:
			case CPCommand::CP_CMD_LOAD_INDXA | 2:
			case CPCommand::CP_CMD_LOAD_INDXA | 3:
			case CPCommand::CP_CMD_LOAD_INDXA | 4:
			case CPCommand::CP_CMD_LOAD_INDXA | 5:
			case CPCommand::CP_CMD_LOAD_INDXA | 6:
			case CPCommand::CP_CMD_LOAD_INDXA | 7:
				return GetSize() >= 5;

			case CPCommand::CP_CMD_LOAD_INDXB | 0:
			case CPCommand::CP_CMD_LOAD_INDXB | 1:
			case CPCommand::CP_CMD_LOAD_INDXB | 2:
			case CPCommand::CP_CMD_LOAD_INDXB | 3:
			case CPCommand::CP_CMD_LOAD_INDXB | 4:
			case CPCommand::CP_CMD_LOAD_INDXB | 5:
			case CPCommand::CP_CMD_LOAD_INDXB | 6:
			case CPCommand::CP_CMD_LOAD_INDXB | 7:
				return GetSize() >= 5;

			case CPCommand::CP_CMD_LOAD_INDXC | 0:
			case CPCommand::CP_CMD_LOAD_INDXC | 1:
			case CPCommand::CP_CMD_LOAD_INDXC | 2:
			case CPCommand::CP_CMD_LOAD_INDXC | 3:
			case CPCommand::CP_CMD_LOAD_INDXC | 4:
			case CPCommand::CP_CMD_LOAD_INDXC | 5:
			case CPCommand::CP_CMD_LOAD_INDXC | 6:
			case CPCommand::CP_CMD_LOAD_INDXC | 7:
				return GetSize() >= 5;

			case CPCommand::CP_CMD_LOAD_INDXD | 0:
			case CPCommand::CP_CMD_LOAD_INDXD | 1:
			case CPCommand::CP_CMD_LOAD_INDXD | 2:
			case CPCommand::CP_CMD_LOAD_INDXD | 3:
			case CPCommand::CP_CMD_LOAD_INDXD | 4:
			case CPCommand::CP_CMD_LOAD_INDXD | 5:
			case CPCommand::CP_CMD_LOAD_INDXD | 6:
			case CPCommand::CP_CMD_LOAD_INDXD | 7:
				return GetSize() >= 5;

			case CPCommand::CP_CMD_DRAW_QUAD | 0:
			case CPCommand::CP_CMD_DRAW_QUAD | 1:
			case CPCommand::CP_CMD_DRAW_QUAD | 2:
			case CPCommand::CP_CMD_DRAW_QUAD | 3:
			case CPCommand::CP_CMD_DRAW_QUAD | 4:
			case CPCommand::CP_CMD_DRAW_QUAD | 5:
			case CPCommand::CP_CMD_DRAW_QUAD | 6:
			case CPCommand::CP_CMD_DRAW_QUAD | 7:
			case CPCommand::CP_CMD_DRAW_QUAD_STRIP | 0:
			case CPCommand::CP_CMD_DRAW_QUAD_STRIP | 1:
			case CPCommand::CP_CMD_DRAW_QUAD_STRIP | 2:
			case CPCommand::CP_CMD_DRAW_QUAD_STRIP | 3:
			case CPCommand::CP_CMD_DRAW_QUAD_STRIP | 4:
			case CPCommand::CP_CMD_DRAW_QUAD_STRIP | 5:
			case CPCommand::CP_CMD_DRAW_QUAD_STRIP | 6:
			case CPCommand::CP_CMD_DRAW_QUAD_STRIP | 7:
			case CPCommand::CP_CMD_DRAW_TRIANGLE | 0:
			case CPCommand::CP_CMD_DRAW_TRIANGLE | 1:
			case CPCommand::CP_CMD_DRAW_TRIANGLE | 2:
			case CPCommand::CP_CMD_DRAW_TRIANGLE | 3:
			case CPCommand::CP_CMD_DRAW_TRIANGLE | 4:
			case CPCommand::CP_CMD_DRAW_TRIANGLE | 5:
			case CPCommand::CP_CMD_DRAW_TRIANGLE | 6:
			case CPCommand::CP_CMD_DRAW_TRIANGLE | 7:
			case CPCommand::CP_CMD_DRAW_STRIP | 0:
			case CPCommand::CP_CMD_DRAW_STRIP | 1:
			case CPCommand::CP_CMD_DRAW_STRIP | 2:
			case CPCommand::CP_CMD_DRAW_STRIP | 3:
			case CPCommand::CP_CMD_DRAW_STRIP | 4:
			case CPCommand::CP_CMD_DRAW_STRIP | 5:
			case CPCommand::CP_CMD_DRAW_STRIP | 6:
			case CPCommand::CP_CMD_DRAW_STRIP | 7:
			case CPCommand::CP_CMD_DRAW_FAN | 0:
			case CPCommand::CP_CMD_DRAW_FAN | 1:
			case CPCommand::CP_CMD_DRAW_FAN | 2:
			case CPCommand::CP_CMD_DRAW_FAN | 3:
			case CPCommand::CP_CMD_DRAW_FAN | 4:
			case CPCommand::CP_CMD_DRAW_FAN | 5:
			case CPCommand::CP_CMD_DRAW_FAN | 6:
			case CPCommand::CP_CMD_DRAW_FAN | 7:
			case CPCommand::CP_CMD_DRAW_LINE | 0:
			case CPCommand::CP_CMD_DRAW_LINE | 1:
			case CPCommand::CP_CMD_DRAW_LINE | 2:
			case CPCommand::CP_CMD_DRAW_LINE | 3:
			case CPCommand::CP_CMD_DRAW_LINE | 4:
			case CPCommand::CP_CMD_DRAW_LINE | 5:
			case CPCommand::CP_CMD_DRAW_LINE | 6:
			case CPCommand::CP_CMD_DRAW_LINE | 7:
			case CPCommand::CP_CMD_DRAW_LINESTRIP | 0:
			case CPCommand::CP_CMD_DRAW_LINESTRIP | 1:
			case CPCommand::CP_CMD_DRAW_LINESTRIP | 2:
			case CPCommand::CP_CMD_DRAW_LINESTRIP | 3:
			case CPCommand::CP_CMD_DRAW_LINESTRIP | 4:
			case CPCommand::CP_CMD_DRAW_LINESTRIP | 5:
			case CPCommand::CP_CMD_DRAW_LINESTRIP | 6:
			case CPCommand::CP_CMD_DRAW_LINESTRIP | 7:
			case CPCommand::CP_CMD_DRAW_POINT | 0:
			case CPCommand::CP_CMD_DRAW_POINT | 1:
			case CPCommand::CP_CMD_DRAW_POINT | 2:
			case CPCommand::CP_CMD_DRAW_POINT | 3:
			case CPCommand::CP_CMD_DRAW_POINT | 4:
			case CPCommand::CP_CMD_DRAW_POINT | 5:
			case CPCommand::CP_CMD_DRAW_POINT | 6:
			case CPCommand::CP_CMD_DRAW_POINT | 7:
			{
				if (GetSize() < 3)
					return false;

				int vtxnum = Peek16(1);
				return GetSize() >= (vtxnum * vertexSize[cmd & 7] + 3);
			}

			default:
			{
				Halt("GFX: Unsupported opcode: 0x%02X (%s, readPtr: 0x%x)\n", cmd, allocated ? "Call DL" : "Stream", readPtr);
				break;
			}
		}

		return false;
	}

	uint8_t FifoProcessor::Read8()
	{
		assert(GetSize() >= 1);

		lock.Lock();
		uint8_t value = fifo[readPtr++];
		if (readPtr >= fifoSize)
		{
			readPtr = 0;
		}
		lock.Unlock();
		return value;
	}

	uint16_t FifoProcessor::Read16()
	{
		assert(GetSize() >= 2);
		return ((uint16_t)Read8() << 8) | Read8();
	}

	uint32_t FifoProcessor::Read32()
	{
		assert(GetSize() >= 4);
		return ((uint32_t)Read8() << 24) | ((uint32_t)Read8() << 16) | ((uint32_t)Read8() << 8) | Read8();
	}

	float FifoProcessor::ReadFloat()
	{
		assert(GetSize() >= 4);
		uint32_t value = Read32();
		return *(float*)&value;
	}

	uint8_t FifoProcessor::Peek8(size_t offset)
	{
		lock.Lock();
		size_t ptr = readPtr + offset;
		if (ptr >= fifoSize)
		{
			ptr -= fifoSize;
		}
		lock.Unlock();
		return fifo[ptr];
	}

	uint8_t FifoProcessor::Peek16(size_t offset)
	{
		return ((uint16_t)Peek8(offset) << 8) | Peek8(offset + 1);
	}

	void FifoProcessor::RecalcVertexSize()
	{

	}

	void FifoProcessor::ExecuteCommand()
	{

	}

	#pragma endregion "FIFO Processing"


	// Helper function
	std::string CommandProcessor::AttrToString(VertexAttr attr)
	{
		switch (attr)
		{
			case VertexAttr::VTX_POS:           return "Position";
			case VertexAttr::VTX_NRM:           return "Normal";
			case VertexAttr::VTX_BINRM:         return "Binormal";
			case VertexAttr::VTX_TANGENT:       return "Tangent";
			case VertexAttr::VTX_COLOR0:        return "Color 0";
			case VertexAttr::VTX_COLOR1:        return "Color 1";
			case VertexAttr::VTX_TEXCOORD0:     return "Texture Coordinate 0";
			case VertexAttr::VTX_TEXCOORD1:     return "Texture Coordinate 1";
			case VertexAttr::VTX_TEXCOORD2:     return "Texture Coordinate 2";
			case VertexAttr::VTX_TEXCOORD3:     return "Texture Coordinate 3";
			case VertexAttr::VTX_TEXCOORD4:     return "Texture Coordinate 4";
			case VertexAttr::VTX_TEXCOORD5:     return "Texture Coordinate 5";
			case VertexAttr::VTX_TEXCOORD6:     return "Texture Coordinate 6";
			case VertexAttr::VTX_TEXCOORD7:		return "Texture Coordinate 7";
			case VertexAttr::VTX_MATIDX0:		return "Matrix Index 0";
			case VertexAttr::VTX_MATIDX1:		return "Matrix Index 1";
			case VertexAttr::VTX_MAX_ATTR:		return "MAX attr";
		}
		return "Unknown attribute";
	}

	// calculate size of current vertex
	int CommandProcessor::gx_vtxsize(unsigned v)
	{
		int vtxsize = 0;
		static int cntp[] = { 2, 3 };
		static int cntn[] = { 3, 9 };
		static int cntt[] = { 1, 2 };
		static int fmtsz[] = { 1, 1, 2, 2, 4 };
		static int cfmtsz[] = { 2, 3, 4, 2, 4, 4 };

		if (cp.vcdLo.PosNrmMatIdx) vtxsize++;
		if (cp.vcdLo.Tex0MatIdx) vtxsize++;
		if (cp.vcdLo.Tex1MatIdx) vtxsize++;
		if (cp.vcdLo.Tex2MatIdx) vtxsize++;
		if (cp.vcdLo.Tex3MatIdx) vtxsize++;
		if (cp.vcdLo.Tex4MatIdx) vtxsize++;
		if (cp.vcdLo.Tex5MatIdx) vtxsize++;
		if (cp.vcdLo.Tex6MatIdx) vtxsize++;
		if (cp.vcdLo.Tex7MatIdx) vtxsize++;

		// Position

		switch (cp.vcdLo.Position)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[cp.vatA[v].posfmt] * cntp[cp.vatA[v].poscnt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		// Normal

		switch (cp.vcdLo.Normal)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[cp.vatA[v].nrmfmt] * cntn[cp.vatA[v].nrmcnt];
				break;
			case VCD_INDEX8:
				if (cp.vatA[v].nrmidx3) vtxsize += 3;
				else vtxsize += 1;
				break;
			case VCD_INDEX16:
				if (cp.vatA[v].nrmidx3) vtxsize += 2 * 3;
				else vtxsize += 2;
				break;
		}

		// Colors

		switch (cp.vcdLo.Color0)
		{
			case VCD_DIRECT:
				vtxsize += cfmtsz[cp.vatA[v].col0fmt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		switch (cp.vcdLo.Color1)
		{
			case VCD_DIRECT:
				vtxsize += cfmtsz[cp.vatA[v].col1fmt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		// TexCoords

		switch (cp.vcdHi.Tex0Coord)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[cp.vatA[v].tex0fmt] * cntt[cp.vatA[v].tex0cnt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		switch (cp.vcdHi.Tex1Coord)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[cp.vatB[v].tex1fmt] * cntt[cp.vatB[v].tex1cnt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		switch (cp.vcdHi.Tex2Coord)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[cp.vatB[v].tex2fmt] * cntt[cp.vatB[v].tex2cnt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		switch (cp.vcdHi.Tex3Coord)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[cp.vatB[v].tex3fmt] * cntt[cp.vatB[v].tex3cnt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		switch (cp.vcdHi.Tex4Coord)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[cp.vatB[v].tex4fmt] * cntt[cp.vatB[v].tex4cnt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		switch (cp.vcdHi.Tex5Coord)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[cp.vatC[v].tex5fmt] * cntt[cp.vatC[v].tex5cnt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		switch (cp.vcdHi.Tex6Coord)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[cp.vatC[v].tex6fmt] * cntt[cp.vatC[v].tex6cnt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		switch (cp.vcdHi.Tex7Coord)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[cp.vatC[v].tex7fmt] * cntt[cp.vatC[v].tex7cnt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		return vtxsize;
	}

	void CommandProcessor::FifoReconfigure(FifoProcessor *gxfifo)
	{
		for (unsigned v = 0; v < 8; v++)
		{
			gxfifo->vertexSize[v] = gx_vtxsize(v);
		}
	}

	void * CommandProcessor::GetArrayPtr(ArrayId arrayId, int idx, int compSize)
	{
		uint32_t address = cp.arrayBase[(size_t)arrayId].Base + 
			(uint32_t)idx * cp.arrayStride[(size_t)arrayId].Stride;
		return HW->mem->MIGetMemoryPointerForCP(address);
	}

	void CommandProcessor::FetchComp(float* comp, int count, int type, int fmt, int shft, FifoProcessor* gxfifo, ArrayId arrayId)
	{
		void* ptr;
		static int fmtsz[] = { 1, 1, 2, 2, 4 };

		union
		{
			uint8_t u8[3];
			uint16_t u16[3];
			int8_t s8[3];
			int16_t s16[3];
			uint32_t u32[3];
		} Comp;

		switch (type)
		{
			case VCD_NONE:      // Skip attribute
				return;
			case VCD_INDEX8:
				ptr = GetArrayPtr(arrayId, gxfifo->Read8(), fmtsz[fmt]);
				break;
			case VCD_INDEX16:
				ptr = GetArrayPtr(arrayId, gxfifo->Read16(), fmtsz[fmt]);
				break;
			default:
				ptr = nullptr;
				break;
		}

		switch (fmt)
		{
			case VFMT_U8:
				if (type == VCD_DIRECT)
				{
					for (int i = 0; i < count; i++)
					{
						Comp.u8[i] = gxfifo->Read8();
					}
				}
				else
				{
					for (int i = 0; i < count; i++)
					{
						Comp.u8[i] = ((uint8_t*)ptr)[i];
					}
				}

				for (int i = 0; i < count; i++)
				{
					comp[i] = (float)(Comp.u8[i]) / (float)pow(2.0, shft);
				}
				break;

			case VFMT_S8:
				if (type == VCD_DIRECT)
				{
					for (int i = 0; i < count; i++)
					{
						Comp.s8[i] = gxfifo->Read8();
					}
				}
				else
				{
					for (int i = 0; i < count; i++)
					{
						Comp.s8[i] = ((uint8_t*)ptr)[i];
					}
				}

				for (int i = 0; i < count; i++)
				{
					comp[i] = (float)(Comp.s8[i]) / (float)pow(2.0, shft);
				}
				break;

			case VFMT_U16:
				if (type == VCD_DIRECT)
				{
					for (int i = 0; i < count; i++)
					{
						Comp.u16[i] = gxfifo->Read16();
					}
				}
				else
				{
					for (int i = 0; i < count; i++)
					{
						Comp.u16[i] = _BYTESWAP_UINT16(((uint16_t*)ptr)[i]);
					}
				}

				for (int i = 0; i < count; i++)
				{
					comp[i] = (float)(Comp.u16[i]) / (float)pow(2.0, shft);
				}
				break;

			case VFMT_S16:
				if (type == VCD_DIRECT)
				{
					for (int i = 0; i < count; i++)
					{
						Comp.s16[i] = gxfifo->Read16();
					}
				}
				else
				{
					for (int i = 0; i < count; i++)
					{
						Comp.s16[i] = _BYTESWAP_UINT16(((uint16_t*)ptr)[i]);
					}
				}

				for (int i = 0; i < count; i++)
				{
					comp[i] = (float)(Comp.s16[i]) / (float)pow(2.0, shft);
				}
				break;

			case VFMT_F32:
				if (type == VCD_DIRECT)
				{
					for (int i = 0; i < count; i++)
					{
						Comp.u32[i] = gxfifo->Read32();
					}
				}
				else
				{
					for (int i = 0; i < count; i++)
					{
						Comp.u32[i] = _BYTESWAP_UINT32(((uint32_t*)ptr)[i]);
					}
				}

				for (int i = 0; i < count; i++)
				{
					comp[i] = *(float*)&Comp.u32[i];
				}
				break;

			default:
				Halt("FetchComp: Invalid combination of VAT settings\n");
				break;
		}
	}

	void CommandProcessor::FetchNorm(float* comp, int count, int type, int fmt, int shft, FifoProcessor* gxfifo, ArrayId arrayId, bool nrmidx3)
	{
		void* ptr1;
		void* ptr2;
		void* ptr3;

		void** ptrptr[3] = { &ptr1, &ptr2, &ptr3 };
		static int fmtsz[] = { 1, 1, 2, 2, 4 };

		union
		{
			uint8_t u8[9];
			uint16_t u16[9];
			int8_t s8[9];
			int16_t s16[9];
			uint32_t u32[9];
		} Comp;

		switch (type)
		{
			case VCD_NONE:      // Skip attribute
				return;
			case VCD_INDEX8:
				ptr1 = GetArrayPtr(arrayId, gxfifo->Read8(), fmtsz[fmt]);
				if (count == 9 && nrmidx3)
				{
					ptr2 = GetArrayPtr(arrayId, gxfifo->Read8(), fmtsz[fmt]);
					ptr3 = GetArrayPtr(arrayId, gxfifo->Read8(), fmtsz[fmt]);
				}
				break;
			case VCD_INDEX16:
				ptr1 = GetArrayPtr(arrayId, gxfifo->Read16(), fmtsz[fmt]);
				if (count == 9 && nrmidx3)
				{
					ptr2 = GetArrayPtr(arrayId, gxfifo->Read16(), fmtsz[fmt]);
					ptr3 = GetArrayPtr(arrayId, gxfifo->Read16(), fmtsz[fmt]);
				}
				break;
		}

		switch (fmt)
		{
			case VFMT_S8:
				if (type == VCD_DIRECT)
				{
					for (int i = 0; i < count; i++)
					{
						Comp.s8[i] = gxfifo->Read8();
					}
				}
				else
				{
					for (int i = 0; i < count; i++)
					{
						void* ptr;
						if (count == 9 && nrmidx3)
						{
							ptr = *ptrptr[i / 3];
						}
						else
						{
							ptr = ptr1;
						}
						Comp.s8[i] = ((uint8_t*)ptr)[i];
					}
				}

				for (int i = 0; i < count; i++)
				{
					comp[i] = (float)(Comp.s8[i]) / (float)pow(2.0, shft);
				}
				break;

			case VFMT_S16:
				if (type == VCD_DIRECT)
				{
					for (int i = 0; i < count; i++)
					{
						Comp.s16[i] = gxfifo->Read16();
					}
				}
				else
				{
					for (int i = 0; i < count; i++)
					{
						void* ptr;
						if (count == 9 && nrmidx3)
						{
							ptr = *ptrptr[i / 3];
						}
						else
						{
							ptr = ptr1;
						}
						Comp.s16[i] = _BYTESWAP_UINT16(((uint16_t*)ptr)[i]);
					}
				}

				for (int i = 0; i < count; i++)
				{
					comp[i] = (float)(Comp.s16[i]) / (float)pow(2.0, shft);
				}
				break;

			case VFMT_F32:
				if (type == VCD_DIRECT)
				{
					for (int i = 0; i < count; i++)
					{
						Comp.u32[i] = gxfifo->Read32();
					}
				}
				else
				{
					for (int i = 0; i < count; i++)
					{
						void* ptr;
						if (count == 9 && nrmidx3)
						{
							ptr = *ptrptr[i / 3];
						}
						else
						{
							ptr = ptr1;
						}
						Comp.u32[i] = _BYTESWAP_UINT32(((uint32_t*)ptr)[i]);
					}
				}

				for (int i = 0; i < count; i++)
				{
					comp[i] = *(float*)&Comp.u32[i];
				}
				break;

			default:
				Halt("FetchComp: Invalid combination of VAT settings (normals)\n");
				break;
		}
	}

	GFX::Color CommandProcessor::FetchColor(int type, int fmt, FifoProcessor* gxfifo, ArrayId arrayId)
	{
		void* ptr;
		GFX::Color col{};
		static int cfmtsz[] = { 2, 3, 4, 2, 4, 4 };

		col.R = 0;
		col.G = 0;
		col.B = 0;
		col.A = 255;

		uint16_t p16;
		uint32_t p32;

		uint8_t r, g, b, a;

		switch (type)
		{
			case VCD_NONE:      // Skip attribute
				return col;
			case VCD_INDEX8:
				ptr = GetArrayPtr(arrayId, gxfifo->Read8(), cfmtsz[fmt]);
				break;
			case VCD_INDEX16:
				ptr = GetArrayPtr(arrayId, gxfifo->Read16(), cfmtsz[fmt]);
				break;
			default:
				ptr = nullptr;
				break;
		}

		switch (fmt)
		{
			case VFMT_RGB565:

				if (type == VCD_DIRECT)
				{
					p16 = gxfifo->Read16();
				}
				else
				{
					p16 = _BYTESWAP_UINT16(((uint16_t*)ptr)[0]);
				}

				r = p16 >> 11;
				g = (p16 >> 5) & 0x3f;
				b = p16 & 0x1f;

				col.R = (r << 3) | (r >> 2);
				col.G = (g << 2) | (g >> 4);
				col.B = (b << 3) | (b >> 2);
				col.A = 255;

				break;

			case VFMT_RGB8:
				if (type == VCD_DIRECT)
				{
					col.R = gxfifo->Read8();
					col.G = gxfifo->Read8();
					col.B = gxfifo->Read8();
				}
				else
				{
					col.R = ((uint8_t*)ptr)[0];
					col.G = ((uint8_t*)ptr)[1];
					col.B = ((uint8_t*)ptr)[2];
				}
				col.A = 255;
				break;

			case VFMT_RGBX8:
				if (type == VCD_DIRECT)
				{
					col.R = gxfifo->Read8();
					col.G = gxfifo->Read8();
					col.B = gxfifo->Read8();
					gxfifo->Read8();
				}
				else
				{
					col.R = ((uint8_t*)ptr)[0];
					col.G = ((uint8_t*)ptr)[1];
					col.B = ((uint8_t*)ptr)[2];
				}
				col.A = 255;
				break;

			case VFMT_RGBA4:

				if (type == VCD_DIRECT)
				{
					p16 = gxfifo->Read16();
				}
				else
				{
					p16 = _BYTESWAP_UINT16(((uint16_t*)ptr)[0]);
				}

				r = (p16 >> 12) & 0xf;
				g = (p16 >> 8) & 0xf;
				b = (p16 >> 4) & 0xf;
				a = (p16 >> 0) & 0xf;

				col.R = (r << 4) | r;
				col.G = (g << 4) | g;
				col.B = (b << 4) | b;
				col.A = (a << 4) | a;

				break;

			case VFMT_RGBA6:

				if (type == VCD_DIRECT)
				{
					p32 = ((uint32_t)gxfifo->Read8() << 16) | ((uint32_t)gxfifo->Read8() << 8) | gxfifo->Read8();
				}
				else
				{
					p32 = ((uint32_t)((uint8_t*)ptr)[0] << 16) | ((uint32_t)((uint8_t*)ptr)[1] << 8) | ((uint8_t*)ptr)[2];
				}

				r = (p32 >> 18) & 0x3f;
				g = (p32 >> 12) & 0x3f;
				b = (p32 >> 6) & 0x3f;
				a = (p32 >> 0) & 0x3f;

				col.R = (r << 6) | r;
				col.G = (g << 6) | g;
				col.B = (b << 6) | b;
				col.A = (a << 6) | a;

				break;

			case VFMT_RGBA8:

				if (type == VCD_DIRECT)
				{
					col.R = gxfifo->Read8();
					col.G = gxfifo->Read8();
					col.B = gxfifo->Read8();
					col.A = gxfifo->Read8();
				}
				else
				{
					col.R = ((uint8_t*)ptr)[0];
					col.G = ((uint8_t*)ptr)[1];
					col.B = ((uint8_t*)ptr)[2];
					col.A = ((uint8_t*)ptr)[3];
				}

				break;

			default:
				Halt("FetchComp: Invalid combination of VAT settings (color)\n");
				break;
		}

		return col;
	}

	// collect vertex data
	void CommandProcessor::FifoWalk(unsigned vatnum, GFX::Vertex* vtx, FifoProcessor* gxfifo)
	{
		// overrided by 'mtxidx' attributes
		vtx->matIdx0.PosNrmMatIdx = HW->gfx->xf->xf.matIdxA.PosNrmMatIdx;
		vtx->matIdx0.Tex0MatIdx = HW->gfx->xf->xf.matIdxA.Tex0MatIdx;
		vtx->matIdx0.Tex1MatIdx = HW->gfx->xf->xf.matIdxA.Tex1MatIdx;
		vtx->matIdx0.Tex2MatIdx = HW->gfx->xf->xf.matIdxA.Tex2MatIdx;
		vtx->matIdx0.Tex3MatIdx = HW->gfx->xf->xf.matIdxA.Tex3MatIdx;
		vtx->matIdx1.Tex4MatIdx = HW->gfx->xf->xf.matIdxB.Tex4MatIdx;
		vtx->matIdx1.Tex5MatIdx = HW->gfx->xf->xf.matIdxB.Tex5MatIdx;
		vtx->matIdx1.Tex6MatIdx = HW->gfx->xf->xf.matIdxB.Tex6MatIdx;
		vtx->matIdx1.Tex7MatIdx = HW->gfx->xf->xf.matIdxB.Tex7MatIdx;

		// Matrix Index

		if (cp.vcdLo.PosNrmMatIdx)
		{
			vtx->matIdx0.PosNrmMatIdx = gxfifo->Read8();
		}

		if (cp.vcdLo.Tex0MatIdx)
		{
			vtx->matIdx0.Tex0MatIdx = gxfifo->Read8();
		}

		if (cp.vcdLo.Tex1MatIdx)
		{
			vtx->matIdx0.Tex1MatIdx = gxfifo->Read8();
		}

		if (cp.vcdLo.Tex2MatIdx)
		{
			vtx->matIdx0.Tex2MatIdx = gxfifo->Read8();
		}

		if (cp.vcdLo.Tex3MatIdx)
		{
			vtx->matIdx0.Tex3MatIdx = gxfifo->Read8();
		}

		if (cp.vcdLo.Tex4MatIdx)
		{
			vtx->matIdx1.Tex4MatIdx = gxfifo->Read8();
		}

		if (cp.vcdLo.Tex5MatIdx)
		{
			vtx->matIdx1.Tex5MatIdx = gxfifo->Read8();
		}

		if (cp.vcdLo.Tex6MatIdx)
		{
			vtx->matIdx1.Tex6MatIdx = gxfifo->Read8();
		}

		if (cp.vcdLo.Tex7MatIdx)
		{
			vtx->matIdx1.Tex7MatIdx = gxfifo->Read8();
		}

		// Position

		vtx->Position[0] = vtx->Position[1] = vtx->Position[2] = 1.0f;

		FetchComp(vtx->Position,
			cp.vatA[vatnum].poscnt == VCNT_POS_XYZ ? 3 : 2,
			cp.vcdLo.Position,
			cp.vatA[vatnum].posfmt,
			cp.vatA[vatnum].bytedeq ? cp.vatA[vatnum].posshft : 0,
			gxfifo,
			ArrayId::Pos);

		// Normal

		vtx->Normal[0] = vtx->Normal[1] = vtx->Normal[2] = 1.0f;
		vtx->Binormal[0] = vtx->Binormal[1] = vtx->Binormal[2] = 1.0f;
		vtx->Tangent[0] = vtx->Tangent[1] = vtx->Tangent[2] = 1.0f;

		int nrmshft = 0;

		switch (cp.vatA[vatnum].nrmfmt)
		{
			case VFMT_U8:
			case VFMT_S8:
				nrmshft = 6;
				break;
			case VFMT_U16:
			case VFMT_S16:
				nrmshft = 14;
				break;
		}

		FetchNorm(vtx->Normal,
			cp.vatA[vatnum].nrmcnt == VCNT_NRM_NBT ? 9 : 3,
			cp.vcdLo.Normal,
			cp.vatA[vatnum].nrmfmt,
			nrmshft,
			gxfifo,
			ArrayId::Nrm,
			cp.vatA[vatnum].nrmidx3 ? true : false);

		// Color0 

		vtx->Col[0] = FetchColor(cp.vcdLo.Color0, cp.vatA[vatnum].col0fmt, gxfifo, ArrayId::Color0);

		// Color1

		vtx->Col[1] = FetchColor(cp.vcdLo.Color1, cp.vatA[vatnum].col1fmt, gxfifo, ArrayId::Color1);

		// TexNCoord

		vtx->TexCoord[0][0] = vtx->TexCoord[0][1] = 1.0f;

		FetchComp(vtx->TexCoord[0],
			cp.vatA[vatnum].tex0cnt == VCNT_TEX_ST ? 2 : 1,
			cp.vcdHi.Tex0Coord,
			cp.vatA[vatnum].tex0fmt,
			cp.vatA[vatnum].bytedeq ? cp.vatA[vatnum].tex0shft : 0,
			gxfifo,
			ArrayId::Tex0Coord);

		vtx->TexCoord[1][0] = vtx->TexCoord[1][1] = 1.0f;

		FetchComp(vtx->TexCoord[1],
			cp.vatB[vatnum].tex1cnt == VCNT_TEX_ST ? 2 : 1,
			cp.vcdHi.Tex1Coord,
			cp.vatB[vatnum].tex1fmt,
			cp.vatA[vatnum].bytedeq ? cp.vatB[vatnum].tex1shft : 0,
			gxfifo,
			ArrayId::Tex1Coord);

		vtx->TexCoord[2][0] = vtx->TexCoord[2][1] = 1.0f;

		FetchComp(vtx->TexCoord[2],
			cp.vatB[vatnum].tex2cnt == VCNT_TEX_ST ? 2 : 1,
			cp.vcdHi.Tex2Coord,
			cp.vatB[vatnum].tex2fmt,
			cp.vatA[vatnum].bytedeq ? cp.vatB[vatnum].tex2shft : 0,
			gxfifo,
			ArrayId::Tex2Coord);

		vtx->TexCoord[3][0] = vtx->TexCoord[3][1] = 1.0f;

		FetchComp(vtx->TexCoord[3],
			cp.vatB[vatnum].tex3cnt == VCNT_TEX_ST ? 2 : 1,
			cp.vcdHi.Tex3Coord,
			cp.vatB[vatnum].tex3fmt,
			cp.vatA[vatnum].bytedeq ? cp.vatB[vatnum].tex3shft : 0,
			gxfifo,
			ArrayId::Tex3Coord);

		vtx->TexCoord[4][0] = vtx->TexCoord[4][1] = 1.0f;

		FetchComp(vtx->TexCoord[4],
			cp.vatB[vatnum].tex4cnt == VCNT_TEX_ST ? 2 : 1,
			cp.vcdHi.Tex4Coord,
			cp.vatB[vatnum].tex4fmt,
			cp.vatA[vatnum].bytedeq ? cp.vatC[vatnum].tex4shft : 0,
			gxfifo,
			ArrayId::Tex4Coord);

		vtx->TexCoord[5][0] = vtx->TexCoord[5][1] = 1.0f;

		FetchComp(vtx->TexCoord[5],
			cp.vatC[vatnum].tex5cnt == VCNT_TEX_ST ? 2 : 1,
			cp.vcdHi.Tex5Coord,
			cp.vatC[vatnum].tex5fmt,
			cp.vatA[vatnum].bytedeq ? cp.vatC[vatnum].tex5shft : 0,
			gxfifo,
			ArrayId::Tex5Coord);

		vtx->TexCoord[6][0] = vtx->TexCoord[6][1] = 1.0f;

		FetchComp(vtx->TexCoord[6],
			cp.vatC[vatnum].tex6cnt == VCNT_TEX_ST ? 2 : 1,
			cp.vcdHi.Tex6Coord,
			cp.vatC[vatnum].tex6fmt,
			cp.vatA[vatnum].bytedeq ? cp.vatC[vatnum].tex6shft : 0,
			gxfifo,
			ArrayId::Tex6Coord);

		vtx->TexCoord[7][0] = vtx->TexCoord[7][1] = 1.0f;

		FetchComp(vtx->TexCoord[7],
			cp.vatC[vatnum].tex7cnt == VCNT_TEX_ST ? 2 : 1,
			cp.vcdHi.Tex7Coord,
			cp.vatC[vatnum].tex7fmt,
			cp.vatA[vatnum].bytedeq ? cp.vatC[vatnum].tex7shft : 0,
			gxfifo,
			ArrayId::Tex7Coord);
	}

	void CommandProcessor::GxBadFifo(uint8_t command)
	{
		Halt(
			"Unimplemented command : 0x%02X\n"
			"VCD configuration :\n"
			"pmidx:%i\n"
			"t0idx:%i\t tex0:%i\n"
			"t1idx:%i\t tex1:%i\n"
			"t2idx:%i\t tex2:%i\n"
			"t3idx:%i\t tex3:%i\n"
			"t4idx:%i\t tex4:%i\n"
			"t5idx:%i\t tex5:%i\n"
			"t6idx:%i\t tex6:%i\n"
			"t7idx:%i\t tex7:%i\n"
			"pos:%i\n"
			"nrm:%i\n"
			"col0:%i\n"
			"col1:%i\n",
			command,
			cp.vcdLo.PosNrmMatIdx,
			cp.vcdLo.Tex0MatIdx, cp.vcdHi.Tex0Coord,
			cp.vcdLo.Tex1MatIdx, cp.vcdHi.Tex1Coord,
			cp.vcdLo.Tex2MatIdx, cp.vcdHi.Tex2Coord,
			cp.vcdLo.Tex3MatIdx, cp.vcdHi.Tex3Coord,
			cp.vcdLo.Tex4MatIdx, cp.vcdHi.Tex4Coord,
			cp.vcdLo.Tex5MatIdx, cp.vcdHi.Tex5Coord,
			cp.vcdLo.Tex6MatIdx, cp.vcdHi.Tex6Coord,
			cp.vcdLo.Tex7MatIdx, cp.vcdHi.Tex7Coord,
			cp.vcdLo.Position,
			cp.vcdLo.Normal,
			cp.vcdLo.Color0,
			cp.vcdLo.Color1
		);
	}

	void CommandProcessor::GxCommand(FifoProcessor* gxfifo)
	{
#if GFX_BLACKJACK_AND_SHADERS
		GLenum gl_error;
#endif

		HW->gfx->GPFrameBegin();

		uint8_t cmd = gxfifo->Read8();

		if (logOpcode) {
			Report(Channel::GP, "GxCommand: 0x%02X\n", cmd);
		}

		switch(cmd)
		{
			// do nothing
			case CP_CMD_NOP | 0:
			case CP_CMD_NOP | 1:
			case CP_CMD_NOP | 2:
			case CP_CMD_NOP | 3:
			case CP_CMD_NOP | 4:
			case CP_CMD_NOP | 5:
			case CP_CMD_NOP | 6:
			case CP_CMD_NOP | 7:
				break;

			case CP_CMD_VCACHE_INVD | 0:
			case CP_CMD_VCACHE_INVD | 1:
			case CP_CMD_VCACHE_INVD | 2:
			case CP_CMD_VCACHE_INVD | 3:
			case CP_CMD_VCACHE_INVD | 4:
			case CP_CMD_VCACHE_INVD | 5:
			case CP_CMD_VCACHE_INVD | 6:
			case CP_CMD_VCACHE_INVD | 7:
				//Report(Channel::GP, "Invalidate V$\n");
				break;

			case CP_CMD_CALL_DL | 0:
			case CP_CMD_CALL_DL | 1:
			case CP_CMD_CALL_DL | 2:
			case CP_CMD_CALL_DL | 3:
			case CP_CMD_CALL_DL | 4:
			case CP_CMD_CALL_DL | 5:
			case CP_CMD_CALL_DL | 6:
			case CP_CMD_CALL_DL | 7:
			{
				uint32_t physAddress = gxfifo->Read32() & 0x03ffffe0;
				uint8_t* fifoPtr = (uint8_t *)HW->mem->MIGetMemoryPointerForCP(physAddress);
				size_t size = gxfifo->Read32() & ~0x1f;

				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_CALL_DL: addr: 0x%08X, size: %i\n", physAddress, size);
				}

				FifoProcessor* callDlFifo = new FifoProcessor(fifoPtr, size);

				while (callDlFifo->EnoughToExecute())
				{
					GxCommand(callDlFifo);
				}

				delete callDlFifo;
				break;
			}

			// ---------------------------------------------------------------
			// loading of internal regs
			
			case CP_CMD_LOAD_BPREG | 0:
			case CP_CMD_LOAD_BPREG | 1:
			case CP_CMD_LOAD_BPREG | 2:
			case CP_CMD_LOAD_BPREG | 3:
			case CP_CMD_LOAD_BPREG | 4:
			case CP_CMD_LOAD_BPREG | 5:
			case CP_CMD_LOAD_BPREG | 6:
			case CP_CMD_LOAD_BPREG | 7:
			case CP_CMD_LOAD_BPREG | 8:
			case CP_CMD_LOAD_BPREG | 0xa:
			case CP_CMD_LOAD_BPREG | 0xb:
			case CP_CMD_LOAD_BPREG | 0xc:
			case CP_CMD_LOAD_BPREG | 0xd:
			case CP_CMD_LOAD_BPREG | 0xe:
			case CP_CMD_LOAD_BPREG | 0xf:
			{
				uint32_t word = gxfifo->Read32();

				bpLoads++;

				size_t index = word >> 24;
				uint32_t value = word & 0xffffff;

				if (GpRegsLog)
				{
					Report(Channel::GP, "Load reg: index: 0x%02X, data: 0x%08X\n", index, value);
				}

				HW->gfx->su->loadSUReg(index, value);
				break;
			}

			case CP_CMD_LOAD_CPREG | 0:
			case CP_CMD_LOAD_CPREG | 1:
			case CP_CMD_LOAD_CPREG | 2:
			case CP_CMD_LOAD_CPREG | 3:
			case CP_CMD_LOAD_CPREG | 4:
			case CP_CMD_LOAD_CPREG | 5:
			case CP_CMD_LOAD_CPREG | 6:
			case CP_CMD_LOAD_CPREG | 7:
			{
				uint8_t index = gxfifo->Read8();
				uint32_t word = gxfifo->Read32();
				loadCPReg(index, word, gxfifo);
				break;
			}

			case CP_CMD_LOAD_XFREG | 0:
			case CP_CMD_LOAD_XFREG | 1:
			case CP_CMD_LOAD_XFREG | 2:
			case CP_CMD_LOAD_XFREG | 3:
			case CP_CMD_LOAD_XFREG | 4:
			case CP_CMD_LOAD_XFREG | 5:
			case CP_CMD_LOAD_XFREG | 6:
			case CP_CMD_LOAD_XFREG | 7:
			{
				uint16_t len, index;

				len = gxfifo->Read16() + 1;
				index = gxfifo->Read16();

				xfLoads += (uint32_t)len;

				if (GpRegsLog)
				{
					Report(Channel::GP, "XF load, start index: %04X, n : %i\n", index, len);
				}

				HW->gfx->xf->loadXFRegs(index, len, gxfifo);
				break;
			}

			case CP_CMD_LOAD_INDXA | 0:
			case CP_CMD_LOAD_INDXA | 1:
			case CP_CMD_LOAD_INDXA | 2:
			case CP_CMD_LOAD_INDXA | 3:
			case CP_CMD_LOAD_INDXA | 4:
			case CP_CMD_LOAD_INDXA | 5:
			case CP_CMD_LOAD_INDXA | 6:
			case CP_CMD_LOAD_INDXA | 7:
			{
				uint16_t idx, start, len;
				idx = gxfifo->Read16();
				start = gxfifo->Read16();
				len = (start >> 12) + 1;
				start &= 0xfff;
				Report(Channel::GP, "CP_CMD_LOAD_INDXA: idx: %i, start: %i, len: %i\n", idx, start, len);
				break;
			}

			case CP_CMD_LOAD_INDXB | 0:
			case CP_CMD_LOAD_INDXB | 1:
			case CP_CMD_LOAD_INDXB | 2:
			case CP_CMD_LOAD_INDXB | 3:
			case CP_CMD_LOAD_INDXB | 4:
			case CP_CMD_LOAD_INDXB | 5:
			case CP_CMD_LOAD_INDXB | 6:
			case CP_CMD_LOAD_INDXB | 7:
			{
				uint16_t idx, start, len;
				idx = gxfifo->Read16();
				start = gxfifo->Read16();
				len = (start >> 12) + 1;
				start &= 0xfff;
				Report(Channel::GP, "CP_CMD_LOAD_INDXB: idx: %i, start: %i, len: %i\n", idx, start, len);
				break;
			}

			case CP_CMD_LOAD_INDXC | 0:
			case CP_CMD_LOAD_INDXC | 1:
			case CP_CMD_LOAD_INDXC | 2:
			case CP_CMD_LOAD_INDXC | 3:
			case CP_CMD_LOAD_INDXC | 4:
			case CP_CMD_LOAD_INDXC | 5:
			case CP_CMD_LOAD_INDXC | 6:
			case CP_CMD_LOAD_INDXC | 7:
			{
				uint16_t idx, start, len;
				idx = gxfifo->Read16();
				start = gxfifo->Read16();
				len = (start >> 12) + 1;
				start &= 0xfff;
				Report(Channel::GP, "CP_CMD_LOAD_INDXC: idx: %i, start: %i, len: %i\n", idx, start, len);
				break;
			}

			case CP_CMD_LOAD_INDXD | 0:
			case CP_CMD_LOAD_INDXD | 1:
			case CP_CMD_LOAD_INDXD | 2:
			case CP_CMD_LOAD_INDXD | 3:
			case CP_CMD_LOAD_INDXD | 4:
			case CP_CMD_LOAD_INDXD | 5:
			case CP_CMD_LOAD_INDXD | 6:
			case CP_CMD_LOAD_INDXD | 7:
			{
				uint16_t idx, start, len;
				idx = gxfifo->Read16();
				start = gxfifo->Read16();
				len = (start >> 12) + 1;
				start &= 0xfff;
				Report(Channel::GP, "CP_CMD_LOAD_INDXD: idx: %i, start: %i, len: %i\n", idx, start, len);
				break;
			}

			// ---------------------------------------------------------------
			// draw commands

			// 0x80
			case CP_CMD_DRAW_QUAD | 0:
			case CP_CMD_DRAW_QUAD | 1:
			case CP_CMD_DRAW_QUAD | 2:
			case CP_CMD_DRAW_QUAD | 3:
			case CP_CMD_DRAW_QUAD | 4:
			case CP_CMD_DRAW_QUAD | 5:
			case CP_CMD_DRAW_QUAD | 6:
			case CP_CMD_DRAW_QUAD | 7:
			{
				unsigned vatnum = cmd & 7;
				unsigned vtxnum = gxfifo->Read16();
				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_DRAW_QUAD: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
				}

				if (vtxnum != 0) {
					tris += (vtxnum / 4) / 2;

#if GFX_BLACKJACK_AND_SHADERS
					for (unsigned i = 0; i < vtxnum; i++) {
						FifoWalk(vatnum, &vertex_data[i], gxfifo);
					}
					glBufferData(GL_ARRAY_BUFFER, vtxnum * sizeof(Vertex), vertex_data, GL_STATIC_DRAW);
					glDrawArrays(GL_QUADS, 0, vtxnum);
					gl_error = glGetError();
					if (gl_error != GL_NO_ERROR) {
						Halt("GL Error CP_CMD_DRAW_QUAD: %x\n", gl_error);
					}
#else
					HW->gfx->ras->RAS_Begin(GFX::RAS_QUAD, vtxnum);
					GFX::Vertex vtx;
					while (vtxnum--) {

						FifoWalk(vatnum, &vtx, gxfifo);
						HW->gfx->ras->RAS_SendVertex(&vtx);
					}
					HW->gfx->ras->RAS_End();
#endif
				}
				break;
			}

			// 0x88
			case CP_CMD_DRAW_QUAD_STRIP | 0:
			case CP_CMD_DRAW_QUAD_STRIP | 1:
			case CP_CMD_DRAW_QUAD_STRIP | 2:
			case CP_CMD_DRAW_QUAD_STRIP | 3:
			case CP_CMD_DRAW_QUAD_STRIP | 4:
			case CP_CMD_DRAW_QUAD_STRIP | 5:
			case CP_CMD_DRAW_QUAD_STRIP | 6:
			case CP_CMD_DRAW_QUAD_STRIP | 7:
			{
				unsigned vatnum = cmd & 7;
				unsigned vtxnum = gxfifo->Read16();
				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_DRAW_QUAD_STRIP: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
				}

				if (vtxnum != 0) {
					tris += (vtxnum / 2 - 1) / 2;

#if GFX_BLACKJACK_AND_SHADERS
					for (unsigned i = 0; i < vtxnum; i++) {
						FifoWalk(vatnum, &vertex_data[i], gxfifo);
					}
					glBufferData(GL_ARRAY_BUFFER, vtxnum * sizeof(Vertex), vertex_data, GL_STATIC_DRAW);
					glDrawArrays(GL_QUAD_STRIP, 0, vtxnum);
					gl_error = glGetError();
					if (gl_error != GL_NO_ERROR) {
						Halt("GL Error CP_CMD_DRAW_QUAD: %x\n", gl_error);
					}
#else
					HW->gfx->ras->RAS_Begin(GFX::RAS_QUAD_STRIP, vtxnum);
					GFX::Vertex vtx;
					while (vtxnum--) {

						FifoWalk(vatnum, &vtx, gxfifo);
						HW->gfx->ras->RAS_SendVertex(&vtx);
					}
					HW->gfx->ras->RAS_End();
#endif
				}
				break;
			}

			// 0x90
			case CP_CMD_DRAW_TRIANGLE | 0:
			case CP_CMD_DRAW_TRIANGLE | 1:
			case CP_CMD_DRAW_TRIANGLE | 2:
			case CP_CMD_DRAW_TRIANGLE | 3:
			case CP_CMD_DRAW_TRIANGLE | 4:
			case CP_CMD_DRAW_TRIANGLE | 5:
			case CP_CMD_DRAW_TRIANGLE | 6:
			case CP_CMD_DRAW_TRIANGLE | 7:
			{
				unsigned vatnum = cmd & 7;
				unsigned vtxnum = gxfifo->Read16();
				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_DRAW_TRIANGLE: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
				}

				if (vtxnum != 0) {
					tris += vtxnum / 3;

#if GFX_BLACKJACK_AND_SHADERS
					for (unsigned i = 0; i < vtxnum; i++) {
						FifoWalk(vatnum, &vertex_data[i], gxfifo);
					}
					glBufferData(GL_ARRAY_BUFFER, vtxnum * sizeof(Vertex), vertex_data, GL_STATIC_DRAW);
					glDrawArrays(GL_TRIANGLES, 0, vtxnum);
					gl_error = glGetError();
					if (gl_error != GL_NO_ERROR) {
						Halt("GL Error CP_CMD_DRAW_TRIANGLE: %x\n", gl_error);
					}
#else
					HW->gfx->ras->RAS_Begin(GFX::RAS_TRIANGLE, vtxnum);
					GFX::Vertex vtx;
					while (vtxnum--) {

						FifoWalk(vatnum, &vtx, gxfifo);
						HW->gfx->ras->RAS_SendVertex(&vtx);
					}
					HW->gfx->ras->RAS_End();
#endif
				}
				break;
			}

			// 0x98 
			case CP_CMD_DRAW_STRIP | 0:
			case CP_CMD_DRAW_STRIP | 1:
			case CP_CMD_DRAW_STRIP | 2:
			case CP_CMD_DRAW_STRIP | 3:
			case CP_CMD_DRAW_STRIP | 4:
			case CP_CMD_DRAW_STRIP | 5:
			case CP_CMD_DRAW_STRIP | 6:
			case CP_CMD_DRAW_STRIP | 7:
			{
				unsigned vatnum = cmd & 7;
				unsigned vtxnum = gxfifo->Read16();
				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_DRAW_STRIP: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
				}

				if (vtxnum != 0) {
					tris += vtxnum - 2;

#if GFX_BLACKJACK_AND_SHADERS
					for (unsigned i = 0; i < vtxnum; i++) {
						FifoWalk(vatnum, &vertex_data[i], gxfifo);
					}
					glBufferData(GL_ARRAY_BUFFER, vtxnum * sizeof(Vertex), vertex_data, GL_STATIC_DRAW);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, vtxnum);
					gl_error = glGetError();
					if (gl_error != GL_NO_ERROR) {
						Halt("GL Error CP_CMD_DRAW_STRIP: %x\n", gl_error);
					}
#else
					HW->gfx->ras->RAS_Begin(GFX::RAS_TRIANGLE_STRIP, vtxnum);
					GFX::Vertex vtx;
					while (vtxnum--) {

						FifoWalk(vatnum, &vtx, gxfifo);
						HW->gfx->ras->RAS_SendVertex(&vtx);
					}
					HW->gfx->ras->RAS_End();
#endif
				}
				break;
			}

			// 0xA0
			case CP_CMD_DRAW_FAN | 0:
			case CP_CMD_DRAW_FAN | 1:
			case CP_CMD_DRAW_FAN | 2:
			case CP_CMD_DRAW_FAN | 3:
			case CP_CMD_DRAW_FAN | 4:
			case CP_CMD_DRAW_FAN | 5:
			case CP_CMD_DRAW_FAN | 6:
			case CP_CMD_DRAW_FAN | 7:
			{
				unsigned vatnum = cmd & 7;
				unsigned vtxnum = gxfifo->Read16();
				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_DRAW_FAN: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
				}

				if (vtxnum != 0) {
					tris += vtxnum - 2;

#if GFX_BLACKJACK_AND_SHADERS
					for (unsigned i = 0; i < vtxnum; i++) {
						FifoWalk(vatnum, &vertex_data[i], gxfifo);
					}
					glBufferData(GL_ARRAY_BUFFER, vtxnum * sizeof(Vertex), vertex_data, GL_STATIC_DRAW);
					glDrawArrays(GL_TRIANGLE_FAN, 0, vtxnum);
					gl_error = glGetError();
					if (gl_error != GL_NO_ERROR) {
						Halt("GL Error CP_CMD_DRAW_FAN: %x\n", gl_error);
					}
#else
					HW->gfx->ras->RAS_Begin(GFX::RAS_TRIANGLE_FAN, vtxnum);
					GFX::Vertex vtx;
					while (vtxnum--) {

						FifoWalk(vatnum, &vtx, gxfifo);
						HW->gfx->ras->RAS_SendVertex(&vtx);
					}
					HW->gfx->ras->RAS_End();
#endif
				}
				break;
			}

			// 0xA8
			case CP_CMD_DRAW_LINE | 0:
			case CP_CMD_DRAW_LINE | 1:
			case CP_CMD_DRAW_LINE | 2:
			case CP_CMD_DRAW_LINE | 3:
			case CP_CMD_DRAW_LINE | 4:
			case CP_CMD_DRAW_LINE | 5:
			case CP_CMD_DRAW_LINE | 6:
			case CP_CMD_DRAW_LINE | 7:
			{
				unsigned vatnum = cmd & 7;
				unsigned vtxnum = gxfifo->Read16();
				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_DRAW_LINE: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
				}

				if (vtxnum != 0) {
					lines += vtxnum / 2;

#if GFX_BLACKJACK_AND_SHADERS
					for (unsigned i = 0; i < vtxnum; i++) {
						FifoWalk(vatnum, &vertex_data[i], gxfifo);
					}
					glBufferData(GL_ARRAY_BUFFER, vtxnum * sizeof(Vertex), vertex_data, GL_STATIC_DRAW);
					glDrawArrays(GL_LINES, 0, vtxnum);
					gl_error = glGetError();
					if (gl_error != GL_NO_ERROR) {
						Halt("GL Error CP_CMD_DRAW_LINE: %x\n", gl_error);
					}
#else
					HW->gfx->ras->RAS_Begin(GFX::RAS_LINE, vtxnum);
					GFX::Vertex vtx;
					while (vtxnum--) {

						FifoWalk(vatnum, &vtx, gxfifo);
						HW->gfx->ras->RAS_SendVertex(&vtx);
					}
					HW->gfx->ras->RAS_End();
#endif
				}
				break;
			}

			// 0xB0
			case CP_CMD_DRAW_LINESTRIP | 0:
			case CP_CMD_DRAW_LINESTRIP | 1:
			case CP_CMD_DRAW_LINESTRIP | 2:
			case CP_CMD_DRAW_LINESTRIP | 3:
			case CP_CMD_DRAW_LINESTRIP | 4:
			case CP_CMD_DRAW_LINESTRIP | 5:
			case CP_CMD_DRAW_LINESTRIP | 6:
			case CP_CMD_DRAW_LINESTRIP | 7:
			{
				unsigned vatnum = cmd & 7;
				unsigned vtxnum = gxfifo->Read16();
				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_DRAW_LINESTRIP: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
				}

				if (vtxnum != 0) {
					lines += vtxnum - 1;

#if GFX_BLACKJACK_AND_SHADERS
					for (unsigned i = 0; i < vtxnum; i++) {
						FifoWalk(vatnum, &vertex_data[i], gxfifo);
					}
					glBufferData(GL_ARRAY_BUFFER, vtxnum * sizeof(Vertex), vertex_data, GL_STATIC_DRAW);
					glDrawArrays(GL_LINE_STRIP, 0, vtxnum);
					gl_error = glGetError();
					if (gl_error != GL_NO_ERROR) {
						Halt("GL Error CP_CMD_DRAW_LINESTRIP: %x\n", gl_error);
					}
#else
					HW->gfx->ras->RAS_Begin(GFX::RAS_LINE_STRIP, vtxnum);
					GFX::Vertex vtx;
					while (vtxnum--) {

						FifoWalk(vatnum, &vtx, gxfifo);
						HW->gfx->ras->RAS_SendVertex(&vtx);
					}
					HW->gfx->ras->RAS_End();
#endif
				}
				break;
			}

			// 0xB8
			case CP_CMD_DRAW_POINT | 0:
			case CP_CMD_DRAW_POINT | 1:
			case CP_CMD_DRAW_POINT | 2:
			case CP_CMD_DRAW_POINT | 3:
			case CP_CMD_DRAW_POINT | 4:
			case CP_CMD_DRAW_POINT | 5:
			case CP_CMD_DRAW_POINT | 6:
			case CP_CMD_DRAW_POINT | 7:
			{
				unsigned vatnum = cmd & 7;
				unsigned vtxnum = gxfifo->Read16();
				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_DRAW_POINT: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
				}

				if (vtxnum != 0) {
					pts += vtxnum;

#if GFX_BLACKJACK_AND_SHADERS
					for (unsigned i = 0; i < vtxnum; i++) {
						FifoWalk(vatnum, &vertex_data[i], gxfifo);
					}
					glBufferData(GL_ARRAY_BUFFER, vtxnum * sizeof(Vertex), vertex_data, GL_STATIC_DRAW);
					glDrawArrays(GL_POINTS, 0, vtxnum);
					gl_error = glGetError();
					if (gl_error != GL_NO_ERROR) {
						Halt("GL Error CP_CMD_DRAW_POINT: %x\n", gl_error);
					}
#else
					HW->gfx->ras->RAS_Begin(GFX::RAS_POINT, vtxnum);
					GFX::Vertex vtx;
					while (vtxnum--) {

						FifoWalk(vatnum, &vtx, gxfifo);
						HW->gfx->ras->RAS_SendVertex(&vtx);
					}
					HW->gfx->ras->RAS_End();
#endif
				}
				break;
			}

			// ---------------------------------------------------------------
			// Unknown/unsupported fifo command
			
			default:
			{
				GxBadFifo(cmd);
				break;
			}
		}
	}

	void CommandProcessor::ResetFrameStats()
	{
		tris = pts = lines = 0;
		cpLoads = bpLoads = xfLoads = 0;
	}
}