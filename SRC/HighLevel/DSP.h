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
    u32         ram_addr;       // offset in RAM buffer
    u16         iram_addr;      // offset in DSP IRAM
    u16         iram_len;       // length in bytes
    u32         checksum;       // byte checksum
    DSPUID      uid;            // unique microcode ID (see above)

    // DSPCR callbacks
    void        (*SetResetBit)(BOOL val);
    BOOL        (*GetResetBit)();
    void        (*SetIntBit)(BOOL val);
    BOOL        (*GetIntBit)();
    void        (*SetHaltBit)(BOOL val);
    BOOL        (*GetHaltBit)();

    // mailbox callbacks
    void        (*WriteOutMailboxHi)(u16 value);
    void        (*WriteOutMailboxLo)(u16 value);
    u16         (*ReadOutMailboxHi)();
    u16         (*ReadOutMailboxLo)();
    u16         (*ReadInMailboxHi)();
    u16         (*ReadInMailboxLo)();

    // DSP callbacks
    void        (*init)();
    void        (*resume)();
} DSPMicrocode;

// DSP state
typedef struct DSPControl
{
    BOOL            fakeMode;   // 1: task is always DSP_FAKE_UCODE
    DSPMicrocode    *task;      // current microcode
    u16             out[2];     // CPU->DSP mailbox
    u16             in[2];      // DSP->CPU mailbox
    s64             time;       // for update handler
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
void    DSPWriteOutMailboxHi(u16 value);
void    DSPWriteOutMailboxLo(u16 value);
u16     DSPReadOutMailboxHi();
u16     DSPReadOutMailboxLo();
u16     DSPReadInMailboxHi();
u16     DSPReadInMailboxLo();

void    DSPOpen();
void    DSPClose();
void    DSPUpdate();
void    DSPAssertInt();
