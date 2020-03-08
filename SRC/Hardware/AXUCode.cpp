// DSP AX slave microcode
#include "pch.h"

#define HI 0
#define LO 1

/*/
    0x0C00500A      DSP Control Register (DSPCR)

        0 0 0 0  0 0 0 0  0 0 0 0  0 0 0 0 
                                     ¦ ¦ ¦
                                     ¦ ¦  -- 0: RES
                                     ¦  ---- 1: INT
                                      ------ 2: HALT
/*/

static BOOL reset, dspint, halt;

static void AXSetResetBit(BOOL val)
{
    reset = val;
    if(reset)
    {
        DBReport(DSP GREEN "AX reset\n");
    }
}
static BOOL AXGetResetBit() { return 0; }

static void AXSetIntBit(BOOL val)
{
    dspint = val;
    if(dspint)
    {
        DBReport(DSP GREEN "AX assert int\n");
    }
}
static BOOL AXGetIntBit() { return 0; }

static void AXSetHaltBit(BOOL val)
{
    if(halt != val)
    {
        DBReport(DSP GREEN "AX halt=%i\n", val);
    }
    halt = val;
}
static BOOL AXGetHaltBit() { return halt; }

/*/
    0x0C005000      DSP Output Mailbox Register High Part (CPU->DSP)
    0x0C005002      DSP Output Mailbox Register Low Part (CPU->DSP)
    0x0C005004      DSP Input Mailbox Register High Part (DSP->CPU)
    0x0C005006      DSP Input Mailbox Register Low Part (DSP->CPU)
/*/

static void AXWriteOutMailboxHi(uint16_t value)
{
    DBReport(DSP RED "write OUT_HI %04X\n", value);
}
static void AXWriteOutMailboxLo(uint16_t value)
{
    DBReport(DSP RED "write OUT_LO %04X\n", value);
}

static uint16_t AXReadOutMailboxHi() { DBReport(DSP RED "read OUT_HI\n"); return dsp.out[HI]; }
static uint16_t AXReadOutMailboxLo() { DBReport(DSP RED "read OUT_LO\n"); return dsp.out[LO]; }

static uint16_t AXReadInMailboxHi()  { DBReport(DSP RED "read IN_HI\n"); return dsp.in[HI]; }
static uint16_t AXReadInMailboxLo()  { DBReport(DSP RED "read IN_LO\n"); return dsp.in[LO]; }

// DSP callbacks

static void AXInit()
{
    // send init confirmation mail to CPU and signal DSP interrupt handler
    dsp.in[HI] = 0xdcd1; dsp.in[LO] = 0;
    DSPAssertInt();
    DBReport(DSP GREEN "DSP Interrupt (AX init mail arrived)\n");
}

static void AXResume()
{
}

DSPMicrocode AXSlave = {
    0, 0, 0, 0, DSP_AX_UCODE,

    // DSPCR callbacks
    AXSetResetBit,       AXGetResetBit,         // RESET
    AXSetIntBit,         AXGetIntBit,           // INT
    AXSetHaltBit,        AXGetHaltBit,          // HALT

    // mailbox callbacks
    AXWriteOutMailboxHi, AXWriteOutMailboxLo,   // write CPU->DSP
    AXReadOutMailboxHi,  AXReadOutMailboxLo,    // read CPU->DSP
    AXReadInMailboxHi,   AXReadInMailboxLo,     // read DSP->CPU

    // DSP callbacks
    AXInit, AXResume
};
