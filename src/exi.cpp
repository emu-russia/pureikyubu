// EXI - expansion (or extension) interface.
#include "pch.h"

// IMPORTANT : all EXI transfers are completed instantly in emu (mean no DMA chain).

/* ---------------------------------------------------------------------------

	Purpose :
	---------

	EXI is used for reading of SRAM, RTC, IPL font (from MX chip)
	EXI used for memory cards and broad band adapter.
	bootrom using EXI for AD16 writes ('AD' - what does it mean ?)

	SRAM : little piece of battery-backed data. 64 bytes or so.
	RTC  : 32-bit counter of seconds, since current millenium
	AD16 : This is most likely a debugging device called `Barnacle`.

	memcard should be in another module (see memcard.cpp)
	broad band adapter should be in another module (see bba.cpp)

	EXI Device Map of dedicated MX Chip :
	-------------------------------------

	  --------------------------------------------
	 |chan | dev |  addr  |     description       |
	 |============================================|
	 |  0  |  0  |        | Memory Card (Slot A)  |
	 |--------------------------------------------|
	 |  0  |  1  |00000000| Mask ROM              | *
	 |--------------------------------------------|
	 |  0  |  1  |20000000| Real-Time Clock (RTC) |
	 |--------------------------------------------|
	 |  0  |  1  |20000100| SRAM                  |
	 |--------------------------------------------|
	 |  0  |  1  |20010000| UART                  |
	 |--------------------------------------------|
	 |  0  |  1  |20010100| GPIO (Panasonic Q)    |
	 |--------------------------------------------|
	 |  1  |  0  |        | Memory Card (Slot B)  |
	 |--------------------------------------------|
	 |  2  |  0  |        | AD16 (trace step)     |
	  --------------------------------------------

		* - not actually address, but command (addr << 6) | 0x20000000,
			used as is for emulation, though.

	Summary : EXI is sort of USB architecture.

	Tip from monk :
	---------------

	If You wan't to support the OSReport on a lower level, You just have
	to handle the immediate writes to UART (0x20010000, when read, should
	return a value between 0 and 4 inclusive) and set the console type to
	one of the devkits (I use 0x10000006).

--------------------------------------------------------------------------- */

using namespace Debug;

// SI state (registers and other data)
EIControl exi;

// forward refs on EXI transfers
void    UnknownTransfer();  // ???
void    ADTransfer();       // AD16

// EXI transfer bindings
static void (*EXITransfer[3][3])() = {
	{ MCTransfer     , MXTransfer     , UnknownTransfer },
	{ MCTransfer     , UnknownTransfer, UnknownTransfer },
	{ ADTransfer     , UnknownTransfer, UnknownTransfer }
};

// ---------------------------------------------------------------------------
// EXI utilities

//
// update EXI interrupt status
//

void EXIUpdateInterrupts()
{
	if ( // match interrupt with its mask
		(
			(exi.regs[0].csr & (exi.regs[0].csr << 1) & EXI_CSR_INTERRUPTS) ||
			(exi.regs[1].csr & (exi.regs[1].csr << 1) & EXI_CSR_INTERRUPTS) ||
			(exi.regs[2].csr & (exi.regs[2].csr << 1) & EXI_CSR_INTERRUPTS)
			)
		)
	{
		// assert CPU interrupt
		PIAssertInt(PI_INTERRUPT_EXI);
	}
	else
	{
		// clear cpu interrupt
		PIClearInt(PI_INTERRUPT_EXI);
	}
}

//
// attach / detach device on EXI channel
//

void EXIAttach(int chan)
{
	if (exi.log) Report(Channel::EXI, "attaching device at channel %i\n", chan);

	// set attach flag
	exi.regs[chan].csr |= EXI_CSR_EXT;

	// assert attach interrupt
	exi.regs[chan].csr |= EXI_CSR_EXTINT;
	EXIUpdateInterrupts();
}

void EXIDetach(int chan)
{
	if (exi.log) Report(Channel::EXI, "detaching device at channel %i\n", chan);

	// clear attach flag
	exi.regs[chan].csr &= ~EXI_CSR_EXT;

	// assert detach interrupt
	exi.regs[chan].csr |= EXI_CSR_EXTINT;
	EXIUpdateInterrupts();
}

// ---------------------------------------------------------------------------
// basic transfers (memcard and BBA transfers are too big to put them here)

// undefined transfer
void UnknownTransfer()
{
	// dont do nothing on the exi transfer
	if (exi.log)
	{
		Report(Channel::EXI, "unknown transfer (channel:%i, device:%i)\n", exi.chan, exi.sel);
	}
}

// AD16 device transfer (EXI device 2:0)
// This is most likely a debugging device called `Barnacle`.
void ADTransfer()
{
	// read or write ?
	switch (EXI_CR_RW(exi.regs[2].cr))
	{
		case 0:                 // read
		{
			if (exi.ad16_cmd == 0) exi.regs[2].data = 0x04120000;
			else if (exi.ad16_cmd == 0xa2000000) exi.regs[2].data = exi.ad16 << 16;
			else Report(Channel::EXI, "unknown AD16 command\n");
			return;
		}

		case 1:                 // write
		{
			if (exi.firstImm)
			{
				exi.firstImm = false;
				exi.ad16_cmd = exi.regs[2].data;
			}
			else
			{
				if (exi.ad16_cmd != 0xa0000000)
				{
					Report(Channel::EXI, "unknown AD command (%08X)\n", exi.ad16_cmd);
					return;
				}
				exi.ad16 = exi.regs[2].data >> 16;
				if (exi.log) Report(Channel::EXI, "AD16 set to %04X\n", exi.ad16);
			}
			return;
		}

		default:
		{
			if (EXI_CR_RW(exi.regs[2].cr))
			{
				Report(Channel::EXI, "unknown EXI transfer mode for AD16\n");
			}
		}
	}
}

// ---------------------------------------------------------------------------
// registers

//
// communication control
//

static void exi_select(int chan)
{
	// set flag
	exi.firstImm = true;

	if (exi.regs[chan].csr & EXI_CSR_CS0B)
	{
		exi.sel = 0;
		return;
	}
	if (exi.regs[chan].csr & EXI_CSR_CS1B)
	{
		exi.sel = 1;
		return;
	}
	if (exi.regs[chan].csr & EXI_CSR_CS2B)
	{
		exi.sel = 2;
		return;
	}

	// no device selected
	exi.sel = -1;
}

static void write_csr(int chan, uint32_t data)
{
	if (chan == 0 && (data & EXI_CSR_ROMDIS) != 0) {

		Report(Channel::EXI, "BootROM Decryption Disabled\n");
	}

	// clear interrupts 
	exi.regs[chan].csr &= ~(data & EXI_CSR_INTERRUPTS);

	// update register and do select
	exi.regs[chan].csr = (exi.regs[chan].csr & EXI_CSR_READONLY) | (data & ~EXI_CSR_READONLY);
	exi_select(chan);
	EXIUpdateInterrupts();
}

static void exi0_read_csr(uint32_t addr, uint32_t* reg) { *reg = exi.regs[0].csr; }
static void exi1_read_csr(uint32_t addr, uint32_t* reg) { *reg = exi.regs[1].csr; }
static void exi2_read_csr(uint32_t addr, uint32_t* reg) { *reg = exi.regs[2].csr; }
static void exi0_write_csr(uint32_t addr, uint32_t data) { write_csr(0, data); }
static void exi1_write_csr(uint32_t addr, uint32_t data) { write_csr(1, data); }
static void exi2_write_csr(uint32_t addr, uint32_t data) { write_csr(2, data); }

//
// memory address for EXI DMA
//

static void exi0_read_madr(uint32_t addr, uint32_t* reg) { *reg = exi.regs[0].madr; }
static void exi1_read_madr(uint32_t addr, uint32_t* reg) { *reg = exi.regs[1].madr; }
static void exi2_read_madr(uint32_t addr, uint32_t* reg) { *reg = exi.regs[2].madr; }
static void exi0_write_madr(uint32_t addr, uint32_t data) { exi.regs[0].madr = data; }
static void exi1_write_madr(uint32_t addr, uint32_t data) { exi.regs[1].madr = data; }
static void exi2_write_madr(uint32_t addr, uint32_t data) { exi.regs[2].madr = data; }

//
// data length for DMA
//

static void exi0_read_len(uint32_t addr, uint32_t* reg) { *reg = exi.regs[0].len; }
static void exi1_read_len(uint32_t addr, uint32_t* reg) { *reg = exi.regs[1].len; }
static void exi2_read_len(uint32_t addr, uint32_t* reg) { *reg = exi.regs[2].len; }
static void exi0_write_len(uint32_t addr, uint32_t data) { exi.regs[0].len = data; }
static void exi1_write_len(uint32_t addr, uint32_t data) { exi.regs[1].len = data; }
static void exi2_write_len(uint32_t addr, uint32_t data) { exi.regs[2].len = data; }

//
// EXI control 
//

static void exi_write_cr(int chan, uint32_t data)
{
	EXIRegs* regs = &exi.regs[chan];
	regs->cr = data;

	if (regs->cr & EXI_CR_TSTART)
	{
		if (exi.sel == -1)
		{
			Report(Channel::EXI, "device should be selected before transfer\n");
			return;
		}

		// start transfer
		EXITransfer[exi.chan = chan][exi.sel]();

		// complete transfer
		regs->cr &= ~EXI_CR_TSTART;

		// assert transfer complete interrupt
		regs->csr |= EXI_CSR_TCINT;
		EXIUpdateInterrupts();
	}
}

static void exi0_read_cr(uint32_t addr, uint32_t* reg) { *reg = exi.regs[0].cr; }
static void exi1_read_cr(uint32_t addr, uint32_t* reg) { *reg = exi.regs[1].cr; }
static void exi2_read_cr(uint32_t addr, uint32_t* reg) { *reg = exi.regs[2].cr; }
static void exi0_write_cr(uint32_t addr, uint32_t data) { exi_write_cr(0, data); }
static void exi1_write_cr(uint32_t addr, uint32_t data) { exi_write_cr(1, data); }
static void exi2_write_cr(uint32_t addr, uint32_t data) { exi_write_cr(2, data); }

//
// EXI immediate data
//

static void exi0_read_data(uint32_t addr, uint32_t* reg) { *reg = exi.regs[0].data; }
static void exi1_read_data(uint32_t addr, uint32_t* reg) { *reg = exi.regs[1].data; }
static void exi2_read_data(uint32_t addr, uint32_t* reg) { *reg = exi.regs[2].data; }
static void exi0_write_data(uint32_t addr, uint32_t data) { exi.regs[0].data = data; }
static void exi1_write_data(uint32_t addr, uint32_t data) { exi.regs[1].data = data; }
static void exi2_write_data(uint32_t addr, uint32_t data) { exi.regs[2].data = data; }

// ---------------------------------------------------------------------------
// init

void EIOpen(HWConfig* config)
{
	Report(Channel::EXI, "External devices interface bus\n");

	// clear registers
	memset(&exi, 0, sizeof(EIControl));

	// load user variables
	exi.log = config->exi_log;
	exi.osReport = config->exi_osReport;

	// reset devices
	exi.sel = -1;           // deselect MX device
	SRAMLoad(&exi.sram);    // load sram
	RTCUpdate();
	if (!mi.BootromPresent)
	{
		FontLoad(&exi.ansiFont, ANSI_SIZE, config->ansiFilename);
		FontLoad(&exi.sjisFont, SJIS_SIZE, config->sjisFilename);
	}

	// set traps for EXI channel 0 registers
	PISetTrap(32, EXI0_CSR, exi0_read_csr, exi0_write_csr);
	PISetTrap(32, EXI0_MADR, exi0_read_madr, exi0_write_madr);
	PISetTrap(32, EXI0_LEN, exi0_read_len, exi0_write_len);
	PISetTrap(32, EXI0_CR, exi0_read_cr, exi0_write_cr);
	PISetTrap(32, EXI0_DATA, exi0_read_data, exi0_write_data);

	// set traps for EXI channel 1 registers
	PISetTrap(32, EXI1_CSR, exi1_read_csr, exi1_write_csr);
	PISetTrap(32, EXI1_MADR, exi1_read_madr, exi1_write_madr);
	PISetTrap(32, EXI1_LEN, exi1_read_len, exi1_write_len);
	PISetTrap(32, EXI1_CR, exi1_read_cr, exi1_write_cr);
	PISetTrap(32, EXI1_DATA, exi1_read_data, exi1_write_data);

	// set traps for EXI channel 2 registers
	PISetTrap(32, EXI2_CSR, exi2_read_csr, exi2_write_csr);
	PISetTrap(32, EXI2_MADR, exi2_read_madr, exi2_write_madr);
	PISetTrap(32, EXI2_LEN, exi2_read_len, exi2_write_len);
	PISetTrap(32, EXI2_CR, exi2_read_cr, exi2_write_cr);
	PISetTrap(32, EXI2_DATA, exi2_read_data, exi2_write_data);

	// open memory cards
	MCOpen(config);

	// open broad band adapter
}

void EIClose()
{
	// close memory cards
	MCClose();

	// close broad band adapter

	// unload fonts
	FontUnload(&exi.ansiFont);
	FontUnload(&exi.sjisFont);

	// sync sram
	SRAMSave(&exi.sram);
}
