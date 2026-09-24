// EXI - external interface
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

namespace Flipper
{
	// EXI transfer bindings
	//
	// The device of a channel is the one that is plugged into the matching port of the peripheral
	// pool: a memory card in a card slot (see peripherals.h). The devices that are part of the
	// console itself - the boot ROM / RTC / SRAM chip on CS1B and the AD16 debugger of channel 2 -
	// are not peripherals and stay here.
	static EXITransferCallback exi_cb[3][3] = {
		{ ExternalInterface::CardTransferA	, MXTransfer						, ExternalInterface::UnknownTransfer },
		{ ExternalInterface::CardTransferB	, ExternalInterface::UnknownTransfer, ExternalInterface::UnknownTransfer },
		{ ExternalInterface::ADTransfer		, ExternalInterface::UnknownTransfer, ExternalInterface::UnknownTransfer }
	};


	// ---------------------------------------------------------------------------
	// EXI utilities

	//
	// update EXI interrupt status
	//

	void ExternalInterface::EXIUpdateInterrupts()
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
			HW->pi->PIAssertInt(PI_INTERRUPT_EXI);
		}
		else
		{
			// clear cpu interrupt
			HW->pi->PIClearInt(PI_INTERRUPT_EXI);
		}
	}

	//
	// attach / detach device on EXI channel
	//

	void ExternalInterface::EXIAttach(int chan)
	{
		if (exi.log) Report(Channel::EXI, "attaching device at channel %i\n", chan);

		// set attach flag
		exi.regs[chan].csr |= EXI_CSR_EXT;

		// assert attach interrupt
		exi.regs[chan].csr |= EXI_CSR_EXTINT;
		EXIUpdateInterrupts();
	}

	void ExternalInterface::EXIDetach(int chan)
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
	void ExternalInterface::UnknownTransfer(void *ctx)
	{
		ExternalInterface* exi = (ExternalInterface*)ctx;
		// dont do nothing on the exi transfer
		if (exi->exi.log)
		{
			Report(Channel::EXI, "unknown transfer (channel:%i, device:%i)\n", exi->exi.chan, exi->exi.sel);
		}
	}

	// The transfer of a memory card: the card that is plugged into the slot of the channel runs its
	// side of the protocol (see MemoryCardDevice in memcard.cpp).
	void ExternalInterface::CardTransferA(void* ctx)
	{
		Peripherals::Instance().TransferEXI(0, 0, (ExternalInterface*)ctx);
	}

	void ExternalInterface::CardTransferB(void* ctx)
	{
		Peripherals::Instance().TransferEXI(1, 0, (ExternalInterface*)ctx);
	}

	// AD16 device transfer (EXI device 2:0)
	// This is most likely a debugging device called `Barnacle`.
	void ExternalInterface::ADTransfer(void* ctx)
	{
		ExternalInterface* exi = (ExternalInterface*)ctx;
		// read or write ?
		switch (EXI_CR_RW(exi->exi.regs[2].cr))
		{
			case 0:                 // read
			{
				if (exi->exi.ad16_cmd == 0) exi->exi.regs[2].data = 0x04120000;
				else if (exi->exi.ad16_cmd == 0xa2000000) exi->exi.regs[2].data = exi->exi.ad16 << 16;
				else Report(Channel::EXI, "unknown AD16 command\n");
				return;
			}

			case 1:                 // write
			{
				if (exi->exi.firstImm)
				{
					exi->exi.firstImm = false;
					exi->exi.ad16_cmd = exi->exi.regs[2].data;
				}
				else
				{
					if (exi->exi.ad16_cmd != 0xa0000000)
					{
						Report(Channel::EXI, "unknown AD command (%08X)\n", exi->exi.ad16_cmd);
						return;
					}
					exi->exi.ad16 = exi->exi.regs[2].data >> 16;
					if (exi->exi.log) Report(Channel::EXI, "AD16 set to %04X\n", exi->exi.ad16);
				}
				return;
			}

			default:
			{
				if (EXI_CR_RW(exi->exi.regs[2].cr))
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

	void ExternalInterface::exi_select(int chan)
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

	void ExternalInterface::write_csr(int chan, uint32_t data)
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

	void ExternalInterface::exi_read_dummy(uint32_t addr, uint32_t* reg, void* ctx) { *reg = 0; }
	void ExternalInterface::exi_write_dummy(uint32_t addr, uint32_t data, void* ctx) {}

	void ExternalInterface::exi0_read_csr(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = exi->exi.regs[0].csr; }
	void ExternalInterface::exi1_read_csr(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = exi->exi.regs[1].csr; }
	void ExternalInterface::exi2_read_csr(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = exi->exi.regs[2].csr; }
	void ExternalInterface::exi0_write_csr(uint32_t addr, uint32_t data, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; exi->write_csr(0, data); }
	void ExternalInterface::exi1_write_csr(uint32_t addr, uint32_t data, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; exi->write_csr(1, data); }
	void ExternalInterface::exi2_write_csr(uint32_t addr, uint32_t data, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; exi->write_csr(2, data); }

	//
	// memory address for EXI DMA
	//

	void ExternalInterface::exi0_read_madrh(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = exi->exi.regs[0].madr >> 16; }
	void ExternalInterface::exi1_read_madrh(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = exi->exi.regs[1].madr >> 16; }
	void ExternalInterface::exi2_read_madrh(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = exi->exi.regs[2].madr >> 16; }
	void ExternalInterface::exi0_write_madrh(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[0].madr &= 0x0000ffff;
		exi->exi.regs[0].madr |= data << 16;
	}
	void ExternalInterface::exi1_write_madrh(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[1].madr &= 0x0000ffff;
		exi->exi.regs[1].madr |= data << 16;
	}
	void ExternalInterface::exi2_write_madrh(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[2].madr &= 0x0000ffff;
		exi->exi.regs[2].madr |= data << 16;
	}

	void ExternalInterface::exi0_read_madrl(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = (uint16_t)exi->exi.regs[0].madr; }
	void ExternalInterface::exi1_read_madrl(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = (uint16_t)exi->exi.regs[1].madr; }
	void ExternalInterface::exi2_read_madrl(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = (uint16_t)exi->exi.regs[2].madr; }
	void ExternalInterface::exi0_write_madrl(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[0].madr &= 0xffff0000;
		exi->exi.regs[0].madr |= (uint16_t)data;
	}
	void ExternalInterface::exi1_write_madrl(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[1].madr &= 0xffff0000;
		exi->exi.regs[1].madr |= (uint16_t)data;
	}
	void ExternalInterface::exi2_write_madrl(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[2].madr &= 0xffff0000;
		exi->exi.regs[2].madr |= (uint16_t)data;
	}

	//
	// data length for DMA
	//

	void ExternalInterface::exi0_read_lenh(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = exi->exi.regs[0].len >> 16; }
	void ExternalInterface::exi1_read_lenh(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = exi->exi.regs[1].len >> 16; }
	void ExternalInterface::exi2_read_lenh(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = exi->exi.regs[2].len >> 16; }
	void ExternalInterface::exi0_write_lenh(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[0].len &= 0x0000ffff;
		exi->exi.regs[0].len |= data << 16;
	}
	void ExternalInterface::exi1_write_lenh(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[1].len &= 0x0000ffff;
		exi->exi.regs[1].len |= data << 16;
	}
	void ExternalInterface::exi2_write_lenh(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[2].len &= 0x0000ffff;
		exi->exi.regs[2].len |= data << 16;
	}

	void ExternalInterface::exi0_read_lenl(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = (uint16_t)exi->exi.regs[0].len; }
	void ExternalInterface::exi1_read_lenl(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = (uint16_t)exi->exi.regs[1].len; }
	void ExternalInterface::exi2_read_lenl(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = (uint16_t)exi->exi.regs[2].len; }
	void ExternalInterface::exi0_write_lenl(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[0].len &= 0xffff0000;
		exi->exi.regs[0].len |= (uint16_t)data;
	}
	void ExternalInterface::exi1_write_lenl(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[1].len &= 0xffff0000;
		exi->exi.regs[1].len |= (uint16_t)data;
	}
	void ExternalInterface::exi2_write_lenl(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[2].len &= 0xffff0000;
		exi->exi.regs[2].len |= (uint16_t)data;
	}

	//
	// EXI control 
	//

	void ExternalInterface::exi_write_cr(int chan, uint32_t data)
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
			exi.chan = chan;
			exi_cb[exi.chan][exi.sel](this);

			// The transfer the device just performed moved `len` bytes between it and main memory
			// when it was a DMA one (issue #394). The emulator completes EXI transfers instantly
			// and does not touch main memory here - the device callback did - so this is where the
			// channel is accounted for.
			if (regs->cr & EXI_CR_DMA)
			{
				HwProfile::Count(HwProfile::Counter::DmaExi, regs->len);
			}

			// complete transfer
			regs->cr &= ~EXI_CR_TSTART;

			// assert transfer complete interrupt
			regs->csr |= EXI_CSR_TCINT;
			EXIUpdateInterrupts();
		}
	}

	void ExternalInterface::exi0_read_cr(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = exi->exi.regs[0].cr; }
	void ExternalInterface::exi1_read_cr(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = exi->exi.regs[1].cr; }
	void ExternalInterface::exi2_read_cr(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = exi->exi.regs[2].cr; }
	void ExternalInterface::exi0_write_cr(uint32_t addr, uint32_t data, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; exi->exi_write_cr(0, data); }
	void ExternalInterface::exi1_write_cr(uint32_t addr, uint32_t data, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; exi->exi_write_cr(1, data); }
	void ExternalInterface::exi2_write_cr(uint32_t addr, uint32_t data, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; exi->exi_write_cr(2, data); }

	//
	// EXI immediate data
	//

	void ExternalInterface::exi0_read_datah(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = exi->exi.regs[0].data >> 16; }
	void ExternalInterface::exi1_read_datah(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = exi->exi.regs[1].data >> 16; }
	void ExternalInterface::exi2_read_datah(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = exi->exi.regs[2].data >> 16; }
	void ExternalInterface::exi0_write_datah(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[0].data &= 0x0000ffff;
		exi->exi.regs[0].data |= data << 16;
	}
	void ExternalInterface::exi1_write_datah(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[1].data &= 0x0000ffff;
		exi->exi.regs[1].data |= data << 16;
	}
	void ExternalInterface::exi2_write_datah(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[2].data &= 0x0000ffff;
		exi->exi.regs[2].data |= data << 16;
	}

	void ExternalInterface::exi0_read_datal(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = (uint16_t)exi->exi.regs[0].data; }
	void ExternalInterface::exi1_read_datal(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = (uint16_t)exi->exi.regs[1].data; }
	void ExternalInterface::exi2_read_datal(uint32_t addr, uint32_t* reg, void* ctx) { ExternalInterface* exi = (ExternalInterface*)ctx; *reg = (uint16_t)exi->exi.regs[2].data; }
	void ExternalInterface::exi0_write_datal(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[0].data &= 0xffff0000;
		exi->exi.regs[0].data |= (uint16_t)data;
	}
	void ExternalInterface::exi1_write_datal(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[1].data &= 0xffff0000;
		exi->exi.regs[1].data |= (uint16_t)data;
	}
	void ExternalInterface::exi2_write_datal(uint32_t addr, uint32_t data, void* ctx) {
		ExternalInterface* exi = (ExternalInterface*)ctx;
		exi->exi.regs[2].data &= 0xffff0000;
		exi->exi.regs[2].data |= (uint16_t)data;
	}

	// ---------------------------------------------------------------------------
	// save states

	// The EXI is a protocol spoken over several register writes per transaction: the guest selects
	// a device and a direction in the channel's CSR, presents the command and the address in the
	// immediate data register, starts the transfer in the CR and reads the answer back out of the
	// data register. `mxaddr` and `ad16_cmd` are the two devices' own step counters and `firstImm`
	// is the "the next immediate write is the command, not the data" flag, so all of them are part
	// of the machine's state even though they are not registers the guest can read.
	//
	// The section, in the order written and read back:
	//
	//   for each of the three channels: CSR (16 bits), MADR (32), LEN (32), CR (16), DATA (32)
	//   the SRAM the MX chip backs: the settings record, field by field (64 bytes)
	//   `firstImm` (1 byte), `mxaddr` (32), `ad16` (32), `ad16_cmd` (32), `chan` (32), `sel` (32)
	void ExternalInterface::SaveState(SaveStates::StateWriter& writer) const
	{
		// The register file of the three channels. CSR carries the interrupt latches (EXTINT,
		// TCINT, EXIINT) and their masks, which is the interrupt state of the block; CR never
		// comes back with TSTART set, because the transfer it starts completes inside the write
		// that starts it (see exi_write_cr), so there is no in-flight transfer to reconstruct.
		for (int chan = 0; chan < 3; chan++)
		{
			const EXIRegs& regs = exi.regs[chan];
			writer.Fields(regs.csr, regs.madr, regs.len, regs.cr, regs.data);
		}

		// The battery-backed settings record. It is guest state: the IPL's calendar and options
		// screens write the language, the display offset and the counter bias into it, and they
		// are read back by the guest through the same MX chip window. The block loads it from
		// `Data/sram.bin` at construction and writes that file only when it is destroyed, so
		// during a session the copy in the machine is the only current one - a state that left it
		// out would show the guest the settings the emulator started with.
		//
		// The struct is expanded field by field (the cursor takes integers, not structs); the
		// reserved tail travels raw, because the guest can write those bytes too.
		writer.Fields(exi.sram.checkSum, exi.sram.checkSumInv, exi.sram.ead0, exi.sram.ead1,
			exi.sram.counterBias, exi.sram.displayOffsetH, exi.sram.ntd, exi.sram.language,
			exi.sram.flags);
		writer.Raw(exi.sram.dummy, sizeof(exi.sram.dummy));

		// The protocol state. `sel` is the device the last CSR write selected (-1 for none) and
		// `chan` the channel of the last transfer, so they are what the next immediate write
		// consults. `ad16` is the Barnacle's trace step and `ad16_cmd` the command it is in the
		// middle of, which is exactly the kind of state split over several guest writes the
		// section has to keep.
		writer.Fields(exi.firstImm, exi.mxaddr, exi.ad16, exi.ad16_cmd, exi.chan, exi.sel);

		// NOT part of the state:
		//
		// `rtcVal` is the host clock, not the machine: RTCUpdate re-derives it on every read of
		// the RTC register (bootrtc.cpp), so saving it would only pin a value the next read
		// replaces anyway.
		//
		// `uart[256]`, `upos` and `uartNE` are the OSReport text the guest prints through the MX
		// chip's UART window and the cursor into it. The guest never reads that buffer back (a
		// read of the UART register answers a status byte), so it is host output state: restoring
		// it would make the block print text the user has already seen. `uartNE` is not read or
		// written anywhere in this build at all.
		//
		// `ansiFont`, `sjisFont`, `bootrom`, `bootromSize` and `BootromPresent` are host pointers
		// to images the front end loaded from files (the IPL font and the boot ROM), not machine
		// state; the boot ROM is named by the state's own META section instead. Loading a state
		// must not re-load or free them.
		//
		// The memory cards are not here either: their contents live in the device's own file and
		// are written through immediately, and the card's protocol state belongs to the device in
		// the peripheral pool (see MemoryCardDevice in memcard.cpp), not to the EXI block. The
		// same goes for the channel configuration - what is plugged into a slot is a setting, and
		// a state says nothing about it.
		//
		// `log` and `osReport` are settings.
	}

	void ExternalInterface::LoadState(SaveStates::StateReader& reader)
	{
		for (int chan = 0; chan < 3; chan++)
		{
			EXIRegs& regs = exi.regs[chan];
			reader.Fields(regs.csr, regs.madr, regs.len, regs.cr, regs.data);
		}

		reader.Fields(exi.sram.checkSum, exi.sram.checkSumInv, exi.sram.ead0, exi.sram.ead1,
			exi.sram.counterBias, exi.sram.displayOffsetH, exi.sram.ntd, exi.sram.language,
			exi.sram.flags);
		reader.Raw(exi.sram.dummy, sizeof(exi.sram.dummy));

		// The registers are put back as values, never through the write entries: writing a CR
		// would start the transfer it describes, writing a CSR would re-run the device select and
		// writing the data register would push bytes into a device. The decoded values are what
		// the guest would have read back, so this is what the block must hold.
		//
		// The channel and the device select are read into locals first: a channel or a select that
		// no code path can produce is a broken image, not a machine (`sel` is only ever set to 0,
		// 1, 2 or -1 by exi_select, `chan` to 0..2 by exi_write_cr), and the next transfer would
		// index the dispatch table with it.
		int32_t chan = 0;
		int32_t sel = -1;

		reader.Fields(exi.firstImm, exi.mxaddr, exi.ad16, exi.ad16_cmd, chan, sel);

		if (reader.Failed())
		{
			return;
		}

		if (sel < -1 || sel > 2)
		{
			reader.Fail("the EXI device select is not one this machine has");
			return;
		}

		if (chan < 0 || chan > 2)
		{
			reader.Fail("the EXI channel is not one this machine has");
			return;
		}

		exi.chan = chan;
		exi.sel = sel;

		// The latched causes and their masks are back, so the block's own "apply" helper is run to
		// re-derive the processor interrupt line from them. It is exactly what every CSR / CR write
		// does at its end, and it is a pure function of the three CSR registers (see
		// EXIUpdateInterrupts), so it restores the line the state had.
		//
		// It has to run here, after the registers, and it depends on the processor interface's
		// section being already in place: the helper recomputes the *core's* pending-interrupt
		// line from the PI's whole cause and mask registers (PIAssertInt / PIClearInt derive
		// `Core->AssertInterrupt`), not only from the EXI bit. The state's sections are written and
		// applied with the PI before this one, which is what makes it correct; the disk interface
		// does the same with its DIUpdateInt. If the section order ever changes, this call has to
		// move to the post-load refresh the caller runs once the whole machine is in.
		EXIUpdateInterrupts();
	}

	// ---------------------------------------------------------------------------
	// init

	ExternalInterface::ExternalInterface(Flipper* flipper, HWConfig* config)
	{
		Report(Channel::EXI, "External devices interface bus\n");

		// clear registers
		memset(&exi, 0, sizeof(exi));

		// load user variables
		exi.log = config->exi_log;
		exi.osReport = config->exi_osReport;

		// reset devices
		exi.sel = -1;           // deselect MX device
		SRAMLoad(&exi.sram);    // load sram
		RTCUpdate(&exi.rtcVal);
		if (!exi.BootromPresent)
		{
			FontLoad(&exi.ansiFont, ANSI_SIZE, config->ansiFilename);
			FontLoad(&exi.sjisFont, SJIS_SIZE, config->sjisFilename);
		}

		// set traps for EXI channel 0 registers
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI0_CSR, exi_read_dummy, exi_write_dummy, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI0_CSR + 2, exi0_read_csr, exi0_write_csr, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI0_MADR, exi0_read_madrh, exi0_write_madrh, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI0_MADR + 2, exi0_read_madrl, exi0_write_madrl, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI0_LEN, exi0_read_lenh, exi0_write_lenh, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI0_LEN + 2, exi0_read_lenl, exi0_write_lenl, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI0_CR, exi_read_dummy, exi_write_dummy, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI0_CR + 2, exi0_read_cr, exi0_write_cr, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI0_DATA, exi0_read_datah, exi0_write_datah, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI0_DATA + 2, exi0_read_datal, exi0_write_datal, this);

		// set traps for EXI channel 1 registers
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI1_CSR, exi_read_dummy, exi_write_dummy, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI1_CSR + 2, exi1_read_csr, exi1_write_csr, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI1_MADR, exi1_read_madrh, exi1_write_madrh, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI1_MADR + 2, exi1_read_madrl, exi1_write_madrl, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI1_LEN, exi1_read_lenh, exi1_write_lenh, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI1_LEN + 2, exi1_read_lenl, exi1_write_lenl, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI1_CR, exi_read_dummy, exi_write_dummy, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI1_CR + 2, exi1_read_cr, exi1_write_cr, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI1_DATA, exi1_read_datah, exi1_write_datah, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI1_DATA + 2, exi1_read_datal, exi1_write_datal, this);

		// set traps for EXI channel 2 registers
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI2_CSR, exi_read_dummy, exi_write_dummy, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI2_CSR + 2, exi2_read_csr, exi2_write_csr, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI2_MADR, exi2_read_madrh, exi2_write_madrh, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI2_MADR + 2, exi2_read_madrl, exi2_write_madrl, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI2_LEN, exi2_read_lenh, exi2_write_lenh, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI2_LEN + 2, exi2_read_lenl, exi2_write_lenl, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI2_CR, exi_read_dummy, exi_write_dummy, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI2_CR + 2, exi2_read_cr, exi2_write_cr, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI2_DATA, exi2_read_datah, exi2_write_datah, this);
		flipper->pi->PISetTrap(PI_REGSPACE_EXI | EXI2_DATA + 2, exi2_read_datal, exi2_write_datal, this);

		LoadBootrom(config, exi.BootromPresent, exi.bootromSize, &exi.bootrom);
	}

	ExternalInterface::~ExternalInterface()
	{
		// unload fonts
		FontUnload(&exi.ansiFont);
		FontUnload(&exi.sjisFont);

		// sync sram
		SRAMSave(&exi.sram);

		if (exi.bootrom)
		{
			delete[] exi.bootrom;
			exi.bootrom = nullptr;
		}
	}
}