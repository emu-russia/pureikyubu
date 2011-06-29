/******* OLDER SI FROM 0.09 *******/

#define SI_OLD

// TODO : move these flags into user vars
//#define DISABLE_PADS        // force, to completely disable PAD reading
#define USE_RUMBLE_PAD      // enable rumbling functions
#define TWO_PADS_ONLY       // only first two PADs are emulated

#pragma pack(1)
typedef struct
{
    u32     command;
    u16     button;
    s8      stickX;
    s8      stickY;
    s8      substickX;
    s8      substickY;
    u8      triggerLeft;
    u8      triggerRight;
} PADBUF;
#pragma pack()

// si polling register mask
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

// si communication control / status register mask
#define SI_COMCSR_TCINT         (1 << 31)
#define SI_COMCSR_TCINTMSK      (1 << 30)
#define SI_COMCSR_COMERR        (1 << 29)
#define SI_COMCSR_RDSTINT       (1 << 28)
#define SI_COMCSR_RDSTINTMSK    (1 << 27)
#define SI_COMCSR_OUTLEN(reg)   ((reg >> 16) & 0x7f)
#define SI_COMCSR_INLEN(reg)    ((reg >>  8) & 0x7f)
#define SI_COMCSR_CHAN(reg)     ((reg >> 1) & 3)
#define SI_COMCSR_TSTART        (1)

// si status register mask
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

void    SIPoll();
void    SIOpen();
