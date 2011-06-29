#define SAVE_STATE_MAGIC    0xc0011dea  // version 1.0

// emulator state block (use integer types only!)
typedef struct SaveState
{
    // all data, except magik, in Intel little-endian format

    // file info
    struct
    {
        u32     magik;                  // must be swapped SAVE_STATE_MAGIC
        u8      dummy[12];              // zeroes
        u32     useDVD;                 // DVD save-state file, if 1
        char    fileName[MAX_PATH];     // current DVD/exec file name
    } File;

    // memory state
    struct
    {
        u8      ram[RAMSIZE];           // physical memory buffer
    } Memory;

    // CPU state
    struct
    {
        u32     gpr[32];                // general purpose regs
        u32     spr[1024];              // special purpose regs
        u32     sr[16];                 // segment regs
        u32     cr;                     // condition reg
        u64     fpr[32], ps1[32];       // floating point regs
        u64     time;                   // time-base pair
        u32     msr;                    // machine state reg
        u32     fpscr;                  // floating point status/control
    } Processor;

    // hardware state
    struct
    {
        u8      dummy;
    } GCHardware;
    
    // high level state
    struct
    {
        u8      dummy;
    } GCHighLevel;

} SaveState;

void    SaveLoad(BOOL save, char *saveFile);
