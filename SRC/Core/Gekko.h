// Gekko processor core public interface.

#pragma once

#include "../Common/Thread.h"
#include <list>
#include "GekkoDefs.h"
#include "GekkoAnalyzer.h"
#include "GatherBuffer.h"
#include "TLB.h"
#include "Cache.h"

// floating point register
typedef union _FPREG
{
    double         dbl;
    uint64_t       uval;
} FPREG;

// time-base
typedef union _TBREG
{
    volatile int64_t   sval;               // for comparsion
    volatile uint64_t  uval;               // for incrementing
    struct
    {
        uint32_t     l;                  // for output
        uint32_t     u;
    } Part;
} TBREG;

typedef struct _GekkoRegs
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
} GekkoRegs;

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

        uint64_t    msec;
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

    public:

        GatherBuffer gatherBuffer;

        // The instruction cache is not emulated because it is accessed only in one direction (Read).
        // Accordingly, it makes no sense to store a copy of RAM, you can just immediately read it from memory.

        Cache cache;

        // TODO: Will be hidden more
        GekkoRegs regs;

        GekkoCore();
        ~GekkoCore();

        void Run() { gekkoThread->Resume(); }
        bool IsRunning() { return gekkoThread->IsRunning(); }
        void Suspend() { gekkoThread->Suspend(); }

        void Reset();

        void Tick();
        int64_t GetTicks();
        int64_t OneSecond();
        int64_t OneMillisecond() { return msec; }

        void Step();

        void AssertInterrupt();
        void ClearInterrupt();
        void Exception(Gekko::Exception code);

        size_t GetOpcodeCount() { return ops; }
        void ResetOpcodeCount() { ops = 0; }

        void ExecuteOpcodeDebug(uint32_t pc, uint32_t instr);

#pragma region "Memory interface"

        // Centralized hub for access to the data bus (memory) from CPU side.
        void ReadByte(uint32_t addr, uint32_t* reg);
        void WriteByte(uint32_t addr, uint32_t data);
        void ReadHalf(uint32_t addr, uint32_t* reg);
        void ReadHalfS(uint32_t addr, uint32_t* reg);    // Signed wrapper. Used only by interpeter. TODO: Wipe it out, ambigious.
        void WriteHalf(uint32_t addr, uint32_t data);
        void ReadWord(uint32_t addr, uint32_t* reg);
        void WriteWord(uint32_t addr, uint32_t data);
        void ReadDouble(uint32_t addr, uint64_t* reg);
        void WriteDouble(uint32_t addr, uint64_t* data);

        // Translate address by Mmu
        uint32_t EffectiveToPhysical(uint32_t ea, MmuAccess type, int & WIMG);

        static void SwapArea(uint32_t* addr, int count);
        static void SwapAreaHalf(uint16_t* addr, int count);

#pragma endregion "Memory interface"

#pragma region "Breakpoints"

        void AddBreakpoint(uint32_t addr);
        void RemoveBreakpoint(uint32_t addr);
        void AddReadBreak(uint32_t addr);
        void AddWriteBreak(uint32_t addr);
        void ClearBreakpoints();

        void AddOneShotBreakpoint(uint32_t addr);

        void ToggleBreakpoint(uint32_t addr);
        bool IsBreakpoint(uint32_t addr);

#pragma endregion "Breakpoints"

    };

    extern GekkoCore* Gekko;
}
