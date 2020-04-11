// Flipper MI Stubs

#include "pch.h"

MIControl mi;

void __fastcall MIReadByte(uint32_t pa, uint32_t* reg)
{
    uint8_t* ptr;

    if (mi.ram == nullptr)
    {
        *reg = 0;
        return;
    }

    if (pa >= BOOTROM_START_ADDRESS)
    {
        if (mi.BootromPresent)
        {
            ptr = &mi.bootrom[pa - BOOTROM_START_ADDRESS];
            *reg = (uint32_t)*ptr;
        }
        else
        {
            *reg = 0xFF;
        }
        return;
    }

    // hardware trap
    if (pa >= HW_BASE)
    {
        return;
    }

    // embedded frame buffer
    if (pa >= EFB_BASE)
    {
        return;
    }

    // bus load byte
    if (pa < mi.ramSize)
    {
        ptr = &mi.ram[pa];
        *reg = (uint32_t)*ptr;
    }
    else
    {
        *reg = 0;
    }
}

void __fastcall MIWriteByte(uint32_t pa, uint32_t data)
{
    uint8_t* ptr;

    if (mi.ram == nullptr)
    {
        return;
    }

    if (pa >= BOOTROM_START_ADDRESS)
    {
        return;
    }

    // hardware trap
    if (pa >= HW_BASE)
    {
        return;
    }

    // embedded frame buffer
    if (pa >= EFB_BASE)
    {
        return;
    }

    // bus store byte
    if (pa < mi.ramSize)
    {
        ptr = &mi.ram[pa];
        *ptr = (uint8_t)data;
    }
}

void __fastcall MIReadHalf(uint32_t pa, uint32_t* reg)
{
    uint8_t* ptr;

    if (mi.ram == nullptr)
    {
        *reg = 0;
        return;
    }

    if (pa >= BOOTROM_START_ADDRESS)
    {
        if (mi.BootromPresent)
        {
            ptr = &mi.bootrom[pa - BOOTROM_START_ADDRESS];
            *reg = (uint32_t)_byteswap_ushort(*(uint16_t*)ptr);
        }
        else
        {
            *reg = 0xFFFF;
        }
        return;
    }

    // hardware trap
    if (pa >= HW_BASE)
    {
        return;
    }

    // embedded frame buffer
    if (pa >= EFB_BASE)
    {
        return;
    }

    // bus load halfword
    if (pa < mi.ramSize)
    {
        ptr = &mi.ram[pa];
        *reg = (uint32_t)_byteswap_ushort(*(uint16_t*)ptr);
    }
    else
    {
        *reg = 0;
    }
}

void __fastcall MIWriteHalf(uint32_t pa, uint32_t data)
{
    uint8_t* ptr;

    if (mi.ram == nullptr)
    {
        return;
    }

    if (pa >= BOOTROM_START_ADDRESS)
    {
        return;
    }

    // hardware trap
    if (pa >= HW_BASE)
    {
        return;
    }

    // embedded frame buffer
    if (pa >= EFB_BASE)
    {
        return;
    }

    // bus store halfword
    if (pa < mi.ramSize)
    {
        ptr = &mi.ram[pa];
        *(uint16_t*)ptr = _byteswap_ushort((uint16_t)data);
    }
}

void __fastcall MIReadWord(uint32_t pa, uint32_t* reg)
{
    uint8_t* ptr;

    if (mi.ram == nullptr)
    {
        *reg = 0;
        return;
    }

    // bus load word
    if (pa < mi.ramSize)
    {
        ptr = &mi.ram[pa];
        *reg = _byteswap_ulong(*(uint32_t*)ptr);
        return;
    }

    if (pa >= BOOTROM_START_ADDRESS)
    {
        if (mi.BootromPresent)
        {
            ptr = &mi.bootrom[pa - BOOTROM_START_ADDRESS];
            *reg = _byteswap_ulong(*(uint32_t*)ptr);
        }
        else
        {
            *reg = 0xFFFFFFFF;
        }
        return;
    }

    // hardware trap
    if (pa >= HW_BASE)
    {
        return;
    }

    // embedded frame buffer
    if (pa >= EFB_BASE)
    {
        return;
    }

    *reg = 0;
}

void __fastcall MIWriteWord(uint32_t pa, uint32_t data)
{
    uint8_t* ptr;

    if (mi.ram == nullptr)
    {
        return;
    }

    if (pa >= BOOTROM_START_ADDRESS)
    {
        return;
    }

    // hardware trap
    if (pa >= HW_BASE)
    {
        return;
    }

    // embedded frame buffer
    if (pa >= EFB_BASE)
    {
        return;
    }

    // bus store word
    if (pa < mi.ramSize)
    {
        ptr = &mi.ram[pa];
        *(uint32_t*)ptr = _byteswap_ulong(data);
    }
}

//
// fortunately longlongs are never used in GC hardware access
// (because all regs are generally integers)
//

void __fastcall MIReadDouble(uint32_t pa, uint64_t* reg)
{
    if (pa >= BOOTROM_START_ADDRESS)
    {
        assert(true);
    }

    if (pa >= RAMSIZE || mi.ram == nullptr)
    {
        *reg = 0;
        return;
    }

    uint8_t* buf = &mi.ram[pa];

    // bus load doubleword
    *reg = _byteswap_uint64(*(uint64_t*)buf);
}

void __fastcall MIWriteDouble(uint32_t pa, uint64_t* data)
{
    if (pa >= BOOTROM_START_ADDRESS)
    {
        return;
    }

    if (pa >= RAMSIZE || mi.ram == nullptr)
    {
        return;
    }

    uint8_t* buf = &mi.ram[pa];

    // bus store doubleword
    *(uint64_t*)buf = _byteswap_uint64(*data);
}

void __fastcall MIWriteBurst(uint32_t phys_addr, uint8_t burstData[32])
{
    // Hack for now
    for (int i = 0; i < 8; i++)
    {
        MIWriteWord(phys_addr + 4 * i, _byteswap_ulong(*(uint32_t*)(&burstData[4 * i])));
    }
}

// ---------------------------------------------------------------------------

void MIOpen(HWConfig* config)
{
    mi.ramSize = config->ramsize;
    mi.ram = (uint8_t*)malloc(mi.ramSize);
    assert(mi.ram);

    mi.BootromPresent = false;

    memset(mi.ram, 0, mi.ramSize);
}

void MIClose()
{
    if (mi.ram)
    {
        free(mi.ram);
        mi.ram = nullptr;
    }
}
