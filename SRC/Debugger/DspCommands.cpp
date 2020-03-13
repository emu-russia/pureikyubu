// Dsp debug commands

#include "pch.h"

void dsp_init_handlers()
{
    con.cmds["dspdisa"] = cmd_dspdisa;
    con.cmds["dregs"] = cmd_dregs;
    con.cmds["dmem"] = cmd_dmem;
    con.cmds["imem"] = cmd_imem;
    con.cmds["drun"] = cmd_drun;
    con.cmds["dstop"] = cmd_dstop;
    con.cmds["dstep"] = cmd_dstep;
    con.cmds["dbrk"] = cmd_dbrk;
    con.cmds["dlist"] = cmd_dlist;
    con.cmds["dunbrk"] = cmd_dunbrk;
    con.cmds["dpc"] = cmd_dpc;
    con.cmds["dreset"] = cmd_dreset;
    con.cmds["du"] = cmd_du;
}

void dsp_help()
{
    con_print(CYAN  "--- dsp debug commands --------------------------------------------------------");
    con_print(WHITE "    dspdisa              " NORM "- Disassemble DSP code into text file");
    con_print(WHITE "    dregs                " NORM "- Show DSP registers");
    con_print(WHITE "    dmem                 " NORM "- Dump DSP DMEM");
    con_print(WHITE "    imem                 " NORM "- Dump DSP IMEM");
    con_print(WHITE "    drun                 " NORM "- Run DSP thread until break, halt or dstop");
    con_print(WHITE "    dstop                " NORM "- Stop DSP thread");
    con_print(WHITE "    dstep                " NORM "- Step DSP instruction");
    con_print(WHITE "    dbrk                 " NORM "- Add IMEM breakpoint");
    con_print(WHITE "    dlist                " NORM "- List IMEM breakpoints");
    con_print(WHITE "    dunbrk               " NORM "- Clear all IMEM breakpoints");
    con_print(WHITE "    dpc                  " NORM "- Set DSP program counter");
    con_print(WHITE "    dreset               " NORM "- Issue DSP reset");
    con_print(WHITE "    du                   " NORM "- Disassemble some DSP instructions at program counter");
    con_print("\n");
}

// disasm dsp ucode
void cmd_dspdisa(int argc, char argv[][CON_LINELEN])
{
    if (argc < 2)
    {
        con_print("syntax: dspdisa <dsp_ucode.bin> [start_addr]\n");
        con_print("disassemble dsp ucode from binary file and dump it into dspdisa.txt\n");
        con_print("start_addr in DSP slots;\n");
        con_print("example of use: dspdisa Data\\dsp_irom.bin 0x8000\n");
    }

    size_t start_addr = 0;      // in DSP slots (halfwords)

    if (argc >= 3)
    {
        start_addr = strtoul(argv[2], nullptr, 0);
    }

    uint32_t ucodeSize = 0;
    uint8_t* ucode = (uint8_t*)FileLoad(argv[1], &ucodeSize);
    if (!ucode)
    {
        con_print("Failed to load %s\n", argv[1]);
        return;
    }

    FILE* f = nullptr;
    fopen_s(&f, "Data\\dspdisa.txt", "wt");
    if (!f)
    {
        free(ucode);
        con_print("Failed to create dsp_disa.txt\n");
        return;
    }

    uint8_t* ucodePtr = ucode;
    size_t bytesLeft = ucodeSize;
    size_t offset = 0;      // in DSP slots (halfwords)

    if (f)
    {
        fprintf(f, "// Disassembled %s\n\n", argv[1]);
    }

    while (bytesLeft != 0)
    {
        DSP::AnalyzeInfo info = { 0 };

        // Analyze

        bool result = DSP::Analyzer::Analyze(ucodePtr, ucodeSize - 2 * offset, info);
        if (!result)
        {
            con_print("DSP::Analyze failed at offset: 0x%08X\n", offset);
            break;
        }

        // Disassemble

        std::string text = DSP::DspDisasm::Disasm((uint16_t)(offset + start_addr), info);

        if (f)
        {
            fprintf(f, "%s\n", text.c_str());
        }

        offset += (info.sizeInBytes / sizeof(uint16_t));
        bytesLeft -= info.sizeInBytes;
        ucodePtr += info.sizeInBytes;
    }

    free(ucode);

    if (f)
    {
        fflush(f);
        fclose(f);
    }

    DBReport("Done.\n");
}

// Show dsp registers
void cmd_dregs(int argc, char argv[][CON_LINELEN])
{
    if (!dspCore)
    {
        DBReport("DspCore not ready\n");
        return;
    }

    // A special trick to display all registers (we intentionally make them dirty by inverting bits)
    DSP::DspRegs regsChanged = dspCore->regs;

    regsChanged.pc = ~regsChanged.pc;
    regsChanged.prod = ~regsChanged.prod;
    regsChanged.cr = ~regsChanged.cr;
    regsChanged.sr = ~regsChanged.sr;

    for (int i = 0; i < 4; i++)
    {
        regsChanged.ar[i] = ~regsChanged.ar[i];
        regsChanged.ix[i] = ~regsChanged.ix[i];
        regsChanged.gpr[i] = ~regsChanged.gpr[i];
        regsChanged.st[i] = ~regsChanged.st[i];
    }

    for (int i = 0; i < 2; i++)
    {
        regsChanged.ac[i] = ~regsChanged.ac[i];
        regsChanged.ax[i] = ~regsChanged.ax[i];
    }

    dspCore->DumpRegs(&regsChanged);
}

// Dump DSP DMEM
void cmd_dmem(int argc, char argv[][CON_LINELEN])
{
    if (!dspCore)
    {
        DBReport("DspCore not ready\n");
        return;
    }

    if (argc < 2)
    {
        con_print("syntax: dmem <dsp_addr>, dmem .\n");
        con_print("Dump 32 bytes of DMEM at dsp_addr. dsp_addr in halfword DSP slots.\n");
        con_print("dmem . will dump 0x800 bytes at dmem address 0\n");
        con_print("example of use: dmem 0x8000\n");
    }

    DSP::DspAddress dsp_addr = 0;
    size_t bytes = 32;

    if (argv[1][0] == '.')
    {
        dsp_addr = 0;
        bytes = 0x800;
    }
    else
    {
        dsp_addr = (DSP::DspAddress)strtoul(argv[1], nullptr, 0);
    }

    DBReport("DMEM Dump %i bytes\n", bytes);

    while (bytes != 0)
    {
        uint8_t* ptr = dspCore->TranslateDMem(dsp_addr);
        if (ptr == nullptr)
        {
            DBReport(_DSP "TranslateDMem failed on dsp addr: 0x%04X\n", dsp_addr);
            break;
        }

        DBReport("%04X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
            dsp_addr,
            ptr[0], ptr[1], ptr[2], ptr[3],
            ptr[4], ptr[5], ptr[6], ptr[7], 
            ptr[8], ptr[9], ptr[0xa], ptr[0xb], 
            ptr[0xc], ptr[0xd], ptr[0xe], ptr[0xf] );

        bytes -= 0x10;
        dsp_addr += 0x10;
    }
}

// Dump DSP IMEM
void cmd_imem(int argc, char argv[][CON_LINELEN])
{
    if (!dspCore)
    {
        DBReport("DspCore not ready\n");
        return;
    }

    if (argc < 2)
    {
        con_print("syntax: imem <dsp_addr>, imem .\n");
        con_print("Dump 32 bytes of IMEM at dsp_addr. dsp_addr in halfword DSP slots.\n");
        con_print("imem . will dump 32 bytes of imem at program counter address.\n");
        con_print("example of use: imem 0\n");
    }

    DSP::DspAddress dsp_addr = 0;
    size_t bytes = 32;

    if (argv[1][0] == '.')
    {
        dsp_addr = dspCore->regs.pc;
    }
    else
    {
        dsp_addr = (DSP::DspAddress)strtoul(argv[1], nullptr, 0);
    }
    
    DBReport("IMEM Dump %i bytes\n", bytes);

    while (bytes != 0)
    {
        uint8_t* ptr = dspCore->TranslateIMem(dsp_addr);
        if (ptr == nullptr)
        {
            DBReport(_DSP "TranslateIMem failed on dsp addr: 0x%04X\n", dsp_addr);
            break;
        }

        DBReport("%04X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
            dsp_addr,
            ptr[0], ptr[1], ptr[2], ptr[3],
            ptr[4], ptr[5], ptr[6], ptr[7],
            ptr[8], ptr[9], ptr[0xa], ptr[0xb],
            ptr[0xc], ptr[0xd], ptr[0xe], ptr[0xf]);

        bytes -= 0x10;
        dsp_addr += 0x10;
    }
}

// Run DSP thread until break, halt or dstop
void cmd_drun(int argc, char argv[][CON_LINELEN])
{
    if (!dspCore)
    {
        DBReport("DspCore not ready\n");
        return;
    }
       
    dspCore->Run();
}

// Stop DSP thread
void cmd_dstop(int argc, char argv[][CON_LINELEN])
{
    if (!dspCore)
    {
        DBReport("DspCore not ready\n");
        return;
    }

    dspCore->Suspend();
}

// Step DSP instruction
void cmd_dstep(int argc, char argv[][CON_LINELEN])
{
    if (!dspCore)
    {
        DBReport("DspCore not ready\n");
        return;
    }

    if (dspCore->IsRunning())
    {
        DBReport(_DSP "It is impossible while running DSP thread.\n");
        return;
    }

    DSP::DspRegs prevRegs = dspCore->regs;

    // Show instruction to be executed

    DSP::DspAddress pcAddr = dspCore->regs.pc;

    uint8_t* imemPtr = dspCore->TranslateIMem(pcAddr);
    if (imemPtr == nullptr)
    {
    	DBReport(_DSP "TranslateIMem failed on dsp addr: 0x%04X\n", pcAddr);
    	return;
    }

    DSP::AnalyzeInfo info = { 0 };

    if (!DSP::Analyzer::Analyze(imemPtr, DSP::DspCore::MaxInstructionSizeInBytes, info))
    {
        DBReport(_DSP "DSP Analyzer failed on dsp addr: 0x%04X\n", pcAddr);
        return;
    }

    std::string code = DSP::DspDisasm::Disasm(pcAddr, info);

    DBReport("%s\n", code.c_str());

    dspCore->Step();

    // Dump modified regs
    dspCore->DumpRegs(&prevRegs);
}

// Add IMEM breakpoint
void cmd_dbrk(int argc, char argv[][CON_LINELEN])
{
    if (!dspCore)
    {
        DBReport("DspCore not ready\n");
        return;
    }

    if (argc < 2)
    {
        con_print("syntax: dbrk <dsp_addr>\n");
        con_print("Add breakpoint at dsp_addr. dsp_addr in halfword DSP slots.\n");
        con_print("example of use: dbrk 0x8020\n");
    }

    DSP::DspAddress dsp_addr = (DSP::DspAddress)strtoul(argv[1], nullptr, 0);

    dspCore->AddBreakpoint(dsp_addr);

    DBReport("DSP breakpoint added: 0x%04X\n", dsp_addr);
}

// List IMEM breakpoints
void cmd_dlist(int argc, char argv[][CON_LINELEN])
{
    if (!dspCore)
    {
        DBReport("DspCore not ready\n");
        return;
    }

    DBReport("DSP breakpoints:\n");

    dspCore->ListBreakpoints();
}

// Clear all IMEM breakpoints
void cmd_dunbrk(int argc, char argv[][CON_LINELEN])
{
    if (!dspCore)
    {
        DBReport("DspCore not ready\n");
        return;
    }

    dspCore->ClearBreakpoints();

    DBReport("DSP breakpoints cleared.\n");
}

// Set DSP program counter
void cmd_dpc(int argc, char argv[][CON_LINELEN])
{
    if (!dspCore)
    {
        DBReport("DspCore not ready\n");
        return;
    }

    if (argc < 2)
    {
        con_print("syntax: dpc <dsp_addr>\n");
        con_print("Set DSP program counter to dsp_addr. dsp_addr in halfword DSP slots.\n");
        con_print("example of use: dpc 0x8000\n");
    }

    dspCore->regs.pc = (DSP::DspAddress)strtoul(argv[1], nullptr, 0);
}

// Issue DSP reset
void cmd_dreset(int argc, char argv[][CON_LINELEN])
{
    if (!dspCore)
    {
        DBReport("DspCore not ready\n");
        return;
    }

    dspCore->Reset();
}

// Disassemble some DSP instructions at program counter
void cmd_du(int argc, char argv[][CON_LINELEN])
{
    if (!dspCore)
    {
        DBReport("DspCore not ready\n");
        return;
    }

    size_t instrCount = 8;
    DSP::DspAddress addr = dspCore->regs.pc;

    while (instrCount--)
    {
        uint8_t* imemPtr = dspCore->TranslateIMem(addr);
        if (imemPtr == nullptr)
        {
            DBReport(_DSP "TranslateIMem failed on dsp addr: 0x%04X\n", addr);
            break;
        }

        DSP::AnalyzeInfo info = { 0 };

        if (!DSP::Analyzer::Analyze(imemPtr, DSP::DspCore::MaxInstructionSizeInBytes, info))
        {
            DBReport(_DSP "DSP Analyzer failed on dsp addr: 0x%04X\n", addr);
            return;
        }

        std::string code = DSP::DspDisasm::Disasm(addr, info);

        DBReport("%s\n", code.c_str());

        addr += (DSP::DspAddress)(info.sizeInBytes >> 1);
    }
}
