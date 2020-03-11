// some memory definitions

// used terms
/*/
    effective address : address before translation
    physical address  : address after translation
    real address      : address for data bus transfers
    translation       : effective -> physical
/*/

// ---------------------------------------------------------------------------

// memory read/write routines (thats all you need for CPU)
void __fastcall MEMReadByte(uint32_t addr, uint32_t *reg);      // load byte
void __fastcall MEMWriteByte(uint32_t addr, uint32_t data);     // store byte
void __fastcall MEMReadHalf(uint32_t addr, uint32_t *reg);      // load halfword
void __fastcall MEMReadHalfS(uint32_t addr, uint32_t *reg);     // load signed halfword
void __fastcall MEMWriteHalf(uint32_t addr, uint32_t data);     // store halfword
void __fastcall MEMReadWord(uint32_t addr, uint32_t *reg);      // load word
void __fastcall MEMWriteWord(uint32_t addr, uint32_t data);     // store word
void __fastcall MEMReadDouble(uint32_t addr, uint64_t *reg);    // load doubleword
void __fastcall MEMWriteDouble(uint32_t addr, uint64_t *data);  // store doubleword
void __fastcall MEMFetch(uint32_t addr, uint32_t* opcode);

// memory mapper API
extern uint32_t(__fastcall *MEMEffectiveToPhysical)(uint32_t ea, bool IR); // translate

// other memory APIs
void    MEMOpen(int mode);                                      // open
void    MEMClose();                                             // close
void    MEMSelect(int mode, bool save=true);                    // select translation mode

extern "C" uint32_t __fastcall MEMSwap(uint32_t data);          // swap long
extern "C" uint16_t __fastcall MEMSwapHalf(uint16_t data);      // swap short
void    MEMSwapArea(uint32_t *addr, int count);           // swap longs
void    MEMSwapAreaHalf(uint16_t *addr, int count);       // swap shorts

// ---------------------------------------------------------------------------

// memory control/state block (all important data is here)
typedef struct MEMControl
{
    // latch variables
    bool        opened;             // opened ?

    // memory state
    int         mmu;                // translation mode (0: simple, 1: mmu)
    bool        ir, dr;             // remap request

    uint8_t     lc[0x40000+4096];   // L2 locked cache
} MEMControl;

extern  MEMControl mem;
