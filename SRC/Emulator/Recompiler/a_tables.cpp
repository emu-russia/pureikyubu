// recompiler tables setup
#include "dolphin.h"

#include "Recompiler/X86.h"

// opcode tables
void (__fastcall *a_19[2048])(u32 op, u32 pc); // 19
void (__fastcall *a_31[2048])(u32 op, u32 pc); // 31
void (__fastcall *a_59[64])(u32 op, u32 pc);   // 59
void (__fastcall *a_63[2048])(u32 op, u32 pc); // 63
void (__fastcall *a_4 [2048])(u32 op, u32 pc); // 4

#define OP(name) void __fastcall a_##name##(u32 op, u32 pc)

static void __fastcall cpu_not_implemented(u32 op, u32 pc)
{
    char text[256];
    PPCD_CB d;

    // disassemble
    d.instr = op;
    d.pc = pc;
    PPCDisasm(&d);
    sprintf(
        text, "%08X  <%08X>  (%i, %i)  %-10s %s",
        pc, op, op >> 26, op & 0x7ff, d.mnemonic, d.operands
    );

/*/
    DolwinError( "** CPU COMPILER ERROR **",
                 "unimplemented opcode : %s\n\n",
                 text );
/*/

    // fallback interpreter
    PC = pc;
    IPTExecuteOpcode();
}

// switch to extension opcode table
static OP(OP19) { a_19[op & 0x7ff](op, pc); }
static OP(OP31) { a_31[op & 0x7ff](op, pc); }
static OP(OP59) { a_59[op &  0x3f](op, pc); }
static OP(OP63) { a_63[op & 0x7ff](op, pc); }
static OP(OP4)  { a_4 [op & 0x7ff](op, pc); }

// not implemented opcode
OP(NI)
{
    // show error, but only during runtime
    MOV_ECX_IMM(op);
    MOV_EDX_IMM(pc);
    CALLFN(&cpu_not_implemented);
}

// setup main opcode table
void (__fastcall *a_1[64])(u32 op, u32 pc) = {
 a_NI    , a_NI    , a_NI    , a_NI    , a_NI    , a_NI    , a_NI    , a_NI,
 a_NI    , a_NI    , a_CMPLI , a_CMPI  , a_NI    , a_NI    , a_ADDI  , a_ADDIS,
 a_BCX   , a_NI    , a_BX    , a_OP19  , a_RLWIMI, a_RLWINM, a_NI    , a_RLWNM,
 a_ORI   , a_ORIS  , a_NI    , a_NI    , a_NI    , a_NI    , a_NI    , a_OP31,
 a_LWZ   , a_LWZU  , a_LBZ   , a_LBZU  , a_STW   , a_STWU  , a_STB   , a_STBU,
 a_LHZ   , a_LHZU  , a_NI    , a_NI    , a_STH   , a_STHU  , a_NI    , a_NI,
 a_NI    , a_NI    , a_NI    , a_NI    , a_NI    , a_NI    , a_NI    , a_NI,
 a_NI    , a_NI    , a_NI    , a_NI    , a_NI    , a_NI    , a_NI    , a_OP63
};

// setup recompiler tables
void RECInitTables()
{
    /**** CLEAN-UP ****/

    // free compiled groups
    if(cpu.groups)
    for(u32 n=0; n<(RAMSIZE >> 2); n++)
    {
        if(cpu.groups[n] != (u8 *)RECDefaultGroup)
        {
            free(cpu.groups[n]);
            cpu.groups[n] = (u8 *)RECDefaultGroup;
        }
    }

    // free group allocation table and temporary buffer
    if(cpu.groups) { free(cpu.groups); cpu.groups = NULL; }
    if(cpu.recbuf) { free(cpu.recbuf); cpu.recbuf = NULL; }

    /**** CREATE TABLES ****/

    // allocate temporary buffer for recompilated opcodes
    ASSERT(cpu.recbuf != NULL, "Dirty recompiler state.");
    cpu.recbuf = (u8 *)malloc(RECBUFSIZE);
    ASSERT(cpu.recbuf == NULL, "Not enough memory for recompilation buffer.");

    // allocate group allocation table
    ASSERT(cpu.groups != NULL, "Dirty recompiler state.");
    cpu.groups = (u8 **)malloc(RAMSIZE);
    ASSERT(cpu.groups == NULL, "Not enough memory for group table.");

    // fill group allocation table by compile initiation *trigger*
    for(u32 n=0; n<(RAMSIZE >> 2); n++)
    {
        cpu.groups[n] = (u8 *)RECDefaultGroup;
    }

    // special "null" group for high level calls
    // contain only one x86 instruction - 'RET'
    u8 *null_group = (u8 *)malloc(2);
    ASSERT(null_group == NULL, "Huhu =)");  // haha
    null_group[0] = 0xc3;   // ret
    null_group[1] = 0xcc;   // int3
    cpu.groups[0] = null_group;

    /**** SETUP OPCODE TABLES ****/

    // set all tables to default "not implemented" opcode
    for(int i=0; i<2048; i++)
    {
        if(i < 64) a_59[i] = a_NI;
        a_19[i] = a_31[i] = a_63[i] = a_NI;
        a_4[i] = a_NI;
    }

    // "19" extension
    a_19[  32] = a_BCLR;
    a_19[  33] = a_BCLRL;
    a_19[ 300] = a_ISYNC;
    a_19[1056] = a_BCCTR;
    a_19[1057] = a_BCCTRL;

    // "31" extension
    a_31[   0] = a_CMP;
    a_31[  46] = a_LWZX;
    a_31[  64] = a_CMPL;
    a_31[ 110] = a_LWZUX;
    a_31[ 174] = a_LBZX;
    a_31[ 238] = a_LBZUX;
    a_31[ 302] = a_STWX;
    a_31[ 366] = a_STWUX;
    a_31[ 430] = a_STBX;
    a_31[ 494] = a_STBUX;
    a_31[ 558] = a_LHZX;
    a_31[ 612] = a_TLBIE;
    a_31[ 622] = a_LHZUX;
    a_31[ 678] = a_MFSPR;
    a_31[ 814] = a_STHX;
    a_31[ 878] = a_STHUX;
    a_31[ 934] = a_MTSPR;
    a_31[1132] = a_TLBSYNC;
    a_31[1196] = a_SYNC;
    a_31[1708] = a_EIEIO;

    // "59" extension

    // "63" extension

    // "4" extension

    /**** MISC TABLES ****/

}
