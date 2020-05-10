# OSInitAudioSystem

To execute the DSP initialization code ("DSP Stub"), you need to make MMEM->ARAM DMA 0x01000000 -> 0. Such special DMA will cause the DMA Stub to load into the IMEM at address 0.

Then you need to reset the ResetMod bit, and perform a DSP soft reset (set the Reset bit). In this case, the DSP will start to execute not from the address 0x8000, but from the address 0x0, where the DSP Stub is located.

See DSP Stub reversing in OSInitAudioSystem_stub.md.

```c++
void __OSInitAudioSystem()
{
    // copy DSP init ucode to 32B aligned memory area
    // save used region below of arenaHi
    memcpy(OSGetArenaHi() - 128, 0x81000000, sizeof(DSPInitCode));
    memcpy(0x81000000, DSPInitCode, sizeof(DSPInitCode));
    DCFlushRange(0x81000000, sizeof(DSPInitCode));

    AR_SIZE = 0x0043;
    AIDCR = AIDCR_RESETMOD | AIDCR_DSPINT | AIDCR_ARINT | AIDCR_AIINT | AIDCR_HALT;

    // reset DSP and wait
    AIDCR |= AIDCR_RES;
    while(AIDCR & AIDCR_RES);

    // Discard any outcoming CPU->DSP mailbox message
    DSP_OUTMBOXH = 0;

    // Discard any incoming DSP->CPU mailbox message    
    while(((DSP_INMBOXH << 16) | DSP_INMBOXL) & 0x80000000);

    // send DSP initialization ucode to ARAM at offset 0
    AR_DMA_MMADDR = OSCachedToPhysical(0x81000000);
    AR_DMA_ARADDR = 0;
    AR_DMA_CNT = 32;        // Actually 32 * 4 bytes

    // wait ARAM DMA interrupt
    u16 old = AIDCR;    // keep AIDCR_ARDMA
    while((AIDCR & AIDCR_ARINT) == 0);
    AIDCR = old;        // clear DMA status

    // wait a little
    OSTick old = OSGetTick();
    while((OSGetTick() - old) < 2194);

    // send DSP initialization ucode to ARAM at offset 0 (again)
    AR_DMA_MMADDR = OSCachedToPhysical(0x81000000);
    AR_DMA_ARADDR = 0;
    AR_DMA_CNT = 32;        // Actually 32 * 4 bytes

    // wait DMA complete
    u16 old = AIDCR;    // keep AIDCR_ARDMA
    while(AIDCR & AIDCR_ARDMA);
    AIDCR = old;        // clear DMA status

    // Unhalt and reset DSP in stub mode (vector: 0x0000)

    AIDCR &= ~AIDCR_RESETMOD;
    while(AIDCR & AIDCR_DSPDMA);
    AIDCR &= ~AIDCR_HALT;

    // Wait message from stub

    while((DSP_INMBOXH & 0x8000) == 0);
    u16 dummy = DSP_INMBOXL;    // read nowhere

#if _DEBUG
    // Check message 
#endif

    // Halt DSP and reset in normal mode (vector: 0x8000)

    AIDCR |= AIDCR_HALT;

    AIDCR = AIDCR_RESETMOD | AIDCR_DSPINT | AIDCR_ARINT | AIDCR_AIINT | AIDCR_HALT;

    // reset DSP and wait
    AIDCR |= AIDCR_RES;
    while(AIDCR & AIDCR_RES);    

    // restore saved region
    memcpy(0x81000000, OSGetArenaHi() - 128, sizeof(DSPInitCode));
}
```


```c++
// DSP mailbox registers
#define DSP_OUTMBOXH        *(volatile u16 *)(0xCC005000)
#define DSP_OUTMBOXL        *(volatile u16 *)(0xCC005002)
#define DSP_INMBOXH         *(volatile u16 *)(0xCC005004)
#define DSP_INMBOXL         *(volatile u16 *)(0xCC005006)                               

// known ARAM controller registers
#define AR_SIZE             (*(volatile u16 *)(0xCC005012))     // 16 bit regs
#define AR_MODE             (*(volatile u16 *)(0xCC005016))
#define AR_REFRESH          (*(volatile u16 *)(0xCC00501A))
#define AR_DMA_MMADDR_H     (*(volatile u16 *)(0xCC005020))
#define AR_DMA_MMADDR_L     (*(volatile u16 *)(0xCC005022))
#define AR_DMA_ARADDR_H     (*(volatile u16 *)(0xCC005024))
#define AR_DMA_ARADDR_L     (*(volatile u16 *)(0xCC005026))
#define AR_DMA_CNT_H        (*(volatile u16 *)(0xCC005028))
#define AR_DMA_CNT_L        (*(volatile u16 *)(0xCC00502A))

#define AR_DMA_MMADDR       (*(volatile u16 *)(0xCC005020))     // 32 bit regs
#define AR_DMA_ARADDR       (*(volatile u16 *)(0xCC005024))
#define AR_DMA_CNT          (*(volatile u16 *)(0xCC005028))

// DSP interface main control register
#define AIDCR               (*(volatile u16 *)(0xCC00500A))

// AIDCR bits (bits 10 and 11 are unknown)
#define AIDCR_RESETMOD      (1 << 11)       // reset vector modifier (0: 0x0000, 1: 0x8000)
#define AIDCR_DSPDMA        (1 << 10)       // dsp dma in progress, if set
#define AIDCR_ARDMA         (1 << 9)        // aram dma in progress, if set
#define AIDCR_DSPINTMSK     (1 << 8)        // dsp interrupt mask   (RW)
#define AIDCR_DSPINT        (1 << 7)        // dsp interrupt active (RWC)
#define AIDCR_ARINTMSK      (1 << 6)        // aram dma interrupt mask   (RW)
#define AIDCR_ARINT         (1 << 5)        // aram dma interrupt active (RWC)
#define AIDCR_AIINTMSK      (1 << 4)        // ai dma interrupt mask   (RW)
#define AIDCR_AIINT         (1 << 3)        // ai dma interrupt active (RWC)
#define AIDCR_HALT          (1 << 2)        // halt DSP
#define AIDCR_PIINT         (1 << 1)        // assert CPU->DSP interrupt. Cleared by DSP on acknowledge.
#define AIDCR_RES           (1 << 0)        // reset DSP. Cleared by DSP on acknowledge.

static  u16 DSPInitCode[0x40] = {       // 128 bytes
    0x029F, 0x0010, 0x029F, 0x0033, 0x029F, 0x0034, 0x029F, 0x0035,
    0x029F, 0x0036, 0x029F, 0x0037, 0x029F, 0x0038, 0x029F, 0x0039,
    0x1206, 0x1203, 0x1204, 0x1205, 0x0080, 0x8000, 0x0088, 0xFFFF,
    0x0084, 0x1000, 0x0064, 0x001D, 0x0218, 0x0000, 0x8100, 0x1C1E,
    0x0044, 0x1B1E, 0x0084, 0x0800, 0x0064, 0x0027, 0x191E, 0x0000,
    0x00DE, 0xFFFC, 0x02A0, 0x8000, 0x029C, 0x0028, 0x16FC, 0x0054,
    0x16FD, 0x4348, 0x0021, 0x02FF, 0x02FF, 0x02FF, 0x02FF, 0x02FF,
    0x02FF, 0x02FF, 0x02FF, 0x02FF, 0x0000, 0x0000, 0x0000, 0x0000,
};
```
