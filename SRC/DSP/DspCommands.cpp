// Dsp debug commands

#include "pch.h"

using namespace Debug;

namespace DSP
{

    // disasm dsp ucode to file
    static Json::Value* cmd_dspdisa(std::vector<std::string>& args)
    {
        auto start_addr = 0LL;      // in DSP slots (halfwords)
        if (args.size() >= 3)
        {
            start_addr = std::strtoul(args[2].c_str(), nullptr, 0);
        }
        
        auto ucode = Util::FileLoad(args[1]);
        if (ucode.empty())
        {
            Report(Channel::Norm, "Failed to load %s\n", args[1].c_str());
            return nullptr;
        }

        auto file = std::ofstream("Data\\dspdisa.txt");
        if (!file.is_open())
        {
            Report(Channel::Norm, "Failed to create dsp_disa.txt\n");
            return nullptr;
        }

        uint8_t* ucodePtr = ucode.data();
        size_t bytesLeft = ucode.size();
        size_t offset = 0;      // in DSP slots (halfwords)

        file << fmt::format("// Disassembled {:s}\n\n", args[1].c_str());

        while (bytesLeft != 0)
        {
            DSP::AnalyzeInfo info = { 0 };

            /* Analyze. */
            bool result = DSP::Analyzer::Analyze(ucodePtr, ucode.size() - 2 * offset, info);
            if (!result)
            {
                Report(Channel::Norm, "DSP::Analyze failed at offset: 0x%08X\n", offset);
                break;
            }

            /* Disassemble. */
            std::string text = DSP::DspDisasm::Disasm((uint16_t)(offset + start_addr), info);

            file << fmt::format("{:s}\n", text.c_str());

            offset += (info.sizeInBytes / sizeof(uint16_t));
            bytesLeft -= info.sizeInBytes;
            ucodePtr += info.sizeInBytes;
        }

        file.close();

        Report(Channel::Norm, "Done.\n");
        return nullptr;
    }

    // Show dsp registers
    static Json::Value* cmd_dregs(std::vector<std::string>& args)
    {
        // A special trick to display all registers (we intentionally make them dirty by inverting bits)
        DSP::DspRegs regsChanged = Flipper::HW->DSP->regs;

        regsChanged.pc = ~regsChanged.pc;
        regsChanged.prod.bitsPacked = ~regsChanged.prod.bitsPacked;
        regsChanged.bank = ~regsChanged.bank;
        regsChanged.sr.bits = ~regsChanged.sr.bits;

        for (int i = 0; i < 4; i++)
        {
            regsChanged.ar[i] = ~regsChanged.ar[i];
            regsChanged.ix[i] = ~regsChanged.ix[i];
            regsChanged.lm[i] = ~regsChanged.lm[i];
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

        Report(Channel::Norm, "DMEM Dump %i bytes\n", bytes);

        while (bytes != 0)
        {
            uint8_t* ptr = Flipper::HW->DSP->TranslateDMem(dsp_addr);
            if (ptr == nullptr)
            {
                Report(Channel::DSP, "TranslateDMem failed on dsp addr: 0x%04X\n", dsp_addr);
                break;
            }

            Report(Channel::Norm, "%04X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
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

        Report(Channel::Norm, "IMEM Dump %i bytes\n", bytes);

        while (bytes != 0)
        {
            uint8_t* ptr = Flipper::HW->DSP->TranslateIMem(dsp_addr);
            if (ptr == nullptr)
            {
                Report(Channel::DSP, "TranslateIMem failed on dsp addr: 0x%04X\n", dsp_addr);
                break;
            }

            Report(Channel::Norm, "%04X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
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
            Report(Channel::DSP, "It is impossible while running DSP thread.\n");
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
                Report(Channel::DSP, "TranslateIMem failed on dsp addr: 0x%04X\n", pcAddr);
                return nullptr;
            }

            DSP::AnalyzeInfo info = { 0 };

            if (!DSP::Analyzer::Analyze(imemPtr, DSP::DspCore::MaxInstructionSizeInBytes, info))
            {
                Report(Channel::DSP, "DSP Analyzer failed on dsp addr: 0x%04X\n", pcAddr);
                return nullptr;
            }

            std::string code = DSP::DspDisasm::Disasm(pcAddr, info);

            Report(Channel::Norm, "%s\n", code.c_str());

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

        Report(Channel::Norm, "DSP breakpoint added: 0x%04X\n", dsp_addr);
        return nullptr;
    }

    // Add IMEM canary
    static Json::Value* cmd_dcan(std::vector<std::string>& args)
    {
        DSP::DspAddress dsp_addr = (DSP::DspAddress)strtoul(args[1].c_str(), nullptr, 0);

        Flipper::HW->DSP->AddCanary(dsp_addr, args[2]);

        Report(Channel::Norm, "DSP canary added: 0x%04X\n", dsp_addr);
        return nullptr;
    }

    // List IMEM breakpoints and canaries
    static Json::Value* cmd_dlist(std::vector<std::string>& args)
    {
        Report(Channel::Norm, "DSP breakpoints:\n");

        Flipper::HW->DSP->ListBreakpoints();

        Report(Channel::Norm, "DSP canaries:\n");

        Flipper::HW->DSP->ListCanaries();
        return nullptr;
    }

    // Clear all IMEM breakpoints
    static Json::Value* cmd_dbrkclr(std::vector<std::string>& args)
    {
        Flipper::HW->DSP->ClearBreakpoints();

        Report(Channel::Norm, "DSP breakpoints cleared.\n");
        return nullptr;
    }

    // Clear all IMEM canaries
    static Json::Value* cmd_dcanclr(std::vector<std::string>& args)
    {
        Flipper::HW->DSP->ClearCanaries();

        Report(Channel::Norm, "DSP canaries cleared.\n");
        return nullptr;
    }

    // Set DSP program counter
    static Json::Value* cmd_dpc(std::vector<std::string>& args)
    {
        if (Flipper::HW->DSP->IsRunning())
        {
            Report(Channel::DSP, "It is impossible while running DSP thread.\n");
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
            Report(Channel::DSP, "It is impossible while running DSP thread.\n");
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
                Report(Channel::DSP, "TranslateIMem failed on dsp addr: 0x%04X\n", addr);
                break;
            }

            DSP::AnalyzeInfo info = { 0 };

            if (!DSP::Analyzer::Analyze(imemPtr, DSP::DspCore::MaxInstructionSizeInBytes, info))
            {
                Report(Channel::DSP, "DSP Analyzer failed on dsp addr: 0x%04X\n", addr);
                return nullptr;
            }

            std::string code = DSP::DspDisasm::Disasm(addr, info);

            Report(Channel::Norm, "%s\n", code.c_str());

            addr += (DSP::DspAddress)(info.sizeInBytes >> 1);
        }
        return nullptr;
    }

    // Dump DSP call stack
    static Json::Value* cmd_dst(std::vector<std::string>& args)
    {
        if (Flipper::HW->DSP->IsRunning())
        {
            Report(Channel::DSP, "It is impossible while running DSP thread.\n");
            return nullptr;
        }

        Report(Channel::Norm, "DSP Call Stack:\n");

        for (auto it = Flipper::HW->DSP->regs.st[0].begin(); it != Flipper::HW->DSP->regs.st[0].end(); ++it)
        {
            Report(Channel::Norm, "0x%04X\n", *it);
        }
        return nullptr;
    }

    // Dump DSP IFX
    static Json::Value* cmd_difx(std::vector<std::string>& args)
    {
        if (Flipper::HW->DSP->IsRunning())
        {
            Report(Channel::DSP, "It is impossible while running DSP thread.\n");
            return nullptr;
        }

        Report(Channel::Norm, "DSP IFX Dump:\n");

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
            Report(Channel::Norm, "No DSP message.\n");
            return nullptr;
        }
        value |= Flipper::HW->DSP->DspToCpuReadLo(true);
        Report(Channel::Norm, "DSP Message: 0x%08X\n", value);
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
            Report(Channel::DSP, "It is impossible while running DSP thread.\n");
            return nullptr;
        }

        static const char* dspRegNames[] = {
		    "ar0", "ar1", "ar2", "ar3",
            "ix0", "ix1", "ix2", "ix3",
            "lm0", "lm1", "lm2", "lm3",
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
            Report(Channel::Norm, "Invalid register name: %s\n", args[1].c_str());
            return nullptr;
        }

        Flipper::HW->DSP->MoveToReg(regIndex, value);
        return nullptr;
    }

    // Multiplier tests

    static Json::Value* dsp_muls(std::vector<std::string>& args)
    {
        uint32_t a = strtoul(args[1].c_str(), nullptr, 0) & 0xffff;
        uint32_t b = strtoul(args[2].c_str(), nullptr, 0) & 0xffff;

        Report(Channel::Norm, "MUL signed 0x%04X * 0x%04X\n", (uint16_t)a, (uint16_t)b);

        DspProduct prod = DspCore::Muls((int16_t)a, (int16_t)b, false);

        Report(Channel::Norm, "prod: h:%04X, m1:%04X, l:%04X, m2:%04X\n",
            prod.h, prod.m1, prod.l, prod.m2);

        DspCore::PackProd(prod);

        DspLongAccumulator acc;
        acc.bits = prod.bitsPacked;

        Report(Channel::Norm, "prod packed: %02X_%04X_%04X\n", acc.h, acc.m, acc.l);
        
        Report(Channel::Norm, "Signed Multiply by host: 0x%llX\n", ((int64_t)(int32_t)(int16_t)a * (int64_t)(int32_t)(int16_t)b) & 0xff'ffff'ffff);

        return nullptr;
    }

    static Json::Value* dsp_mulu(std::vector<std::string>& args)
    {
        uint32_t a = strtoul(args[1].c_str(), nullptr, 0) & 0xffff;
        uint32_t b = strtoul(args[2].c_str(), nullptr, 0) & 0xffff;

        Report(Channel::Norm, "MUL Unsigned 0x%04X * 0x%04X\n", (uint16_t)a, (uint16_t)b);

        DspProduct prod = DspCore::Mulu(a, b, false);

        Report(Channel::Norm, "prod: h:%04X, m1:%04X, l:%04X, m2:%04X\n",
            prod.h, prod.m1, prod.l, prod.m2);

        DspCore::PackProd(prod);

        DspLongAccumulator acc;
        acc.bits = prod.bitsPacked;

        Report(Channel::Norm, "prod packed: %02X_%04X_%04X\n", acc.h, acc.m, acc.l);

        Report(Channel::Norm, "Unsigned Multiply by host: 0x%08X\n", (uint32_t)(uint16_t)a * (uint32_t)(uint16_t)b);

        return nullptr;
    }

    static Json::Value* CmdDspIsRunning(std::vector<std::string>& args)
    {
        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Bool;

        output->value.AsBool = Flipper::HW->DSP->IsRunning();

        return output;
    }

    static Json::Value* CmdDspRun(std::vector<std::string>& args)
    {
        Flipper::HW->DSP->Run();
        return nullptr;
    }

    static Json::Value* CmdDspSuspend(std::vector<std::string>& args)
    {
        Flipper::HW->DSP->Suspend();
        return nullptr;
    }

    static Json::Value* CmdDspStep(std::vector<std::string>& args)
    {
        if (!Flipper::HW->DSP->IsRunning())
        {
            Flipper::HW->DSP->Step();
        }
        
        return nullptr;
    }

    static Json::Value* CmdDspGetReg(std::vector<std::string>& args)
    {
        int reg = atoi(args[1].c_str());

        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Int;

        output->value.AsUint16 = Flipper::HW->DSP->MoveFromReg(reg);

        return output;
    }

    static Json::Value* CmdDspGetPsr(std::vector<std::string>& args)
    {
        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Int;

        output->value.AsUint16 = Flipper::HW->DSP->regs.sr.bits;

        return output;
    }

    static Json::Value* CmdDspGetPc(std::vector<std::string>& args)
    {
        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Int;

        output->value.AsUint16 = Flipper::HW->DSP->regs.pc;

        return output;
    }

    static Json::Value* CmdDspPackProd(std::vector<std::string>& args)
    {
        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Int;

        DspProduct prod;
        Flipper::HW->DSP->PackProd(prod);

        output->value.AsInt = prod.bitsPacked;

        return output;
    }

    static Json::Value* CmdDspTranslateDMem(std::vector<std::string>& args)
    {
        DspAddress addr = strtoul(args[1].c_str(), nullptr, 0);

        uint8_t* ptr = Flipper::HW->DSP->TranslateDMem(addr);

        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Int;

        output->value.AsInt = (uint64_t)ptr;

        return output;
    }

    static Json::Value* CmdDspTranslateIMem(std::vector<std::string>& args)
    {
        DspAddress addr = strtoul(args[1].c_str(), nullptr, 0);

        uint8_t* ptr = Flipper::HW->DSP->TranslateIMem(addr);

        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Int;

        output->value.AsInt = (uint64_t)ptr;

        return output;
    }

    static Json::Value* CmdDspTestBreakpoint(std::vector<std::string>& args)
    {
        DspAddress addr = strtoul(args[1].c_str(), nullptr, 0);

        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Bool;

        output->value.AsBool = Flipper::HW->DSP->TestBreakpoint(addr);

        return output;
    }

    static Json::Value* CmdDspToggleBreakpoint(std::vector<std::string>& args)
    {
        DspAddress addr = strtoul(args[1].c_str(), nullptr, 0);

        Flipper::HW->DSP->ToggleBreakpoint(addr);

        return nullptr;
    }

    static Json::Value* CmdDspAddOneShotBreakpoint(std::vector<std::string>& args)
    {
        DspAddress addr = strtoul(args[1].c_str(), nullptr, 0);

        Flipper::HW->DSP->AddOneShotBreakpoint(addr);

        return nullptr;
    }

    static bool IsCall(uint32_t address, uint32_t& targetAddress)
    {
        DSP::AnalyzeInfo info = { 0 };

        targetAddress = 0;

        uint8_t* ptr = (uint8_t*)Flipper::HW->DSP->TranslateIMem(address);
        if (!ptr)
        {
            return false;
        }

        if (!DSP::Analyzer::Analyze(ptr, DSP::DspCore::MaxInstructionSizeInBytes, info))
            return false;

        if (info.flowControl)
        {
            if (info.instr == DSP::DspInstruction::CALLcc || info.instr == DSP::DspInstruction::CALLR)
            {
                targetAddress = info.ImmOperand.Address;
                return true;
            }
        }

        return false;
    }

    static Json::Value* CmdDspIsCall(std::vector<std::string>& args)
    {
        DSP::DspAddress address = strtoul(args[1].c_str(), nullptr, 0);

        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Array;

        uint32_t targetAddress = 0;

        bool res = IsCall(address, targetAddress);

        output->AddBool(nullptr, res);
        output->AddUInt32(nullptr, targetAddress);

        return output;
    }

    static bool IsCallOrJump(uint32_t address, uint32_t& targetAddress)
    {
        DSP::AnalyzeInfo info = { 0 };

        targetAddress = 0;

        uint8_t* ptr = Flipper::HW->DSP->TranslateIMem(address);
        if (!ptr)
        {
            return false;
        }

        if (!DSP::Analyzer::Analyze(ptr, DSP::DspCore::MaxInstructionSizeInBytes, info))
            return false;

        if (info.flowControl)
        {
            if (info.instr == DSP::DspInstruction::Jcc ||
                info.instr == DSP::DspInstruction::CALLcc)
            {
                targetAddress = info.ImmOperand.Address;
                return true;
            }
        }

        return false;
    }

    static Json::Value* CmdDspIsCallOrJump(std::vector<std::string>& args)
    {
        DSP::DspAddress address = strtoul(args[1].c_str(), nullptr, 0);

        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Array;

        uint32_t targetAddress = 0;

        bool res = IsCallOrJump(address, targetAddress);

        output->AddBool(nullptr, res);
        output->AddUInt32(nullptr, targetAddress);

        return output;
    }

    static Json::Value* CmdDspDisasm(std::vector<std::string>& args)
    {
        DSP::DspAddress address = strtoul(args[1].c_str(), nullptr, 0);

        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Array;

        // Disassemble

        DSP::AnalyzeInfo info = { 0 };

        std::string text = "";

        uint8_t* ptr = (uint8_t*)Flipper::HW->DSP->TranslateIMem(address);
        if (ptr)
        {
            if (DSP::Analyzer::Analyze(ptr, DSP::DspCore::MaxInstructionSizeInBytes, info))
            {
                text = DSP::DspDisasm::Disasm(address, info);
            }
        }

        // Return as text

        output->AddBool(nullptr, info.flowControl);
        output->AddInt(nullptr, (int)info.sizeInBytes / sizeof(uint16_t));
        output->AddAnsiString(nullptr, text.c_str());

        return output;
    }

    void dsp_init_handlers()
    {
        JDI::Hub.AddCmd("dspdisa", cmd_dspdisa);
        JDI::Hub.AddCmd("dregs", cmd_dregs);
        JDI::Hub.AddCmd("dreg", cmd_dreg);
        JDI::Hub.AddCmd("dmem", cmd_dmem);
        JDI::Hub.AddCmd("imem", cmd_imem);
        JDI::Hub.AddCmd("drun", cmd_drun);
        JDI::Hub.AddCmd("dstop", cmd_dstop);
        JDI::Hub.AddCmd("dstep", cmd_dstep);
        JDI::Hub.AddCmd("dbrk", cmd_dbrk);
        JDI::Hub.AddCmd("dcan", cmd_dcan);
        JDI::Hub.AddCmd("dlist", cmd_dlist);
        JDI::Hub.AddCmd("dbrkclr", cmd_dbrkclr);
        JDI::Hub.AddCmd("dcanclr", cmd_dcanclr);
        JDI::Hub.AddCmd("dpc", cmd_dpc);
        JDI::Hub.AddCmd("dreset", cmd_dreset);
        JDI::Hub.AddCmd("du", cmd_du);
        JDI::Hub.AddCmd("dst", cmd_dst);
        JDI::Hub.AddCmd("difx", cmd_difx);
        JDI::Hub.AddCmd("cpumbox", cmd_cpumbox);
        JDI::Hub.AddCmd("dspmbox", cmd_dspmbox);
        JDI::Hub.AddCmd("cpudspint", cmd_cpudspint);
        JDI::Hub.AddCmd("dspcpuint", cmd_dspcpuint);
        JDI::Hub.AddCmd("dsp_muls", dsp_muls);
        JDI::Hub.AddCmd("dsp_mulu", dsp_mulu);
        
        JDI::Hub.AddCmd("DspIsRunning", CmdDspIsRunning);
        JDI::Hub.AddCmd("DspRun", CmdDspRun);
        JDI::Hub.AddCmd("DspSuspend", CmdDspSuspend);
        JDI::Hub.AddCmd("DspStep", CmdDspStep);

        JDI::Hub.AddCmd("DspGetReg", CmdDspGetReg);
        JDI::Hub.AddCmd("DspGetPsr", CmdDspGetPsr);
        JDI::Hub.AddCmd("DspGetPc", CmdDspGetPc);
        JDI::Hub.AddCmd("DspPackProd", CmdDspPackProd);

        JDI::Hub.AddCmd("DspTranslateDMem", CmdDspTranslateDMem);
        JDI::Hub.AddCmd("DspTranslateIMem", CmdDspTranslateIMem);

        JDI::Hub.AddCmd("DspTestBreakpoint", CmdDspTestBreakpoint);
        JDI::Hub.AddCmd("DspToggleBreakpoint", CmdDspToggleBreakpoint);
        JDI::Hub.AddCmd("DspAddOneShotBreakpoint", CmdDspAddOneShotBreakpoint);

        JDI::Hub.AddCmd("DspIsCall", CmdDspIsCall);
        JDI::Hub.AddCmd("DspIsCallOrJump", CmdDspIsCallOrJump);
        JDI::Hub.AddCmd("DspDisasm", CmdDspDisasm);
    }

}
