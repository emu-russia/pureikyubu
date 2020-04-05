// Dsp debug commands

#include "pch.h"
#include "../UI/UserFile.h"

namespace DSP
{

    // disasm dsp ucode to file
    static Json::Value* cmd_dspdisa(std::vector<std::string>& args)
    {
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
    static Json::Value* cmd_dregs(std::vector<std::string>& args)
    {
        // A special trick to display all registers (we intentionally make them dirty by inverting bits)
        DSP::DspRegs regsChanged = Flipper::HW->DSP->regs;

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

        Flipper::HW->DSP->DumpRegs(&regsChanged);
        return nullptr;
    }

    // Dump DSP DMEM
    static Json::Value* cmd_dmem(std::vector<std::string>& args)
    {
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
            uint8_t* ptr = Flipper::HW->DSP->TranslateDMem(dsp_addr);
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
    static Json::Value* cmd_imem(std::vector<std::string>& args)
    {
        DSP::DspAddress dsp_addr = 0;
        size_t bytes = 32;

        if (args[1].c_str()[0] == '.')
        {
            dsp_addr = Flipper::HW->DSP->regs.pc;
        }
        else
        {
            dsp_addr = (DSP::DspAddress)strtoul(args[1].c_str(), nullptr, 0);
        }

        DBReport("IMEM Dump %i bytes\n", bytes);

        while (bytes != 0)
        {
            uint8_t* ptr = Flipper::HW->DSP->TranslateIMem(dsp_addr);
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
    static Json::Value* cmd_drun(std::vector<std::string>& args)
    {
        Flipper::HW->DSP->Run();
        return nullptr;
    }

    // Stop DSP thread
    static Json::Value* cmd_dstop(std::vector<std::string>& args)
    {
        Flipper::HW->DSP->Suspend();
        return nullptr;
    }

    // Step DSP instruction
    static Json::Value* cmd_dstep(std::vector<std::string>& args)
    {
        if (Flipper::HW->DSP->IsRunning())
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
            DSP::DspRegs prevRegs = Flipper::HW->DSP->regs;

            // Show instruction to be executed

            DSP::DspAddress pcAddr = Flipper::HW->DSP->regs.pc;

            uint8_t* imemPtr = Flipper::HW->DSP->TranslateIMem(pcAddr);
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

            Flipper::HW->DSP->Step();

            // Dump modified regs
            Flipper::HW->DSP->DumpRegs(&prevRegs);
        }
        return nullptr;
    }

    // Add IMEM breakpoint
    static Json::Value* cmd_dbrk(std::vector<std::string>& args)
    {
        DSP::DspAddress dsp_addr = (DSP::DspAddress)strtoul(args[1].c_str(), nullptr, 0);

        Flipper::HW->DSP->AddBreakpoint(dsp_addr);

        DBReport("DSP breakpoint added: 0x%04X\n", dsp_addr);
        return nullptr;
    }

    // Add IMEM canary
    static Json::Value* cmd_dcan(std::vector<std::string>& args)
    {
        DSP::DspAddress dsp_addr = (DSP::DspAddress)strtoul(args[1].c_str(), nullptr, 0);

        Flipper::HW->DSP->AddCanary(dsp_addr, args[2]);

        DBReport("DSP canary added: 0x%04X\n", dsp_addr);
        return nullptr;
    }

    // List IMEM breakpoints and canaries
    static Json::Value* cmd_dlist(std::vector<std::string>& args)
    {
        DBReport("DSP breakpoints:\n");

        Flipper::HW->DSP->ListBreakpoints();

        DBReport("DSP canaries:\n");

        Flipper::HW->DSP->ListCanaries();
        return nullptr;
    }

    // Clear all IMEM breakpoints
    static Json::Value* cmd_dbrkclr(std::vector<std::string>& args)
    {
        Flipper::HW->DSP->ClearBreakpoints();

        DBReport("DSP breakpoints cleared.\n");
        return nullptr;
    }

    // Clear all IMEM canaries
    static Json::Value* cmd_dcanclr(std::vector<std::string>& args)
    {
        Flipper::HW->DSP->ClearCanaries();

        DBReport("DSP canaries cleared.\n");
        return nullptr;
    }

    // Set DSP program counter
    static Json::Value* cmd_dpc(std::vector<std::string>& args)
    {
        if (Flipper::HW->DSP->IsRunning())
        {
            DBReport2(DbgChannel::DSP, "It is impossible while running DSP thread.\n");
            return nullptr;
        }

        Flipper::HW->DSP->regs.pc = (DSP::DspAddress)strtoul(args[1].c_str(), nullptr, 0);
        return nullptr;
    }

    // Issue DSP reset
    static Json::Value* cmd_dreset(std::vector<std::string>& args)
    {
        Flipper::HW->DSP->HardReset();
        return nullptr;
    }

    // Disassemble some DSP instructions at program counter
    static Json::Value* cmd_du(std::vector<std::string>& args)
    {
        if (Flipper::HW->DSP->IsRunning())
        {
            DBReport2(DbgChannel::DSP, "It is impossible while running DSP thread.\n");
            return nullptr;
        }

        size_t instrCount = 8;
        DSP::DspAddress addr = 0;

        if (args.size() < 2)
        {
            addr = Flipper::HW->DSP->regs.pc;
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
            uint8_t* imemPtr = Flipper::HW->DSP->TranslateIMem(addr);
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
    static Json::Value* cmd_dst(std::vector<std::string>& args)
    {
        if (Flipper::HW->DSP->IsRunning())
        {
            DBReport2(DbgChannel::DSP, "It is impossible while running DSP thread.\n");
            return nullptr;
        }

        DBReport("DSP Call Stack:\n");

        for (auto it = Flipper::HW->DSP->regs.st[0].begin(); it != Flipper::HW->DSP->regs.st[0].end(); ++it)
        {
            DBReport("0x%04X\n", *it);
        }
        return nullptr;
    }

    // Dump DSP IFX
    static Json::Value* cmd_difx(std::vector<std::string>& args)
    {
        if (Flipper::HW->DSP->IsRunning())
        {
            DBReport2(DbgChannel::DSP, "It is impossible while running DSP thread.\n");
            return nullptr;
        }

        DBReport("DSP IFX Dump:\n");

        Flipper::HW->DSP->DumpIfx();
        return nullptr;
    }

    // Write message to CPU Mailbox
    static Json::Value* cmd_cpumbox(std::vector<std::string>& args)
    {
        uint32_t value = strtoul(args[1].c_str(), nullptr, 0);
        
        Flipper::HW->DSP->CpuToDspWriteHi(value >> 16);
        Flipper::HW->DSP->CpuToDspWriteLo((uint16_t)value);
        return nullptr;
    }

    // Read message from DSP Mailbox
    static Json::Value* cmd_dspmbox(std::vector<std::string>& args)
    {
        uint32_t value = 0;

        // Simulate reading by DSP
        value |= Flipper::HW->DSP->DspToCpuReadHi(true) << 16;
        if ((value & 0x80000000) == 0)
        {
            DBReport("No DSP message.\n");
            return nullptr;
        }
        value |= Flipper::HW->DSP->DspToCpuReadLo(true);
        DBReport("DSP Message: 0x%08X\n", value);
        return nullptr;
    }

    // Send CPU->DSP interrupt
    static Json::Value* cmd_cpudspint(std::vector<std::string>& args)
    {
        Flipper::HW->DSP->DSPSetIntBit(true);
        return nullptr;
    }

    // Send DSP->CPU interrupt
    static Json::Value* cmd_dspcpuint(std::vector<std::string>& args)
    {
        DSPAssertInt();
        return nullptr;
    }

    // Modify DSP register
    static Json::Value* cmd_dreg(std::vector<std::string>& args)
    {
        if (Flipper::HW->DSP->IsRunning())
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

        Flipper::HW->DSP->MoveToReg(regIndex, value);
        return nullptr;
    }

    void dsp_init_handlers()
    {
        Debug::Hub.AddCmd("dspdisa", cmd_dspdisa);
        Debug::Hub.AddCmd("dregs", cmd_dregs);
        Debug::Hub.AddCmd("dreg", cmd_dreg);
        Debug::Hub.AddCmd("dmem", cmd_dmem);
        Debug::Hub.AddCmd("imem", cmd_imem);
        Debug::Hub.AddCmd("drun", cmd_drun);
        Debug::Hub.AddCmd("dstop", cmd_dstop);
        Debug::Hub.AddCmd("dstep", cmd_dstep);
        Debug::Hub.AddCmd("dbrk", cmd_dbrk);
        Debug::Hub.AddCmd("dcan", cmd_dcan);
        Debug::Hub.AddCmd("dlist", cmd_dlist);
        Debug::Hub.AddCmd("dbrkclr", cmd_dbrkclr);
        Debug::Hub.AddCmd("dcanclr", cmd_dcanclr);
        Debug::Hub.AddCmd("dpc", cmd_dpc);
        Debug::Hub.AddCmd("dreset", cmd_dreset);
        Debug::Hub.AddCmd("du", cmd_du);
        Debug::Hub.AddCmd("dst", cmd_dst);
        Debug::Hub.AddCmd("difx", cmd_difx);
        Debug::Hub.AddCmd("cpumbox", cmd_cpumbox);
        Debug::Hub.AddCmd("dspmbox", cmd_dspmbox);
        Debug::Hub.AddCmd("cpudspint", cmd_cpudspint);
        Debug::Hub.AddCmd("dspcpuint", cmd_dspcpuint);
    }

}
