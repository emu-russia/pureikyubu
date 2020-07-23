// MI - memory interface.
//
// MI is used in __OSInitMemoryProtection and by few games for debug.
// MI is implemented only for HW2 consoles! it is not back-compatible.
#include "pch.h"

using namespace Debug;

// hardware traps tables.
void (*hw_read8[0x10000])(uint32_t, uint32_t*);
void (*hw_write8[0x10000])(uint32_t, uint32_t);
void (*hw_read16[0x10000])(uint32_t, uint32_t*);
void (*hw_write16[0x10000])(uint32_t, uint32_t);
void (*hw_read32[0x10000])(uint32_t, uint32_t*);
void (*hw_write32[0x10000])(uint32_t, uint32_t);

// stubs for MI registers
static void no_write(uint32_t addr, uint32_t data) {}
static void no_read(uint32_t addr, uint32_t *reg)  { *reg = 0; }

MIControl mi;

void MIReadByte(uint32_t pa, uint32_t* reg)
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
        hw_read8[pa & 0xffff](pa, reg);
        return;
    }

    // embedded frame buffer
    if (pa >= EFB_BASE)
    {
        EFBPeek8(pa & EFB_MASK, reg);
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

void MIWriteByte(uint32_t pa, uint32_t data)
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
        hw_write8[pa & 0xffff](pa, (uint8_t)data);
        return;
    }

    // embedded frame buffer
    if (pa >= EFB_BASE)
    {
        EFBPoke8(pa & EFB_MASK, data);
        return;
    }

    // bus store byte
    if (pa < mi.ramSize)
    {
        ptr = &mi.ram[pa];
        *ptr = (uint8_t)data;
    }
}

void MIReadHalf(uint32_t pa, uint32_t* reg)
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
        hw_read16[pa & 0xfffe](pa, reg);
        return;
    }

    // embedded frame buffer
    if (pa >= EFB_BASE)
    {
        EFBPeek16(pa & EFB_MASK, reg);
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

void MIWriteHalf(uint32_t pa, uint32_t data)
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
        hw_write16[pa & 0xfffe](pa, data);
        return;
    }

    // embedded frame buffer
    if (pa >= EFB_BASE)
    {
        EFBPoke16(pa & EFB_MASK, data);
        return;
    }

    // bus store halfword
    if (pa < mi.ramSize)
    {
        ptr = &mi.ram[pa];
        *(uint16_t*)ptr = _byteswap_ushort((uint16_t)data);
    }
}

void MIReadWord(uint32_t pa, uint32_t* reg)
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
        hw_read32[pa & 0xfffc](pa, reg);
        return;
    }

    // embedded frame buffer
    if (pa >= EFB_BASE)
    {
        EFBPeek32(pa & EFB_MASK, reg);
        return;
    }

    *reg = 0;
}

void MIWriteWord(uint32_t pa, uint32_t data)
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
        hw_write32[pa & 0xfffc](pa, data);
        return;
    }

    // embedded frame buffer
    if (pa >= EFB_BASE)
    {
        EFBPoke32(pa & EFB_MASK, data);
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

void MIReadDouble(uint32_t pa, uint64_t* reg)
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

void MIWriteDouble(uint32_t pa, uint64_t* data)
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
    *(uint64_t*)buf = _byteswap_uint64 (*data);
}

void MIReadBurst(uint32_t phys_addr, uint8_t burstData[32])
{
    if ((phys_addr + 32) > RAMSIZE)
        return;

    memcpy(burstData, &mi.ram[phys_addr], 32);
}

void MIWriteBurst(uint32_t phys_addr, uint8_t burstData[32])
{
    if (phys_addr == GX_FIFO)
    {
        GXFifoWriteBurst(burstData);
        return;
    }

    if ((phys_addr + 32) > RAMSIZE)
        return;

    memcpy(&mi.ram[phys_addr], burstData, 32);
}

// ---------------------------------------------------------------------------
// default hardware R/W operations.
// emulation is halted on unknown register access.

static void def_hw_read8(uint32_t addr, uint32_t* reg)
{
    Halt("MI: Unhandled HW access:  R8 %08X", addr);
}

static void def_hw_write8(uint32_t addr, uint32_t data)
{
    Halt("MI: Unhandled HW access:  W8 %08X = %02X", addr, (uint8_t)data);
}

static void def_hw_read16(uint32_t addr, uint32_t* reg)
{
    Halt("MI: Unhandled HW access: R16 %08X", addr);
}

static void def_hw_write16(uint32_t addr, uint32_t data)
{
    Halt("MI: Unhandled HW access: W16 %08X = %04X", addr, (uint16_t)data);
}

static void def_hw_read32(uint32_t addr, uint32_t* reg)
{
    Halt("MI: Unhandled HW access: R32 %08X", addr);
}

static void def_hw_write32(uint32_t addr, uint32_t data)
{
    Halt("MI: Unhandled HW access: W32 %08X = %08X", addr, data);
}

// ---------------------------------------------------------------------------
// traps API

static void MISetTrap8(
    uint32_t addr,
    void (*rdTrap)(uint32_t, uint32_t*),
    void (*wrTrap)(uint32_t, uint32_t))
{
    if (rdTrap == NULL) rdTrap = def_hw_read8;
    if (wrTrap == NULL) wrTrap = def_hw_write8;

    hw_read8[addr & 0xffff] = rdTrap;
    hw_write8[addr & 0xffff] = wrTrap;
}

static void MISetTrap16(
    uint32_t addr,
    void (*rdTrap)(uint32_t, uint32_t*),
    void (*wrTrap)(uint32_t, uint32_t))
{
    if (rdTrap == NULL) rdTrap = def_hw_read16;
    if (wrTrap == NULL) wrTrap = def_hw_write16;

    hw_read16[addr & 0xfffe] = rdTrap;
    hw_write16[addr & 0xfffe] = wrTrap;
}

static void MISetTrap32(
    uint32_t addr,
    void (*rdTrap)(uint32_t, uint32_t*),
    void (*wrTrap)(uint32_t, uint32_t))
{
    if (rdTrap == NULL) rdTrap = def_hw_read32;
    if (wrTrap == NULL) wrTrap = def_hw_write32;

    hw_read32[addr & 0xfffc] = rdTrap;
    hw_write32[addr & 0xfffc] = wrTrap;
}

// wrapper
void MISetTrap(
    uint32_t type,                               // 8, 16 or 32
    uint32_t addr,                               // physical trap address
    void (*rdTrap)(uint32_t, uint32_t*),  // register read trap
    void (*wrTrap)(uint32_t, uint32_t))    // register write trap
{
    // address must be in correct range
    if (!((addr >= HW_BASE) && (addr < (HW_BASE + HW_MAX_KNOWN))))
    {
        Halt(
            "MI: Trap address is out of GAMECUBE registers range.\n"
            "address : %08X\n", addr
        );
    }

    // select trap type
    switch (type)
    {
        case 8:
            MISetTrap8(addr, rdTrap, wrTrap);
            break;
        case 16:
            MISetTrap16(addr, rdTrap, wrTrap);
            break;
        case 32:
            MISetTrap32(addr, rdTrap, wrTrap);
            break;

            // should never happen
        default:
            throw "Unknown trap type";
    }
}

// set default traps for any access,
// called every time when emu restarted
static void MIClearTraps()
{
    uint32_t addr;

    // possible errors, if greater 0xffff
    assert(HW_MAX_KNOWN < 0x10000);

    // for 8-bit registers
    for (addr = HW_BASE; addr < (HW_BASE + HW_MAX_KNOWN); addr++)
    {
        MISetTrap8(addr, NULL, NULL);
    }

    // for 16-bit registers
    for (addr = HW_BASE; addr < (HW_BASE + HW_MAX_KNOWN); addr += 2)
    {
        MISetTrap16(addr, NULL, NULL);
    }

    // for 32-bit registers
    for (addr = HW_BASE; addr < (HW_BASE + HW_MAX_KNOWN); addr += 4)
    {
        MISetTrap32(addr, NULL, NULL);
    }
}

// Load and descramble bootrom.
// This implementation makes working with Bootrom easier, since we do not need to monitor cache transactions ("bursts") from the processor.

void LoadBootrom(HWConfig* config)
{
    mi.BootromPresent = false;
    mi.bootromSize = BOOTROM_SIZE;

    // Load bootrom image

    if (_tcslen(config->BootromFilename) == 0)
    {
        Report(Channel::MI, "Bootrom not loaded (not specified)\n");
        return;
    }

    auto bootrom = Util::FileLoad(config->BootromFilename);
    if (bootrom.empty())
    {
        Report(Channel::MI, "Cannot load Bootrom: %s\n", config->BootromFilename);
        return;
    }

    mi.bootrom = new uint8_t[mi.bootromSize];

    if (bootrom.size() != mi.bootromSize)
    {
        delete [] mi.bootrom;
        mi.bootrom = nullptr;
        return;
    }

    memcpy(mi.bootrom, bootrom.data(), bootrom.size());

    // Determine size of encrypted data (find first empty cache burst line)

    const size_t strideSize = 0x20;
    uint8_t zeroStride[strideSize] = { 0 };

    size_t beginOffset = 0x100;
    size_t endOffset = mi.bootromSize - strideSize;
    size_t offset = beginOffset;

    while (offset < endOffset)
    {
        if (!memcmp(&mi.bootrom[offset], zeroStride, sizeof(zeroStride)))
        {
            break;
        }

        offset += strideSize;
    }

    if (offset == endOffset)
    {
        // Empty cacheline not found, something wrong with the image

        delete[] mi.bootrom;
        mi.bootrom = nullptr;
        return;
    }

    // Descramble

    IPLDescrambler(&mi.bootrom[beginOffset], (offset - beginOffset));
    mi.BootromPresent = true;

    // Show version

    Report(Channel::MI, "Loaded and descrambled valid Bootrom\n");
    Report(Channel::Norm, "%s", (char*)mi.bootrom);
}

void MIOpen(HWConfig * config)
{
    Report(Channel::MI, "Flipper memory interface\n");

    // now any access will generate unhandled warning,
    // if emulator try to read or write register,
    // so we need to set traps for missing regs:

    // clear all traps
    MIClearTraps();

    mi.ramSize = config->ramsize;
    mi.ram = new uint8_t[mi.ramSize];

    memset(mi.ram, 0, mi.ramSize);

    for(uint32_t ofs=0; ofs<=0x28; ofs+=2)
    {
        MISetTrap(16, 0x0C004000 | ofs, no_read, no_write);
    }

    LoadBootrom(config);
}

void MIClose()
{
    if (mi.ram)
    {
        delete [] mi.ram;
        mi.ram = nullptr;
    }

    if (mi.bootrom)
    {
        delete[] mi.bootrom;
        mi.bootrom = nullptr;
    }

    MIClearTraps();
}

uint8_t* MITranslatePhysicalAddress(uint32_t physAddr, size_t bytes)
{
    if (!mi.ram || bytes == 0)
        return nullptr;

    if (physAddr < (RAMSIZE - bytes))
    {
        return &mi.ram[physAddr];
    }
    
    return nullptr;
}
