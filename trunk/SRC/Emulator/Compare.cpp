// Dolwin CPU compare engine. thanks to ector for idea ;)
#include "dolphin.h"

// Processing :
//      Step 1: Run 1st Dolwin.
//      Step 2: Load file in debugger, and start compare as server
//      Step 3: Run 2nd Dolwin.
//      Step 4: Switch core to recompiler, close debugger,
//              and boot same file as 1st. Start compare as client.
//      Step 5: After 2nd file boot, CPU compare will start to sync both Dolwins
//      Step 6: Press F11 in 1st Dolwin debugger to execute opcode
//      Step 7: 2nd Dolwin will execute too
//      Step 8: Goto step 6.
// (Run Once must be enabled, to allow multiple Dolwin instancies)

// compare engine externals
CompareControl comp;

#define PIPENAME    "\\\\.\\pipe\\cpucompare"

// ---------------------------------------------------------------------------
// API

void COMPCreateServer()             // create compare server
{
    // close active connections
    COMPDisconnect();

    comp.pipe = CreateNamedPipe(
        PIPENAME,
        PIPE_ACCESS_OUTBOUND,
        PIPE_WAIT | PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
        1,
        0x1000,
        0x1000,
        INFINITE,
        0
    );

    if(comp.pipe == INVALID_HANDLE_VALUE)
    {
        DBReport(YEL "COMP: server is already created\n");
        return;
    }

    comp.server  = TRUE;
    comp.started = TRUE;
    DBReport(YEL "COMP: started as server\n\n");
}

void COMPConnectClient()            // connect as client
{
    // close active connections
    COMPDisconnect();

    comp.pipe = CreateFile(
        PIPENAME,
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if(comp.pipe == INVALID_HANDLE_VALUE)
    {
        DBReport(YEL "COMP: no server\n");
        return;
    }

    comp.server  = FALSE;
    comp.started = TRUE;
    DBReport(YEL "COMP: connected as client\n\n");
}

void COMPDisconnect()               // close connection
{
    if(!comp.started) return;

    DisconnectNamedPipe(comp.pipe);
    CloseHandle(comp.pipe);

    DBReport(YEL "COMP: disconnected\n\n");
    comp.started = FALSE;
}

static void fill_ppc_context(CompareData * data)
{
    data->pc    = PC;
    data->cr    = CR;
    data->tbr   = UTBR;
    data->msr   = MSR;
    data->fpscr = FPSCR;
    for(int i=0; i<32; i++) data->gpr[i]    = GPR[i];
    for(i=0; i<32; i++)     data->fp_ps0[i] = cpu.fpr[i].uval;
    for(i=0; i<32; i++)     data->ps1[i]    = cpu.ps1[i].uval;
    for(i=0; i<1024; i++)   data->spr[i]    = SPR[i];
    for(i=0; i<16; i++)     data->sr[i]     = SR[i];
}

static void cpu_dump(char * dump)
{
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
}

void COMPDoCompare()                // sync compare
{
    if(!comp.started) return;

    if(comp.server)
    {
        u32 written;
        CompareData host;

        fill_ppc_context(&host);
        HRESULT res = WriteFile(comp.pipe, &host, sizeof(CompareData), &written, NULL);
        if(FAILED(res))
        {
            DBReport(YEL "COMP: connection aborted by client\n");
            COMPDisconnect();
        }
    }
    else
    {
        u32 readen;
        CompareData remote;

        HRESULT res = ReadFile(comp.pipe, &remote, sizeof(CompareData), &readen, NULL);
        if(FAILED(res))
        {
            DBReport(YEL "COMP: connection aborted by server\n");
            COMPDisconnect();
        }
        else
        {
            char dump[1024];
            CompareData host;
            fill_ppc_context(&host);

            // do actual compare
            if(host.pc != remote.pc)
            {
                cpu_dump(dump);
                if(emu.doldebug) DBHalt("COMP: difference in program counter! (pc: %08X)\n", PC);
                else DolwinQuestion("CPU Compare", "difference in program counter! (pc: %08X)\n\n%s", PC, dump);
            }
            if(memcmp(&host.gpr, &remote.gpr, 32*4))
            {
                cpu_dump(dump);
                if(emu.doldebug) DBHalt("COMP: difference in GPRs! (pc: %08X)\n", PC);
                else DolwinQuestion("CPU Compare", "difference in GPRs! (pc: %08X)\n\n%s", PC, dump);
            }
            if(host.cr != remote.cr)
            {
                cpu_dump(dump);
                if(emu.doldebug) DBHalt("COMP: difference in condition register! (pc: %08X)\n", PC);
                else DolwinQuestion("CPU Compare", "difference in condition register! (pc: %08X)\n\n%s", PC, dump);
            }
        }
    }
}
