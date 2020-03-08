// CPU controls 
#include "pch.h"

// CPU control/state block (all important data is here)
CPUControl cpu;

// generate (not assert!) exception
void (*CPUException)(uint32_t vector);

// memory operations (using MEM* or DB* read/write operations)
void (__fastcall *CPUReadByte)(uint32_t addr, uint32_t *reg);
void (__fastcall *CPUWriteByte)(uint32_t addr, uint32_t data);
void (__fastcall *CPUReadHalf)(uint32_t addr, uint32_t *reg);
void (__fastcall *CPUReadHalfS)(uint32_t addr, uint32_t *reg);
void (__fastcall *CPUWriteHalf)(uint32_t addr, uint32_t data);
void (__fastcall *CPUReadWord)(uint32_t addr, uint32_t *reg);
void (__fastcall *CPUWriteWord)(uint32_t addr, uint32_t data);
void (__fastcall *CPUReadDouble)(uint32_t addr, uint64_t *reg);
void (__fastcall *CPUWriteDouble)(uint32_t addr, uint64_t *data);

// run from PC (using interpreter/debugger/recompiler)
void (*CPUStart)();

// ---------------------------------------------------------------------------

static bool IsMMXPresent()
{
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    return (cpuInfo[3] & 0x800000) != 0;
}

static bool IsSSEPresent()
{
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    return (cpuInfo[3] & 0x2000000) != 0;
}

// init tables, allocate memory
void CPUInit()
{
    cpu.mmx = IsMMXPresent();
    cpu.sse = IsSSEPresent();

    // setup interpreter tables
    IPTInitTables();
}

// free CPU resources
void CPUFini()
{
}

// select the core, before run
void CPUOpen(int bailout, int delay, int counterFactor)
{
    cpu.mmx = IsMMXPresent();
    cpu.sse = IsSSEPresent();

    // CPU bailout - number of CPU instructions to update hardware
    cpu.bailout = bailout;
    if(cpu.bailout <= 0) cpu.bailout = 1;
    cpu.bailtime = cpu.bailout;

    // CPU delay - number of CPU instructions to update TBR/DEC
    cpu.delay = delay;
    //if(cpu.delay > CPU_MAX_DELAY) cpu.delay = CPU_MAX_DELAY;
    if(cpu.delay <= 0) cpu.delay = 1;
    cpu.delayVal = cpu.delay;

    cpu.cf = counterFactor;
    cpu.one_second = CPU_TIMER_CLOCK;
    cpu.decreq = 0;

    // select core
    CPUStart = IPTStart;
    CPUException = IPTException;

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

    uint32_t old = DEC;
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
