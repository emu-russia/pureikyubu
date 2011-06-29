// CPU controls 
#include "dolphin.h"

// CPU control/state block (all important data is here)
CPUControl cpu;

// generate (look, not assert!) exception
void (*CPUException)(u32 vector);

// memory operations (using MEM* or DB* read/write operations)
void (__fastcall *CPUReadByte)(u32 addr, u32 *reg);
void (__fastcall *CPUWriteByte)(u32 addr, u32 data);
void (__fastcall *CPUReadHalf)(u32 addr, u32 *reg);
void (__fastcall *CPUReadHalfS)(u32 addr, u32 *reg);
void (__fastcall *CPUWriteHalf)(u32 addr, u32 data);
void (__fastcall *CPUReadWord)(u32 addr, u32 *reg);
void (__fastcall *CPUWriteWord)(u32 addr, u32 data);
void (__fastcall *CPUReadDouble)(u32 addr, u64 *reg);
void (__fastcall *CPUWriteDouble)(u32 addr, u64 *data);

// run from PC (using interpreter/debugger/recompiler)
void (*CPUStart)();

// ---------------------------------------------------------------------------

static BOOL IsMMXPresent()
{
    DWORD flag;

    __asm   mov     eax, 1
    __asm   cpuid
    __asm   and     edx, 0x800000
    __asm   mov     flag, edx

    return (flag != 0);
}

static BOOL IsSSEPresent()
{
    DWORD flag;

    __asm   mov     eax, 1
    __asm   cpuid
    __asm   and     edx, 0x2000000
    __asm   mov     flag, edx

    return (flag != 0);
}

// init tables, allocate memory
void CPUInit()
{
    cpu.mmx = IsMMXPresent() && GetConfigInt(USER_MMX, USER_MMX_DEFAULT);
    cpu.sse = IsSSEPresent() && cpu.mmx;

    // setup interpreter tables
    IPTInitTables();

    // setup recompiler
    RECInitTables();
}

// free CPU resources
void CPUFini()
{
}

// select the core, before run
void CPUOpen()
{
    cpu.mmx = IsMMXPresent() && GetConfigInt(USER_MMX, USER_MMX_DEFAULT);
    cpu.sse = IsSSEPresent() && cpu.mmx;

    // CPU bailout - number of CPU instructions to update hardware
    cpu.bailout = GetConfigInt(USER_CPU_TIME, USER_CPU_TIME_DEFAULT);
    if(cpu.bailout <= 0) cpu.bailout = 1;
    cpu.bailtime = cpu.bailout;

    // CPU delay - number of CPU instructions to update TBR/DEC
    cpu.delay = GetConfigInt(USER_CPU_DELAY, USER_CPU_DELAY_DEFAULT);
    //if(cpu.delay > CPU_MAX_DELAY) cpu.delay = CPU_MAX_DELAY;
    if(cpu.delay <= 0) cpu.delay = 1;
    cpu.delayVal = cpu.delay;

    cpu.cf = GetConfigInt(USER_CPU_CF, USER_CPU_CF_DEFAULT);
    cpu.one_second = CPU_TIMER_CLOCK;
    cpu.decreq = 0;

    // select core
    cpu.core = GetConfigInt(USER_CPU, USER_CPU_DEFAULT);
    switch(cpu.core)
    {
        case CPU_INTERPRETER:
            CPUStart = IPTStart;
            CPUException = IPTException;
            break;

        case CPU_RECOMPILER:
            CPUStart = RECStart;
            CPUException = RECException;
            break;

        default:
            DolwinError( "Gekko core select", 
                         "Unknown core number : %i",
                         cpu.core );
    }

    // debugger has its own core, to control CPU execution
    if(emu.doldebug)
    {
        CPUStart = DBStart;
        CPUException = DBException;
    }

    // set CPU memory operations to default (using MEM*);
    // debugger will override them by DB* calls after start, if need.
    CPUReadByte     = MEMReadByte;
    CPUWriteByte    = MEMWriteByte;
    CPUReadHalf     = MEMReadHalf;
    CPUReadHalfS    = MEMReadHalfS;
    CPUWriteHalf    = MEMWriteHalf;
    CPUReadWord     = MEMReadWord;
    CPUWriteWord    = MEMWriteWord;
    CPUReadDouble   = MEMReadDouble;
    CPUWriteDouble  = MEMWriteDouble;

    // clear opcode counter (used for MIPS calculaion)
    cpu.ops = 0;

    // we dont care about CPU registers, because they are 
    // undefined, since any GC program is virtually loaded
    // after BOOTROM. although, we should set some system
    // registers for correct MMU emulation (see Bootrom.cpp).
}

// modify CPU counters
void CPUTick()
{
    cpu.delayVal--;
    if(cpu.delayVal == 0) cpu.delayVal = cpu.delay;
    else return;

    UTBR += cpu.cf;         // timer

    u32 old = DEC;
    DEC -= cpu.cf;          // decrementer
    if((old ^ DEC) & 0x80000000)
    {
        if(MSR & MSR_EE)
        {
            cpu.decreq = 1;
            DBReport(CPU "decrementer exception (OS alarm), pc:%08X\n", PC);
        }
    }
}
