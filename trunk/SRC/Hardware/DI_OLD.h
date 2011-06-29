// size of DVD raw image data
#define DVD_SIZE         0x57058000     // 1.4 GB

// DI registers
#define DI_SR            (0x0C006000)
#define DI_CVR           (0x0C006004)
#define DI_CMDBUF0       (0x0C006008)
#define DI_CMDBUF1       (0x0C00600C)
#define DI_CMDBUF2       (0x0C006010)
#define DI_MAR           (0x0C006014)
#define DI_LEN           (0x0C006018)
#define DI_CR            (0x0C00601C)
#define DI_IMMBUF        (0x0C006020)
#define DI_CFG           (0x0C006024)

// DI command word
typedef union
{
    u8      byte[4];
    u32     word;
} DICMD;

// status register layout
#define DI_SR_BRKINT     (1 << 6)
#define DI_SR_BRKINTMSK  (1 << 5)
#define DI_SR_TCINT      (1 << 4)
#define DI_SR_TCINTMSK   (1 << 3)
#define DI_SR_DEINT      (1 << 2)
#define DI_SR_DEINTMSK   (1 << 1)
#define DI_SR_BRK        (1 << 0)

// cover status
#define DI_CVR_CVRINT    (1 << 2)
#define DI_CVR_CVRINTMSK (1 << 1)
#define DI_CVR_CVR       (1 << 0)

// control register
#define DI_CR_RW         (1 << 2)
#define DI_CR_DMA        (1 << 1)
#define DI_CR_TSTART     (1 << 0)

// cover control
void    DIOpenCover();          // open 
void    DICloseCover();         // close
BOOL    DIGetCoverState();      // 1: cover open, 0: closed

void    DIOpen();
