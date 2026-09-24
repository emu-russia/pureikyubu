// SI - serial interface.
//
// The SI is the block that frames the four controller sockets. What is plugged into a socket is a
// device of the peripheral pool (see peripherals.h): the automatic poll asks the device for the
// state of its controls, and a communication transfer hands the device the command bytes of the
// channel and takes its answer back.
#include "pch.h"

// IMPORTANT : transfer will never be aborted by communication error, 
// so all ERROR bits/status in SI regs are not used in emulator.

// polling intervals are also not critical. all controllers are polled
// before VI blank (in vi.cpp)

// SI_EXILK is not used (same as EXI clock timing, because of instant EXI transfers)

// si.cpp polling schematics :
/*/
	 -----
	| OUT |
	|-----| <
	| INH |  |     ----------       --------------------
	|-----|  |----| PADState |<----| peripheral device  |
	| INL |  |     ----------       --------------------
	 -----  <
/*/

// Note : digital L and R are only set when its analog key is pressed all the way down;

namespace Flipper
{

	// ---------------------------------------------------------------------------
	// dispatch command

	//
	// command data are sending via communication buffer,
	// the device of the channel parses the command opcode and forms the response
	// data packet, written back to the communication buffer
	//

	void SerialInterface::SICommand(int chan, int outlen, int inlen, uint8_t* ptr)
	{
		// COMERR is re-evaluated on each COM completion (serial-interface.md 7.4).
		SI_COMCSR_REG &= ~SI_COMCSR_COMERR;

		if (!Peripherals::Instance().TransferSI(chan, outlen, inlen, ptr))
		{
			// Nothing is plugged into the channel, so nothing answers the transfer: the channel
			// latches its no-response error and the transfer is the last one that failed
			// (serial-interface.md 7.1, 7.4). A guest that probes a socket reads that error to tell
			// an empty one from a device that answered; the response bytes alone cannot tell them
			// apart, because with no device driving the line the SI only hears itself.
			SI_SR_REG |= (SI_SR_NOREP0 >> (chan * 8));
			SI_COMCSR_REG |= SI_COMCSR_COMERR;

			// The line is left idle, and the receiver reads the idle polarity for every bit cell of
			// the response, so every response byte reads as all ones (3.2, 6.4). The Wind Waker's pad
			// code takes the device type out of these bytes and waits for the probe of a socket it
			// believes to hold a controller to settle: leaving the command bytes of the transfer in
			// the buffer instead makes it wait for a device that is not there.
			for (int i = 0; i < inlen; i++)
			{
				ptr[i] = 0xff;
			}
		}
	}

	void SerialInterface::SIClearInterrupt()
	{
		if ((SI_COMCSR_REG & SI_COMCSR_RDSTINT) == 0 && (SI_COMCSR_REG & SI_COMCSR_TCINT) == 0)
		{
			HW->pi->PIClearInt(PI_INTERRUPT_SI);
		}
	}

	// ---------------------------------------------------------------------------
	// register traps

	//
	// command output buffers are read/write
	//

	void SerialInterface::si_wr_out_hi(int chan, uint32_t data)
	{
		si.out[chan] &= 0x0000ffff;
		si.out[chan] |= data << 16;
	}

	void SerialInterface::si_wr_out_lo(int chan, uint32_t mask, uint32_t data)
	{
		si.out[chan] &= 0xffff0000;
		si.out[chan] |= (uint16_t)data;

		// The visible register no longer matches the shadow one until SISR[WR] copies it.
		SI_SR_REG |= mask;

		// control motor
		if (si.out[chan] == 0x00400000) Peripherals::Instance().SetMotorSI(chan, PAD_MOTOR_STOP);
		else if (si.out[chan] == 0x00400001) Peripherals::Instance().SetMotorSI(chan, PAD_MOTOR_RUMBLE);
		else if (si.out[chan] == 0x00400002) Peripherals::Instance().SetMotorSI(chan, PAD_MOTOR_STOP_HARD);
	}

	/* ******* CHAN 0 ******* */

	void SerialInterface::si_wr_out0_hi(uint32_t addr, uint32_t data, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_wr_out_hi(0, data); }
	void SerialInterface::si_wr_out0_lo(uint32_t addr, uint32_t data, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_wr_out_lo(0, SI_SR_WRST0, data); }
	void SerialInterface::si_rd_out0_hi(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; *reg = si->si.out[0] >> 16; }
	void SerialInterface::si_rd_out0_lo(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; *reg = (uint16_t)si->si.out[0]; }

	/* ******* CHAN 1 ******* */

	void SerialInterface::si_wr_out1_hi(uint32_t addr, uint32_t data, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_wr_out_hi(1, data); }
	void SerialInterface::si_wr_out1_lo(uint32_t addr, uint32_t data, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_wr_out_lo(1, SI_SR_WRST1, data); }
	void SerialInterface::si_rd_out1_hi(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; *reg = si->si.out[1] >> 16; }
	void SerialInterface::si_rd_out1_lo(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; *reg = (uint16_t)si->si.out[1]; }

	/* ******* CHAN 2 ******* */

	void SerialInterface::si_wr_out2_hi(uint32_t addr, uint32_t data, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_wr_out_hi(2, data); }
	void SerialInterface::si_wr_out2_lo(uint32_t addr, uint32_t data, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_wr_out_lo(2, SI_SR_WRST2, data); }
	void SerialInterface::si_rd_out2_hi(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; *reg = si->si.out[2] >> 16; }
	void SerialInterface::si_rd_out2_lo(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; *reg = (uint16_t)si->si.out[2]; }

	/* ******* CHAN 3 ******* */

	void SerialInterface::si_wr_out3_hi(uint32_t addr, uint32_t data, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_wr_out_hi(3, data); }
	void SerialInterface::si_wr_out3_lo(uint32_t addr, uint32_t data, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_wr_out_lo(3, SI_SR_WRST3, data); }
	void SerialInterface::si_rd_out3_hi(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; *reg = si->si.out[3] >> 16; }
	void SerialInterface::si_rd_out3_lo(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; *reg = (uint16_t)si->si.out[3]; }

	//
	// input buffers are read only
	//

	// The names are a bit silly here. The registers themselves are called High and Low. We also refer to the higher and lower halves, which are also called hi and lo.

	void SerialInterface::si_inh_hi(int chan, uint32_t* reg)          // high [31:16]
	{
		uint32_t res;

		// return swapped joypad values
		res = si.pad[chan].button;

		*reg = res;
	}

	void SerialInterface::si_inh_lo(int chan, uint32_t mask, uint32_t* reg)          // high [15:0]
	{
		uint32_t res;

		// return swapped joypad values
		res = (uint8_t)si.pad[chan].stickY;
		res |= (uint8_t)si.pad[chan].stickX << 8;

		// clear RDST mask and interrupt
		SI_SR_REG &= ~mask;
		if ((SI_SR_REG &
			(SI_SR_RDST0 |
				SI_SR_RDST1 |
				SI_SR_RDST2 |
				SI_SR_RDST3)) == 0)
		{
			SI_COMCSR_REG &= ~SI_COMCSR_RDSTINT;
			SIClearInterrupt();
		}

		*reg = res;
	}

	void SerialInterface::si_inl_hi(int chan, uint32_t* reg)          // low [31:16]
	{
		uint32_t res;

		// return swapped joypad values
		res = (uint8_t)si.pad[chan].substickY;
		res |= (uint8_t)si.pad[chan].substickX << 8;

		*reg = res;
	}

	void SerialInterface::si_inl_lo(int chan, uint32_t* reg)          // low [15:0]
	{
		uint32_t res;

		// return swapped joypad values
		res = (uint8_t)si.pad[chan].triggerRight;
		res |= (uint8_t)si.pad[chan].triggerLeft << 8;

		*reg = res;
	}

	/* ******* CHAN 0 ******* */

	void SerialInterface::si_inh0_hi(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_inh_hi(0, reg); }
	void SerialInterface::si_inh0_lo(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_inh_lo(0, SI_SR_RDST0, reg); }
	void SerialInterface::si_inl0_hi(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_inl_hi(0, reg); }
	void SerialInterface::si_inl0_lo(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_inl_lo(0, reg); }

	/* ******* CHAN 1 ******* */

	void SerialInterface::si_inh1_hi(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_inh_hi(1, reg); }
	void SerialInterface::si_inh1_lo(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_inh_lo(1, SI_SR_RDST1, reg); }
	void SerialInterface::si_inl1_hi(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_inl_hi(1, reg); }
	void SerialInterface::si_inl1_lo(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_inl_lo(1, reg); }

	/* ******* CHAN 2 ******* */

	void SerialInterface::si_inh2_hi(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_inh_hi(2, reg); }
	void SerialInterface::si_inh2_lo(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_inh_lo(2, SI_SR_RDST2, reg); }
	void SerialInterface::si_inl2_hi(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_inl_hi(2, reg); }
	void SerialInterface::si_inl2_lo(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_inl_lo(2, reg); }

	/* ******* CHAN 3 ******* */

	void SerialInterface::si_inh3_hi(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_inh_hi(3, reg); }
	void SerialInterface::si_inh3_lo(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_inh_lo(3, SI_SR_RDST3, reg); }
	void SerialInterface::si_inl3_hi(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_inl_hi(3, reg); }
	void SerialInterface::si_inl3_lo(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; si->si_inl_lo(3, reg); }

	//
	// communication buffer access
	//

	void SerialInterface::write_sicom(uint32_t addr, uint32_t data, void* ctx)
	{
		SerialInterface* si = (SerialInterface*)ctx;
		unsigned ofs = addr & 0x7f;

		si->si.combuf[ofs + 0] = (uint8_t)(data >> 8);
		si->si.combuf[ofs + 1] = (uint8_t)data;
	}

	void SerialInterface::read_sicom(uint32_t addr, uint32_t* reg, void* ctx)
	{
		SerialInterface* si = (SerialInterface*)ctx;
		unsigned ofs = addr & 0x7f;

		*reg = ((uint32_t)si->si.combuf[ofs + 0] << 8);
		*reg |= si->si.combuf[ofs + 1];
	}

	// ---------------------------------------------------------------------------
	// si control registers

	//
	// polling register
	//

	void SerialInterface::write_poll_hi(uint32_t addr, uint32_t data, void* ctx) {
		SerialInterface* si = (SerialInterface*)ctx;
		si->SI_POLL_REG &= 0x0000ffff;
		si->SI_POLL_REG |= data << 16;
	}
	void SerialInterface::write_poll_lo(uint32_t addr, uint32_t data, void* ctx) {
		SerialInterface* si = (SerialInterface*)ctx;
		si->SI_POLL_REG &= 0xffff0000;
		si->SI_POLL_REG |= data;
	}
	void SerialInterface::read_poll_hi(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; *reg = si->SI_POLL_REG >> 16; }
	void SerialInterface::read_poll_lo(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; *reg = (uint16_t)si->SI_POLL_REG; }

	//
	// communication control/status 
	//

	void SerialInterface::write_commcsr_hi(uint32_t addr, uint32_t data, void* ctx)
	{
		SerialInterface* si = (SerialInterface*)ctx;
		data <<= 16;
		uint32_t outlen = SI_COMCSR_OUTLEN(data);

		// clear incoming interrupt
		if (data & SI_COMCSR_TCINT)
		{
			si->SI_COMCSR_REG &= ~SI_COMCSR_TCINT;
			si->SIClearInterrupt();
		}

		// change RDST interrupt mask
		if (data & SI_COMCSR_RDSTINTMSK) si->SI_COMCSR_REG |= SI_COMCSR_RDSTINTMSK;
		else si->SI_COMCSR_REG &= ~SI_COMCSR_RDSTINTMSK;

		// change TCINT interrupt mask
		if (data & SI_COMCSR_TCINTMSK) si->SI_COMCSR_REG |= SI_COMCSR_TCINTMSK;
		else si->SI_COMCSR_REG &= ~SI_COMCSR_TCINTMSK;

		// OUTLNGTH is the only other writable field of the high halfword; COMERR and RDSTINT
		// are read-only and the reserved bits read as 0 (11.5).
		si->SI_COMCSR_REG &= ~SI_COMCSR_OUTLEN_MASK;
		si->SI_COMCSR_REG |= (outlen << 16);
	}

	void SerialInterface::write_commcsr_lo(uint32_t addr, uint32_t data, void* ctx)
	{
		SerialInterface* si = (SerialInterface*)ctx;

		// CHANNEL and INLNGTH are ordinary writable fields of the low halfword (11.5); TSTART is
		// a pending bit, so it is never stored (it is set below for the duration of the transfer).
		si->SI_COMCSR_REG &= ~(SI_COMCSR_CHAN_MASK | SI_COMCSR_INLEN_MASK | SI_COMCSR_TSTART);
		si->SI_COMCSR_REG |= (data & (SI_COMCSR_CHAN_MASK | SI_COMCSR_INLEN_MASK));

		// commands are executed immediately
		if (data & SI_COMCSR_TSTART)
		{
			// the transfer is pending while it runs, and reads back complete once it has finished
			si->SI_COMCSR_REG |= SI_COMCSR_TSTART;

			int chan = SI_COMCSR_CHAN(si->SI_COMCSR_REG);

			// setup in/out length
			int inlen = SI_COMCSR_INLEN(si->SI_COMCSR_REG);
			if (inlen == 0) inlen = 128;
			int outlen = SI_COMCSR_OUTLEN(si->SI_COMCSR_REG);
			if (outlen == 0) outlen = 128;

			// make actual transfer
			si->SICommand(chan, outlen, inlen, si->si.combuf);

			// complete transfer
			si->SI_COMCSR_REG &= ~SI_COMCSR_TSTART;

			// set completion interrupt
			si->SI_COMCSR_REG |= SI_COMCSR_TCINT;

			// generate cpu interrupt (if mask allows that)
			if (si->SI_COMCSR_REG & SI_COMCSR_TCINTMSK)
			{
				HW->pi->PIAssertInt(PI_INTERRUPT_SI);
			}
		}
	}

	void SerialInterface::read_commcsr_hi(uint32_t addr, uint32_t* reg, void* ctx)
	{
		SerialInterface* si = (SerialInterface*)ctx;
		*reg = si->SI_COMCSR_REG >> 16;
	}
	void SerialInterface::read_commcsr_lo(uint32_t addr, uint32_t* reg, void* ctx)
	{
		SerialInterface* si = (SerialInterface*)ctx;
		*reg = (uint16_t)si->SI_COMCSR_REG;
	}

	//
	// status register
	//

	void SerialInterface::write_sisr_hi(uint32_t addr, uint32_t data, void* ctx)
	{
		SerialInterface* si = (SerialInterface*)ctx;
		data <<= 16;

		// The latched error bits of a channel are write-1-to-clear (11.6); channels 0 and 1 live in
		// this halfword. A written 0 leaves the bit alone.
		si->SI_SR_REG &= ~(data & SI_SR_ERROR_HI);

		// copy the visible command registers into the shadow ones
		if (data & SI_SR_WR)
		{
			si->si.shdw[0] = si->si.out[0];
			si->SI_SR_REG &= ~SI_SR_WRST0;
			si->si.shdw[1] = si->si.out[1];
			si->SI_SR_REG &= ~SI_SR_WRST1;
			si->si.shdw[2] = si->si.out[2];
			si->SI_SR_REG &= ~SI_SR_WRST2;
			si->si.shdw[3] = si->si.out[3];
			si->SI_SR_REG &= ~SI_SR_WRST3;
		}
	}
	void SerialInterface::write_sisr_lo(uint32_t addr, uint32_t data, void* ctx)
	{
		SerialInterface* si = (SerialInterface*)ctx;

		// Channels 2 and 3 clear their latch in the low halfword.
		si->SI_SR_REG &= ~(data & SI_SR_ERROR_LO);
	}

	void SerialInterface::read_sisr_hi(uint32_t addr, uint32_t* reg, void* ctx)
	{
		SerialInterface* si = (SerialInterface*)ctx;
		*reg = si->SI_SR_REG >> 16;
	}
	void SerialInterface::read_sisr_lo(uint32_t addr, uint32_t* reg, void* ctx)
	{
		SerialInterface* si = (SerialInterface*)ctx;
		*reg = (uint16_t)si->SI_SR_REG;
	}

	// 
	// EXI clock lock reg (dummy)
	//

	void SerialInterface::write_exilk_hi(uint32_t addr, uint32_t data, void* ctx) {
		SerialInterface* si = (SerialInterface*)ctx;
		si->si.exilk &= 0x0000ffff;
		si->si.exilk |= data << 16;
	}
	void SerialInterface::write_exilk_lo(uint32_t addr, uint32_t data, void* ctx) {
		SerialInterface* si = (SerialInterface*)ctx;
		si->si.exilk &= 0xffff0000;
		si->si.exilk |= data;
	}
	void SerialInterface::read_exilk_hi(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; *reg = si->si.exilk >> 16; }
	void SerialInterface::read_exilk_lo(uint32_t addr, uint32_t* reg, void* ctx) { SerialInterface* si = (SerialInterface*)ctx; *reg = (uint16_t)si->si.exilk; }

	// ---------------------------------------------------------------------------
	// polling

	// The poll schedule (serial-interface.md 5.1):
	//
	//   SIPOLL[X] is the interval between two polls in horizontal video lines,
	//   SIPOLL[Y] is how many polls a frame may issue,
	//   polling is anchored to the vertical blank, and `Y == 0` disables it.
	//
	// The schedule is therefore counted in *video lines*, not in CPU ticks: the interval used to
	// be a fixed `SI_POLLING_INTERVAL` of Gekko ticks, which at the derived timer clock is about
	// 1.6 ms - a poll every ~100 us per channel, i.e. roughly ten times the frame rate the
	// hardware runs at. On a channel whose enable bit is set and whose response the guest has not
	// read yet, every one of those polls re-raises RDSTINT, so the guest is buried in serial
	// interrupts: Animal Crossing (GAFE01) spends its whole run in the SI handler instead of
	// booting, which is the black screen.
	//
	// The video line counter is the natural tick for this: it is driven by the VI and wraps once
	// per frame, so a new frame is exactly "the line count went backwards".
	void SerialInterface::SIPoll(uint32_t line)
	{
		// A new frame: restart the poll budget and allow the first poll of the frame.
		if (line < si.lastPollLine)
		{
			si.pollsThisFrame = 0;
			si.pollLineDue = true;
		}
		si.lastPollLine = line;

		uint32_t interval = SI_POLL_X(SI_POLL_REG);
		uint32_t perFrame = SI_POLL_Y(SI_POLL_REG);

		// `Y == 0` means the poller is off, and a zero interval would poll every line.
		if (perFrame == 0 || interval == 0)
		{
			return;
		}

		if (si.pollsThisFrame >= perFrame)
		{
			return;
		}

		// The first poll of a frame happens at the blank; after that one poll every `interval`
		// lines. A frame shorter than `interval` therefore still gets its first poll.
		if (!si.pollLineDue && (line - si.pollLineBase) < interval)
		{
			return;
		}

		si.pollLineDue = false;
		si.pollLineBase = line;
		si.pollsThisFrame++;

		// Every enabled channel is polled. The device that is plugged into it refreshes the state of
		// its controls, and a channel whose device answers raises its read-status flag.
		for (int chan = 0; chan < 4; chan++)
		{
			if ((SI_POLL_REG & (SI_POLL_EN0 >> chan)) == 0)
			{
				continue;
			}

			// update pad input buffer
			if (Peripherals::Instance().PollSI(chan, &si.pad[chan]))
			{
				SI_SR_REG |= (SI_SR_RDST0 >> (chan * 8));
				SI_COMCSR_REG |= SI_COMCSR_RDSTINT;
			}
		}

		// generate RDST interrupt
		if ((SI_COMCSR_REG & SI_COMCSR_RDSTINT) && (SI_COMCSR_REG & SI_COMCSR_RDSTINTMSK))
		{
			// assert processor interrupt
			HW->pi->PIAssertInt(PI_INTERRUPT_SI);
		}
	}

	// ---------------------------------------------------------------------------
	// save states

	// What the SI must carry into a state is everything the block itself decided, in the order it
	// is written here and read back in exactly the same order:
	//
	//   SICnOUTBUF   the four CPU visible command latches                    4 x 32 bits
	//   SICnOUTBUF'  the four hidden shadows the transfer engine shifts out   4 x 32 bits
	//   SIPOLL, SICOMCSR, SISR, SIEXILK, four registers                      4 x 32 bits
	//   SICOMBUF     the communication buffer, 128 bytes plus the 32 bytes of overrun protection
	//   the poll schedule: `lastPollLine`, `pollLineBase`, `pollsThisFrame` (3 x 32 bits) and
	//   `pollLineDue` (1 byte)
	//
	// The visible and the hidden command latch are both here because the guest can write the
	// visible one at any time and only SISR[WR] copies it into the shadow: a state taken between
	// those two writes has to bring back the difference, exactly like the bypass write mask of
	// the command processor. SICOMCSR carries the interrupt latches (TCINT / RDSTINT) and SISR
	// the per channel latched errors, which is why both are in the state rather than derived.
	void SerialInterface::SaveState(SaveStates::StateWriter& writer) const
	{
		writer.Array(si.out);
		writer.Array(si.shdw);

		writer.Fields(si.poll, si.comcsr, si.sr, si.exilk);

		// The whole buffer, overrun protection included: `combuf` is what the transfer of the
		// channel reads its command bytes from and writes its answer into, and the tail past the
		// 128 bytes the register window exposes is written by the overrun path, so a state that
		// kept only the visible part would come back with a different buffer.
		writer.Raw(si.combuf, sizeof(si.combuf));

		// The poll schedule (serial-interface.md 5.1) is not a set of registers the guest can
		// restore: it is how far into the current frame the poller is, so a state taken between
		// two polls has to carry where the next one is due.
		writer.Fields(si.lastPollLine, si.pollLineBase, si.pollsThisFrame, si.pollLineDue);

		// NOT part of the state:
		//
		// `pad[4]` is what the last poll got out of the device pool (SIPoll asks
		// Peripherals::PollSI for every enabled channel), and the pool re-fills it on every poll.
		// It is the state of the *controllers* - the player's hands - not of the machine, and it
		// changes on its own while the emulator runs. Leaving it out is what makes a load do the
		// right thing with it: the buffer the guest reads before the next poll still holds the
		// snapshot of the live input taken on the machine that is running now, rather than the
		// input of the moment the state was saved, and the next poll refreshes it from the pool.
		//
		// `rumble[4]` is derived from that same pool: the constructor asks every channel's motor
		// whether it can rumble (SetMotorSI) and the answer only changes when the user plugs
		// another controller in. There is nothing here the guest wrote, and re-deriving it on load
		// would send a motor command to the host's controller, which is a thing a load must not do.
		//
		// `log` is the `si_log` setting and never affects the machine.
	}

	void SerialInterface::LoadState(SaveStates::StateReader& reader)
	{
		reader.Array(si.out);
		reader.Array(si.shdw);

		reader.Fields(si.poll, si.comcsr, si.sr, si.exilk);
		reader.Raw(si.combuf, sizeof(si.combuf));
		reader.Fields(si.lastPollLine, si.pollLineBase, si.pollsThisFrame, si.pollLineDue);

		// Nothing here is replayed through a register write: SICOMCSR and SISR are put back as
		// values, so a state taken between two guest writes to them comes back in the middle of
		// that sequence, and no transfer runs, no interrupt is raised and no status bit moves
		// because of the load.
		//
		// The PI line is deliberately left alone. The other blocks' sections restore the PI's
		// cause register as it was, and the SI's own copy of it is *not* a function of the
		// latches: a guest that raises RDSTINT and then clears RDSTINTMSK leaves the PI bit
		// asserted (write_commcsr_hi only calls SIClearInterrupt when it clears a latch), so
		// recomputing the line here would clear an interrupt the state says was pending.
	}

	// ---------------------------------------------------------------------------
	// init

	// The SI input buffer registers are writable, but the emulated SI gets their contents from
	// the transfer engine, so a value written by the CPU is simply not retained.

	static void si_wr_input_buffer(uint32_t addr, uint32_t data, void* context)
	{
	}

	SerialInterface::SerialInterface(Flipper* flipper, HWConfig* config)
	{
		Debug::Report(Debug::Channel::SI, "Serial interface driver\n");

		// clear all registers
		memset(&si, 0, sizeof(si));

		si.log = config->si_log;

		si.lastPollLine = 0;
		si.pollLineBase = 0;
		si.pollsThisFrame = 0;
		si.pollLineDue = true;

		// these values are actually written when IPL boots
		// meaning is unknown (some pad command) and no need to be known
		si.out[0] =
		si.out[1] =
		si.out[2] =
		si.out[3] = 0x00400300; // continue polling ?

		// The boot ROM immediately copies the visible buffers into the shadow ones, so the
		// transfer engine starts from the same command bytes without a pending write status.
		for (int i = 0; i < 4; i++)
		{
			si.shdw[i] = si.out[i];
		}

		// enable polling (for homebrewn), IPL enabling it
		SI_POLL_REG |= (SI_POLL_EN0 | SI_POLL_EN1 | SI_POLL_EN2 | SI_POLL_EN3);

		// update joypad data
		SIPoll(0);

		// set rumble flags
		for (int i = 0; i < 4; i++) {
			si.rumble[i] = Peripherals::Instance().SetMotorSI(i, PAD_MOTOR_STOP);
		}

		// joypads in/out command buffer
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN0_OUTBUF, si_rd_out0_hi, si_wr_out0_hi, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN0_OUTBUF + 2, si_rd_out0_lo, si_wr_out0_lo, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN0_INBUFH, si_inh0_hi, si_wr_input_buffer, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN0_INBUFH + 2, si_inh0_lo, si_wr_input_buffer, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN0_INBUFL, si_inl0_hi, si_wr_input_buffer, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN0_INBUFL + 2, si_inl0_lo, si_wr_input_buffer, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN1_OUTBUF, si_rd_out1_hi, si_wr_out1_hi, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN1_OUTBUF + 2, si_rd_out1_lo, si_wr_out1_lo, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN1_INBUFH, si_inh1_hi, si_wr_input_buffer, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN1_INBUFH + 2, si_inh1_lo, si_wr_input_buffer, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN1_INBUFL, si_inl1_hi, si_wr_input_buffer, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN1_INBUFL + 2, si_inl1_lo, si_wr_input_buffer, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN2_OUTBUF, si_rd_out2_hi, si_wr_out2_hi, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN2_OUTBUF + 2, si_rd_out2_lo, si_wr_out2_lo, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN2_INBUFH, si_inh2_hi, si_wr_input_buffer, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN2_INBUFH + 2, si_inh2_lo, si_wr_input_buffer, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN2_INBUFL, si_inl2_hi, si_wr_input_buffer, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN2_INBUFL + 2, si_inl2_lo, si_wr_input_buffer, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN3_OUTBUF, si_rd_out3_hi, si_wr_out3_hi, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN3_OUTBUF + 2, si_rd_out3_lo, si_wr_out3_lo, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN3_INBUFH, si_inh3_hi, si_wr_input_buffer, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN3_INBUFH + 2, si_inh3_lo, si_wr_input_buffer, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN3_INBUFL, si_inl3_hi, si_wr_input_buffer, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_CHAN3_INBUFL + 2, si_inl3_lo, si_wr_input_buffer, this);

		// si control registers
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_POLL, read_poll_hi, write_poll_hi, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_POLL + 2, read_poll_lo, write_poll_lo, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_COMCSR, read_commcsr_hi, write_commcsr_hi, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_COMCSR + 2, read_commcsr_lo, write_commcsr_lo, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_SR, read_sisr_hi, write_sisr_hi, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_SR + 2, read_sisr_lo, write_sisr_lo, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_EXILK, read_exilk_hi, write_exilk_hi, this);
		flipper->pi->PISetTrap(PI_REGSPACE_SI | SI_EXILK + 2, read_exilk_lo, write_exilk_lo, this);

		// serial communcation buffer
		for (unsigned ofs = 0; ofs < 128; ofs += 2)
		{
			flipper->pi->PISetTrap((PI_REGSPACE_SI | SI_COMBUF) + ofs, read_sicom, write_sicom, this);
		}
	}

	SerialInterface::~SerialInterface()
	{
	}
}