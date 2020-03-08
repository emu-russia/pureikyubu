// infinite timeout interval
#define DSP_INFINITE    (0x7fffffffffffffff)

// known microcodes
enum DSPUID
{
    DSP_FAKE_UCODE   = 'KAKA',  // fake
    DSP_BOOT_UCODE   = 'BOOT',  // boot microcode
    DSP_CARD_UCODE   = 'CARD',  // card unlock
    DSP_AX_UCODE     = 'AXAX',  // AX slave
    DSP_JAUDIO_UCODE = 'JAJA',  // zelda jaudio
};

// microcode
typedef struct DSPMicrocode
{
    uint32_t    ram_addr;       // offset in RAM buffer
    uint16_t    iram_addr;      // offset in DSP IRAM
    uint16_t    iram_len;       // length in bytes
    uint32_t    checksum;       // byte checksum
    DSPUID      uid;            // unique microcode ID (see above)

    // DSPCR callbacks
    void        (*SetResetBit)(BOOL val);
    BOOL        (*GetResetBit)();
    void        (*SetIntBit)(BOOL val);
    BOOL        (*GetIntBit)();
    void        (*SetHaltBit)(BOOL val);
    BOOL        (*GetHaltBit)();

    // mailbox callbacks
    void        (*WriteOutMailboxHi)(uint16_t value);
    void        (*WriteOutMailboxLo)(uint16_t value);
    uint16_t    (*ReadOutMailboxHi)();
    uint16_t    (*ReadOutMailboxLo)();
    uint16_t    (*ReadInMailboxHi)();
    uint16_t    (*ReadInMailboxLo)();

    // DSP callbacks
    void        (*init)();
    void        (*resume)();
} DSPMicrocode;

// DSP state
typedef struct DSPControl
{
    BOOL            fakeMode;   // 1: task is always DSP_FAKE_UCODE
    DSPMicrocode    *task;      // current microcode
    uint16_t        out[2];     // CPU->DSP mailbox
    uint16_t        in[2];      // DSP->CPU mailbox
    int64_t         time;       // for update handler
} DSPControl;

extern  DSPControl dsp;

// DSP controls

/*/
    0x0C00500A      DSP Control Register (DSPCR)

        0 0 0 0  0 0 0 0  0 0 0 0  0 0 0 0 
                                     ¦ ¦ ¦
                                     ¦ ¦  -- 0: RES
                                     ¦  ---- 1: INT
                                      ------ 2: HALT
/*/
void    DSPSetResetBit(BOOL val);
BOOL    DSPGetResetBit();
void    DSPSetIntBit(BOOL val);
BOOL    DSPGetIntBit();
void    DSPSetHaltBit(BOOL val);
BOOL    DSPGetHaltBit();

/*/
    0x0C005000      DSP Output Mailbox Register High Part (CPU->DSP)
    0x0C005002      DSP Output Mailbox Register Low Part (CPU->DSP)
    0x0C005004      DSP Input Mailbox Register High Part (DSP->CPU)
    0x0C005006      DSP Input Mailbox Register Low Part (DSP->CPU)
/*/
void    DSPWriteOutMailboxHi(uint16_t value);
void    DSPWriteOutMailboxLo(uint16_t value);
uint16_t     DSPReadOutMailboxHi();
uint16_t     DSPReadOutMailboxLo();
uint16_t     DSPReadInMailboxHi();
uint16_t     DSPReadInMailboxLo();

void    DSPOpen();
void    DSPClose();
void    DSPUpdate();
void    DSPAssertInt();
