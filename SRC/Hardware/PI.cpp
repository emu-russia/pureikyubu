// PI - processor interface (interrupts and console control regs, FIFO)
#include "pch.h"

using namespace Debug;

// PI state (registers and other data)
PIControl pi;

// hardware traps tables.
static void (*hw_read8[0x10000])(uint32_t, uint32_t*);
static void (*hw_write8[0x10000])(uint32_t, uint32_t);
static void (*hw_read16[0x10000])(uint32_t, uint32_t*);
static void (*hw_write16[0x10000])(uint32_t, uint32_t);
static void (*hw_read32[0x10000])(uint32_t, uint32_t*);
static void (*hw_write32[0x10000])(uint32_t, uint32_t);

#pragma region "Processor Memory Interface"

// Interface for accessing memory and memory-mapped registers from the processor side.
// For more details see Docs/HW/ProcessorInterface.md

// In previous versions, this functionality was mistakenly located in the MI module.
// After a sufficient study of architecture, it became clear that, in fact, it belongs here.

namespace Gekko
{
    void SixtyBus_ReadByte(uint32_t pa, uint32_t* reg)
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

    void SixtyBus_WriteByte(uint32_t pa, uint32_t data)
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

        // bus store byte
        if (pa < mi.ramSize)
        {
            ptr = &mi.ram[pa];
            *ptr = (uint8_t)data;
        }
    }

    void SixtyBus_ReadHalf(uint32_t pa, uint32_t* reg)
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
                *reg = (uint32_t)_BYTESWAP_UINT16(*(uint16_t*)ptr);
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

        // bus load halfword
        if (pa < mi.ramSize)
        {
            ptr = &mi.ram[pa];
            *reg = (uint32_t)_BYTESWAP_UINT16(*(uint16_t*)ptr);
        }
        else
        {
            *reg = 0;
        }
    }

    void SixtyBus_WriteHalf(uint32_t pa, uint32_t data)
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

        // bus store halfword
        if (pa < mi.ramSize)
        {
            ptr = &mi.ram[pa];
            *(uint16_t*)ptr = _BYTESWAP_UINT16((uint16_t)data);
        }
    }

    void SixtyBus_ReadWord(uint32_t pa, uint32_t* reg)
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
            *reg = _BYTESWAP_UINT32(*(uint32_t*)ptr);
            return;
        }

        if (pa >= BOOTROM_START_ADDRESS)
        {
            if (mi.BootromPresent)
            {
                ptr = &mi.bootrom[pa - BOOTROM_START_ADDRESS];
                *reg = _BYTESWAP_UINT32(*(uint32_t*)ptr);
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
        if ((pa & PI_EFB_ADDRESS_MASK) == PI_MEMSPACE_EFB)
        {
            *reg = Flipper::Gx->EfbPeek(pa);
            return;
        }

        *reg = 0;
    }

    void SixtyBus_WriteWord(uint32_t pa, uint32_t data)
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
        if ((pa & PI_EFB_ADDRESS_MASK) == PI_MEMSPACE_EFB)
        {
            Flipper::Gx->EfbPoke(pa, data);
            return;
        }

        // bus store word
        if (pa < mi.ramSize)
        {
            ptr = &mi.ram[pa];
            *(uint32_t*)ptr = _BYTESWAP_UINT32(data);
        }
    }

    //
    // longlongs are never used in GC hardware access
    //

    void SixtyBus_ReadDouble(uint32_t pa, uint64_t* reg)
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
        *reg = _BYTESWAP_UINT64(*(uint64_t*)buf);
    }

    void SixtyBus_WriteDouble(uint32_t pa, uint64_t* data)
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
        *(uint64_t*)buf = _BYTESWAP_UINT64(*data);
    }

    void SixtyBus_ReadBurst(uint32_t phys_addr, uint8_t burstData[32])
    {
        PIReadBurst(phys_addr, burstData);
    }

    void SixtyBus_WriteBurst(uint32_t phys_addr, uint8_t burstData[32])
    {
        PIWriteBurst(phys_addr, burstData);
    }
}

void PIReadBurst(uint32_t phys_addr, uint8_t burstData[32])
{
    if ((phys_addr + 32) > RAMSIZE)
        return;

    memcpy(burstData, &mi.ram[phys_addr], 32);
}

void PIWriteBurst(uint32_t phys_addr, uint8_t burstData[32])
{
    if (phys_addr == PI_REGSPACE_GX_FIFO)
    {
        Flipper::Gx->FifoWriteBurst(burstData);
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
    Halt("PI: Unhandled HW access:  R8 %08X", addr);
}

static void def_hw_write8(uint32_t addr, uint32_t data)
{
    Halt("PI: Unhandled HW access:  W8 %08X = %02X", addr, (uint8_t)data);
}

static void def_hw_read16(uint32_t addr, uint32_t* reg)
{
    Halt("PI: Unhandled HW access: R16 %08X", addr);
}

static void def_hw_write16(uint32_t addr, uint32_t data)
{
    Halt("PI: Unhandled HW access: W16 %08X = %04X", addr, (uint16_t)data);
}

static void def_hw_read32(uint32_t addr, uint32_t* reg)
{
    Halt("PI: Unhandled HW access: R32 %08X", addr);
}

static void def_hw_write32(uint32_t addr, uint32_t data)
{
    Halt("PI: Unhandled HW access: W32 %08X = %08X", addr, data);
}

// ---------------------------------------------------------------------------
// traps API

static void PISetTrap8(
    uint32_t addr,
    void (*rdTrap)(uint32_t, uint32_t*),
    void (*wrTrap)(uint32_t, uint32_t))
{
    if (rdTrap == NULL) rdTrap = def_hw_read8;
    if (wrTrap == NULL) wrTrap = def_hw_write8;

    hw_read8[addr & 0xffff] = rdTrap;
    hw_write8[addr & 0xffff] = wrTrap;
}

static void PISetTrap16(
    uint32_t addr,
    void (*rdTrap)(uint32_t, uint32_t*),
    void (*wrTrap)(uint32_t, uint32_t))
{
    if (rdTrap == NULL) rdTrap = def_hw_read16;
    if (wrTrap == NULL) wrTrap = def_hw_write16;

    hw_read16[addr & 0xfffe] = rdTrap;
    hw_write16[addr & 0xfffe] = wrTrap;
}

static void PISetTrap32(
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
void PISetTrap(
    uint32_t type,                               // 8, 16 or 32
    uint32_t addr,                               // physical trap address
    void (*rdTrap)(uint32_t, uint32_t*),  // register read trap
    void (*wrTrap)(uint32_t, uint32_t))    // register write trap
{
    // address must be in correct range
    if (!((addr >= HW_BASE) && (addr < (HW_BASE + HW_MAX_KNOWN))))
    {
        Halt(
            "PI: Trap address is out of GAMECUBE registers range.\n"
            "address : %08X\n", addr
        );
    }

    // select trap type
    switch (type)
    {
        case 8:
            PISetTrap8(addr, rdTrap, wrTrap);
            break;
        case 16:
            PISetTrap16(addr, rdTrap, wrTrap);
            break;
        case 32:
            PISetTrap32(addr, rdTrap, wrTrap);
            break;

            // should never happen
        default:
            throw "PI: Unknown trap type";
    }
}

// Set default traps for any access, called every time when emu restarted
static void PIClearTraps()
{
    uint32_t addr;

    // possible errors, if greater 0xffff
    assert(HW_MAX_KNOWN < 0x10000);

    // for 8-bit registers
    for (addr = HW_BASE; addr < (HW_BASE + HW_MAX_KNOWN); addr++)
    {
        PISetTrap8(addr, NULL, NULL);
    }

    // for 16-bit registers
    for (addr = HW_BASE; addr < (HW_BASE + HW_MAX_KNOWN); addr += 2)
    {
        PISetTrap16(addr, NULL, NULL);
    }

    // for 32-bit registers
    for (addr = HW_BASE; addr < (HW_BASE + HW_MAX_KNOWN); addr += 4)
    {
        PISetTrap32(addr, NULL, NULL);
    }
}

#pragma endregion "Processor Memory Interface"

// ---------------------------------------------------------------------------
// interrupts

// return short interrupt description
static const char *intdesc(uint32_t mask)
{
    switch(mask & 0xffff)
    {
        case PI_INTERRUPT_HSP       : return "HSP";
        case PI_INTERRUPT_DEBUG     : return "DEBUG";
        case PI_INTERRUPT_CP        : return "CP";
        case PI_INTERRUPT_PE_FINISH : return "PE_FINISH";
        case PI_INTERRUPT_PE_TOKEN  : return "PE_TOKEN";
        case PI_INTERRUPT_VI        : return "VI";
        case PI_INTERRUPT_MEM       : return "MEM";
        case PI_INTERRUPT_DSP       : return "DSP";
        case PI_INTERRUPT_AI        : return "AI";
        case PI_INTERRUPT_EXI       : return "EXI";
        case PI_INTERRUPT_SI        : return "SI";
        case PI_INTERRUPT_DI        : return "DI";
        case PI_INTERRUPT_RSW       : return "RSW";
        case PI_INTERRUPT_PI        : return "PI_ERROR";
    }
    
    // default
    return "UNKNOWN";
}

static void printOut(uint32_t mask, const char *fix)
{
    std::string buf;
    for(uint32_t m=1; m<=PI_INTERRUPT_HSP; m<<=1)
    {
        if (mask & m)
        {
            buf += intdesc(m);
            buf += "INT ";
        }
    }
    Report(Channel::PI, "%s%s (pc: %08X, time: 0x%llx)", buf.c_str(), fix, Core->regs.pc, Core->GetTicks());
}

// assert interrupt
void PIAssertInt(uint32_t mask)
{
    pi.intsr |= mask;
    if(pi.intmr & mask)
    {
        if (pi.log)
        {
            printOut(mask, "asserted");
        }
    }

#ifdef _LINUX
    PIInterruptSource intSource = (PIInterruptSource)(31 - __builtin_clz(mask));
#else
    PIInterruptSource intSource = (PIInterruptSource)(31 - __lzcnt(mask));
#endif
    pi.intCounters[(size_t)intSource]++;

    if (pi.intsr & pi.intmr)
    {
        Core->AssertInterrupt();
    }
    else
    {
        Core->ClearInterrupt();
    }
}

// clear interrupt
void PIClearInt(uint32_t mask)
{ 
    if(pi.intsr & mask)
    {
        if (pi.log)
        {
            printOut(mask, "cleared");
        }
    }
    pi.intsr &= ~mask;

    if (pi.intsr & pi.intmr)
    {
        Core->AssertInterrupt();
    }
    else
    {
        Core->ClearInterrupt();
    }
}

// ---------------------------------------------------------------------------
// traps for interrupt regs

static void read_intsr(uint32_t addr, uint32_t *reg)
{
    *reg = pi.intsr | ((pi.rswhack & 1) << 16);
}

// writes turns them off ?
static void write_intsr(uint32_t addr, uint32_t data)
{
    PIClearInt(data);
}

static void read_intmr(uint32_t addr, uint32_t *reg)
{
    *reg = pi.intmr;
}

static void write_intmr(uint32_t addr, uint32_t data)
{
    pi.intmr = data;

    // print out list of masked interrupts
    if(pi.intmr && pi.log)
    {
        std::string buf;
        for(uint32_t m=1; m<=PI_INTERRUPT_HSP; m<<=1)
        {
            if (pi.intmr & m)
            {
                buf += intdesc(m);
                buf += " ";
            }
        }

        Report(Channel::PI, "unmasked : %s\n", buf.c_str());
    }
}

//
// Flipper revision
//

static void read_FlipperID(uint32_t addr, uint32_t *reg)
{
    uint32_t ver = pi.consoleVer;

    // bootrom using this register in following way :
    //
    //  [8000002C] =  1                     // set machine type to retail1
    //  [8000002C] += [CC00302C] >> 28;     // upgrade revision

    // set to HW2 final production board 
    // we need to set [8000002C] with value 3 (retail3)
    // so return '2'
    *reg = ((ver & 0xf) - 1) << 28;
}

//
// CONFIG register
//

static void write_config(uint32_t addr, uint32_t data)
{
    if(data)
    {
        // It is not yet clear what may or may not be affected by the support for system reset, for now we will leave only debug messages.

        if (data & PI_CONFIG_SYSRSTB)
        {
            Report(Channel::PI, "System Reset requested.\n");
        }

        if (data & PI_CONFIG_MEMRSTB)
        {
            Report(Channel::PI, "MEM Reset requested.\n");
        }

        if (data & PI_CONFIG_DIRSTB)
        {
            Report(Channel::PI, "DVD Reset requested.\n");
        }

        // reset emulator
        //EMUClose();
        //EMUOpen();
    }
}

static void read_config(uint32_t addr, uint32_t *reg)
{
    // on system power-on, the code is zero
    *reg = 0;
}

//
// PI fifo (CPU)
//

static void PI_CPRegRead(uint32_t addr, uint32_t* reg)
{
    *reg = Flipper::Gx->PiCpReadReg((GX::PI_CPMappedRegister)((addr & 0xFF) >> 2));
}

static void PI_CPRegWrite(uint32_t addr, uint32_t data)
{
    Flipper::Gx->PiCpWriteReg((GX::PI_CPMappedRegister)((addr & 0xFF) >> 2), data);
}

// ---------------------------------------------------------------------------
// init

void PIOpen(HWConfig* config)
{
    Report(Channel::PI, "Processor interface\n");

    pi.rswhack = config->rswhack;
    pi.consoleVer = config->consoleVer;
    pi.log = false;

    // now any access will generate unhandled warning,
    // if emulator try to read or write register,
    // so we need to set traps for missing regs:

    // clear all traps
    PIClearTraps();

    // clear interrupt registers
    pi.intsr = pi.intmr = 0;

    // set interrupt registers hooks
    PISetTrap(32, PI_INTSR   , read_intsr, write_intsr);
    PISetTrap(32, PI_INTMR   , read_intmr, write_intmr);
    PISetTrap(32, PI_CHIPID  , read_FlipperID, nullptr);
    PISetTrap( 8, PI_CONFIG  , read_config, write_config);
    PISetTrap(32, PI_CONFIG  , read_config, write_config);

    // Processor interface CP fifo.
    // Some of the CP FIFO registers are mapped to PI registers for the reason that writes to the FIFO Stream Pointer are made by the Gekko Burst transactions and are serviced by PI.
    PISetTrap(32, PI_BASE , PI_CPRegRead, PI_CPRegWrite);
    PISetTrap(32, PI_TOP  , PI_CPRegRead, PI_CPRegWrite);
    PISetTrap(32, PI_WRPTR, PI_CPRegRead, PI_CPRegWrite);

    // Reset interrupt counters
    for (size_t i = 0; i < (size_t)PIInterruptSource::Max; i++)
    {
        pi.intCounters[i] = 0;
    }
}

void PIClose()
{
    PIClearTraps();
}
