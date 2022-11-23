// PI and CP FIFO processing

#include "pch.h"

using namespace Debug;

void GXWriteFifo(uint8_t dataPtr[32]);

namespace GX
{
	#pragma region "Dealing with registers"

	void GXCore::CP_BREAK()
	{
		if (state.cpregs.cr & CP_CR_BPINTEN && (state.cpregs.sr & CP_SR_BPINT) == 0)
		{
			state.cpregs.sr |= CP_SR_BPINT;
			PIAssertInt(PI_INTERRUPT_CP);
			Report(Channel::CP, "BREAK");
		}
	}

	void GXCore::CP_OVF()
	{
		if (state.cpregs.cr & CP_CR_OVFEN && (state.cpregs.sr & CP_SR_OVF) == 0)
		{
			state.cpregs.sr |= CP_SR_OVF;
			PIAssertInt(PI_INTERRUPT_CP);
			Report(Channel::CP, "OVF");
		}
	}

	void GXCore::CP_UVF()
	{
		if (state.cpregs.cr & CP_CR_UVFEN && (state.cpregs.sr & CP_SR_UVF) == 0)
		{
			state.cpregs.sr |= CP_SR_UVF;
			PIAssertInt(PI_INTERRUPT_CP);
			Report(Channel::CP, "UVF");
		}
	}

	void GXCore::CPThread(void* Param)
	{
		GXCore* gx = (GXCore*)Param;

		int64_t ticks = Core->GetTicks();
		if (ticks < gx->state.updateTbrValue)
		{
			return;
		}
		gx->state.updateTbrValue = ticks + gx->state.tickPerFifo;

		// Calculate count
		if (gx->state.cpregs.wrptr >= gx->state.cpregs.rdptr)
		{
			gx->state.cpregs.cnt = gx->state.cpregs.wrptr - gx->state.cpregs.rdptr;
		}
		else
		{
			gx->state.cpregs.cnt = (gx->state.cpregs.top - gx->state.cpregs.rdptr) + (gx->state.cpregs.wrptr - gx->state.cpregs.base);
		}

		// Watermarks logic. Active only in linked-mode.
		if (gx->state.cpregs.cnt > gx->state.cpregs.himark && (gx->state.cpregs.cr & CP_CR_WPINC))
		{
			gx->CP_OVF();
		}
		if (gx->state.cpregs.cnt < gx->state.cpregs.lomark && (gx->state.cpregs.cr & CP_CR_WPINC))
		{
			gx->CP_UVF();
		}

		// Breakpoint
		if ((gx->state.cpregs.rdptr & ~0x1f) == (gx->state.cpregs.bpptr & ~0x1f) && (gx->state.cpregs.cr & CP_CR_BPEN))
		{
			gx->CP_BREAK();
		}

		// Advance read pointer.
		if (gx->state.cpregs.cnt != 0 && gx->state.cpregs.cr & CP_CR_RDEN && (gx->state.cpregs.sr & (CP_SR_OVF | CP_SR_UVF | CP_SR_BPINT)) == 0)
		{
			gx->state.cpregs.sr &= ~CP_SR_RD_IDLE;

			gx->state.cpregs.sr &= ~CP_SR_CMD_IDLE;

			// TODO: Refactoring hacks

			::GXWriteFifo(&mi.ram[gx->state.cpregs.rdptr & RAMMASK]);

			//gx->fifo->WriteBytes(&mi.ram[gx->state.cpregs.rdptr & RAMMASK]);


			gx->state.cpregs.sr |= CP_SR_CMD_IDLE;

			gx->state.cpregs.rdptr += 32;
			if (gx->state.cpregs.rdptr == gx->state.cpregs.top)
			{
				gx->state.cpregs.rdptr = gx->state.cpregs.base;
			}
		}
		else
		{
			gx->state.cpregs.sr |= (CP_SR_RD_IDLE | CP_SR_CMD_IDLE);
		}
	}

	// TODO: Make a GP update when copying the frame buffer by Pixel Engine.

	void GXCore::DONE_INT()
	{
		if (state.peregs.sr & PE_SR_DONEMSK)
		{
			state.peregs.sr |= PE_SR_DONE;
			PIAssertInt(PI_INTERRUPT_PE_FINISH);
		}
	}

	void GXCore::TOKEN_INT()
	{
		if (state.peregs.sr & PE_SR_TOKENMSK)
		{
			state.peregs.sr |= PE_SR_TOKEN;
			PIAssertInt(PI_INTERRUPT_PE_TOKEN);
		}
	}

	void GXCore::CPDrawDoneCallback()
	{
		DONE_INT();
	}

	void GXCore::CPDrawTokenCallback(uint16_t tokenValue)
	{
		state.peregs.token = tokenValue;
		TOKEN_INT();
	}

	uint16_t GXCore::CpReadReg(CPMappedRegister id)
	{
		switch (id)
		{
			case CPMappedRegister::CP_STATUS_ID:
				return state.cpregs.sr;
			case CPMappedRegister::CP_ENABLE_ID:
				return state.cpregs.cr;
			case CPMappedRegister::CP_CLR_ID:
				return 0;
			case CPMappedRegister::CP_MEMPERF_SEL_ID:
				return 0;
			case CPMappedRegister::CP_STM_LOW_ID:
				return 0;
			case CPMappedRegister::CP_FIFO_BASEL_ID:
				return state.cpregs.basel & 0xffe0;
			case CPMappedRegister::CP_FIFO_BASEH_ID:
				return state.cpregs.baseh;
			case CPMappedRegister::CP_FIFO_TOPL_ID:
				return state.cpregs.topl & 0xffe0;
			case CPMappedRegister::CP_FIFO_TOPH_ID:
				return state.cpregs.toph;
			case CPMappedRegister::CP_FIFO_HICNTL_ID:
				return state.cpregs.himarkl & 0xffe0;
			case CPMappedRegister::CP_FIFO_HICNTH_ID:
				return state.cpregs.himarkh;
			case CPMappedRegister::CP_FIFO_LOCNTL_ID:
				return state.cpregs.lomarkl & 0xffe0;
			case CPMappedRegister::CP_FIFO_LOCNTH_ID:
				return state.cpregs.lomarkh;
			case CPMappedRegister::CP_FIFO_COUNTL_ID:
				return state.cpregs.cntl & 0xffe0;
			case CPMappedRegister::CP_FIFO_COUNTH_ID:
				return state.cpregs.cnth;
			case CPMappedRegister::CP_FIFO_WPTRL_ID:
				return state.cpregs.wrptrl & 0xffe0;
			case CPMappedRegister::CP_FIFO_WPTRH_ID:
				return state.cpregs.wrptrh;
			case CPMappedRegister::CP_FIFO_RPTRL_ID:
				return state.cpregs.rdptrl & 0xffe0;
			case CPMappedRegister::CP_FIFO_RPTRH_ID:
				return state.cpregs.rdptrh;
			case CPMappedRegister::CP_FIFO_BRKL_ID:
				return state.cpregs.bpptrl & 0xffe0;
			case CPMappedRegister::CP_FIFO_BRKH_ID:
				return state.cpregs.bpptrh;
			case CPMappedRegister::CP_COUNTER0L_ID:
				return 0;
			case CPMappedRegister::CP_COUNTER0H_ID:
				return 0;
			case CPMappedRegister::CP_COUNTER1L_ID:
				return 0;
			case CPMappedRegister::CP_COUNTER1H_ID:
				return 0;
			case CPMappedRegister::CP_COUNTER2L_ID:
				return 0;
			case CPMappedRegister::CP_COUNTER2H_ID:
				return 0;
			case CPMappedRegister::CP_COUNTER3L_ID:
				return 0;
			case CPMappedRegister::CP_COUNTER3H_ID:
				return 0;
			case CPMappedRegister::CP_VC_CHKCNTL_ID:
				return 0;
			case CPMappedRegister::CP_VC_CHKCNTH_ID:
				return 0;
			case CPMappedRegister::CP_VC_MISSL_ID:
				return 0;
			case CPMappedRegister::CP_VC_MISSH_ID:
				return 0;
			case CPMappedRegister::CP_VC_STALLL_ID:
				return 0;
			case CPMappedRegister::CP_VC_STALLH_ID:
				return 0;
			case CPMappedRegister::CP_FRCLK_CNTL_ID:
				return 0;
			case CPMappedRegister::CP_FRCLK_CNTH_ID:
				return 0;
			case CPMappedRegister::CP_XF_ADDR_ID:
				return 0;
			case CPMappedRegister::CP_XF_DATAL_ID:
				return 0;
			case CPMappedRegister::CP_XF_DATAH_ID:
				return 0;
		}

		return 0;
	}

	void GXCore::CpWriteReg(CPMappedRegister id, uint16_t value)
	{
		switch (id)
		{
			case CPMappedRegister::CP_STATUS_ID:
				break;
			case CPMappedRegister::CP_ENABLE_ID:
				state.cpregs.cr = (uint16_t)value;

				// clear breakpoint
				if ((value & CP_CR_BPINTEN) == 0)
				{
					state.cpregs.sr &= ~CP_SR_BPINT;
				}

				if ((state.cpregs.sr & CP_SR_BPINT) == 0 && (state.cpregs.sr & CP_SR_OVF) == 0 && (state.cpregs.sr & CP_SR_UVF) == 0)
				{
					PIClearInt(PI_INTERRUPT_CP);
				}
				break;
			case CPMappedRegister::CP_CLR_ID:
				// clear watermark conditions
				if (value & CP_CLR_OVFCLR)
				{
					state.cpregs.sr &= ~CP_SR_OVF;
				}
				if (value & CP_CLR_UVFCLR)
				{
					state.cpregs.sr &= ~CP_SR_UVF;
				}

				if ((state.cpregs.sr & CP_SR_BPINT) == 0 && (state.cpregs.sr & CP_SR_OVF) == 0 && (state.cpregs.sr & CP_SR_UVF) == 0)
				{
					PIClearInt(PI_INTERRUPT_CP);
				}
				break;
			case CPMappedRegister::CP_MEMPERF_SEL_ID:
				break;
			case CPMappedRegister::CP_STM_LOW_ID:
				break;
			case CPMappedRegister::CP_FIFO_BASEL_ID:
				state.cpregs.basel = value & 0xffe0;
				break;
			case CPMappedRegister::CP_FIFO_BASEH_ID:
				state.cpregs.baseh = value;
				break;
			case CPMappedRegister::CP_FIFO_TOPL_ID:
				state.cpregs.topl = value & 0xffe0;
				break;
			case CPMappedRegister::CP_FIFO_TOPH_ID:
				state.cpregs.toph = value;
				break;
			case CPMappedRegister::CP_FIFO_HICNTL_ID:
				state.cpregs.himarkl = value & 0xffe0;
				break;
			case CPMappedRegister::CP_FIFO_HICNTH_ID:
				state.cpregs.himarkh = value;
				break;
			case CPMappedRegister::CP_FIFO_LOCNTL_ID:
				state.cpregs.lomarkl = value & 0xffe0;
				break;
			case CPMappedRegister::CP_FIFO_LOCNTH_ID:
				state.cpregs.lomarkh = value;
				break;
			case CPMappedRegister::CP_FIFO_COUNTL_ID:
				state.cpregs.cntl = value & 0xffe0;
				break;
			case CPMappedRegister::CP_FIFO_COUNTH_ID:
				state.cpregs.cnth = value;
				break;
			case CPMappedRegister::CP_FIFO_WPTRL_ID:
				state.cpregs.wrptrl = value & 0xffe0;
				break;
			case CPMappedRegister::CP_FIFO_WPTRH_ID:
				state.cpregs.wrptrh = value;
				break;
			case CPMappedRegister::CP_FIFO_RPTRL_ID:
				state.cpregs.rdptrl = value & 0xffe0;
				break;
			case CPMappedRegister::CP_FIFO_RPTRH_ID:
				state.cpregs.rdptrh = value;
				break;
			case CPMappedRegister::CP_FIFO_BRKL_ID:
				state.cpregs.bpptrl = value & 0xffe0;
				break;
			case CPMappedRegister::CP_FIFO_BRKH_ID:
				state.cpregs.bpptrh = value;
				break;
			case CPMappedRegister::CP_COUNTER0L_ID:
				break;
			case CPMappedRegister::CP_COUNTER0H_ID:
				break;
			case CPMappedRegister::CP_COUNTER1L_ID:
				break;
			case CPMappedRegister::CP_COUNTER1H_ID:
				break;
			case CPMappedRegister::CP_COUNTER2L_ID:
				break;
			case CPMappedRegister::CP_COUNTER2H_ID:
				break;
			case CPMappedRegister::CP_COUNTER3L_ID:
				break;
			case CPMappedRegister::CP_COUNTER3H_ID:
				break;
			case CPMappedRegister::CP_VC_CHKCNTL_ID:
				break;
			case CPMappedRegister::CP_VC_CHKCNTH_ID:
				break;
			case CPMappedRegister::CP_VC_MISSL_ID:
				break;
			case CPMappedRegister::CP_VC_MISSH_ID:
				break;
			case CPMappedRegister::CP_VC_STALLL_ID:
				break;
			case CPMappedRegister::CP_VC_STALLH_ID:
				break;
			case CPMappedRegister::CP_FRCLK_CNTL_ID:
				break;
			case CPMappedRegister::CP_FRCLK_CNTH_ID:
				break;
			case CPMappedRegister::CP_XF_ADDR_ID:
				break;
			case CPMappedRegister::CP_XF_DATAL_ID:
				break;
			case CPMappedRegister::CP_XF_DATAH_ID:
				break;
		}
	}

	uint32_t GXCore::PiCpReadReg(PI_CPMappedRegister id)
	{
		switch (id)
		{
			case PI_CPMappedRegister::PI_CPBAS_ID:
				return state.pi_cp_base & ~0x1f;
			case PI_CPMappedRegister::PI_CPTOP_ID:
				return state.pi_cp_top & ~0x1f;
			case PI_CPMappedRegister::PI_CPWRT_ID:
				return state.pi_cp_wrptr & ~0x1f;
			case PI_CPMappedRegister::PI_CPABT_ID:
				Report(Channel::GP, "PI CP Abort read not implemented!\n");
				return 0;
		}

		return 0;
	}

	void GXCore::PiCpWriteReg(PI_CPMappedRegister id, uint32_t value)
	{
		switch (id)
		{
		case PI_CPMappedRegister::PI_CPBAS_ID:
			state.pi_cp_base = value & ~0x1f;
			break;
		case PI_CPMappedRegister::PI_CPTOP_ID:
			state.pi_cp_top = value & ~0x1f;
			break;
		case PI_CPMappedRegister::PI_CPWRT_ID:
			state.pi_cp_wrptr = value & ~0x1f;
			break;
		case PI_CPMappedRegister::PI_CPABT_ID:
			Report(Channel::GP, "PI CP Abort write not implemented!\n");
			break;
		}
	}

	// This method handles all the magic that occurs when writing to GX FIFO Streaming Pointer.

	void GXCore::FifoWriteBurst(uint8_t data[32])
	{
		// PI FIFO

		state.pi_cp_wrptr &= ~PI_CPWRT_WRAP;

		PIWriteBurst(state.pi_cp_wrptr & RAMMASK, data);
		state.pi_cp_wrptr += 32;

		if (state.pi_cp_wrptr == state.pi_cp_top)
		{
			state.pi_cp_wrptr = state.pi_cp_base;
			state.pi_cp_wrptr |= PI_CPWRT_WRAP;
		}

		// CP FIFO

		if (state.cpregs.cr & CP_CR_WPINC)
		{
			state.cpregs.wrptr += 32;

			if (state.cpregs.wrptr == state.cpregs.top)
			{
				state.cpregs.wrptr = state.cpregs.base;
			}

			// All other work is done by CommandProcessor thread.
		}
	}

	// show PI fifo configuration
	void GXCore::DumpPIFIFO()
	{
		Report(Channel::Norm, "PI fifo configuration");
		Report(Channel::Norm, "   base :0x%08X", state.pi_cp_base);
		Report(Channel::Norm, "   top  :0x%08X", state.pi_cp_top);
		Report(Channel::Norm, "   wrptr:0x%08X", state.pi_cp_wrptr);
		Report(Channel::Norm, "   wrap :%i", (state.pi_cp_wrptr & PI_CPWRT_WRAP) ? (1) : (0));
	}

	// show CP fifo configuration
	void GXCore::DumpCPFIFO()
	{
		// fifo modes
		char* md = (state.cpregs.cr & CP_CR_WPINC) ? ((char*)"immediate ") : ((char*)"multi-");
		char bp = (state.cpregs.cr & CP_CR_BPEN) ? ('B') : ('b');    // breakpoint
		char lw = (state.cpregs.cr & CP_CR_UVFEN) ? ('U') : ('u');    // low-wmark
		char hw = (state.cpregs.cr & CP_CR_OVFEN) ? ('O') : ('o');    // high-wmark

		Report(Channel::Norm, "CP %sfifo configuration:%c%c%c", md, bp, lw, hw);
		Report(Channel::Norm, " status :0x%08X", state.cpregs.sr);
		Report(Channel::Norm, " enable :0x%08X", state.cpregs.cr);
		Report(Channel::Norm, "   base :0x%08X", state.cpregs.base);
		Report(Channel::Norm, "   top  :0x%08X", state.cpregs.top);
		Report(Channel::Norm, "   low  :0x%08X", state.cpregs.lomark);
		Report(Channel::Norm, "   high :0x%08X", state.cpregs.himark);
		Report(Channel::Norm, "   cnt  :0x%08X", state.cpregs.cnt);
		Report(Channel::Norm, "   wrptr:0x%08X", state.cpregs.wrptr);
		Report(Channel::Norm, "   rdptr:0x%08X", state.cpregs.rdptr);
		Report(Channel::Norm, "   break:0x%08X", state.cpregs.bpptr);
	}

	#pragma endregion "Dealing with registers"


	#pragma region "FIFO Processing"

	FifoProcessor::FifoProcessor(GXCore* gx)
	{
		gxcore = gx;
		fifo = new uint8_t[fifoSize];
		memset(fifo, 0, fifoSize);
		allocated = true;
	}

	FifoProcessor::FifoProcessor(GXCore* gx, uint8_t* fifoPtr, size_t size)
	{
		gxcore = gx;
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

	void FifoProcessor::WriteBytes(uint8_t dataPtr[32])
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

		while (EnoughToExecute())
		{
			ExecuteCommand();
		}
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
			case CPCommand::CP_CMD_NOP:
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
				Report(Channel::GP, "GX: Unsupported opcode: 0x%02X\n", cmd);
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

}
