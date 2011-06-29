// dvd (disk) interface
//
// disk interface registers has 0xCC006000 base
// cpu can access these registers through 32-bit or 16-bit reads and writes
// org : only 32-bit are implemented for now
//
// TODO : all register's stuff is ready. need only to complete DICommand().
//
// disk is rotated on constant speed (CAV) and data density is same for whole
// surface. DVD sectors are written vinil-like (from the edge to center).
// so inner sectors (higher sectors offsets) will be readen faster.
#include "dolphin.h"

//
// local data
//

// DI registers
static  u32     DISR, DICVR, DIMAR, DILEN, DICR;
static  DICMD   DICMDBUF[3], DIIMMBUF;

// other variables
static  BOOL    coverst;    // 1: cover open, 0: closed

// for streaming 
static  u32     strofs;
static  BOOL    stren;

// ---------------------------------------------------------------------------

// helper functions

// open cover
void DIOpenCover()
{
    if(coverst == TRUE) return;
    coverst = TRUE;

    if(emu.running == TRUE)
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

// close cover
void DICloseCover()
{
    if(coverst == FALSE) return;
    coverst = FALSE;

    if(emu.running == TRUE)
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
    return coverst;
}

// disk transfer complete interrupt
static void DIINT()
{
    DISR |= DI_SR_TCINT;
    if(DISR & DI_SR_TCINTMSK)
    {
        PIAssertInt(PI_INTERRUPT_DI);
    }
}

// execute DVD command packet
static void DICommand()
{
    u32 seek = DICMDBUF[1].word << 2;

    switch(DICMDBUF[0].word)
    {
        // data is too small to initiate di_read
        case 0x12000000:    // read manufacture info
            memset(&RAM[DIMAR & RAMMASK], 0, 0x20);
            DICR &= ~DI_CR_TSTART;
            DILEN = 0;
            DIINT();
            break;

        case 0xA8000040:    // disk id ?
        case 0xA8000000:    // read sector
            ASSERT(!(DICR & DI_CR_DMA), "Non-DMA disk transfer");
            ASSERT(DILEN & 0x1f, "Not aligned disk DMA transfer. Should be on 32-byte boundary!");

            DVDSeek(seek);
            DVDRead(&RAM[DIMAR & RAMMASK], DILEN);
            if(emu.doldebug)
            {
                DBReport(
                    DI "dma transfer (dimar:%08X, ofs:%08X, len:%i b)", 
                    DIMAR | (1<<31), 
                    seek,
                    DILEN
                );
            }
            
            DICR &= ~DI_CR_TSTART;
            DILEN = 0;
            DIINT();            
            break;

        case 0xE3000000:    // stop motor
            DICR &= ~DI_CR_TSTART;
            DIINT();
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
                DICMDBUF[0].word, PC,
                (DICR & DI_CR_RW) ? ("write") : ("read"),
                (DICR & DI_CR_DMA) ? ("yes") : ("no"),
                DIMAR,
                DILEN,
                DIIMMBUF.word
            );
            return;
    }
}

// ---------------------------------------------------------------------------

// status register
static void __fastcall write_disr(u32 addr, u32 data)
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
        DolwinReport("DVD break!");

        DISR &= ~DI_SR_BRK;
        DISR |= DI_SR_BRKINT;
        if(DISR & DI_SR_BRKINTMSK)
        {
            PIAssertInt(PI_INTERRUPT_DI);
        }
    }
}
static void __fastcall read_disr(u32 addr, u32 *reg) { *reg = DISR; }

// control register
static void __fastcall write_dicr(u32 addr, u32 data)
{
    DICR = data;

    // start command
    if(DICR & DI_CR_TSTART)
    {
        // execute
        DICommand();
    }
}
static void __fastcall read_dicr(u32 addr, u32 *reg) { *reg = DICR; }

// cover register
static void __fastcall write_cvr(u32 addr, u32 data)
{
    // clear cover interrupt
    if(data & DI_CVR_CVRINT) DICVR &= ~DI_CVR_CVRINT;

    // set mask
    if(data & DI_CVR_CVRINTMSK) DICVR |= DI_CVR_CVRINTMSK;
    else DICVR &= ~DI_CVR_CVRINTMSK;
}
static void __fastcall read_cvr(u32 addr, u32 *reg) { *reg = DICVR; }

// dma registers
static void __fastcall read_dimar(u32 addr, u32 *reg)  { *reg = DIMAR; }
static void __fastcall write_dimar(u32 addr, u32 data)
{
    DIMAR = data;
}
static void __fastcall read_dilen(u32 addr, u32 *reg)  { *reg = DILEN; }
static void __fastcall write_dilen(u32 addr, u32 data)
{
    DILEN = data;
}

// di buffers
static void __fastcall read_cmdbuf0(u32 addr, u32 *reg)  { *reg = DICMDBUF[0].word; }
static void __fastcall write_cmdbuf0(u32 addr, u32 data) { DICMDBUF[0].word = data; }
static void __fastcall read_cmdbuf1(u32 addr, u32 *reg)  { *reg = DICMDBUF[1].word; }
static void __fastcall write_cmdbuf1(u32 addr, u32 data) { DICMDBUF[1].word = data; }
static void __fastcall read_cmdbuf2(u32 addr, u32 *reg)  { *reg = DICMDBUF[2].word; }
static void __fastcall write_cmdbuf2(u32 addr, u32 data) { DICMDBUF[2].word = data; }
static void __fastcall read_diimmbuf(u32 addr, u32 *reg) { *reg = DIIMMBUF.word; }
static void __fastcall write_diimmbuf(u32 addr, u32 data){ DIIMMBUF.word = data; }

// register is read only.
// currently, bit 0 is used for ROM scramble disable, bits 1-7 are reserved
// used in EXISync->__OSGetDIConfig call, return 0 and forget.
static void __fastcall get_di_config(u32 addr, u32 *reg) { *reg = 0; }

// ---------------------------------------------------------------------------

// debug notifications

// return DI register name (from patent)
static char *getDIRegName(u32 addr)
{
    switch(addr)
    {
        case 0xcc006000: return "DISR";
        case 0xcc006004: return "DICVR";
        case 0xcc006008: return "DICMDBUF0";
        case 0xcc00600c: return "DICMDBUF1";
        case 0xcc006010: return "DICMDBUF2";
        case 0xcc006014: return "DIMAR";
        case 0xcc006018: return "DILEN";
        case 0xcc00601c: return "DICR";
        case 0xcc006020: return "DIIMMBUF";
        case 0xcc006024: return "DICFG";
    }
    return "???";
}

//
// report developer, about unhandled register access
//

static void __fastcall di_notify_read(u32 addr, u32 *reg)
{
    DolwinReport(
        "Some features of DI wasn't implemented !!\n"
        "read %s", getDIRegName(addr)
    );

    *reg = 0;
}

static void __fastcall di_notify_write(u32 addr, u32 data)
{
    DolwinReport(
        "Some features of DI wasn't implemented !!\n"
        "write %s = %08X", getDIRegName(addr), data
    );
}

// ---------------------------------------------------------------------------

// start DVD subsystem

void DIOpen()
{
    // clear registers
    DISR = DICVR = DIMAR = DILEN = DICR = 0;

    // close cover
    coverst = FALSE;

    // for streaming
    strofs = 0;
    stren = FALSE;

    // set register hooks
    HWSetTrap(32, DI_SR     , read_disr    , write_disr);
    HWSetTrap(32, DI_CVR    , read_cvr     , write_cvr);
    HWSetTrap(32, DI_CMDBUF0, read_cmdbuf0 , write_cmdbuf0);
    HWSetTrap(32, DI_CMDBUF1, read_cmdbuf1 , write_cmdbuf1);
    HWSetTrap(32, DI_CMDBUF2, read_cmdbuf2 , write_cmdbuf2);
    HWSetTrap(32, DI_MAR    , read_dimar   , write_dimar);
    HWSetTrap(32, DI_LEN    , read_dilen   , write_dilen);
    HWSetTrap(32, DI_CR     , read_dicr    , write_dicr);
    HWSetTrap(32, DI_IMMBUF , read_diimmbuf, write_diimmbuf);
    HWSetTrap(32, DI_CFG    , get_di_config, NULL);
}
