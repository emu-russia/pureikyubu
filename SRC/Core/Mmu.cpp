// memory engine
#include "pch.h"

// shared data
MEMControl mem;

// ---------------------------------------------------------------------------
// simple translation (only for Dolphin OS)

uint32_t __fastcall GCEffectiveToPhysical(uint32_t ea, bool IR)
{
    // Required to run bootrom
    if (ea >= BOOTROM_START_ADDRESS)
    {
        return ea;
    }

    // ignore no memory, page faults, alignment, etc errors
    return ea & RAMMASK;        // thats all =:)
}

void __fastcall MEMReadByte(uint32_t addr, uint32_t *reg)
{
    // Locked cache
    if (addr >= 0xe0000000)
    {
        uint8_t * ptr = &mem.lc[addr & 0x3ffff];
        *reg = (uint32_t)*ptr;
        return;
    }

    uint32_t pa = GCEffectiveToPhysical(addr, false);
    MIReadByte(pa, reg);
}

void __fastcall MEMWriteByte(uint32_t addr, uint32_t data)
{
    // Locked cache
    if (addr >= 0xe0000000)
    {
        uint8_t* ptr = &mem.lc[addr & 0x3ffff];
        *ptr = (uint8_t)data;
        return;
    }

    uint32_t pa = GCEffectiveToPhysical(addr, false);
    MIWriteByte(pa, data);
}

void __fastcall MEMReadHalf(uint32_t addr, uint32_t *reg)
{
    // Locked cache
    if (addr >= 0xe0000000)
    {
        uint8_t* ptr = &mem.lc[addr & 0x3ffff];
        *reg = (uint32_t)_byteswap_ushort(*(uint16_t*)ptr);
        return;
    }

    uint32_t pa = GCEffectiveToPhysical(addr, false);
    MIReadHalf(pa, reg);
}

void __fastcall MEMReadHalfS(uint32_t addr, uint32_t *reg)
{
    // Locked cache
    if (addr >= 0xe0000000)
    {
        uint8_t* ptr = &mem.lc[addr & 0x3ffff];
        *reg = _byteswap_ushort(*(uint16_t*)ptr);
        if (*reg & 0x8000) *reg |= 0xffff0000;
        return;
    }

    uint32_t pa = GCEffectiveToPhysical(addr, false);
    MIReadHalf(pa, reg);
    if (*reg & 0x8000) *reg |= 0xffff0000;
}

void __fastcall MEMWriteHalf(uint32_t addr, uint32_t data)
{
    // Locked cache
    if (addr >= 0xe0000000)
    {
        uint8_t* ptr = &mem.lc[addr & 0x3ffff];
        *(uint16_t*)ptr = _byteswap_ushort((uint16_t)data);
        return;
    }

    uint32_t pa = GCEffectiveToPhysical(addr, false);
    MIWriteHalf(pa, data);
}

void __fastcall MEMReadWord(uint32_t addr, uint32_t *reg)
{
    // Locked cache
    if (addr >= 0xe0000000)
    {
        uint8_t* ptr = &mem.lc[addr & 0x3ffff];
        *reg = _byteswap_ulong(*(uint32_t*)ptr);
        return;
    }

    uint32_t pa = GCEffectiveToPhysical(addr, false);
    MIReadWord(pa, reg);
}

void __fastcall MEMWriteWord(uint32_t addr, uint32_t data)
{
    // Locked cache
    if (addr >= 0xe0000000)
    {
        uint8_t* ptr = &mem.lc[addr & 0x3ffff];
        *(uint32_t*)ptr = _byteswap_ulong(data);
        return;
    }
   
    uint32_t pa = GCEffectiveToPhysical(addr, false);
    MIWriteWord(pa, data);
}

//
// fortunately longlongs are never used in GC hardware access
// (because all regs are generally integers)
//

void __fastcall MEMReadDouble(uint32_t addr, uint64_t *reg)
{
    // Locked cache
    if (addr >= 0xe0000000)
    {
        uint8_t* buf = &mem.lc[addr & 0x3ffff];
        *reg = _byteswap_uint64(*(uint64_t *)buf);
        return;
    }

    uint32_t pa = GCEffectiveToPhysical(addr, false);
    MIReadDouble(pa, reg);
}

void __fastcall MEMWriteDouble(uint32_t addr, uint64_t *data)
{
    // Locked cache
    if (addr >= 0xe0000000)
    {
        uint8_t* buf = &mem.lc[addr & 0x3ffff];
        *(uint64_t *)buf = _byteswap_uint64(*data);
        return;
    }

    uint32_t pa = GCEffectiveToPhysical(addr, false);
    MIWriteDouble(pa, data);
}

// ---------------------------------------------------------------------------

// TODO: Not Implemented (https://github.com/ogamespec/dolwin/issues/9)

// PPC MMU simulation (used by GC-Linux and other advanced stuff).
// we are using memory map table, to speed up address translation.
// we should "remap" tables, after changing of some PPC system registers, or 
// page translation table. it is good place to remap "data" before load/store
// operation, and "instruction" before any "non-linear" PC change (i.e. branch,
// exception or like).

// we do not support access rights for BAT logic (in that case we must have two
// standalone lookup talbes for Load and Store operations).

// BAT fields
#define BATBEPI(batu)   (batu >> 17)
#define BATBL(batu)     ((batu >> 2) & 0x7ff)
#define BATBRPN(batl)   (batl >> 17)

//static uint32_t *dbatu[4] = { &DBAT0U, &DBAT1U, &DBAT2U, &DBAT3U };
//static uint32_t *dbatl[4] = { &DBAT0L, &DBAT1L, &DBAT2L, &DBAT3L };
//static uint32_t *ibatu[4] = { &IBAT0U, &IBAT1U, &IBAT2U, &IBAT3U };
//static uint32_t *ibatl[4] = { &IBAT0L, &IBAT1L, &IBAT2L, &IBAT3L };

uint32_t __fastcall MMUEffectiveToPhysical(uint32_t ea, bool IR)
{
#if 0
    uint32_t pa;

    // ea = effective address
    // pa = physical address
    // pn = page number

    // perform direct translation
    if(IR)
    {
        if(!(MSR & MSR_IR)) return pa = ea;

        for(int n=0; n<4; n++)
        {
            uint32_t bepi = BATBEPI(*ibatu[n]);
            uint32_t bl   = BATBL(*ibatu[n]);
            uint32_t tst  = (ea >> 17) & (0x7800 | ~bl);
            if(bepi == tst)
            {
                pa = BATBRPN(*ibatl[n]) | ((ea >> 17) & bl);
                pa = (pa << 17) | (ea & 0x1ffff);
                return pa;
            }
        }
    }
    else
    {
        if(!(MSR & MSR_DR)) return pa = ea;

        for(int n=0; n<4; n++)
        {
            uint32_t bepi = BATBEPI(*dbatu[n]);
            uint32_t bl   = BATBL(*dbatu[n]);
            uint32_t tst  = (ea >> 17) & (0x7800 | ~bl);
            if(bepi == tst)
            {
                pa = BATBRPN(*dbatl[n]) | ((ea >> 17) & bl);
                pa = (pa << 17) | (ea & 0x1ffff);
                return pa;
            }
        }
    }
#endif

    return -1;
}
