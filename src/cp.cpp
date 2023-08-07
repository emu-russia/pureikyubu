// CP - command processor
#include "pch.h"

// TODO: It's a bit crooked right now after refactoring, but will settle with time

using namespace Debug;

size_t pe_done_num;   // number of drawdone (PE_FINISH) events

void CPDrawDone()
{
	pe_done_num++;
	if (pe_done_num == 1)
	{
		vi.xfb = false;     // disable VI output
	}

	Flipper::Gx->CPDrawDoneCallback();
}

void CPDrawToken(uint16_t tokenValue)
{
	vi.xfb = false;     // disable VI output

	Flipper::Gx->CPDrawTokenCallback(tokenValue);
}

//
// Stubs
//

static void CPRegRead(uint32_t addr, uint32_t* reg)
{
	*reg = Flipper::Gx->CpReadReg((GX::CPMappedRegister)((addr & 0xFF) >> 1));
}

static void CPRegWrite(uint32_t addr, uint32_t data)
{
	Flipper::Gx->CpWriteReg((GX::CPMappedRegister)((addr & 0xFF) >> 1), data);
}

// init

void CPOpen()
{
	Report(Channel::CP, "Command processor (for GX)\n");

	pe_done_num = 0;

	// Command Processor
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_STATUS_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_ENABLE_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_CLR_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_MEMPERF_SEL_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_STM_LOW_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_BASEL_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_BASEH_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_TOPL_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_TOPH_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_HICNTL_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_HICNTH_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_LOCNTL_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_LOCNTH_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_COUNTL_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_COUNTH_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_WPTRL_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_WPTRH_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_RPTRL_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_RPTRH_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_BRKL_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_BRKH_ID), CPRegRead, CPRegWrite);

	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_COUNTER0L_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_COUNTER0H_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_COUNTER1L_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_COUNTER1H_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_COUNTER2L_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_COUNTER2H_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_COUNTER3L_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_COUNTER3H_ID), CPRegRead, CPRegWrite);

	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_VC_CHKCNTL_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_VC_CHKCNTH_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_VC_MISSL_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_VC_MISSH_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_VC_STALLL_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_VC_STALLH_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FRCLK_CNTL_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FRCLK_CNTH_ID), CPRegRead, CPRegWrite);

	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_XF_ADDR_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_XF_DATAL_ID), CPRegRead, CPRegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_XF_DATAH_ID), CPRegRead, CPRegWrite);
}

void CPClose()
{
}


namespace GX
{
	#pragma region "Dealing with registers"

	void GXCore::CP_BREAK()
	{
		if (state.cpregs.cr & CP_CR_BPINTEN && (state.cpregs.sr & CP_SR_BPINT) == 0)
		{
			state.cpregs.sr |= CP_SR_BPINT;
			PIAssertInt(PI_INTERRUPT_CP);
			Report(Channel::CP, "BREAK\n");
		}
	}

	void GXCore::CP_OVF()
	{
		if (state.cpregs.cr & CP_CR_OVFEN && (state.cpregs.sr & CP_SR_OVF) == 0)
		{
			state.cpregs.sr |= CP_SR_OVF;
			PIAssertInt(PI_INTERRUPT_CP);
			Report(Channel::CP, "OVF\n");
		}
	}

	void GXCore::CP_UVF()
	{
		if (state.cpregs.cr & CP_CR_UVFEN && (state.cpregs.sr & CP_SR_UVF) == 0)
		{
			state.cpregs.sr |= CP_SR_UVF;
			PIAssertInt(PI_INTERRUPT_CP);
			Report(Channel::CP, "UVF\n");
		}
	}

	void GXCore::GXWriteFifo(uint8_t dataPtr[32])
	{
		fifo->WriteBytes(dataPtr);

		while (fifo->EnoughToExecute())
		{
			GxCommand(fifo);
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

			gx->GXWriteFifo(&mi.ram[gx->state.cpregs.rdptr & RAMMASK]);

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
		Report(Channel::Norm, "PI fifo configuration\n");
		Report(Channel::Norm, "   base :0x%08X\n", state.pi_cp_base);
		Report(Channel::Norm, "   top  :0x%08X\n", state.pi_cp_top);
		Report(Channel::Norm, "   wrptr:0x%08X\n", state.pi_cp_wrptr);
		Report(Channel::Norm, "   wrap :%i\n", (state.pi_cp_wrptr & PI_CPWRT_WRAP) ? (1) : (0));
	}

	// show CP fifo configuration
	void GXCore::DumpCPFIFO()
	{
		// fifo modes
		char* md = (state.cpregs.cr & CP_CR_WPINC) ? ((char*)"immediate ") : ((char*)"multi-");
		char bp = (state.cpregs.cr & CP_CR_BPEN) ? ('B') : ('b');    // breakpoint
		char lw = (state.cpregs.cr & CP_CR_UVFEN) ? ('U') : ('u');    // low-wmark
		char hw = (state.cpregs.cr & CP_CR_OVFEN) ? ('O') : ('o');    // high-wmark

		Report(Channel::Norm, "CP %sfifo configuration:%c%c%c\n", md, bp, lw, hw);
		Report(Channel::Norm, " status :0x%08X\n", state.cpregs.sr);
		Report(Channel::Norm, " enable :0x%08X\n", state.cpregs.cr);
		Report(Channel::Norm, "   base :0x%08X\n", state.cpregs.base);
		Report(Channel::Norm, "   top  :0x%08X\n", state.cpregs.top);
		Report(Channel::Norm, "   low  :0x%08X\n", state.cpregs.lomark);
		Report(Channel::Norm, "   high :0x%08X\n", state.cpregs.himark);
		Report(Channel::Norm, "   cnt  :0x%08X\n", state.cpregs.cnt);
		Report(Channel::Norm, "   wrptr:0x%08X\n", state.cpregs.wrptr);
		Report(Channel::Norm, "   rdptr:0x%08X\n", state.cpregs.rdptr);
		Report(Channel::Norm, "   break:0x%08X\n", state.cpregs.bpptr);
	}

	// index range = 00..FF
	// reg size = 32 bit
	void GXCore::loadCPReg(size_t index, uint32_t value, FifoProcessor* gxfifo)
	{
		state.cpLoads++;

		if (GpRegsLog)
		{
			Report(Channel::GP, "Load CP: index: 0x%02X, data: 0x%08X\n", index, value);
		}

		switch(index)
		{
			case CP_MATINDEX_A_ID:
			{
				state.cp.matIndexA.bits = value;
			}
			return;

			case CP_MATINDEX_B_ID:
			{
				state.cp.matIndexB.bits = value;
			}
			return;

			case CP_VCD_LO_ID:
			{
				state.cp.vcdLo.bits = value;
				FifoReconfigure(gxfifo);
			}
			return;

			case CP_VCD_HI_ID:
			{
				state.cp.vcdHi.bits = value;
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
				state.cp.vatA[index & 7].bits = value;
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
				state.cp.vatB[index & 7].bits = value;
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
				state.cp.vatC[index & 7].bits = value;
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
				state.cp.arrayBase[index & 0xf].bits = value;
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
				state.cp.arrayStride[index & 0xf].bits = value & 0xFF;
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


	// Helper function
	std::string GXCore::AttrToString(VertexAttr attr)
	{
		switch (attr)
		{
			case VertexAttr::VTX_POSMATIDX:     return "Position Matrix Index";
			case VertexAttr::VTX_TEX0MTXIDX:    return "Texture Coordinate 0 Matrix Index";
			case VertexAttr::VTX_TEX1MTXIDX:    return "Texture Coordinate 1 Matrix Index";
			case VertexAttr::VTX_TEX2MTXIDX:    return "Texture Coordinate 2 Matrix Index";
			case VertexAttr::VTX_TEX3MTXIDX:    return "Texture Coordinate 3 Matrix Index";
			case VertexAttr::VTX_TEX4MTXIDX:    return "Texture Coordinate 4 Matrix Index";
			case VertexAttr::VTX_TEX5MTXIDX:    return "Texture Coordinate 5 Matrix Index";
			case VertexAttr::VTX_TEX6MTXIDX:    return "Texture Coordinate 6 Matrix Index";
			case VertexAttr::VTX_TEX7MTXIDX:    return "Texture Coordinate 7 Matrix Index";
			case VertexAttr::VTX_POS:           return "Position";
			case VertexAttr::VTX_NRM:           return "Normal or Normal/Binormal/Tangent";
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
			case VertexAttr::VTX_MAX_ATTR:		return "MAX attr";
		}
		return "Unknown attribute";
	}

	// calculate size of current vertex
	int GXCore::gx_vtxsize(unsigned v)
	{
		int vtxsize = 0;
		static int cntp[] = { 2, 3 };
		static int cntn[] = { 3, 9 };
		static int cntt[] = { 1, 2 };
		static int fmtsz[] = { 1, 1, 2, 2, 4 };
		static int cfmtsz[] = { 2, 3, 4, 2, 4, 4 };

		if (state.cp.vcdLo.PosNrmMatIdx) vtxsize++;
		if (state.cp.vcdLo.Tex0MatIdx) vtxsize++;
		if (state.cp.vcdLo.Tex1MatIdx) vtxsize++;
		if (state.cp.vcdLo.Tex2MatIdx) vtxsize++;
		if (state.cp.vcdLo.Tex3MatIdx) vtxsize++;
		if (state.cp.vcdLo.Tex4MatIdx) vtxsize++;
		if (state.cp.vcdLo.Tex5MatIdx) vtxsize++;
		if (state.cp.vcdLo.Tex6MatIdx) vtxsize++;
		if (state.cp.vcdLo.Tex7MatIdx) vtxsize++;

		// Position

		switch (state.cp.vcdLo.Position)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[state.cp.vatA[v].posfmt] * cntp[state.cp.vatA[v].poscnt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		// Normal

		switch (state.cp.vcdLo.Normal)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[state.cp.vatA[v].nrmfmt] * cntn[state.cp.vatA[v].nrmcnt];
				break;
			case VCD_INDEX8:
				if (state.cp.vatA[v].nrmidx3) vtxsize += 3;
				else vtxsize += 1;
				break;
			case VCD_INDEX16:
				if (state.cp.vatA[v].nrmidx3) vtxsize += 2 * 3;
				else vtxsize += 2;
				break;
		}

		// Colors

		switch (state.cp.vcdLo.Color0)
		{
			case VCD_DIRECT:
				vtxsize += cfmtsz[state.cp.vatA[v].col0fmt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		switch (state.cp.vcdLo.Color1)
		{
			case VCD_DIRECT:
				vtxsize += cfmtsz[state.cp.vatA[v].col1fmt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		// TexCoords

		switch (state.cp.vcdHi.Tex0Coord)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[state.cp.vatA[v].tex0fmt] * cntt[state.cp.vatA[v].tex0cnt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		switch (state.cp.vcdHi.Tex1Coord)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[state.cp.vatB[v].tex1fmt] * cntt[state.cp.vatB[v].tex1cnt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		switch (state.cp.vcdHi.Tex2Coord)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[state.cp.vatB[v].tex2fmt] * cntt[state.cp.vatB[v].tex2cnt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		switch (state.cp.vcdHi.Tex3Coord)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[state.cp.vatB[v].tex3fmt] * cntt[state.cp.vatB[v].tex3cnt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		switch (state.cp.vcdHi.Tex4Coord)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[state.cp.vatB[v].tex4fmt] * cntt[state.cp.vatB[v].tex4cnt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		switch (state.cp.vcdHi.Tex5Coord)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[state.cp.vatC[v].tex5fmt] * cntt[state.cp.vatC[v].tex5cnt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		switch (state.cp.vcdHi.Tex6Coord)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[state.cp.vatC[v].tex6fmt] * cntt[state.cp.vatC[v].tex6cnt];
				break;
			case VCD_INDEX8:
				vtxsize += 1;
				break;
			case VCD_INDEX16:
				vtxsize += 2;
				break;
		}

		switch (state.cp.vcdHi.Tex7Coord)
		{
			case VCD_DIRECT:
				vtxsize += fmtsz[state.cp.vatC[v].tex7fmt] * cntt[state.cp.vatC[v].tex7cnt];
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

	void GXCore::FifoReconfigure(FifoProcessor *gxfifo)
	{
		for (unsigned v = 0; v < 8; v++)
		{
			gxfifo->vertexSize[v] = gx_vtxsize(v);
		}
	}

	void * GXCore::GetArrayPtr(ArrayId arrayId, int idx, int compSize)
	{
		uint32_t address = state.cp.arrayBase[(size_t)arrayId].Base + 
			(uint32_t)idx * state.cp.arrayStride[(size_t)arrayId].Stride;
		return MIGetMemoryPointerForVertexArray(address);
	}

	void GXCore::FetchComp(float* comp, int count, int type, int fmt, int shft, FifoProcessor* gxfifo, ArrayId arrayId)
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

	void GXCore::FetchNorm(float* comp, int count, int type, int fmt, int shft, FifoProcessor* gxfifo, ArrayId arrayId, bool nrmidx3)
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

	Color GXCore::FetchColor(int type, int fmt, FifoProcessor* gxfifo, ArrayId arrayId)
	{
		void* ptr;
		Color col;
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
	void GXCore::FifoWalk(unsigned vatnum, FifoProcessor* gxfifo)
	{
		// overrided by 'mtxidx' attributes
		state.xf.posidx = state.xf.matIdxA.PosNrmMatIdx;
		state.xf.texidx[0] = state.xf.matIdxA.Tex0MatIdx;
		state.xf.texidx[1] = state.xf.matIdxA.Tex1MatIdx;
		state.xf.texidx[2] = state.xf.matIdxA.Tex2MatIdx;
		state.xf.texidx[3] = state.xf.matIdxA.Tex3MatIdx;
		state.xf.texidx[4] = state.xf.matIdxB.Tex4MatIdx;
		state.xf.texidx[5] = state.xf.matIdxB.Tex5MatIdx;
		state.xf.texidx[6] = state.xf.matIdxB.Tex6MatIdx;
		state.xf.texidx[7] = state.xf.matIdxB.Tex7MatIdx;

		// Matrix Index

		if (state.cp.vcdLo.PosNrmMatIdx)
		{
			state.xf.posidx = gxfifo->Read8();
		}

		if (state.cp.vcdLo.Tex0MatIdx)
		{
			state.xf.texidx[0] = gxfifo->Read8();
		}

		if (state.cp.vcdLo.Tex1MatIdx)
		{
			state.xf.texidx[1] = gxfifo->Read8();
		}

		if (state.cp.vcdLo.Tex2MatIdx)
		{
			state.xf.texidx[2] = gxfifo->Read8();
		}

		if (state.cp.vcdLo.Tex3MatIdx)
		{
			state.xf.texidx[3] = gxfifo->Read8();
		}

		if (state.cp.vcdLo.Tex4MatIdx)
		{
			state.xf.texidx[4] = gxfifo->Read8();
		}

		if (state.cp.vcdLo.Tex5MatIdx)
		{
			state.xf.texidx[5] = gxfifo->Read8();
		}

		if (state.cp.vcdLo.Tex6MatIdx)
		{
			state.xf.texidx[6] = gxfifo->Read8();
		}

		if (state.cp.vcdLo.Tex7MatIdx)
		{
			state.xf.texidx[7] = gxfifo->Read8();
		}

		// Position

		vtx->pos[0] = vtx->pos[1] = vtx->pos[2] = 1.0f;

		FetchComp(vtx->pos,
			state.cp.vatA[vatnum].poscnt == VCNT_POS_XYZ ? 3 : 2,
			state.cp.vcdLo.Position,
			state.cp.vatA[vatnum].posfmt,
			state.cp.vatA[vatnum].bytedeq ? state.cp.vatA[vatnum].posshft : 0,
			gxfifo,
			ArrayId::Pos);

		// Normal

		vtx->nrm[0] = vtx->nrm[1] = vtx->nrm[2] = 1.0f;
		vtx->nrm[3] = vtx->nrm[4] = vtx->nrm[5] = 1.0f;
		vtx->nrm[6] = vtx->nrm[7] = vtx->nrm[8] = 1.0f;

		int nrmshft = 0;

		switch (state.cp.vatA[vatnum].nrmfmt)
		{
			case VFMT_S8:
				nrmshft = 6;
				break;
			case VFMT_S16:
				nrmshft = 14;
				break;
		}

		FetchNorm(vtx->nrm,
			state.cp.vatA[vatnum].nrmcnt == VCNT_NRM_NBT ? 9 : 3,
			state.cp.vcdLo.Normal,
			state.cp.vatA[vatnum].nrmfmt,
			nrmshft,
			gxfifo,
			ArrayId::Nrm,
			state.cp.vatA[vatnum].nrmidx3 ? true : false);

		// Color0 

		vtx->col[0] = FetchColor(state.cp.vcdLo.Color0, state.cp.vatA[vatnum].col0fmt, gxfifo, ArrayId::Color0);

		// Color1

		vtx->col[1] = FetchColor(state.cp.vcdLo.Color1, state.cp.vatA[vatnum].col1fmt, gxfifo, ArrayId::Color1);

		// TexNCoord

		vtx->tcoord[0][0] = vtx->tcoord[0][1] = 1.0f;

		FetchComp(vtx->tcoord[0],
			state.cp.vatA[vatnum].tex0cnt == VCNT_TEX_ST ? 2 : 1,
			state.cp.vcdHi.Tex0Coord,
			state.cp.vatA[vatnum].tex0fmt,
			state.cp.vatA[vatnum].bytedeq ? state.cp.vatA[vatnum].tex0shft : 0,
			gxfifo,
			ArrayId::Tex0Coord);

		vtx->tcoord[1][0] = vtx->tcoord[1][1] = 1.0f;

		FetchComp(vtx->tcoord[1],
			state.cp.vatB[vatnum].tex1cnt == VCNT_TEX_ST ? 2 : 1,
			state.cp.vcdHi.Tex1Coord,
			state.cp.vatB[vatnum].tex1fmt,
			state.cp.vatA[vatnum].bytedeq ? state.cp.vatB[vatnum].tex1shft : 0,
			gxfifo,
			ArrayId::Tex1Coord);

		vtx->tcoord[2][0] = vtx->tcoord[2][1] = 1.0f;

		FetchComp(vtx->tcoord[2],
			state.cp.vatB[vatnum].tex2cnt == VCNT_TEX_ST ? 2 : 1,
			state.cp.vcdHi.Tex2Coord,
			state.cp.vatB[vatnum].tex2fmt,
			state.cp.vatA[vatnum].bytedeq ? state.cp.vatB[vatnum].tex2shft : 0,
			gxfifo,
			ArrayId::Tex2Coord);

		vtx->tcoord[3][0] = vtx->tcoord[3][1] = 1.0f;

		FetchComp(vtx->tcoord[3],
			state.cp.vatB[vatnum].tex3cnt == VCNT_TEX_ST ? 2 : 1,
			state.cp.vcdHi.Tex3Coord,
			state.cp.vatB[vatnum].tex3fmt,
			state.cp.vatA[vatnum].bytedeq ? state.cp.vatB[vatnum].tex3shft : 0,
			gxfifo,
			ArrayId::Tex3Coord);

		vtx->tcoord[4][0] = vtx->tcoord[4][1] = 1.0f;

		FetchComp(vtx->tcoord[4],
			state.cp.vatB[vatnum].tex4cnt == VCNT_TEX_ST ? 2 : 1,
			state.cp.vcdHi.Tex4Coord,
			state.cp.vatB[vatnum].tex4fmt,
			state.cp.vatA[vatnum].bytedeq ? state.cp.vatC[vatnum].tex4shft : 0,
			gxfifo,
			ArrayId::Tex4Coord);

		vtx->tcoord[5][0] = vtx->tcoord[5][1] = 1.0f;

		FetchComp(vtx->tcoord[5],
			state.cp.vatC[vatnum].tex5cnt == VCNT_TEX_ST ? 2 : 1,
			state.cp.vcdHi.Tex5Coord,
			state.cp.vatC[vatnum].tex5fmt,
			state.cp.vatA[vatnum].bytedeq ? state.cp.vatC[vatnum].tex5shft : 0,
			gxfifo,
			ArrayId::Tex5Coord);

		vtx->tcoord[6][0] = vtx->tcoord[6][1] = 1.0f;

		FetchComp(vtx->tcoord[6],
			state.cp.vatC[vatnum].tex6cnt == VCNT_TEX_ST ? 2 : 1,
			state.cp.vcdHi.Tex6Coord,
			state.cp.vatC[vatnum].tex6fmt,
			state.cp.vatA[vatnum].bytedeq ? state.cp.vatC[vatnum].tex6shft : 0,
			gxfifo,
			ArrayId::Tex6Coord);

		vtx->tcoord[7][0] = vtx->tcoord[7][1] = 1.0f;

		FetchComp(vtx->tcoord[7],
			state.cp.vatC[vatnum].tex7cnt == VCNT_TEX_ST ? 2 : 1,
			state.cp.vcdHi.Tex7Coord,
			state.cp.vatC[vatnum].tex7fmt,
			state.cp.vatA[vatnum].bytedeq ? state.cp.vatC[vatnum].tex7shft : 0,
			gxfifo,
			ArrayId::Tex7Coord);
	}

	void GXCore::GxBadFifo(uint8_t command)
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
			state.cp.vcdLo.PosNrmMatIdx,
			state.cp.vcdLo.Tex0MatIdx, state.cp.vcdHi.Tex0Coord,
			state.cp.vcdLo.Tex1MatIdx, state.cp.vcdHi.Tex1Coord,
			state.cp.vcdLo.Tex2MatIdx, state.cp.vcdHi.Tex2Coord,
			state.cp.vcdLo.Tex3MatIdx, state.cp.vcdHi.Tex3Coord,
			state.cp.vcdLo.Tex4MatIdx, state.cp.vcdHi.Tex4Coord,
			state.cp.vcdLo.Tex5MatIdx, state.cp.vcdHi.Tex5Coord,
			state.cp.vcdLo.Tex6MatIdx, state.cp.vcdHi.Tex6Coord,
			state.cp.vcdLo.Tex7MatIdx, state.cp.vcdHi.Tex7Coord,
			state.cp.vcdLo.Position,
			state.cp.vcdLo.Normal,
			state.cp.vcdLo.Color0,
			state.cp.vcdLo.Color1
		);
	}

	void GXCore::GxCommand(FifoProcessor* gxfifo)
	{
		if(frame_done)
		{
			GL_OpenSubsystem();
			GL_BeginFrame();
			frame_done = 0;
		}

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
				uint32_t physAddress = gxfifo->Read32() & RAMMASK;
				uint8_t* fifoPtr = &mi.ram[physAddress];
				size_t size = gxfifo->Read32() & ~0x1f;

				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_CALL_DL: addr: 0x%08X, size: %i\n", physAddress, size);
				}

				FifoProcessor* callDlFifo = new FifoProcessor(this, fifoPtr, size);

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
				loadBPReg(word >> 24, word & 0xffffff);
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

				loadXFRegs(index, len, gxfifo);
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
				static   Vertex   quad[4];
				unsigned vatnum = cmd & 7;
				unsigned vtxnum = gxfifo->Read16();
				usevat = vatnum;
				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_DRAW_QUAD: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
				}
															/*/
					1---2       tri1: 0-1-2
					|  /|       tri2: 0-2-3
					| / |
					|/  |
					0---3
															/*/

				while(vtxnum > 0)
				{
					for(unsigned n=0; n<4; n++)
					{
						vtx = &quad[n];
						FifoWalk(vatnum, gxfifo);
					}
					if (!disableDraw)
					{
						GL_RenderTriangle(&quad[0], &quad[1], &quad[2]);
						GL_RenderTriangle(&quad[0], &quad[2], &quad[3]);
					}
					vtxnum -= 4;
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
				static   Vertex   tri[3];
				unsigned vatnum = cmd & 7;
				unsigned vtxnum = gxfifo->Read16();
				usevat = vatnum;
				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_DRAW_TRIANGLE: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
				}
															/*/
					1---2       tri: 0-1-2
					|  /
					| /
					|/
					0  
															/*/

				while(vtxnum > 0)
				{
					for(unsigned n=0; n<3; n++)
					{
						vtx = &tri[n];
						FifoWalk(vatnum, gxfifo);
					}
					if (!disableDraw)
					{
						GL_RenderTriangle(&tri[0], &tri[1], &tri[2]);
					}
					vtxnum -= 3;
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
				static   Vertex   tri[3];
				unsigned c = 2, order[3] = { 0, 1, 2 }, tmp;
				unsigned vatnum = cmd & 7;
				unsigned vtxnum = gxfifo->Read16();
				usevat = vatnum;
				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_DRAW_STRIP: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
				}
															/*/
						1---3---5   tri1: 0-1-2
					   /|  /|  /    tri2: 1-2-3
					  / | / | /     tri3: 2-3-4
					 /  |/  |/      tri4: 3-4-5
					0---2---4       ...
															/*/
				if(vtxnum == 0) break;
				assert(vtxnum >= 3);

				vtx = &tri[0];
				FifoWalk(vatnum, gxfifo);
				vtx = &tri[1];
				FifoWalk(vatnum, gxfifo);
				vtxnum -= 2;

				while(vtxnum-- > 0)
				{
					vtx = &tri[c++];
					FifoWalk(vatnum, gxfifo);
					if(c > 2) c = 0;

					if (!disableDraw)
					{
						GL_RenderTriangle(
							&tri[order[0]],
							&tri[order[1]],
							&tri[order[2]]
						);
					}

					tmp      = order[0];
					order[0] = order[1];
					order[1] = order[2];
					order[2] = tmp;
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
				static   Vertex   tri[3];
				unsigned c = 2, order[2] = { 1, 2 }, tmp;
				unsigned vatnum = cmd & 7;
				unsigned vtxnum = gxfifo->Read16();
				usevat = vatnum;
				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_DRAW_FAN: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
				}
															/*/
					1---2---3   tri1: 0-1-2
					|  /  _/    tri2: 0-2-3
					| / _/      trin: 0-[n-1]-n
					|/_/    
					0/
															/*/
				if(vtxnum == 0) break;
				assert(vtxnum >= 3);

				vtx = &tri[0];
				FifoWalk(vatnum, gxfifo);
				vtx = &tri[1];
				FifoWalk(vatnum, gxfifo);
				vtxnum -= 2;

				while(vtxnum-- > 0)
				{
					vtx = &tri[c];
					FifoWalk(vatnum, gxfifo);
					c = (c == 2) ? (c = 1) : (c = 2);

					if (!disableDraw)
					{
						GL_RenderTriangle(
							&tri[0],
							&tri[order[0]],
							&tri[order[1]]
						);
					}

					// order[0] <-> order[1]
					tmp      = order[0];
					order[0] = order[1];
					order[1] = tmp;
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
				static   Vertex   v[2];
				unsigned vatnum = cmd & 7;
				unsigned vtxnum = gxfifo->Read16();
				usevat = vatnum;
				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_DRAW_LINE: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
				}
															/*/
						1   3   5
					   /   /   / 
					  /   /   / 
					 /   /   /      
					0   2   4       
															/*/
				if(vtxnum == 0) break;

				while(vtxnum > 0)
				{
					vtx = &v[0];
					FifoWalk(vatnum, gxfifo);
					vtx = &v[1];
					FifoWalk(vatnum, gxfifo);

					if (!disableDraw)
					{
						GL_RenderLine(&v[0], &v[1]);
					}
					vtxnum -= 2;
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
				static   Vertex   v[2];
				unsigned c = 1, order[2] = { 0, 1 }, tmp;
				unsigned vatnum = cmd & 7;
				unsigned vtxnum = gxfifo->Read16();
				usevat = vatnum;
				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_DRAW_LINESTRIP: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
				}
															/*/
						1   3   5
					   /|  /|  / 
					  / | / | /  
					 /  |/  |/   
					0   2   4    
															/*/
				if(vtxnum == 0) break;
				assert(vtxnum >= 2);

				vtx = &v[0];
				FifoWalk(vatnum, gxfifo);
				vtxnum--;

				while(vtxnum-- > 0)
				{
					vtx = &v[c++];
					FifoWalk(vatnum, gxfifo);
					if(c > 1) c = 0;

					if (!disableDraw)
					{
						GL_RenderLine(
							&v[order[0]],
							&v[order[1]]
						);
					}

					tmp      = order[0];
					order[0] = order[1];
					order[1] = tmp;
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
				static  Vertex  p;
				unsigned vatnum = cmd & 7;
				unsigned vtxnum = gxfifo->Read16();
				usevat = vatnum;
				if (logDrawCommands)
				{
					Report(Channel::GP, "CP_CMD_DRAW_POINT: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
				}
															/*/
					0---0       tri: 0-0-0 (1x1x1 tri)
					|  /
					| /
					|/
					0  
															/*/

				while(vtxnum-- > 0)
				{
					vtx = &p;
					FifoWalk(vatnum, gxfifo);
					if (!disableDraw)
					{
						GL_RenderPoint(vtx);
					}
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
}
