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
    }
    if(AIDCR & AIDCR_ARINT)
    {
        AIDCR &= ~AIDCR_ARINT;
    }
    if(AIDCR & AIDCR_AIINT)
    {
        AIDCR &= ~AIDCR_AIINT;
    }

    if ((AIDCR & AIDCR_DSPINT) == 0 && (AIDCR & AIDCR_ARINT) == 0 && (AIDCR & AIDCR_AIINT) == 0)
    {
        PIClearInt(PI_INTERRUPT_DSP);
    }

    // ARAM/DSP DMA always ready
    AIDCR &= ~(AIDCR_ARDMA | AIDCR_DSPDMA);

    // DSP controls
    Flipper::HW->DSP->DSPSetResetBit((AIDCR >> 0) & 1);
    Flipper::HW->DSP->DSPSetIntBit  ((AIDCR >> 1) & 1);
    Flipper::HW->DSP->DSPSetHaltBit ((AIDCR >> 2) & 1);
}

static void __fastcall read_aidcr(uint32_t addr, uint32_t *reg)
{
    // DSP controls
    AIDCR &= ~7;
    AIDCR |= Flipper::HW->DSP->DSPGetResetBit() << 0;
    AIDCR |= Flipper::HW->DSP->DSPGetIntBit()   << 1;
    AIDCR |= Flipper::HW->DSP->DSPGetHaltBit()  << 2;

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
        if (ai.log)
        {
            DBReport2(DbgChannel::AI, "AIDINT");
        }
    }
}

// how much time AI DMA need to playback "n" bytes in Gekko ticks
static int64_t AIGetTime(size_t dmaBytes, long rate)
{
    size_t samples = dmaBytes / 4;    // left+right, 16-bit
    return samples * (ai.one_second / rate);
}

static void AIStartDMA()
{
    ai.dcnt = ai.len & ~AID_EN;
    ai.dmaTime = Gekko::Gekko->GetTicks() + AIGetTime(32, ai.dmaRate);
    if (ai.log)
    {
        DBReport2(DbgChannel::AI, "DMA started: %08X, %i bytes\n", ai.currentDmaAddr | (1 << 31), ai.dcnt * 32);
    }
    ai.audioThread->Resume();
}

// Simulate AI FIFO
static void AIFeedMixer()
{
    if (ai.dcnt == 0 || (ai.len & AID_EN) == 0)
        return;

    int bytes = 32;

    BeginProfileSfx();
    Flipper::HW->Mixer->PushBytes(Flipper::AxChannel::AudioDma, &mi.ram[ai.currentDmaAddr & RAMMASK], bytes);
    EndProfileSfx();

    ai.dmaTime = Gekko::Gekko->GetTicks() + AIGetTime(bytes, ai.dmaRate);

    ai.currentDmaAddr += bytes;
    ai.dcnt--;
}

static void AIStopDMA()
{
    ai.dmaTime = -1;
    ai.dcnt = 0;
    if (ai.log)
    {
        DBReport2(DbgChannel::AI, "DMA stopped\n");
    }
}

static void AISetDMASampleRate(Flipper::AudioSampleRate rate)
{
    Flipper::HW->Mixer->SetSampleRate(Flipper::AxChannel::AudioDma, rate);
    if (ai.log)
    {
        DBReport2(DbgChannel::AI, "DMA sample rate: %i\n", rate == Flipper::AudioSampleRate::Rate_32000 ? 32000 : 48000);
    }
}

static void AISetDvdAudioSampleRate(Flipper::AudioSampleRate rate)
{
    Flipper::HW->Mixer->SetSampleRate(Flipper::AxChannel::DvdAudio, rate);

    if (rate == Flipper::AudioSampleRate::Rate_48000)
    {
        DVD::DDU->SetDvdAudioSampleRate(DVD::DvdAudioSampleRate::Rate_48000);
    }
    else
    {
        DVD::DDU->SetDvdAudioSampleRate(DVD::DvdAudioSampleRate::Rate_32000);
    }

    if (ai.log)
    {
        DBReport2(DbgChannel::AIS, "DVD Audio sample rate: %i\n", rate == Flipper::AudioSampleRate::Rate_32000 ? 32000 : 48000);
    }
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
        ai.currentDmaAddr = (ai.madr.shadow.hi << 16) | ai.madr.shadow.lo;
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
        ai.currentDmaAddr = (ai.madr.shadow.hi << 16) | ai.madr.shadow.lo;
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

    // start/stop audio dma transfer
    if(ai.len & AID_EN)
    {
        AIStartDMA();
        Flipper::HW->Mixer->Enable(Flipper::AxChannel::AudioDma, true);
    }
    else
    {
        AIStopDMA();
        Flipper::HW->Mixer->Enable(Flipper::AxChannel::AudioDma, false);
    }
}
static void __fastcall read_len(uint32_t addr, uint32_t *reg)
{
    *reg = ai.len;
}

//
// read sample block (32b) counter
//

static void __fastcall read_dcnt(uint32_t addr, uint32_t *reg)
{
    *reg = ai.dcnt;
}

// ---------------------------------------------------------------------------
// streaming

// streaming trigger and counter coincidence
void AISINT()
{
    // only if AIINT is validated
    if((ai.cr & AICR_AIINTVLD) == 0)
    {
        ai.cr |= AICR_AIINT;
        if(ai.cr & AICR_AIINTMSK)
        {
            PIAssertInt(PI_INTERRUPT_AI);
            if (ai.log)
            {
                DBReport2(DbgChannel::AIS, "AISINT\n");
            }
        }
    }
}

// AI control register
static void __fastcall write_cr(uint32_t addr, uint32_t data)
{
    ai.cr = data & 0x7F;

    // clear stream interrupt
    if(ai.cr & AICR_AIINT)
    {
        ai.cr &= ~AICR_AIINT;
        PIClearInt(PI_INTERRUPT_AI);
    }

    // enable sample counter
    if (ai.cr & AICR_PSTAT)
    {
        if (ai.log)
        {
            DBReport2(DbgChannel::AIS, "start streaming clock\n");
        }
        DVD::DDU->EnableAudioStreamClock(true);
        Flipper::HW->Mixer->Enable(Flipper::AxChannel::DvdAudio, true);
        ai.streamFifoPtr = 0;
    }
    else
    {
        if (ai.log)
        {
            DBReport2(DbgChannel::AIS, "stop streaming clock\n");
        }
        DVD::DDU->EnableAudioStreamClock(false);
        Flipper::HW->Mixer->Enable(Flipper::AxChannel::DvdAudio, false);
    }

    // reset sample counter
    if(ai.cr & AICR_SCRESET)
    {
        if (ai.log)
        {
            DBReport2(DbgChannel::AIS, "reset sample counter\n");
        }
        ai.scnt = 0;
        ai.cr &= ~AICR_SCRESET;
    }

    // set DMA sample rate
    if (ai.cr & AICR_DFR)
    {
        ai.dmaRate = 32000;
        AISetDMASampleRate(Flipper::AudioSampleRate::Rate_32000);
    }
    else
    {
        ai.dmaRate = 48000;
        AISetDMASampleRate(Flipper::AudioSampleRate::Rate_48000);
    }

    // set DVD Audio sample rate
    if (ai.cr & AICR_AFR) AISetDvdAudioSampleRate(Flipper::AudioSampleRate::Rate_48000);
    else AISetDvdAudioSampleRate(Flipper::AudioSampleRate::Rate_32000);
}
static void __fastcall read_cr(uint32_t addr, uint32_t *reg)
{
    *reg = ai.cr;
}

// stream samples counter
static void __fastcall read_scnt(uint32_t addr, uint32_t *reg)
{
    *reg = ai.scnt;
}

// interrupt trigger
static void __fastcall write_it(uint32_t addr, uint32_t data)
{
    if (ai.log)
    {
        DBReport2(DbgChannel::AIS, "set trigger to : 0x%08X\n", data);
    }
    ai.it = data;
}
static void __fastcall read_it(uint32_t addr, uint32_t *reg)     { *reg = ai.it; }

// stream volume register
static void __fastcall write_vr(uint32_t addr, uint32_t data)
{
    ai.vr = (uint16_t)data;
}
static void __fastcall read_vr(uint32_t addr, uint32_t *reg)
{
    *reg = ai.vr;
}

// ---------------------------------------------------------------------------
// DSPCore interface (mailbox and interrupt)

static void __fastcall write_out_mbox_h(uint32_t addr, uint32_t data) { Flipper::HW->DSP->CpuToDspWriteHi((uint16_t)data); }
static void __fastcall write_out_mbox_l(uint32_t addr, uint32_t data) { Flipper::HW->DSP->CpuToDspWriteLo((uint16_t)data); }
static void __fastcall read_out_mbox_h(uint32_t addr, uint32_t* reg) { *reg = Flipper::HW->DSP->CpuToDspReadHi(false); }
static void __fastcall read_out_mbox_l(uint32_t addr, uint32_t* reg) { *reg = Flipper::HW->DSP->CpuToDspReadLo(false); }

static void __fastcall read_in_mbox_h(uint32_t addr, uint32_t* reg) { *reg = Flipper::HW->DSP->DspToCpuReadHi(false); }
static void __fastcall read_in_mbox_l(uint32_t addr, uint32_t* reg) { *reg = Flipper::HW->DSP->DspToCpuReadLo(false); }

static void __fastcall write_in_mbox_h(uint32_t addr, uint32_t data) { DBHalt("Processor is not allowed to write DSP Mailbox!"); }
static void __fastcall write_in_mbox_l(uint32_t addr, uint32_t data) { DBHalt("Processor is not allowed to write DSP Mailbox!"); }

void DSPAssertInt()
{
    if (ai.log)
    {
        DBReport2(DbgChannel::AI, "DSPAssertInt\n");
    }

    AIDCR |= AIDCR_DSPINT;
    if (AIDCR & AIDCR_DSPINTMSK)
    {
        PIAssertInt(PI_INTERRUPT_DSP);
    }
}

bool DSPGetInterruptStatus()
{
    return (AIDCR & AIDCR_DSPINT) != 0;
}

bool DSPGetResetModifier()
{
    return (AIDCR & AIDCR_RESETMOD) != 0;
}

// ---------------------------------------------------------------------------

// AI DMA and DVD Audio are played uncompetitively from different streams.
// All work on Sample Rate Conversion and sound mixing for convenience is done in Mixer (AX).

static uint16_t AdjustVolume(uint16_t sampleValue, int volume)
{
    // Let's try how this conversion will behave on a float, if it slows down, then translate it to ints.
    // In theory, on modern processors, float is fast.
    float value = (float)sampleValue / (float)0xFFFF;
    float volumeF = (float)volume / (float)0xFF;
    float adjusted = value * volumeF;
    return (uint16_t)(adjusted * (float)0xFFFF);
}

// Called from DDU Core when DVD Audio decodes the next sample
static void AIStreamCallback(uint16_t l, uint16_t r)
{
    // Check FIFO overflow
    if (ai.streamFifoPtr >= sizeof(ai.streamFifo))
    {
        ai.streamFifoPtr = 0;
        // Feed mixer
        Flipper::HW->Mixer->PushBytes(Flipper::AxChannel::DvdAudio, ai.streamFifo, sizeof(ai.streamFifo));
    }

    // Adjust volume and swap endianess
    int leftVolume = (uint8_t)ai.vr;
    int rightVolume = (uint8_t)(ai.vr >> 8);
    l = _byteswap_ushort(l);
    r = _byteswap_ushort(r);
    //l = AdjustVolume(l, leftVolume);
    //r = AdjustVolume(r, rightVolume);

    // Put sample in FIFO
    uint16_t* ptr = (uint16_t *)&ai.streamFifo[ai.streamFifoPtr];

    ptr[0] = l;
    ptr[1] = r;

    ai.streamFifoPtr += 4;

    // update stream sample counter
    if (ai.cr & AICR_PSTAT)
    {
        ai.scnt++;
        if (ai.scnt >= ai.it)
        {
            AISINT();
        }
    }
}

// Update audio DMA thread
static void AIUpdate(void *Parameter)
{
    while (true)
    {
        if ((uint64_t)Gekko::Gekko->GetTicks() >= ai.dmaTime)
        {
            if (ai.dcnt == 0)
            {
                if (ai.len & AID_EN)
                {
                    // Restart Dma and signal AID_INT
                    ai.currentDmaAddr = (ai.madr.shadow.hi << 16) | ai.madr.shadow.lo;
                    ai.dcnt = ai.len & ~AID_EN;
                    AIDINT();
                }
                else
                {
                    ai.audioThread->Suspend();
                }
            }
            else
            {
                if (ai.len & AID_EN)
                {
                    AIFeedMixer();
                }
                else
                {
                    ai.audioThread->Suspend();
                }
            }
        }
    }
}

void AIOpen(HWConfig* config)
{
    DBReport2(DbgChannel::AI, "Audio interface (DMA, DVD Streaming and DSP)\n");

    // clear regs
    memset(&ai, 0, sizeof(AIControl));
    
    DVD::DDU->SetStreamCallback(AIStreamCallback);

    ai.audioThread = new Thread(AIUpdate, true, nullptr, "AI");
    assert(ai.audioThread);

    ai.one_second = Gekko::Gekko->OneSecond();
    ai.dmaRate = ai.cr & AICR_DFR ? 32000 : 48000;
    ai.dmaTime = Gekko::Gekko->GetTicks() + AIGetTime(32, ai.dmaRate);
    ai.log = false;
    AIStopDMA();

    // set register traps
    MISetTrap(16, AI_DCR, read_aidcr, write_aidcr);

    MISetTrap(16, DSP_OUTMBOXH, read_out_mbox_h, write_out_mbox_h);
    MISetTrap(16, DSP_OUTMBOXL, read_out_mbox_l, write_out_mbox_l);
    MISetTrap(16, DSP_INMBOXH , read_in_mbox_h , write_in_mbox_h);
    MISetTrap(16, DSP_INMBOXL , read_in_mbox_l , write_in_mbox_l);

    MISetTrap(16, AID_MADRH , read_dmah, write_dmah);
    MISetTrap(16, AID_MADRL , read_dmal, write_dmal);
    MISetTrap(16, AID_LEN   , read_len , write_len);
    MISetTrap(16, AID_CNT   , read_dcnt, nullptr);

    MISetTrap(32, AIS_CR  , read_cr  , write_cr);
    MISetTrap(32, AIS_VR  , read_vr  , write_vr);
    MISetTrap(32, AIS_SCNT, read_scnt, nullptr);
    MISetTrap(32, AIS_IT  , read_it  , write_it);
}

void AIClose()
{
    AIStopDMA();
    delete ai.audioThread;
    ai.audioThread = nullptr;
    DVD::DDU->SetStreamCallback(nullptr);
}
