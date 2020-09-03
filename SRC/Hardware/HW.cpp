// hardware init/update code
// IMPORTANT: whole HW should use physical CPU addressing, not effective!
#include "pch.h"

using namespace Debug;

namespace Flipper
{
    Flipper* HW;
    DSP::Dsp16* DSP;      // Instance of dsp core
    GX::GXCore* Gx;         // Instance of GX processor

    // This thread acts as the HWUpdate of Dolwin 0.10.
    // Previously, an HWUpdate call occurred after each Gekko instruction (or so).
    // This was tied only to update VI, SI and AI.
    // After switching to multitasking, leave the old HWUpdate update mechanism, will be gradually replaced by other threads of different Flipper components.
    void Flipper::HwUpdateThread(void* Parameter)
    {
        Flipper* flipper = (Flipper*)Parameter;

        int64_t ticks = Gekko::Gekko->GetTicks();
        if (ticks < flipper->hwUpdateTbrValue)
        {
            return;
        }
        flipper->hwUpdateTbrValue = ticks + Flipper::ticksToHwUpdate;

        flipper->Update();
    }

    Flipper::Flipper(HWConfig* config)
    {
        Report(Channel::Info,
            "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n"
            "Hardware Initialization.\n"
            "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\n"
        );

        MIOpen(config); // memory protection and 1T-SRAM interface
        VIOpen(config); // video (TV)
        CP_PEOpen();    // Command Processor & PixelEngin
        AIOpen(config); // audio (AID and AIS)
        AROpen();       // aux. memory (ARAM)
        EIOpen(config); // expansion interface (EXI)
        DIOpen();       // disk
        SIOpen();       // GC controllers
        PIOpen(config); // interrupts, console regs

        DSP->core->HardReset();

        // Load IROM.

        auto iromImage = Util::FileLoad(config->DspIromFilename);

        if (DSP->LoadIrom(iromImage))
        {
            Report(Channel::DSP, "Loaded DSP IROM: %s\n", Util::WstringToString(config->DspIromFilename).c_str());
        }
        else
        {
            Report(Channel::Norm, "Failed to load DSP IROM: %s\n", Util::WstringToString(config->DspIromFilename).c_str());
        }

        // Load DROM.

        auto dromImage = Util::FileLoad(config->DspDromFilename);

        if (DSP->LoadDrom(dromImage))
        {
            Report(Channel::DSP, "Loaded DSP DROM: %s\n", Util::WstringToString(config->DspDromFilename).c_str());
        }
        else
        {
            Report(Channel::Norm, "Failed to load DSP DROM\n", Util::WstringToString(config->DspDromFilename).c_str());
        }

        Report(Channel::Norm, "\n");

        Gx->Open();
        GXOpen(config, mi.ram);
        PADOpen();

        JDI::Hub.AddNode(HW_JDI_JSON, hw_init_handlers);

        hwUpdateThread = new Thread(HwUpdateThread, false, this, "HW");
    }

    Flipper::~Flipper()
    {
        delete hwUpdateThread;

        JDI::Hub.RemoveNode(HW_JDI_JSON);

        DSP->Suspend();

        CP_PEClose();
        AIClose();
        ARClose();      // release ARAM
        EIClose();      // take care about closing of memcards and BBA
        VIClose();      // close GDI (if opened)
        DIClose();      // release streaming buffer
        MIClose();
        PIClose();

        PADClose();
        GXClose();
        Gx->Close();
    }

    void Flipper::Update()
    {
        // update joypads and video
        VIUpdate();
        SIPoll();
    }

}
