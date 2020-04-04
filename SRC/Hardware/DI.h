#pragma pack(push, 1)

// size of DVD data
#define DVD_SIZE         0x57058000     // approx. 1.4 GB
#define DVD_STREAM_BLK   32768          // size of single ADPCM block for playback update

// DI registers 
//                       (32-bit)
#define DI_SR            0x0C006000     // Status Register
#define DI_CVR           0x0C006004     // Cover Register
#define DI_CMDBUF0       0x0C006008     // Command Buffer 0
#define DI_CMDBUF1       0x0C00600C     // Command Buffer 1
#define DI_CMDBUF2       0x0C006010     // Command Buffer 2
#define DI_MAR           0x0C006014     // DMA Memory Address Register
#define DI_LEN           0x0C006018     // DMA Transfer Length Register
#define DI_CR            0x0C00601C     // Control Register
#define DI_IMMBUF        0x0C006020     // Immediate Data Buffer
#define DI_CFG           0x0C006024     // Configuration Register
//                       (16-bit)
#define DI_SR_H          0x0C006000
#define DI_SR_L          0x0C006002
#define DI_CVR_H         0x0C006004
#define DI_CVR_L         0x0C006006
#define DI_CMDBUF0_H     0x0C006008
#define DI_CMDBUF0_L     0x0C00600A
#define DI_CMDBUF1_H     0x0C00600C
#define DI_CMDBUF1_L     0x0C00600E
#define DI_CMDBUF2_H     0x0C006010
#define DI_CMDBUF2_L     0x0C006012
#define DI_MAR_H         0x0C006014
#define DI_MAR_L         0x0C006016
#define DI_LEN_H         0x0C006018
#define DI_LEN_L         0x0C00601A
#define DI_CR_H          0x0C00601C
#define DI_CR_L          0x0C00601E
#define DI_IMMBUF_H      0x0C006020
#define DI_IMMBUF_L      0x0C006022
#define DI_CFG_H         0x0C006024
#define DI_CFG_L         0x0C006026

#define DISR             di.sr
#define DICVR            di.cvr
#define DIMAR            di.mar
#define DILEN            di.len
#define DICR             di.cr

// DI Status Register mask
#define DI_SR_BRKINT     (1 << 6)
#define DI_SR_BRKINTMSK  (1 << 5)
#define DI_SR_TCINT      (1 << 4)
#define DI_SR_TCINTMSK   (1 << 3)
#define DI_SR_DEINT      (1 << 2)
#define DI_SR_DEINTMSK   (1 << 1)
#define DI_SR_BRK        (1 << 0)

// DI Cover Register mask
#define DI_CVR_CVRINT    (1 << 2)
#define DI_CVR_CVRINTMSK (1 << 1)
#define DI_CVR_CVR       (1 << 0)

// DI Control Register mask
#define DI_CR_RW         (1 << 2)
#define DI_CR_DMA        (1 << 1)
#define DI_CR_TSTART     (1 << 0)

// ---------------------------------------------------------------------------
// hardware API

// DI state (registers and other data)
typedef struct DIControl
{
    uint32_t        sr, cvr, cr;    // DI registers
    uint32_t        mar, len;
    uint32_t        cmdbuf[3];
    uint32_t        immbuf;
    uint32_t        cfg;

    bool            coverst;        // 1: cover open, 0: closed
    bool            streaming;      // 1: streaming audio enabled
    uint32_t        strseek;        // streaming position on disk
    int32_t         strcount;       // streaming counter (streaming will stop, when reach zero)
    uint8_t         *workArea;      // streaming work area

    bool            running;        // DI subsystem is online
    bool            log;
} DIControl;

extern  DIControl di;

void    DIOpenCover();          // open
void    DICloseCover();         // close
bool    DIGetCoverState();      // 1: cover open, 0: closed
void    DIStreamUpdate();       // update DVD streaming playback, called every VIINT
void    DIOpen();
void    DIClose();

#pragma pack(pop)
