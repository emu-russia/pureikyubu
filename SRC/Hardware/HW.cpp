// hardware init/update code
// IMPORTANT: whole HW should use physical CPU addressing, not effective!
#include "pch.h"

static bool hw_assert;      // assert on not implemented HW in non DEBUG
static bool update;         // 1: HW update enabled

// ---------------------------------------------------------------------------
// init and update

void HWOpen(HWConfig* config)
{
    DBReport(
        GREEN "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n"
        GREEN "Hardware Initialization.\n"
        GREEN "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\n"
    );

    MIOpen(config); // memory protection and 1T-SRAM interface
    VIOpen(config); // video (TV)
    CPOpen(config); // fifo
    AIOpen(config); // audio (AID and AIS)
    DSPOpen(config);    // DSP
    AROpen();       // aux. memory (ARAM)
    EIOpen(config); // expansion interface (EXI)
    DIOpen();       // disk
    SIOpen();       // GC controllers
    PIOpen(config); // interrupts, console regs

    HWEnableUpdate(1);

    DBReport("\n");
}

void HWClose()
{
    ARClose();      // release ARAM
    EIClose();      // take care about closing of memcards and BBA
    VIClose();      // close GDI (if opened)
    DIClose();      // release streaming buffer
    DSPClose();
    MIClose();
}

// update hardware counters/streams/any time-related tasks;
// we are using OS time (TBR), as counter basis.
void HWUpdate()
{
    if(update)
    {
        // check for pending interrupts
        PICheckInterrupts();

        // update joypads and video
        VIUpdate();     // PADs are updated there (SIPoll)
        CPUpdate();     // GX fifo

        // update audio and DSP
        BeginProfileSfx();
        AIUpdate();
        DSPUpdate();
        EndProfileSfx();

        //DBReport(YEL "*** HW UPDATE *** (%s)\n", OSTimeFormat(UTBR, 1));
    }
}

// allow/disallow HW update
void HWEnableUpdate(bool en)
{
    update = en;
}
