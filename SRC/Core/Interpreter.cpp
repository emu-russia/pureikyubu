// default C interpreter (opcode parser)
#include "pch.h"

// parse and execute single opcode
void IPTExecuteOpcode(Gekko::GekkoCore * core)
{
    // execute one instruction
    // (possible CPU_EXCEPTION_DSI, ISI, ALIGN, PROGRAM, FPUNAVAIL, SYSCALL)
    uint32_t op;
    MEMFetch(PC, &op);
    if(cpu.exception) goto JumpPC;  // ISI
    c_1[op >> 26](op); cpu.ops++;
    if(cpu.exception) goto JumpPC;  // DSI, ALIGN, PROGRAM, FPUNA, SC

    // according to manual, decrementer has lower priority rather external interrupt
    
    // time to update HW (possible CPU_EXCEPTION_INTERRUPT)
    if(cpu.branch)
    {
        HWUpdate();
        if(cpu.exception) goto JumpPC;
    }

    // modify CPU counters (possible CPU_EXCEPTION_DECREMENTER)
    core->Tick();
    if(cpu.branch)
    {
        if(cpu.decreq)
        {
            cpu.decreq = 0;
            CPUException(CPU_EXCEPTION_DECREMENTER);
            if(cpu.exception) goto JumpPC;
        }
    }

    // branch or exception ?
    if(cpu.branch)
    {
JumpPC:
        cpu.exception = false;
        cpu.branch = false;

    } else PC += 4;
}

// interpreter exception
void IPTException(uint32_t code)
{
    if (cpu.exception)
    {
        DBHalt("CPU Double Fault!\n");
    }

    // save regs
    SRR0 = PC;
    SRR1 = MSR;
    
    // disable address translation
    MSR &= ~(MSR_IR | MSR_DR);

    // Gekko exceptions are always recoverable
    MSR |= MSR_RI;

    // clear MSR[EE] when interrupt/DEC exception
    /*if((code == CPU_EXCEPTION_INTERRUPT) ||
       (code == CPU_EXCEPTION_DECREMENTER)) */MSR &= ~MSR_EE;

    // change PC and set exception flag
    PC = code;
    cpu.exception = true;
}
