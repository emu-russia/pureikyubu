/******* OLDER SI FROM 0.09 *******/

// low-level serial interface impl.
// external plug-in is used for platform impl.
// ref. us pat. 6,609,977 
//
// TODO : check PAD timing
#include "dolphin.h"

//
// local data
//

// serial interface registers
static PADBUF pad[4];                   // joypad in/out buffers
static u32    cmdShadow[4];             // shadow command regs
static int    siinlen, sioutlen;        // comm buffer in/out data length
static u8     SICOM[128];               // communication buffer
static u32    SIPOLL, SICOMCSR, SISR;   // control registers

// input plugin data
static PADState padd;

// ---------------------------------------------------------------------------

// command parser

//
// command data are sending via communication buffer,
// we are parsing command opcode and forming response 
// data packet, written back to the communication buffer
//

static void SIReadBuffer(int chan, u8 *ptr)
{
    u8  cmd = ptr[0];

    switch(cmd)
    {
        // get device type and status
        case 0x00:
        {
            // 0 : use sub-type
            // 2 : n64 mouse
            // 5 : n64 controller
            // 9 : default gc controller
#ifdef TWO_PADS_ONLY
            if(chan & 2) ptr[0] = 0;
            else ptr[0] = 9;
#else
            ptr[0] = 9;
#endif  
            ptr[1] = 0;     // sub-type
            ptr[2] = 0;     // always 0 ?
            break;
        }

        // HACK : use it for first time
        case 0x40:
        case 0x42:
            return;

        // unknown, freeloader uses it, when booting
        // case 0x40:

        // read origins
        case 0x41:
#ifdef TWO_PADS_ONLY
            if(chan & 2) break;
#endif
            ptr[0] = 0x41;
            ptr[1] = 0;
            ptr[2] = ptr[3] = ptr[4] = ptr[5] = 0x80;
            ptr[6] = ptr[7] = 0x1f;
            break;

        // calibrate
        //case 0x42:

        default:
        {
            DolwinReport(
                "unknown SI command\n"
                "chan:%i\n"
                "cmd:%02X\n"
                "out:%i in:%i\n"
                "pc:%08X",
                chan, cmd, sioutlen, siinlen, PC
            );
        }
    }
}

// ---------------------------------------------------------------------------

// pad buffers

//
// command output buffer is read/write
//

static void __fastcall si0_WriteOutBuf(u32 addr, u32 data)    // write
{
    cmdShadow[0] = data;
    SISR |= SI_SR_WRST0;

    // control motor
#ifdef  USE_RUMBLE_PAD
    if(data == 0x00400001) PADSetRumble(0, TRUE);
    else if(data == 0x00400000) PADSetRumble(0, FALSE);
#endif
}

static void __fastcall si0_ReadOutBuf(u32 addr, u32 *reg)     // read
{
    *reg = pad[0].command;
}

static void __fastcall si1_WriteOutBuf(u32 addr, u32 data)    // write
{
    cmdShadow[1] = data;
    SISR |= SI_SR_WRST1;

    // control motor
#ifdef  USE_RUMBLE_PAD
    if(data == 0x00400001) PADSetRumble(1, TRUE);
    else if(data == 0x00400000) PADSetRumble(1, FALSE);
#endif
}

static void __fastcall si1_ReadOutBuf(u32 addr, u32 *reg)     // read
{
    *reg = pad[1].command;
}

#ifndef TWO_PADS_ONLY

static void __fastcall si2_WriteOutBuf(u32 addr, u32 data)    // write
{
    cmdShadow[2] = data;
    SISR |= SI_SR_WRST2;

    // control motor
#ifdef  USE_RUMBLE_PAD
    if(data == 0x00400001) PADSetRumble(2, TRUE);
    else if(data == 0x00400000) PADSetRumble(2, FALSE);
#endif
}

static void __fastcall si2_ReadOutBuf(u32 addr, u32 *reg)     // read
{
    *reg = pad[2].command;
}

static void __fastcall si3_WriteOutBuf(u32 addr, u32 data)    // write
{
    cmdShadow[3] = data;
    SISR |= SI_SR_WRST3;

    // control motor
#ifdef  USE_RUMBLE_PAD
    if(data == 0x00400001) PADSetRumble(3, TRUE);
    else if(data == 0x00400000) PADSetRumble(3, FALSE);
#endif
}

static void __fastcall si3_ReadOutBuf(u32 addr, u32 *reg)     // read
{
    *reg = pad[3].command;
}

#endif  // TWO_PADS_ONLY

//
// input buffers are read only
//

static void __fastcall si0_InBufLo(u32 addr, u32 *reg)
{
    u32 res;

    // return swapped joypad values
    res  = (u8)pad[0].stickY;
    res |= (u8)pad[0].stickX << 8;
    res |= pad[0].button << 16;

    // clear RDST mask and interrupt
    SISR &= ~SI_SR_RDST0;
    if((SISR & 
       (SI_SR_RDST0 | 
        SI_SR_RDST1 | 
        SI_SR_RDST2 | 
        SI_SR_RDST3)) == 0)
    {
        SICOMCSR &= ~SI_COMCSR_RDSTINT;
        PIClearInt(PI_INTERRUPT_SI);
    }

    *reg = res;
}

static void __fastcall si0_InBufHi(u32 addr, u32 *reg)
{
    u32 res;

    // return swapped joypad values
    res  = (u8)pad[0].triggerRight;
    res |= (u8)pad[0].triggerLeft << 8;
    res |= (u8)pad[0].substickY << 16;
    res |= (u8)pad[0].substickX << 24;

    *reg = res;
}

static void __fastcall si1_InBufLo(u32 addr, u32 *reg)
{
    u32 res;

    // return swapped joypad values
    res  = (u8)pad[1].stickY;
    res |= ((u8)pad[1].stickX << 8);
    res |= (pad[1].button << 16);

    // clear RDST mask and interrupt
    SISR &= ~SI_SR_RDST1;
    if((SISR & 
       (SI_SR_RDST0 | 
        SI_SR_RDST1 | 
        SI_SR_RDST2 | 
        SI_SR_RDST3)) == 0)
    {
        SICOMCSR &= ~SI_COMCSR_RDSTINT;
        PIClearInt(PI_INTERRUPT_SI);
    }

    *reg = res;
}

static void __fastcall si1_InBufHi(u32 addr, u32 *reg)
{
    u32 res;

    // return swapped joypad values
    res  = (u8)pad[1].triggerRight;
    res |= (u8)pad[1].triggerLeft << 8;
    res |= (u8)pad[1].substickY << 16;
    res |= (u8)pad[1].substickX << 24;

    *reg = res;
}

#ifndef TWO_PADS_ONLY

static void __fastcall si2_InBufLo(u32 addr, u32 *reg)
{
    u32 res;

    // return swapped joypad values
    res  = (u8)pad[2].stickY;
    res |= ((u8)pad[2].stickX << 8);
    res |= (pad[2].button << 16);

    // clear RDST mask and interrupt
    SISR &= ~SI_SR_RDST2;
    if((SISR & 
       (SI_SR_RDST0 | 
        SI_SR_RDST1 | 
        SI_SR_RDST2 | 
        SI_SR_RDST3)) == 0)
    {        
        SICOMCSR &= ~SI_COMCSR_RDSTINT;
        PIClearInt(PI_INTERRUPT_SI);
    }

    *reg = res;
}

static void __fastcall si2_InBufHi(u32 addr, u32 *reg)
{
    u32 res;

    // return swapped joypad values
    res  = (u8)pad[2].triggerRight;
    res |= (u8)pad[2].triggerLeft << 8;
    res |= (u8)pad[2].substickY << 16;
    res |= (u8)pad[2].substickX << 24;

    *reg = res;
}

static void __fastcall si3_InBufLo(u32 addr, u32 *reg)
{
    u32 res;

    // return swapped joypad values
    res  = (u8)pad[3].stickY;
    res |= ((u8)pad[3].stickX << 8);
    res |= (pad[3].button << 16);

    // clear RDST mask and interrupt
    SISR &= ~SI_SR_RDST3;
    if((SISR & 
       (SI_SR_RDST0 | 
        SI_SR_RDST1 | 
        SI_SR_RDST2 | 
        SI_SR_RDST3)) == 0)
    {
        SICOMCSR &= ~SI_COMCSR_RDSTINT;
        PIClearInt(PI_INTERRUPT_SI);
    }

    *reg = res;
}

static void __fastcall si3_InBufHi(u32 addr, u32 *reg)
{
    u32 res;

    // return swapped joypad values
    res  = (u8)pad[3].triggerRight;
    res |= (u8)pad[3].triggerLeft << 8;
    res |= (u8)pad[3].substickY << 16;
    res |= (u8)pad[3].substickX << 24;

    *reg = res;
}

#endif  // TWO_PADS_ONLY

//
// communication buffer writes
// 

static void __fastcall sicom_write(u32 addr, u32 data)
{
    unsigned ofs = addr & 0x7f;
    u8      *out = (u8 *)&data;

    SICOM[ofs+0] = out[3];
    SICOM[ofs+1] = out[2];
    SICOM[ofs+2] = out[1];
    SICOM[ofs+3] = out[0];
}

static void __fastcall sicom_read(u32 addr, u32 *reg)
{
    unsigned ofs = addr & 0x7f;
    u8      *in  = (u8 *)reg;

    in[0] = SICOM[ofs+3];
    in[1] = SICOM[ofs+2];
    in[2] = SICOM[ofs+1];
    in[3] = SICOM[ofs+0];
}

// ---------------------------------------------------------------------------

// si control registers

//
// polling register
//

// external for video interface (retrace callback routine)
void SIPoll()
{
#ifndef DISABLE_PADS
    BeginProfilePAD();

    if(SIPOLL & SI_POLL_EN0)
    {
        // update pad input buffer
        PADReadButtons(0, &padd);
        memcpy(&pad[0].button, &padd, sizeof(PADState));

        // set RDST flag
        SISR |= SI_SR_RDST0;
        SICOMCSR |= SI_COMCSR_RDSTINT;
    }

    if(SIPOLL & SI_POLL_EN1)
    {
        // update pad input buffer
        PADReadButtons(1, &padd);
        memcpy(&pad[1].button, &padd, sizeof(PADState));

        // set RDST flag
        SISR |= SI_SR_RDST1;
        SICOMCSR |= SI_COMCSR_RDSTINT;
    }

#ifndef TWO_PADS_ONLY
    if(SIPOLL & SI_POLL_EN2)
    {
        // update pad input buffer
        PADReadButtons(2, &padd);
        memcpy(&pad[2].button, &padd, sizeof(PADState));

        // set RDST flag
        SISR |= SI_SR_RDST2;
        SICOMCSR |= SI_COMCSR_RDSTINT;
    }

    if(SIPOLL & SI_POLL_EN3)
    {
        // update pad input buffer
        PADReadButtons(3, &padd);
        memcpy(&pad[3].button, &padd, sizeof(PADState));

        // set RDST flag
        SISR |= SI_SR_RDST3;
        SICOMCSR |= SI_COMCSR_RDSTINT;
    }
#endif  // TWO_PADS_ONLY

    // generate RDST interrupt
    if(SICOMCSR & SI_COMCSR_RDSTINT)
    {
        // check RDST interrupt enable flag
        if(SICOMCSR & SI_COMCSR_RDSTINTMSK)
        {
            // assert processor interrupt
            PIAssertInt(PI_INTERRUPT_SI);
        }
    }

    EndProfilePAD();
#endif  // DISABLE_PADS
}

static void __fastcall write_poll(u32 addr, u32 data) { SIPOLL = data; }
static void __fastcall read_poll(u32 addr, u32 *reg)  { *reg = SIPOLL; }

//
// communication control/status 
//

static void __fastcall write_commcsr(u32 addr, u32 data)
{
    // clear incoming interrupt
    if(data & SI_COMCSR_TCINT)
    {
        SICOMCSR &= ~SI_COMCSR_TCINT;
        PIClearInt(PI_INTERRUPT_SI);
    }

    // set RDST interrupt mask (unused in libraries)
    if(data & SI_COMCSR_RDSTINTMSK) SICOMCSR |= SI_COMCSR_RDSTINTMSK;
    else SICOMCSR &= ~SI_COMCSR_RDSTINTMSK;

    // commands are executed immediately
    if(data & SI_COMCSR_TSTART)
    {
        // select channel
        int chan = SI_COMCSR_CHAN(data);

        // setup in/out length
        siinlen  = SI_COMCSR_INLEN(data);
        if(siinlen == 0) siinlen = 128;
        sioutlen = SI_COMCSR_OUTLEN(data);
        if(sioutlen == 0) sioutlen = 128;

        // make actual transfer
        SIReadBuffer(chan, SICOM);

        // complete transfer
        SICOMCSR &= ~SI_COMCSR_TSTART;

        // set completion interrupt
        SICOMCSR |= SI_COMCSR_TCINT;

        // generate cpu interrupt (if mask allows that)
        if(data & SI_COMCSR_TCINTMSK)
        {
            SICOMCSR |= SI_COMCSR_TCINTMSK;
            PIAssertInt(PI_INTERRUPT_SI);
        }
    }
}

static void __fastcall read_commcsr(u32 addr, u32 *reg)
{
    *reg = SICOMCSR;
}

//
// status register
//

static void __fastcall write_sisr(u32 addr, u32 data)
{
    // copy shadow command registers
    if(data & SI_SR_WR)
    {
        pad[0].command = cmdShadow[0];
        SISR &= ~SI_SR_WRST0;
        pad[1].command = cmdShadow[1];
        SISR &= ~SI_SR_WRST1;
#ifndef TWO_PADS_ONLY
        pad[2].command = cmdShadow[2];
        SISR &= ~SI_SR_WRST2;
        pad[3].command = cmdShadow[3];
        SISR &= ~SI_SR_WRST3;
#endif
    }
}

static void __fastcall read_sisr(u32 addr, u32 *reg)
{
    *reg = SISR;
}

// ---------------------------------------------------------------------------

// default stubs and developer notifications
// used only for debugging

// ignore register access
static void __fastcall si_no_read(u32 addr, u32 *reg)  {}
static void __fastcall si_no_write(u32 addr, u32 data) {}

// return SI register name (from patent)
static char *getSIRegName(u32 addr)
{
    switch(addr)
    {
        case 0xcc006400: return "SIC0OUTBUF";
        case 0xcc006404: return "SIC0INBUFH";
        case 0xcc006408: return "SIC0INBUFL";

        case 0xcc00640c: return "SIC1OUTBUF";
        case 0xcc006410: return "SIC1INBUFH";
        case 0xcc006414: return "SIC1INBUFL";

        case 0xcc006418: return "SIC2OUTBUF";
        case 0xcc00641c: return "SIC2INBUFH";
        case 0xcc006420: return "SIC2INBUFL";

        case 0xcc006424: return "SIC3OUTBUF";
        case 0xcc006428: return "SIC3INBUFH";
        case 0xcc00642c: return "SIC3INBUFL";

        case 0xcc006430: return "SIPOLL";
        case 0xcc006434: return "SICOMCSR";
        case 0xcc006438: return "SISR";
    }

    if((addr >= 0xcc006480) && (addr < 0xcc006500))
    {
        static char buf[16];
        sprintf(buf, "SICOM[%i]", addr & 0x7f);
        return buf;
    }

    return "SI unknown";
}

//
// report developer, about unhandled register access
//

static void __fastcall si_notify_read(u32 addr, u32 *reg)
{
    DolwinReport(
        "Some features of SI wasn't implemented !!\n"
        "read %s", getSIRegName(addr)
    );

    *reg = 0;
}

static void __fastcall si_notify_write(u32 addr, u32 data)
{
    DolwinReport(
        "Some features of SI wasn't implemented !!\n"
        "write %s = %08X", getSIRegName(addr), data
    );
}

// ---------------------------------------------------------------------------

// start SI subsystem

void SIOpen()
{
    // clear all registers
    SIPOLL = SICOMCSR = SISR = 0;
    memset(SICOM, 0, sizeof(SICOM));
    memset(pad, 0, sizeof(pad));

    // these values are actually written when IPL boots
    // meaning is unknown (some pad command) and no need to be known
    pad[0].command = 
    pad[1].command = 
    pad[2].command = 
    pad[3].command = 0x00400300;

    // enable polling (for homebrewn), IPL enabling it
    SIPOLL |= (SI_POLL_EN0 | SI_POLL_EN1);
#ifndef TWO_PADS_ONLY
    SIPOLL |= (SI_POLL_EN2 | SI_POLL_EN3);
#endif

    // update joypad data
    SIPoll();

    // joypads in/out command buffer
    HWSetTrap(32, 0x0c006400, si0_ReadOutBuf, si0_WriteOutBuf);
    HWSetTrap(32, 0x0c006404, si0_InBufLo, NULL);
    HWSetTrap(32, 0x0c006408, si0_InBufHi, NULL);

    HWSetTrap(32, 0x0c00640c, si1_ReadOutBuf, si1_WriteOutBuf);
    HWSetTrap(32, 0x0c006410, si1_InBufLo, NULL);
    HWSetTrap(32, 0x0c006414, si1_InBufHi, NULL);

#ifndef TWO_PADS_ONLY
    HWSetTrap(32, 0x0c006418, si2_ReadOutBuf, si2_WriteOutBuf);
    HWSetTrap(32, 0x0c00641c, si2_InBufLo, NULL);
    HWSetTrap(32, 0x0c006420, si2_InBufHi, NULL);

    HWSetTrap(32, 0x0c006424, si3_ReadOutBuf, si3_WriteOutBuf);
    HWSetTrap(32, 0x0c006428, si3_InBufLo, NULL);
    HWSetTrap(32, 0x0c00642c, si3_InBufHi, NULL);
#endif  // TWO_PADS_ONLY

    // control si registers
    HWSetTrap(32, 0x0c006430, read_poll, write_poll);           // si poll
    HWSetTrap(32, 0x0c006434, read_commcsr, write_commcsr);     // si comm
    HWSetTrap(32, 0x0c006438, read_sisr, write_sisr);           // si status

    // serial communcation buffer
    for(int ofs=0; ofs<128; ofs+=4)
    {
        HWSetTrap(32, 0x0c006480 | ofs, sicom_read, sicom_write);
    }
}
