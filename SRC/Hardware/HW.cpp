// hardware init/update code
// IMPORTANT: whole HW should use physical CPU addressing, not effective!
#include "pch.h"

DSP::DspCore* DspCore;      // instance of dsp core

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

    DspCore = new DSP::DspCore(config);
    assert(DspCore);

    DBReport("\n");
}

void HWClose()
{
    if (DspCore)
    {
        delete DspCore;
        DspCore = nullptr;
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
    PICheckInterrupts();

    // update joypads and video
    VIUpdate();     // PADs are updated there (SIPoll)
    CPUpdate();     // GX fifo

    // update audio and DSP
    BeginProfileSfx();
    AIUpdate();
    //dspCore->Update();        // Updated by own thread
    EndProfileSfx();

    //DBReport(YEL "*** HW UPDATE *** (%s)\n", OSTimeFormat(UTBR, 1));
}

namespace Flipper
{
    Flipper::Flipper(HWConfig* config)
    {
        HWOpen(config);
        assert(GXOpen(mi.ram, wnd.hMainWindow));
        assert(AXOpen());
        assert(PADOpen());
    }

    Flipper::~Flipper()
    {
        PADClose();
        AXClose();
        GXClose();
        HWClose();
    }
}
