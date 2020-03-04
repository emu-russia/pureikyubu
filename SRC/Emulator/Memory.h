// some memory definitions

// used terms
/*/
    effective address : address before translation
    physical address  : address after translation
    real address      : address for data bus transfers
    translation       : effective -> physical
/*/

// main memory buffer
#define RAM         mem.ram

// amount of main memory (in bytes)
#define RAMSIZE     0x01800000      // 24 mb

// physical memory mask (for simple translation mode).
// 0x0fffffff, because GC architecture is limited by 256 mb of RAM
#define RAMMASK     0x0fffffff

// GC physical memory map. this is real addresses, not effective.
/*/
    00000000  24MB  Main Memory (RAM)
    08000000   2MB  Embedded Framebuffer (EFB)
    0C000000        Command Processor (CP)
    0C001000        Pixel Engine (PE)
    0C002000        Video Interface (VI)
    0C003000        Processor Interface (PI)
    0C004000        Memory Interface (MI)
    0C005000        DSP and DMA Audio Interface (AID)
    0C006000        DVD Interface (DI)
    0C006400        Serial Interface (SI)
    0C006800        External Interface (EXI)
    0C006C00        Audio Streaming Interface (AIS)
    0C008000        PI FIFO (GX)
    FFF00000   2MB  Boot ROM

    EFB - this is not straight "direct" access. reads and writes
    are passing through some Flipper logic, so its just simulation of
    direct access.

    Hardware Registers (HW) are located above 0x0C000000. Dolwin
    memory engine is using hardware traps, which are handling all 
    registers operations. traps are abstracting HW from Emulator,
    so basically any Hardware will work with Dolwin, with minimal
    modifications of Emulator core.

    Boot ROM is available only during CPU reset. after reset, 
    execution will begin from 0xFFF00100 reset vector, with 
    enabled bootrom EXI reading logic. small program, called
    "BS" (term by Nintendo) will run and load IPL menu up to
    0x81300000 address (already effective!). then IPL menu (or "BS2") 
    will run, with disabled EXI scrambler.
    Dolwin is simulating all BS and BS2 actions before running
    any DVD/executable. see HighLevel\Bootrom.cpp for details.
/*/

// ---------------------------------------------------------------------------

// memory read/write routines (thats all you need for CPU).
// Gekko has 64-bit data bus, so MEM*Double are necessary.
extern void (__fastcall *MEMReadByte)(uint32_t addr, uint32_t *reg);      // load byte
extern void (__fastcall *MEMWriteByte)(uint32_t addr, uint32_t data);     // store byte
extern void (__fastcall *MEMReadHalf)(uint32_t addr, uint32_t *reg);      // load halfword
extern void (__fastcall *MEMReadHalfS)(uint32_t addr, uint32_t *reg);     // load signed halfword
extern void (__fastcall *MEMWriteHalf)(uint32_t addr, uint32_t data);     // store halfword
extern void (__fastcall *MEMReadWord)(uint32_t addr, uint32_t *reg);      // load word
extern void (__fastcall *MEMWriteWord)(uint32_t addr, uint32_t data);     // store word
extern void (__fastcall *MEMReadDouble)(uint32_t addr, uint64_t *reg);    // load doubleword
extern void (__fastcall *MEMWriteDouble)(uint32_t addr, uint64_t *data);  // store doubleword
extern uint32_t(__fastcall *MEMFetch)(uint32_t addr);

// memory mapper API
extern uint32_t(__fastcall *MEMEffectiveToPhysical)(uint32_t ea, BOOL IR); // translate
void    MEMMap(BOOL IR, BOOL DR, uint32_t startEA, uint32_t startPA, uint32_t length);
void    MEMDoRemap(BOOL IR, BOOL DR);
void    MEMRemapMemory(BOOL IR, BOOL DR);

// other memory APIs
void    MEMInit();                                              // init
void    MEMFini();                                              // deinit
void    MEMOpen();                                              // open
void    MEMClose();                                             // close
void    MEMSelect(int mode, BOOL save=1);                       // select translation mode
extern  uint32_t __fastcall MEMSwap(uint32_t data);                       // swap long
extern  uint16_t __fastcall MEMSwapHalf(uint16_t data);                   // swap short
void    __fastcall MEMSwapArea(uint32_t *addr, int count);           // swap longs
void    __fastcall MEMSwapAreaHalf(uint16_t *addr, int count);       // swap shorts

// ---------------------------------------------------------------------------

// memory control/state block (all important data is here)
typedef struct MEMControl
{
    // latch variables
    BOOL        inited;             // inited ?
    BOOL        opened;             // opened ?

    // memory state
    int         mmu;                // translation mode (0: simple, 1: mmu)
    BOOL        mmudirect;          // direct mmu translation (mean no lookup tables)
    uint8_t*    ram;                // main memory (RAMSIZE)
    uint8_t**   imap;               // instruction translation lookup table (alive only when mmu)
    uint8_t**   dmap;               // data translation lookup table (alive only when mmu)
    BOOL        ir, dr;             // remap request

    uint8_t     lc[0x40000+4096];   // L2 locked cache
} MEMControl;

extern  MEMControl mem;
