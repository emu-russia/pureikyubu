// Dolphin OS structures and definitions

// Note: all structures are big-endian (like on GC), use swap on access!

/* ---------------------------------------------------------------------------
    OS low memory vars
--------------------------------------------------------------------------- */

#define OS_PHYSICAL_CONTEXT     0x800000C0      // OSContext *
#define OS_CURRENT_CONTEXT      0x800000D4      // OSContext *
#define OS_DEFAULT_THREAD       0x800000D8      // OSThread *

/* ---------------------------------------------------------------------------
    Context API
--------------------------------------------------------------------------- */

// floating point context modes
#define     OS_CONTEXT_MODE_FPU         1   // normal mode
#define     OS_CONTEXT_MODE_PSFP        2   // gekko paired-single

// context status
#define     OS_CONTEXT_STATE_FPSAVED    1   // set when FPU is saved
#define     OS_CONTEXT_STATE_EXC        2   // set when saved by exception

// CPU context
typedef struct _OSContext
{
    // GPRs
    uint32_t     gpr[32];

    uint32_t     cr, lr, ctr, xer;

    // FPRs (or paired-single 0-part)
    double     fpr[32];

    uint32_t     fpscr_pad;
    uint32_t     fpscr;          // dummy in emulator

    // exception handling regs
    uint32_t     srr[2];

    // context flags
    uint16_t     mode;           // one of OS_CONTEXT_MODE*
    uint16_t     state;          // or'ed OS_CONTEXT_STATE*

    // gekko-specific regs
    uint32_t     gqr[8];         // quantization mode regs
    double       psr[32];        // paired-single 1-part

} OSContext;

#define OS_CONTEXT_SIZE     768

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

/* ---------------------------------------------------------------------------
    Utilities for emulator
--------------------------------------------------------------------------- */

void    OSCheckContextStruct();
