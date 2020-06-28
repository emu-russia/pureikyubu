// Dolphin OS structures and definitions

// Note: all structures are big-endian (like on GC), use swap on access!

/* ---------------------------------------------------------------------------
    OS low memory vars
--------------------------------------------------------------------------- */

#define OS_PHYSICAL_CONTEXT     0x800000C0      // OSContext *
#define OS_CURRENT_CONTEXT      0x800000D4      // OSContext *
#define OS_DEFAULT_THREAD       0x800000D8      // OSThread *
#define OS_LINK_ACTIVE          0x800000DC      // OSThreadLink
#define OS_CURRENT_THREAD       0x800000E4      // OSThread *

/* ---------------------------------------------------------------------------
    Context API
--------------------------------------------------------------------------- */

// floating point context modes
#define     OS_CONTEXT_MODE_FPU         1   // normal mode
#define     OS_CONTEXT_MODE_PSFP        2   // gekko paired-single

// context status
#define     OS_CONTEXT_STATE_FPSAVED    1   // set when FPU is saved
#define     OS_CONTEXT_STATE_EXC        2   // set when saved by exception

#define OS_CONTEXT_FRAME_SIZE     768

#pragma pack(push, 1)

// CPU context
struct OSContext
{
    // GPRs
    uint32_t     gpr[32];

    uint32_t     cr, lr, ctr, xer;

    // FPRs (or paired-single 0-part)
    union
    {
        double      fpr[32];
        uint64_t    fprAsUint[32];
    };

    uint32_t     fpscr_pad;
    uint32_t     fpscr;          // dummy in emulator

    // exception handling regs
    uint32_t     srr[2];

    // context flags
    uint16_t     mode;           // one of OS_CONTEXT_MODE*
    uint16_t     state;          // or'ed OS_CONTEXT_STATE*

    // gekko-specific regs
    uint32_t     gqr[8];         // quantization mode regs

    uint32_t    padding;

    union
    {
        double      psr[32];        // paired-single 1-part
        uint64_t    psrAsUint[32];
    };

};

struct OSThreadLink
{
    uint32_t    next;
    uint32_t    prev;
};

struct OSThreadQueue
{
    uint32_t    head;
    uint32_t    tail;
};

typedef OSThreadQueue OSMutexQueue;

struct OSThread
{
    OSContext   context;

    uint16_t    state;
    uint16_t    attr;
    int32_t     suspend;
    uint32_t    priority;
    uint32_t    base;
    uint32_t    val;

    uint32_t    queue;
    OSThreadLink link;
    OSThreadQueue queueJoin;
    uint32_t    mutex;
    OSMutexQueue queueMutex;
    OSThreadLink linkActive;

    uint32_t    stackBase;
    uint32_t    stackEnd;
};

#pragma pack(pop)

// os calls
void    OSSetCurrentContext ( void );
void    OSGetCurrentContext ( void );
void    OSSaveContext       ( void );
void    OSLoadContext       ( void );
void    OSClearContext      ( void );
void    OSInitContext       ( void );
void    OSLoadFPUContext    ( void );
void    OSSaveFPUContext    ( void );
void    OSFillFPUContext    ( void );

void    __OSContextInit     ( void );

/* ---------------------------------------------------------------------------
    Interrupt handling
--------------------------------------------------------------------------- */

void    OSDisableInterrupts ( void );
void    OSEnableInterrupts  ( void );
void    OSRestoreInterrupts ( void );
