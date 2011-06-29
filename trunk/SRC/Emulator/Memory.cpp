#include "dolphin.h"

// shared data
MEMControl mem;

// memory read/write routines (thats all you need for CPU)
void (__fastcall *MEMReadByte)(u32 addr, u32 *reg);     // load byte
void (__fastcall *MEMWriteByte)(u32 addr, u32 data);    // store byte
void (__fastcall *MEMReadHalf)(u32 addr, u32 *reg);     // load halfword
void (__fastcall *MEMReadHalfS)(u32 addr, u32 *reg);    // load signed halfword
void (__fastcall *MEMWriteHalf)(u32 addr, u32 data);    // store halfword
void (__fastcall *MEMReadWord)(u32 addr, u32 *reg);     // load word
void (__fastcall *MEMWriteWord)(u32 addr, u32 data);    // store word
void (__fastcall *MEMReadDouble)(u32 addr, u64 *reg);   // load doubleword
void (__fastcall *MEMWriteDouble)(u32 addr, u64 *data); // store doubleword
u32  (__fastcall *MEMFetch)(u32 addr);
u32  (__fastcall *MEMEffectiveToPhysical)(u32 ea, BOOL IR); // translate

// ---------------------------------------------------------------------------
// memory sub-system management (init stuff)

void MEMInit()
{
    if(mem.inited) return;

    // allocate main memory buffer
    RAM = (u8 *)malloc(RAMSIZE);
    ASSERT(RAM == NULL, "No space for main memory buffer.");

    #pragma message ("Hack : remove, when finish MMU tables")
    mem.mmudirect = 1;

    mem.inited = TRUE;
}

void MEMFini()
{
    if(!mem.inited) return;

    // destroy main memory
    if(RAM)
    {
        free(RAM);
        RAM = NULL;
    }

    mem.inited = FALSE;
}

void MEMOpen()
{
    ASSERT(!mem.inited, "Initialize memory first!");
    if(mem.opened) return;

    // get memory mode (simple by default)
    int mode = GetConfigInt(USER_MMU, USER_MMU_DEFAULT);

    // select memory mode
    MEMSelect(mode);

    // clear RAM
    memset(RAM, 0, RAMSIZE);

    mem.opened = TRUE;
}

void MEMClose()
{
    ASSERT(!mem.inited, "Initialize memory first!");
    if(!mem.opened) return;

    // release memory maps (if present)
    if(mem.imap)
    {
        free(mem.imap);
        mem.imap = NULL;
    }
    if(mem.dmap)
    {
        free(mem.dmap);
        mem.dmap = NULL;
    }

    mem.opened = FALSE;
}

// ---------------------------------------------------------------------------
// memory mapper engine

// forward referencies to memory operations :

// simple translation only for GC DolphinOS (good enough mostly)
void __fastcall GCReadByte(u32 addr, u32 *reg);
void __fastcall GCWriteByte(u32 addr, u32 data);
void __fastcall GCReadHalf(u32 addr, u32 *reg);
void __fastcall GCReadHalfS(u32 addr, u32 *reg);
void __fastcall GCWriteHalf(u32 addr, u32 data);
void __fastcall GCReadWord(u32 addr, u32 *reg);
void __fastcall GCWriteWord(u32 addr, u32 data);
void __fastcall GCReadDouble(u32 addr, u64 *reg);
void __fastcall GCWriteDouble(u32 addr, u64 *data);
u32  __fastcall GCFetch(u32 addr);
u32  __fastcall GCEffectiveToPhysical(u32 ea, BOOL IR=0);

// for PowerPC memory mapping (as on real, slow, very compatible)
void __fastcall MMUReadByte(u32 addr, u32 *reg);
void __fastcall MMUWriteByte(u32 addr, u32 data);
void __fastcall MMUReadHalf(u32 addr, u32 *reg);
void __fastcall MMUReadHalfS(u32 addr, u32 *reg);
void __fastcall MMUWriteHalf(u32 addr, u32 data);
void __fastcall MMUReadWord(u32 addr, u32 *reg);
void __fastcall MMUWriteWord(u32 addr, u32 data);
void __fastcall MMUReadDouble(u32 addr, u64 *reg);
void __fastcall MMUWriteDouble(u32 addr, u64 *data);
u32  __fastcall MMUFetch(u32 addr);
u32  __fastcall MMUEffectiveToPhysical(u32 ea, BOOL IR=0);

// select memory mode
void MEMSelect(u8 mode, BOOL save)
{
    // select translation mode
    if(mode)    // MMU
    {
        if(!mem.mmudirect)
        {
            // take care about memory map, if mode is MMU
            mem.imap = (u8 **)malloc(4*1024*1024);
            ASSERT(mem.imap, "Not enough memory for MMU instruction translation buffer.");
            mem.dmap = (u8 **)malloc(4*1024*1024);
            ASSERT(mem.dmap, "Not enough memory for MMU data translation buffer.");

            // clear lookups
            memset(mem.imap, 0, 4*1024*1024);
            memset(mem.dmap, 0, 4*1024*1024);
        }

        MEMRemapMemory(1, 1);

        // set advanced translation
        MEMReadByte            = MMUReadByte;
        MEMWriteByte           = MMUWriteByte;
        MEMReadHalf            = MMUReadHalf;
        MEMReadHalfS           = MMUReadHalfS;
        MEMWriteHalf           = MMUWriteHalf;
        MEMReadWord            = MMUReadWord;
        MEMWriteWord           = MMUWriteWord;
        MEMReadDouble          = MMUReadDouble;
        MEMWriteDouble         = MMUWriteDouble;
        MEMFetch               = MMUFetch;
        MEMEffectiveToPhysical = MMUEffectiveToPhysical;
    }
    else        // simple
    {
        // release memory maps (if present)
        if(mem.imap)
        {
            free(mem.imap);
            mem.imap = NULL;
        }
        if(mem.dmap)
        {
            free(mem.dmap);
            mem.dmap = NULL;
        }

        // set simple translation
        MEMReadByte            = GCReadByte;
        MEMWriteByte           = GCWriteByte;
        MEMReadHalf            = GCReadHalf;
        MEMReadHalfS           = GCReadHalfS;
        MEMWriteHalf           = GCWriteHalf;
        MEMReadWord            = GCReadWord;
        MEMWriteWord           = GCWriteWord;
        MEMReadDouble          = GCReadDouble;
        MEMWriteDouble         = GCWriteDouble;
        MEMFetch               = GCFetch;
        MEMEffectiveToPhysical = GCEffectiveToPhysical;
    }

    // save variable
    if(save)
    {
        mem.mmu = mode;
        SetConfigInt(USER_MMU, mem.mmu);
    }
}

// ---------------------------------------------------------------------------
// simple translation (only for Dolphin OS). unlike 0.09, we are using C here.

u32 __fastcall GCEffectiveToPhysical(u32 ea, BOOL IR)
{
    if(!mem.opened) return -1;
    // ignore no memory, page faults, alignment, etc errors
    return ea & RAMMASK;        // thats all =:)
}

void __fastcall GCReadByte(u32 addr, u32 *reg)
{
    u32 pa = GCEffectiveToPhysical(addr);

    // hardware trap
    if(pa >= HW_BASE)
    {
        hw_read8[addr & 0xffff](addr, reg);
        return;
    }

    // embedded frame buffer
    if(pa >= EFB_BASE)
    {
        EFBPeek8(addr & EFB_MASK, reg);
        return;
    }

    // bus load byte
    u8 *ptr;
    if(addr >= 0xe0000000) ptr = &mem.lc[addr & 0x3ffff];
    else ptr = &RAM[pa];
    *reg = (u32)*ptr;
}

void __fastcall GCWriteByte(u32 addr, u32 data)
{
    u32 pa = GCEffectiveToPhysical(addr);

    // hardware trap
    if(pa >= HW_BASE)
    {
        hw_write8[addr & 0xffff](addr, (u8)data);
        return;
    }

    // embedded frame buffer
    if(pa >= EFB_BASE)
    {
        EFBPoke16(addr & EFB_MASK, data);
        return;
    }

    // bus store byte
    u8 *ptr;
    if(addr >= 0xe0000000) ptr = &mem.lc[addr & 0x3ffff];
    else ptr = &RAM[pa];
    *ptr = (u8)data;
}

void __fastcall GCReadHalf(u32 addr, u32 *reg)
{
    u32 pa = GCEffectiveToPhysical(addr);

    // hardware trap
    if(pa >= HW_BASE)
    {
        hw_read16[addr & 0xfffe](addr, reg);
        return;
    }

    // embedded frame buffer
    if(pa >= EFB_BASE)
    {
        EFBPeek16(addr & EFB_MASK, reg);
        return;
    }

    // bus load halfword
    u8 *ptr;
    if(addr >= 0xe0000000) ptr = &mem.lc[addr & 0x3ffff];
    else ptr = &RAM[pa];
    *reg = (u32)MEMSwapHalf(*(u16 *)ptr);
}

void __fastcall GCReadHalfS(u32 addr, u32 *reg)
{
    u32 pa = GCEffectiveToPhysical(addr);

    // hardware trap
    if(pa >= HW_BASE)
    {
        hw_read16[addr & 0xfffe](addr, reg);
        if(*reg & 0x8000) *reg |= 0xffff0000;
        return;
    }

    // embedded frame buffer
    if(pa >= EFB_BASE)
    {
        EFBPeek16(addr & EFB_MASK, reg);
        if(*reg & 0x8000) *reg |= 0xffff0000;
        return;
    }
    
    // bus load halfword signed
    u8 *ptr;
    if(addr >= 0xe0000000) ptr = &mem.lc[addr & 0x3ffff];
    else ptr = &RAM[pa];
    *reg = MEMSwapHalf(*(u16 *)ptr);
    if(*reg & 0x8000) *reg |= 0xffff0000;
}

void __fastcall GCWriteHalf(u32 addr, u32 data)
{
    u32 pa = GCEffectiveToPhysical(addr);

    // hardware trap
    if(pa >= HW_BASE)
    {
        hw_write16[addr & 0xfffe](addr, data);
        return;
    }

    // embedded frame buffer
    if(pa >= EFB_BASE)
    {
        EFBPoke16(addr & EFB_MASK, data);
        return;
    }

    // bus store halfword
    u8 *ptr;
    if(addr >= 0xe0000000) ptr = &mem.lc[addr & 0x3ffff];
    else ptr = &RAM[pa];
    *(u16 *)ptr = MEMSwapHalf((u16)data);
}

void __fastcall GCReadWord(u32 addr, u32 *reg)
{
    u32 pa = GCEffectiveToPhysical(addr);

    // hardware trap
    if(pa >= HW_BASE)
    {
        hw_read32[addr & 0xfffc](addr, reg);
        return;
    }

    // embedded frame buffer
    if(pa >= EFB_BASE)
    {
        EFBPeek32(addr & EFB_MASK, reg);
        return;
    }

    // bus load word
    u8 *ptr;
    if(addr >= 0xe0000000) ptr = &mem.lc[addr & 0x3ffff];
    else ptr = &RAM[pa];
    *reg = MEMSwap(*(u32 *)ptr);
}

void __fastcall GCWriteWord(u32 addr, u32 data)
{
    u32 pa = GCEffectiveToPhysical(addr);

    // hardware trap
    if(pa >= HW_BASE)
    {
        hw_write32[addr & 0xfffc](addr, data);
        return;
    }

    // embedded frame buffer
    if(pa >= EFB_BASE)
    {
        EFBPoke32(addr & EFB_MASK, data);
        return;
    }

    // bus store word
    u8 *ptr;
    if(addr >= 0xe0000000) ptr = &mem.lc[addr & 0x3ffff];
    else ptr = &RAM[pa];
    *(u32 *)ptr = MEMSwap(data);
}

//
// fortunately longlongs are never used in GC hardware access
// (because all regs are generally integers)
//

void __fastcall GCReadDouble(u32 addr, u64 *_reg)
{
    u32 pa = GCEffectiveToPhysical(addr);
    u8 *buf = &RAM[pa], *reg = (u8 *)_reg;

    if(addr >= 0xe0000000) buf = &mem.lc[addr & 0x3ffff];

    // bus load doubleword
    reg[0] = buf[7];
    reg[1] = buf[6];
    reg[2] = buf[5];
    reg[3] = buf[4];
    reg[4] = buf[3];
    reg[5] = buf[2];
    reg[6] = buf[1];
    reg[7] = buf[0];
}

void __fastcall GCWriteDouble(u32 addr, u64 *_data)
{
    u32 pa = GCEffectiveToPhysical(addr);
    u8 *buf = &RAM[pa], *data = (u8 *)_data;

    if(addr >= 0xe0000000) buf = &mem.lc[addr & 0x3ffff];

    // bus store doubleword
    buf[0] = data[7];
    buf[1] = data[6];
    buf[2] = data[5];
    buf[3] = data[4];
    buf[4] = data[3];
    buf[5] = data[2];
    buf[6] = data[1];
    buf[7] = data[0];
}

// fetch opcode (0.09 fetch accelerator is gone, hence we are using MMU)
// return 1, if cannot fetch (no memory)
u32 __fastcall GCFetch(u32 addr)
{
    u32 pa = GCEffectiveToPhysical(addr);
    if(pa >= RAMSIZE) return 1;

    // bus fetch instruction
    u8 *ptr = &RAM[pa];
    return MEMSwap(*(u32 *)ptr);
}

// ---------------------------------------------------------------------------
// PPC MMU simulation (used by GC-Linux and other advanced stuff).
// we are using memory map table, to speed up address translation.
// we should "remap" tables, after changing of some PPC system registers, or 
// page translation table. it is good place to remap "data" before load/store
// operation, and "instruction" before any "non-linear" PC change (i.e. branch,
// exception or like).

// direct-store facility (T=1) is in history now, so Dolwin doesnt support it, 
// like new software ;)

// we do not support access rights for BAT logic (in that case we must have two
// standalone lookup talbes for Load and Store operations).

static u32 *dbatu[4] = { &DBAT0U, &DBAT1U, &DBAT2U, &DBAT3U };
static u32 *dbatl[4] = { &DBAT0L, &DBAT1L, &DBAT2L, &DBAT3L };
static u32 *ibatu[4] = { &IBAT0U, &IBAT1U, &IBAT2U, &IBAT3U };
static u32 *ibatl[4] = { &IBAT0L, &IBAT1L, &IBAT2L, &IBAT3L };

u32 __fastcall MMUEffectiveToPhysical(u32 ea, BOOL IR)
{
    u32 pa;
    if(!mem.opened) return -1;

    // ea = effective address
    // pa = physical address
    // pn = page number

    // use translation lookup table
    if(!mem.mmudirect)
    {
        u8 *pn;
        if(IR) pn = mem.imap[ea >> 12];
        else pn = mem.dmap[ea >> 12];
        if(pn)
        {
            u8 *ptr = &pn[ea & 4095];
            return (u32)(ptr - RAM);
        }
        else return -1;
    }

    // perform direct translation
    if(IR)
    {
        if(!(MSR & MSR_IR)) return pa = ea;

        for(s32 n=0; n<4; n++)
        {
            u32 bepi = BATBEPI(*ibatu[n]);
            u32 bl   = BATBL(*ibatu[n]);
            u32 tst  = (ea >> 17) & (0x7800 | ~bl);
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

        for(s32 n=0; n<4; n++)
        {
            u32 bepi = BATBEPI(*dbatu[n]);
            u32 bl   = BATBL(*dbatu[n]);
            u32 tst  = (ea >> 17) & (0x7800 | ~bl);
            if(bepi == tst)
            {
                pa = BATBRPN(*dbatl[n]) | ((ea >> 17) & bl);
                pa = (pa << 17) | (ea & 0x1ffff);
                return pa;
            }
        }
    }

/*/
    DolwinError( 
        "Memory Mapping Unit",
        "Cannot translate effective address : %08X\n"
        "IR:%i DR:%i pc:%08X\n"
        "Implement segment page translation logic!", 
        ea, (MSR & MSR_IR) ? 1 : 0, (MSR & MSR_DR) ? 1 : 0, PC
    );
/*/

    return -1;
}

void __fastcall MMUReadByte(u32 addr, u32 *reg)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        u32 pa = MMUEffectiveToPhysical(addr);
        if(pa == -1) return;

        // hardware trap
        if(pa >= HW_BASE)
        {
            hw_read8[addr & 0xffff](addr, reg);
            return;
        }

        // embedded frame buffer
        if(pa >= EFB_BASE)
        {
            EFBPeek8(addr & EFB_MASK, reg);
            return;
        }

        // bus load byte
        u8 *ptr = &RAM[pa];
        *reg = (u32)*ptr;
    }
    else GCReadByte(addr, reg);
}

void __fastcall MMUWriteByte(u32 addr, u32 data)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        u32 pa = MMUEffectiveToPhysical(addr);
        if(pa == -1) return;

        // hardware trap
        if(pa >= HW_BASE)
        {
            hw_write8[addr & 0xffff](addr, (u8)data);
            return;
        }

        // embedded frame buffer
        if(pa >= EFB_BASE)
        {
            EFBPoke8(addr & EFB_MASK, data);
            return;
        }

        // bus store byte
        u8 *ptr = &RAM[pa];
        *ptr = (u8)data;
    }
    else GCWriteByte(addr, data);
}

void __fastcall MMUReadHalf(u32 addr, u32 *reg)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        u32 pa = MMUEffectiveToPhysical(addr);
        if(pa == -1) return;

        // hardware trap
        if(pa >= HW_BASE)
        {
            hw_read16[addr & 0xfffe](addr, reg);
            return;
        }

        // embedded frame buffer
        if(pa >= EFB_BASE)
        {
            EFBPeek16(addr & EFB_MASK, reg);
            return;
        }

        // bus load halfword
        u8 *ptr = &RAM[pa];
        *reg = (u32)MEMSwapHalf(*(u16 *)ptr);
    }
    else GCReadHalf(addr, reg);
}

void __fastcall MMUReadHalfS(u32 addr, u32 *reg)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        u32 pa = MMUEffectiveToPhysical(addr);
        if(pa == -1) return;

        // hardware trap
        if(pa >= HW_BASE)
        {
            hw_read16[addr & 0xfffe](addr, reg);
            if(*reg & 0x8000) *reg |= 0xffff0000;
            return;
        }

        // embedded frame buffer
        if(pa >= EFB_BASE)
        {
            EFBPeek16(addr & EFB_MASK, reg);
            if(*reg & 0x8000) *reg |= 0xffff0000;
            return;
        }
        
        // bus load halfword signed
        u8 *ptr = &RAM[pa];
        *reg = MEMSwapHalf(*(u16 *)ptr);
        if(*reg & 0x8000) *reg |= 0xffff0000;
    }
    else GCReadHalfS(addr, reg);
}

void __fastcall MMUWriteHalf(u32 addr, u32 data)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        u32 pa = MMUEffectiveToPhysical(addr);
        if(pa == -1) return;

        // hardware trap
        if(pa >= HW_BASE)
        {
            hw_write16[addr & 0xfffe](addr, data);
            return;
        }

        // embedded frame buffer
        if(pa >= EFB_BASE)
        {
            EFBPoke16(addr & EFB_MASK, data);
            return;
        }

        // bus store halfword
        u8 *ptr = &RAM[pa];
        *(u16 *)ptr = MEMSwapHalf((u16)data);
    }
    else GCWriteHalf(addr, data);
}

void __fastcall MMUReadWord(u32 addr, u32 *reg)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        u32 pa = MMUEffectiveToPhysical(addr);
        if(pa == -1) return;

        // hardware trap
        if(pa >= HW_BASE)
        {
            hw_read32[addr & 0xfffc](addr, reg);
            return;
        }

        // embedded frame buffer
        if(pa >= EFB_BASE)
        {
            EFBPeek32(addr & EFB_MASK, reg);
            return;
        }

        // bus load word
        u8 *ptr = &RAM[pa];
        *reg = MEMSwap(*(u32 *)ptr);
    }
    else GCReadWord(addr, reg);
}

void __fastcall MMUWriteWord(u32 addr, u32 data)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        u32 pa = MMUEffectiveToPhysical(addr);
        if(pa == -1) return;

        // hardware trap
        if(pa >= HW_BASE)
        {
            hw_write32[addr & 0xfffc](addr, data);
            return;
        }

        // embedded frame buffer
        if(pa >= EFB_BASE)
        {
            EFBPoke32(addr & EFB_MASK, data);
            return;
        }

        // bus store word
        u8 *ptr = &RAM[pa];
        *(u32 *)ptr = MEMSwap(data);
    }
    else GCWriteWord(addr, data);
}

//
// fortunately longlongs are never used in GC hardware access
// (because all regs are generally integers)
//

void __fastcall MMUReadDouble(u32 addr, u64 *_reg)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        u32 pa = MMUEffectiveToPhysical(addr);
        if(pa == -1) return;
        u8 *buf = &RAM[pa], *reg = (u8 *)_reg;

        // bus load doubleword
        reg[0] = buf[7];
        reg[1] = buf[6];
        reg[2] = buf[5];
        reg[3] = buf[4];
        reg[4] = buf[3];
        reg[5] = buf[2];
        reg[6] = buf[1];
        reg[7] = buf[0];
    }
    else GCReadDouble(addr, _reg);
}

void __fastcall MMUWriteDouble(u32 addr, u64 *_data)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        u32 pa = MMUEffectiveToPhysical(addr);
        if(pa == -1) return;
        u8 *buf = &RAM[pa], *data = (u8 *)_data;

        // bus store doubleword
        buf[0] = data[7];
        buf[1] = data[6];
        buf[2] = data[5];
        buf[3] = data[4];
        buf[4] = data[3];
        buf[5] = data[2];
        buf[6] = data[1];
        buf[7] = data[0];
    }
    else GCWriteDouble(addr, _data);
}

// fetch opcode (0.09 fetch accelerator is gone, hence we are using MMU)
// return 1, if cannot fetch (no memory)
u32 __fastcall MMUFetch(u32 addr)
{
    if(MSR & MSR_IR)
    {
        u32 pa = MMUEffectiveToPhysical(addr, 1);
        if(pa >= RAMSIZE) return 1;

        // bus fetch instruction
        u8 *ptr = &RAM[pa];
        return MEMSwap(*(u32 *)ptr);
    }
    else return GCFetch(addr);
}

// map specified region of memory
void MEMMap(BOOL IR, BOOL DR, u32 startEA, u32 startPA, u32 length)
{
    u32 ea = startEA;
    u32 pa = startPA;
    u32 sz = ((length + 4095) & ~4095); // page round up

    if(emu.doldebug)
    {
        DBReport( YEL "mapping [I%i][D%i] %08X->%08X, %s\n",
                  IR, DR, ea, pa, FileSmartSize(sz) );
    }

    while(sz)
    {
        if(pa >= RAMSIZE) break;
        if(IR) mem.imap[ea >> 12] = &RAM[pa & ~4095];
        if(DR) mem.dmap[ea >> 12] = &RAM[pa & ~4095];
        ea += 4096;
        pa += 4096;
        sz -= 4096;
    }
}

// do actual remapping
void MEMDoRemap(BOOL IR, BOOL DR)
{
    // IMPLEMENT !!
}

// set to remap imap/dmap before next opcode/ldst operation
void MEMRemapMemory(BOOL IR, BOOL DR)
{
    if(mem.mmudirect) return;
    mem.ir = IR;
    if(emu.doldebug && IR) DBReport( YEL "remapping instruction MMU logic..\n");
    mem.dr = DR;
    if(emu.doldebug && DR) DBReport( YEL "remapping data MMU logic..\n");
}

// ---------------------------------------------------------------------------
// swap endianness

// assembly, because this functions is used in speed applications
__declspec(naked) u32 __fastcall MEMSwap(u32 data)
{
    __asm   bswap   ecx
    __asm   mov     eax, ecx
    __asm   ret
}

// assembly, because this functions is used in speed applications
__declspec(naked) u16 __fastcall MEMSwapHalf(u16 data)
{ 
    __asm   xchg    ch, cl
    __asm   mov     eax, ecx
    __asm   and     eax, 0xffff
    __asm   ret
}

// swap longs (no need in assembly, used by tools)
void __fastcall MEMSwapArea(u32 *addr, s32 count)
{
    u32 *until = addr + count / sizeof(u32);

    while(addr != until)
    {
        *addr = MEMSwap(*addr);
        addr++;
    }
}

// swap shorts (no need in assembly, used by tools)
void __fastcall MEMSwapAreaHalf(u16 *addr, s32 count)
{
    u16 *until = addr + count / sizeof(u16);

    while(addr != until)
    {
        *addr = MEMSwapHalf(*addr);
        addr++;
    }    
}
