// AR - auxiliary RAM (audio RAM) interface
#include "pch.h"

/* ---------------------------------------------------------------------------
   useful bits from AIDCR : 
        AIDCR_ARINTMSK      - mask (blocks PI)
        AIDCR_ARINT         - wr:clear, rd:dma int active

   short description of ARAM transfer :

      AR_DMA_MMADDR_H = (AR_DMA_MMADDR_H & 0x03FF) | (mainmem_addr >> 16);
      AR_DMA_MMADDR_L = (AR_DMA_MMADDR_L & 0x001F) | (mainmem_addr & 0xFFFF);

      AR_DMA_ARADDR_H = (AR_DMA_ARADDR_H & 0x03FF) | (aram_addr >> 16);
      AR_DMA_ARADDR_L = (AR_DMA_ARADDR_L & 0x001F) | (aram_addr & 0xFFFF);

      AR_DMA_CNT_H = (AR_DMA_CNT_H & 0x7FFF) | (type << 15);
      AR_DMA_CNT_H = (AR_DMA_CNT_H & 0x03FF) | (length >> 16);
      AR_DMA_CNT_L = (AR_DMA_CNT_L & 0x001F) | (length & 0xFFFF);

   transfer starts, when CNT_H and CNT_L are become double-buffered (both valid)
   (where type - 0:RAM->ARAM, 1:ARAM->RAM)

--------------------------------------------------------------------------- */

ARControl aram;

// ---------------------------------------------------------------------------

static void ARINT()
{
    if(AIDCR & AIDCR_ARINTMSK)
    {
        AIDCR |= AIDCR_ARINT;
        PIAssertInt(PI_INTERRUPT_DSP);
    }
}

static void ARDMA(BOOL type, uint32_t maddr, uint32_t aaddr, uint32_t size)
{
    maddr &= RAMMASK;

    if(aram.cntv[0] && aram.cntv[1])
    {
        aram.cntv[0] = aram.cntv[1] = FALSE;    // invalidate

        // inform developer about aram transfers
        // and do some alignment checks
        if (type == RAM_TO_ARAM)
        {
            bool specialAramDspDma = maddr == 0x0100'0000 && aaddr == 0;

            if (!specialAramDspDma)
            {
                DBReport2(DbgChannel::AR, "RAM copy %08X -> %08X (%i)", maddr, aaddr, size);
            }
        }
        else DBReport2(DbgChannel::AR, "ARAM copy %08X -> %08X (%i)", aaddr, maddr, size);

        // main memory address is not a multiple of 32 bytes
        assert((maddr & 0x1f) == 0);
        // DMA transfer length is not a multiple of 32 bytes!
        assert((size & 0x1f) == 0);

        // ARAM driver is trying to check for expansion
        // by reading ARAM on high addresses
        // we are not allowing to read expansion
        if(aaddr >= ARAMSIZE)
        {
            if(type == ARAM_TO_RAM)
            {
                memset(&mi.ram[maddr], 0, size);

                aram.cnt &= 0x80000000;     // clear dma counter
                ARINT();                    // invoke aram TC interrupt
            }
            return;
        }

        // blast data
        if (type == RAM_TO_ARAM)
        {
            if (maddr == 0x0100'0000 && aaddr == 0)
            {
                // Transfer size multiplied by 4
                size *= 4;

                // Special ARAM DMA to IRAM
                memcpy(Flipper::HW->DSP->iram, &mi.ram[maddr], size);

                DBReport2(DbgChannel::DSP, "MMEM -> IRAM transfer %d bytes.\n", size);
            }
            else
            {
                memcpy(&ARAM[aaddr], &mi.ram[maddr], size);
            }
        }
        else
        {
            // Not sure if DSP IRAM is mapped this way also.. Leave it now

            memcpy(&mi.ram[maddr], &ARAM[aaddr], size);
        }

        aram.cnt &= 0x80000000;     // clear dma counter
        ARINT();                    // invoke aram TC interrupt
    }
}

// ---------------------------------------------------------------------------
// 16-bit ARAM registers

// RAM pointer

static void __fastcall ar_write_maddr_h(uint32_t addr, uint32_t data)
{
    aram.mmaddr &= 0x0000ffff;
    aram.mmaddr |= (data << 16);
}
static void __fastcall ar_read_maddr_h(uint32_t addr, uint32_t *reg) { *reg = aram.mmaddr >> 16; }

static void __fastcall ar_write_maddr_l(uint32_t addr, uint32_t data)
{
    aram.mmaddr &= 0xffff0000;
    aram.mmaddr |= (data & 0xffff);
}
static void __fastcall ar_read_maddr_l(uint32_t addr, uint32_t *reg) { *reg = (uint16_t)aram.mmaddr; }

// ARAM pointer

static void __fastcall ar_write_araddr_h(uint32_t addr, uint32_t data)
{
    aram.araddr &= 0x0000ffff;
    aram.araddr |= (data << 16);
}
static void __fastcall ar_read_araddr_h(uint32_t addr, uint32_t *reg) { *reg = aram.araddr >> 16; }

static void __fastcall ar_write_araddr_l(uint32_t addr, uint32_t data)
{
    aram.araddr &= 0xffff0000;
    aram.araddr |= (data & 0xffff);
}
static void __fastcall ar_read_araddr_l(uint32_t addr, uint32_t *reg) { *reg = (uint16_t)aram.araddr; }

//
// byte count register (always 0)
//

static void __fastcall ar_write_cnt_h(uint32_t addr, uint32_t data)
{
    aram.cnt &= 0x0000ffff;
    aram.cnt |= (data << 16);

    aram.cntv[0] = TRUE;
    ARDMA(aram.cnt >> 31, aram.mmaddr, aram.araddr, aram.cnt & 0x7fffffff);
}
static void __fastcall ar_read_cnt_h(uint32_t addr, uint32_t *reg) { *reg = aram.cnt >> 16; }

static void __fastcall ar_write_cnt_l(uint32_t addr, uint32_t data)
{
    aram.cnt &= 0xffff0000;
    aram.cnt |= (data & 0xffff);

    aram.cntv[1] = TRUE;
    ARDMA(aram.cnt >> 31, aram.mmaddr, aram.araddr, aram.cnt & 0x7fffffff);
}
static void __fastcall ar_read_cnt_l(uint32_t addr, uint32_t *reg) { *reg = (uint16_t)aram.cnt; }

//
// hacks
//

static void __fastcall no_read(uint32_t addr, uint32_t *reg)  { *reg = 0; }
static void __fastcall no_write(uint32_t addr, uint32_t data) {}

static void __fastcall ar_hack_size_r(uint32_t addr, uint32_t *reg) { *reg = aram.size; }
static void __fastcall ar_hack_size_w(uint32_t addr, uint32_t data) { aram.size = (uint16_t)data; }
static void __fastcall ar_hack_mode(uint32_t addr, uint32_t *reg)   { *reg = 1; }

// ---------------------------------------------------------------------------
// 32-bit ARAM registers

static void __fastcall ar_write_maddr(uint32_t addr, uint32_t data)   { aram.mmaddr = data; }
static void __fastcall ar_read_maddr(uint32_t addr, uint32_t *reg)    { *reg = aram.mmaddr; }

static void __fastcall ar_write_araddr(uint32_t addr, uint32_t data)  { aram.araddr = data; }
static void __fastcall ar_read_araddr(uint32_t addr, uint32_t *reg)   { *reg = aram.araddr; }

static void __fastcall ar_write_cnt(uint32_t addr, uint32_t data)
{
    aram.cnt = data;
    aram.cntv[0] = aram.cntv[1] = TRUE;
    ARDMA(aram.cnt >> 31, aram.mmaddr, aram.araddr, aram.cnt & 0x7fffffff);
}
static void __fastcall ar_read_cnt(uint32_t addr, uint32_t *reg)      { *reg = aram.cnt; }

// ---------------------------------------------------------------------------
// init

void AROpen()
{
    DBReport2(DbgChannel::AR, "Aux. memory (ARAM) driver\n");

    // reallocate ARAM
    ARAM = (uint8_t *)malloc(ARAMSIZE);
    assert(ARAM);

    // clear ARAM data
    memset(ARAM, 0, ARAMSIZE);

    // invalidate CNT regs double-buffer
    aram.cntv[0] = aram.cntv[1] = FALSE;

    // clear registers
    aram.mmaddr = aram.araddr = aram.cnt = 0;

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
}

void ARClose()
{
    // destroy ARAM
    if(ARAM)
    {
        free(ARAM);
        ARAM = NULL;
    }
}
