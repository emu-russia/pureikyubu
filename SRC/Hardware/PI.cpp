// PI - processor interface (interrupts and console control regs, FIFO)
#include "pch.h"

using namespace Debug;

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
    std::string buf;
    for(uint32_t m=1; m<=PI_INTERRUPT_HSP; m<<=1)
    {
        if (mask & m)
        {
            buf += intdesc(m);
            buf += "INT ";
        }
    }
    Report(Channel::PI, "%s%s (pc: %08X, time: 0x%llx)", buf.c_str(), fix, Gekko::Gekko->regs.pc, Gekko::Gekko->GetTicks());
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

    if (pi.intsr & pi.intmr)
    {
        Gekko::Gekko->AssertInterrupt();
    }
    else
    {
        Gekko::Gekko->ClearInterrupt();
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
        Gekko::Gekko->AssertInterrupt();
    }
    else
    {
        Gekko::Gekko->ClearInterrupt();
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
// GC revision
//

static void read_mbrev(uint32_t addr, uint32_t *reg)
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

static void write_reset(uint32_t addr, uint32_t data)
{
    // reset emulator
    if(data)
    {
        //EMUClose();
        //EMUOpen();
    }
}

static void read_reset(uint32_t addr, uint32_t *reg)
{
    // on system power-on, the code is zero
    *reg = 0;
}

//
// PI fifo (CPU)
//

static void read_pi_base(uint32_t addr, uint32_t *reg)   { *reg = pi.base & ~0x1f; }
static void write_pi_base(uint32_t addr, uint32_t data)  { pi.base = data & ~0x1f; }
static void read_pi_top(uint32_t addr, uint32_t *reg)    { *reg = pi.top & ~0x1f; }
static void write_pi_top(uint32_t addr, uint32_t data)   { pi.top = data & ~0x1f; }
static void read_pi_wrptr(uint32_t addr, uint32_t *reg)  { *reg = pi.wrptr & ~0x1f; }
static void write_pi_wrptr(uint32_t addr, uint32_t data) { pi.wrptr = data & ~0x1f; }

// show PI fifo configuration
void DumpPIFIFO()
{
    Report(Channel::Norm, "PI fifo configuration");
    Report(Channel::Norm, "   base :0x%08X", pi.base);
    Report(Channel::Norm, "   top  :0x%08X", pi.top);
    Report(Channel::Norm, "   wrptr:0x%08X", pi.wrptr);
    Report(Channel::Norm, "   wrap :%i", (pi.wrptr & PI_WRPTR_WRAP) ? (1) : (0));
}

// ---------------------------------------------------------------------------
// init

void PIOpen(HWConfig* config)
{
    Report(Channel::PI, "Processor interface (interrupts)\n");

    pi.rswhack = config->rswhack;
    pi.consoleVer = config->consoleVer;
    pi.log = false;

    // clear interrupt registers
    pi.intsr = pi.intmr = 0;

    // set interrupt registers hooks
    MISetTrap(32, PI_INTSR   , read_intsr, write_intsr);
    MISetTrap(32, PI_INTMR   , read_intmr, write_intmr);
    MISetTrap(32, PI_MB_REV  , read_mbrev, NULL);
    MISetTrap( 8, PI_RST_CODE, read_reset, write_reset);
    MISetTrap(32, PI_RST_CODE, read_reset, write_reset);

    // processor interface (CPU fifo)
    MISetTrap(32, PI_BASE , read_pi_base , write_pi_base);
    MISetTrap(32, PI_TOP  , read_pi_top  , write_pi_top);
    MISetTrap(32, PI_WRPTR, read_pi_wrptr, write_pi_wrptr);
}
