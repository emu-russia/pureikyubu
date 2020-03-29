// Dsp debug commands

#include "pch.h"

namespace Debug
{

    void dsp_init_handlers()
    {
        con.cmds["dspdisa"] = cmd_dspdisa;
        con.cmds["dregs"] = cmd_dregs;
        con.cmds["dreg"] = cmd_dreg;
        con.cmds["dmem"] = cmd_dmem;
        con.cmds["imem"] = cmd_imem;
        con.cmds["drun"] = cmd_drun;
        con.cmds["dstop"] = cmd_dstop;
        con.cmds["dstep"] = cmd_dstep;
        con.cmds["dbrk"] = cmd_dbrk;
        con.cmds["dcan"] = cmd_dcan;
        con.cmds["dlist"] = cmd_dlist;
        con.cmds["dbrkclr"] = cmd_dbrkclr;
        con.cmds["dcanclr"] = cmd_dcanclr;
        con.cmds["dpc"] = cmd_dpc;
        con.cmds["dreset"] = cmd_dreset;
        con.cmds["du"] = cmd_du;
        con.cmds["dst"] = cmd_dst;
        con.cmds["difx"] = cmd_difx;
        con.cmds["cpumbox"] = cmd_cpumbox;
        con.cmds["dspmbox"] = cmd_dspmbox;
        con.cmds["cpudspint"] = cmd_cpudspint;
        con.cmds["dspcpuint"] = cmd_dspcpuint;
    }

    void dsp_help()
    {
        DBReport2(DbgChannel::Header, "## DSP Debug Commands\n");
        DBReport("    dspdisa              - Disassemble DSP code into text file\n");
        DBReport("    dregs                - Show DSP registers\n");
        DBReport("    dreg <reg> <value>   - Modify DSP register\n");
        DBReport("    dmem                 - Dump DSP DMEM\n");
        DBReport("    imem                 - Dump DSP IMEM\n");
        DBReport("    drun                 - Run DSP thread until break, halt or dstop\n");
        DBReport("    dstop                - Stop DSP thread\n");
        DBReport("    dstep [n]            - Step DSP instruction(s)\n");
        DBReport("    dbrk                 - Add IMEM breakpoint\n");
        DBReport("    dcan                 - Add IMEM canary\n");
        DBReport("    dlist                - List IMEM breakpoints\n");
        DBReport("    dbrkclr              - Clear all IMEM breakpoints\n");
        DBReport("    dcanclr              - Clear all IMEM canaries\n");
        DBReport("    dpc                  - Set DSP program counter\n");
        DBReport("    dreset               - Issue DSP reset\n");
        DBReport("    du [addr] [count]    - Disassemble some DSP instructions at pc / address\n");
        DBReport("    dst                  - Dump DSP call stack\n");
        DBReport("    difx                 - Dump DSP IFX (internal hardware)\n");
        DBReport("    cpumbox              - Write message to CPU Mailbox\n");
        DBReport("    dspmbox              - Read message from DSP Mailbox\n");
        DBReport("    cpudspint            - Send CPU->DSP interrupt\n");
        DBReport("    dspcpuint            - Send DSP->CPU interrupt\n");
        DBReport("\n");
    }

    // disasm dsp ucode to file
    Json::Value* cmd_dspdisa(std::vector<std::string>& args)
    {
        if (args.size() < 2)
        {
            DBReport("syntax: dspdisa <dsp_ucode.bin> [start_addr]\n");
            DBReport("disassemble dsp ucode from binary file and dump it into dspdisa.txt\n");
            DBReport("start_addr in DSP slots;\n");
            DBReport("example of use: dspdisa Data\\dsp_irom.bin 0x8000\n");
            return nullptr;
        }

        size_t start_addr = 0;      // in DSP slots (halfwords)

        if (args.size() >= 3)
        {
            start_addr = strtoul(args[2].c_str(), nullptr, 0);
        }

        size_t ucodeSize = 0;
        uint8_t* ucode = (uint8_t*)UI::FileLoad(args[1].c_str(), &ucodeSize);
        if (!ucode)
        {
            DBReport("Failed to load %s\n", args[1].c_str());
            return nullptr;
        }

        FILE* f = nullptr;
        fopen_s(&f, "Data\\dspdisa.txt", "wt");
        if (!f)
        {
            free(ucode);
            DBReport("Failed to create dsp_disa.txt\n");
            return nullptr;
        }

        uint8_t* ucodePtr = ucode;
        size_t bytesLeft = ucodeSize;
        size_t offset = 0;      // in DSP slots (halfwords)

        if (f)
        {
            fprintf(f, "// Disassembled %s\n\n", args[1].c_str());
        }

        while (bytesLeft != 0)
        {
            DSP::AnalyzeInfo info = { 0 };

            // Analyze

            bool result = DSP::Analyzer::Analyze(ucodePtr, ucodeSize - 2 * offset, info);
            if (!result)
            {
                DBReport("DSP::Analyze failed at offset: 0x%08X\n", offset);
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
        return nullptr;
    }

    // Show dsp registers
    Json::Value* cmd_dregs(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        // A special trick to display all registers (we intentionally make them dirty by inverting bits)
        DSP::DspRegs regsChanged = DspCore->regs;

        regsChanged.pc = ~regsChanged.pc;
        regsChanged.prod.bitsUnpacked = ~regsChanged.prod.bitsUnpacked;
        regsChanged.bank = ~regsChanged.bank;
        regsChanged.sr.bits = ~regsChanged.sr.bits;

        for (int i = 0; i < 4; i++)
        {
            regsChanged.ar[i] = ~regsChanged.ar[i];
            regsChanged.ix[i] = ~regsChanged.ix[i];
            regsChanged.gpr[i] = ~regsChanged.gpr[i];
        }

        for (int i = 0; i < 2; i++)
        {
            regsChanged.ac[i].bits = ~regsChanged.ac[i].bits;
            regsChanged.ax[i].bits = ~regsChanged.ax[i].bits;
        }

        DspCore->DumpRegs(&regsChanged);
        return nullptr;
    }

    // Dump DSP DMEM
    Json::Value* cmd_dmem(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        if (args.size() < 2)
        {
            DBReport("syntax: dmem <dsp_addr>, dmem .\n");
            DBReport("Dump 32 bytes of DMEM at dsp_addr. dsp_addr in halfword DSP slots.\n");
            DBReport("dmem . will dump 0x800 bytes at dmem address 0\n");
            DBReport("example of use: dmem 0x8000\n");
            return nullptr;
        }

        DSP::DspAddress dsp_addr = 0;
        size_t bytes = 32;

        if (args[1].c_str()[0] == '.')
        {
            dsp_addr = 0;
            bytes = 0x800;
        }
        else
        {
            dsp_addr = (DSP::DspAddress)strtoul(args[1].c_str(), nullptr, 0);
        }

        DBReport("DMEM Dump %i bytes\n", bytes);

        while (bytes != 0)
        {
            uint8_t* ptr = DspCore->TranslateDMem(dsp_addr);
            if (ptr == nullptr)
            {
                DBReport2(DbgChannel::DSP, "TranslateDMem failed on dsp addr: 0x%04X\n", dsp_addr);
                break;
            }

            DBReport("%04X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                dsp_addr,
                ptr[0], ptr[1], ptr[2], ptr[3],
                ptr[4], ptr[5], ptr[6], ptr[7],
                ptr[8], ptr[9], ptr[0xa], ptr[0xb],
                ptr[0xc], ptr[0xd], ptr[0xe], ptr[0xf]);

            bytes -= 0x10;
            dsp_addr += 8;
        }
        return nullptr;
    }

    // Dump DSP IMEM
    Json::Value* cmd_imem(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        if (args.size() < 2)
        {
            DBReport("syntax: imem <dsp_addr>, imem .\n");
            DBReport("Dump 32 bytes of IMEM at dsp_addr. dsp_addr in halfword DSP slots.\n");
            DBReport("imem . will dump 32 bytes of imem at program counter address.\n");
            DBReport("example of use: imem 0\n");
            return nullptr;
        }

        DSP::DspAddress dsp_addr = 0;
        size_t bytes = 32;

        if (args[1].c_str()[0] == '.')
        {
            dsp_addr = DspCore->regs.pc;
        }
        else
        {
            dsp_addr = (DSP::DspAddress)strtoul(args[1].c_str(), nullptr, 0);
        }

        DBReport("IMEM Dump %i bytes\n", bytes);

        while (bytes != 0)
        {
            uint8_t* ptr = DspCore->TranslateIMem(dsp_addr);
            if (ptr == nullptr)
            {
                DBReport2(DbgChannel::DSP, "TranslateIMem failed on dsp addr: 0x%04X\n", dsp_addr);
                break;
            }

            DBReport("%04X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                dsp_addr,
                ptr[0], ptr[1], ptr[2], ptr[3],
                ptr[4], ptr[5], ptr[6], ptr[7],
                ptr[8], ptr[9], ptr[0xa], ptr[0xb],
                ptr[0xc], ptr[0xd], ptr[0xe], ptr[0xf]);

            bytes -= 0x10;
            dsp_addr += 8;
        }
        return nullptr;
    }

    // Run DSP thread until break, halt or dstop
    Json::Value* cmd_drun(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        DspCore->Run();
        return nullptr;
    }

    // Stop DSP thread
    Json::Value* cmd_dstop(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        DspCore->Suspend();
        return nullptr;
    }

    // Step DSP instruction
    Json::Value* cmd_dstep(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        if (DspCore->IsRunning())
        {
            DBReport2(DbgChannel::DSP, "It is impossible while running DSP thread.\n");
            return nullptr;
        }

        int n = 1;

        if (args.size() > 1)
        {
            n = atoi(args[1].c_str());
        }

        while (n--)
        {
            DSP::DspRegs prevRegs = DspCore->regs;

            // Show instruction to be executed

            DSP::DspAddress pcAddr = DspCore->regs.pc;

            uint8_t* imemPtr = DspCore->TranslateIMem(pcAddr);
            if (imemPtr == nullptr)
            {
                DBReport2(DbgChannel::DSP, "TranslateIMem failed on dsp addr: 0x%04X\n", pcAddr);
                return nullptr;
            }

            DSP::AnalyzeInfo info = { 0 };

            if (!DSP::Analyzer::Analyze(imemPtr, DSP::DspCore::MaxInstructionSizeInBytes, info))
            {
                DBReport2(DbgChannel::DSP, "DSP Analyzer failed on dsp addr: 0x%04X\n", pcAddr);
                return nullptr;
            }

            std::string code = DSP::DspDisasm::Disasm(pcAddr, info);

            DBReport("%s\n", code.c_str());

            DspCore->Step();

            // Dump modified regs
            DspCore->DumpRegs(&prevRegs);
        }
        return nullptr;
    }

    // Add IMEM breakpoint
    Json::Value* cmd_dbrk(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        if (args.size() < 2)
        {
            DBReport("syntax: dbrk <dsp_addr>\n");
            DBReport("Add breakpoint at dsp_addr. dsp_addr in halfword DSP slots.\n");
            DBReport("example of use: dbrk 0x8020\n");
            return nullptr;
        }

        DSP::DspAddress dsp_addr = (DSP::DspAddress)strtoul(args[1].c_str(), nullptr, 0);

        DspCore->AddBreakpoint(dsp_addr);

        DBReport("DSP breakpoint added: 0x%04X\n", dsp_addr);
        return nullptr;
    }

    // Add IMEM canary
    Json::Value* cmd_dcan(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        if (args.size() < 3)
        {
            DBReport("syntax: dcan <dsp_addr> <message>\n");
            DBReport("Add canary at dsp_addr. dsp_addr in halfword DSP slots.\n");
            DBReport("When the PC is equal to the canary address, a debug message is displayed\n");
            DBReport("example of use: dcan 0x10 \"Ucode entrypoint\"\n");
            return nullptr;
        }

        DSP::DspAddress dsp_addr = (DSP::DspAddress)strtoul(args[1].c_str(), nullptr, 0);

        DspCore->AddCanary(dsp_addr, args[2]);

        DBReport("DSP canary added: 0x%04X\n", dsp_addr);
        return nullptr;
    }

    // List IMEM breakpoints and canaries
    Json::Value* cmd_dlist(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        DBReport("DSP breakpoints:\n");

        DspCore->ListBreakpoints();

        DBReport("DSP canaries:\n");

        DspCore->ListCanaries();
        return nullptr;
    }

    // Clear all IMEM breakpoints
    Json::Value* cmd_dbrkclr(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        DspCore->ClearBreakpoints();

        DBReport("DSP breakpoints cleared.\n");
        return nullptr;
    }

    // Clear all IMEM canaries
    Json::Value* cmd_dcanclr(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        DspCore->ClearCanaries();

        DBReport("DSP canaries cleared.\n");
        return nullptr;
    }

    // Set DSP program counter
    Json::Value* cmd_dpc(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        if (DspCore->IsRunning())
        {
            DBReport2(DbgChannel::DSP, "It is impossible while running DSP thread.\n");
            return nullptr;
        }

        if (args.size() < 2)
        {
            DBReport("syntax: dpc <dsp_addr>\n");
            DBReport("Set DSP program counter to dsp_addr. dsp_addr in halfword DSP slots.\n");
            DBReport("example of use: dpc 0x8000\n");
            return nullptr;
        }

        DspCore->regs.pc = (DSP::DspAddress)strtoul(args[1].c_str(), nullptr, 0);
        return nullptr;
    }

    // Issue DSP reset
    Json::Value* cmd_dreset(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        DspCore->HardReset();
        return nullptr;
    }

    // Disassemble some DSP instructions at program counter
    Json::Value* cmd_du(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        if (DspCore->IsRunning())
        {
            DBReport2(DbgChannel::DSP, "It is impossible while running DSP thread.\n");
            return nullptr;
        }

        size_t instrCount = 8;
        DSP::DspAddress addr = 0;

        if (args.size() < 2)
        {
            addr = DspCore->regs.pc;
        }
        else
        {
            addr = (DSP::DspAddress)strtoul(args[1].c_str(), nullptr, 0);
        }

        if (args.size() < 3)
        {
            instrCount = 8;
        }
        else
        {
            instrCount = atoi(args[2].c_str());
        }

        while (instrCount--)
        {
            uint8_t* imemPtr = DspCore->TranslateIMem(addr);
            if (imemPtr == nullptr)
            {
                DBReport2(DbgChannel::DSP, "TranslateIMem failed on dsp addr: 0x%04X\n", addr);
                break;
            }

            DSP::AnalyzeInfo info = { 0 };

            if (!DSP::Analyzer::Analyze(imemPtr, DSP::DspCore::MaxInstructionSizeInBytes, info))
            {
                DBReport2(DbgChannel::DSP, "DSP Analyzer failed on dsp addr: 0x%04X\n", addr);
                return nullptr;
            }

            std::string code = DSP::DspDisasm::Disasm(addr, info);

            DBReport("%s\n", code.c_str());

            addr += (DSP::DspAddress)(info.sizeInBytes >> 1);
        }
        return nullptr;
    }

    // Dump DSP call stack
    Json::Value* cmd_dst(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        if (DspCore->IsRunning())
        {
            DBReport2(DbgChannel::DSP, "It is impossible while running DSP thread.\n");
            return nullptr;
        }

        DBReport("DSP Call Stack:\n");

        for (auto it = DspCore->regs.st[0].begin(); it != DspCore->regs.st[0].end(); ++it)
        {
            DBReport("0x%04X\n", *it);
        }
        return nullptr;
    }

    // Dump DSP IFX
    Json::Value* cmd_difx(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        if (DspCore->IsRunning())
        {
            DBReport2(DbgChannel::DSP, "It is impossible while running DSP thread.\n");
            return nullptr;
        }

        DBReport("DSP IFX Dump:\n");

        DspCore->DumpIfx();
        return nullptr;
    }

    // Write message to CPU Mailbox
    Json::Value* cmd_cpumbox(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        if (args.size() < 2)
        {
            DBReport("syntax: cpumbox <value>\n");
            DBReport("example of use: cpumbox 0x8001FEED\n");
            return nullptr;
        }

        uint32_t value = strtoul(args[1].c_str(), nullptr, 0);
        
        DspCore->CpuToDspWriteHi(value >> 16);
        DspCore->CpuToDspWriteLo((uint16_t)value);
        return nullptr;
    }

    // Read message from DSP Mailbox
    Json::Value* cmd_dspmbox(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        uint32_t value = 0;

        value |= DspCore->DspToCpuReadHi(true) << 16;
        if ((value & 0x80000000) == 0)
        {
            DBReport("No DSP message.\n");
            return nullptr;
        }
        value |= DspCore->DspToCpuReadLo();
        DBReport("DSP Message: 0x%08X\n", value);
        return nullptr;
    }

    // Send CPU->DSP interrupt
    Json::Value* cmd_cpudspint(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        DspCore->DSPSetIntBit(true);
        return nullptr;
    }

    // Send DSP->CPU interrupt
    Json::Value* cmd_dspcpuint(std::vector<std::string>& args)
    {
        DSPAssertInt();
        return nullptr;
    }

    // Modify DSP register
    Json::Value* cmd_dreg(std::vector<std::string>& args)
    {
        if (!DspCore)
        {
            DBReport("DspCore not ready\n");
            return nullptr;
        }

        if (DspCore->IsRunning())
        {
            DBReport2(DbgChannel::DSP, "It is impossible while running DSP thread.\n");
            return nullptr;
        }

        static const char* dspRegNames[] = {
		    "ar0", "ar1", "ar2", "ar3",
            "ix0", "ix1", "ix2", "ix3",
            "r8", "r9", "r10", "r11",
            "st0", "st1", "st2", "st3",
            "ac0h", "ac1h",
            "config", "sr",
            "prodl", "prodm1", "prodh", "prodm2",
            "ax0l", "ax0h", "ax1l", "ax1h",
            "ac0l", "ac1l", "ac0m", "ac1m"
        };

        if (args.size() < 3)
        {
            std::string reglist = "";

            int regscount = 0;

            for (int i = 0; i < _countof(dspRegNames); i++)
            {
                reglist += std::string(dspRegNames[i]) + " ";
                regscount++;
                if (regscount >= 8)
                {
                    regscount = 0;
                    reglist += "\n";
                }
            }

            DBReport("syntax: dreg <register> <value>\n");
            DBReport("Register names: %s\n", reglist.c_str());
            DBReport("example of use: dreg ar0 0x300\n");

            return nullptr;
        }

        uint16_t value = (uint16_t)strtoul(args[2].c_str(), nullptr, 0);

        int regIndex = -1;

        for (int i = 0; i < _countof(dspRegNames); i++)
        {
            if (!_stricmp(args[1].c_str(), dspRegNames[i]))
            {
                regIndex = i;
                break;
            }
        }

        if (regIndex < 0)
        {
            DBReport("Invalid register name: %s\n", args[1].c_str());
            return nullptr;
        }

        DspCore->MoveToReg(regIndex, value);
        return nullptr;
    }

}
