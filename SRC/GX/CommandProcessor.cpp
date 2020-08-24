// PI and CP FIFO processing

#include "pch.h"

using namespace Debug;

namespace GX
{
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

		int64_t ticks = Gekko::Gekko->GetTicks();
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
			gx->ProcessFifo(&mi.ram[gx->state.cpregs.rdptr & RAMMASK]);
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
		return 0;
	}

	void GXCore::CpWriteReg(CPMappedRegister id, uint16_t value)
	{

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

		MIWriteBurst(state.pi_cp_wrptr & RAMMASK, data);
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
		Report(Channel::Norm, "control :0x%08X", state.cpregs.cr);
		Report(Channel::Norm, " enable :0x%08X", state.cpregs.sr);
		Report(Channel::Norm, "   base :0x%08X", state.cpregs.base);
		Report(Channel::Norm, "   top  :0x%08X", state.cpregs.top);
		Report(Channel::Norm, "   low  :0x%08X", state.cpregs.lomark);
		Report(Channel::Norm, "   high :0x%08X", state.cpregs.himark);
		Report(Channel::Norm, "   cnt  :0x%08X", state.cpregs.cnt);
		Report(Channel::Norm, "   wrptr:0x%08X", state.cpregs.wrptr);
		Report(Channel::Norm, "   rdptr:0x%08X", state.cpregs.rdptr);
		Report(Channel::Norm, "   break:0x%08X", state.cpregs.bpptr);
	}

	void GXCore::ProcessFifo(uint8_t data[32])
	{
		// GXWriteFifo(data);
	}

}
