// Emulator controls
#include "dolphin.h"

// emulator state
Emulator emu;

// ---------------------------------------------------------------------------

// this function is called once, during Dolwin life-time
void EMUInit()
{
    assert(!emu.running);
    if(emu.initok == true) return;

    MEMInit();
    CPUInit();
    MEMOpen(GetConfigInt(USER_MMU, USER_MMU_DEFAULT));
    MEMSelect(0, 0);

    emu.initok = true;
}

// this function is called last, during Dolwin life-time
void EMUDie()
{
    assert(!emu.running);
    if(emu.initok == false) return;

    CPUFini();
    MEMFini();

    emu.initok = false;
}

// this function calls every time, after user loading new file
void EMUOpen(int bailout, int delay, int counterFactor)
{
    if(emu.running == true) return;
    emu.running = true;

    OnMainWindowOpening();

    // open other sub-systems
    MEMOpen(GetConfigInt(USER_MMU, USER_MMU_DEFAULT));
    MEMSelect(mem.mmu, 0);
    CPUOpen(bailout, delay, counterFactor);
    assert(GXOpen(mem.ram, wnd.hMainWindow));
    assert(AXOpen());
    assert(PADOpen());
    HWOpen(wnd.hMainWindow);
    ReloadFile();   // PC will be set here
    HLEOpen();

    // debugger has its own core, to control CPU execution
    if (emu.doldebug)
    {
        CPUStart = DBStart;
        CPUException = DBException;
    }

    // take care about user interface
    OnMainWindowOpened();

    // start emulation!
    CPUStart();
}

// this function calls every time, after user stops emulation
void EMUClose()
{
    if(emu.running == false) return;
    emu.running = false;

    // take care about user interface
    OnMainWindowClosing();

    // close other sub-systems
    PADClose();
    AXClose();
    GXClose();
    HLEClose();
    HWClose();
    MEMClose();

    // take care about user interface
    OnMainWindowClosed();
}

// you can use EMUClose(), then EMUOpen() to reset emulator, 
// and reload last used file.
