// high level Dolphin OS (experimental)
#include "dolphin.h"

#define PARAM(n)    GPR[3+n]
#define RET_VAL     GPR[3]
#define SWAP        MEMSwap

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
    HLEHit(HLE_OS_SET_CURRENT_CONTEXT);

    __OSCurrentContext  = PARAM(0);
    __OSPhysicalContext = __OSCurrentContext & RAMMASK; // simple translation
    CPUWriteWord(OS_CURRENT_CONTEXT, __OSCurrentContext);
    CPUWriteWord(OS_PHYSICAL_CONTEXT, __OSPhysicalContext);

    OSContext *c = (OSContext *)(&RAM[__OSPhysicalContext]);

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
        MSR |= MSR_FP;
    }

    MSR |= MSR_RI;
}

void OSGetCurrentContext(void)
{
    HLEHit(HLE_OS_GET_CURRENT_CONTEXT);

    RET_VAL = __OSCurrentContext;
}

void OSSaveContext(void)
{
    int i;
    HLEHit(HLE_OS_SAVE_CONTEXT);

    OSContext * c = (OSContext *)(&RAM[PARAM(0) & RAMMASK]);

    // always save FP/PS context
    OSSaveFPUContext();

    // save gprs
    for(i=13; i<32; i++)
        c->gpr[i] = SWAP(GPR[i]);

    // save gqrs 1..7 (GRQ0 is always 0)
    for(i=1; i<8; i++)
        c->gqr[i] = SWAP(GQR[i]);

    // misc regs
    c->cr = SWAP(CR);
    c->lr = SWAP(LR);
    c->ctr = SWAP(CTR);
    c->xer = SWAP(XER);
    c->srr[0] = c->lr;
    c->srr[1] = SWAP(MSR);

    c->gpr[1] = SWAP(SP);
    c->gpr[2] = SWAP(SDA2);
    c->gpr[3] = SWAP(GPR[0] = 1);

    RET_VAL = 0;
    // usual blr
}

// OSLoadContext return is patched as RFI (not usual BLR)
// see Symbols.cpp, SYMSetHighlevel, line 97
void OSLoadContext(void)
{
    HLEHit(HLE_OS_LOAD_CONTEXT);

    OSContext * c = (OSContext *)(&RAM[PARAM(0) & RAMMASK]);

    // thread switch on OSDisableInterrupts is omitted, because
    // interrupts are generated only at branch opcodes;
    // r0, r4, r5, r6 are garbed here ..

    // load gprs 0..2
    GPR[0] = SWAP(c->gpr[0]);
    GPR[1] = SWAP(c->gpr[1]);   // SP
    GPR[2] = SWAP(c->gpr[2]);   // SDA2
    
    // always load FP/PS context
    OSLoadFPUContext();

    // load gqrs 1..7 (GRQ0 is always 0)
    for(int i=1; i<8; i++)
        GQR[i] = SWAP(c->gqr[i]);

    // load other gprs
    uint16_t state = (c->state >> 8) | (c->state << 8);
    if(state & OS_CONTEXT_STATE_EXC)
    {
        state &= ~OS_CONTEXT_STATE_EXC;
        c->state = (state >> 8) | (state << 8);
        for(int i=5; i<32; i++)
            GPR[i] = SWAP(c->gpr[i]);
    }
    else
    {
        for(int i=13; i<32; i++)
            GPR[i] = SWAP(c->gpr[i]);
    }

    // misc regs
    CR  = SWAP(c->cr);
    LR  = SWAP(c->lr);
    CTR = SWAP(c->ctr);
    XER = SWAP(c->xer);

    // set srr regs to update msr and pc
    SRR0 = SWAP(c->srr[0]);
    SRR1 = SWAP(c->srr[1]);

    GPR[3] = SWAP(c->gpr[3]);
    GPR[4] = SWAP(c->gpr[4]);
    // rfi will be called
}

void OSClearContext(void)
{
    HLEHit(HLE_OS_CLEAR_CONTEXT);

    OSContext * c = (OSContext *)(&RAM[PARAM(0) & RAMMASK]);

    c->mode = 0;
    c->state = 0;

    if(PARAM(0) == __OSDefaultThread/*context*/) 
    {
        __OSDefaultThread = NULL;
        CPUWriteWord(OS_DEFAULT_THREAD, __OSDefaultThread);
    }
}

void OSInitContext(void)
{
    int i;
    HLEHit(HLE_OS_INIT_CONTEXT);

    OSContext * c = (OSContext *)(&RAM[PARAM(0) & RAMMASK]);

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
    c->gpr[2] = SWAP(SDA2);
    c->gpr[13] = SWAP(SDA1);
}

void OSLoadFPUContext(void)
{
    PARAM(1) = PARAM(0);
    OSContext * c = (OSContext *)(&RAM[PARAM(1) & RAMMASK]);

    //u16 state = (c->state >> 8) | (c->state << 8);
    //if(! (state & OS_CONTEXT_STATE_FPSAVED) )
    {
        FPSCR = SWAP(c->fpscr);

        for(int i=0; i<32; i++)
        {
            if(PSE)
            {
                cpu.ps1[i].uval = *(uint64_t *)(&c->psr[i]);
                swap_double(&cpu.ps1[i].uval);
            }
            cpu.fpr[i].uval = *(uint64_t *)(&c->fpr[i]);
            swap_double(&cpu.fpr[i].uval);
        }
    }
}

void OSSaveFPUContext(void)
{
    PARAM(2) = PARAM(0);
    OSContext * c = (OSContext *)(&RAM[PARAM(2) & RAMMASK]);

    //c->state |= (OS_CONTEXT_STATE_FPSAVED >> 8) | (OS_CONTEXT_STATE_FPSAVED << 8);
    c->fpscr = SWAP(FPSCR);

    for(int i=0; i<32; i++)
    {
        *(uint64_t *)(&c->fpr[i]) = cpu.fpr[i].uval;
        swap_double(&c->fpr[i]);
        if(PSE)
        {
            *(uint64_t *)(&c->psr[i]) = cpu.ps1[i].uval;
            swap_double(&c->psr[i]);
        }
    }
}

void OSFillFPUContext(void)
{
    OSContext * c = (OSContext *)(&RAM[PARAM(0) & RAMMASK]);
    
    MSR |= MSR_FP;
    c->fpscr = SWAP(FPSCR);

    for(int i=0; i<32; i++)
    {
        *(uint64_t *)(&c->fpr[i]) = cpu.fpr[i].uval;
        swap_double(&c->fpr[i]);
        if(PSE)
        {
            *(uint64_t *)(&c->psr[i]) = cpu.ps1[i].uval;
            swap_double(&c->psr[i]);
        }
    }
}

void __OSContextInit(void)
{
    DBReport( GREEN "HLE OS context driver installed.\n");
    DBReport( GREEN "Note: FP-Unavail is NOT used and FPRs are always saved.\n\n");

    __OSDefaultThread = NULL;
    CPUWriteWord(OS_DEFAULT_THREAD, __OSDefaultThread);
    MSR |= (MSR_FP | MSR_RI);
}

/* ---------------------------------------------------------------------------
    Interrupt handling
--------------------------------------------------------------------------- */

// called VERY often!
void OSDisableInterrupts(void)
{
    HLEHit(HLE_OS_DISABLE_INTERRUPTS);

    uint32_t prev = MSR;
    MSR &= ~MSR_EE;
    RET_VAL = (prev >> 15) & 1;
}

// this one is rare
void OSEnableInterrupts(void)
{
    HLEHit(HLE_OS_ENABLE_INTERRUPTS);

    uint32_t prev = MSR;
    MSR |= MSR_EE;
    RET_VAL = (prev >> 15) & 1;
}

// called VERY often!
void OSRestoreInterrupts(void)
{
    HLEHit(HLE_OS_RESTORE_INTERRUPTS);

    uint32_t prev = MSR;
    if(PARAM(0)) MSR |= MSR_EE;
    else MSR &= ~MSR_EE;
    RET_VAL = (prev >> 15) & 1;
}

/* ---------------------------------------------------------------------------
    Utils
--------------------------------------------------------------------------- */

// show OSContext data align offsets
void OSCheckContextStruct()
{
    int i;
    OSContext context;

    for(i=0; i<32; i++)
        DBReport("GPR[%i] = %i\n", i, (uint32_t)&context.gpr[i] - (uint32_t)&context.gpr[0]);

    DBReport("CR = %i\n", (uint32_t)&context.cr - (uint32_t)&context.gpr[0]);
    DBReport("LR = %i\n", (uint32_t)&context.lr - (uint32_t)&context.gpr[0]);
    DBReport("CTR = %i\n", (uint32_t)&context.ctr - (uint32_t)&context.gpr[0]);
    DBReport("XER = %i\n", (uint32_t)&context.xer - (uint32_t)&context.gpr[0]);

    for(i=0; i<32; i++)
        DBReport("FPR[%i] = %i\n", i, (uint32_t)&context.fpr[i] - (uint32_t)&context.gpr[0]);

    DBReport("FPSCR = %i\n", (uint32_t)&context.fpscr_pad - (uint32_t)&context.gpr[0]);

    DBReport("SRR0 = %i\n", (uint32_t)&context.srr[0] - (uint32_t)&context.gpr[0]);
    DBReport("SRR1 = %i\n", (uint32_t)&context.srr[1] - (uint32_t)&context.gpr[0]);

    DBReport("mode = %i\n", (uint32_t)&context.mode - (uint32_t)&context.gpr[0]);
    DBReport("state = %i\n", (uint32_t)&context.state - (uint32_t)&context.gpr[0]);

    for(i=0; i<8; i++)
        DBReport("GQR[%i] = %i\n", i, (uint32_t)&context.gqr[i] - (uint32_t)&context.gpr[0]);
    for(i=0; i<32; i++)
        DBReport("PSR[%i] = %i\n", i, (uint32_t)&context.psr[i] - (uint32_t)&context.gpr[0]);

    DBReport("OSContext size: %i(%i)/%i\n", sizeof(OSContext), 712, OS_CONTEXT_SIZE);
}

// covert GC time to human-usable time string;
// example output : "30 Jun 2004 3:06:14:127"
char * OSTimeFormat(uint64_t tbr, BOOL noDate /* FALSE */)
{
    // FILETIME - number of 1/10000000 intervals, since Jan 1 1601
    // GC time  - number of 1/40500000 sec intervals, since Jan 1 2000
    // To convert GCTIME -> FILETIME :
    //      1: adjust GCTIME by number of 1/10000000 intervals
    //         between Jan 1 1601 and Jan 1 2000.
    //      2: assume X - 1/10000000 sec, Y - 1/40500000 sec,
    //         FILETIME = (GCTIME * Y) / X

    // coversion GCTIME -> FILETIME
    #define MAGIK 0x0713AD7857941000
    double x = 1.0 / 10000000.0, y = 1.0 / 40500000.0;
    tbr += MAGIK;
    uint64_t ft = (uint64_t)( ((double)(int64_t)tbr * y) / x );
    FILETIME fileTime; SYSTEMTIME sysTime;
    fileTime.dwHighDateTime = (uint32_t)(ft >> 32);
    fileTime.dwLowDateTime  = (uint32_t)(ft & 0x00000000ffffffff);
    FileTimeToSystemTime(&fileTime, &sysTime);

    // format string
    static char *mnstr[12] =
        { "Jan", "Feb", "Mar", "Apr",
          "May", "Jun", "Jul", "Aug",
          "Sep", "Oct", "Nov", "Dec"
        };
    static char gcTime[256];
    if(noDate)
    {
        sprintf( gcTime, "%02i:%02i:%02i:%03i",
                sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds );
    }
    else
    {
        sprintf( gcTime, "%i %s %i %02i:%02i:%02i:%03i",
                sysTime.wDay, mnstr[sysTime.wMonth - 1], sysTime.wYear,
                sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds );
    }
    return gcTime;
}
