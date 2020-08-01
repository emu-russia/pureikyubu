// AR - auxiliary RAM (audio RAM) interface
#include "pch.h"

/* ---------------------------------------------------------------------------
   useful bits from AIDCR : 
        AIDCR_ARDMA         - ARAM dma in progress
        AIDCR_ARINTMSK      - mask (blocks PI)
        AIDCR_ARINT         - wr:clear, rd:dma int active

   short description of ARAM transfer :

      AR_DMA_MMADDR_H = (AR_DMA_MMADDR_H & 0x03FF) | (mainmem_addr >> 16);
      AR_DMA_MMADDR_L = (AR_DMA_MMADDR_L & 0x001F) | (mainmem_addr & 0xFFFF);

      AR_DMA_ARADDR_H = (AR_DMA_ARADDR_H & 0x03FF) | (aram_addr >> 16);
      AR_DMA_ARADDR_L = (AR_DMA_ARADDR_L & 0x001F) | (aram_addr & 0xFFFF);

      AR_DMA_CNT_H = (AR_DMA_CNT_H & 0x7FFF) | (type << 15);    type - 0:RAM->ARAM, 1:ARAM->RAM
      AR_DMA_CNT_H = (AR_DMA_CNT_H & 0x03FF) | (length >> 16);
      AR_DMA_CNT_L = (AR_DMA_CNT_L & 0x001F) | (length & 0xFFFF);

   transfer starts, by writing into CNT_L

--------------------------------------------------------------------------- */

using namespace Debug;

ARControl aram;

static void ARINT()
{
    AIDCR |= AIDCR_ARINT;
    if(AIDCR & AIDCR_ARINTMSK)
    {
        if (aram.log)
        {
            Report(Channel::AR, "ARINT\n");
        }
        PIAssertInt(PI_INTERRUPT_DSP);
    }
}

static void ARAMDmaThread(void* Parameter)
{
    while (true)
    {
        if (Gekko::Gekko->GetTicks() < aram.gekkoTicks)
            continue;
        aram.gekkoTicks = Gekko::Gekko->GetTicks() + aram.gekkoTicksPerSlice;

        int type = aram.cnt >> 31;
        uint32_t cnt = aram.cnt & 0x3FF'FFE0;

        // blast data
        if (type == RAM_TO_ARAM)
        {
            memcpy(&ARAM[aram.araddr], &mi.ram[aram.mmaddr], 32);
        }
        else
        {
            memcpy(&mi.ram[aram.mmaddr], &ARAM[aram.araddr], 32);
        }

        aram.araddr += 32;
        aram.mmaddr += 32;
        cnt -= 32;
        aram.cnt = cnt | (type << 31);

        if ((aram.cnt & ~0x8000'0000) == 0)
        {
            AIDCR &= ~AIDCR_ARDMA;
            ARINT();                    // invoke aram TC interrupt
            //if (aram.dspRunningBeforeAramDma)
            //{
            //    Flipper::HW->DSP->Run();
            //}
            aram.dmaThread->Suspend();
        }
    }
}

static void ARDMA()
{
    int type = aram.cnt >> 31;
    int cnt = aram.cnt & 0x3FF'FFE0;
    bool specialAramDspDma = aram.mmaddr == 0x0100'0000 && aram.araddr == 0;

    // inform developer about aram transfers
    if (aram.log)
    {
        if (type == RAM_TO_ARAM)
        {
            if (!specialAramDspDma)
            {
                Report(Channel::AR, "RAM copy %08X -> %08X (%i)", aram.mmaddr, aram.araddr, cnt);
            }
        }
        else Report(Channel::AR, "ARAM copy %08X -> %08X (%i)", aram.araddr, aram.mmaddr, cnt);
    }

    // Special ARAM DMA (DSP Init)

    if (specialAramDspDma)
    {
        // Transfer size multiplied by 4
        cnt *= 4;

        // Special ARAM DMA to IRAM

        Flipper::HW->DSP->SpecialAramImemDma(&mi.ram[aram.mmaddr], cnt);

        aram.cnt &= 0x80000000;     // clear dma counter
        ARINT();                    // invoke aram TC interrupt
        return;
    }

    // ARAM driver is trying to check for expansion
    // by reading ARAM on high addresses
    // we are not allowing to read expansion
    if(aram.araddr >= ARAMSIZE)
    {
        if(type == ARAM_TO_RAM)
        {
            memset(&mi.ram[aram.mmaddr], 0, cnt);

            aram.cnt &= 0x80000000;     // clear dma counter
            ARINT();                    // invoke aram TC interrupt
        }
        return;
    }

    // For other cases - delegate job to thread

    assert(!aram.dmaThread->IsRunning());
    AIDCR |= AIDCR_ARDMA;
    aram.gekkoTicks = Gekko::Gekko->GetTicks() + aram.gekkoTicksPerSlice;
    aram.dspRunningBeforeAramDma = Flipper::HW->DSP->IsRunning();
    //if (aram.dspRunningBeforeAramDma)
    //{
    //    Flipper::HW->DSP->Suspend();
    //}
    aram.dmaThread->Resume();
}

// ---------------------------------------------------------------------------
// 16-bit ARAM registers

// RAM pointer

static void ar_write_maddr_h(uint32_t addr, uint32_t data)
{
    aram.mmaddr &= 0x0000ffff;
    aram.mmaddr |= ((data & 0x3ff) << 16);
}
static void ar_read_maddr_h(uint32_t addr, uint32_t *reg) { *reg = (aram.mmaddr >> 16) & 0x3FF; }

static void ar_write_maddr_l(uint32_t addr, uint32_t data)
{
    aram.mmaddr &= 0xffff0000;
    aram.mmaddr |= ((data & ~0x1F) & 0xffff);
}
static void ar_read_maddr_l(uint32_t addr, uint32_t *reg) { *reg = (uint16_t)aram.mmaddr & ~0x1F; }

// ARAM pointer

static void ar_write_araddr_h(uint32_t addr, uint32_t data)
{
    aram.araddr &= 0x0000ffff;
    aram.araddr |= ((data & 0x3FF) << 16);
}
static void ar_read_araddr_h(uint32_t addr, uint32_t *reg) { *reg = (aram.araddr >> 16) & 0x3FF; }

static void ar_write_araddr_l(uint32_t addr, uint32_t data)
{
    aram.araddr &= 0xffff0000;
    aram.araddr |= ((data & ~0x1F) & 0xffff);
}
static void ar_read_araddr_l(uint32_t addr, uint32_t *reg) { *reg = (uint16_t)aram.araddr & ~0x1F; }

//
// byte count register
//

static void ar_write_cnt_h(uint32_t addr, uint32_t data)
{
    aram.cnt &= 0x0000ffff;
    aram.cnt |= ((data & 0x83FF) << 16);
}
static void ar_read_cnt_h(uint32_t addr, uint32_t *reg) { *reg = (aram.cnt >> 16) & 0x83FF; }

static void ar_write_cnt_l(uint32_t addr, uint32_t data)
{
    aram.cnt &= 0xffff0000;
    aram.cnt |= ((data & ~0x1F) & 0xffff);
    ARDMA();
}
static void ar_read_cnt_l(uint32_t addr, uint32_t *reg) { *reg = (uint16_t)aram.cnt & ~0x1F; }

//
// hacks
//

static void no_read(uint32_t addr, uint32_t *reg)  { *reg = 0; }
static void no_write(uint32_t addr, uint32_t data) {}

static void ar_hack_size_r(uint32_t addr, uint32_t *reg) { *reg = aram.size; }
static void ar_hack_size_w(uint32_t addr, uint32_t data) { aram.size = (uint16_t)data; }
static void ar_hack_mode(uint32_t addr, uint32_t *reg)   { *reg = 1; }

// ---------------------------------------------------------------------------
// 32-bit ARAM registers

static void ar_write_maddr(uint32_t addr, uint32_t data)   { aram.mmaddr = data & 0x03FF'FFE0; }
static void ar_read_maddr(uint32_t addr, uint32_t *reg)    { *reg = aram.mmaddr; }

static void ar_write_araddr(uint32_t addr, uint32_t data)  { aram.araddr = data & 0x03FF'FFE0; }
static void ar_read_araddr(uint32_t addr, uint32_t *reg)   { *reg = aram.araddr; }

static void ar_write_cnt(uint32_t addr, uint32_t data)
{
    aram.cnt = data & 0x83FF'FFE0;
    ARDMA();
}
static void ar_read_cnt(uint32_t addr, uint32_t *reg)      { *reg = aram.cnt & 0x83FF'FFE0; }

// ---------------------------------------------------------------------------
// init

void AROpen()
{
    Report(Channel::AR, "Aux. memory (ARAM) driver\n");

    // reallocate ARAM
    ARAM = new uint8_t[ARAMSIZE];

    // clear ARAM data
    memset(ARAM, 0, ARAMSIZE);

    // clear registers
    aram.mmaddr = aram.araddr = aram.cnt = 0;
    aram.gekkoTicksPerSlice = 1;
    aram.log = false;

    // set traps to aram registers
    MISetTrap(16, AR_DMA_MMADDR_H, ar_read_maddr_h, ar_write_maddr_h);
    MISetTrap(16, AR_DMA_MMADDR_L, ar_read_maddr_l, ar_write_maddr_l);
    MISetTrap(16, AR_DMA_ARADDR_H, ar_read_araddr_h, ar_write_araddr_h);
    MISetTrap(16, AR_DMA_ARADDR_L, ar_read_araddr_l, ar_write_araddr_l);
    MISetTrap(16, AR_DMA_CNT_H, ar_read_cnt_h, ar_write_cnt_h);
    MISetTrap(16, AR_DMA_CNT_L, ar_read_cnt_l, ar_write_cnt_l);

    MISetTrap(32, AR_DMA_MMADDR, ar_read_maddr, ar_write_maddr);
    MISetTrap(32, AR_DMA_ARADDR, ar_read_araddr, ar_write_araddr);
    MISetTrap(32, AR_DMA_CNT, ar_read_cnt, ar_write_cnt);

    // hacks
    MISetTrap(16, AR_SIZE   , ar_hack_size_r, ar_hack_size_w);
    MISetTrap(16, AR_MODE   , ar_hack_mode  , no_write);
    MISetTrap(16, AR_REFRESH, no_read       , no_write);

    aram.dmaThread = new Thread(ARAMDmaThread, true, nullptr, "ARAMDmaThread");
}

void ARClose()
{
    delete aram.dmaThread;
    aram.dmaThread = nullptr;

    // destroy ARAM
    if(ARAM)
    {
        delete [] ARAM;
        ARAM = nullptr;
    }
}
