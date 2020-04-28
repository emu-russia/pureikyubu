// high level Dolphin OS (experimental)
#include "pch.h"

#define PARAM(n)    Gekko::Gekko->regs.gpr[3+n]
#define RET_VAL     Gekko::Gekko->regs.gpr[3]
#define SWAP        _byteswap_ulong

// internal OS vars
static  uint32_t     __OSPhysicalContext;    // OS_PHYSICAL_CONTEXT
static  uint32_t     __OSCurrentContext;     // OS_CURRENT_CONTEXT

static  uint32_t     __OSDefaultThread;      // OS_DEFAULT_THREAD

/* ---------------------------------------------------------------------------
    Context API, based on Dolphin OS reversing of OSContext module
--------------------------------------------------------------------------- */

// IMPORTANT : FPRs are ALWAYS saved, because FP Unavail handler is not used

// stack operations are not emulated, because they are simple

// fast longlong swap, invented by org
static void swap_double(void *srcPtr)
{
    uint8_t *src = (uint8_t*)srcPtr;
    register uint8_t t;

    for(int i=0; i<4; i++)
    {
        t = src[7-i];
        src[7-i] = src[i];
        src[i] = t;
    }
}

void OSSetCurrentContext(void)
{
    __OSCurrentContext  = PARAM(0);
    __OSPhysicalContext = __OSCurrentContext & RAMMASK; // simple translation
    Gekko::Gekko->WriteWord(OS_CURRENT_CONTEXT, __OSCurrentContext);
    Gekko::Gekko->WriteWord(OS_PHYSICAL_CONTEXT, __OSPhysicalContext);

    OSContext *c = (OSContext *)(&mi.ram[__OSPhysicalContext]);

    if(__OSCurrentContext == __OSDefaultThread/*context*/)
    {
        c->srr[1] |= SWAP(MSR_FP);
    }
    else
    {
        // floating point regs are always available!
        //c->srr[1] &= ~SWAP(MSR_FP);
        //MSR &= ~MSR_FP;

        c->srr[1] |= SWAP(MSR_FP);
        Gekko::Gekko->regs.msr |= MSR_FP;
    }

    Gekko::Gekko->regs.msr |= MSR_RI;
}

void OSGetCurrentContext(void)
{
    RET_VAL = __OSCurrentContext;
}

void OSSaveContext(void)
{
    int i;

    OSContext * c = (OSContext *)(&mi.ram[PARAM(0) & RAMMASK]);

    // always save FP/PS context
    OSSaveFPUContext();

    // save gprs
    for(i=13; i<32; i++)
        c->gpr[i] = SWAP(Gekko::Gekko->regs.gpr[i]);

    // save gqrs 1..7 (GRQ0 is always 0)
    for(i=1; i<8; i++)
        c->gqr[i] = SWAP(Gekko::Gekko->regs.spr[(int)Gekko::SPR::GQRs + i]);

    // misc regs
    c->cr = SWAP(Gekko::Gekko->regs.cr);
    c->lr = SWAP(Gekko::Gekko->regs.spr[(int)Gekko::SPR::LR]);
    c->ctr = SWAP(Gekko::Gekko->regs.spr[(int)Gekko::SPR::CTR]);
    c->xer = SWAP(Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER]);
    c->srr[0] = c->lr;
    c->srr[1] = SWAP(Gekko::Gekko->regs.msr);

    c->gpr[1] = SWAP(Gekko::Gekko->regs.gpr[1]);
    c->gpr[2] = SWAP(Gekko::Gekko->regs.gpr[2]);
    c->gpr[3] = SWAP(Gekko::Gekko->regs.gpr[0] = 1);

    RET_VAL = 0;
    // usual blr
}

// OSLoadContext return is patched as RFI (not usual BLR)
// see Symbols.cpp, SYMSetHighlevel, line 97
void OSLoadContext(void)
{
    OSContext * c = (OSContext *)(&mi.ram[PARAM(0) & RAMMASK]);

    // thread switch on OSDisableInterrupts is omitted, because
    // interrupts are generated only at branch opcodes;
    // r0, r4, r5, r6 are garbed here ..

    // load gprs 0..2
    Gekko::Gekko->regs.gpr[0] = SWAP(c->gpr[0]);
    Gekko::Gekko->regs.gpr[1] = SWAP(c->gpr[1]);   // SP
    Gekko::Gekko->regs.gpr[2] = SWAP(c->gpr[2]);   // SDA2
    
    // always load FP/PS context
    OSLoadFPUContext();

    // load gqrs 1..7 (GRQ0 is always 0)
    for(int i=1; i<8; i++)
        Gekko::Gekko->regs.spr[(int)Gekko::SPR::GQRs + i] = SWAP(c->gqr[i]);

    // load other gprs
    uint16_t state = (c->state >> 8) | (c->state << 8);
    if(state & OS_CONTEXT_STATE_EXC)
    {
        state &= ~OS_CONTEXT_STATE_EXC;
        c->state = (state >> 8) | (state << 8);
        for(int i=5; i<32; i++)
            Gekko::Gekko->regs.gpr[i] = SWAP(c->gpr[i]);
    }
    else
    {
        for(int i=13; i<32; i++)
            Gekko::Gekko->regs.gpr[i] = SWAP(c->gpr[i]);
    }

    // misc regs
    Gekko::Gekko->regs.cr  = SWAP(c->cr);
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::LR] = SWAP(c->lr);
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::CTR] = SWAP(c->ctr);
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] = SWAP(c->xer);

    // set srr regs to update msr and pc
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::SRR0] = SWAP(c->srr[0]);
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::SRR1] = SWAP(c->srr[1]);

    Gekko::Gekko->regs.gpr[3] = SWAP(c->gpr[3]);
    Gekko::Gekko->regs.gpr[4] = SWAP(c->gpr[4]);
    // rfi will be called
}

void OSClearContext(void)
{
    OSContext * c = (OSContext *)(&mi.ram[PARAM(0) & RAMMASK]);

    c->mode = 0;
    c->state = 0;

    if(PARAM(0) == __OSDefaultThread/*context*/) 
    {
        __OSDefaultThread = NULL;
        Gekko::Gekko->WriteWord(OS_DEFAULT_THREAD, __OSDefaultThread);
    }
}

void OSInitContext(void)
{
    int i;

    OSContext * c = (OSContext *)(&mi.ram[PARAM(0) & RAMMASK]);

    c->srr[0] = SWAP(PARAM(1));
    c->gpr[1] = SWAP(PARAM(2));
    c->srr[1] = SWAP(MSR_EE | MSR_ME | MSR_IR | MSR_DR | MSR_RI);
    
    c->cr = 0;
    c->xer = 0;

    for(i=0; i<8; i++)
        c->gqr[i] = 0;

    OSClearContext();

    for(i=3; i<32; i++)
        c->gpr[i] = 0;
    c->gpr[2] = SWAP(Gekko::Gekko->regs.gpr[2]);
    c->gpr[13] = SWAP(Gekko::Gekko->regs.gpr[13]);
}

void OSLoadFPUContext(void)
{
    PARAM(1) = PARAM(0);
    OSContext * c = (OSContext *)(&mi.ram[PARAM(1) & RAMMASK]);

    //u16 state = (c->state >> 8) | (c->state << 8);
    //if(! (state & OS_CONTEXT_STATE_FPSAVED) )
    {
        Gekko::Gekko->regs.fpscr = SWAP(c->fpscr);

        for(int i=0; i<32; i++)
        {
            if(Gekko::Gekko->regs.spr[(int)Gekko::SPR::HID2] & HID2_PSE)
            {
                Gekko::Gekko->regs.ps1[i].uval = *(uint64_t *)(&c->psr[i]);
                swap_double(&Gekko::Gekko->regs.ps1[i].uval);
            }
            Gekko::Gekko->regs.fpr[i].uval = *(uint64_t *)(&c->fpr[i]);
            swap_double(&Gekko::Gekko->regs.fpr[i].uval);
        }
    }
}

void OSSaveFPUContext(void)
{
    PARAM(2) = PARAM(0);
    OSContext * c = (OSContext *)(&mi.ram[PARAM(2) & RAMMASK]);

    //c->state |= (OS_CONTEXT_STATE_FPSAVED >> 8) | (OS_CONTEXT_STATE_FPSAVED << 8);
    c->fpscr = SWAP(Gekko::Gekko->regs.fpscr);

    for(int i=0; i<32; i++)
    {
        *(uint64_t *)(&c->fpr[i]) = Gekko::Gekko->regs.fpr[i].uval;
        swap_double(&c->fpr[i]);
        if(Gekko::Gekko->regs.spr[(int)Gekko::SPR::HID2] & HID2_PSE)
        {
            *(uint64_t *)(&c->psr[i]) = Gekko::Gekko->regs.ps1[i].uval;
            swap_double(&c->psr[i]);
        }
    }
}

void OSFillFPUContext(void)
{
    OSContext * c = (OSContext *)(&mi.ram[PARAM(0) & RAMMASK]);
    
    Gekko::Gekko->regs.msr |= MSR_FP;
    c->fpscr = SWAP(Gekko::Gekko->regs.fpscr);

    for(int i=0; i<32; i++)
    {
        *(uint64_t *)(&c->fpr[i]) = Gekko::Gekko->regs.fpr[i].uval;
        swap_double(&c->fpr[i]);
        if(Gekko::Gekko->regs.spr[(int)Gekko::SPR::HID2] & HID2_PSE)
        {
            *(uint64_t *)(&c->psr[i]) = Gekko::Gekko->regs.ps1[i].uval;
            swap_double(&c->psr[i]);
        }
    }
}

void __OSContextInit(void)
{
    DBReport2(DbgChannel::HLE, "HLE OS context driver installed.\n");
    DBReport2(DbgChannel::HLE, "Note: FP-Unavail is NOT used and FPRs are always saved.\n\n");

    __OSDefaultThread = NULL;
    Gekko::Gekko->WriteWord(OS_DEFAULT_THREAD, __OSDefaultThread);
    Gekko::Gekko->regs.msr |= (MSR_FP | MSR_RI);
}

/* ---------------------------------------------------------------------------
    Interrupt handling
--------------------------------------------------------------------------- */

// called VERY often!
void OSDisableInterrupts(void)
{
    uint32_t prev = Gekko::Gekko->regs.msr;
    Gekko::Gekko->regs.msr &= ~MSR_EE;
    RET_VAL = (prev >> 15) & 1;
}

// this one is rare
void OSEnableInterrupts(void)
{
    uint32_t prev = Gekko::Gekko->regs.msr;
    Gekko::Gekko->regs.msr |= MSR_EE;
    RET_VAL = (prev >> 15) & 1;
}

// called VERY often!
void OSRestoreInterrupts(void)
{
    uint32_t prev = Gekko::Gekko->regs.msr;
    if(PARAM(0)) Gekko::Gekko->regs.msr |= MSR_EE;
    else Gekko::Gekko->regs.msr &= ~MSR_EE;
    RET_VAL = (prev >> 15) & 1;
}
