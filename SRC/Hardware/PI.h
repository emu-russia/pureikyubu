// PI registers (all registers are 32-bit, 99% sure)

#define PI_INTSR            0x0C003000      // master interrupt reg
#define PI_INTMR            0x0C003004      // master interrupt mask
#define PI_MB_REV           0x0C00302C      // console revision
#define PI_RST_CODE         0x0C003024      // reset code

// PI interrupt regs mask
#define PI_INTERRUPT_HSP        0x2000      // high-speed port
#define PI_INTERRUPT_DEBUG      0x1000      // debug hardware
#define PI_INTERRUPT_CP         0x0800      // command fifo
#define PI_INTERRUPT_PE_FINISH  0x0400      // PE finish command (draw done)
#define PI_INTERRUPT_PE_TOKEN   0x0200      // PE token parsed (draw sync)
#define PI_INTERRUPT_VI         0x0100      // 4 VI line ints (only first is used)
#define PI_INTERRUPT_MEM        0x0080      // memory protection failed
#define PI_INTERRUPT_DSP        0x0040      // various DSP (ARAM, AI FIFO, DSP)
#define PI_INTERRUPT_AI         0x0020      // DVD streaming trigger interrupt
#define PI_INTERRUPT_EXI        0x0010      // EXI transfer complete
#define PI_INTERRUPT_SI         0x0008      // serial interrupts
#define PI_INTERRUPT_DI         0x0004      // DVD cover, break, transfer complete
#define PI_INTERRUPT_RSW        0x0002      // reset "switch"
#define PI_INTERRUPT_ERROR      0x0001      // GP verify failed

// ---------------------------------------------------------------------------
// hardware API

// PI state (registers and other data)
typedef struct PIControl
{
    uint32_t    intsr;          // interrupt cause
    uint32_t    intmr;          // interrupt mask
    bool        rswhack;        // reset "switch" hack
    bool        log;            // log interrupts
} PIControl;

extern  PIControl pi;

#define INTSR   pi.intsr
#define INTMR   pi.intmr

void    PICheckInterrupts();    // call to check for pending interrupt(s)
void    PIAssertInt(uint32_t mask);  // set interrupt(s)
void    PIClearInt(uint32_t mask);   // clear interrupt(s)
void    PIOpen();
