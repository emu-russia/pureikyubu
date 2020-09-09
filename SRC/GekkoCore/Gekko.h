// Gekko processor core public interface.

#pragma once

#ifdef _LINUX
#define __FASTCALL __attribute__((fastcall))
#else
#define __FASTCALL __fastcall
#endif

#include "GekkoDefs.h"
#include "GekkoAnalyzer.h"
#include "GatherBuffer.h"
#include "TLB.h"
#include "Cache.h"

// floating point register
union FPREG
{
    double         dbl;
    uint64_t       uval;
};

// time-base
union TBREG
{
    volatile int64_t   sval;               // for comparsion
    volatile uint64_t  uval;               // for incrementing
    struct
    {
        uint32_t     l;                  // for output
        uint32_t     u;
    } Part;
};

struct GekkoRegs
{
    uint32_t    gpr[32];            // general purpose regs
    FPREG       fpr[32], ps1[32];   // floating point regs (fpr=ps0 for paired singles)
    uint32_t    spr[1024];          // special purpose regs
    uint32_t    sr[16];             // segment regs
    uint32_t    cr;                 // condition reg
    uint32_t    msr;                // machine state reg
    uint32_t    fpscr;              // FP status/control reg (rounding only for now)
    uint32_t    pc;                 // program counter
    TBREG       tb;                 // time-base counter (timer)
};

namespace Gekko
{
    class Interpreter;
    class Jitc;
    class CodeSegment;

    enum class MmuAccess
    {
        Read = 0,
        Write,
        Execute,
    };

    // MMU never throws Gekko exceptions. If something went wrong, BadAddress is returned. Then the consumer decides what to do.
    constexpr uint32_t BadAddress = 0xffff'ffff;

    // So that the consumer can understand what went wrong.
    enum class MmuResult
    {
        Ok = 0,
        PageFault,      // No matching PTE found in page tables (and no matching BAT array entry)
        ProtectedFetch,    // Block/page protection violation 
        ProtectedRead,     // Block/page protection violation 
        ProtectedWrite,    // Block/page protection violation 
        NoExecute,      // No-execute protection violation / Instruction fetch from guarded memory
    };

    // The reason the PROGRAM exception occurred.
    enum class PrivilegedCause
    {
        None = 0,
        FpuEnabled,
        IllegalInstruction,
        Privileged,
        Trap,
    };

    class GekkoCore
    {
        friend Interpreter;
        friend Jitc;
        friend CodeSegment;

        // How many ticks Gekko takes to execute one instruction. 
        // Ideally, 1 instruction is executed in 1 tick. But it is unlikely that at the current level it is possible to achieve the performance of 486 MIPS.
        // Therefore, we are a little tricky and "slow down" the work of the emulated processor (we make several ticks per 1 instruction).
        static const int CounterStep = 2;

        Thread* gekkoThread = nullptr;
        static void GekkoThreadProc(void* Parameter);

        std::list<uint32_t> breakPointsExecute;
        std::list<uint32_t> breakPointsRead;
        std::list<uint32_t> breakPointsWrite;
        SpinLock breakPointsLock;
        uint32_t oneShotBreakpoint = BadAddress;

        bool TestBreakpointForJitc(uint32_t addr);
        void TestBreakpoints();
        void TestReadBreakpoints(uint32_t accessAddress);
        void TestWriteBreakpoints(uint32_t accessAddress);

        bool EnableTestBreakpoints = false;
        bool EnableTestReadBreakpoints = false;
        bool EnableTestWriteBreakpoints = false;

        Interpreter* interp;
        Jitc* jitc;

        int64_t     one_second;         // one second in timer ticks
        size_t      ops;                // instruction counter (only for debug!)
        size_t      segmentsExecuted;   // The number of completed recompiler segments.
        
        uint32_t EffectiveToPhysicalNoMmu(uint32_t ea, MmuAccess type, int& WIMG);
        uint32_t EffectiveToPhysicalMmu(uint32_t ea, MmuAccess type, int& WIMG);

        volatile bool decreq = false;       // decrementer exception request
        volatile bool intFlag = false;      // INT signal
        volatile bool exception = false;    // exception pending

        MmuResult MmuLastResult = MmuResult::Ok;

        // For convenient access to BAT registers (Mmu related)
        uint32_t *dbatu[4];
        uint32_t *dbatl[4];
        uint32_t *ibatu[4];
        uint32_t *ibatl[4];

        bool BlockAddressTranslation(uint32_t ea, uint32_t& pa, MmuAccess type, int& WIMG);
        uint32_t SegmentTranslation(uint32_t ea, MmuAccess type, int& WIMG);

        TLB dtlb;
        TLB itlb;

        PrivilegedCause PrCause;

        // Opcode stats
        int opcodeStats[(size_t)Instruction::Max] = { 0 };
        bool opcodeStatsEnabled = false;
        Thread* opcodeStatsThread = nullptr;
        static void OpcodeStatsThreadProc(void* Parameter);

        // Used to safely stop the main GekkoCore thread. In suspended state, the thread goes into a low-priority loop waiting for the start of emulation.
        // You can't just do Thread->Suspend(), because the thread can stop at an important part of the emulation and after restarting the emulation it will go into the #UB-state.
        volatile bool suspended = true;

    public:

        GatherBuffer gatherBuffer;

        // The instruction cache is not emulated because it is accessed only in one direction (Read).
        // Accordingly, it makes no sense to store a copy of RAM, you can just immediately read it from memory.

        Cache cache;

        // TODO: Will be hidden more
        GekkoRegs regs;

        GekkoCore();
        ~GekkoCore();

        void Run() { suspended = false; }
        bool IsRunning() { return !suspended; }
        void Suspend() { suspended = true; }

        void Reset();

        void Tick();
        int64_t GetTicks();
        int64_t OneSecond();

        void Step();

        void AssertInterrupt();
        void ClearInterrupt();
        void Exception(Gekko::Exception code);

        void ExecuteOpcodeDebug(uint32_t pc, uint32_t instr);

#pragma region "Memory interface"

        // Centralized hub for access to the data bus (memory) from CPU side.
        // These methods require fastcall, as they are called from recompiled code.

        void __FASTCALL ReadByte(uint32_t addr, uint32_t* reg);
        void __FASTCALL WriteByte(uint32_t addr, uint32_t data);
        void __FASTCALL ReadHalf(uint32_t addr, uint32_t* reg);
        void __FASTCALL ReadHalfS(uint32_t addr, uint32_t* reg);    // Signed wrapper. Used only by interpeter. TODO: Wipe it out, ambigious.
        void __FASTCALL WriteHalf(uint32_t addr, uint32_t data);
        void __FASTCALL ReadWord(uint32_t addr, uint32_t* reg);
        void __FASTCALL WriteWord(uint32_t addr, uint32_t data);
        void __FASTCALL ReadDouble(uint32_t addr, uint64_t* reg);
        void __FASTCALL WriteDouble(uint32_t addr, uint64_t* data);

        // Translate address by Mmu
        uint32_t EffectiveToPhysical(uint32_t ea, MmuAccess type, int & WIMG);

        static void SwapArea(uint32_t* addr, int count);
        static void SwapAreaHalf(uint16_t* addr, int count);

#pragma endregion "Memory interface"

#pragma region "Debug"

        void AddBreakpoint(uint32_t addr);
        void RemoveBreakpoint(uint32_t addr);
        void AddReadBreak(uint32_t addr);
        void AddWriteBreak(uint32_t addr);
        void ClearBreakpoints();

        void AddOneShotBreakpoint(uint32_t addr);

        void ToggleBreakpoint(uint32_t addr);
        bool IsBreakpoint(uint32_t addr);

        int64_t GetInstructionCounter() { return ops; }
        void ResetInstructionCounter() { ops = 0; }

        bool IsOpcodeStatsEnabled();
        void EnableOpcodeStats(bool enable);
        void PrintOpcodeStats(size_t maxCount);
        void ResetOpcodeStats();
        void RunOpcodeStatsThread();
        void StopOpcodeStatsThread();

#pragma endregion "Debug"

    };

    extern GekkoCore* Gekko;
}
