// SI - serial interface (only GC "Spec5" controllers atm, by PAD plugin calls).
#include "pch.h"

// SI state (registers and other data)
SIState si;

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
	|-----|  |----| PADState |<----| PC device (plugin) |
	| INL |  |     ----------       --------------------
	 -----  <
/*/

// Note : digital L and R are only set when its analog key is pressed all the way down;

// ---------------------------------------------------------------------------
// dispatch command

//
// command data are sending via communication buffer,
// we are parsing command opcode and forming response 
// data packet, written back to the communication buffer
//

static void SICommand(int chan, uint8_t* ptr)
{
	uint8_t  cmd = ptr[0];

	switch (cmd)
	{
		// get device type and status
		case 0x00:
		{
			// 0 : use sub-type
			// 2 : n64 mouse
			// 5 : n64 controller
			// 9 : default gc controller
			ptr[0] = 9;
			ptr[1] = 0;     // sub-type
			ptr[2] = 0;     // always 0 ?
			break;
		}

		// HACK : use it for first time
		case 0x40:
		case 0x42:
			return;

		// unknown, freeloader uses it, when booting
		// case 0x40:

		// read origins
		case 0x41:
			ptr[0] = 0x41;
			ptr[1] = 0;
			ptr[2] = ptr[3] = ptr[4] = ptr[5] = 0x80;
			ptr[6] = ptr[7] = 0x1f;
			break;

		// calibrate
		//case 0x42:

		default:
		{
			Debug::Halt(
				"Unknown SI command. chan:%i, cmd:%02X, out:%i, in:%i\n",
				chan, cmd, SI_COMCSR_OUTLEN(SI_COMCSR_REG), SI_COMCSR_INLEN(SI_COMCSR_REG));
		}
	}
}

static void SIClearInterrupt()
{
	if ((SI_COMCSR_REG & SI_COMCSR_RDSTINT) == 0 && (SI_COMCSR_REG & SI_COMCSR_TCINT) == 0)
	{
		PIClearInt(PI_INTERRUPT_SI);
	}
}

// ---------------------------------------------------------------------------
// register traps

//
// command output buffers are read/write
//

static void si_wr_out(int chan, uint32_t mask, uint32_t data)
{
	si.shdw[chan] = data;
	SI_SR_REG |= mask;

	// control motor
	if (data == 0x00400000) PADSetRumble(chan, PAD_MOTOR_STOP);
	else if (data == 0x00400001) PADSetRumble(chan, PAD_MOTOR_RUMBLE);
	else if (data == 0x00400002) PADSetRumble(chan, PAD_MOTOR_STOP_HARD);
}

/* ******* CHAN 0 ******* */

static void si_wr_out0(uint32_t addr, uint32_t data) { si_wr_out(0, SI_SR_WRST0, data); }
static void si_rd_out0(uint32_t addr, uint32_t* reg) { *reg = si.out[0]; }

/* ******* CHAN 1 ******* */

static void si_wr_out1(uint32_t addr, uint32_t data) { si_wr_out(1, SI_SR_WRST1, data); }
static void si_rd_out1(uint32_t addr, uint32_t* reg) { *reg = si.out[1]; }

/* ******* CHAN 2 ******* */

static void si_wr_out2(uint32_t addr, uint32_t data) { si_wr_out(2, SI_SR_WRST2, data); }
static void si_rd_out2(uint32_t addr, uint32_t* reg) { *reg = si.out[2]; }

/* ******* CHAN 3 ******* */

static void si_wr_out3(uint32_t addr, uint32_t data) { si_wr_out(3, SI_SR_WRST3, data); }
static void si_rd_out3(uint32_t addr, uint32_t* reg) { *reg = si.out[3]; }

//
// input buffers are read only
//

static void si_inh(int chan, uint32_t mask, uint32_t* reg)          // high
{
	uint32_t res;

	// return swapped joypad values
	res = (uint8_t)si.pad[chan].stickY;
	res |= (uint8_t)si.pad[chan].stickX << 8;
	res |= si.pad[chan].button << 16;

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

static void si_inl(int chan, uint32_t* reg)          // low
{
	uint32_t res;

	// return swapped joypad values
	res = (uint8_t)si.pad[chan].triggerRight;
	res |= (uint8_t)si.pad[chan].triggerLeft << 8;
	res |= (uint8_t)si.pad[chan].substickY << 16;
	res |= (uint8_t)si.pad[chan].substickX << 24;

	*reg = res;
}

/* ******* CHAN 0 ******* */

static void si_inh0(uint32_t addr, uint32_t* reg) { si_inh(0, SI_SR_RDST0, reg); }
static void si_inl0(uint32_t addr, uint32_t* reg) { si_inl(0, reg); }

/* ******* CHAN 1 ******* */

static void si_inh1(uint32_t addr, uint32_t* reg) { si_inh(1, SI_SR_RDST1, reg); }
static void si_inl1(uint32_t addr, uint32_t* reg) { si_inl(1, reg); }

/* ******* CHAN 2 ******* */

static void si_inh2(uint32_t addr, uint32_t* reg) { si_inh(2, SI_SR_RDST2, reg); }
static void si_inl2(uint32_t addr, uint32_t* reg) { si_inl(2, reg); }

/* ******* CHAN 3 ******* */

static void si_inh3(uint32_t addr, uint32_t* reg) { si_inh(3, SI_SR_RDST3, reg); }
static void si_inl3(uint32_t addr, uint32_t* reg) { si_inl(3, reg); }

//
// communication buffer access
//

static void write_sicom(uint32_t addr, uint32_t data, void *ctx)
{
	unsigned ofs = addr & 0x7f;

	si.combuf[ofs + 0] = (uint8_t)(data >> 8);
	si.combuf[ofs + 1] = (uint8_t)data;
}

static void read_sicom(uint32_t addr, uint32_t* reg, void* ctx)
{
	unsigned ofs = addr & 0x7f;

	*reg  = ((uint32_t)si.combuf[ofs + 0] << 8);
	*reg |= si.combuf[ofs + 1];
}

// ---------------------------------------------------------------------------
// si control registers

//
// polling register
//

static void write_poll(uint32_t addr, uint32_t data) { SI_POLL_REG = data; }
static void read_poll(uint32_t addr, uint32_t* reg) { *reg = SI_POLL_REG; }

//
// communication control/status 
//

static void write_commcsr(uint32_t addr, uint32_t data)
{
	// clear incoming interrupt
	if (data & SI_COMCSR_TCINT)
	{
		SI_COMCSR_REG &= ~SI_COMCSR_TCINT;
		SIClearInterrupt();
	}

	// change RDST interrupt mask
	if (data & SI_COMCSR_RDSTINTMSK) SI_COMCSR_REG |= SI_COMCSR_RDSTINTMSK;
	else SI_COMCSR_REG &= ~SI_COMCSR_RDSTINTMSK;

	// change TCINT interrupt mask
	if (data & SI_COMCSR_TCINTMSK) SI_COMCSR_REG |= SI_COMCSR_TCINTMSK;
	else SI_COMCSR_REG &= ~SI_COMCSR_TCINTMSK;

	// commands are executed immediately
	if (data & SI_COMCSR_TSTART)
	{
		// select channel
		int chan = SI_COMCSR_CHAN(data);
		SI_COMCSR_REG |= (chan << 1);

		// setup in/out length
		int inlen = SI_COMCSR_INLEN(data);
		SI_COMCSR_REG |= (inlen << 8);
		if (inlen == 0) inlen = 128;
		int outlen = SI_COMCSR_OUTLEN(data);
		SI_COMCSR_REG |= (outlen << 16);
		if (outlen == 0) outlen = 128;

		// make actual transfer
		SICommand(chan, si.combuf);

		// complete transfer
		SI_COMCSR_REG &= ~SI_COMCSR_TSTART;

		// set completion interrupt
		SI_COMCSR_REG |= SI_COMCSR_TCINT;

		// generate cpu interrupt (if mask allows that)
		if (SI_COMCSR_REG & SI_COMCSR_TCINTMSK)
		{
			PIAssertInt(PI_INTERRUPT_SI);
		}
	}
}

static void read_commcsr(uint32_t addr, uint32_t* reg)
{
	*reg = SI_COMCSR_REG;
}

//
// status register
//

static void write_sisr(uint32_t addr, uint32_t data)
{
	// copy shadow command registers
	if (data & SI_SR_WR)
	{
		si.out[0] = si.shdw[0];
		SI_SR_REG &= ~SI_SR_WRST0;
		si.out[1] = si.shdw[1];
		SI_SR_REG &= ~SI_SR_WRST1;
		si.out[2] = si.shdw[2];
		SI_SR_REG &= ~SI_SR_WRST2;
		si.out[3] = si.shdw[3];
		SI_SR_REG &= ~SI_SR_WRST3;
	}
}

static void read_sisr(uint32_t addr, uint32_t* reg)
{
	*reg = SI_SR_REG;
}

// 
// EXI clock lock reg (dummy)
//

static void write_exilk(uint32_t addr, uint32_t data) { si.exilk = data; }
static void read_exilk(uint32_t addr, uint32_t* reg) { *reg = si.exilk; }

// ---------------------------------------------------------------------------
// polling

void SIPoll()
{
	int64_t ticks = Core->GetTicks();
	if (ticks < si.pollingTime)
	{
		return;
	}
	si.pollingTime = ticks + SI_POLLING_INTERVAL;

	if (SI_POLL_REG & SI_POLL_EN0)
	{
		// update pad input buffer
		bool connected = false;
		connected = PADReadButtons(0, &si.pad[0]);

		// set RDST flag
		if (connected)
		{
			SI_SR_REG |= SI_SR_RDST0;
			SI_COMCSR_REG |= SI_COMCSR_RDSTINT;
		}
	}

	if (SI_POLL_REG & SI_POLL_EN1)
	{
		// update pad input buffer
		bool connected = false;
		connected = PADReadButtons(1, &si.pad[1]);

		// set RDST flag
		if (connected)
		{
			SI_SR_REG |= SI_SR_RDST1;
			SI_COMCSR_REG |= SI_COMCSR_RDSTINT;
		}
	}

	if (SI_POLL_REG & SI_POLL_EN2)
	{
		// update pad input buffer
		bool connected = false;
		connected = PADReadButtons(2, &si.pad[2]);

		// set RDST flag
		if (connected)
		{
			SI_SR_REG |= SI_SR_RDST2;
			SI_COMCSR_REG |= SI_COMCSR_RDSTINT;
		}
	}

	if (SI_POLL_REG & SI_POLL_EN3)
	{
		// update pad input buffer
		bool connected = false;
		connected = PADReadButtons(3, &si.pad[3]);

		// set RDST flag
		if (connected)
		{
			SI_SR_REG |= SI_SR_RDST3;
			SI_COMCSR_REG |= SI_COMCSR_RDSTINT;
		}
	}

	// generate RDST interrupt
	if ((SI_COMCSR_REG & SI_COMCSR_RDSTINT) && (SI_COMCSR_REG & SI_COMCSR_RDSTINTMSK))
	{
		// assert processor interrupt
		PIAssertInt(PI_INTERRUPT_SI);
	}
}

// ---------------------------------------------------------------------------
// init

void SIOpen(HWConfig* config)
{
	Debug::Report(Debug::Channel::SI, "Serial interface driver\n");

	// clear all registers
	memset(&si, 0, sizeof(si));

	si.log = config->si_log;

	si.pollingTime = Core->GetTicks() + SI_POLLING_INTERVAL;

	// these values are actually written when IPL boots
	// meaning is unknown (some pad command) and no need to be known
	si.out[0] =
	si.out[1] =
	si.out[2] =
	si.out[3] = 0x00400300; // continue polling ?

	// enable polling (for homebrewn), IPL enabling it
	SI_POLL_REG |= (SI_POLL_EN0 | SI_POLL_EN1 | SI_POLL_EN2 | SI_POLL_EN3);

	// update joypad data
	SIPoll();

	// set rumble flags
	for (int i = 0; i < 4; i++)
		si.rumble[i] = PADSetRumble(i, PAD_MOTOR_STOP);

	// joypads in/out command buffer
	PISetTrap(PI_REGSPACE_SI | SI_CHAN0_OUTBUF, si_rd_out0, si_wr_out0);
	PISetTrap(PI_REGSPACE_SI | SI_CHAN0_INBUFH, si_inh0, NULL);
	PISetTrap(PI_REGSPACE_SI | SI_CHAN0_INBUFL, si_inl0, NULL);
	PISetTrap(PI_REGSPACE_SI | SI_CHAN1_OUTBUF, si_rd_out1, si_wr_out1);
	PISetTrap(PI_REGSPACE_SI | SI_CHAN1_INBUFH, si_inh1, NULL);
	PISetTrap(PI_REGSPACE_SI | SI_CHAN1_INBUFL, si_inl1, NULL);
	PISetTrap(PI_REGSPACE_SI | SI_CHAN2_OUTBUF, si_rd_out2, si_wr_out2);
	PISetTrap(PI_REGSPACE_SI | SI_CHAN2_INBUFH, si_inh2, NULL);
	PISetTrap(PI_REGSPACE_SI | SI_CHAN2_INBUFL, si_inl2, NULL);
	PISetTrap(PI_REGSPACE_SI | SI_CHAN3_OUTBUF, si_rd_out3, si_wr_out3);
	PISetTrap(PI_REGSPACE_SI | SI_CHAN3_INBUFH, si_inh3, NULL);
	PISetTrap(PI_REGSPACE_SI | SI_CHAN3_INBUFL, si_inl3, NULL);

	// si control registers
	PISetTrap(PI_REGSPACE_SI | SI_POLL, read_poll, write_poll);
	PISetTrap(PI_REGSPACE_SI | SI_COMCSR, read_commcsr, write_commcsr);
	PISetTrap(PI_REGSPACE_SI | SI_SR, read_sisr, write_sisr);
	PISetTrap(PI_REGSPACE_SI | SI_EXILK, read_exilk, write_exilk);

	// serial communcation buffer
	for (unsigned ofs = 0; ofs < 128; ofs += 2)
	{
		PISetTrap((PI_REGSPACE_SI | SI_COMBUF) + ofs, read_sicom, write_sicom);
	}
}

void SIClose()
{
}