// DI - Flipper Disk Interface
#include "pch.h"

using namespace Debug;

namespace Flipper
{

	// ---------------------------------------------------------------------------
	// The aggregate disk interrupt
	//
	// The disk interface reports several internal causes to the CPU through ONE Processor
	// Interface line (PI_INTERRUPT_DI): the transfer-complete, the break and the device-error
	// causes in DISR, plus the cover cause in DICVR. Each cause has its own mask, and the line
	// follows the OR of the ones that are currently latched and unmasked.
	//
	// The line therefore cannot be "asserted by" a single cause: every place that changes a cause
	// or a mask has to re-evaluate it. Dropping it only inside one status-register write, and only
	// when that single write left every cause clear, leaves the line asserted forever as soon as
	// two causes are latched and the guest acknowledges them in separate writes - the CPU then
	// re-enters the external-interrupt handler on every rfi and the guest never returns to its
	// disk driver.
	void DiskInterface::DIUpdateInt()
	{
		// Each cause has its own mask bit (DI_SR_BRKINT/DI_SR_BRKINTMSK and so on), so the line is
		// the OR of the four (cause && its own mask) pairs. The cause bits and their masks are not
		// adjacent, so the two groups cannot be ANDed with each other.
		bool brk = (di.sr & DI_SR_BRKINT) != 0 && (di.sr & DI_SR_BRKINTMSK) != 0;
		bool tc = (di.sr & DI_SR_TCINT) != 0 && (di.sr & DI_SR_TCINTMSK) != 0;
		bool de = (di.sr & DI_SR_DEINT) != 0 && (di.sr & DI_SR_DEINTMSK) != 0;
		bool cvr = (di.cvr & DI_CVR_CVRINT) != 0 && (di.cvr & DI_CVR_CVRINTMSK) != 0;

		if (brk || tc || de || cvr)
		{
			HW->pi->PIAssertInt(PI_INTERRUPT_DI);
		}
		else
		{
			HW->pi->PIClearInt(PI_INTERRUPT_DI);
		}
	}

	// ---------------------------------------------------------------------------
	// cover control. Callbacks are issued from DDU 

	void DiskInterface::DIOpenCover(void *ctx)
	{
		DiskInterface* di = (DiskInterface*)ctx;
		// cover interrupt
		di->DICVR |= DI_CVR_CVRINT;
		di->DIUpdateInt();
	}

	void DiskInterface::DICloseCover(void* ctx)
	{
		DiskInterface* di = (DiskInterface*)ctx;
		// cover interrupt
		di->DICVR |= DI_CVR_CVRINT;
		di->DIUpdateInt();
	}

	// ---------------------------------------------------------------------------
	// Device Error

	void DiskInterface::DIErrorCallback(void* ctx)
	{
		DiskInterface* di = (DiskInterface*)ctx;
		di->DICR &= ~DI_CR_TSTART;

		di->DISR |= DI_SR_DEINT;
		di->DIUpdateInt();

		DVD::DDU->SetTransferCallbacks(DIHostToDduCallbackCommand, DIDduToHostCallback, di);
	}

	// ---------------------------------------------------------------------------
	// Transfer

	// DI breaks itself only after finishing next 32 Byte chunk of data
	void DiskInterface::DIBreak()
	{
		DICR &= ~DI_CR_TSTART;
		DISR &= ~DI_SR_BRK;

		DISR |= DI_SR_BRKINT;
		DIUpdateInt();

		DVD::DDU->SetTransferCallbacks(DIHostToDduCallbackCommand, DIDduToHostCallback, this);
	}

	// DDU transfer complete interrupt
	void DiskInterface::DITransferComplete()
	{
		DICR &= ~DI_CR_TSTART;

		DISR |= DI_SR_TCINT;
		DIUpdateInt();

		if (di.log) {
			Report(Channel::DI, "TransferComplete\n");
		}

		DVD::DDU->SetTransferCallbacks(DIHostToDduCallbackCommand, DIDduToHostCallback, this);
	}

	uint8_t DiskInterface::DIHostToDduCallbackCommand(void *ctx)
	{
		DiskInterface* di = (DiskInterface*)ctx;
		uint8_t data = 0;

		// DI Imm Write Command (DILEN ignored)

		if (di->di.hostToDduByteCounter < sizeof(di->di.cmdbuf))
		{
			data = di->di.cmdbuf[di->di.hostToDduByteCounter++];
		}

		if (di->di.hostToDduByteCounter >= sizeof(di->di.cmdbuf))
		{
			// Dont stop DDU Bus clock

			// Issue transfer data

			DVD::DDU->SetTransferCallbacks(DIHostToDduCallbackData, DIDduToHostCallback, di);
			DVD::DDU->StartTransfer(di->DICR & DI_CR_RW ? DVD::DduBusDirection::HostToDdu : DVD::DduBusDirection::DduToHost);

			if (di->DICR & DI_CR_RW)
			{
				di->di.hostToDduByteCounter = 32;       // A special value that overloads the FIFO before reading the first byte from the DDU side.
			}
			else
			{
				di->di.dduToHostByteCounter = 0;
			}
		}

		return data;
	}

	uint8_t DiskInterface::DIHostToDduCallbackData(void *ctx)
	{
		DiskInterface* di = (DiskInterface*)ctx;
		uint8_t data = 0;

		if (di->DICR & DI_CR_DMA)
		{
			// DI Dma Write

			if (di->di.hostToDduByteCounter >= 32)
			{
				di->di.hostToDduByteCounter = 0;

				if (di->DILEN)
				{
					uint32_t dimar = di->DIMAR & DI_DIMAR_MASK;
					uint8_t* ptr = (uint8_t*)HW->mem->MIGetMemoryPointerForIO(dimar);
					memcpy(ptr, di->di.dmaFifo, 32);
					di->DIMAR += 32;
					di->DILEN -= 32;
				}

				if (di->DILEN == 0)
				{
					DVD::DDU->TransferComplete();        // Stop DDU Bus clock
					di->DITransferComplete();
					return 0;
				}

				if (di->DISR & DI_SR_BRK)
				{
					// Can break only after writing next chunk
					di->DIBreak();
					return 0;
				}
			}

			data = di->di.dmaFifo[di->di.hostToDduByteCounter];
			di->di.hostToDduByteCounter++;
		}
		else
		{
			if (di->di.hostToDduByteCounter < sizeof(di->di.immbuf))
			{
				data = di->di.immbuf[di->di.hostToDduByteCounter++];
			}

			if (di->di.hostToDduByteCounter >= sizeof(di->di.immbuf))
			{
				DVD::DDU->TransferComplete();        // Stop DDU Bus clock
				di->DITransferComplete();
			}
		}

		return data;
	}

	void DiskInterface::DIDduToHostCallback(uint8_t data, void *ctx)
	{
		DiskInterface* di = (DiskInterface*)ctx;

		if (di->DICR & DI_CR_DMA)
		{
			// DI Dma Read

			di->di.dmaFifo[di->di.dduToHostByteCounter] = data;
			di->di.dduToHostByteCounter++;
			if (di->di.dduToHostByteCounter >= 32)
			{
				di->di.dduToHostByteCounter = 0;

				if (di->DISR & DI_SR_BRK)
				{
					// Can break only after reading next chunk
					di->DIBreak();
					return;
				}

				if (di->DILEN)
				{
					uint32_t dimar = di->DIMAR & DI_DIMAR_MASK;
					uint8_t* memptr = (uint8_t*)HW->mem->MIGetMemoryPointerForIO(dimar);
					memcpy(memptr, di->di.dmaFifo, 32);
					di->DIMAR += 32;
					di->DILEN -= 32;
				}

				if (di->DILEN == 0)
				{
					DVD::DDU->TransferComplete();    // Stop DDU Bus clock
					di->DITransferComplete();
				}
			}
		}
		else
		{
			// DI Imm Read (DILEN ignored)

			if (di->di.dduToHostByteCounter < sizeof(di->di.immbuf))
			{
				di->di.immbuf[di->di.dduToHostByteCounter] = data;
				di->di.dduToHostByteCounter++;
			}

			if (di->di.dduToHostByteCounter >= sizeof(di->di.immbuf))
			{
				di->di.dduToHostByteCounter = 0;
				DVD::DDU->TransferComplete();    // Stop DDU Bus clock
				di->DITransferComplete();
			}
		}
	}

	// ---------------------------------------------------------------------------
	// DI register traps

	// status register
	void DiskInterface::write_sr(uint16_t data)
	{
		// set masks
		if (data & DI_SR_BRKINTMSK) DISR |= DI_SR_BRKINTMSK;
		else DISR &= ~DI_SR_BRKINTMSK;
		if (data & DI_SR_TCINTMSK)  DISR |= DI_SR_TCINTMSK;
		else DISR &= ~DI_SR_TCINTMSK;

		// clear interrupts
		if (data & DI_SR_BRKINT)
		{
			DISR &= ~DI_SR_BRKINT;
		}
		if (data & DI_SR_TCINT)
		{
			DISR &= ~DI_SR_TCINT;
		}
		if (data & DI_SR_DEINT)
		{
			DISR &= ~DI_SR_DEINT;
		}
		// The guest acknowledged some of the internal causes; the aggregate line has to be
		// re-evaluated, not cleared from this one write.
		DIUpdateInt();

		// Issue break
		if (data & DI_SR_BRK)
		{
			// Send break to DDU immediately (DIBRK signal)
			DVD::DDU->Break();
		}
	}

	// control register
	void DiskInterface::write_cr(uint16_t data)
	{
		DICR = data;

		// start command
		if (DICR & DI_CR_TSTART)
		{
			// Issue command

			if (di.log) {

				Report(Channel::DI, "cmdbuf: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
					di.cmdbuf[0], di.cmdbuf[1], di.cmdbuf[2], di.cmdbuf[3],
					di.cmdbuf[4], di.cmdbuf[5], di.cmdbuf[6], di.cmdbuf[7],
					di.cmdbuf[8], di.cmdbuf[9], di.cmdbuf[10], di.cmdbuf[11]);
				Report(Channel::DI, "immbuf: %02X %02X %02X %02X\n",
					di.immbuf[0], di.immbuf[1], di.immbuf[2], di.immbuf[3]);
				Report(Channel::DI, "mar: 0x%08X, len: 0x%08X\n", DIMAR, DILEN);
			}

			di.hostToDduByteCounter = 0;
			DVD::DDU->SetTransferCallbacks(DIHostToDduCallbackCommand, DIDduToHostCallback, this);
			DVD::DDU->StartTransfer(DVD::DduBusDirection::HostToDdu);
		}
	}

	// cover register
	void DiskInterface::write_cvr(uint16_t data)
	{
		// clear cover interrupt
		if (data & DI_CVR_CVRINT)
		{
			DICVR &= ~DI_CVR_CVRINT;
		}

		// set mask
		if (data & DI_CVR_CVRINTMSK) DICVR |= DI_CVR_CVRINTMSK;
		else DICVR &= ~DI_CVR_CVRINTMSK;

		DIUpdateInt();
	}

	void DiskInterface::read_cvr(uint32_t* reg)
	{
		uint32_t value = DICVR & ~DI_CVR_CVR;

		if (DVD::DDU->GetCoverStatus() == DVD::CoverStatus::Open)
		{
			value |= DI_CVR_CVR;
		}

		*reg = value;
	}

	void DiskInterface::DIRegRead(uint32_t addr, uint32_t* reg, void* context)
	{
		DiskInterface* di = (DiskInterface*)context;

		switch (addr & 0xFF) {

			case DI_SR + 2:
				*reg = (uint16_t)di->DISR;
				break;
			case DI_CVR + 2:
				di->read_cvr(reg);
				break;
			case DI_CMDBUF0:
			case DI_CMDBUF0 + 2:
			case DI_CMDBUF1:
			case DI_CMDBUF1 + 2:
			case DI_CMDBUF2:
			case DI_CMDBUF2 + 2: {
				uint32_t ofs = (addr & 0xFF) - DI_CMDBUF0;
				*reg = (uint32_t)di->di.cmdbuf[ofs] << 8;
				*reg |= di->di.cmdbuf[ofs + 1];
				break;
			}
			case DI_MAR:
				*reg = (di->DIMAR >> 16) & DI_DIMAR_MASK_HI;
				break;
			case DI_MAR + 2:
				*reg = (uint16_t)di->DIMAR & DI_DIMAR_MASK_LO;
				break;
			case DI_LEN:
				*reg = di->DILEN >> 16;
				break;
			case DI_LEN + 2:
				*reg = (uint16_t)di->DILEN & ~0x1f;
				break;
			case DI_CR + 2:
				*reg = (uint16_t)di->DICR;
				break;
			case DI_IMMBUF:
			case DI_IMMBUF + 2: {
				uint32_t ofs = (addr & 0xFF) - DI_IMMBUF;
				*reg = (uint32_t)di->di.immbuf[ofs] << 8;
				*reg |= di->di.immbuf[ofs + 1];
				break;
			}
			case DI_CFG + 2:
				// register is read only.
				// currently, bit 0 is used for BootROM scramble disable ("chicken bit"), bits 1-7 are reserved
				// used in EXISync->__OSGetDIConfig call, return 0 and forget.
				*reg = 0;
				break;
			default:
				*reg = 0;
				break;
		}
	}

	void DiskInterface::DIRegWrite(uint32_t addr, uint32_t data, void* context)
	{
		DiskInterface* di = (DiskInterface*)context;

		switch (addr & 0xFF) {

			case DI_SR + 2:
				di->write_sr((uint16_t)data);
				break;
			case DI_CVR + 2:
				di->write_cvr((uint16_t)data);
				break;
			case DI_CMDBUF0:
			case DI_CMDBUF0 + 2:
			case DI_CMDBUF1:
			case DI_CMDBUF1 + 2:
			case DI_CMDBUF2:
			case DI_CMDBUF2 + 2: {
				uint32_t ofs = (addr & 0xFF) - DI_CMDBUF0;
				di->di.cmdbuf[ofs] = (uint8_t)(data >> 8);
				di->di.cmdbuf[ofs + 1] = (uint8_t)data;
				break;
			}
			case DI_MAR:
				di->DIMAR &= 0x0000ffff;
				di->DIMAR |= (data & DI_DIMAR_MASK_HI) << 16;
				break;
			case DI_MAR + 2:
				di->DIMAR &= 0xffff0000;
				di->DIMAR |= (uint16_t)data & DI_DIMAR_MASK_LO;
				break;
			case DI_LEN:
				di->DILEN &= 0x0000ffff;
				di->DILEN |= data << 16;
				break;
			case DI_LEN + 2:
				di->DILEN &= 0xffff0000;
				di->DILEN |= (uint16_t)data & ~0x1f;
				break;
			case DI_CR + 2:
				di->write_cr((uint16_t)data);
				break;
			case DI_IMMBUF:
			case DI_IMMBUF + 2: {
				uint32_t ofs = (addr & 0xFF) - DI_IMMBUF;
				di->di.immbuf[ofs] = (uint8_t)(data >> 8);
				di->di.immbuf[ofs + 1] = (uint8_t)data;
				break;
			}
			default:
				break;
		}
	}

	// ---------------------------------------------------------------------------
	// init

	DiskInterface::DiskInterface(Flipper* flipper, HWConfig* config)
	{
		Debug::Report(Debug::Channel::DI, "DVD interface hardware\n");

		// Current DVD is set by Loader, or when disk is swapped by UI.

		// clear registers
		memset(&di, 0, sizeof(di));

		di.log = config->di_log;

		// Register DDU callbacks
		DVD::DDU->SetCoverOpenCallback(DIOpenCover, this);
		DVD::DDU->SetCoverCloseCallback(DICloseCover, this);
		DVD::DDU->SetErrorCallback(DIErrorCallback, this);
		DVD::DDU->SetTransferCallbacks(DIHostToDduCallbackCommand, DIDduToHostCallback, this);

		// set register traps
		for (uint32_t i = 0; i < DI_REG_MAX; i += 2) {
			flipper->pi->PISetTrap(PI_REGSPACE_DI + i, DIRegRead, DIRegWrite, this);
		}
	}

	DiskInterface::~DiskInterface()
	{
		DVD::DDU->TransferComplete();

		DVD::DDU->SetCoverOpenCallback(nullptr, nullptr);
		DVD::DDU->SetCoverCloseCallback(nullptr, nullptr);
		DVD::DDU->SetErrorCallback(nullptr, nullptr);
		DVD::DDU->SetTransferCallbacks(nullptr, nullptr, nullptr);

		di.dduToHostByteCounter = 0;
		di.hostToDduByteCounter = 32;
	}
}