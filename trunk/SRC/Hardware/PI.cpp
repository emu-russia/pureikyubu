// PI - processor interface (interrupts and console control regs)
// PI fifo located in CP.cpp module
#include "dolphin.h"

// PI state (registers and other data)
PIControl pi;

// ---------------------------------------------------------------------------
// interrupts

// return short interrupt description
static char *intdesc(u32 mask)
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

static void printOut(u32 mask, char *fix)
{
    if(emu.doldebug)
    {
        char buf[256], *p = buf;
        for(u32 m=1; m<=PI_INTERRUPT_HSP; m<<=1)
        {
            if(mask & m) p += sprintf(p, "%sINT ", intdesc(m));
        }
        *p = 0;
        DBReport(PI "%s%s (pc: %08X, time: %s)", buf, fix, PC, OSTimeFormat(UTBR, 1));
    }
}

// generate interrupt (if pending)
void PICheckInterrupts()
{
    if((INTSR & INTMR) && (MSR & MSR_EE))
    {
        if(emu.doldebug)
        {
            printOut(INTSR, "signaled");
        }
        CPUException(CPU_EXCEPTION_INTERRUPT);
    }
}

// assert (look, not generate!) interrupt
void PIAssertInt(u32 mask)
{
    INTSR |= mask;
    if((INTMR & mask) && emu.doldebug)
    {
        printOut(mask, "asserted");
    }
    PICheckInterrupts();
}

// clear interrupt
void PIClearInt(u32 mask)
{ 
    if((INTSR & mask) && emu.doldebug)
    {
        printOut(mask, "cleared");
    }
    INTSR &= ~mask;
}

// ---------------------------------------------------------------------------
// traps for interrupt regs

static void __fastcall read_intsr(u32 addr, u32 *reg)
{
    *reg = INTSR | (pi.rswhack << 16);
}

// writes turns them off ?
static void __fastcall write_intsr(u32 addr, u32 data)
{
    INTSR &= ~data;
    PICheckInterrupts();
}

static void __fastcall read_intmr(u32 addr, u32 *reg)
{
    *reg = INTMR;
}

static void __fastcall write_intmr(u32 addr, u32 data)
{
    INTMR = data;

    // print out list of masked interrupts
    if(INTMR && emu.doldebug)
    {
        char buf[256], *p = buf;
        for(u32 m=1; m<=PI_INTERRUPT_HSP; m<<=1)
        {
            if(INTMR & m) p += sprintf(p, "%s ", intdesc(m));
        }
        *p = 0;

        DBReport(PI "unmasked : %s\n", buf);
    }

    PICheckInterrupts();
}

//
// GC revision
//

static void __fastcall read_mbrev(u32 addr, u32 *reg)
{
    u32 ver = GetConfigInt(USER_CONSOLE, USER_CONSOLE_DEFAULT);

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

static void __fastcall write_reset(u32 addr, u32 data)
{
    // reset emulator
    if(data)
    {
        //EMUClose();
        //EMUOpen();
    }
}

static void __fastcall read_reset(u32 addr, u32 *reg)
{
    // on system power-on, the code is zero
    *reg = 0;
}

// ---------------------------------------------------------------------------
// init

void PIOpen()
{
    DBReport(CYAN "PI: Processor interface (interrupts)\n");

    pi.rswhack = GetConfigInt(USER_PI_RSWHACK, USER_PI_RSWHACK_DEFAULT);

    // clear interrupt registers
    INTSR = INTMR = 0;

    // set interrupt registers hooks
    HWSetTrap(32, PI_INTSR   , read_intsr, write_intsr);
    HWSetTrap(32, PI_INTMR   , read_intmr, write_intmr);
    HWSetTrap(32, PI_MB_REV  , read_mbrev, NULL);
    HWSetTrap( 8, PI_RST_CODE, read_reset, write_reset);
    HWSetTrap(32, PI_RST_CODE, read_reset, write_reset);
}
