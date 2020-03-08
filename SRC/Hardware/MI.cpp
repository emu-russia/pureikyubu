// MI - memory interface.
//
// MI is used in __OSInitMemoryProtection and by few games for debug.
// MI is implemented only for HW2 consoles! it is not back-compatible.
#include "pch.h"

// stubs for MI registers
static void __fastcall no_write(uint32_t addr, uint32_t data) {}
static void __fastcall no_read(uint32_t addr, uint32_t *reg)  { *reg = 0; }

MIControl mi;

void MIOpen(HWConfig * config)
{
    DBReport(CYAN "MI: Flipper memory interface\n");

    mi.ramSize = config->ramsize;
    mi.ram = (uint8_t *)malloc(mi.ramSize);
    assert(mi.ram);

    memset(mi.ram, 0, mi.ramSize);

    for(uint32_t ofs=0; ofs<=0x28; ofs+=2)
    {
        HWSetTrap(16, 0x0C004000 | ofs, no_read, no_write);
    }
}

void MIClose()
{
    if (mi.ram)
    {
        free(mi.ram);
        mi.ram = nullptr;
    }
}
