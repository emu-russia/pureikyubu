// hardware init/update code
// IMPORTANT: whole HW should use physical CPU addressing, not effective!
#include "pch.h"

DSP::DspCore* dspCore;      // instance of dsp core

namespace Flipper
{
    Flipper* HW;

    // This thread acts as the HWUpdate of Dolwin 0.10.
    // Previously, an HWUpdate call occurred after each Gekko instruction (or so).
    // This was tied only to update VI, SI and AI.
    // After switching to multitasking, leave the old HWUpdate update mechanism, will be gradually replaced by other threads of different different Flipper components.
    void Flipper::HwUpdateThread(void* Parameter)
    {
        Flipper* flipper = (Flipper*)Parameter;

        // TODO: Add update queue
        while (true)
        {
            int64_t ticks = Gekko::Gekko->GetTicks();
            if (ticks >= flipper->hwUpdateTbrValue)
            {
                flipper->hwUpdateTbrValue = ticks + 200;
                flipper->Update();
            }
        }
    }

    Flipper::Flipper(HWConfig* config)
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

        GXOpen(mi.ram, wnd.hMainWindow);
        AXOpen();
        PADOpen();

        Debug::Hub.AddNode(HW_JDI_JSON, hw_init_handlers);

        hwUpdateThread = new Thread(HwUpdateThread, false, this);
    }

    Flipper::~Flipper()
    {
        delete hwUpdateThread;

        Debug::Hub.RemoveNode(HW_JDI_JSON);

        PADClose();
        AXClose();
        GXClose();

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

    void Flipper::Update()
    {
        // update joypads and video
        VIUpdate();     // PADs are updated there (SIPoll)
        CPUpdate();     // GX fifo - Bogus

        // update audio and DSP
        BeginProfileSfx();
        AIUpdate();
        EndProfileSfx();
    }

}
