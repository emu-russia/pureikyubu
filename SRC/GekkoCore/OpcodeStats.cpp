// This module deals with the maintenance of statistics on the use of opcodes.

#include "pch.h"

using namespace Debug;

namespace Gekko
{
    struct OpcodeSortedEntry
    {
        Instruction instr;
        int count;
    };

    bool GekkoCore::IsOpcodeStatsEnabled()
    {
        return opcodeStatsEnabled;
    }

    void GekkoCore::EnableOpcodeStats(bool enable)
    {
        opcodeStatsEnabled = enable;
    }

    static int OpcodeStatsCompare(const void* a, const void* b)
    {
        OpcodeSortedEntry* as = (OpcodeSortedEntry*)a;
        OpcodeSortedEntry* bs = (OpcodeSortedEntry*)b;
        return bs->count - as->count;
    }

    void GekkoCore::PrintOpcodeStats(size_t maxCount)
    {
        OpcodeSortedEntry unsorted[(size_t)Instruction::Max];

        // Sort statistics

        for (size_t i = 0; i < (size_t)Instruction::Max; i++)
        {
            unsorted[i].instr = (Instruction)i;
            unsorted[i].count = opcodeStats[i];
        }

        qsort(unsorted, (size_t)Instruction::Max, sizeof(OpcodeSortedEntry), OpcodeStatsCompare);

        // Print out

        if (maxCount > (size_t)Instruction::Max)
        {
            maxCount = (size_t)Instruction::Max;
        }

        for (size_t i = 0; i < maxCount; i++)
        {
            AnalyzeInfo info;

            info.instr = unsorted[i].instr;
            Report(Channel::Norm, "%s: %i\n", GekkoDisasm::InstrToString(&info).c_str(), unsorted[i].count);
        }

        Report(Channel::Norm, "  \n");
    }

    void GekkoCore::ResetOpcodeStats()
    {
        memset(opcodeStats, 0, sizeof(opcodeStats));
    }

    // Thread that displays statistics on the use of opcodes once per second.

    void GekkoCore::OpcodeStatsThreadProc(void* Parameter)
    {
        GekkoCore* core = (GekkoCore*)Parameter;

        if (core->opcodeStatsEnabled && core->IsRunning())
        {
            core->PrintOpcodeStats(10);
            core->ResetOpcodeStats();
        }

        Thread::Sleep(1000);
    }

    void GekkoCore::RunOpcodeStatsThread()
    {
        if (opcodeStatsThread == nullptr)
        {
            opcodeStatsThread = new Thread(OpcodeStatsThreadProc, false, this, "OpcodeStats");
            EnableOpcodeStats(true);
        }
    }

    void GekkoCore::StopOpcodeStatsThread()
    {
        if (opcodeStatsThread != nullptr)
        {
            delete opcodeStatsThread;
            opcodeStatsThread = nullptr;
            EnableOpcodeStats(false);
        }
    }

}
