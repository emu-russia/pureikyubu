// MI - memory interface.
//
// MI is used in __OSInitMemoryProtection and by few games for debug.
// MI is implemented only for HW2 consoles! it is not back-compatible.
#include "pch.h"

// stubs for MI registers
static void __fastcall no_write(uint32_t addr, uint32_t data) {}
static void __fastcall no_read(uint32_t addr, uint32_t *reg)  { *reg = 0; }

void MIOpen()
{
    DBReport(CYAN "MI: Flipper memory interface\n");

    for(uint32_t ofs=0; ofs<=0x28; ofs+=2)
    {
        HWSetTrap(16, 0x0C004000 | ofs, no_read, no_write);
    }
}
