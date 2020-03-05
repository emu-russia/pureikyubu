// Emulator controls
#include "dolphin.h"

// emulator state
Emulator emu;

// ---------------------------------------------------------------------------

// this function is called once, during Dolwin life-time
void EMUInit()
{
    VERIFY(emu.running == TRUE, "Emulator not initialized!");
    if(emu.initok == TRUE) return;

    MEMInit();
    CPUInit();
    MEMOpen();
    MEMSelect(0, 0);

    emu.initok = TRUE;
}

// this function is called last, during Dolwin life-time
void EMUDie()
{
    VERIFY(emu.running == TRUE, "You should stop emulation, before exit!");
    if(emu.initok == FALSE) return;

    CPUFini();
    MEMFini();

    emu.initok = FALSE;
}

// this function calls every time, after user loading new file
void EMUOpen()
{
    if(emu.running == TRUE) return;
    emu.running = TRUE;

    // set loading cursor (THIS SHOULDNT BE HERE :))
    SetCursor(LoadCursor(NULL, IDC_WAIT));

    // open other sub-systems
    MEMOpen();
    MEMSelect(mem.mmu, 0);
    CPUOpen();
    VERIFY(GXOpen(mem.ram, wnd.hMainWindow) == 0, "Cannot start graphics!");
    VERIFY(AXOpen() == 0, "Cannot start audio!");
    VERIFY(PADOpen() == 0, "Cannot start joypad!");
    HWOpen();
    ReloadFile();   // PC will be set here
    HLEOpen();

    // take care about user interface
    OnMainWindowOpened();

    // set cursor back to normal (THIS SHOULDNT BE HERE :))
    SetCursor(LoadCursor(NULL, IDC_ARROW));

    // start emulation!
    CPUStart();
}

// this function calls every time, after user stops emulation
void EMUClose()
{
    if(emu.running == FALSE) return;
    emu.running = FALSE;

    // set loading cursor (THIS SHOULDNT BE HERE :))
    SetCursor(LoadCursor(NULL, IDC_WAIT));

    // take care about user interface
    OnMainWindowClosed();

    // close other sub-systems
    PADClose();
    AXClose();
    GXClose();
    HLEClose();
    HWClose();
    MEMClose();

    // set cursor back to normal (THIS SHOULDNT BE HERE :))
    SetCursor(LoadCursor(NULL, IDC_ARROW));
}

// you can use EMUClose(), then EMUOpen() to reset emulator, 
// and reload last used file.
