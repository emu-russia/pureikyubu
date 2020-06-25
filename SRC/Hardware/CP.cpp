// CP - command processor, PE - pixel engine.
#include "pch.h"

FifoControl fifo;

// ---------------------------------------------------------------------------
// fifo

// TODO: Make a GP update when copying the frame buffer by Pixel Engine.

static void DONE_INT()
{
    fifo.done_num++; vi.frames++;
    if(fifo.done_num == 1)
    {
        SetStatusText(STATUS_ENUM::Progress, _T("First GX access"), true);
        vi.xfb = 0;     // disable VI output
    }
    if (fifo.log)
    {
        DBReport2(DbgChannel::PE, "PE_DONE (frame:%u)", fifo.done_num);
    }

    if(fifo.pe.sr & PE_SR_DONEMSK)
    {
        fifo.pe.sr |= PE_SR_DONE;
        PIAssertInt(PI_INTERRUPT_PE_FINISH);
    }
}

static void TOKEN_INT()
{
    vi.frames++;
    if (fifo.log)
    {
        DBReport2(DbgChannel::PE, "PE_TOKEN (%04X)", fifo.pe.token);
    }
    vi.xfb = 0;     // disable VI output

    if(fifo.pe.sr & PE_SR_TOKENMSK)
    {
        fifo.pe.sr |= PE_SR_TOKEN;
        PIAssertInt(PI_INTERRUPT_PE_TOKEN);
    }
}

static void CP_BREAK()
{
    if (fifo.cp.cr & CP_CR_BPINTEN && (fifo.cp.sr & CP_SR_BPINT) == 0)
    {
        fifo.cp.sr |= CP_SR_BPINT;
        PIAssertInt(PI_INTERRUPT_CP);
        DBReport2(DbgChannel::CP, "BREAK");
    }
}

static void CP_OVF()
{
    if (fifo.cp.cr & CP_CR_OVFEN && (fifo.cp.sr & CP_SR_OVF) == 0)
    {
        fifo.cp.sr |= CP_SR_OVF;
        PIAssertInt(PI_INTERRUPT_CP);
        DBReport2(DbgChannel::CP, "OVF");
    }
}

static void CP_UVF()
{
    if (fifo.cp.cr & CP_CR_UVFEN && (fifo.cp.sr & CP_SR_UVF) == 0)
    {
        fifo.cp.sr |= CP_SR_UVF;
        PIAssertInt(PI_INTERRUPT_CP);
        DBReport2(DbgChannel::CP, "UVF");
    }
}

static void CPDrawDoneCallback()
{
    DONE_INT();
}

static void CPDrawTokenCallback(uint16_t tokenValue)
{
    fifo.pe.token = tokenValue;
    TOKEN_INT();
}

static void CPThread(void* Param)
{
    while (true)
    {
        int64_t ticks = Gekko::Gekko->GetTicks();
        if (ticks < fifo.updateTbrValue)
        {
            continue;
        }
        fifo.updateTbrValue = ticks + fifo.tickPerFifo;

        // Calculate count
        if (fifo.cp.wrptr >= fifo.cp.rdptr)
        {
            fifo.cp.cnt = fifo.cp.wrptr - fifo.cp.rdptr;
        }
        else
        {
            fifo.cp.cnt = (fifo.cp.top - fifo.cp.rdptr) + (fifo.cp.wrptr - fifo.cp.base);
        }

        // Watermarks logic. Active only in linked-mode.
        if (fifo.cp.cnt > fifo.cp.himark && (fifo.cp.cr & CP_CR_WPINC))
        {
            CP_OVF();
        }
        if (fifo.cp.cnt < fifo.cp.lomark && (fifo.cp.cr & CP_CR_WPINC))
        {
            CP_UVF();
        }

        // Breakpoint
        if ((fifo.cp.rdptr & ~0x1f) == (fifo.cp.bpptr & ~0x1f) && (fifo.cp.cr & CP_CR_BPEN))
        {
            CP_BREAK();
        }

        // Advance read pointer.
        if (fifo.cp.cnt != 0 && fifo.cp.cr & CP_CR_RDEN && (fifo.cp.sr & (CP_SR_OVF | CP_SR_UVF | CP_SR_BPINT)) == 0)
        {
            fifo.cp.sr &= ~CP_SR_RD_IDLE;

            fifo.cp.sr &= ~CP_SR_CMD_IDLE;
            BeginProfileGfx();
            GXWriteFifo(&mi.ram[fifo.cp.rdptr & RAMMASK]);
            EndProfileGfx();
            fifo.cp.sr |= CP_SR_CMD_IDLE;

            fifo.cp.rdptr += 32;
            if (fifo.cp.rdptr == fifo.cp.top)
            {
                fifo.cp.rdptr = fifo.cp.base;
            }
        }
        else
        {
            fifo.cp.sr |= (CP_SR_RD_IDLE | CP_SR_CMD_IDLE);
        }
    }
}

// ---------------------------------------------------------------------------
// registers

//
// pixel engine status register (0x100a)
//

static void write_pe_sr(uint32_t addr, uint32_t data)
{
    // clear interrupts
    if(fifo.pe.sr & PE_SR_DONE)
    {
        fifo.pe.sr &= ~PE_SR_DONE;
        PIClearInt(PI_INTERRUPT_PE_FINISH);
    }
    if(fifo.pe.sr & PE_SR_TOKEN)
    {
        fifo.pe.sr &= ~PE_SR_TOKEN;
        PIClearInt(PI_INTERRUPT_PE_TOKEN);
    }

    // set mask bits
    if(data & PE_SR_DONEMSK) fifo.pe.sr |= PE_SR_DONEMSK;
    else fifo.pe.sr &= ~PE_SR_DONEMSK;
    if(data & PE_SR_TOKENMSK) fifo.pe.sr |= PE_SR_TOKENMSK;
    else fifo.pe.sr &= ~PE_SR_TOKENMSK;
}
static void read_pe_sr(uint32_t addr, uint32_t *reg)  { *reg = fifo.pe.sr; }

static void read_pe_token(uint32_t addr, uint32_t *reg) { *reg = fifo.pe.token; }

//
// command processor
//

// control and status registers

static void read_cp_sr(uint32_t addr, uint32_t *reg)
{
    *reg = fifo.cp.sr;
}

static void write_cp_cr(uint32_t addr, uint32_t data)
{
    fifo.cp.cr = (uint16_t)data;

    // clear breakpoint
    if((data & CP_CR_BPINTEN) == 0)
    {
        fifo.cp.sr &= ~CP_SR_BPINT;
    }

    if ((fifo.cp.sr & CP_SR_BPINT) == 0 && (fifo.cp.sr & CP_SR_OVF) == 0 && (fifo.cp.sr & CP_SR_UVF) == 0)
    {
        PIClearInt(PI_INTERRUPT_CP);
    }
}
static void read_cp_cr(uint32_t addr, uint32_t *reg) { *reg = fifo.cp.cr; }

static void write_cp_clr(uint32_t addr, uint32_t data)
{
    // clear watermark conditions
    if(data & CP_CLR_OVFCLR)
    {
        fifo.cp.sr &= ~CP_SR_OVF;
    }
    if(data & CP_CLR_UVFCLR)
    {
        fifo.cp.sr &= ~CP_SR_UVF;
    }

    if ((fifo.cp.sr & CP_SR_BPINT) == 0 && (fifo.cp.sr & CP_SR_OVF) == 0 && (fifo.cp.sr & CP_SR_UVF) == 0)
    {
        PIClearInt(PI_INTERRUPT_CP);
    }
}

// pointers

// show CP fifo configuration
void DumpCPFIFO()
{
    // fifo modes
    char*md = (fifo.cp.cr & CP_CR_WPINC) ? ((char *)"immediate ") : ((char *)"multi-");
    char bp = (fifo.cp.cr & CP_CR_BPEN) ? ('B') : ('b');    // breakpoint
    char lw = (fifo.cp.cr & CP_CR_UVFEN)? ('U') : ('u');    // low-wmark
    char hw = (fifo.cp.cr & CP_CR_OVFEN)? ('O') : ('o');    // high-wmark

    DBReport("CP %sfifo configuration:%c%c%c", md, bp, lw, hw);
    DBReport("control :0x%08X", fifo.cp.cr);
    DBReport(" status :0x%08X", fifo.cp.sr);
    DBReport("   base :0x%08X", fifo.cp.base);
    DBReport("   top  :0x%08X", fifo.cp.top);
    DBReport("   low  :0x%08X", fifo.cp.lomark);
    DBReport("   high :0x%08X", fifo.cp.himark);
    DBReport("   cnt  :0x%08X", fifo.cp.cnt);
    DBReport("   wrptr:0x%08X", fifo.cp.wrptr);
    DBReport("   rdptr:0x%08X", fifo.cp.rdptr);
    DBReport("   break:0x%08X", fifo.cp.bpptr);
}

static void read_cp_baseh(uint32_t addr, uint32_t *reg)    { *reg = fifo.cp.baseh; }
static void write_cp_baseh(uint32_t addr, uint32_t data)   { fifo.cp.baseh = data; }
static void read_cp_basel(uint32_t addr, uint32_t *reg)    { *reg = fifo.cp.basel & 0xffe0; }
static void write_cp_basel(uint32_t addr, uint32_t data)   { fifo.cp.basel = data & 0xffe0; }
static void read_cp_toph(uint32_t addr, uint32_t *reg)     { *reg = fifo.cp.toph; }
static void write_cp_toph(uint32_t addr, uint32_t data)    { fifo.cp.toph = data; }
static void read_cp_topl(uint32_t addr, uint32_t *reg)     { *reg = fifo.cp.topl & 0xffe0; }
static void write_cp_topl(uint32_t addr, uint32_t data)    { fifo.cp.topl = data & 0xffe0; }

static void read_cp_hmarkh(uint32_t addr, uint32_t *reg)   { *reg = fifo.cp.himarkh; }
static void write_cp_hmarkh(uint32_t addr, uint32_t data)  { fifo.cp.himarkh = data; }
static void read_cp_hmarkl(uint32_t addr, uint32_t *reg)   { *reg = fifo.cp.himarkl & 0xffe0; }
static void write_cp_hmarkl(uint32_t addr, uint32_t data)  { fifo.cp.himarkl = data & 0xffe0; }
static void read_cp_lmarkh(uint32_t addr, uint32_t *reg)   { *reg = fifo.cp.lomarkh; }
static void write_cp_lmarkh(uint32_t addr, uint32_t data)  { fifo.cp.lomarkh = data; }
static void read_cp_lmarkl(uint32_t addr, uint32_t *reg)   { *reg = fifo.cp.lomarkl & 0xffe0; }
static void write_cp_lmarkl(uint32_t addr, uint32_t data)  { fifo.cp.lomarkl = data & 0xffe0; }

static void read_cp_cnth(uint32_t addr, uint32_t *reg)     { *reg = fifo.cp.cnth; }
static void write_cp_cnth(uint32_t addr, uint32_t data)    { fifo.cp.cnth = data; }
static void read_cp_cntl(uint32_t addr, uint32_t *reg)     { *reg = fifo.cp.cntl & 0xffe0; }
static void write_cp_cntl(uint32_t addr, uint32_t data)    { fifo.cp.cntl = data & 0xffe0; }

static void read_cp_wrptrh(uint32_t addr, uint32_t *reg)   { *reg = fifo.cp.wrptrh; }
static void write_cp_wrptrh(uint32_t addr, uint32_t data)  { fifo.cp.wrptrh = data; }
static void read_cp_wrptrl(uint32_t addr, uint32_t *reg)   { *reg = fifo.cp.wrptrl & 0xffe0; }
static void write_cp_wrptrl(uint32_t addr, uint32_t data)  { fifo.cp.wrptrl = data & 0xffe0; }
static void read_cp_rdptrh(uint32_t addr, uint32_t *reg)   { *reg = fifo.cp.rdptrh; }
static void write_cp_rdptrh(uint32_t addr, uint32_t data)  { fifo.cp.rdptrh = data; }
static void read_cp_rdptrl(uint32_t addr, uint32_t *reg)   { *reg = fifo.cp.rdptrl & 0xffe0; }
static void write_cp_rdptrl(uint32_t addr, uint32_t data)  { fifo.cp.rdptrl = data & 0xffe0; }

static void read_cp_bpptrh(uint32_t addr, uint32_t *reg)   { *reg = fifo.cp.bpptrh; }
static void write_cp_bpptrh(uint32_t addr, uint32_t data)  { fifo.cp.bpptrh = data; }
static void read_cp_bpptrl(uint32_t addr, uint32_t *reg)   { *reg = fifo.cp.bpptrl & 0xffe0; }
static void write_cp_bpptrl(uint32_t addr, uint32_t data)  { fifo.cp.bpptrl = data & 0xffe0; }

//
// stubs
//

static void no_write(uint32_t addr, uint32_t data) {}
static void no_read(uint32_t addr, uint32_t *reg)  { *reg = 0; }

// ---------------------------------------------------------------------------
// init

void CPOpen(HWConfig * config)
{
    DBReport2(DbgChannel::CP, "Command processor (for GX)\n");

    // clear registers
    memset(&fifo, 0, sizeof(FifoControl));

    fifo.log = false;

    // command processor
    MISetTrap(16, CP_SR         , read_cp_sr, NULL);
    MISetTrap(16, CP_CR         , read_cp_cr, write_cp_cr);
    MISetTrap(16, CP_CLR        , NULL, write_cp_clr);
    MISetTrap(16, CP_TEX        , no_read, no_write);
    MISetTrap(16, CP_BASE       , read_cp_basel, write_cp_basel);
    MISetTrap(16, CP_BASE + 2   , read_cp_baseh, write_cp_baseh);
    MISetTrap(16, CP_TOP        , read_cp_topl, write_cp_topl);
    MISetTrap(16, CP_TOP + 2    , read_cp_toph, write_cp_toph);
    MISetTrap(16, CP_HIWMARK    , read_cp_hmarkl, write_cp_hmarkl);
    MISetTrap(16, CP_HIWMARK + 2, read_cp_hmarkh, write_cp_hmarkh);
    MISetTrap(16, CP_LOWMARK    , read_cp_lmarkl, write_cp_lmarkl);
    MISetTrap(16, CP_LOWMARK + 2, read_cp_lmarkh, write_cp_lmarkh);
    MISetTrap(16, CP_CNT        , read_cp_cntl, write_cp_cntl);
    MISetTrap(16, CP_CNT + 2    , read_cp_cnth, write_cp_cnth);
    MISetTrap(16, CP_WRPTR      , read_cp_wrptrl, write_cp_wrptrl);
    MISetTrap(16, CP_WRPTR + 2  , read_cp_wrptrh, write_cp_wrptrh);
    MISetTrap(16, CP_RDPTR      , read_cp_rdptrl, write_cp_rdptrl);
    MISetTrap(16, CP_RDPTR + 2  , read_cp_rdptrh, write_cp_rdptrh);
    MISetTrap(16, CP_BPPTR      , read_cp_bpptrl, write_cp_bpptrl);
    MISetTrap(16, CP_BPPTR + 2  , read_cp_bpptrh, write_cp_bpptrh);
    
    // I-Ninja. TODO: Research
    MISetTrap(16, 0x0c00'0040, no_read, nullptr);
    MISetTrap(16, 0x0c00'0042, no_read, nullptr);
    MISetTrap(16, 0x0c00'0044, no_read, nullptr);
    MISetTrap(16, 0x0c00'0046, no_read, nullptr);
    MISetTrap(16, 0x0c00'0048, no_read, nullptr);
    MISetTrap(16, 0x0c00'004a, no_read, nullptr);
    MISetTrap(16, 0x0c00'004c, no_read, nullptr);
    MISetTrap(16, 0x0c00'004e, no_read, nullptr);

    // pixel engine
    MISetTrap(16, PE_ZCR       , no_read, no_write);
    MISetTrap(16, PE_ACR       , no_read, no_write);
    MISetTrap(16, PE_ALPHA_DST , no_read, no_write);
    MISetTrap(16, PE_ALPHA_MODE, no_read, no_write);
    MISetTrap(16, PE_ALPHA_READ, no_read, no_write);
    MISetTrap(16, PE_SR        , read_pe_sr, write_pe_sr);
    MISetTrap(16, PE_TOKEN     , read_pe_token, NULL);

    GXSetDrawCallbacks(CPDrawDoneCallback, CPDrawTokenCallback);

    fifo.tickPerFifo = 100;
    fifo.updateTbrValue = Gekko::Gekko->GetTicks() + fifo.tickPerFifo;

    fifo.thread = new Thread(CPThread, false, nullptr, "CPThread");
    assert(fifo.thread);
}

void CPClose()
{
    delete fifo.thread;
}
