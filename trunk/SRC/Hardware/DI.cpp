// DI - disk interface. both 16/32-registers are implemeted.
// fixed 0.09 stop motor command (TSTART wasnt cleared and no DIINT)
#include "dolphin.h"

// DI state (registers and other data)
DIControl di;

// ---------------------------------------------------------------------------
// cover control

void DIOpenCover()
{
    if(di.coverst == TRUE) return;
    di.coverst = TRUE;
    DBReport(DI "cover opened\n");

    if(emu.running)
    {
        DICVR |= DI_CVR_CVR;    // set cover flag
        
        // cover interrupt
        DICVR |= DI_CVR_CVRINT;
        if(DICVR & DI_CVR_CVRINTMSK)
        {
            PIAssertInt(PI_INTERRUPT_DI);
        }
    }
}

void DICloseCover()
{
    if(di.coverst == FALSE) return;
    di.coverst = FALSE;
    DBReport(DI "cover closed\n");

    if(emu.running)
    {
        DICVR &= ~DI_CVR_CVR;   // clear cover flag

        // cover interrupt
        DICVR |= DI_CVR_CVRINT;
        if(DICVR & DI_CVR_CVRINTMSK)
        {
            PIAssertInt(PI_INTERRUPT_DI);
        }
    }
}

// 1: cover open, 0: closed
BOOL DIGetCoverState()
{
    return di.coverst;
}

// ---------------------------------------------------------------------------
// dispatch command

// disk transfer complete interrupt
static void DIINT()
{
    DISR |= DI_SR_TCINT;
    if(DISR & DI_SR_TCINTMSK)
    {
        PIAssertInt(PI_INTERRUPT_DI);
    }
}

// execute DVD command
static void DICommand()
{
    u32 seek = di.cmdbuf[1] << 2;

    switch(di.cmdbuf[0] >> 24)
    {
        case 0x12:          // read manufacture info (DMA)
            memset(&RAM[DIMAR & RAMMASK], 0, 0x20);
            DILEN = 0;
            break;

        case 0xA8:          // read sector (disk id) (DMA)
            ASSERT(!(DICR & DI_CR_DMA), "Non-DMA disk transfer");
            ASSERT(DILEN & 0x1f, "Not aligned disk DMA transfer. Should be on 32-byte boundary!");

            BeginProfileDVD();
            DVDSeek(seek);
            DVDRead(&RAM[DIMAR & RAMMASK], DILEN);
            EndProfileDVD();

            DBReport( DI "dma transfer (dimar:%08X, ofs:%08X, len:%i b)\n", 
                      DIMAR, seek, DILEN );
            DILEN = 0;
            break;

        case 0xAB:          // seek
            break;

        case 0xE3:          // stop motor (IMM)
            // stop motor is experimental thing and its not used in regular SDK DVD library
            // should we disable streaming ?
            //di.streaming = 0;
            break;

        case 0xE1:          // set stream (IMM).
            if(di.cmdbuf[1] << 2)
            {
                di.strseek  = di.cmdbuf[1] << 2;
                di.strcount = di.cmdbuf[2];

                di.strseekOld  = di.strseek;
                di.strcountOld = di.strcount;

                DBReport( DI "DVD Streaming!! DVD positioned on track, starting %08X, %i bytes long.\n",
                          di.strseek, di.strcount );

                // stream playback is enabled automatically
                di.streaming = 1;
                DIStreamUpdate();
            }
/*/
            else
            {
                // disable stream playback.
                // I dont know why, but DVD library ignoring 0xE4 stream control command :(
                di.streaming = 0;
                AXPlayStream(0, 0);
            }
/*/
            break;

        case 0xE2:          // get stream status (IMM)
            di.immbuf = di.streaming;
            break;

        case 0xE4:          // stream control (IMM)
            di.streaming = (di.cmdbuf[0] >> 16) & 1;
            DBReport( "DVD Streaming!! Play(?) : %s\n",
                      di.streaming ? "On" : "Off" );
            if(di.streaming == 0)
            {
                AXPlayStream(0, 0);
            }
            break;

        // unknown command
        default:
            DolwinReport(
                "Unknown DVD command : %08X (pc:%08X)\n\n"
                "type:%s\n"
                "dma:%s\n"
                "madr:%08X\n"
                "dlen:%08X\n"
                "imm:%08X\n",
                di.cmdbuf[0], PC,
                (DICR & DI_CR_RW) ? ("write") : ("read"),
                (DICR & DI_CR_DMA) ? ("yes") : ("no"),
                DIMAR,
                DILEN,
                di.immbuf
            );
            return;
    }

    DICR &= ~DI_CR_TSTART;
    DIINT();
}

// ---------------------------------------------------------------------------
// 32-bit register traps

// status register
static void __fastcall write_sr(u32 addr, u32 data)
{
    // set masks
    if(data & DI_SR_BRKINTMSK) DISR |= DI_SR_BRKINTMSK;
    else DISR &= ~DI_SR_BRKINTMSK;
    if(data & DI_SR_TCINTMSK)  DISR |= DI_SR_TCINTMSK;
    else DISR &= ~DI_SR_TCINTMSK;

    // clear interrupts
    if(data & DI_SR_BRKINT)
    {
        DISR &= ~DI_SR_BRKINT;
        PIClearInt(PI_INTERRUPT_DI);
    }
    if(data & DI_SR_TCINT)
    {
        DISR &= ~DI_SR_TCINT;
        PIClearInt(PI_INTERRUPT_DI);
    }

    // assert break
    if(data & DI_SR_BRK)
    {
        DISR &= ~DI_SR_BRK;
        DISR |= DI_SR_BRKINT;
        if(DISR & DI_SR_BRKINTMSK)
        {
            PIAssertInt(PI_INTERRUPT_DI);
        }
    }
}
static void __fastcall read_sr(u32 addr, u32 *reg) { *reg = (u16)DISR; }

// control register
static void __fastcall write_cr(u32 addr, u32 data)
{
    DICR = (u16)data;

    // start command
    if(DICR & DI_CR_TSTART)
    {
        // execute
        DICommand();
    }
}
static void __fastcall read_cr(u32 addr, u32 *reg) { *reg = (u16)DICR; }

// cover register
static void __fastcall write_cvr(u32 addr, u32 data)
{
    // clear cover interrupt
    if(data & DI_CVR_CVRINT)
    {
        DICVR &= ~DI_CVR_CVRINT;
        PIClearInt(PI_INTERRUPT_DI);
    }

    // set mask
    if(data & DI_CVR_CVRINTMSK) DICVR |= DI_CVR_CVRINTMSK;
    else DICVR &= ~DI_CVR_CVRINTMSK;
}
static void __fastcall read_cvr(u32 addr, u32 *reg) { *reg = (u16)DICVR; }

// dma registers
static void __fastcall read_mar(u32 addr, u32 *reg)  { *reg = DIMAR; }
static void __fastcall write_mar(u32 addr, u32 data) { DIMAR = data; }
static void __fastcall read_len(u32 addr, u32 *reg)  { *reg = DILEN; }
static void __fastcall write_len(u32 addr, u32 data) { DILEN = data; }

// di buffers
static void __fastcall read_cmdbuf0(u32 addr, u32 *reg)  { *reg = di.cmdbuf[0]; }
static void __fastcall write_cmdbuf0(u32 addr, u32 data) { di.cmdbuf[0] = data; }
static void __fastcall read_cmdbuf1(u32 addr, u32 *reg)  { *reg = di.cmdbuf[1]; }
static void __fastcall write_cmdbuf1(u32 addr, u32 data) { di.cmdbuf[1] = data; }
static void __fastcall read_cmdbuf2(u32 addr, u32 *reg)  { *reg = di.cmdbuf[2]; }
static void __fastcall write_cmdbuf2(u32 addr, u32 data) { di.cmdbuf[2] = data; }
static void __fastcall read_immbuf(u32 addr, u32 *reg)   { *reg = di.immbuf; }
static void __fastcall write_immbuf(u32 addr, u32 data)  { di.immbuf = data; }

// register is read only.
// currently, bit 0 is used for ROM scramble disable, bits 1-7 are reserved
// used in EXISync->__OSGetDIConfig call, return 0 and forget.
static void __fastcall read_cfg(u32 addr, u32 *reg) { *reg = 0; }

// ---------------------------------------------------------------------------
// 16-bit register traps (simply wraps to 32-bit regs)

// stubs for high parts of SR, CR and CVR regs
static void __fastcall no_write(u32 addr, u32 data) {}
static void __fastcall no_read(u32 addr, u32 *reg)  { *reg = 0; }

// access by high or low part
static void __fastcall read_mar_h(u32 addr, u32 *reg)  { *reg = DIMAR >> 16; }
static void __fastcall read_mar_l(u32 addr, u32 *reg)  { *reg = (u16)DIMAR; }
static void __fastcall write_mar_h(u32 addr, u32 data) { DIMAR = (DIMAR & 0x0000ffff) | (data << 16); }
static void __fastcall write_mar_l(u32 addr, u32 data) { DIMAR = (DIMAR & 0xffff0000) | ((u16)data); }
static void __fastcall read_len_h(u32 addr, u32 *reg)  { *reg = DILEN >> 16; }
static void __fastcall read_len_l(u32 addr, u32 *reg)  { *reg = (u16)DILEN; }
static void __fastcall write_len_h(u32 addr, u32 data) { DILEN = (DILEN & 0x0000ffff) | (data << 16); }
static void __fastcall write_len_l(u32 addr, u32 data) { DILEN = (DILEN & 0xffff0000) | ((u16)data); }

// access by high or low part
static void __fastcall read_cmdbuf0_h(u32 addr, u32 *reg)  { *reg = di.cmdbuf[0] >> 16; }
static void __fastcall read_cmdbuf0_l(u32 addr, u32 *reg)  { *reg = (u16)di.cmdbuf[0]; }
static void __fastcall write_cmdbuf0_h(u32 addr, u32 data) { di.cmdbuf[0] = (di.cmdbuf[0] & 0x0000ffff) | (data << 16); }
static void __fastcall write_cmdbuf0_l(u32 addr, u32 data) { di.cmdbuf[0] = (di.cmdbuf[0] & 0xffff0000) | ((u16)data); }
static void __fastcall read_cmdbuf1_h(u32 addr, u32 *reg)  { *reg = di.cmdbuf[1] >> 16; }
static void __fastcall read_cmdbuf1_l(u32 addr, u32 *reg)  { *reg = (u16)di.cmdbuf[1]; }
static void __fastcall write_cmdbuf1_h(u32 addr, u32 data) { di.cmdbuf[1] = (di.cmdbuf[1] & 0x0000ffff) | (data << 16); }
static void __fastcall write_cmdbuf1_l(u32 addr, u32 data) { di.cmdbuf[1] = (di.cmdbuf[1] & 0xffff0000) | ((u16)data); }
static void __fastcall read_cmdbuf2_h(u32 addr, u32 *reg)  { *reg = di.cmdbuf[2] >> 16; }
static void __fastcall read_cmdbuf2_l(u32 addr, u32 *reg)  { *reg = (u16)di.cmdbuf[2]; }
static void __fastcall write_cmdbuf2_h(u32 addr, u32 data) { di.cmdbuf[2] = (di.cmdbuf[2] & 0x0000ffff) | (data << 16); }
static void __fastcall write_cmdbuf2_l(u32 addr, u32 data) { di.cmdbuf[2] = (di.cmdbuf[2] & 0xffff0000) | ((u16)data); }
static void __fastcall read_immbuf_h(u32 addr, u32 *reg)   { *reg = di.immbuf >> 16; }
static void __fastcall read_immbuf_l(u32 addr, u32 *reg)   { *reg = (u16)di.immbuf; }
static void __fastcall write_immbuf_h(u32 addr, u32 data)  { di.immbuf = (di.immbuf & 0x0000ffff) | (data << 16); }
static void __fastcall write_immbuf_l(u32 addr, u32 data)  { di.immbuf = (di.immbuf & 0xffff0000) | ((u16)data); }

// ---------------------------------------------------------------------------
// step DVD stream buffer

void DIStreamUpdate()
{
    if(di.streaming)
    {
        // load new data
        s32 count = (di.strcount < DVD_STREAM_BLK) ?
                    di.strcount : 
                    DVD_STREAM_BLK ;
        BeginProfileDVD();
        DVDSeek(di.strseek);
        DVDRead(di.workArea, count);
        EndProfileDVD();
        
        BeginProfileSfx();
        AXPlayStream(di.workArea, count);
        EndProfileSfx();

        // step forward
        di.strseek += count;
        di.strcount -= count;

        // reset playback
        if(di.strcount <= 0)
        {
            di.strseekOld  = di.strseek;
            di.strcountOld = di.strcount;
        }

        // *** update stream sample counter ***
        if(ai.cr & AICR_PSTAT)
        {
            ai.scnt += count / 32 - 4;
            if(ai.scnt >= ai.it)
            {
                AISINT();
            }
        }
    }
}

// ---------------------------------------------------------------------------
// init

void DIOpen()
{
    DBReport(CYAN "DI: DVD interface hardware\n");

    // current DVD is set by Loader, or when disk is swapped by UserWindow.

    // clear registers and close cover
    memset(&di, 0, sizeof(DIControl));

    AXPlayStream(0, 0);

    ASSERT(di.workArea, "Dirty DVD streaming workarea");
    di.workArea = (u8 *)malloc(DVD_STREAM_BLK + 1024);
    ASSERT(di.workArea == NULL, "No space for DVD streaming workarea");
    memset(di.workArea, 0, DVD_STREAM_BLK);

    // set 32-bit register traps
    HWSetTrap(32, DI_SR     , read_sr      , write_sr);
    HWSetTrap(32, DI_CVR    , read_cvr     , write_cvr);
    HWSetTrap(32, DI_CMDBUF0, read_cmdbuf0 , write_cmdbuf0);
    HWSetTrap(32, DI_CMDBUF1, read_cmdbuf1 , write_cmdbuf1);
    HWSetTrap(32, DI_CMDBUF2, read_cmdbuf2 , write_cmdbuf2);
    HWSetTrap(32, DI_MAR    , read_mar     , write_mar);
    HWSetTrap(32, DI_LEN    , read_len     , write_len);
    HWSetTrap(32, DI_CR     , read_cr      , write_cr);
    HWSetTrap(32, DI_IMMBUF , read_immbuf  , write_immbuf);
    HWSetTrap(32, DI_CFG    , read_cfg     , NULL);

    // set 16-bit register traps (unused in SDK, but may be used in other apps)
    HWSetTrap(16, DI_SR_H     , no_read        , no_write);
    HWSetTrap(16, DI_SR_L     , read_sr        , write_sr);
    HWSetTrap(16, DI_CVR_H    , no_read        , no_write);
    HWSetTrap(16, DI_CVR_L    , read_cvr       , write_cvr);
    HWSetTrap(16, DI_CMDBUF0_H, read_cmdbuf0_h , write_cmdbuf0_h);
    HWSetTrap(16, DI_CMDBUF0_L, read_cmdbuf0_l , write_cmdbuf0_l);
    HWSetTrap(16, DI_CMDBUF1_H, read_cmdbuf1_h , write_cmdbuf1_h);
    HWSetTrap(16, DI_CMDBUF1_L, read_cmdbuf1_l , write_cmdbuf1_l);
    HWSetTrap(16, DI_CMDBUF2_H, read_cmdbuf2_h , write_cmdbuf2_h);
    HWSetTrap(16, DI_CMDBUF2_L, read_cmdbuf2_l , write_cmdbuf2_l);
    HWSetTrap(16, DI_MAR_H    , read_mar_h     , write_mar_h);
    HWSetTrap(16, DI_MAR_L    , read_mar_l     , write_mar_l);
    HWSetTrap(16, DI_LEN_H    , read_len_h     , write_len_h);
    HWSetTrap(16, DI_LEN_L    , read_len_l     , write_len_l);
    HWSetTrap(16, DI_CR_H     , no_read        , no_write);
    HWSetTrap(16, DI_CR_L     , read_cr        , write_cr);
    HWSetTrap(16, DI_IMMBUF_H , read_immbuf_h  , write_immbuf_h);
    HWSetTrap(16, DI_IMMBUF_L , read_immbuf_l  , write_immbuf_l);
    HWSetTrap(16, DI_CFG_H    , read_cfg       , NULL);
    HWSetTrap(16, DI_CFG_L    , read_cfg       , NULL);
}

void DIClose()
{
    if(di.workArea)
    {
        free(di.workArea);
        di.workArea = NULL;
    }
}
