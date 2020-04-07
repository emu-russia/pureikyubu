// Gekko processor core public interface.

#pragma once

#include "../Common/Thread.h"
#include <vector>
#include <atomic>
#include "GekkoDefs.h"

// TODO: Get rid of this non-incapsulated mess

// registers
#define GPR     cpu.gpr
#define SPR     cpu.spr
#define PPC_SR  cpu.sr
#define FPRU(n) (cpu.fpr[n].uval)
#define FPRD(n) (cpu.fpr[n].dbl)
#define SP      (GPR[1])
#define SDA1    (GPR[13])
#define SDA2    (GPR[2])
#define XER     (SPR[1])
#define PPC_LR  (SPR[8])
#define CTR     (SPR[9])
#define DSISR   (SPR[18])
#define PPC_DAR (SPR[19])
#define PPC_DEC (SPR[22])
#define SDR1    (SPR[25])
#define SRR0    (SPR[26])
#define SRR1    (SPR[27])
#define SPRG0   (SPR[272])
#define SPRG1   (SPR[273])
#define SPRG2   (SPR[274])
#define SPRG3   (SPR[275])
#define EAR     (SPR[282])
#define PVR     (SPR[287])
#define IBAT0U  (SPR[528])
#define IBAT0L  (SPR[529])
#define IBAT1U  (SPR[530])
#define IBAT1L  (SPR[531])
#define IBAT2U  (SPR[532])
#define IBAT2L  (SPR[533])
#define IBAT3U  (SPR[534])
#define IBAT3L  (SPR[535])
#define DBAT0U  (SPR[536])
#define DBAT0L  (SPR[537])
#define DBAT1U  (SPR[538])
#define DBAT1L  (SPR[539])
#define DBAT2U  (SPR[540])
#define DBAT2L  (SPR[541])
#define DBAT3U  (SPR[542])
#define DBAT3L  (SPR[543])
#define HID0    (SPR[1008])
#define HID1    (SPR[1009])
#define IABR    (SPR[1010])
#define DABR    (SPR[1013])
#define PPC_CR  cpu.cr
#define MSR     cpu.msr
#define FPSCR   cpu.fpscr
#define TBR     cpu.tb.sval
#define UTBR    cpu.tb.uval
#define PC      cpu.pc

// BAT fields
#define BATBEPI(batu)   (batu >> 17)
#define BATBL(batu)     ((batu >> 2) & 0x7ff)
#define BATBRPN(batl)   (batl >> 17)

// floating point register
typedef union FPREG
{
    double         dbl;
    uint64_t       uval;
} FPREG;

// time-base
typedef union TBREG
{
    int64_t         sval;               // for comparsion
    uint64_t        uval;               // for incrementing
    struct
    {
        uint32_t     l;                  // for output
        uint32_t     u;
    } Part;
} TBREG;

// ** Gekko specific **

#define PS0(n)      (cpu.fpr[n].dbl)
#define PS1(n)      (cpu.ps1[n].dbl)

// GQR0 reserved by OS (always 0)
// GQR1 reserved by CW compiler
// others are used by OS "fast-cast"
#define GQR         (&SPR[912])
#define GQR0        (GQR[0])
#define GQR1        (GQR[1])
#define GQR2        (GQR[2])            // u8
#define GQR3        (GQR[3])            // u16
#define GQR4        (GQR[4])            // s8
#define GQR5        (GQR[5])            // s16
#define GQR6        (GQR[6])
#define GQR7        (GQR[7])

#define HID2        (SPR[920])
#define WPAR        (SPR[921])          // write-gathering buffer
#define DMAU        (SPR[922])          // locked cache DMA
#define DMAL        (SPR[923])

#define WPE         (HID2 & HID2_WPE)

#define LSQE        (HID2 & HID2_LSQE)
#define PSE         (HID2 & HID2_PSE)
#define LD_SCALE(n) ((GQR[n] >> 24) & 0x3f)
#define LD_TYPE(n)  ((GQR[n] >> 16) & 7)
#define ST_SCALE(n) ((GQR[n] >>  8) & 0x3f)
#define ST_TYPE(n)  ((GQR[n]      ) & 7)

// ---------------------------------------------------------------------------
// CPU externals

// CPU memory operations. using MEM* or DB* read/write operations,
// (depending on whenever Debugger is running or not)
extern void (__fastcall *CPUReadByte)(uint32_t addr, uint32_t*reg);
extern void (__fastcall *CPUWriteByte)(uint32_t addr, uint32_t data);
extern void (__fastcall *CPUReadHalf)(uint32_t addr, uint32_t*reg);
extern void (__fastcall *CPUReadHalfS)(uint32_t addr, uint32_t*reg);
extern void (__fastcall *CPUWriteHalf)(uint32_t addr, uint32_t data);
extern void (__fastcall *CPUReadWord)(uint32_t addr, uint32_t*reg);
extern void (__fastcall *CPUWriteWord)(uint32_t addr, uint32_t data);
extern void (__fastcall *CPUReadDouble)(uint32_t addr, uint64_t*reg);
extern void (__fastcall *CPUWriteDouble)(uint32_t addr, uint64_t*data);

// CPU control/state block (all important data is here)
typedef struct CPUControl
{
    // CPU state (all registers)
    uint32_t    gpr[32];            // general purpose regs
    FPREG       fpr[32], ps1[32];   // floating point regs (fpr=ps0 for paired singles)
    uint32_t    spr[1024];          // special purpose regs
    uint32_t    sr[16];             // segment regs
    uint32_t    cr;                 // condition reg
    uint32_t    msr;                // machine state reg
    uint32_t    fpscr;              // FP status/control reg (rounding only for now)
    uint32_t    pc;                 // program counter
    TBREG       tb;         // time-base counter (timer)

    int64_t     one_second;         // one second in timer ticks
    bool        decreq;             // decrementer exception request
    uint32_t    ops;                // instruction counter (only for debug!)

    // for default interpreter
    bool        exception;          // exception pending
    bool        branch;             // non-linear PC change
    uint32_t    rotmask[32][32];    // mask for integer rotate opcodes 
    std::atomic<bool> RESERVE;            // for lwarx/stwcx.   
    uint32_t    RESERVE_ADDR;       // for lwarx/stwcx.
    float       ldScale[64];        // for paired-single loads
    float       stScale[64];        // for paired-single stores
} CPUControl;

extern  CPUControl cpu;

namespace Gekko
{
    class Interpreter;

    class GekkoCore
    {
        // How many ticks Gekko takes to execute one instruction. 
        // Ideally, 1 instruction is executed in 1 tick. But it is unlikely that at the current level it is possible to achieve the performance of 486 MIPS.
        // Therefore, we are a little tricky and “slow down” the work of the emulated processor (we make several ticks per 1 instruction).
        static const int CounterStep = 1;

        Thread* gekkoThread = nullptr;
        static void GekkoThreadProc(void* Parameter);

        std::vector<uint32_t> breakPointsExecute;
        std::vector<uint32_t> breakPointsRead;
        std::vector<uint32_t> breakPointsWrite;
        MySpinLock::LOCK breakPointsLock = MySpinLock::LOCK_IS_FREE;

        Interpreter* interp;

    public:
        GekkoCore();
        ~GekkoCore();

        void Run() { gekkoThread->Resume(); }
        bool IsRunning() { return gekkoThread->IsRunning(); }
        void Suspend() { gekkoThread->Suspend(); }

        void Reset();

        void Tick();
        int64_t GetTicks();
        int64_t OneSecond();

        // translate
        uint32_t EffectiveToPhysical(uint32_t ea, bool IR); 

        static void SwapArea(uint32_t* addr, int count);
        static void SwapAreaHalf(uint16_t* addr, int count);

        void Step();

        bool intFlag = false;      // INT signal

        void AssertInterrupt();
        void ClearInterrupt();
        void Exception(uint32_t code);
    };

    extern GekkoCore* Gekko;
}
