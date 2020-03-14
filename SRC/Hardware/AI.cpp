// AI - audio interface
#include "pch.h"

// all AI timers update is based on TBR.

/*/
    AI Interrupts
    -------------

    The Audio Interface API is responsible for managing two interrupts to the
    host CPU: 
        The streamed sample counter interrupt (AIINT). 
        The AI DMA interrupt (AIDINT). 
    Note that the streamed sample counter interrupt is generated directly by
    the AI hardware. The AI DMA interrupt, however, comes from the memory
    controller of the audio subsystem.

    Audio streaming sample counter interrupt (AIINT)
    The Audio Interface provides facilities for counting the number of streamed
    (left/right) samples played and asserting an interrupt at some programmable
    trigger value. Note that only audio samples streamed from the optical disc
    are counted. Note also that samples are counted after the sample rate
    conversion stage-thus, the stream will always be at a 48KHz sample rate.

    AI DMA interrupt (AIDINT)
    The Audio Interface API provides control over the AI DMA. The actual DMA
    controller resides within the audio subsystem of the Graphics Processor ASIC.
    The AI DMA feeds data from main memory to the AI FIFO, which is 32 bytes in
    length (the size of a single DMA block). The AI FIFO consumes data at a rate
    of either 48,000 or 32,000 stereo samples per second. The sample rate of the
    AI FIFO DMA may be controlled through AISetDSPSampleRate().
/*/

// AI state (registers and other data)
AIControl ai;

// ---------------------------------------------------------------------------
// AIDCR

static void __fastcall write_aidcr(uint32_t addr, uint32_t data)
{
    AIDCR = (uint16_t)data;

    // clear pending interrupts
    if(AIDCR & AIDCR_DSPINT)
    {
        AIDCR &= ~AIDCR_DSPINT;
        PIClearInt(PI_INTERRUPT_DSP);
    }
    if(AIDCR & AIDCR_ARINT)
    {
        AIDCR &= ~AIDCR_ARINT;
        PIClearInt(PI_INTERRUPT_DSP);
    }
    if(AIDCR & AIDCR_AIINT)
    {
        AIDCR &= ~AIDCR_AIINT;
        PIClearInt(PI_INTERRUPT_DSP);
    }

    // ARAM hack (DMA always ready)
    AIDCR &= ~AIDCR_ARDMA;

    // DSP controls
    dspCore->DSPSetResetBit((AIDCR >> 0) & 1);
    dspCore->DSPSetIntBit  ((AIDCR >> 1) & 1);
    dspCore->DSPSetHaltBit ((AIDCR >> 2) & 1);
}

static void __fastcall read_aidcr(uint32_t addr, uint32_t *reg)
{
    // DSP controls
    AIDCR &= ~7;
    AIDCR |= dspCore->DSPGetResetBit() << 0;
    AIDCR |= dspCore->DSPGetIntBit()   << 1;
    AIDCR |= dspCore->DSPGetHaltBit()  << 2;

    *reg = AIDCR;
}

// ---------------------------------------------------------------------------
// DMA

// dma transfer complete (when AIDCNT == 0)
void AIDINT()
{
    AIDCR |= AIDCR_AIINT;
    if(AIDCR & AIDCR_AIINTMSK)
    {
        PIAssertInt(PI_INTERRUPT_DSP);
        DBReport(AI "AIDINT");
    }
}

// how much time AI DMA need to playback "n" bytes.
static int64_t AIGetTime(long dmaBytes, long rate)
{
    long samples = dmaBytes / 4;    // left+right, 16-bit
    return samples * (ai.one_second / rate);
}

static void AIStartDMA(uint32_t addr, long bytes)
{
    addr &= RAMMASK;
    AXPlayAudio(&mi.ram[addr], bytes);
    DBReport(AI "DMA started: %08X, %i bytes\n", addr | (1 << 31), bytes);
}

static void AIStopDMA()
{
    AXPlayAudio(0, 0);
    DBReport(AI "DMA stopped\n");
}

static void AISetDMASampleRate(long rate)
{
    AXSetRate(ai.dmaRate = rate);
    DBReport(AI "DMA sample rate : %i\n", ai.dmaRate);
}

//
// dma buffer address
// read and writes are from shadow regs
// transfer starts only, if shadows are valid
// 

static void __fastcall write_dmah(uint32_t addr, uint32_t data)
{
    ai.madr.valid[0] = true;
    ai.madr.shadow.hi = (uint16_t)data;

    // setup buffer
    if(ai.madr.valid[0] && ai.madr.valid[1])
    {
        ai.madr.valid[0] = ai.madr.valid[1] = false;
        ai.lastDma = (ai.madr.shadow.hi << 16) | ai.madr.shadow.lo;
    }
}

static void __fastcall write_dmal(uint32_t addr, uint32_t data)
{
    ai.madr.valid[1] = true;
    ai.madr.shadow.lo = (uint16_t)data;

    // setup buffer
    if(ai.madr.valid[0] && ai.madr.valid[1])
    {
        ai.madr.valid[0] = ai.madr.valid[1] = false;
        ai.lastDma = (ai.madr.shadow.hi << 16) | ai.madr.shadow.lo;
    }
}

static void __fastcall read_dmah(uint32_t addr, uint32_t *reg) { *reg = ai.madr.shadow.hi; }
static void __fastcall read_dmal(uint32_t addr, uint32_t *reg) { *reg = ai.madr.shadow.lo; }

//
// dma length / control
//

static void __fastcall write_len(uint32_t addr, uint32_t data)
{
    ai.len = (uint16_t)data;

    // begin audio dma transfer
    if(ai.len & AID_EN)
    {
        ai.dcnt = ai.len & ~AID_EN;
        AIStartDMA(ai.lastDma, ai.dcnt * 32);
    }
    else AIStopDMA();
}
static void __fastcall read_len(uint32_t addr, uint32_t *reg) { *reg = ai.len; }

//
// read sample block (32b) counter
//

static void __fastcall read_dcnt(uint32_t addr, uint32_t *reg)
{
    *reg = ai.dcnt--;
    if(ai.dcnt & 0x8000) ai.dcnt = 0;
}

// ---------------------------------------------------------------------------
// streaming

// streaming trigger and counter coincidence
void AISINT()
{
    // only if AIINT is validated
    if(ai.cr & AICR_AIINTVLD)
    {
        ai.cr |= AICR_AIINT;
        if(ai.cr & AICR_AIINTMSK)
        {
            PIAssertInt(PI_INTERRUPT_AI);
            DBReport(AIS "AISINT\n");
        }
    }
}

// AI control register
static void __fastcall write_cr(uint32_t addr, uint32_t data)
{
    ai.cr = data;

    // clear stream interrupt
    if(ai.cr & AICR_AIINT)
    {
        ai.cr &= ~AICR_AIINT;
        PIClearInt(PI_INTERRUPT_AI);
    }

    // enable sample counter
    if(ai.cr & AICR_PSTAT)
    {
        DBReport(AIS "start streaming clock\n");
    }
    else DBReport(AIS "stop streaming clock\n");

    // reset sample counter
    if(ai.cr & AICR_SCRESET)
    {
        DBReport(AIS "reset sample counter\n");
        ai.scnt = 0;
        ai.cr &= ~AICR_SCRESET;
    }

    // set DMA sample rate
    if(ai.cr & AICR_DFR) AISetDMASampleRate(48000);
    else AISetDMASampleRate(32000);
}
static void __fastcall read_cr(uint32_t addr, uint32_t *reg)     { *reg = ai.cr; }

// stream samples counter
static void __fastcall read_scnt(uint32_t addr, uint32_t *reg)
{
    *reg = ai.scnt;
}
static void __fastcall write_dummy(uint32_t addr, uint32_t data) {}

// interrupt trigger
static void __fastcall write_it(uint32_t addr, uint32_t data)
{
    DBReport(AIS "set trigger to : 0x%08X\n", data);
    ai.it = data;
}
static void __fastcall read_it(uint32_t addr, uint32_t *reg)     { *reg = ai.it; }

// stream volume register
static void __fastcall write_vr(uint32_t addr, uint32_t data)
{
    ai.vr = (uint16_t)data;
    AXSetVolume((uint8_t)ai.vr, (uint8_t)(ai.vr >> 8));
}
static void __fastcall read_vr(uint32_t addr, uint32_t *reg)     { *reg = ai.vr; }

// ---------------------------------------------------------------------------
// DSP mailbox controls

static void __fastcall write_out_mbox_h(uint32_t addr, uint32_t data) { dspCore->CpuToDspWriteHi((uint16_t)data); }
static void __fastcall write_out_mbox_l(uint32_t addr, uint32_t data) { dspCore->CpuToDspWriteLo((uint16_t)data); }
static void __fastcall read_out_mbox_h(uint32_t addr, uint32_t* reg) { *reg = dspCore->CpuToDspReadHi(); }
static void __fastcall read_out_mbox_l(uint32_t addr, uint32_t* reg)
{
    DolwinError(__FILE__, "Processor is not allowed to read own Lower Mailbox!");
    *reg = 0;
}

static void __fastcall read_in_mbox_h(uint32_t addr, uint32_t* reg) { *reg = dspCore->DspToCpuReadHi(); }
static void __fastcall read_in_mbox_l(uint32_t addr, uint32_t* reg) { *reg = dspCore->DspToCpuReadLo(); }

static void __fastcall write_in_mbox_h(uint32_t addr, uint32_t data) { DolwinError(__FILE__, "Processor is not allowed to write DSP Mailbox!"); }
static void __fastcall write_in_mbox_l(uint32_t addr, uint32_t data) { DolwinError(__FILE__, "Processor is not allowed to write DSP Mailbox!"); }

// ---------------------------------------------------------------------------

void AIUpdate()
{
    // *** update audio DMA ***
    if((TBR >= ai.dmaTime) && (ai.len & AID_EN))
    {
        AIDINT();
        ai.dcnt = ai.len & ~AID_EN;
        ai.dmaTime = TBR + AIGetTime(ai.dcnt * 32, ai.dmaRate);
        AIStartDMA(ai.lastDma, ai.dcnt * 32);
    }

    // *** update stream sample counter ***
    if(ai.cr & AICR_PSTAT)
    {
        if(!di.streaming) ai.scnt++;
        if(ai.scnt >= ai.it)
        {
            AISINT();
        }
    }
}

void AIOpen(HWConfig* config)
{
    DBReport(CYAN "AI: Audio interface (DMA and DSP)\n");

    // clear regs
    memset(&ai, 0, sizeof(AIControl));
    ai.dmaRate = 32000;     // was division by 0 in AIGetTime
    
    ai.one_second = config->one_second;

    // set register traps
    MISetTrap(16, AI_DCR, read_aidcr, write_aidcr);

    MISetTrap(16, DSP_OUTMBOXH, read_out_mbox_h, write_out_mbox_h);
    MISetTrap(16, DSP_OUTMBOXL, read_out_mbox_l, write_out_mbox_l);
    MISetTrap(16, DSP_INMBOXH , read_in_mbox_h , write_in_mbox_h);
    MISetTrap(16, DSP_INMBOXL , read_in_mbox_l , write_in_mbox_l);

    MISetTrap(16, AID_MADRH , read_dmah, write_dmah);
    MISetTrap(16, AID_MADRL , read_dmal, write_dmal);
    MISetTrap(16, AID_LEN   , read_len , write_len);
    MISetTrap(16, AID_CNT   , read_dcnt, NULL);

    MISetTrap(32, AIS_CR  , read_cr  , write_cr);
    MISetTrap(32, AIS_VR  , read_vr  , write_vr);
    MISetTrap(32, AIS_SCNT, read_scnt, NULL);
    MISetTrap(32, AIS_IT  , read_it  , write_it);
}

void DSPAssertInt()
{
    AIDCR |= AIDCR_DSPINT;
    if (AIDCR & AIDCR_DSPINTMSK)
    {
        PIAssertInt(PI_INTERRUPT_DSP);
    }
}
