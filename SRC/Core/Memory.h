// some memory definitions

// used terms
/*/
    effective address : address before translation
    physical address  : address after translation
    real address      : address for data bus transfers
    translation       : effective -> physical
/*/

// ---------------------------------------------------------------------------

extern uint32_t  __fastcall GCEffectiveToPhysical(uint32_t ea, bool IR = 0);
extern uint32_t  __fastcall MMUEffectiveToPhysical(uint32_t ea, bool IR = 0);

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

// ---------------------------------------------------------------------------

// memory control/state block (all important data is here)
typedef struct MEMControl
{
    // memory state
    int         mmu;                // translation mode (0: simple, 1: mmu)

    uint8_t     lc[0x40000+4096];   // L2 locked cache
} MEMControl;

extern  MEMControl mem;
