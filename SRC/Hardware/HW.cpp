// hardware init/update code
// IMPORTANT: whole HW should use physical CPU addressing, not effective!
#include "pch.h"

DSP::DspCore* dspCore;      // instance of dsp core

// ---------------------------------------------------------------------------
// init and update

void HWOpen(HWConfig* config)
{
    DBReport2(DbgChannel::Info,
        "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n"
        "Hardware Initialization.\n"
        "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\n"
    );

    MIOpen(config); // memory protection and 1T-SRAM interface
    VIOpen(config); // video (TV)
    CPOpen(config); // fifo
    AIOpen(config); // audio (AID and AIS)
    AROpen();       // aux. memory (ARAM)
    EIOpen(config); // expansion interface (EXI)
    DIOpen();       // disk
    SIOpen();       // GC controllers
    PIOpen(config); // interrupts, console regs

    dspCore = new DSP::DspCore(config);
    assert(dspCore);

    DBReport("\n");
}

void HWClose()
{
    if (dspCore)
    {
        delete dspCore;
        dspCore = nullptr;
    }

    ARClose();      // release ARAM
    EIClose();      // take care about closing of memcards and BBA
    VIClose();      // close GDI (if opened)
    DIClose();      // release streaming buffer
    MIClose();
}

// update hardware counters/streams/any time-related tasks;
// we are using OS time (TBR), as counter basis.
void HWUpdate()
{
    // check for pending interrupts
    if (PICheckInterrupts())
        return;

    // update joypads and video
    VIUpdate();     // PADs are updated there (SIPoll)
    if (PICheckInterrupts())
        return;
    CPUpdate();     // GX fifo
    if (PICheckInterrupts())
        return;

    // update audio and DSP
    BeginProfileSfx();
    AIUpdate();
    EndProfileSfx();
    if (PICheckInterrupts())
        return;
}

namespace Flipper
{
    Flipper::Flipper(HWConfig* config)
    {
        HWOpen(config);
        GXOpen(mi.ram, wnd.hMainWindow);
        AXOpen();
        PADOpen();

        Debug::Hub.AddNode(HW_JDI_JSON, hw_init_handlers);
    }

    Flipper::~Flipper()
    {
        PADClose();
        AXClose();
        GXClose();
        HWClose();
    }
}
