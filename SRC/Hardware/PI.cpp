// PI - processor interface (interrupts and console control regs)
// PI fifo located in CP.cpp module
#include "pch.h"

// PI state (registers and other data)
PIControl pi;

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
        case PI_INTERRUPT_ERROR     : return "ERROR";
    }
    
    // default
    return "UNKNOWN";
}

static void printOut(uint32_t mask, const char *fix)
{
    char buf[256], *p = buf;
    for(uint32_t m=1; m<=PI_INTERRUPT_HSP; m<<=1)
    {
        if(mask & m) p += sprintf_s(p, sizeof(buf) - (p-buf), "%sINT ", intdesc(m));
    }
    *p = 0;
    DBReport2(DbgChannel::PI, "%s%s (pc: %08X, time: 0x%llx)", buf, fix, PC, UTBR);
}

// generate interrupt (if pending)
bool PICheckInterrupts()
{
    if((INTSR & INTMR) && (MSR & MSR_EE))
    {
        if (pi.log)
        {
            printOut(INTSR, "signaled");
        }
        CPUException(CPU_EXCEPTION_INTERRUPT);
        return true;
    }
    return false;
}

// assert (watch, not generate!) interrupt
void PIAssertInt(uint32_t mask)
{
    INTSR |= mask;
    if((INTMR & mask))
    {
        if (pi.log)
        {
            printOut(mask, "asserted");
        }
    }
}

// clear interrupt
void PIClearInt(uint32_t mask)
{ 
    if((INTSR & mask))
    {
        if (pi.log)
        {
            printOut(mask, "cleared");
        }
    }
    INTSR &= ~mask;
}

// ---------------------------------------------------------------------------
// traps for interrupt regs

static void __fastcall read_intsr(uint32_t addr, uint32_t *reg)
{
    *reg = INTSR | ((pi.rswhack & 1) << 16);
}

// writes turns them off ?
static void __fastcall write_intsr(uint32_t addr, uint32_t data)
{
    PIClearInt(data);
}

static void __fastcall read_intmr(uint32_t addr, uint32_t *reg)
{
    *reg = INTMR;
}

static void __fastcall write_intmr(uint32_t addr, uint32_t data)
{
    INTMR = data;

    // print out list of masked interrupts
    if(INTMR && pi.log)
    {
        char buf[256], *p = buf;
        for(uint32_t m=1; m<=PI_INTERRUPT_HSP; m<<=1)
        {
            if(INTMR & m) p += sprintf_s(p, sizeof(buf) - (p-buf), "%s ", intdesc(m));
        }
        *p = 0;

        DBReport2(DbgChannel::PI, "unmasked : %s\n", buf);
    }
}

//
// GC revision
//

static void __fastcall read_mbrev(uint32_t addr, uint32_t *reg)
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
// reset register
//

static void __fastcall write_reset(uint32_t addr, uint32_t data)
{
    // reset emulator
    if(data)
    {
        //EMUClose();
        //EMUOpen();
    }
}

static void __fastcall read_reset(uint32_t addr, uint32_t *reg)
{
    // on system power-on, the code is zero
    *reg = 0;
}

// ---------------------------------------------------------------------------
// init

void PIOpen(HWConfig* config)
{
    DBReport2(DbgChannel::PI, "Processor interface (interrupts)\n");

    pi.rswhack = config->rswhack;
    pi.consoleVer = config->consoleVer;
    pi.log = false;

    // clear interrupt registers
    INTSR = INTMR = 0;

    // set interrupt registers hooks
    MISetTrap(32, PI_INTSR   , read_intsr, write_intsr);
    MISetTrap(32, PI_INTMR   , read_intmr, write_intmr);
    MISetTrap(32, PI_MB_REV  , read_mbrev, NULL);
    MISetTrap( 8, PI_RST_CODE, read_reset, write_reset);
    MISetTrap(32, PI_RST_CODE, read_reset, write_reset);
}
