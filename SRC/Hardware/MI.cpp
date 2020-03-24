// MI - memory interface.
//
// MI is used in __OSInitMemoryProtection and by few games for debug.
// MI is implemented only for HW2 consoles! it is not back-compatible.
#include "pch.h"

// hardware traps tables.
void(__fastcall* hw_read8[0x10000])(uint32_t, uint32_t*);
void(__fastcall* hw_write8[0x10000])(uint32_t, uint32_t);
void(__fastcall* hw_read16[0x10000])(uint32_t, uint32_t*);
void(__fastcall* hw_write16[0x10000])(uint32_t, uint32_t);
void(__fastcall* hw_read32[0x10000])(uint32_t, uint32_t*);
void(__fastcall* hw_write32[0x10000])(uint32_t, uint32_t);

// stubs for MI registers
static void __fastcall no_write(uint32_t addr, uint32_t data) {}
static void __fastcall no_read(uint32_t addr, uint32_t *reg)  { *reg = 0; }

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
        hw_write8[pa & 0xffff](pa, (uint8_t)data);
        return;
    }

    // embedded frame buffer
    if (pa >= EFB_BASE)
    {
        EFBPoke16(pa & EFB_MASK, data);
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

void __fastcall MIReadWord(uint32_t pa, uint32_t* reg)
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

    // bus load word
    if (pa < mi.ramSize)
    {
        ptr = &mi.ram[pa];
        *reg = _byteswap_ulong(*(uint32_t*)ptr);
    }
    else
    {
        *reg = 0;
    }
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
    *(uint64_t*)buf = _byteswap_uint64 (*data);
}

// ---------------------------------------------------------------------------
// default hardware R/W operations.
// emulation is halted on unknown register access, if hw_assert = 1

static void __fastcall def_hw_read8(uint32_t addr, uint32_t* reg)
{
    DBHalt("MI: Unhandled HW access:  R8 %08X", addr);
}

static void __fastcall def_hw_write8(uint32_t addr, uint32_t data)
{
    DBHalt("MI: Unhandled HW access:  W8 %08X = %02X", addr, (uint8_t)data);
}

static void __fastcall def_hw_read16(uint32_t addr, uint32_t* reg)
{
    DBHalt("MI: Unhandled HW access: R16 %08X", addr);
}

static void __fastcall def_hw_write16(uint32_t addr, uint32_t data)
{
    DBHalt("MI: Unhandled HW access: W16 %08X = %04X", addr, (uint16_t)data);
}

static void __fastcall def_hw_read32(uint32_t addr, uint32_t* reg)
{
    DBHalt("MI: Unhandled HW access: R32 %08X", addr);
}

static void __fastcall def_hw_write32(uint32_t addr, uint32_t data)
{
    DBHalt("MI: Unhandled HW access: W32 %08X = %08X", addr, data);
}

// ---------------------------------------------------------------------------
// traps API

static void MISetTrap8(
    uint32_t addr,
    void(__fastcall* rdTrap)(uint32_t, uint32_t*),
    void(__fastcall* wrTrap)(uint32_t, uint32_t))
{
    if (rdTrap == NULL) rdTrap = def_hw_read8;
    if (wrTrap == NULL) wrTrap = def_hw_write8;

    hw_read8[addr & 0xffff] = rdTrap;
    hw_write8[addr & 0xffff] = wrTrap;
}

static void MISetTrap16(
    uint32_t addr,
    void(__fastcall* rdTrap)(uint32_t, uint32_t*),
    void(__fastcall* wrTrap)(uint32_t, uint32_t))
{
    if (rdTrap == NULL) rdTrap = def_hw_read16;
    if (wrTrap == NULL) wrTrap = def_hw_write16;

    hw_read16[addr & 0xfffe] = rdTrap;
    hw_write16[addr & 0xfffe] = wrTrap;
}

static void MISetTrap32(
    uint32_t addr,
    void(__fastcall* rdTrap)(uint32_t, uint32_t*),
    void(__fastcall* wrTrap)(uint32_t, uint32_t))
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
    void(__fastcall* rdTrap)(uint32_t, uint32_t*),  // register read trap
    void(__fastcall* wrTrap)(uint32_t, uint32_t))    // register write trap
{
    // address must be in correct range
    if (!((addr >= HW_BASE) && (addr < (HW_BASE + HW_MAX_KNOWN))))
    {
        DolwinError(
            "Hardware sub-system error",
            "Trap address is out of GAMECUBE registers range.\n"
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
            DolwinError(
                "Hardware sub-system error",
                "Unknown trap type : %u (%08X)",
                type, addr
            );
    }
}

// set default traps for any access,
// called every time when emu restarted
static void MIClearTraps()
{
    register uint32_t addr;

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

    if (strlen(config->BootromFilename) == 0)
    {
        DBReport2(DbgChannel::MI, "Bootrom not loaded (not specified)\n");
        return;
    }

    uint32_t bootromImageSize = 0;

    mi.bootrom = (uint8_t *)FileLoad(config->BootromFilename, &bootromImageSize);

    if (mi.bootrom == nullptr)
    {
        DBReport2(DbgChannel::MI, "Cannot load Bootrom: %s\n", config->BootromFilename);
        return;
    }

    if (bootromImageSize != mi.bootromSize)
    {
        free(mi.bootrom);
        mi.bootrom = nullptr;
        return;
    }

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

        free(mi.bootrom);
        mi.bootrom = nullptr;
        return;
    }

    // Descramble

    IPLDescrambler(&mi.bootrom[beginOffset], (offset - beginOffset));
    mi.BootromPresent = true;

    // Show version

    DBReport2(DbgChannel::MI, "Loaded and descrambled valid Bootrom\n");
    DBReport("%s", (char*)mi.bootrom);
}

void MIOpen(HWConfig * config)
{
    DBReport2(DbgChannel::MI, "Flipper memory interface\n");

    // now any access will generate unhandled warning,
    // if emulator try to read or write register,
    // so we need to set traps for missing regs:

    // clear all traps
    MIClearTraps();

    mi.ramSize = config->ramsize;
    mi.ram = (uint8_t *)malloc(mi.ramSize);
    assert(mi.ram);

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
        free(mi.ram);
        mi.ram = nullptr;
    }

    if (mi.bootrom)
    {
        free(mi.bootrom);
        mi.bootrom = nullptr;
    }

    MIClearTraps();
}
