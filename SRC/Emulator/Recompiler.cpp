// default recompiler (DYNAREC, JITC)
#include "dolphin.h"

#include "Recompiler/X86.h"

// ---------------------------------------------------------------------------

// return size of group in opcodes.
// note : group is including last branch!
u32 RECGroupSize(u32 start)
{
    // align for 4 bytes and translate
    u32 pa = MEMEffectiveToPhysical(start & ~3);
    u32 cnt = 1;

    while(cnt < CPU_MAX_GROUP)
    {
        register u32 op = MEMFetch(pa);

        // check opcode. if branch, then return.
        switch(op >> 26)
        {
            case 020:           // bc*
            case 022:           // b*
                goto enough;

            case 023:
            switch(op & 0x7ff)
            {
                case 32:        // bclr
                case 33:        // bclrl
                case 1056:      // bcctr
                case 1057:      // bcctrl
                    goto enough;
            }
        }

        // keep checking
        pa += 4;
        cnt++;
    }

enough:

    // overflow detected. I dont know BIGGEST game function, but
    // we assume, that 10000 opcodes will be enough even for
    // crazy developer :)
    if(cnt >= CPU_MAX_GROUP)
    {
        DBReport( YEL
                  "** CPU WARNING **\n"
                  "very huge function at %08X\n",
                  start );
    }

    return cnt;                 // in opcodes
}

static void dump_group(char *fname, u32 ea)
{
    int len;
    u8 *code;

    FILE *f = fopen(fname, "w");
    if(!f) return;

    fprintf(f, "compiler group at %08X\n\n", ea);
    ea = MEMEffectiveToPhysical(ea);
    if(ea == 0xffffffff)
    {
        fprintf(f, "no memory there.\n");
        return;
    }
    code = cpu.groups[ea >> 2];
    if((u32)code == (u32)RECDefaultGroup)
    {
        fprintf(f, "not compiled.\n");
        return;
    }

    int i = CPU_MAX_GROUP;
    while(i)
    {
        if(*code == 0xcc) break;    // int3
        char *d = dasm86(code, (int)code, &len);
        fprintf(f, ":%08X%s\n", (u32)code, d);
        code += len;
        i -= len;
    }

    fclose(f);
}

static void __fastcall _trace(u32 pc)
{
    DolwinReport(
        "executing %08X\n\n"
        "r0 =%08X\tr8 =%08X\tr16=%08X\tr24=%08X\n"
        "r1 =%08X\tr9 =%08X\tr17=%08X\tr25=%08X\n"
        "r2 =%08X\tr10=%08X\tr18=%08X\tr26=%08X\n"
        "r3 =%08X\tr11=%08X\tr19=%08X\t270=%08X\n"
        "r4 =%08X\tr12=%08X\tr20=%08X\tr28=%08X\n"
        "r5 =%08X\tr13=%08X\tr21=%08X\tr29=%08X\n"
        "r6 =%08X\tr14=%08X\tr22=%08X\tr30=%08X\n"
        "r7 =%08X\tr15=%08X\tr23=%08X\tr31=%08X\n" , 
        pc,
        GPR[ 0], GPR[ 8], GPR[16], GPR[24],
        GPR[ 1], GPR[ 9], GPR[17], GPR[25],
        GPR[ 2], GPR[10], GPR[18], GPR[26],
        GPR[ 3], GPR[11], GPR[19], GPR[27],
        GPR[ 4], GPR[12], GPR[20], GPR[28],
        GPR[ 5], GPR[13], GPR[21], GPR[29],
        GPR[ 6], GPR[14], GPR[22], GPR[30],
        GPR[ 7], GPR[15], GPR[23], GPR[31]
    );
}

// recompile single group
void * __fastcall RECCompileGroup(u32 start)
{
    PC = start;                 // update PC
    cpu.recptr = 0;             // reset recbuf position

    u32 groupSize = RECGroupSize(start);
    u32 pc = start, pa = MEMEffectiveToPhysical(start);
    u32 spa = pa;               // save start address

    // call CPU update
    {
        MOV_DD_IMM(&PC, start);
        //CALLFN(CPUTick);
        CALLFN(HWUpdate);
    }

    // recompile opcode by opcode
    u32 oldSize = groupSize;
    while(groupSize--)
    {
        if(emu.doldebug) NOP();

        //MOV_ECX_IMM(pc);
        //CALLFN(_trace);

        INC_DD(&cpu.ops);

        if(comp.started)
        {
            MOV_DD_IMM(&PC, pc);
            CALLFN(COMPDoCompare);
        }

        CALLFN(CPUTick);
        CALLFN(HWUpdate);

        u32 op = MEMFetch(pa);
        a_1[op >> 26](op, pc);

        ASSERT(cpu.recptr >= RECBUFSIZE, "Recompiler overflow!");
        pc += 4, pa += 4;
    }
    groupSize = oldSize;

    // end of group (for x86 disassembly)
    RET();
    INT3();

    // allocate memory for new group, and copy just
    // compiled group there (from temporary buffer).
    u8 *group = (u8 *)malloc(cpu.recptr);
    if(group == NULL)
    {
        DolwinError( "RECCompileGroup in Emulator\\Recompiler.cpp",
                     "Not enough memory for new group (%i opcodes)!",
                     groupSize );
    }
    memcpy(group, cpu.recbuf, cpu.recptr);

    // "dirty" group ?
    if(cpu.groups[spa >> 2] != (u8 *)RECDefaultGroup)
    {
        DBReport( YEL
                  "** CPU WARNING **\n"
                  "dirty group at %08X\n",
                  start );

        // deallocate previous dirty group (to replace by new one)
        free(cpu.groups[spa >> 2]);
    }

    // update group allocation table
    cpu.groups[spa >> 2] = group;
    
    // dump new group
#if 0
    if(cpu.recptr >= RECBUFSIZE)
    {
        char path[256];
        sprintf(path, ".\\REC\\%08X.bin", start);
        FILE *f1 = fopen(path, "wb");
        if(f1)
        {
            fwrite(group, cpu.recptr, 1, f1);
            fclose(f1);
        }
    }
    else
    {
        char path[256];
        sprintf(path, ".\\REC\\%08X.txt", start);
        dump_group(path, start);
    }
#endif

    return group;
}

// compile initiation *trigger*
__declspec(naked) void __fastcall RECDefaultGroup(u32 addr)
{
    __asm   call    RECCompileGroup
    __asm   jmp     eax
    __asm   _emit   0xcc    // int3
}

// "flush" some recompiler memory. can be used for self-modifying code.
void RECFlushRange(u32 addr, u32 size)
{
    // align and translate
    addr = MEMEffectiveToPhysical(addr & ~3);
    if(size & 3) size = (size & ~3) + 4;
    register u32 cur = addr, end = addr + size;
    cur >>= 2, end >>= 2;

    while(cur < end)
    {
        if(cpu.groups[cur] != (u8 *)RECDefaultGroup)
        {
            free(cpu.groups[cur]);
            cpu.groups[cur] = (u8 *)RECDefaultGroup;
        }
        cur++;
    }
}

// GO!
void RECStart()
{
    u32 pa = MEMEffectiveToPhysical(PC & ~3);
    void (__fastcall *group)(u32) = (void (__fastcall *)(u32))cpu.groups[pa >> 2];
    group(PC);
}

// compiler exception
void RECException(u32 code)
{
    DolwinQuestion("Default recompilator", "Compiler exception generated!");
}

/*/
    PONG Status (missing opcodes) :

        mulli   (integer)
        or      (logical)
        srawi   (integer shift)
        subfic  (integer)
        add     (integer)
        sub     (integer)
        crxor   (condition register logical)
        extsb   (integer)
/*/
