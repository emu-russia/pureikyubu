#pragma pack(1)

#define ARAMSIZE        (16 * 1024 * 1024)  // 16 mb
#define ARAM            aram.mem

// aram dma transfer type
#define RAM_TO_ARAM     0
#define ARAM_TO_RAM     1

// known ARAM controller registers
#define AR_SIZE             0x0C005012      // 16 bit regs
#define AR_MODE             0x0C005016
#define AR_REFRESH          0x0C00501A
#define AR_DMA_MMADDR_H     0x0C005020
#define AR_DMA_MMADDR_L     0x0C005022
#define AR_DMA_ARADDR_H     0x0C005024
#define AR_DMA_ARADDR_L     0x0C005026
#define AR_DMA_CNT_H        0x0C005028
#define AR_DMA_CNT_L        0x0C00502A
#define AR_DMA_MMADDR       0x0C005020      // 32 bit regs
#define AR_DMA_ARADDR       0x0C005024
#define AR_DMA_CNT          0x0C005028

// not sure about AR_SIZE, AR_MODE and AR_REFRESH.

// ---------------------------------------------------------------------------
// hardware API

// ARAM state (registers and other data)
typedef struct ARControl
{
    u8*         mem;                // aux. memory buffer (size is ARAMSIZE)
    u32         mmaddr, araddr;     // DMA address
    u32         cnt;                // count
    BOOL        cntv[2];            // count register validate state
    u16         size;               // "AR_SIZE" (0x5012) register
} ARControl;

void    AROpen();
void    ARClose();

extern  ARControl aram;

#pragma pack()
