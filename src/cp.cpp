// CP - command processor
#include "pch.h"

// TODO: It's a bit crooked right now after refactoring, but will settle with time

// CP is architecturally NOT part of the graphics Pipeline, but is the initiator of drawing primitives and updating the internal context of GFX registers (XF, SU, PE, etc. load reg commands)

using namespace Debug;

namespace Flipper
{
	// init

	CommandProcessor::CommandProcessor(Flipper* flipper, HWConfig* config)
	{
		Report(Channel::CP, "Command processor (for GFX)\n");

		memset(&cpregs, 0, sizeof(cpregs));
		cpregs.cr |= CP_CR_WPINC;		// 1 on reset

		// Command Processor
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_STATUS, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_ENABLE, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_CLR, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_MEMPERF_SEL, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_STM_LOW, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_BASEL, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_BASEH, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_TOPL, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_TOPH, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_HICNTL, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_HICNTH, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_LOCNTL, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_LOCNTH, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_COUNTL, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_COUNTH, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_WPTRL, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_WPTRH, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_RPTRL, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_RPTRH, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_BRKL, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FIFO_BRKH, CPRegRead, CPRegWrite, this);

		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_COUNTER0L, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_COUNTER0H, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_COUNTER1L, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_COUNTER1H, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_COUNTER2L, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_COUNTER2H, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_COUNTER3L, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_COUNTER3H, CPRegRead, CPRegWrite, this);

		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_VC_CHKCNTL, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_VC_CHKCNTH, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_VC_MISSL, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_VC_MISSH, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_VC_STALLL, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_VC_STALLH, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FRCLK_CNTL, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_FRCLK_CNTH, CPRegRead, CPRegWrite, this);

		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_XF_ADDR, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_XF_DATAL, CPRegRead, CPRegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_CP | CP_XF_DATAH, CPRegRead, CPRegWrite, this);

		fifo = new FifoProcessor(this);
		fifo->Reset();

		tickPerFifo = 100;

		// clear counters
		tris = pts = lines = 0;
	}

	CommandProcessor::~CommandProcessor()
	{
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

	//! The reader is held while the break point is enabled (CP_ENABLE[FIFOBRK]) and its read
	//! pointer sits on it. This is the condition that gates the fetch; CP_SR_BPINT only reports it.
	bool CommandProcessor::AtBreakPoint() const
	{
		return (cpregs.cr & CP_CR_BPEN) != 0 &&
			((cpregs.rdptr & ~0x1f) == (cpregs.bpptr & ~0x1f));
	}

	//! The read unit has nothing left to take: the ring is empty. This is CP_STATUS[RD_IDLE] as
	//! the hardware forms it - the read pointer has caught up with the write pointer and no fetch
	//! is in flight (a block is taken eagerly here, so there is never one in flight). Neither the
	//! read enable nor the break point enters it: the bit describes the *reader*, not whether the
	//! guest has let it run, and a guest that waits on it after stopping the reader would still
	//! wait for the entries already in the ring.
	bool CommandProcessor::ReaderIdle() const
	{
		uint32_t count = 0;
		FifoCount(&count);

		return count == 0;
	}

	//! Whether the reader may take another block. The hardware issues the request while the FIFO
	//! reads are enabled, the ring has an entry, the break point is not armed on it and the
	//! CP-side stream buffer has room; that last one is the back pressure - the reader stops
	//! there, the ring fills up and the guest waits for space, exactly as it does on hardware.
	bool CommandProcessor::CanFetch()
	{
		uint32_t count = 0;
		FifoCount(&count);

		return count != 0 &&
			(cpregs.cr & CP_CR_RDEN) != 0 &&
			!AtBreakPoint() &&
			!StreamBufferFull();
	}

	//! The stream buffer is holding as many blocks as it is allowed to. The hardware stops the
	//! reader when the CP-side FIFO is nearly full (its occupancy plus the requests in flight
	//! passes the mark); the buffer here is far larger than the hardware's, so the limit is on
	//! the blocks it holds rather than on its size, and it is what keeps the ring meaningful.
	bool CommandProcessor::StreamBufferFull()
	{
		// The mark is on the blocks the buffer holds, but it must never stop the reader in the
		// middle of the command at the head. A draw carries up to 64K vertices and is far larger
		// than the mark; stopping there leaves the command incomplete for good, the guest waits
		// for FIFO space that can no longer be freed, and the machine hangs. The reader therefore
		// keeps going while the command at the head still needs more, and the mark applies again
		// once there is none (the buffer itself is sized for the largest one, see MaxCommandBytes).
		size_t limit = StreamBufferBlocks * 32;
		size_t pending = fifo->PendingCommandBytes();
		if (pending > limit)
			limit = pending;

		return fifo->GetSize() >= limit;
	}

	//! Bring the idle bits of CP_STATUS up to date with the reader's current state. They used to
	//! be latched only by a fetch, but the CP thread is now woken only when the ring has entries,
	//! so after the last entry was taken the bits stayed clear forever and a guest that polls
	//! CP_STATUS in a loop never saw the reader go idle. That is a real wait: Super Monkey Ball 2
	//! spins on bit 2 (`GXGetGPStatus`) before it starts drawing, so the game never got past its
	//! first frame.
	void CommandProcessor::UpdateReaderStatus()
	{
		if (ReaderIdle())
		{
			cpregs.sr |= CP_SR_RD_IDLE;
		}
		else
		{
			cpregs.sr &= ~CP_SR_RD_IDLE;
		}

		// The command processor itself is idle while its own stream buffer has nothing left to
		// parse - the hardware's StmBuf_Empty, and with it the sub-units the CP feeds. The two
		// bits are independent: the reader can be idle while a command is still being run.
		if (fifo->GetSize() == 0)
		{
			cpregs.sr |= CP_SR_CMD_IDLE;
		}
		else
		{
			cpregs.sr &= ~CP_SR_CMD_IDLE;
		}
	}

	//! CP_STATUS as the guest sees it: the latched water mark / break point flags plus the idle
	//! bits of the reader's current state.
	uint16_t CommandProcessor::Status()
	{
		UpdateReaderStatus();
		return cpregs.sr;
	}

	// The read pointer has reached the break point. The break point itself stops the reader; the
	// status flag is raised whenever the break point is enabled (CP_ENABLE[FIFOBRK]) and cleared
	// when it is disabled (command-processor.md 4.3: "the FIFO read pointer reached the break
	// point (cleared by disabling the break point)"). CP_ENABLE[FIFOBRKINT] only decides whether
	// the CP interrupt is raised on top of that.
	void CommandProcessor::CP_BREAK()
	{
		if ((cpregs.cr & CP_CR_BPEN) == 0)
		{
			return;
		}

		if ((cpregs.sr & CP_SR_BPINT) != 0)
		{
			return;
		}

		cpregs.sr |= CP_SR_BPINT;

		if (cpregs.cr & CP_CR_BPINTEN)
		{
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
		ExecuteFifo();
	}

	bool CommandProcessor::HasFifoWork()
	{
		// Same count calculation as PumpFifo, but without any of its side effects.
		uint32_t cnt;

		if (cpregs.wrptr >= cpregs.rdptr)
		{
			cnt = cpregs.wrptr - cpregs.rdptr;
		}
		else
		{
			cnt = (cpregs.top - cpregs.rdptr) + (cpregs.wrptr - cpregs.base);
		}

		// Only the break point stops the reader: the overflow and underflow bits are status
		// (and interrupt) flags, not gates on the fetch. The break itself is reported from
		// PumpFifo, so the FIFO must still look busy while the reader sits on it - otherwise the
		// status bit and the CP interrupt would never be raised and a host waiting for them would
		// wait forever. Once the flag is up there is nothing left to do.
		return cnt != 0 && (cpregs.cr & CP_CR_RDEN) != 0 &&
			(!AtBreakPoint() || (cpregs.sr & CP_SR_BPINT) == 0);
	}

	// The emulated CP consumes one FIFO entry every `tickPerFifo` ticks, so the entries it owes
	// The emulated CP consumes one FIFO entry every `tickPerFifo` ticks, so the entries it owes
	// follow from the time that passed since the last drain. The anchor only moves by the entries
	// actually drained: nothing owed is ever dropped, and no extra batch limit is applied, because
	// a limit would tie the reader's emulated throughput to how often the host happens to schedule
	// the CP thread - the FIFO would then fill up on a busy host and the guest would stall on
	// registers that should have moved on. The drain stops by itself when the FIFO runs dry.
	void CommandProcessor::DrainFifo()
	{
		fifoLock.Lock();

		// A CPU reset can set the time base back; the anchor has to follow it, or the reader
		// would stay parked in the future for as long as the reset set it back.
		if (Core->GetTicks() < lastDrainTick)
		{
			lastDrainTick = Core->GetTicks();
		}

		int64_t budget = (Core->GetTicks() - lastDrainTick) / (int64_t)tickPerFifo;
		if (budget > 0)
		{
			lastDrainTick += budget * (int64_t)tickPerFifo;

			// The reader has already taken what the guest wrote (FifoWriteBurst), so the work
			// left here is the parse: the stream buffer holds the commands and ExecuteFifo runs
			// every complete one. The budget bounds how often the parse runs, not how much it
			// runs - half a command left behind would stall the stream.
			while (FetchFifoEntry())
			{
				// Whatever the reader could not take earlier (the buffer had no room then).
			}

			ExecuteFifo();
			UpdateReaderStatus();
		}

		fifoLock.Unlock();
	}

	// One burst of the graphics FIFO: this is what the CP does on a tick, and what the unit tests
	// drive by hand to run a display list deterministically.
	void CommandProcessor::PumpFifo()
	{
		if (FetchFifoEntry())
		{
			ExecuteFifo();
		}
	}

	// Move one 32-byte entry of the main-memory ring into the CP-side stream buffer. The read
	// pointer comes from the FIFO base/top registers the guest writes, so the whole burst has to be
	// inside main memory before it is handed to the GX.
	bool CommandProcessor::FetchFifoEntry()
	{
		// Calculate count
		if (cpregs.wrptr >= cpregs.rdptr)
		{
			cpregs.cnt = cpregs.wrptr - cpregs.rdptr;
		}
		else
		{
			cpregs.cnt = (cpregs.top - cpregs.rdptr) + (cpregs.wrptr - cpregs.base);
		}

		// Watermarks. The hardware compares the count against the high/low water marks in both
		// modes; the flags are status (and, with their enables, CP interrupts) and never gate the
		// fetch - only the break point, the read enable and a full streaming buffer do.
		if (cpregs.cnt > cpregs.himark)
		{
			CP_OVF();
		}
		if (cpregs.cnt < cpregs.lomark)
		{
			CP_UVF();
		}

		// Breakpoint
		if (AtBreakPoint())
		{
			CP_BREAK();
		}

		// Advance read pointer.
		UpdateReaderStatus();

		if (!CanFetch())
		{
			return false;
		}

		uint8_t* fifoBurst = (uint8_t*)HW->mem->MIGetMemoryPointerForCP(cpregs.rdptr);

		if (fifoBurst == nullptr || !Verify::MainMemory(cpregs.rdptr, 32, HW->mem->MIGetMemorySize()))
		{
			Report(Channel::CP, "CP FIFO read pointer is out of main memory: %08X\n", cpregs.rdptr);
			return false;
		}

		fifo->PushBytes(fifoBurst);
		cpregs.rdptr += 32;
		if (cpregs.rdptr == cpregs.top)
		{
			cpregs.rdptr = cpregs.base;
		}

		// The block is the reader's, so the occupancy and both status bits have moved on.
		UpdateReaderStatus();

		return true;
	}

	void CommandProcessor::ExecuteFifo()
	{
		while (fifo->EnoughToExecute())
		{
			GxCommand(fifo);
		}
	}

	// The distance between the write and the read pointer, over the ring. Both pointers live in
	// 32-byte units, so the result is in the same units (command-processor.md 4.5).
	void CommandProcessor::FifoCount(uint32_t* count) const
	{
		if (cpregs.wrptr >= cpregs.rdptr)
		{
			*count = cpregs.wrptr - cpregs.rdptr;
		}
		else
		{
			*count = (cpregs.top - cpregs.rdptr) + (cpregs.wrptr - cpregs.base);
		}
	}

	void CommandProcessor::ResetFifoProcessor()
	{
		fifo->Reset();
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
				return Status();
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
			// The count is what the hardware reports, not a value the CPU wrote: it is the distance
			// between the write and the read pointer, in the same 32-byte units and over the same
			// ring (command-processor.md 4.5). The CP keeps it live so that a program which polls
			// CP_FIFO_COUNT sees the FIFO drain as the CP walks it.
			case CP_FIFO_COUNTL:
			{
				uint32_t count = 0;
				FifoCount(&count);
				return (uint16_t)(count & 0xffe0);
			}
			case CP_FIFO_COUNTH:
			{
				uint32_t count = 0;
				FifoCount(&count);
				return (uint16_t)(count >> 16);
			}
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
				return (uint16_t)cpregs.xfAddr;
			case CP_XF_DATAL:
				return (uint16_t)cpregs.xfData;
			case CP_XF_DATAH:
				return (uint16_t)(cpregs.xfData >> 16);
		}

		return 0;
	}

	void CommandProcessor::CpWriteReg(uint32_t addr, uint16_t value)
	{
		// The reader walks these registers under `fifoLock` (DrainFifo). A CPU-side write has to
		// respect the same lock: the reader reads the pointer, fetches the burst and only then
		// advances it (`rdptr += 32`), so a write that lands in between - GXSetGPFifo repointing
		// the FIFO is exactly that - is overwritten by the stale advance and the CP skips the
		// first 32-byte entry of the new FIFO. mgt-fifo-brkpt depends on the switch being atomic.
		fifoLock.Lock();

		switch (addr)
		{
			case CP_STATUS:
				break;
			case CP_ENABLE:
				cpregs.cr = (uint16_t)value;

				// The break-point status flag is cleared by disabling the break point itself
				// (CP_ENABLE[FIFOBRK]); clearing only its interrupt enable does not release the
				// reader.
				if ((value & CP_CR_BPEN) == 0)
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
				// Writing the register address starts a read of the XF register space over the
				// CP -> XF read-back path; the answer is left in CP_XF_DATAL / CP_XF_DATAH.
				cpregs.xfAddr = value;
				ReadXFReg(value);
				break;
			case CP_XF_DATAL:
			case CP_XF_DATAH:
				// Read-back data, read-only.
				break;
		}

		fifoLock.Unlock();
	}

	void CommandProcessor::FifoWriteBurst()
	{
		// CP FIFO
		//
		// The write side of the ring belongs to the PI (CPBAS / CPTOP / CPWRT); the CP keeps its
		// own copy of the write pointer (CP_FIFO_WPTRH/L) and steps it on every FIFO write burst,
		// but only while CP_ENABLE[WRPTRINC] is set. That gate is what lets the CPU use the write
		// path as a plain DMA into a scratch buffer - it repoints the PI's base / top / write
		// pointer at the scratch area and back without the CP taking the data for commands.

		if (cpregs.cr & CP_CR_WPINC)
		{
			cpregs.wrptr += 32;

			if (cpregs.wrptr == cpregs.top)
			{
				cpregs.wrptr = cpregs.base;
			}

			// ... and it is the reader's immediately: on hardware the read unit chases the write
			// pointer and gives up only when the ring runs dry, the reads are disabled, a break
			// point is armed or its own stream buffer is full. Taking the block here is what keeps
			// the ring empty while the guest runs, so a repoint of the FIFO (GXSetGPFifo rewrites
			// base / top / pointers and re-enables the reader) can never strand entries that were
			// written but never fetched: they are already in the CP's buffer, and the occupancy
			// the guest reads is what the CP still has to parse.
			fifoLock.Lock();
			while (FetchFifoEntry())
			{
				// Buffered; the CP thread runs the commands.
			}
			fifoLock.Unlock();
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

		// The occupancy follows from the two pointers, not from the copy the fetch leaves behind.
		uint32_t count = 0;
		FifoCount(&count);

		Report(Channel::Norm, "CP %sfifo configuration:%c%c%c\n", md, bp, lw, hw);
		Report(Channel::Norm, " status :0x%08X\n", Status());
		Report(Channel::Norm, " enable :0x%08X\n", cpregs.cr);
		Report(Channel::Norm, "   base :0x%08X\n", cpregs.base);
		Report(Channel::Norm, "   top  :0x%08X\n", cpregs.top);
		Report(Channel::Norm, "   low  :0x%08X\n", cpregs.lomark);
		Report(Channel::Norm, "   high :0x%08X\n", cpregs.himark);
		Report(Channel::Norm, "   cnt  :0x%08X\n", count);
		Report(Channel::Norm, "   wrptr:0x%08X\n", cpregs.wrptr);
		Report(Channel::Norm, "   rdptr:0x%08X\n", cpregs.rdptr);
		Report(Channel::Norm, "   break:0x%08X\n", cpregs.bpptr);
	}

	// One bypass (BP) register write. The write mask register 0xFE limits which bits of the very
	// next write update the target register and is consumed by it; the mask goes down the bypass
	// chain with the write, so that the block that owns the register can keep the bits it leaves
	// out of its own value (GDTev.h SS_MASK, MergeBpWriteMask).
	void CommandProcessor::BpRegWrite(size_t index, uint32_t value)
	{
		// The mask register itself is not part of the register file
		if (index == 0xFE)
		{
			bpWriteMask = value & 0xFFFFFF;
			bpWriteMaskPending = true;
			return;
		}

		uint32_t mask = 0xFFFFFF;

		if (bpWriteMaskPending)
		{
			mask = bpWriteMask;
			bpWriteMaskPending = false;
		}

		// The bypass load goes through the XF, which forwards it to the SU and on down the chain.
		XFSync();
		HW->gfx->xf->CPSuCommand(index, value, mask);
	}

	// index range = 00..FF
	// reg size = 32 bit
	void CommandProcessor::loadCPReg(size_t index, uint32_t value)
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
			}
			return;

			case CP_VCD_HI_ID:
			{
				cp.vcdHi.bits = value;
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
				// The value is part of the report: an unhandled CP register is only meaningful
				// together with what the guest tried to write into it.
				Report(Channel::GP, "Unknown CP load, index: 0x%02X, data: 0x%08X\n", index, value);
			}
		}
	}

	// The CP -> XF handshake (gfx-xf.md 2.1). The CP may only push a word into the XF while the XF
	// is ready; the only thing that makes the XF busy in this emulator is a read-back word that the
	// CP has not taken yet, and taking it (XFrdValid / XFrdData) releases the XF again. The word
	// that the CP takes this way is latched in CP_XF_DATAL / CP_XF_DATAH, where the CPU sees it.

	void CommandProcessor::XFSync()
	{
		while (!HW->gfx->xf->CPReady())
		{
			uint32_t value;

			if (!HW->gfx->xf->CPTakeReadData(&value))
			{
				break;
			}

			cpregs.xfData = value;
		}
	}

	// Read an XF register over the CP -> XF read-back path (xf_cmd_regread). The XF puts the value
	// of its own register state on the read-back line, and the CP latches it.

	uint32_t CommandProcessor::ReadXFReg(size_t index)
	{
		XFSync();									// wait for XFready, then request the register
		HW->gfx->xf->CPRegRead(index);
		XFSync();									// take the value the XF answered with

		return cpregs.xfData;
	}

	#pragma endregion "Dealing with registers"


	#pragma region "FIFO Processing"

	FifoProcessor::FifoProcessor(CommandProcessor* owner)
	{
		fifo = new uint8_t[fifoSize];
		memset(fifo, 0, fifoSize);
		allocated = true;
		this->owner = owner;
	}

	FifoProcessor::FifoProcessor(uint8_t* fifoPtr, size_t size, CommandProcessor* owner)
	{
		fifo = fifoPtr;
		fifoSize = size + 1;
		writePtr = fifoSize - 1;
		allocated = false;
		this->owner = owner;
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
			case CPCommand::CP_CMD_LOAD_BPREG | 9:
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
				return GetSize() >= (vtxnum * VertexSize(cmd & 7) + 3);
			}

			default:
			{
				// `allocated` is true for the processor that owns its own buffer, which is the main
				// command stream; the other one is a display list being walked at this moment.
				Halt("GFX: Unsupported opcode: 0x%02X (%s, readPtr: 0x%x)\n", cmd, allocated ? "Stream" : "Call DL", readPtr);
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

	//! The 16-bit unit at `offset`, big-endian like every other word in the stream. The count of a
	//! draw command is the word this reads: returning it as a byte truncated the count to its low
	//! eight bits, and a draw of more than 255 vertices then looked complete to
	//! FifoProcessor::EnoughToExecute while most of its vertex data was still on its way in. The
	//! command processor ran it anyway and read whatever followed in the buffer, so the tail of
	//! every long draw decoded as zeros (the lit-atn-func half-cylinder stopped a quarter of the
	//! way round, issue #385).
	size_t FifoProcessor::PendingCommandBytes()
	{
		if (GetSize() < 3)
			return 0;

		uint8_t cmd = Peek8(0);
		if (cmd < (uint8_t)CPCommand::CP_CMD_DRAW_QUAD || cmd > ((uint8_t)CPCommand::CP_CMD_DRAW_POINT | 7))
			return 0;

		return (size_t)Peek16(1) * VertexSize(cmd & 7) + 3;
	}

	uint16_t FifoProcessor::Peek16(size_t offset)
	{
		return (uint16_t)(((uint16_t)Peek8(offset) << 8) | Peek8(offset + 1));
	}

	//! The CP owns the vertex format state (VCD / VAT); the stream only asks it for the sizes.
	size_t FifoProcessor::VertexSize(unsigned vat)
	{
		return owner != nullptr ? owner->VertexSize(vat) : 0;
	}

	size_t CommandProcessor::VertexSize(unsigned vat)
	{
		return (size_t)gx_vtxsize(vat);
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
		static int cfmtsz[] = { 2, 3, 4, 2, 3, 4 };

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

	void * CommandProcessor::GetArrayPtr(ArrayId arrayId, int idx, int compSize)
	{
		uint32_t address = cp.arrayBase[(size_t)arrayId].Base + 
			(uint32_t)idx * cp.arrayStride[(size_t)arrayId].Stride;

		// The array base, the stride and the index all come from guest state, and the caller
		// reads up to four 32-bit components from the result, so the whole window has to be
		// inside main memory: the old code validated the start address only and then walked off
		// the end of the RAM allocation.
		if (!Verify::MainMemory(address, 16, HW->mem->MIGetMemorySize()))
		{
			return nullptr;
		}

		return HW->mem->MIGetMemoryPointerForCP(address);
	}

	// XF_IndexLoadRegA..D (00100xxx .. 00111xxx). A block load of the XF matrix/light area whose
	// source address is not in the command: the command carries an index into one of the four index
	// arrays (A..D are the CP arrays 0xC..0xF), and the CP reads that array entry as the source
	// address and streams `count` words of it into the XF, starting at the XF address that rides in
	// the command. It is how the GX SDK's GXLoadPosMtxIndx / GXLoadNrmMtxIndx3x3 / GXLoadTexMtxIndx /
	// GXLoadLightObjIndx take a matrix or a light record out of an array of them, and it is what the
	// demos that walk a model one matrix slot at a time use (tf-reflect, DL-tf-mtx, tev-outline).
	void CommandProcessor::LoadIndexedXF(ArrayId arrayId, FifoProcessor* gxfifo)
	{
		uint32_t word = gxfifo->Read32();

		size_t index = word >> 16;					// which entry of the index array
		size_t count = ((word >> 12) & 0xF) + 1;	// how many words to load
		size_t xfAddr = word & 0xFFF;				// the XF address, in words

		// The array entry is a main memory address, so the array window and the block it points at
		// are both guest data and are validated before anything is read.
		uint32_t address = cp.arrayBase[(size_t)arrayId].Base +
			(uint32_t)index * cp.arrayStride[(size_t)arrayId].Stride;

		if (!Verify::MainMemory(address, count * sizeof(uint32_t), HW->mem->MIGetMemorySize()))
		{
			Report(Channel::CP, "XF index array load is out of memory (array %i, index %zi, address 0x%08X, %zi words)\n",
				(int)arrayId, index, address, count);
			return;
		}

		uint32_t* src = (uint32_t*)HW->mem->MIGetMemoryPointerForCP(address);

		XFSync();
		HW->gfx->xf->CPRegLoadBegin(xfAddr, count);

		// The array entry is guest memory, so its words are big-endian like every other word the
		// CP reads through an indexed array (see FetchComp): the XF wants the numeric word.
		for (size_t i = 0; i < count; i++)
		{
			HW->gfx->xf->CPRegLoadData(_BYTESWAP_UINT32(src[i]));
		}
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

		// An indirect attribute whose address does not resolve to main memory cannot be read;
		// the direct form reads from the FIFO itself and does not use the pointer.
		if (type != VCD_DIRECT && ptr == nullptr)
		{
			Report(Channel::CP, "CP: vertex array address is out of main memory\n");
			return;
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
		static int cfmtsz[] = { 2, 3, 4, 2, 3, 4 };

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
	void CommandProcessor::FifoWalk(unsigned vatnum, GFX::Vertex* vtx, FifoProcessor* gxfifo, const GFX::MatrixIndex0& matIdx0, const GFX::MatrixIndex1& matIdx1)
	{
		// The XF matrix index registers hold the defaults; the 'mtxidx' attributes override them.
		vtx->matIdx0 = matIdx0;
		vtx->matIdx1 = matIdx1;

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

	// Execute one draw command. The CP owns the vertex fetch (VCD/VAT and the attribute arrays), so
	// it unpacks the vertex data and pushes the resulting vertex rows into the XF, which is the
	// entry point of the graphics pipeline.

	void CommandProcessor::DrawPrimitive(uint8_t command, FifoProcessor* gxfifo, GFX::RAS_Primitive prim)
	{
		unsigned vatnum = command & 7;
		unsigned vtxnum = gxfifo->Read16();

		if (logDrawCommands)
		{
			Report(Channel::GP, "Draw: cmd: 0x%02X, vtxnum: %i, vat: %i\n", command, vtxnum, vatnum);
		}

		if (vtxnum == 0)
			return;

		size_t primsBefore = tris + pts + lines;

		switch (prim)
		{
			case GFX::RAS_QUAD:				tris += (vtxnum / 4) / 2; break;
			case GFX::RAS_QUAD_STRIP:		tris += (vtxnum / 2 - 1) / 2; break;
			case GFX::RAS_TRIANGLE:			tris += vtxnum / 3; break;
			case GFX::RAS_TRIANGLE_STRIP:	tris += vtxnum - 2; break;
			case GFX::RAS_TRIANGLE_FAN:		tris += vtxnum - 2; break;
			case GFX::RAS_LINE:				lines += vtxnum / 2; break;
			case GFX::RAS_LINE_STRIP:		lines += vtxnum - 1; break;
			case GFX::RAS_POINT:			pts += vtxnum; break;
		}

		// The frame counters are cleared at every frame end, so the profiler keeps its own copy of
		// what this draw produced (issue #394): it reports the primitives and the vertices of the
		// last second, which is how a title that has stopped feeding the FIFO shows up.
		HwProfile::Count(HwProfile::Counter::GfxPrimitives, tris + pts + lines - primsBefore);
		HwProfile::Count(HwProfile::Counter::GfxVertices, vtxnum);

		// The default matrix indexes live in the XF; read them over the CP -> XF read-back path.

		GFX::MatrixIndex0 matIdx0{};
		matIdx0.bits = ReadXFReg(GFX::XF_MATINDEX_A_ID);

		GFX::MatrixIndex1 matIdx1{};
		matIdx1.bits = ReadXFReg(GFX::XF_MATINDEX_B_ID);

		XFSync();
		HW->gfx->xf->CPDrawBegin(prim, vtxnum);

		GFX::Vertex vtx;

		while (vtxnum--)
		{
			FifoWalk(vatnum, &vtx, gxfifo, matIdx0, matIdx1);
			HW->gfx->xf->CPVertex(&vtx);
		}

		HW->gfx->xf->CPDrawEnd();
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

				// The object's size field is the byte count of the list, encoded like the address
				// above: only bits [25:5] are meaningful, which is the number of 32-byte blocks
				// (the CP walks the object a block at a time).
				size_t size = gxfifo->Read32() & 0x03ffffe0;

				// Both the address and the size are guest data, and the list is read through a
				// raw pointer: a display list that does not fit in main memory would read past
				// the end of the RAM allocation. An in-range list is still executed, clamped to
				// what the memory actually holds.
				if (fifoPtr == nullptr || !Verify::MainMemory(physAddress, size, HW->mem->MIGetMemorySize()))
				{
					uint32_t ramSize = (uint32_t)HW->mem->MIGetMemorySize();
					uint32_t offset = physAddress & Verify::MainMemoryMask;

					Report(Channel::CP, "CP_CMD_CALL_DL: display list out of memory (addr 0x%08X, size %zi)\n",
						physAddress, size);

					if (fifoPtr == nullptr || offset >= ramSize)
					{
						break;
					}

					size = ramSize - offset;
				}

				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_CALL_DL: addr: 0x%08X, size: %i\n", physAddress, size);
				}

				FifoProcessor* callDlFifo = new FifoProcessor(fifoPtr, size, this);

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
			case CP_CMD_LOAD_BPREG | 9:
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

				BpRegWrite(index, value);
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
				loadCPReg(index, word);
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

				// Push the block write into the XF: the load header, then one word per register.
				XFSync();
				HW->gfx->xf->CPRegLoadBegin(index, len);

				for (size_t i = 0; i < len; i++)
				{
					HW->gfx->xf->CPRegLoadData(gxfifo->Read32());
				}
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
				LoadIndexedXF(ArrayId::IndexRegA, gxfifo);
				break;

			case CP_CMD_LOAD_INDXB | 0:
			case CP_CMD_LOAD_INDXB | 1:
			case CP_CMD_LOAD_INDXB | 2:
			case CP_CMD_LOAD_INDXB | 3:
			case CP_CMD_LOAD_INDXB | 4:
			case CP_CMD_LOAD_INDXB | 5:
			case CP_CMD_LOAD_INDXB | 6:
			case CP_CMD_LOAD_INDXB | 7:
				LoadIndexedXF(ArrayId::IndexRegB, gxfifo);
				break;

			case CP_CMD_LOAD_INDXC | 0:
			case CP_CMD_LOAD_INDXC | 1:
			case CP_CMD_LOAD_INDXC | 2:
			case CP_CMD_LOAD_INDXC | 3:
			case CP_CMD_LOAD_INDXC | 4:
			case CP_CMD_LOAD_INDXC | 5:
			case CP_CMD_LOAD_INDXC | 6:
			case CP_CMD_LOAD_INDXC | 7:
				LoadIndexedXF(ArrayId::IndexRegC, gxfifo);
				break;

			case CP_CMD_LOAD_INDXD | 0:
			case CP_CMD_LOAD_INDXD | 1:
			case CP_CMD_LOAD_INDXD | 2:
			case CP_CMD_LOAD_INDXD | 3:
			case CP_CMD_LOAD_INDXD | 4:
			case CP_CMD_LOAD_INDXD | 5:
			case CP_CMD_LOAD_INDXD | 6:
			case CP_CMD_LOAD_INDXD | 7:
				LoadIndexedXF(ArrayId::IndexRegD, gxfifo);
				break;

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
				DrawPrimitive(cmd, gxfifo, GFX::RAS_QUAD);
				break;

			// 0x88
			case CP_CMD_DRAW_QUAD_STRIP | 0:
			case CP_CMD_DRAW_QUAD_STRIP | 1:
			case CP_CMD_DRAW_QUAD_STRIP | 2:
			case CP_CMD_DRAW_QUAD_STRIP | 3:
			case CP_CMD_DRAW_QUAD_STRIP | 4:
			case CP_CMD_DRAW_QUAD_STRIP | 5:
			case CP_CMD_DRAW_QUAD_STRIP | 6:
			case CP_CMD_DRAW_QUAD_STRIP | 7:
				DrawPrimitive(cmd, gxfifo, GFX::RAS_QUAD_STRIP);
				break;

			// 0x90
			case CP_CMD_DRAW_TRIANGLE | 0:
			case CP_CMD_DRAW_TRIANGLE | 1:
			case CP_CMD_DRAW_TRIANGLE | 2:
			case CP_CMD_DRAW_TRIANGLE | 3:
			case CP_CMD_DRAW_TRIANGLE | 4:
			case CP_CMD_DRAW_TRIANGLE | 5:
			case CP_CMD_DRAW_TRIANGLE | 6:
			case CP_CMD_DRAW_TRIANGLE | 7:
				DrawPrimitive(cmd, gxfifo, GFX::RAS_TRIANGLE);
				break;

			// 0x98 
			case CP_CMD_DRAW_STRIP | 0:
			case CP_CMD_DRAW_STRIP | 1:
			case CP_CMD_DRAW_STRIP | 2:
			case CP_CMD_DRAW_STRIP | 3:
			case CP_CMD_DRAW_STRIP | 4:
			case CP_CMD_DRAW_STRIP | 5:
			case CP_CMD_DRAW_STRIP | 6:
			case CP_CMD_DRAW_STRIP | 7:
				DrawPrimitive(cmd, gxfifo, GFX::RAS_TRIANGLE_STRIP);
				break;

			// 0xA0
			case CP_CMD_DRAW_FAN | 0:
			case CP_CMD_DRAW_FAN | 1:
			case CP_CMD_DRAW_FAN | 2:
			case CP_CMD_DRAW_FAN | 3:
			case CP_CMD_DRAW_FAN | 4:
			case CP_CMD_DRAW_FAN | 5:
			case CP_CMD_DRAW_FAN | 6:
			case CP_CMD_DRAW_FAN | 7:
				DrawPrimitive(cmd, gxfifo, GFX::RAS_TRIANGLE_FAN);
				break;

			// 0xA8
			case CP_CMD_DRAW_LINE | 0:
			case CP_CMD_DRAW_LINE | 1:
			case CP_CMD_DRAW_LINE | 2:
			case CP_CMD_DRAW_LINE | 3:
			case CP_CMD_DRAW_LINE | 4:
			case CP_CMD_DRAW_LINE | 5:
			case CP_CMD_DRAW_LINE | 6:
			case CP_CMD_DRAW_LINE | 7:
				DrawPrimitive(cmd, gxfifo, GFX::RAS_LINE);
				break;

			// 0xB0
			case CP_CMD_DRAW_LINESTRIP | 0:
			case CP_CMD_DRAW_LINESTRIP | 1:
			case CP_CMD_DRAW_LINESTRIP | 2:
			case CP_CMD_DRAW_LINESTRIP | 3:
			case CP_CMD_DRAW_LINESTRIP | 4:
			case CP_CMD_DRAW_LINESTRIP | 5:
			case CP_CMD_DRAW_LINESTRIP | 6:
			case CP_CMD_DRAW_LINESTRIP | 7:
				DrawPrimitive(cmd, gxfifo, GFX::RAS_LINE_STRIP);
				break;

			// 0xB8
			case CP_CMD_DRAW_POINT | 0:
			case CP_CMD_DRAW_POINT | 1:
			case CP_CMD_DRAW_POINT | 2:
			case CP_CMD_DRAW_POINT | 3:
			case CP_CMD_DRAW_POINT | 4:
			case CP_CMD_DRAW_POINT | 5:
			case CP_CMD_DRAW_POINT | 6:
			case CP_CMD_DRAW_POINT | 7:
				DrawPrimitive(cmd, gxfifo, GFX::RAS_POINT);
				break;

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

	void CommandProcessor::GetStats(CommandProcessorStats* stats) const
	{
		if (stats == nullptr)
		{
			return;
		}

		stats->cpLoads = cpLoads;
		stats->xfLoads = xfLoads;
		stats->bpLoads = bpLoads;
		stats->tris = tris;
		stats->points = pts;
		stats->lines = lines;
	}
}
