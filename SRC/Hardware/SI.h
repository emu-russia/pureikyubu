#pragma pack(1)

// SI registers (all registers are 32-bit)

#define SI_CHAN0_OUTBUF     0x0C006400      // Channel 0 Output Buffer
#define SI_CHAN0_INBUFH     0x0C006404      // Channel 0 Input Buffer High
#define SI_CHAN0_INBUFL     0x0C006408      // Channel 0 Input Buffer Low
#define SI_CHAN1_OUTBUF     0x0C00640C      // Channel 1 Output Buffer
#define SI_CHAN1_INBUFH     0x0C006410      // Channel 1 Input Buffer High
#define SI_CHAN1_INBUFL     0x0C006414      // Channel 1 Input Buffer Low
#define SI_CHAN2_OUTBUF     0x0C006418      // Channel 2 Output Buffer
#define SI_CHAN2_INBUFH     0x0C00641C      // Channel 2 Input Buffer High
#define SI_CHAN2_INBUFL     0x0C006420      // Channel 2 Input Buffer Low
#define SI_CHAN3_OUTBUF     0x0C006424      // Channel 3 Output Buffer
#define SI_CHAN3_INBUFH     0x0C006428      // Channel 3 Input Buffer High
#define SI_CHAN3_INBUFL     0x0C00642C      // Channel 3 Input Buffer Low
#define SI_POLL             0x0C006430      // Poll Register
#define SI_COMCSR           0x0C006434      // Communication Control Status Register
#define SI_SR               0x0C006438      // Status Register
#define SI_EXILK            0x0C00643C      // EXI Clock Lock (unused)
#define SI_COMBUF           0x0C006480      // Communication Buffer (128 bytes)

#define SI_POLL_REG         si.poll
#define SI_COMCSR_REG       si.comcsr
#define SI_SR_REG           si.sr

// SI Poll Register mask
#define SI_POLL_X(reg)          ((reg >>16) & 0x3ff)
#define SI_POLL_Y(reg)          ((reg >> 8) & 0xff)
#define SI_POLL_EN0             (1 << 7)
#define SI_POLL_EN1             (1 << 6)
#define SI_POLL_EN2             (1 << 5)
#define SI_POLL_EN3             (1 << 4)
#define SI_POLL_VBCPY0          (1 << 3)
#define SI_POLL_VBCPY1          (1 << 2)
#define SI_POLL_VBCPY2          (1 << 1)
#define SI_POLL_VBCPY3          (1 << 0)

// SI Communication Control Status Register mask
#define SI_COMCSR_TCINT         (1 << 31)
#define SI_COMCSR_TCINTMSK      (1 << 30)
#define SI_COMCSR_COMERR        (1 << 29)
#define SI_COMCSR_RDSTINT       (1 << 28)
#define SI_COMCSR_RDSTINTMSK    (1 << 27)
#define SI_COMCSR_OUTLEN(reg)   ((reg >> 16) & 0x7f)
#define SI_COMCSR_INLEN(reg)    ((reg >>  8) & 0x7f)
#define SI_COMCSR_CHAN(reg)     ((reg >> 1) & 3)
#define SI_COMCSR_TSTART        (1)

// SI Status Register mask
#define SI_SR_WR                (1 << 31)
#define SI_SR_RDST0             (1 << 29)
#define SI_SR_WRST0             (1 << 28)
#define SI_SR_NOREP0            (1 << 27)
#define SI_SR_COLL0             (1 << 26)
#define SI_SR_OVRUN0            (1 << 25)
#define SI_SR_UNRUN0            (1 << 24)
#define SI_SR_RDST1             (1 << 21)
#define SI_SR_WRST1             (1 << 20)
#define SI_SR_NOREP1            (1 << 19)
#define SI_SR_COLL1             (1 << 18)
#define SI_SR_OVRUN1            (1 << 17)
#define SI_SR_UNRUN1            (1 << 16)
#define SI_SR_RDST2             (1 << 13)
#define SI_SR_WRST2             (1 << 12)
#define SI_SR_NOREP2            (1 << 11)
#define SI_SR_COLL2             (1 << 10)
#define SI_SR_OVRUN2            (1 <<  9)
#define SI_SR_UNRUN2            (1 <<  8)
#define SI_SR_RDST3             (1 <<  5)
#define SI_SR_WRST3             (1 <<  4)
#define SI_SR_NOREP3            (1 <<  3)
#define SI_SR_COLL3             (1 <<  2)
#define SI_SR_OVRUN3            (1 <<  1)
#define SI_SR_UNRUN3            (1 <<  0)

// ---------------------------------------------------------------------------
// hardware API

// SI state (registers and other data)
typedef struct SIControl
{
    uint32_t            out[4], shdw[4];// out + shadows
    uint32_t            poll;           // poll control
    uint32_t            comcsr;         // CSR
    uint32_t            sr;             // status
    uint32_t            exilk;          // EXILK dummy
    uint8_t             combuf[128+32]; // communication buffer (+ overrun protection)
    
    PADState            pad[4];         // PAD state (inbuf replacement)
    bool                rumble[4];      // rumble support flags for every controller
                                        // filled when SI is inited, by checking PADSetRumble

    bool                fake;           // SI is in fake mode
    bool                log;            // do debugger log output
} SIControl;

extern  SIControl si;

void    SIPoll();
void    SIOpen(bool fake=false);

#pragma pack()
