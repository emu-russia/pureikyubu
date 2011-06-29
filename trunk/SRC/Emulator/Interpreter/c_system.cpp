// System Instructions
#include "dolphin.h"

#define OP(name) void __fastcall c_##name##(u32 op)

// ---------------------------------------------------------------------------
// os

OP(TWI)
{
    s32 a = RRA, b = SIMM;
    s32 to = RS;
    
    if( ((a < b) && (to & 0x10)) ||
        ((a > b) && (to & 0x08)) ||
        ((a ==b) && (to & 0x04)) ||
        (((u32)a <(u32)b) && (to & 0x02)) ||
        (((u32)a >(u32)b) && (to & 0x01)))
    {
        // pseudo-branch (to resume from next instruction after 'rfi')
        PC += 4;
        CPUException(CPU_EXCEPTION_PROGRAM);
    }
}

OP(TW)
{
    s32 a = RRA, b = RRB;
    s32 to = RS;
    BOOL trap = FALSE;
    
    if( ((a < b) && (to & 0x10)) ||
        ((a > b) && (to & 0x08)) ||
        ((a ==b) && (to & 0x04)) ||
        (((u32)a <(u32)b) && (to & 0x02)) ||
        (((u32)a >(u32)b) && (to & 0x01)))
    {
        // pseudo-branch (to resume from next instruction after 'rfi')
        PC += 4;
        CPUException(CPU_EXCEPTION_PROGRAM);
    }
}

// syscall
OP(SC)
{
    if(!ldat.freeLoader && !ldat.actionReplay)
    {
        // pseudo-branch (to resume from next instruction after 'rfi')
        PC += 4;
        CPUException(CPU_EXCEPTION_SYSCALL);
    }
}

// return from exception
OP(RFI)
{
    MSR &= ~(0x87C0FF73 | 0x00040000);
    MSR |= SRR1 & 0x87C0FF73;
    PC = SRR0 & ~3;
    cpu.branch = TRUE;
}

// ---------------------------------------------------------------------------
// system registers

static inline BOOL msr_ir() { return (MSR & MSR_IR) ? 1 : 0; }
static inline BOOL msr_dr() { return (MSR & MSR_DR) ? 1 : 0; }

// mask = (4)CRM[0] || (4)CRM[1] || ... || (4)CRM[7]
// CR = (rs & mask) | (CR & ~mask)
OP(MTCRF)
{
    u32 m, crm = CRM, a, d = RRS;

    for(s32 i=0; i<8; i++)
    {
        if((crm >> i) & 1)
        {
            a = (d >> (i << 2)) & 0xf;
            m = (0xf << (i << 2));
            CR = (CR & ~m) | (a << (i << 2));
        }
    }
}

// CR[4 * crfD .. 4 * crfd + 3] = XER[0-3]
// XER[0..3] = 0b0000
OP(MCRXR)
{
    u32 mask = 0xf0000000 >> (4 * CRFD);
    CR &= ~mask;
    CR |= (XER & 0xf0000000) >> (4 * CRFD);
    XER &= ~0xf0000000;
}

// rd = cr
OP(MFCR)
{
    RRD = CR;
}

// msr = rs
OP(MTMSR)
{
    MSR = RRS;
}

// rd = msr
OP(MFMSR)
{
    RRD = MSR;
}

// spr = rs
OP(MTSPR)
{
    s32 spr = (RB << 5) | RA;

    if(spr >= 528 && spr <= 543)
    {
        static char * bat[] = {
            "IBAT0U", "IBAT0L", "IBAT1U", "IBAT1L",
            "IBAT2U", "IBAT2L", "IBAT3U", "IBAT3L",
            "DBAT0U", "DBAT0L", "DBAT1U", "DBAT1L",
            "DBAT2U", "DBAT2L", "DBAT3U", "DBAT3L"
        };
        if(emu.doldebug) DBReport( CPU "%s <- %08X (IR:%i DR:%i pc:%08X)\n", 
                                   bat[spr-528], RRS, msr_ir(), msr_dr(), PC );
    }
    else switch(spr)
    {
        // decrementer
        case    22:
            DBReport(CPU "set decrementer (OS alarm) to %s\n", OSTimeFormat(RRS, 1));
            break;

        // page table base
        case    25:
            DBReport( CPU "SDR <- %08X (IR:%i DR:%i pc:%08X)\n", 
                      RRS, msr_ir(), msr_dr(), PC );
            break;

        case    284:
            cpu.tb.l = RRS;
            DBReport(CPU "set TBL : %08X\n", cpu.tb.l);
            break;
        case    285:
            cpu.tb.u = RRS;
            DBReport(CPU "set TBH : %08X\n", cpu.tb.u);
            break;

        // write gathering buffer
        case    921:
            if(RRS != 0x0C008000)
            {
                DolwinQuestion(
                    "Non-predictable situation!", 
                    "Redirecting write gather buffer!\n"
                    "Are you sure, you want to continue?"
                );
            }
            break;

        // locked cache dma (dirty hack)
        case    922:    // DMAU
            SPR[spr] = RRS;
            break;
        case    923:    // DMAL
        {
            SPR[spr] = RRS;
            if(SPR[923] & 2)
            {
                u32 maddr = SPR[922] & ~0x1f;
                u32 lcaddr = SPR[923] & ~0x1f;
                u32 length = ((SPR[922] & 0x1f) << 2) | ((SPR[923] >> 2) & 3);
                if(length == 0) length = 128;
                if(SPR[923] & 0x10)
                {   // load
                    memcpy(
                        &RAM[maddr & RAMMASK],
                        &mem.lc[lcaddr & 0x3ffff],
                        length * 32
                    );
                }
                else
                {   // store
                    memcpy(
                        &mem.lc[lcaddr & 0x3ffff],
                        &RAM[maddr & RAMMASK],
                        length * 32
                    );
                }
            }

/*/
            DolwinQuestion(
                "Non-predictable situation!", 
                "Locked cache is not implemented!\n"
                "Are you sure, you want to continue?"                
            );
/*/
        }
        break;            
    }

    // default
    SPR[spr] = RRS;
}

// rd = spr
OP(MFSPR)
{
    RRD = SPR[(RB << 5) | RA];
}

// rd = tbr
OP(MFTB)
{
    s32 tbr = (RB << 5) | RA;

    if(tbr == 268)
    {
        RRD = cpu.tb.l;
        return;
    }
    else if(tbr == 269)
    {
        RRD = cpu.tb.u;
        return;
    }
}

// sr[a] = rs
OP(MTSR)
{
    SR[RA] = RRS;
}

// sr[rb] = rs
OP(MTSRIN)
{
    SR[RRB & 0xf] = RRS;
}

// rd = sr[a]
OP(MFSR)
{
    RRD = SR[RA];
}

// rd = sr[rb]
OP(MFSRIN)
{
    RRD = SR[RRB & 0xf];
}

// ---------------------------------------------------------------------------
// various context synchronizing

OP(EIEIO)
{
}

OP(SYNC)
{
}

// instruction synchronize. Dolwin interpreter is not super-scalar. :)
OP(ISYNC)
{
}

OP(TLBSYNC)
{
}

OP(TLBIE)
{
}

// ---------------------------------------------------------------------------
// caches

OP(DCBT) {}
OP(DCBTST) {}
OP(DCBZ) {}
OP(DCBZ_L) {}
OP(DCBST) {}
OP(DCBF) {}
OP(DCBI) {}
OP(ICBI) {}
