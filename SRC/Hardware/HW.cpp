// hardware init/update code; register traps (for memory engine).
// IMPORTANT : whole HW should use physical CPU addressing, not effective!
#include "dolphin.h"

// hardware traps tables, shared to memory engine.
// there is no need in 64-bit traps, phew =:)
void (__fastcall *hw_read8  [HW_MAX_KNOWN])(uint32_t, uint32_t *);
void (__fastcall *hw_write8 [HW_MAX_KNOWN])(uint32_t, uint32_t);
void (__fastcall *hw_read16 [HW_MAX_KNOWN])(uint32_t, uint32_t *);
void (__fastcall *hw_write16[HW_MAX_KNOWN])(uint32_t, uint32_t);
void (__fastcall *hw_read32 [HW_MAX_KNOWN])(uint32_t, uint32_t *);
void (__fastcall *hw_write32[HW_MAX_KNOWN])(uint32_t, uint32_t);

static BOOL hw_assert;      // assert on not implemented HW in non DEBUG
static BOOL update;         // 1: HW update enabled

// ---------------------------------------------------------------------------
// default hardware R/W operations.
// emulation is halted on unknown register access, if hw_assert = 1

static void __fastcall def_hw_read8(uint32_t addr, uint32_t *reg)
{
    if(emu.doldebug)
    {
        DBHalt("unhandled HW access :  R8 %08X, (pc:%08X)\n", addr, PC);
    }
    else
    {
        hw_assert = GetConfigInt(USER_HW_ASSERT, USER_HW_ASSERT_DEFAULT);

        if(hw_assert) DolwinError( APPNAME " Hardware Not Implemented",
                                   "unhandled HW access :  R8 %08X (pc:%08X)", addr, PC );
        else *reg = 0;
    }
}

static void __fastcall def_hw_write8(uint32_t addr, uint32_t data)
{
    if(emu.doldebug)
    {
        DBHalt("unhandled HW access :  W8 %08X = %02X, (pc:%08X)\n", addr, (uint8_t)data, PC);
    }
    else
    {
        hw_assert = GetConfigInt(USER_HW_ASSERT, USER_HW_ASSERT_DEFAULT);

        if(hw_assert) DolwinError( APPNAME " Hardware Not Implemented",
                                   "unhandled HW access :  W8 %08X = %02X (pc:%08X)", addr, (uint8_t)data, PC );
    }
}

static void __fastcall def_hw_read16(uint32_t addr, uint32_t *reg)
{
    if(emu.doldebug)
    {
        DBHalt("unhandled HW access : R16 %08X, (pc:%08X)\n", addr, PC);
    }
    else
    {
        hw_assert = GetConfigInt(USER_HW_ASSERT, USER_HW_ASSERT_DEFAULT);

        if(hw_assert) DolwinError( APPNAME " Hardware Not Implemented",
                                   "unhandled HW access : R16 %08X (pc:%08X)", addr, PC );
        else *reg = 0;
    }
}

static void __fastcall def_hw_write16(uint32_t addr, uint32_t data)
{
    if(emu.doldebug)
    {
        DBHalt("unhandled HW access : W16 %08X = %04X, (pc:%08X)\n", addr, (uint16_t)data, PC);
    }
    else
    {
        hw_assert = GetConfigInt(USER_HW_ASSERT, USER_HW_ASSERT_DEFAULT);

        if(hw_assert) DolwinError( APPNAME " Hardware Not Implemented",
                                   "unhandled HW access : W16 %08X = %04X (pc:%08X)", addr, (uint16_t)data, PC );
    }
}

static void __fastcall def_hw_read32(uint32_t addr, uint32_t *reg)
{
    if(emu.doldebug)
    {
        DBHalt("unhandled HW access : R32 %08X, (pc:%08X)\n", addr, PC);
    }
    else
    {
        hw_assert = GetConfigInt(USER_HW_ASSERT, USER_HW_ASSERT_DEFAULT);

        if(hw_assert) DolwinError( APPNAME " Hardware Not Implemented",
                                   "unhandled HW access : R32 %08X (pc:%08X)", addr, PC );
        else *reg = 0;
    }
}

static void __fastcall def_hw_write32(uint32_t addr, uint32_t data)
{
    if(emu.doldebug)
    {
        DBHalt("unhandled HW access : W32 %08X = %08X, (pc:%08X)\n", addr, data, PC);
    }
    else
    {
        hw_assert = GetConfigInt(USER_HW_ASSERT, USER_HW_ASSERT_DEFAULT);

        if(hw_assert) DolwinError( APPNAME " Hardware Not Implemented",
                                   "unhandled HW access : W32 %08X = %08X (pc:%08X)", addr, data, PC );
    }
}

// ---------------------------------------------------------------------------
// traps API

static void HWSetTrap8(
    uint32_t addr,
    void (__fastcall *rdTrap)(uint32_t, uint32_t *),
    void (__fastcall *wrTrap)(uint32_t, uint32_t))
{
    if(rdTrap == NULL) rdTrap = def_hw_read8;
    if(wrTrap == NULL) wrTrap = def_hw_write8;

    hw_read8[addr & 0xffff] = rdTrap;
    hw_write8[addr & 0xffff] = wrTrap;
}

static void HWSetTrap16(
    uint32_t addr,
    void (__fastcall *rdTrap)(uint32_t, uint32_t *),
    void (__fastcall *wrTrap)(uint32_t, uint32_t))
{
    if(rdTrap == NULL) rdTrap = def_hw_read16;
    if(wrTrap == NULL) wrTrap = def_hw_write16;

    hw_read16[addr & 0xfffe] = rdTrap;
    hw_write16[addr & 0xfffe] = wrTrap;
}

static void HWSetTrap32(
    uint32_t addr,
    void (__fastcall *rdTrap)(uint32_t, uint32_t *),
    void (__fastcall *wrTrap)(uint32_t, uint32_t))
{
    if(rdTrap == NULL) rdTrap = def_hw_read32;
    if(wrTrap == NULL) wrTrap = def_hw_write32;

    hw_read32[addr & 0xfffc] = rdTrap;
    hw_write32[addr & 0xfffc] = wrTrap;
}

// wrapper
void HWSetTrap(
    uint32_t type,                               // 8, 16 or 32
    uint32_t addr,                               // physical trap address
    void (__fastcall *rdTrap)(uint32_t, uint32_t *),  // register read trap
    void (__fastcall *wrTrap)(uint32_t, uint32_t))    // register write trap
{
    // address must be in correct range
    if(!( (addr >= HW_BASE) && (addr < (HW_BASE + HW_MAX_KNOWN)) ))
    {
        DolwinError(
            APPNAME " Hardware sub-system error",
            "Trap address is out of GAMECUBE registers range.\n"
            "address : %08X\n", addr
        );
    }

    // select trap type
    switch(type)
    {
        case 8:
            HWSetTrap8(addr, rdTrap, wrTrap);
            break;
        case 16:
            HWSetTrap16(addr, rdTrap, wrTrap);
            break;
        case 32:
            HWSetTrap32(addr, rdTrap, wrTrap);
            break;

        // should never happen
        default:
            DolwinError(
                APPNAME " Hardware sub-system error",
                "Unknown trap type : %u (%08X)",
                type, addr
            );
    }
}

// set default traps for any access,
// called every time when emu restarted
static void HWClearTraps()
{
    register uint32_t addr;

    // possible errors, if greater 0xffff
    ASSERT( HW_MAX_KNOWN > 0xffff, 
            "HW_MAX_KNOWN must be below or equal to 0xffff." );

    // for 8-bit registers
    for(addr=HW_BASE; addr<(HW_BASE + HW_MAX_KNOWN); addr++)
    {
        HWSetTrap8(addr, NULL, NULL);
    }

    // for 16-bit registers
    for(addr=HW_BASE; addr<(HW_BASE + HW_MAX_KNOWN); addr+=2)
    {
        HWSetTrap16(addr, NULL, NULL);
    }

    // for 32-bit registers
    for(addr=HW_BASE; addr<(HW_BASE + HW_MAX_KNOWN); addr+=4)
    {
        HWSetTrap32(addr, NULL, NULL);
    }
}

// ---------------------------------------------------------------------------
// init and update

void HWOpen()
{
    DBReport(
        GREEN "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n"
        GREEN "Hardware Initialization.\n"
        GREEN "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\n"
    );

    // clear all traps
    HWClearTraps();

    // now any access will generate unhandled warning,
    // if emulator try to read or write register,
    // so we need to set traps for missing regs :

    VIOpen();       // video (TV)
    CPOpen();       // fifo
    MIOpen();       // memory protection
    AIOpen();       // audio (AID and AIS)
    AROpen();       // aux. memory (ARAM)
    EIOpen();       // expansion interface (EXI)
    DIOpen();       // disk
    SIOpen();       // GC controllers
    PIOpen();       // interrupts, console regs

    HWEnableUpdate(1);

    DBReport("\n");
}

void HWClose()
{
    ARClose();      // release ARAM
    EIClose();      // take care about closing of memcards and BBA
    VIClose();      // close GDI (if opened)
    DIClose();      // release streaming buffer

    HWClearTraps();
}

// update hardware counters/streams/any time-related tasks;
// we are using OS time (TBR), as counter basis.
void HWUpdate()
{
    if(update)
    {
        // check for pending interrupts
        PICheckInterrupts();

        // update joypads and video
        VIUpdate();     // PADs are updated there (SIPoll)
        CPUpdate();     // GX fifo

        // update audio and DSP
        BeginProfileSfx();
        AIUpdate();
        DSPUpdate();
        EndProfileSfx();

        //DBReport(YEL "*** HW UPDATE *** (%s)\n", OSTimeFormat(UTBR, 1));
    }
}

// allow/disallow HW update
void HWEnableUpdate(BOOL en)
{
    update = en;
    if(MEGA_HLE_MODE) update = 0;
}
