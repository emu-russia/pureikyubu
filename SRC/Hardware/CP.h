#pragma once

// GX fifo scratch buffer (attached to PI fifo, when 0x0C008000)
#define GX_FIFO         0x0C008000

// CP registers. CPU accessing CP regs by 16-bit reads and writes
#define CP_SR           0x0C000000      // status register
#define CP_CR           0x0C000002      // control register
#define CP_CLR          0x0C000004      // clear register
#define CP_TEX          0x0C000006      // something used for TEX units setup
#define CP_BASE         0x0C000020      // GP fifo base
#define CP_TOP          0x0C000024      // GP fifo top
#define CP_HIWMARK      0x0C000028      // FIFO high water count
#define CP_LOWMARK      0x0C00002C      // FIFO low water count
#define CP_CNT          0x0C000030      // FIFO_COUNT (entries currently in FIFO)
#define CP_WRPTR        0x0C000034      // GP FIFO write pointer
#define CP_RDPTR        0x0C000038      // GP FIFO read pointer
#define CP_BPPTR        0x0C00003C      // GP FIFO read address break point

// PE registers. CPU accessing PE regs by 16-bit reads and writes
#define PE_ZCR          0x0C001000      // z configuration
#define PE_ACR          0x0C001002      // alpha/blender configuration
#define PE_ALPHA_DST    0x0C001004      // destination alpha
#define PE_ALPHA_MODE   0x0C001006      // alpha mode
#define PE_ALPHA_READ   0x0C001008      // alpha read mode?
#define PE_SR           0x0C00100A      // status register
#define PE_TOKEN        0x0C00100E      // last token value

// PI fifo registers. CPU accessing them by 32-bit reads and writes
#define PI_BASE         0x0C00300C      // CPU fifo base
#define PI_TOP          0x0C003010      // CPU fifo top
#define PI_WRPTR        0x0C003014      // CPU fifo write pointer

// CP status register mask layout
#define CP_SR_OVF       (1 << 0)        // FIFO overflow (fifo_count > FIFO_HICNT)
#define CP_SR_UVF       (1 << 1)        // FIFO underflow (fifo_count < FIFO_LOCNT)
#define CP_SR_RD_IDLE   (1 << 2)        // FIFO read unit idle
#define CP_SR_CMD_IDLE  (1 << 3)        // CP idle
#define CP_SR_BPINT     (1 << 4)        // FIFO reach break point (cleared by disable FIFO break point)

// CP control register mask layout
#define CP_CR_RDEN      (1 << 0)        // Enable FIFO reads, reset value is 0 disable
#define CP_CR_BPCLR     (1 << 1)        // FIFO break point enable bit, reset value is 0 disable. Write 0 to clear BPINT
#define CP_CR_OVFEN     (1 << 2)        // FIFO overflow interrupt enable, reset value is 0 disable
#define CP_CR_UVFEN     (1 << 3)        // FIFO underflow interrupt enable, reset value is 0 disable
#define CP_CR_WPINC     (1 << 4)        // FIFO write pointer increment enable, reset value is 1 enable
#define CP_CR_BPEN      (1 << 5)        // FIFO break point interrupt enable, reset value is 0 disable

// CP clear register mask layout
#define CP_CLR_OVFCLR   (1 << 0)        // clear FIFO overflow interrupt
#define CP_CLR_UVFCLR   (1 << 1)        // clear FIFO underflow interrupt

// PE status register
#define PE_SR_DONE      (1 << 0)
#define PE_SR_TOKEN     (1 << 1)
#define PE_SR_DONEMSK   (1 << 2)
#define PE_SR_TOKENMSK  (1 << 3)

// PI wrap bit
#define PI_WRPTR_WRAP   (1 << 27)

// CP registers
typedef struct CPRegs
{
    uint16_t     sr;         // status
    uint16_t     cr;         // control
    uint32_t     base, top;
    uint32_t     lomark, himark;
    uint32_t     cnt;
    uint32_t     wrptr, rdptr, bpptr;
} CPRegs;

// PE registers
typedef struct PERegs
{
    uint16_t     sr;         // status register
    uint16_t     token;      // last token
} PERegs;

// PI registers
typedef struct PIRegs
{
    uint32_t     base;
    uint32_t     top;
    uint32_t     wrptr;      // also WRAP bit
} PIRegs;

// ---------------------------------------------------------------------------
// hardware API

// CP, PE and PI fifo state (registers and other data)
typedef struct FifoControl
{
    CPRegs      cp;     // command processor registers
    PERegs      pe;     // pixel engine registers
    PIRegs      pi;     // processor (CPU) fifo regs
    
    long        drawdone;   // events
    long        token;
    uint32_t    done_num;   // number of drawdone (PE_FINISH) events
    bool        log;
} FifoControl;

extern  FifoControl fifo;

void    CPOpen(HWConfig* config);
void    GXFifoWriteBurst(uint8_t data[32]);
