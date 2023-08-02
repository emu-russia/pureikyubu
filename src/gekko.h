// Gekko processor core public interface.

#pragma once

// Compile time macros for GekkoCore.

// For debugging purposes, Jitc is not yet turned on when the code is uploaded to master.

#ifndef GEKKOCORE_USE_JITC
#define GEKKOCORE_USE_JITC 0	//!< Use the recompiler during main code execution (in runtime). Debugging (step-by-step execution) is always done using the interpreter.
#endif

#ifndef GEKKOCORE_JITC_HALT_ON_UNIMPLEMENTED_OPCODE
#define GEKKOCORE_JITC_HALT_ON_UNIMPLEMENTED_OPCODE 0	//!< Halt the emulation on an unimplemented opcode, instead of passing control to the interpeter fallback
#endif

#ifndef GEKKOCORE_SIMPLE_MMU
#define GEKKOCORE_SIMPLE_MMU 0	//!< Use the simple MMU translation used in Dolphin OS (until games start using ARAM mapping, also not suitable for GC-Linux).
#endif

#ifndef GEKKOCORE_GATHER_BUFFER_RETIRE_TICKS
#define GEKKOCORE_GATHER_BUFFER_RETIRE_TICKS 10000		//!< The GatherBuffer has an undocumented feature - after a certain number of cycles the data in it is destroyed and it becomes free (WPAR[BNE] = 0)
#endif


// Gekko architecture definitions (from datasheet).

// GC bus clock is running on 1/3 of CPU clock, and timer is
// running on 1/4 of bus clock (1/12 of CPU clock)
#define CPU_CORE_CLOCK  486000000u  // 486 mhz (its not 485, stop bugging me!)
#define CPU_BUS_CLOCK   (CPU_CORE_CLOCK / 3)
#define CPU_TIMER_CLOCK (CPU_BUS_CLOCK / 4)

// Machine State Flags
#define MSR_BIT(n)			(1 << (31-n))
#define MSR_RESERVED        0xFFFA0088
#define MSR_POW             (MSR_BIT(13))               // Power management enable
#define MSR_ILE             (MSR_BIT(15))               // Exception little-endian mode
#define MSR_EE              (MSR_BIT(16))               // External interrupt enable
#define MSR_PR              (MSR_BIT(17))               // User privilege level
#define MSR_FP              (MSR_BIT(18))               // Floating-point available
#define MSR_ME              (MSR_BIT(19))               // Machine check enable
#define MSR_FE0             (MSR_BIT(20))               // Floating-point exception mode 0
#define MSR_SE              (MSR_BIT(21))               // Single-step trace enable
#define MSR_BE              (MSR_BIT(22))               // Branch trace enable
#define MSR_FE1             (MSR_BIT(23))               // Floating-point exception mode 1
#define MSR_IP              (MSR_BIT(25))               // Exception prefix
#define MSR_IR              (MSR_BIT(26))               // Instruction address translation
#define MSR_DR              (MSR_BIT(27))               // Data address translation
#define MSR_PM              (MSR_BIT(29))               // Performance monitor mode
#define MSR_RI              (MSR_BIT(30))               // Recoverable exception
#define MSR_LE              (MSR_BIT(31))               // Little-endian mode enable

#define HID0_EMCP	0x8000'0000
#define HID0_DBP	0x4000'0000
#define HID0_EBA	0x2000'0000
#define HID0_EBD	0x1000'0000
#define HID0_BCLK	0x0800'0000
#define HID0_ECLK	0x0200'0000
#define HID0_PAR	0x0100'0000
#define HID0_DOZE	0x0080'0000
#define HID0_NAP	0x0040'0000
#define HID0_SLEEP	0x0020'0000
#define HID0_DPM	0x0010'0000
#define HID0_NHR	0x0001'0000			// Not hard reset (software-use only)
#define HID0_ICE	0x0000'8000
#define HID0_DCE	0x0000'4000
#define HID0_ILOCK	0x0000'2000
#define HID0_DLOCK	0x0000'1000
#define HID0_ICFI	0x0000'0800
#define HID0_DCFI	0x0000'0400
#define HID0_SPD	0x0000'0200
#define HID0_IFEM	0x0000'0100
#define HID0_SGE	0x0000'0080
#define HID0_DCFA	0x0000'0040
#define HID0_BTIC	0x0000'0020
#define HID0_ABE	0x0000'0008
#define HID0_BHT	0x0000'0004
#define HID0_NOOPTI	0x0000'0001

#define HID2_LSQE   0x8000'0000          // PS load/store quantization
#define HID2_WPE    0x4000'0000          // gathering enabled
#define HID2_PSE    0x2000'0000          // PS-mode
#define HID2_LCE    0x1000'0000          // locked cache enable

#define WPAR_ADDR   0xffffffe0          // accumulation address
#define WPAR_BNE    0x1                 // buffer not empty

#define GEKKO_CR0_LT    (1 << 31)		// Result < 0
#define GEKKO_CR0_GT    (1 << 30)		// Result > 0
#define GEKKO_CR0_EQ    (1 << 29)		// Result == 0
#define GEKKO_CR0_SO    (1 << 28)		// Copied from XER[SO]

#define GEKKO_XER_SO    (1 << 31)		// Sticky overflow
#define GEKKO_XER_OV    (1 << 30)		// Overflow 
#define GEKKO_XER_CA    (1 << 29)		// Carry

// WIMG Bits
#define WIMG_W		8				// Write-through 
#define WIMG_I		4				// Caching-inhibited 
#define WIMG_M		2				// Bus lock on access
#define WIMG_G		1				// Guarded (block out-of-order access)

// FPSCR Bits
#define FPSCR_FX		(1 << 31)		// Floating-point exception summary
#define FPSCR_FEX		(1 << 30)		// Floating-point enabled exception summary
#define FPSCR_VX		(1 << 29)		// Floating-point invalid operation exception summary
#define FPSCR_OX		(1 << 28)		// Floating-point overflow exception
#define FPSCR_UX		(1 << 27)		// Floating-point underflow exception
#define FPSCR_ZX		(1 << 26)		// Floating-point zero divide exception
#define FPSCR_XX		(1 << 25)		// Floating-point inexact exception
#define FPSCR_VXSNAN	(1 << 24)		// Floating-point invalid operation exception for SNaN
#define FPSCR_VXISI		(1 << 23)		// Floating-point invalid operation exception for ∞ – ∞
#define FPSCR_VXIDI		(1 << 22)		// Floating-point invalid operation exception for ∞ ÷ ∞
#define FPSCR_VXZDZ		(1 << 21)		// Floating-point invalid operation exception for 0 ÷ 0
#define FPSCR_VXIMZ		(1 << 20)		// Floating-point invalid operation exception for ∞ * 0
#define FPSCR_VXVC		(1 << 19)		// Floating-point invalid operation exception for invalid compare
#define FPSCR_FR		(1 << 18)		// Floating-point fraction rounded
#define FPSCR_FI		(1 << 17)		// Floating-point fraction inexact
#define FPSCR_GET_FPRF(fpscr) ((fpscr >> 12) & 0x1f)	// Get floating-point result flags
#define FPSCR_SET_FPRF(fpscr, n) (fpscr = (fpscr & 0xfffe0fff) | ((n & 0x1f) << 12))	// Set floating-point result flags
#define FPSCR_RESERVED	(1 << 11)		// Reserved.
#define FPSCR_VXSOFT	(1 << 10)		// Floating-point invalid operation exception for software request
#define FPSCR_VXSQRT	(1 << 9)		// Floating-point invalid operation exception for invalid square root
#define FPSCR_VXCVI	(1 << 8)		// Floating-point invalid operation exception for invalid integer convert
#define FPSCR_VE	(1 << 7)		// Floating-point invalid operation exception enable
#define FPSCR_OE	(1 << 6)		// IEEE floating-point overflow exception enable
#define FPSCR_UE	(1 << 5)		// IEEE floating-point underflow exception enable
#define FPSCR_ZE	(1 << 4)		// IEEE floating-point zero divide exception enable
#define FPSCR_XE	(1 << 3)		// Floating-point inexact exception enable
#define FPSCR_NI	(1 << 2)		// Floating-point non-IEEE mode
#define FPSCR_GET_RN(fpscr) (fpscr & 3)	// Get floating-point rounding control
#define FPSCR_SET_RN(fpscr, n) (fpscr = (fpscr & ~3) | (n & 3))	// Set floating-point rounding control

// Exception vectors (physical address)

namespace Gekko
{
    enum class Exception : uint32_t
    {
        EXCEPTION_RESERVED = 0x00000,
        EXCEPTION_SYSTEM_RESET = 0x00100,
        EXCEPTION_MACHINE_CHECK = 0x00200,
        EXCEPTION_DSI = 0x00300,
        EXCEPTION_ISI = 0x00400,
        EXCEPTION_EXTERNAL_INTERRUPT = 0x00500,
        EXCEPTION_ALIGNMENT = 0x00600,
        EXCEPTION_PROGRAM = 0x00700,
        EXCEPTION_FP_UNAVAILABLE = 0x00800,
        EXCEPTION_DECREMENTER = 0x00900,
        EXCEPTION_SYSTEM_CALL = 0x00C00,
        EXCEPTION_TRACE = 0x00D00,
        EXCEPTION_PERFORMANCE_MONITOR = 0x00F00,
        EXCEPTION_IABR = 0x01300,
        EXCEPTION_THERMAL = 0x01700,
    };
}

// SPRs

namespace Gekko
{
    enum SPR
    {
        XER = 1,
        LR = 8,
        CTR = 9,
        DSISR = 18,
        DAR = 19,
        DEC = 22,
        SDR1 = 25,
        SRR0 = 26,
        SRR1 = 27,
        SPRG0 = 272,
        SPRG1 = 273,
        SPRG2 = 274,
        SPRG3 = 275,
        EAR = 282,
        TBL = 284,
        TBU = 285,
        PVR = 287,
        IBAT0U = 528,
        IBAT0L = 529,
        IBAT1U = 530,
        IBAT1L = 531,
        IBAT2U = 532,
        IBAT2L = 533,
        IBAT3U = 534,
        IBAT3L = 535,
        DBAT0U = 536,
        DBAT0L = 537,
        DBAT1U = 538,
        DBAT1L = 539,
        DBAT2U = 540,
        DBAT2L = 541,
        DBAT3U = 542,
        DBAT3L = 543,
        HID0 = 1008,
        HID1 = 1009,
        IABR = 1010,
        DABR = 1013,
        GQRs = 912,
        GQR0 = 912,
        GQR1 = 913,
        GQR2 = 914,
        GQR3 = 915,
        GQR4 = 916,
        GQR5 = 917,
        GQR6 = 918,
        GQR7 = 919,
        HID2 = 920,
        WPAR = 921,
        DMAU = 922,
        DMAL = 923,
    };
}

namespace Gekko
{
    enum class TBR
    {
        TBL = 268,
        TBU = 269,
    };
}

// Quantization data types

enum class GEKKO_QUANT_TYPE
{
    SINGLE_FLOAT = 0,		// single-precision floating-point (no conversion)
    RESERVED1,
    RESERVED2,
    RESERVED3,
    U8 = 4,			// unsigned 8 bit integer
    U16 = 5,		// unsigned 16 bit integer
    S8 = 6,			// signed 8 bit integer
    S16 = 7,		// signed 16 bit integer
};

#define GEKKO_DMAU_MEM_ADDR 0xffff'ffe0
#define GEKKO_DMAU_DMA_LEN_U  0x1f
#define GEKKO_DMAL_LC_ADDR 0xffff'ffe0
#define GEKKO_DMAL_DMA_LD 0x10		// 0 Store - transfer from locked cache to external memory. 1 Load - transfer from external memory to locked cache.
#define GEKKO_DMA_LEN_SHIFT 2
#define GEKKO_DMAL_DMA_LEN_L 3
#define GEKKO_DMAL_DMA_T 2
#define GEKKO_DMAL_DMA_F 1

// BAT fields
#define BATBEPI(batu)   (batu >> 17)
#define BATBL(batu)     ((batu >> 2) & 0x7ff)
#define BATBRPN(batl)   (batl >> 17)

// Left until all lurking bugs are eliminated.
#define DOLPHIN_OS_LOCKED_CACHE_ADDRESS 0xE000'0000

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


namespace GekkoCoreUnitTest
{
    class GekkoCoreUnitTest;
}

#ifdef _LINUX
#define __FASTCALL __attribute__((fastcall))
#else
#define __FASTCALL __fastcall
#endif


namespace Gekko
{
    class GekkoCore;
}


namespace Gekko
{
    void SixtyBus_ReadByte(uint32_t phys_addr, uint32_t* reg);
    void SixtyBus_WriteByte(uint32_t phys_addr, uint32_t data);
    void SixtyBus_ReadHalf(uint32_t phys_addr, uint32_t* reg);
    void SixtyBus_WriteHalf(uint32_t phys_addr, uint32_t data);
    void SixtyBus_ReadWord(uint32_t phys_addr, uint32_t* reg);
    void SixtyBus_WriteWord(uint32_t phys_addr, uint32_t data);
    void SixtyBus_ReadDouble(uint32_t phys_addr, uint64_t* reg);
    void SixtyBus_WriteDouble(uint32_t phys_addr, uint64_t* data);
    void SixtyBus_ReadBurst(uint32_t phys_addr, uint8_t burstData[32]);
    void SixtyBus_WriteBurst(uint32_t phys_addr, uint8_t burstData[32]);
}



// This module is used to simulate Gekko TLB.

namespace Gekko
{
    struct TLBEntry
    {
        uint32_t addressTag;
        int8_t wimg;
    };

    class TLB
    {
        std::unordered_map<int, TLBEntry*> tlb;

    public:
        bool Exists(uint32_t ea, uint32_t& pa, int& WIMG);
        void Map(uint32_t ea, uint32_t pa, int WIMG);

        void Invalidate(uint32_t ea);
        void InvalidateAll();
    };
}



// Gekko caches support (including Data locked cache)


namespace Gekko
{
    enum class CacheLogLevel
    {
        None = 0,
        Commands,
        MemOps,
    };

    class Cache
    {
        uint8_t* cacheData;
        const size_t cacheSize = 0x01800000;    // 24 MBytes

        // A sign that the cache block is dirty (does not match the value in RAM).
        bool* modifiedBlocks = nullptr;

        // Cache block invalid, must be casted-in before use
        bool* invalidBlocks = nullptr;

        bool IsDirty(uint32_t pa);
        void SetDirty(uint32_t pa, bool dirty);

        bool IsInvalid(uint32_t pa);
        void SetInvalid(uint32_t pa, bool invalid);

        bool enabled = false;
        bool frozen = false;

        CacheLogLevel log = CacheLogLevel::None;

        void CastIn(uint32_t pa);		// Mem -> Cache
        void CastOut(uint32_t pa);		// Cache -> Mem

        // You can disable cache emulation for debugging purposes.
        // This does not apply to a locked cache.
        bool DisableForDebugReasons = true;

        uint8_t* LockedCache = nullptr;
        uint32_t LockedCacheAddr = 0;
        bool lcenabled = false;

        GekkoCore* core;

    public:
        Cache(GekkoCore* core);
        ~Cache();

        void Reset();

        void Enable(bool enable);
        bool IsEnabled() { return enabled; }

        void Freeze(bool freeze);
        bool IsFrozen() { return frozen; }

        void LockedEnable(bool enable);
        bool IsLockedEnable() { return lcenabled; }

        // Physical addressing

        void Flush(uint32_t pa);
        void Invalidate(uint32_t pa);
        void Store(uint32_t pa);
        void Touch(uint32_t pa);
        void TouchForStore(uint32_t pa);
        void Zero(uint32_t pa);
        void ZeroLocked(uint32_t pa);

        void ReadByte(uint32_t addr, uint32_t* reg);
        void WriteByte(uint32_t addr, uint32_t data);
        void ReadHalf(uint32_t addr, uint32_t* reg);
        void WriteHalf(uint32_t addr, uint32_t data);
        void ReadWord(uint32_t addr, uint32_t* reg);
        void WriteWord(uint32_t addr, uint32_t data);
        void ReadDouble(uint32_t addr, uint64_t* reg);
        void WriteDouble(uint32_t addr, uint64_t* data);

        void LockedCacheDma(bool MemToCache, uint32_t memaddr, uint32_t lcaddr, size_t bursts);

        void SetLogLevel(CacheLogLevel level) { log = level; }
        void DebugDisable(bool disable);
    };
}


// Gekko Gather Buffer

namespace Gekko
{
    class GekkoCore;

    class GatherBuffer
    {
        uint8_t fifo[32 * 4] = { 0 };
        size_t readPtr = 0;
        size_t writePtr = 0;

        void WriteBytes(uint8_t* data, size_t size);
        size_t GatherSize();

        bool log = false;

        GekkoCore* core;

        int64_t retireTimeout = 0;

    public:

        GatherBuffer(GekkoCore* parent) : core(parent) {}

        void Reset();

        void Write8(uint8_t value);
        void Write16(uint16_t value);
        void Write32(uint32_t value);
        void Write64(uint64_t value);

        bool NotEmpty();

    };
}





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

namespace Debug
{
    class JitCommands;
}

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
        friend GatherBuffer;
        friend GekkoCoreUnitTest::GekkoCoreUnitTest;

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

        uint32_t EffectiveToPhysicalNoMmu(uint32_t ea, MmuAccess type, int& WIMG);
        uint32_t EffectiveToPhysicalMmu(uint32_t ea, MmuAccess type, int& WIMG);

        volatile bool decreq = false;       // decrementer exception request
        volatile bool intFlag = false;      // INT signal
        volatile bool exception = false;    // exception pending

        MmuResult MmuLastResult = MmuResult::Ok;

        // For convenient access to BAT registers (Mmu related)
        uint32_t* dbatu[4];
        uint32_t* dbatl[4];
        uint32_t* ibatu[4];
        uint32_t* ibatl[4];

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

        // Request to clear instruction counter
        volatile bool resetInstructionCounter = false;

        GatherBuffer* gatherBuffer;

        // Stats

        size_t compiledSegments = 0;
        size_t executedSegments = 0;

        bool RESERVE = false;    // for lwarx/stwcx.
        uint32_t RESERVE_ADDR = 0;	// for lwarx/stwcx.

    public:

        // The instruction cache is not emulated because it is accessed only in one direction (Read).
        // Accordingly, it makes no sense to store a copy of RAM, you can just immediately read it from memory.

        Cache* cache;

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

#pragma region "Memory interface"

        // Centralized hub for access to the data bus (memory) from CPU side.

        void ReadByte(uint32_t addr, uint32_t* reg);
        void WriteByte(uint32_t addr, uint32_t data);
        void ReadHalf(uint32_t addr, uint32_t* reg);
        void WriteHalf(uint32_t addr, uint32_t data);
        void ReadWord(uint32_t addr, uint32_t* reg);
        void WriteWord(uint32_t addr, uint32_t data);
        void ReadDouble(uint32_t addr, uint64_t* reg);
        void WriteDouble(uint32_t addr, uint64_t* data);

        // Translate address by Mmu
        uint32_t EffectiveToPhysical(uint32_t ea, MmuAccess type, int& WIMG);

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
        void ResetInstructionCounter() { resetInstructionCounter = true; }

        bool IsOpcodeStatsEnabled();
        void EnableOpcodeStats(bool enable);
        void PrintOpcodeStats(size_t maxCount);
        void ResetOpcodeStats();
        void RunOpcodeStatsThread();
        void StopOpcodeStatsThread();

        size_t GetCompiledSegmentsCount() { return compiledSegments; }
        size_t GetExecutedSegmentsCount() { return executedSegments; }
        void ResetCompiledSegmentsCount() { compiledSegments = 0; }
        void ResetExecutedSegmentsCount() { executedSegments = 0; }

        void Halt(const char* text, ...);
        void Report(const char* text, ...);

#pragma endregion "Debug"

    };
}
