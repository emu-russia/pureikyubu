#pragma once

// AI registers (AID regs are 16-bit, AIS regs are 32-bit)
#define DSP_OUTMBOXH        0x0C005000      // CPU->DSP mailbox
#define DSP_OUTMBOXL        0x0C005002
#define DSP_INMBOXH         0x0C005004      // DSP->CPU mailbox
#define DSP_INMBOXL         0x0C005006
#define AI_DCR              0x0C00500A      // AI/DSP control register
#define AID_MADRH           0x0C005030      // DMA start address (High)
#define AID_MADRL           0x0C005032      // DMA start address (Low)
#define AID_LEN             0x0C005036      // DMA control/DMA length (length of audio data in 32 Byte blocks)
#define AID_CNT             0x0C00503A      // counts down to zero showing how many 32 Byte blocks are left
#define AIS_CR              0x0C006C00      // AIS control register 
#define AIS_VR              0x0C006C04      // AIS volume register
#define AIS_SCNT            0x0C006C08      // AIS sample counter
#define AIS_IT              0x0C006C0C      // AIS interrupt timing

// AI/DSP Control Register mask
#define AIDCR_RESETMOD      (1 << 11)       // 1: DSP Reset from 0x8000, 0: DSP Reset from 0x0000 (__OSInitAudioSystem)
#define AIDCR_DSPDMA        (1 << 10)       // DSP dma in progress
#define AIDCR_ARDMA         (1 << 9)        // ARAM dma in progress
#define AIDCR_DSPINTMSK     (1 << 8)        // DSP->CPU interrupt mask (ReadWrite)
#define AIDCR_DSPINT        (1 << 7)        // DSP->CPU interrupt status (ReadWrite-Clear)
#define AIDCR_ARINTMSK      (1 << 6)        // ARAM DMA interrupt mask (RW)
#define AIDCR_ARINT         (1 << 5)        // ARAM DMA interrupt status (RWC)
#define AIDCR_AIINTMSK      (1 << 4)        // AI DMA interrupt mask (RW)
#define AIDCR_AIINT         (1 << 3)        // AI DMA interrupt status (RWC)
#define AIDCR_HALT          (1 << 2)        // halt DSP (stop ucoding)
#define AIDCR_DINT          (1 << 1)        // CPU->DSP interrupt
#define AIDCR_RES           (1 << 0)        // reset DSP (waits for 0)

// enable bit in AIDLEN register
#define AID_EN              (1 << 15)

// Audio Interface Control Register mask
#define AICR_DFR            (1 << 6)        // AID sample rate (HW2 only). 0 - 48000, 1 - 32000
#define AICR_SCRESET        (1 << 5)        // reset sample counter
#define AICR_AIINTVLD       (1 << 4)        // This bit controls whether AIINT is affected by the AIIT register matching (0 - match affects, 1 - not)
#define AICR_AIINT          (1 << 3)        // AIS interrupt status
#define AICR_AIINTMSK       (1 << 2)        // AIS interrupt mask
#define AICR_AFR            (1 << 1)        // AIS sample rate. 0 - 32000, 1 - 48000
#define AICR_PSTAT          (1 << 0)        // This bit enables the DDU AISLR clock

#define AIDCR               ai.dcr

// ---------------------------------------------------------------------------
// hardware API

// AI state (registers and other data)
struct AIControl
{
    // AID
    std::atomic<uint16_t> dcr;  // AI/DSP control register
    volatile uint16_t    madr_hi;        // DMA start address hi
    volatile uint16_t    madr_lo;        // DMA start address lo
    volatile uint16_t    len;            // DMA control/DMA length (length of audio data)
    volatile uint16_t    dcnt;           // DMA count-down

    // AIS
    volatile uint32_t    cr;             // AIS control reg
    volatile uint32_t    vr;             // AIS volume
    volatile uint32_t    scnt;           // sample counter
    volatile uint32_t    it;             // sample counter trigger

    // helpers
    uint32_t    currentDmaAddr; // current DMA address
    int32_t     dmaRate;        // copy of DFR value (32000/48000)
    uint64_t    dmaTime;        // audio DMA update time 

    Thread*     audioThread;    // The main AI thread that receives samples from AI DMA FIFO and DVD Audio (which accumulate in AIS FIFO).
                                // When FIFOs overflow - AudioThread Feed Mixer.

    uint8_t     streamFifo[32];
    size_t      streamFifoPtr;

    int64_t     one_second;     // one CPU second in timer ticks
    bool        log;            // Enable AI log

    uint8_t     zeroes[32];
};

extern  AIControl ai;

void    AIOpen(HWConfig * config);
void    AIClose();

// Used by DspCore

void    DSPAssertInt();
bool    DSPGetInterruptStatus();
bool    DSPGetResetModifier();
