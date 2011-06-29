// MI - memory interface stubs.
//
// MI is used in __OSInitMemoryProtection and by few games for debug.
// MI is implemented only for HW2 consoles! it is not back-compatible.
#include "dolphin.h"

// stubs for MI registers
static void __fastcall no_write(u32 addr, u32 data) {}
static void __fastcall no_read(u32 addr, u32 *reg)  { *reg = 0; }

void MIOpen()
{
    DBReport(CYAN "MI: Flipper memory interface\n");

    for(u32 ofs=0; ofs<=0x28; ofs+=2)
    {
        HWSetTrap(16, 0x0C004000 | ofs, no_read, no_write);
    }
}
