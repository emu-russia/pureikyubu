// Gekko processor core public interface.

#pragma once

#include "../Common/Thread.h"
#include <vector>
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

    enum class GekkoWaiter : int
    {
        HwUpdate = 0,
        DduData,
        DduAudio,
        FlipperAi,
        Max,
    };

    enum class MmuAccess
    {
        Read = 0,
        Write,
        Execute,
    };

    // MMU never throws Gekko exceptions. If something went wrong, BadAddress is returned. Then the consumer decides what to do.
    constexpr uint32_t BadAddress = 0xffffffff;

    // So that the consumer can understand what went wrong.
    enum class MmuResult
    {
        Ok = 0,
        PageFault,      // No matching PTE found in page tables (and no matching BAT array entry)
        Protected,      // Block/page protection violation 
        NoExecute,      // No-execute protection violation / Instruction fetch from guarded memory
    };

    typedef struct _WaitQueueEntry
    {
        uint64_t tbrValue;
        Thread* thread;
        bool requireSuspend;
        bool suspended;
    } WaitQueueEntry;

    class GekkoCore
    {
        friend Interpreter;
        friend Jitc;
        friend CodeSegment;

        // How many ticks Gekko takes to execute one instruction. 
        // Ideally, 1 instruction is executed in 1 tick. But it is unlikely that at the current level it is possible to achieve the performance of 486 MIPS.
        // Therefore, we are a little tricky and "slow down" the work of the emulated processor (we make several ticks per 1 instruction).
        static const int CounterStep = 12;

        Thread* gekkoThread = nullptr;
        static void GekkoThreadProc(void* Parameter);

        // TODO:
        std::vector<uint32_t> breakPointsExecute;
        std::vector<uint32_t> breakPointsRead;
        std::vector<uint32_t> breakPointsWrite;
        SpinLock breakPointsLock;

        Interpreter* interp;
        Jitc* jitc;

        bool waitQueueEnabled = true;   // Let's see how this mechanism will show itself. You can always turn it off.
        WaitQueueEntry waitQueue[(int)GekkoWaiter::Max] = { 0 };
        SpinLock waitQueueLock;
        void DispatchWaitQueue();
        int dispatchQueuePeriod = 20;    // It makes no sense to check waitQueue every tick. +/- some ticks back and forth do not play a role.
        int dispatchQueueCounter = 0;       

        uint64_t    msec;
        int64_t     one_second;         // one second in timer ticks
        size_t      ops;                // instruction counter (only for debug!)
        size_t      segmentsExecuted;   // The number of completed recompiler segments.

        // TODO: Research cache emulation #14
        uint8_t     lc[0x40000 + 4096];   // L2 locked cache
        
        uint32_t __fastcall EffectiveToPhysicalNoMmu(uint32_t ea, MmuAccess type);
        uint32_t __fastcall EffectiveToPhysicalMmu(uint32_t ea, MmuAccess type);

        volatile bool decreq = false;       // decrementer exception request
        volatile bool intFlag = false;      // INT signal

        MmuResult MmuLastResult = MmuResult::Ok;
        int LastWIMG = 0;       // The value of the WIMG bits after the last address translation.

        // For convenient access to BAT registers (Mmu related)
        uint32_t *dbatu[4];
        uint32_t *dbatl[4];
        uint32_t *ibatu[4];
        uint32_t *ibatl[4];

        bool __fastcall BlockAddressTranslation(uint32_t ea, uint32_t& pa, MmuAccess type);
        uint32_t __fastcall SegmentTranslation(uint32_t ea, MmuAccess type);

        TLB tlb;
        Cache cache;

    public:

        GatherBuffer gatherBuffer;

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

        // The thread that polls the TBR may ask the Gekko core to wake him up at the right time.
        void WakeMeUp(GekkoWaiter disignation, uint64_t gekkoTicks, Thread* thread);

        size_t GetOpcodeCount() { return ops; }
        void ResetOpcodeCount() { ops = 0; }

        static bool ExecuteInterpeterFallback();
        void ExecuteOpcodeDebug(uint32_t pc, uint32_t instr);

#pragma region "Memory interface"

        // Centralized hub for access to the data bus (memory) from CPU side.
        void __fastcall ReadByte(uint32_t addr, uint32_t* reg);
        void __fastcall WriteByte(uint32_t addr, uint32_t data);
        void __fastcall ReadHalf(uint32_t addr, uint32_t* reg);
        void __fastcall ReadHalfS(uint32_t addr, uint32_t* reg);    // Signed wrapper. Used only by interpeter. TODO: Wipe it out, ambigious.
        void __fastcall WriteHalf(uint32_t addr, uint32_t data);
        void __fastcall ReadWord(uint32_t addr, uint32_t* reg);
        void __fastcall WriteWord(uint32_t addr, uint32_t data);
        void __fastcall ReadDouble(uint32_t addr, uint64_t* reg);
        void __fastcall WriteDouble(uint32_t addr, uint64_t* data);

        // Translate address by Mmu
        uint32_t EffectiveToPhysical(uint32_t ea, MmuAccess type);

        static void SwapArea(uint32_t* addr, int count);
        static void SwapAreaHalf(uint16_t* addr, int count);

#pragma endregion "Memory interface"

    };

    extern GekkoCore* Gekko;
}
