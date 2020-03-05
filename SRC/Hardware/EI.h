#pragma pack(1)

// EXI registers (all registers are 32-bit)
//                      (chan 0)
#define EXI0_CSR        0x0C006800          // Communication Status Register
#define EXI0_MADR       0x0C006804          // DMA Memory Address Register
#define EXI0_LEN        0x0C006808          // DMA Length Register
#define EXI0_CR         0x0C00680C          // Control Register
#define EXI0_DATA       0x0C006810          // Data Register
//                      (chan 1)
#define EXI1_CSR        0x0C006814          // -"-
#define EXI1_MADR       0x0C006818
#define EXI1_LEN        0x0C00681C
#define EXI1_CR         0x0C006820
#define EXI1_DATA       0x0C006824
//                      (chan 2)
#define EXI2_CSR        0x0C006828          // -"-
#define EXI2_MADR       0x0C00682C
#define EXI2_LEN        0x0C006830
#define EXI2_CR         0x0C006834
#define EXI2_DATA       0x0C006838

// EXI Communication Status Register mask
#define EXI_CSR_ROMDIS      (1 << 13)       // disable IPL decryption logic
#define EXI_CSR_EXT         (1 << 12)       // attached status
#define EXI_CSR_EXTINT      (1 << 11)       // attached / detached interrupt
#define EXI_CSR_EXTINTMSK   (1 << 10)
#define EXI_CSR_CS2B        (1 <<  9)       // device select bits
#define EXI_CSR_CS1B        (1 <<  8)
#define EXI_CSR_CS0B        (1 <<  7)
#define EXI_CSR_CLK(reg)    ((reg >> 4) & 7)// dont care (bus clock)
#define EXI_CSR_TCINT       (1 <<  3)       // transfer complete interrupt
#define EXI_CSR_TCINTMSK    (1 <<  2)
#define EXI_CSR_EXIINT      (1 <<  1)       // exi interrupt from devices (IRQ line)
#define EXI_CSR_EXIINTMSK   (1 <<  0)
#define EXI_CSR_INTERRUPTS  (EXI_CSR_EXTINT | EXI_CSR_TCINT | EXI_CSR_EXIINT)
#define EXI_CSR_READONLY    (EXI_CSR_EXT | EXI_CSR_EXTINT | EXI_CSR_TCINT | EXI_CSR_EXIINT)

// EXI Control Register mask
#define EXI_CR_TLEN(reg)    ((reg >> 4) & 3)// immediate data size
#define EXI_CR_RW(reg)      ((reg >> 2) & 3)// direction (read/write)
#define EXI_CR_DMA          (1 << 1)        // select dma transfer (dma/immediate)
#define EXI_CR_TSTART       (1 << 0)        // start transfer

// EXI registers block
typedef struct EXIRegs
{
    uint32_t         csr;            // communication register 
    uint32_t         madr;           // memory address (32 byte aligned)
    uint32_t         len;            // size (32 bytes aligned)
    uint32_t         cr;             // control register
    uint32_t         data;           // immediate data register
} EXIRegs;

// SRAM structure layout. see YAGCD for details.
typedef struct SRAM
{
    uint16_t     checkSum;
    uint16_t     checkSumInv;
    uint32_t     ead0;
    uint32_t     ead1;
    uint32_t     counterBias;
    int8_t       displayOffsetH;
    uint8_t      ntd;
    uint8_t      language;
    uint8_t      flags;
    uint8_t      dummy[44];          // reserved for future        
} SRAM;

// bootrom encoded font sizes
#define ANSI_SIZE   0x3000
#define SJIS_SIZE   0x4D000

// location of SRAM dump in Dolwin filesystem 
#define SRAM_FILE   ".\\Data\\sram.bin"

// ---------------------------------------------------------------------------
// hardware API

// EXI state (registers and other data)
typedef struct EIControl
{
    // hardware state
    EXIRegs     regs[3];        // exi registers
    SRAM        sram;           // battery-backed memory (misc console settings)
    uint8_t*    ansiFont;       // bootrom font (loaded from file)
    uint8_t*    sjisFont;
    BOOL        rtc;            // 1: RTC enabled
    uint32_t    rtcVal;         // last updated RTC value
    uint32_t    ad16;           // trace step
    char        uart[256];      // UART I/O buffer
    uint32_t    upos;           // UART buffer position (if > 0, UART buffer not empty)

    // helper variables used for EXI transfers
    int32_t     chan, sel;      // curent selected chan:device (sel=-1 no device)
    uint32_t    ad16_cmd;       // command for AD16
    BOOL        firstImm;       // first imm write is always command
    uint32_t    mxaddr;         // "address" inside MX chip for transfers
    BOOL        uartNE;         

    BOOL        log;            // allow log EXI activities
    BOOL        osReport;       // allow UART debugger output (log not affecting this)
} EIControl;

extern  EIControl exi;

void    RTCUpdate();

// for memcards and other external devices
void    EXIUpdateInterrupts();
void    EXIAttach(int chan);    // connect device
void    EXIDetach(int chan);    // disconnect device

void    EIOpen();
void    EIClose();

#pragma pack()
