// Gekko processor core public interface.

#pragma once

#include "../Common/Thread.h"
#include <vector>
#include <list>
#include "GekkoDefs.h"
#include "GekkoAnalyzer.h"
#include "GatherBuffer.h"

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

// ---------------------------------------------------------------------------
// CPU externals

extern void (__fastcall *CPUReadByte)(uint32_t addr, uint32_t*reg);
extern void (__fastcall *CPUWriteByte)(uint32_t addr, uint32_t data);
extern void (__fastcall *CPUReadHalf)(uint32_t addr, uint32_t*reg);
extern void (__fastcall *CPUReadHalfS)(uint32_t addr, uint32_t*reg);
extern void (__fastcall *CPUWriteHalf)(uint32_t addr, uint32_t data);
extern void (__fastcall *CPUReadWord)(uint32_t addr, uint32_t*reg);
extern void (__fastcall *CPUWriteWord)(uint32_t addr, uint32_t data);
extern void (__fastcall *CPUReadDouble)(uint32_t addr, uint64_t*reg);
extern void (__fastcall *CPUWriteDouble)(uint32_t addr, uint64_t*data);

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

    enum class GekkoWaiter : int
    {
        HwUpdate = 0,
        DduData,
        DduAudio,
        FlipperAi,
        Max,
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

        // How many ticks Gekko takes to execute one instruction. 
        // Ideally, 1 instruction is executed in 1 tick. But it is unlikely that at the current level it is possible to achieve the performance of 486 MIPS.
        // Therefore, we are a little tricky and "slow down" the work of the emulated processor (we make several ticks per 1 instruction).
        static const int CounterStep = 1;

        Thread* gekkoThread = nullptr;
        static void GekkoThreadProc(void* Parameter);

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

        // Translate address by Mmu
        uint32_t EffectiveToPhysical(uint32_t ea, bool IR); 

        static void SwapArea(uint32_t* addr, int count);
        static void SwapAreaHalf(uint16_t* addr, int count);

        void Step();

        volatile bool decreq = false;       // decrementer exception request
        volatile bool intFlag = false;      // INT signal

        void AssertInterrupt();
        void ClearInterrupt();
        void Exception(Gekko::Exception code);

        // The thread that polls the TBR may ask the Gekko core to wake him up at the right time.
        void WakeMeUp(GekkoWaiter disignation, uint64_t gekkoTicks, Thread* thread);

        size_t GetOpcodeCount() { return ops; }
        void ResetOpcodeCount() { ops = 0; }

        static bool ExecuteInterpeterFallback();

    };

    extern GekkoCore* Gekko;
}
