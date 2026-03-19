// DI - Flipper Disk Interface
#include "pch.h"

using namespace Debug;

// DI state (registers and other data)
DIState di;

static uint8_t DIHostToDduCallbackCommand();
static uint8_t DIHostToDduCallbackData();
static void DIDduToHostCallback(uint8_t data);

// ---------------------------------------------------------------------------
// cover control. Callbacks are issued from DDU 

static void DIOpenCover()
{
	// cover interrupt
	DICVR |= DI_CVR_CVRINT;
	if (DICVR & DI_CVR_CVRINTMSK)
	{
		PIAssertInt(PI_INTERRUPT_DI);
	}
}

static void DICloseCover()
{
	// cover interrupt
	DICVR |= DI_CVR_CVRINT;
	if (DICVR & DI_CVR_CVRINTMSK)
	{
		PIAssertInt(PI_INTERRUPT_DI);
	}
}

// ---------------------------------------------------------------------------
// Device Error

static void DIErrorCallback()
{
	DICR &= ~DI_CR_TSTART;

	DISR |= DI_SR_DEINT;
	if (DISR & DI_SR_DEINTMSK)
	{
		PIAssertInt(PI_INTERRUPT_DI);
	}

	DVD::DDU->SetTransferCallbacks(DIHostToDduCallbackCommand, DIDduToHostCallback);
}

// ---------------------------------------------------------------------------
// Transfer

// DI breaks itself only after finishing next 32 Byte chunk of data
static void DIBreak()
{
	DICR &= ~DI_CR_TSTART;
	DISR &= ~DI_SR_BRK;

	DISR |= DI_SR_BRKINT;
	if (DISR & DI_SR_BRKINTMSK)
	{
		PIAssertInt(PI_INTERRUPT_DI);
	}

	DVD::DDU->SetTransferCallbacks(DIHostToDduCallbackCommand, DIDduToHostCallback);
}

// DDU transfer complete interrupt
static void DITransferComplete()
{
	DICR &= ~DI_CR_TSTART;

	DISR |= DI_SR_TCINT;
	if (DISR & DI_SR_TCINTMSK)
	{
		PIAssertInt(PI_INTERRUPT_DI);
	}

	if (di.log) {
		Report(Channel::DI, "TransferComplete\n");
	}

	DVD::DDU->SetTransferCallbacks(DIHostToDduCallbackCommand, DIDduToHostCallback);
}

static uint8_t DIHostToDduCallbackCommand()
{
	uint8_t data = 0;

	// DI Imm Write Command (DILEN ignored)

	if (di.hostToDduByteCounter < sizeof(di.cmdbuf))
	{
		data = di.cmdbuf[di.hostToDduByteCounter++];
	}

	if (di.hostToDduByteCounter >= sizeof(di.cmdbuf))
	{
		// Dont stop DDU Bus clock

		// Issue transfer data

		DVD::DDU->SetTransferCallbacks(DIHostToDduCallbackData, DIDduToHostCallback);
		DVD::DDU->StartTransfer(DICR & DI_CR_RW ? DVD::DduBusDirection::HostToDdu : DVD::DduBusDirection::DduToHost);

		if (DICR & DI_CR_RW)
		{
			di.hostToDduByteCounter = 32;       // A special value that overloads the FIFO before reading the first byte from the DDU side.
		}
		else
		{
			di.dduToHostByteCounter = 0;
		}
	}

	return data;
}

static uint8_t DIHostToDduCallbackData()
{
	uint8_t data = 0;

	if (DICR & DI_CR_DMA)
	{
		// DI Dma Write

		if (di.hostToDduByteCounter >= 32)
		{
			di.hostToDduByteCounter = 0;

			if (DILEN)
			{
				uint32_t dimar = DIMAR & DI_DIMAR_MASK;
				uint8_t* ptr = (uint8_t *)MIGetMemoryPointerForIO(dimar);
				memcpy(ptr, di.dmaFifo, 32);
				DIMAR += 32;
				DILEN -= 32;
			}

			if (DILEN == 0)
			{
				DVD::DDU->TransferComplete();        // Stop DDU Bus clock
				DITransferComplete();
				return 0;
			}

			if (DISR & DI_SR_BRK)
			{
				// Can break only after writing next chunk
				DIBreak();
				return 0;
			}
		}

		data = di.dmaFifo[di.hostToDduByteCounter];
		di.hostToDduByteCounter++;
	}
	else
	{
		if (di.hostToDduByteCounter < sizeof(di.immbuf))
		{
			data = di.immbuf[di.hostToDduByteCounter++];
		}

		if (di.hostToDduByteCounter >= sizeof(di.immbuf))
		{
			DVD::DDU->TransferComplete();        // Stop DDU Bus clock
			DITransferComplete();
		}
	}

	return data;
}

static void DIDduToHostCallback(uint8_t data)
{
	if (DICR & DI_CR_DMA)
	{
		// DI Dma Read

		di.dmaFifo[di.dduToHostByteCounter] = data;
		di.dduToHostByteCounter++;
		if (di.dduToHostByteCounter >= 32)
		{
			di.dduToHostByteCounter = 0;

			if (DISR & DI_SR_BRK)
			{
				// Can break only after reading next chunk
				DIBreak();
				return;
			}

			if (DILEN)
			{
				uint32_t dimar = DIMAR & DI_DIMAR_MASK;
				uint8_t* memptr = (uint8_t *)MIGetMemoryPointerForIO(dimar);
				memcpy(memptr, di.dmaFifo, 32);
				DIMAR += 32;
				DILEN -= 32;
			}

			if (DILEN == 0)
			{
				DVD::DDU->TransferComplete();    // Stop DDU Bus clock
				DITransferComplete();
			}
		}
	}
	else
	{
		// DI Imm Read (DILEN ignored)

		if (di.dduToHostByteCounter < sizeof(di.immbuf))
		{
			di.immbuf[di.dduToHostByteCounter] = data;
			di.dduToHostByteCounter++;
		}

		if (di.dduToHostByteCounter >= sizeof(di.immbuf))
		{
			di.dduToHostByteCounter = 0;
			DVD::DDU->TransferComplete();    // Stop DDU Bus clock
			DITransferComplete();
		}
	}
}

// ---------------------------------------------------------------------------
// DI register traps

// status register
static void write_sr(uint16_t data)
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
	if ((DISR & DI_SR_BRKINT) == 0 && (DISR & DI_SR_TCINT) == 0 && (DISR & DI_SR_DEINT) == 0)
	{
		PIClearInt(PI_INTERRUPT_DI);
	}

	// Issue break
	if (data & DI_SR_BRK)
	{
		// Send break to DDU immediately (DIBRK signal)
		DVD::DDU->Break();
	}
}

// control register
static void write_cr(uint16_t data)
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
				di.cmdbuf[8], di.cmdbuf[9], di.cmdbuf[10], di.cmdbuf[11] );
			Report(Channel::DI, "immbuf: %02X %02X %02X %02X\n",
				di.immbuf[0], di.immbuf[1], di.immbuf[2], di.immbuf[3] );
			Report(Channel::DI, "mar: 0x%08X, len: 0x%08X\n", DIMAR, DILEN);
		}

		di.hostToDduByteCounter = 0;
		DVD::DDU->SetTransferCallbacks(DIHostToDduCallbackCommand, DIDduToHostCallback);
		DVD::DDU->StartTransfer(DVD::DduBusDirection::HostToDdu);
	}
}

// cover register
static void write_cvr(uint16_t data)
{
	// clear cover interrupt
	if (data & DI_CVR_CVRINT)
	{
		DICVR &= ~DI_CVR_CVRINT;
		PIClearInt(PI_INTERRUPT_DI);
	}

	// set mask
	if (data & DI_CVR_CVRINTMSK) DICVR |= DI_CVR_CVRINTMSK;
	else DICVR &= ~DI_CVR_CVRINTMSK;
}

static void read_cvr(uint32_t* reg)
{
	uint32_t value = DICVR & ~DI_CVR_CVR;

	if (DVD::DDU->GetCoverStatus() == DVD::CoverStatus::Open)
	{
		value |= DI_CVR_CVR;
	}

	*reg = value;
}

void DIRegRead(uint32_t addr, uint32_t* reg, void* context)
{
	switch (addr & 0xFF) {

		case DI_SR+2:
			*reg = (uint16_t)DISR;
			break;
		case DI_CVR+2:
			read_cvr(reg);
			break;
		case DI_CMDBUF0:
		case DI_CMDBUF0+2:
		case DI_CMDBUF1:
		case DI_CMDBUF1+2:
		case DI_CMDBUF2:
		case DI_CMDBUF2+2: {
			uint32_t ofs = (addr & 0xFF) - DI_CMDBUF0;
			*reg = (uint32_t)di.cmdbuf[ofs] << 8;
			*reg |= di.cmdbuf[ofs + 1];
			break;
		}
		case DI_MAR:
			*reg = (DIMAR >> 16) & DI_DIMAR_MASK_HI;
			break;
		case DI_MAR+2:
			*reg = (uint16_t)DIMAR & DI_DIMAR_MASK_LO;
			break;
		case DI_LEN:
			*reg = DILEN >> 16;
			break;
		case DI_LEN+2:
			*reg = (uint16_t)DILEN & ~0x1f;
			break;
		case DI_CR+2:
			*reg = (uint16_t)DICR;
			break;
		case DI_IMMBUF:
		case DI_IMMBUF+2: {
			uint32_t ofs = (addr & 0xFF) - DI_IMMBUF;
			*reg = (uint32_t)di.immbuf[ofs] << 8;
			*reg |= di.immbuf[ofs + 1];
			break;
		}
		case DI_CFG+2:
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

void DIRegWrite(uint32_t addr, uint32_t data, void* context)
{
	switch (addr & 0xFF) {

		case DI_SR+2:
			write_sr((uint16_t)data);
			break;
		case DI_CVR+2:
			write_cvr((uint16_t)data);
			break;
		case DI_CMDBUF0:
		case DI_CMDBUF0+2:
		case DI_CMDBUF1:
		case DI_CMDBUF1+2:
		case DI_CMDBUF2:
		case DI_CMDBUF2+2: {
			uint32_t ofs = (addr & 0xFF) - DI_CMDBUF0;
			di.cmdbuf[ofs] = (uint8_t)(data >> 8);
			di.cmdbuf[ofs + 1] = (uint8_t)data;
			break;
		}
		case DI_MAR:
			DIMAR &= 0x0000ffff;
			DIMAR |= (data & DI_DIMAR_MASK_HI) << 16;
			break;
		case DI_MAR+2:
			DIMAR &= 0xffff0000;
			DIMAR |= (uint16_t)data & DI_DIMAR_MASK_LO;
			break;
		case DI_LEN:
			DILEN &= 0x0000ffff;
			DILEN |= data << 16;
			break;
		case DI_LEN+2:
			DILEN &= 0xffff0000;
			DILEN |= (uint16_t)data & ~0x1f;
			break;
		case DI_CR+2:
			write_cr((uint16_t)data);
			break;
		case DI_IMMBUF:
		case DI_IMMBUF+2: {
			uint32_t ofs = (addr & 0xFF) - DI_IMMBUF;
			di.immbuf[ofs] = (uint8_t)(data >> 8);
			di.immbuf[ofs + 1] = (uint8_t)data;
			break;
		}
		default:
			break;
	}
}

// ---------------------------------------------------------------------------
// init

void DIOpen(HWConfig* config)
{
	Debug::Report(Debug::Channel::DI, "DVD interface hardware\n");

	// Current DVD is set by Loader, or when disk is swapped by UI.

	// clear registers
	memset(&di, 0, sizeof(di));

	di.log = config->di_log;

	// Register DDU callbacks
	DVD::DDU->SetCoverOpenCallback(DIOpenCover);
	DVD::DDU->SetCoverCloseCallback(DICloseCover);
	DVD::DDU->SetErrorCallback(DIErrorCallback);
	DVD::DDU->SetTransferCallbacks(DIHostToDduCallbackCommand, DIDduToHostCallback);

	// set register traps
	for (uint32_t i = 0; i < DI_REG_MAX; i += 2) {
		PISetTrap(PI_REGSPACE_DI + i, DIRegRead, DIRegWrite);
	}
}

void DIClose()
{
	DVD::DDU->TransferComplete();

	DVD::DDU->SetCoverOpenCallback(nullptr);
	DVD::DDU->SetCoverCloseCallback(nullptr);
	DVD::DDU->SetErrorCallback(nullptr);
	DVD::DDU->SetTransferCallbacks(nullptr, nullptr);

	di.dduToHostByteCounter = 0;
	di.hostToDduByteCounter = 32;
}