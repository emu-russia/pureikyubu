#include "dolphin.h"

// shared data
MEMControl mem;

// memory read/write routines (thats all you need for CPU)
void (__fastcall *MEMReadByte)(uint32_t addr, uint32_t *reg);     // load byte
void (__fastcall *MEMWriteByte)(uint32_t addr, uint32_t data);    // store byte
void (__fastcall *MEMReadHalf)(uint32_t addr, uint32_t *reg);     // load halfword
void (__fastcall *MEMReadHalfS)(uint32_t addr, uint32_t *reg);    // load signed halfword
void (__fastcall *MEMWriteHalf)(uint32_t addr, uint32_t data);    // store halfword
void (__fastcall *MEMReadWord)(uint32_t addr, uint32_t *reg);     // load word
void (__fastcall *MEMWriteWord)(uint32_t addr, uint32_t data);    // store word
void (__fastcall *MEMReadDouble)(uint32_t addr, uint64_t *reg);   // load doubleword
void (__fastcall *MEMWriteDouble)(uint32_t addr, uint64_t *data); // store doubleword
uint32_t (__fastcall *MEMFetch)(uint32_t addr);
uint32_t (__fastcall *MEMEffectiveToPhysical)(uint32_t ea, BOOL IR); // translate

// ---------------------------------------------------------------------------
// memory sub-system management (init stuff)

void MEMInit()
{
    if(mem.inited) return;

    // allocate main memory buffer
    RAM = (uint8_t *)malloc(RAMSIZE);
    VERIFY(RAM == NULL, "No space for main memory buffer.");

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
    VERIFY(!mem.inited, "Initialize memory first!");
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
    VERIFY(!mem.inited, "Initialize memory first!");
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
void __fastcall GCReadByte(uint32_t addr, uint32_t *reg);
void __fastcall GCWriteByte(uint32_t addr, uint32_t data);
void __fastcall GCReadHalf(uint32_t addr, uint32_t *reg);
void __fastcall GCReadHalfS(uint32_t addr, uint32_t *reg);
void __fastcall GCWriteHalf(uint32_t addr, uint32_t data);
void __fastcall GCReadWord(uint32_t addr, uint32_t *reg);
void __fastcall GCWriteWord(uint32_t addr, uint32_t data);
void __fastcall GCReadDouble(uint32_t addr, uint64_t *reg);
void __fastcall GCWriteDouble(uint32_t addr, uint64_t *data);
uint32_t  __fastcall GCFetch(uint32_t addr);
uint32_t  __fastcall GCEffectiveToPhysical(uint32_t ea, BOOL IR=0);

// for PowerPC memory mapping (as on real, slow, very compatible)
void __fastcall MMUReadByte(uint32_t addr, uint32_t *reg);
void __fastcall MMUWriteByte(uint32_t addr, uint32_t data);
void __fastcall MMUReadHalf(uint32_t addr, uint32_t *reg);
void __fastcall MMUReadHalfS(uint32_t addr, uint32_t *reg);
void __fastcall MMUWriteHalf(uint32_t addr, uint32_t data);
void __fastcall MMUReadWord(uint32_t addr, uint32_t *reg);
void __fastcall MMUWriteWord(uint32_t addr, uint32_t data);
void __fastcall MMUReadDouble(uint32_t addr, uint64_t *reg);
void __fastcall MMUWriteDouble(uint32_t addr, uint64_t *data);
uint32_t  __fastcall MMUFetch(uint32_t addr);
uint32_t  __fastcall MMUEffectiveToPhysical(uint32_t ea, BOOL IR=0);

// select memory mode
void MEMSelect(int mode, BOOL save)
{
    // select translation mode
    if(mode)    // MMU
    {
        if(!mem.mmudirect)
        {
            // take care about memory map, if mode is MMU
            mem.imap = (uint8_t **)malloc(4*1024*1024);
            VERIFY(mem.imap, "Not enough memory for MMU instruction translation buffer.");
            mem.dmap = (uint8_t **)malloc(4*1024*1024);
            VERIFY(mem.dmap, "Not enough memory for MMU data translation buffer.");

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

uint32_t __fastcall GCEffectiveToPhysical(uint32_t ea, BOOL IR)
{
    if(!mem.opened) return -1;
    // ignore no memory, page faults, alignment, etc errors
    return ea & RAMMASK;        // thats all =:)
}

void __fastcall GCReadByte(uint32_t addr, uint32_t*reg)
{
    uint32_t pa = GCEffectiveToPhysical(addr);

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
    uint8_t *ptr;
    if(addr >= 0xe0000000) ptr = &mem.lc[addr & 0x3ffff];
    else ptr = &RAM[pa];
    *reg = (uint32_t)*ptr;
}

void __fastcall GCWriteByte(uint32_t addr, uint32_t data)
{
    uint32_t pa = GCEffectiveToPhysical(addr);

    // hardware trap
    if(pa >= HW_BASE)
    {
        hw_write8[addr & 0xffff](addr, (uint8_t)data);
        return;
    }

    // embedded frame buffer
    if(pa >= EFB_BASE)
    {
        EFBPoke16(addr & EFB_MASK, data);
        return;
    }

    // bus store byte
    uint8_t *ptr;
    if(addr >= 0xe0000000) ptr = &mem.lc[addr & 0x3ffff];
    else ptr = &RAM[pa];
    *ptr = (uint8_t)data;
}

void __fastcall GCReadHalf(uint32_t addr, uint32_t *reg)
{
    uint32_t pa = GCEffectiveToPhysical(addr);

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
    uint8_t *ptr;
    if(addr >= 0xe0000000) ptr = &mem.lc[addr & 0x3ffff];
    else ptr = &RAM[pa];
    *reg = (uint32_t)MEMSwapHalf(*(uint16_t *)ptr);
}

void __fastcall GCReadHalfS(uint32_t addr, uint32_t *reg)
{
    uint32_t pa = GCEffectiveToPhysical(addr);

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
    uint8_t *ptr;
    if(addr >= 0xe0000000) ptr = &mem.lc[addr & 0x3ffff];
    else ptr = &RAM[pa];
    *reg = MEMSwapHalf(*(uint16_t *)ptr);
    if(*reg & 0x8000) *reg |= 0xffff0000;
}

void __fastcall GCWriteHalf(uint32_t addr, uint32_t data)
{
    uint32_t pa = GCEffectiveToPhysical(addr);

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
    uint8_t *ptr;
    if(addr >= 0xe0000000) ptr = &mem.lc[addr & 0x3ffff];
    else ptr = &RAM[pa];
    *(uint16_t *)ptr = MEMSwapHalf((uint16_t)data);
}

void __fastcall GCReadWord(uint32_t addr, uint32_t *reg)
{
    uint32_t pa = GCEffectiveToPhysical(addr);

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
    uint8_t *ptr;
    if(addr >= 0xe0000000) ptr = &mem.lc[addr & 0x3ffff];
    else ptr = &RAM[pa];
    *reg = MEMSwap(*(uint32_t *)ptr);
}

void __fastcall GCWriteWord(uint32_t addr, uint32_t data)
{
    uint32_t pa = GCEffectiveToPhysical(addr);

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
    uint8_t *ptr;
    if(addr >= 0xe0000000) ptr = &mem.lc[addr & 0x3ffff];
    else ptr = &RAM[pa];
    *(uint32_t *)ptr = MEMSwap(data);
}

//
// fortunately longlongs are never used in GC hardware access
// (because all regs are generally integers)
//

void __fastcall GCReadDouble(uint32_t addr, uint64_t *_reg)
{
    uint32_t pa = GCEffectiveToPhysical(addr);
    uint8_t *buf = &RAM[pa], *reg = (uint8_t *)_reg;

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

void __fastcall GCWriteDouble(uint32_t addr, uint64_t *_data)
{
    uint32_t pa = GCEffectiveToPhysical(addr);
    uint8_t *buf = &RAM[pa], *data = (uint8_t *)_data;

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
uint32_t __fastcall GCFetch(uint32_t addr)
{
    uint32_t pa = GCEffectiveToPhysical(addr);
    if(pa >= RAMSIZE) return 1;

    // bus fetch instruction
    uint8_t *ptr = &RAM[pa];
    return MEMSwap(*(uint32_t *)ptr);
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

static uint32_t *dbatu[4] = { &DBAT0U, &DBAT1U, &DBAT2U, &DBAT3U };
static uint32_t *dbatl[4] = { &DBAT0L, &DBAT1L, &DBAT2L, &DBAT3L };
static uint32_t *ibatu[4] = { &IBAT0U, &IBAT1U, &IBAT2U, &IBAT3U };
static uint32_t *ibatl[4] = { &IBAT0L, &IBAT1L, &IBAT2L, &IBAT3L };

uint32_t __fastcall MMUEffectiveToPhysical(uint32_t ea, BOOL IR)
{
    uint32_t pa;
    if(!mem.opened) return -1;

    // ea = effective address
    // pa = physical address
    // pn = page number

    // use translation lookup table
    if(!mem.mmudirect)
    {
        uint8_t *pn;
        if(IR) pn = mem.imap[ea >> 12];
        else pn = mem.dmap[ea >> 12];
        if(pn)
        {
            uint8_t *ptr = &pn[ea & 4095];
            return (uint32_t)(ptr - RAM);
        }
        else return -1;
    }

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

void __fastcall MMUReadByte(uint32_t addr, uint32_t *reg)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        uint32_t pa = MMUEffectiveToPhysical(addr);
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
        uint8_t *ptr = &RAM[pa];
        *reg = (uint32_t)*ptr;
    }
    else GCReadByte(addr, reg);
}

void __fastcall MMUWriteByte(uint32_t addr, uint32_t data)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        uint32_t pa = MMUEffectiveToPhysical(addr);
        if(pa == -1) return;

        // hardware trap
        if(pa >= HW_BASE)
        {
            hw_write8[addr & 0xffff](addr, (uint8_t)data);
            return;
        }

        // embedded frame buffer
        if(pa >= EFB_BASE)
        {
            EFBPoke8(addr & EFB_MASK, data);
            return;
        }

        // bus store byte
        uint8_t *ptr = &RAM[pa];
        *ptr = (uint8_t)data;
    }
    else GCWriteByte(addr, data);
}

void __fastcall MMUReadHalf(uint32_t addr, uint32_t *reg)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        uint32_t pa = MMUEffectiveToPhysical(addr);
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
        uint8_t *ptr = &RAM[pa];
        *reg = (uint32_t)MEMSwapHalf(*(uint16_t *)ptr);
    }
    else GCReadHalf(addr, reg);
}

void __fastcall MMUReadHalfS(uint32_t addr, uint32_t *reg)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        uint32_t pa = MMUEffectiveToPhysical(addr);
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
        uint8_t *ptr = &RAM[pa];
        *reg = MEMSwapHalf(*(uint16_t *)ptr);
        if(*reg & 0x8000) *reg |= 0xffff0000;
    }
    else GCReadHalfS(addr, reg);
}

void __fastcall MMUWriteHalf(uint32_t addr, uint32_t data)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        uint32_t pa = MMUEffectiveToPhysical(addr);
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
        uint8_t *ptr = &RAM[pa];
        *(uint16_t *)ptr = MEMSwapHalf((uint16_t)data);
    }
    else GCWriteHalf(addr, data);
}

void __fastcall MMUReadWord(uint32_t addr, uint32_t *reg)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        uint32_t pa = MMUEffectiveToPhysical(addr);
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
        uint8_t *ptr = &RAM[pa];
        *reg = MEMSwap(*(uint32_t *)ptr);
    }
    else GCReadWord(addr, reg);
}

void __fastcall MMUWriteWord(uint32_t addr, uint32_t data)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        uint32_t pa = MMUEffectiveToPhysical(addr);
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
        uint8_t *ptr = &RAM[pa];
        *(uint32_t *)ptr = MEMSwap(data);
    }
    else GCWriteWord(addr, data);
}

//
// fortunately longlongs are never used in GC hardware access
// (because all regs are generally integers)
//

void __fastcall MMUReadDouble(uint32_t addr, uint64_t *_reg)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        uint32_t pa = MMUEffectiveToPhysical(addr);
        if(pa == -1) return;
        uint8_t *buf = &RAM[pa], *reg = (uint8_t *)_reg;

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

void __fastcall MMUWriteDouble(uint32_t addr, uint64_t *_data)
{
    if(mem.dr)
    {
        MEMDoRemap(0, 1);
        mem.dr = 0;
    }
    if(MSR & MSR_DR)
    {
        uint32_t pa = MMUEffectiveToPhysical(addr);
        if(pa == -1) return;
        uint8_t *buf = &RAM[pa], *data = (uint8_t *)_data;

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
uint32_t __fastcall MMUFetch(uint32_t addr)
{
    if(MSR & MSR_IR)
    {
        uint32_t pa = MMUEffectiveToPhysical(addr, 1);
        if(pa >= RAMSIZE) return 1;

        // bus fetch instruction
        uint8_t *ptr = &RAM[pa];
        return MEMSwap(*(uint32_t *)ptr);
    }
    else return GCFetch(addr);
}

// map specified region of memory
void MEMMap(BOOL IR, BOOL DR, uint32_t startEA, uint32_t startPA, uint32_t length)
{
    uint32_t ea = startEA;
    uint32_t pa = startPA;
    uint32_t sz = ((length + 4095) & ~4095); // page round up

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

// swap longs (no need in assembly, used by tools)
void MEMSwapArea(uint32_t *addr, int count)
{
    uint32_t *until = addr + count / sizeof(uint32_t);

    while(addr != until)
    {
        *addr = MEMSwap(*addr);
        addr++;
    }
}

// swap shorts (no need in assembly, used by tools)
void MEMSwapAreaHalf(uint16_t *addr, int count)
{
    uint16_t *until = addr + count / sizeof(uint16_t);

    while(addr != until)
    {
        *addr = MEMSwapHalf(*addr);
        addr++;
    }    
}
