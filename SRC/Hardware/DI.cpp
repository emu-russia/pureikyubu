// DI - Flipper Disk Interface
#include "pch.h"

// DI state (registers and other data)
DIControl di;

static uint8_t DIHostToDduCallbackCommand();
static uint8_t DIHostToDduCallbackData();
static void DIDduToHostCallback(uint8_t data);

// ---------------------------------------------------------------------------
// cover control. Callbacks are issued from DDU 

static void DIOpenCover()
{
    // cover interrupt
    DICVR |= DI_CVR_CVRINT;
    if(DICVR & DI_CVR_CVRINTMSK)
    {
        PIAssertInt(PI_INTERRUPT_DI);
    }
}

static void DICloseCover()
{
    // cover interrupt
    DICVR |= DI_CVR_CVRINT;
    if(DICVR & DI_CVR_CVRINTMSK)
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

    EndProfileDVD();
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

    EndProfileDVD();
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

    DVD::DDU->SetTransferCallbacks(DIHostToDduCallbackCommand, DIDduToHostCallback);

    EndProfileDVD();
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
                MIReadBurst(dimar, di.dmaFifo);
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
                MIWriteBurst(dimar, di.dmaFifo);
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
static void __fastcall write_sr(uint32_t addr, uint32_t data)
{
    // set masks
    if(data & DI_SR_BRKINTMSK) DISR |= DI_SR_BRKINTMSK;
    else DISR &= ~DI_SR_BRKINTMSK;
    if(data & DI_SR_TCINTMSK)  DISR |= DI_SR_TCINTMSK;
    else DISR &= ~DI_SR_TCINTMSK;

    // clear interrupts
    if(data & DI_SR_BRKINT)
    {
        DISR &= ~DI_SR_BRKINT;
    }
    if(data & DI_SR_TCINT)
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
    if(data & DI_SR_BRK)
    {
        // Send break to DDU immediately (DIBRK signal)
        DVD::DDU->Break();
    }
}

static void __fastcall read_sr(uint32_t addr, uint32_t *reg)
{
    *reg = (uint16_t)DISR;
}

// control register
static void __fastcall write_cr(uint32_t addr, uint32_t data)
{
    DICR = (uint16_t)data;

    // start command
    if(DICR & DI_CR_TSTART)
    {
        // Issue command

        di.hostToDduByteCounter = 0;
        DVD::DDU->SetTransferCallbacks(DIHostToDduCallbackCommand, DIDduToHostCallback);
        DVD::DDU->StartTransfer(DVD::DduBusDirection::HostToDdu);

        BeginProfileDVD();
    }
}

static void __fastcall read_cr(uint32_t addr, uint32_t *reg)
{
    *reg = (uint16_t)DICR;
}

// cover register
static void __fastcall write_cvr(uint32_t addr, uint32_t data)
{
    // clear cover interrupt
    if(data & DI_CVR_CVRINT)
    {
        DICVR &= ~DI_CVR_CVRINT;
        PIClearInt(PI_INTERRUPT_DI);
    }

    // set mask
    if(data & DI_CVR_CVRINTMSK) DICVR |= DI_CVR_CVRINTMSK;
    else DICVR &= ~DI_CVR_CVRINTMSK;
}

static void __fastcall read_cvr(uint32_t addr, uint32_t *reg)
{
    uint32_t value = DICVR & ~DI_CVR_CVR;

    if (DVD::DDU->GetCoverStatus() == DVD::CoverStatus::Open)
    {
        value |= DI_CVR_CVR;
    }

    *reg = value;
}

// dma registers
static void __fastcall read_mar(uint32_t addr, uint32_t *reg)  { *reg = DIMAR & DI_DIMAR_MASK; }
static void __fastcall write_mar(uint32_t addr, uint32_t data) { DIMAR = data; }
static void __fastcall read_len(uint32_t addr, uint32_t *reg)  { *reg = DILEN & ~0x1f; }
static void __fastcall write_len(uint32_t addr, uint32_t data) { DILEN = data; }

static void DISetCommandBuffer(int n, uint32_t value)
{
    volatile uint8_t* ptr = &di.cmdbuf[n * 4];
    *(uint32_t*)ptr = _byteswap_ulong(value);
}

static uint32_t DIGetCommandBuffer(int n)
{
    volatile uint8_t* ptr = &di.cmdbuf[n * 4];
    return _byteswap_ulong(*(uint32_t*)ptr);
}

// di buffers
static void __fastcall read_cmdbuf0(uint32_t addr, uint32_t *reg)  { *reg = DIGetCommandBuffer(0); }
static void __fastcall write_cmdbuf0(uint32_t addr, uint32_t data) { DISetCommandBuffer(0, data); }
static void __fastcall read_cmdbuf1(uint32_t addr, uint32_t *reg)  { *reg = DIGetCommandBuffer(1); }
static void __fastcall write_cmdbuf1(uint32_t addr, uint32_t data) { DISetCommandBuffer(1, data); }
static void __fastcall read_cmdbuf2(uint32_t addr, uint32_t *reg)  { *reg = DIGetCommandBuffer(2); }
static void __fastcall write_cmdbuf2(uint32_t addr, uint32_t data) { DISetCommandBuffer(2, data); }
static void __fastcall read_immbuf(uint32_t addr, uint32_t *reg)   { *reg = _byteswap_ulong(*(uint32_t *)di.immbuf); }
static void __fastcall write_immbuf(uint32_t addr, uint32_t data)  { *(uint32_t*)di.immbuf = _byteswap_ulong(data); }

// register is read only.
// currently, bit 0 is used for ROM scramble disable (which ROM?), bits 1-7 are reserved
// used in EXISync->__OSGetDIConfig call, return 0 and forget.
static void __fastcall read_cfg(uint32_t addr, uint32_t *reg) { *reg = 0; }

// ---------------------------------------------------------------------------
// init

void DIOpen()
{
    DBReport2(DbgChannel::DI, "DVD interface hardware\n");

    // Current DVD is set by Loader, or when disk is swapped by UI.

    // clear registers
    memset(&di, 0, sizeof(DIControl));

    di.log = true;

    // Register DDU callbacks
    DVD::DDU->SetCoverOpenCallback(DIOpenCover);
    DVD::DDU->SetCoverCloseCallback(DICloseCover);
    DVD::DDU->SetErrorCallback(DIErrorCallback);
    DVD::DDU->SetTransferCallbacks(DIHostToDduCallbackCommand, DIDduToHostCallback);

    // set 32-bit register traps
    MISetTrap(32, DI_SR     , read_sr      , write_sr);
    MISetTrap(32, DI_CVR    , read_cvr     , write_cvr);
    MISetTrap(32, DI_CMDBUF0, read_cmdbuf0 , write_cmdbuf0);
    MISetTrap(32, DI_CMDBUF1, read_cmdbuf1 , write_cmdbuf1);
    MISetTrap(32, DI_CMDBUF2, read_cmdbuf2 , write_cmdbuf2);
    MISetTrap(32, DI_MAR    , read_mar     , write_mar);
    MISetTrap(32, DI_LEN    , read_len     , write_len);
    MISetTrap(32, DI_CR     , read_cr      , write_cr);
    MISetTrap(32, DI_IMMBUF , read_immbuf  , write_immbuf);
    MISetTrap(32, DI_CFG    , read_cfg     , NULL);
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

    EndProfileDVD();
}
