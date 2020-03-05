// default C interpreter (opcode parser).
#include "dolphin.h"

// start to execute from PC
void IPTStart()
{
    for(;;)
    {
        IPTExecuteOpcode();
    }
}

static void log_opcode(uint32_t op)
{
    char dump[1024];

    sprintf(dump,
        "\n"
        "r0 :%08X\tr8 :%08X\tr16:%08X\tr24:%08X\n"
        "sp :%08X\tr9 :%08X\tr17:%08X\tr25:%08X\n"
        "sd2:%08X\tr10:%08X\tr18:%08X\tr26:%08X\n"
        "r3 :%08X\tr11:%08X\tr19:%08X\tr27:%08X\n"
        "r4 :%08X\tr12:%08X\tr20:%08X\tr28:%08X\n"
        "r5 :%08X\tsd1:%08X\tr21:%08X\tr29:%08X\n"
        "r6 :%08X\tr14:%08X\tr22:%08X\tr30:%08X\n"
        "r7 :%08X\tr15:%08X\tr23:%08X\tr31:%08X\n"
        "\n"
        "lr :%08X\tcr :%08X\tdec:%08X\n"
        "pc :%08X\txer:%08X\tctr:%08X\n",
        GPR[ 0], GPR[ 8], GPR[16], GPR[24],
        GPR[ 1], GPR[ 9], GPR[17], GPR[25],
        GPR[ 2], GPR[10], GPR[18], GPR[26],
        GPR[ 3], GPR[11], GPR[19], GPR[27],
        GPR[ 4], GPR[12], GPR[20], GPR[28],
        GPR[ 5], GPR[13], GPR[21], GPR[29],
        GPR[ 6], GPR[14], GPR[22], GPR[30],
        GPR[ 7], GPR[15], GPR[23], GPR[31],
        LR, CR, DEC,
        PC, XER, CTR
    );

    PPCD_CB d;
    d.instr = op;
    d.pc = PC;
    PPCDisasm (&d);
    DBReport( CPU "%08X: %08X %-10s %s\n%s\n",
              PC, op, d.mnemonic, d.operands, dump );
}

// parse and execute single opcode
void IPTExecuteOpcode()
{
    // execute one instruction
    // (possible CPU_EXCEPTION_DSI, ISI, ALIGN, PROGRAM, FPUNAVAIL, SYSCALL)
    uint32_t op = MEMFetch(PC);
    if(cpu.log) log_opcode(op);
    if(cpu.exception) goto JumpPC;  // ISI
    c_1[op >> 26](op); cpu.ops++;
    if(cpu.exception) goto JumpPC;  // DSI, ALIGN, PROGRAM, FPUNA, SC

    // according to manual, decrementer has lower priority rather external interrupt
    
    // time to update HW ? (possible CPU_EXCEPTION_INTERRUPT)
    if(cpu.branch)
    {
        cpu.bailout--;
        if(cpu.bailout <= 0)
        {
            cpu.bailout = cpu.bailtime;
            HWUpdate();
        }
        if(cpu.exception) goto JumpPC;
    }

    // modify CPU counters (possible CPU_EXCEPTION_DECREMENTER)
    CPUTick();
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
        cpu.exception = FALSE;
        cpu.branch = FALSE;

        // remap instructions
        if(mem.ir)
        {
            MEMDoRemap(1, 0);
            mem.ir = 0;
        }

    } else PC += 4;
}

// interpreter exception
void IPTException(uint32_t code)
{
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
    cpu.exception = TRUE;
}

/*/ ---------------------------------------------------------------------------

    Compatibility Notes :
    ---------------------

    branch          - seems ok (full)
    compare         - seems ok (full)
    condition       - seems ok (full)
    floatingpoint   - CR1? FPSCR correct bit controls? frsp? fp unavail? comments?
    fploadstore     - seems ok (full)
    integer         - overflow opcodes? missing?
    loadstore       - lswx? stswx?
    logical         - seems ok (full)
    pairedsingle    - CR1? FPSCR correct bit controls? fp unavail? comments?
    psloadstore     - check scaling?
    rotate          - seems ok (full)
    shift           - seems ok (full)
    system          - cache?, MMU?, Gekko specific (DMA, WPAR, PMC)?

--------------------------------------------------------------------------- /*/
