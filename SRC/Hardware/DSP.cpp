// DSP microcode emulation
#include "pch.h"

// DSP state
DSPControl dsp;

static  DSPMicrocode temp;
static  uint16_t tempOut[2], tempIn[2];
static  uint32_t cardWorkarea;

#define HI 0
#define LO 1

// ---------------------------------------------------------------------------
// DSP controls (use current task)

/*/
    0x0C00500A      DSP Control Register (DSPCR)

        0 0 0 0  0 0 0 0  0 0 0 0  0 0 0 0 
                                     ¦ ¦ ¦
                                     ¦ ¦  -- 0: RES
                                     ¦  ---- 1: INT
                                      ------ 2: HALT
/*/

void DSPSetResetBit(bool val) { dsp.task->SetResetBit(val); }
bool DSPGetResetBit() { return dsp.task->GetResetBit(); }

void DSPSetIntBit(bool val) { dsp.task->SetIntBit(val); }
bool DSPGetIntBit() { return dsp.task->GetIntBit(); }

void DSPSetHaltBit(bool val) { dsp.task->SetHaltBit(val); }
bool DSPGetHaltBit() { return dsp.task->GetHaltBit(); }

/*/
    0x0C005000      DSP Output Mailbox Register High Part (CPU->DSP)
    0x0C005002      DSP Output Mailbox Register Low Part (CPU->DSP)
    0x0C005004      DSP Input Mailbox Register High Part (DSP->CPU)
    0x0C005006      DSP Input Mailbox Register Low Part (DSP->CPU)
/*/

void DSPWriteOutMailboxHi(uint16_t value) { dsp.task->WriteOutMailboxHi(value); }
void DSPWriteOutMailboxLo(uint16_t value) { dsp.task->WriteOutMailboxLo(value); }

uint16_t DSPReadOutMailboxHi() { return dsp.task->ReadOutMailboxHi(); }
uint16_t DSPReadOutMailboxLo() { return dsp.task->ReadOutMailboxLo(); }

uint16_t DSPReadInMailboxHi()  { return dsp.task->ReadInMailboxHi(); }
uint16_t DSPReadInMailboxLo()  { return dsp.task->ReadInMailboxLo(); }

// ---------------------------------------------------------------------------
// simple fake microcode

static void Fake_SetResetBit(bool val) {}
static bool Fake_GetResetBit() { return 0; }

static void Fake_SetIntBit(bool val) {}
static bool Fake_GetIntBit() { return 0; }

static void Fake_SetHaltBit(bool val) {}
static bool Fake_GetHaltBit() { return 0; }

static void NoWriteMailbox(uint16_t value) {}
static uint16_t NoReadMailbox() { return 0; }
static uint16_t FakeOutMailboxHi()
{
    static uint16_t mbox = 0;
    if(mbox == 0) mbox = 0x8000;
    else mbox = 0;
    return mbox;
}
static uint16_t FakeInMailboxHi() { return 0x8071; }
static uint16_t FakeInMailboxLo() { return 0xfeed; }

static DSPMicrocode fakeUcode = {
    0, 0, 0, 0, DSP_FAKE_UCODE,

    // DSPCR callbacks
    Fake_SetResetBit, Fake_GetResetBit,     // RESET
    Fake_SetIntBit,   Fake_GetIntBit,       // INT
    Fake_SetHaltBit,  Fake_GetHaltBit,      // HALT

    // mailbox callbacks
    NoWriteMailbox,   NoWriteMailbox,       // write CPU->DSP
    FakeOutMailboxHi, NoReadMailbox,        // read CPU->DSP
    FakeInMailboxHi,  FakeInMailboxLo       // read DSP->CPU
};

// ---------------------------------------------------------------------------
// DSP boot microcode

static void IROM_SetResetBit(bool val)
{
    DBReport(DSP RED "set RESET bit: %i\n", val);
}

static void IROM_SetIntBit(bool val)
{
    DBReport(DSP RED "set INT bit: %i\n", val);
}

static void IROM_SetHaltBit(bool val)
{
    DBReport(DSP RED "set HALT bit: %i\n", val);
}

void IROMWriteOutMailboxHi(uint16_t value)
{
    dsp.out[HI] = value | 0x8000;
    if(dsp.time != DSP_INFINITE) return;
    switch(dsp.out[HI])
    {
        case 0x8000:    // reset
            dsp.in[HI] = dsp.in[LO] = 0;
            dsp.time = TBR + 100;
            tempOut[HI] = 0x8000;
            return;
        case 0x80f3:    // boot
            tempOut[HI] = 0x80f3;
            return;
        case 0xcdd1:    // sync
            return;
    }
    DolwinReport(DSP RED "write OUT_HI %04X\n", value);
}
void IROMWriteOutMailboxLo(uint16_t value)
{
    dsp.out[LO] = value;
    if(dsp.time != DSP_INFINITE)
    {
        DSPUpdate();
        return;
    }
    switch(dsp.out[LO])
    {
        case 0x0002:    // sync stop
            if(dsp.out[HI] == 0xcdd1)
            {
                dsp.out[HI] &= ~0x8000; // delivered
                dsp.in[HI] = 0x8071;
                dsp.in[LO] = 0xfeed;
                return;
            }
            break;
        case 0xa001:    // boot mainmem address
            if(dsp.out[HI] == 0x80f3)
            {
                dsp.out[HI] &= ~0x8000; // delivered
                temp.ram_addr = 0;
                tempOut[LO] = 0xa001;
                dsp.time = TBR + 50;
                return;
            }
            break;
        case 0xc002:    // boot iram address
            if(dsp.out[HI] == 0x80f3)
            {
                dsp.out[HI] &= ~0x8000; // delivered
                temp.iram_addr = 0;
                tempOut[LO] = 0xc002;
                dsp.time = TBR + 50;
                return;
            }
            break;
        case 0xa002:    // boot iram length
            if(dsp.out[HI] == 0x80f3)
            {
                dsp.out[HI] &= ~0x8000; // delivered
                temp.iram_len = 0;
                tempOut[LO] = 0xa002;
                dsp.time = TBR + 50;
                return;
            }
            break;
        case 0xb002:    // boot dram address (always 0)
            if(dsp.out[HI] == 0x80f3)
            {
                dsp.out[HI] &= ~0x8000; // delivered
                tempOut[LO] = 0xb002;
                dsp.time = TBR + 50;
                return;
            }
            break;
        case 0xd001:    // boot iram init vector
            if(dsp.out[HI] == 0x80f3)
            {
                dsp.out[HI] &= ~0x8000; // delivered
                tempOut[LO] = 0xd001;
                dsp.time = TBR + 50;
                return;
            }
            break;
    }
    DolwinReport(DSP RED "write OUT_LO %04X\n", value);
}

uint16_t IROMReadOutMailboxHi() { DBReport(DSP RED "read OUT_HI\n"); return dsp.out[HI]; }
uint16_t IROMReadOutMailboxLo() { DBReport(DSP RED "read OUT_LO\n"); return dsp.out[LO]; }

uint16_t IROMReadInMailboxHi()  { DBReport(DSP RED "read IN_HI\n"); return dsp.in[HI]; }
uint16_t IROMReadInMailboxLo()  { DBReport(DSP RED "read IN_LO\n"); return dsp.in[LO]; }

static DSPMicrocode bootUcode = {
    0, 0, 0, 0, DSP_BOOT_UCODE,

    // DSPCR callbacks
    IROM_SetResetBit,      Fake_GetResetBit,        // RESET
    IROM_SetIntBit,        Fake_GetIntBit,          // INT
    IROM_SetHaltBit,       Fake_GetHaltBit,         // HALT

    // mailbox callbacks
    IROMWriteOutMailboxHi, IROMWriteOutMailboxLo,   // write CPU->DSP
    IROMReadOutMailboxHi,  IROMReadOutMailboxLo,    // read CPU->DSP
    IROMReadInMailboxHi,   IROMReadInMailboxLo      // read DSP->CPU
};

// ---------------------------------------------------------------------------
// card unlock microcode

static void CARDWriteOutMailboxHi(uint16_t value)
{
    dsp.out[HI] = value | 0x8000;
    if(dsp.time != DSP_INFINITE) return;
    switch(dsp.out[HI])
    {
        case 0xff00:    // unlock memcard
            tempOut[HI] = 0xff00;
            return;
    }
    DBReport(DSP RED "write OUT_HI %04X\n", value);
}
static void CARDWriteOutMailboxLo(uint16_t value)
{
    dsp.out[LO] = value;
    if(dsp.time != DSP_INFINITE)
    {
        DSPUpdate();
        return;
    }
    switch(dsp.out[LO])
    {
        case 0x0000:    // unlock memcard
            if(dsp.out[HI] == 0xff00)
            {
                dsp.out[HI] &= ~0x8000;
                tempOut[LO] = 0x0000;
                dsp.time = TBR + 50;
                return;
            }
            break;
    }
    DBReport(DSP RED "write OUT_LO %04X\n", value);
}

static void CARDInit()
{
    // send init confirmation mail to CPU and signal DSP interrupt handler
    dsp.in[HI] = 0xdcd1; dsp.in[LO] = 0;
    DSPAssertInt();
    DBReport(DSP GREEN "DSP Interrupt (card init)\n");
}

static void CARDResume()
{
    // unlock (what may do those 300 bytes of microcode?)
    // ...

    // send donw confirmation mail to CPU and signal DSP interrupt handler
    dsp.in[HI] = 0xdcd1; dsp.in[LO] = 3;
    DSPAssertInt();
    DBReport(DSP GREEN "DSP Interrupt (card done)\n");

    // fallback to boot microcode
    dsp.task = &bootUcode;
}

static DSPMicrocode cardUnlock = {
    0, 0, 0x160, 0, DSP_CARD_UCODE,

    // DSPCR callbacks
    Fake_SetResetBit,      Fake_GetResetBit,        // RESET
    Fake_SetIntBit,        Fake_GetIntBit,          // INT
    Fake_SetHaltBit,       Fake_GetHaltBit,         // HALT

    // mailbox callbacks
    CARDWriteOutMailboxHi, CARDWriteOutMailboxLo,   // write CPU->DSP
    IROMReadOutMailboxHi,  IROMReadOutMailboxLo,    // read CPU->DSP
    IROMReadInMailboxHi,   IROMReadInMailboxLo,     // read DSP->CPU

    CARDInit,
    CARDResume
};

static void  __CARDUnlock () { 
//int32_t __CARDUnlock ( int32_t chan, u8 * unlockKey ) 

    uint32_t address = SYMAddress("__CARDMountCallback");

    //void __CARDMountCallback (s32 chan, s32 status ); 
    GPR[4] = 0; 
    HLEExecuteCallback (address) ; 
    GPR[3] = 0; 
} 

// ---------------------------------------------------------------------------
// init and update

// calculate ucode byte checksum
static uint32_t DSP_ucode_checksum(uint8_t *ptr, int len)
{
    uint32_t sum = 0;
    for(int n=0; n<len; n++)
    {
        sum += ptr[n];
    }

    // dump ucode
/*/
    char ucname[256];
    sprintf(ucname, "uc%08X.bin", sum);
    FileSave(ucname, ptr, len);
/*/

    return sum;
}

static DSPMicrocode * DSP_find_ucode(uint16_t iram_addr, uint16_t iram_len, uint32_t sum)
{
    switch(sum)
    {
        // card unlock microcode (small one, 300 bytes only)
        case 0x00006F87:
            DBReport(DSP GREEN "card unlock microcode booted!\n");
            return &cardUnlock;

        // AX slave microcode
        case 0x00067738:        // sep 2001
        case 0x0008134F:        // dec 2001
        case 0x00073930:        // apr 2002
        case 0x0009D716:        // jun 2002
            DBReport(DSP GREEN "AX slave microcode booted!\n");
            return &AXSlave;
    }

    DolwinError("DSP", "Unknown UCode : %08X", sum);
    return NULL;    // not found
}

static void DSPSwitchTask(DSPUID uid)
{
    switch(uid)
    {
        case DSP_FAKE_UCODE:
            dsp.task = &fakeUcode;
            break;
        case DSP_BOOT_UCODE:
            dsp.task = &bootUcode;
            break;
        default:
            DolwinError(
                "DSP", "Unknown DSP microcode : %c%c%c%c",
                (uid >> 24) & 0xff,
                (uid >> 16) & 0xff,
                (uid >>  8) & 0xff,
                (uid >>  0) & 0xff
            );
            break;
    }
}

void DSPAssertInt()
{
    AIDCR |= AIDCR_DSPINT;
    if(AIDCR & AIDCR_DSPINTMSK)
    {
        PIAssertInt(PI_INTERRUPT_DSP);
    }
}

void DSPOpen(HWConfig* config)
{
    dsp.fakeMode = config->dspFakeMode;
    if(dsp.fakeMode)
    {
        DBReport(DSP "working in fake mode\n");
        DSPSwitchTask(DSP_FAKE_UCODE);
    }
    else
    {
        DBReport(DSP "working in simulate mode\n");
        DSPSwitchTask(DSP_BOOT_UCODE);
    }

    if(dsp.fakeMode)
    {
        HLESetCall("__OSInitAudioSystem", os_ignore);
        HLESetCall("DSPSendCommands2__FPUlUlPFUs_v", os_ignore);
        HLESetCall("DsetupTable__FUlUlUlUlUl", os_ignore);
        HLESetCall("DsetDolbyDelay__FUlUs", os_ignore);
        HLESetCall("__AXOutInitDSP", os_ignore);

        HLESetCall("__CARDUnlock", __CARDUnlock);
    }

    dsp.time = DSP_INFINITE;
}

void DSPClose()
{
}

void DSPUpdate()
{
    if(TBR >= dsp.time)
    {
        dsp.time = DSP_INFINITE;
        DBReport(DSP GREEN "UPDATE!!\n");
        switch(tempOut[HI] & ~0x8000)
        {
            case 0x0000:        // reset acknowledge
                dsp.in[HI] = 0x8071; dsp.in[LO] = 0xfeed;   // ready to boot
                DBReport(DSP GREEN "__OSInitAudioSystem DSP reset\n");
                return;
            case 0x00f3:        // send task parameters
                dsp.out[HI] &= ~0x8000;
                switch(tempOut[LO])
                {
                    case 0xa001:
                        temp.ram_addr = (dsp.out[HI] << 16) | dsp.out[LO];
                        DBReport(DSP GREEN "boot task : IRAM MMEM ADDR: 0x%08X\n", temp.ram_addr);
                        break;
                    case 0xc002:
                        temp.iram_addr = (dsp.out[HI] << 16) | dsp.out[LO];
                        DBReport(DSP GREEN "boot task : IRAM DSP ADDR : 0x%08X\n", temp.iram_addr);
                        break;
                    case 0xa002:
                        temp.iram_len = (dsp.out[HI] << 16) | dsp.out[LO];
                        DBReport(DSP GREEN "boot task : IRAM LENGTH   : 0x%08X\n", temp.iram_len);
                        break;
                    case 0xb002:
                        DBReport(DSP GREEN "boot task : DRAM MMEM ADDR: 0x%08X\n", (dsp.out[HI] << 16) | dsp.out[LO]);
                        break;
                    case 0xd001:
                        DBReport(DSP GREEN "boot task : Start Vector  : 0x%08X\n", (dsp.out[HI] << 16) | dsp.out[LO]);
                        dsp.time = TBR + temp.iram_len / 4;
                        tempOut[HI] = 0x0abc;
                        break;
                }
                return;
            case 0x0abc:        // do actual task boot
            {
                uint32_t sum  = DSP_ucode_checksum(&mi.ram[temp.ram_addr], temp.iram_len);
                dsp.task = DSP_find_ucode(temp.iram_addr, temp.iram_len, sum);
                if(dsp.task->init) dsp.task->init();
                return;
            }
            case 0x7f00:        // unlock memcard
                if(tempOut[LO] == 0)
                {
                    dsp.out[HI] &= ~0x8000;
                    cardWorkarea = (dsp.out[HI] << 16) | dsp.out[LO];
                    DBReport(DSP GREEN "unlocking card, block from 0x%08X\n", cardWorkarea);
                    if(dsp.task->resume) dsp.task->resume();
                }
                return;
        }
    }
}
