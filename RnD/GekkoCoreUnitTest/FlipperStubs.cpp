// Flipper Stubs

#include "pch.h"

PIControl pi;
MIControl mi;

void PIReadByte(uint32_t pa, uint32_t* reg)
{
    uint8_t* ptr;

    if (mi.ram == nullptr)
    {
        *reg = 0;
        return;
    }

    if (pa >= PI_MEMSPACE_BOOTROM)
    {
        if (mi.BootromPresent)
        {
            ptr = &mi.bootrom[pa - PI_MEMSPACE_BOOTROM];
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
    if (pa >= PI_MEMSPACE_EFB)
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

void PIWriteByte(uint32_t pa, uint32_t data)
{
    uint8_t* ptr;

    if (mi.ram == nullptr)
    {
        return;
    }

    if (pa >= PI_MEMSPACE_BOOTROM)
    {
        return;
    }

    // hardware trap
    if (pa >= HW_BASE)
    {
        return;
    }

    // embedded frame buffer
    if (pa >= PI_MEMSPACE_EFB)
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

void PIReadHalf(uint32_t pa, uint32_t* reg)
{
    uint8_t* ptr;

    if (mi.ram == nullptr)
    {
        *reg = 0;
        return;
    }

    if (pa >= PI_MEMSPACE_BOOTROM)
    {
        if (mi.BootromPresent)
        {
            ptr = &mi.bootrom[pa - PI_MEMSPACE_BOOTROM];
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
    if (pa >= PI_MEMSPACE_EFB)
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

void PIWriteHalf(uint32_t pa, uint32_t data)
{
    uint8_t* ptr;

    if (mi.ram == nullptr)
    {
        return;
    }

    if (pa >= PI_MEMSPACE_BOOTROM)
    {
        return;
    }

    // hardware trap
    if (pa >= HW_BASE)
    {
        return;
    }

    // embedded frame buffer
    if (pa >= PI_MEMSPACE_EFB)
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

void PIReadWord(uint32_t pa, uint32_t* reg)
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

    if (pa >= PI_MEMSPACE_BOOTROM)
    {
        if (mi.BootromPresent)
        {
            ptr = &mi.bootrom[pa - PI_MEMSPACE_BOOTROM];
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
    if (pa >= PI_MEMSPACE_EFB)
    {
        return;
    }

    *reg = 0;
}

void PIWriteWord(uint32_t pa, uint32_t data)
{
    uint8_t* ptr;

    if (mi.ram == nullptr)
    {
        return;
    }

    if (pa >= PI_MEMSPACE_BOOTROM)
    {
        return;
    }

    // hardware trap
    if (pa >= HW_BASE)
    {
        return;
    }

    // embedded frame buffer
    if (pa >= PI_MEMSPACE_EFB)
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

void PIReadDouble(uint32_t pa, uint64_t* reg)
{
    if (pa >= PI_MEMSPACE_BOOTROM)
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

void PIWriteDouble(uint32_t pa, uint64_t* data)
{
    if (pa >= PI_MEMSPACE_BOOTROM)
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

void PIReadBurst(uint32_t phys_addr, uint8_t burstData[32])
{
    if ((phys_addr + 32) > RAMSIZE)
        return;

    memcpy(burstData, &mi.ram[phys_addr], 32);
}

void PIWriteBurst(uint32_t phys_addr, uint8_t burstData[32])
{
    // Hack for now
    for (int i = 0; i < 8; i++)
    {
        PIWriteWord(phys_addr + 4 * i, _byteswap_ulong(*(uint32_t*)(&burstData[4 * i])));
    }
}

uint8_t* MITranslatePhysicalAddress(uint32_t physAddr, size_t bytes)
{
    if (!mi.ram || bytes == 0)
        return nullptr;

    if (physAddr < (RAMSIZE - bytes))
    {
        return &mi.ram[physAddr];
    }

    if (physAddr >= BOOTROM_START_ADDRESS && mi.BootromPresent)
    {
        return &mi.bootrom[physAddr - BOOTROM_START_ADDRESS];
    }

    return nullptr;
}


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
