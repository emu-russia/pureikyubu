// CP - command processor, PE - pixel engine.
#include "pch.h"

using namespace Debug;

// TODO: Refactoring hacks

size_t done_num;   // number of drawdone (PE_FINISH) events

static void CPDrawDoneCallback()
{
    done_num++;
    vi.frames++;
    if (done_num == 1)
    {
        vi.xfb = 0;     // disable VI output
    }

    Flipper::Gx->CPDrawDoneCallback();
}

static void CPDrawTokenCallback(uint16_t tokenValue)
{
    vi.frames++;
    vi.xfb = 0;     // disable VI output

    Flipper::Gx->CPDrawTokenCallback(tokenValue);
}


#pragma region "registers"

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

// Pixel Engine regs handlers

static void PERegRead(uint32_t addr, uint32_t* reg)
{
    *reg = Flipper::Gx->PeReadReg(  (GX::PEMappedRegister)((addr & 0xFF) >> 1) );
}

static void PERegWrite(uint32_t addr, uint32_t data)
{
    Flipper::Gx->PeWriteReg((GX::PEMappedRegister)((addr & 0xFF) >> 1), data);
}

#pragma endregion "registers"

// ---------------------------------------------------------------------------
// init

void CP_PEOpen()
{
    Report(Channel::CP, "Command processor and Pixel Engine (for GX)\n");

    // Command Processor
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

    MISetTrap(16, 0x0c00'0040, no_read, nullptr);
    MISetTrap(16, 0x0c00'0042, no_read, nullptr);
    MISetTrap(16, 0x0c00'0044, no_read, nullptr);
    MISetTrap(16, 0x0c00'0046, no_read, nullptr);
    MISetTrap(16, 0x0c00'0048, no_read, nullptr);
    MISetTrap(16, 0x0c00'004a, no_read, nullptr);
    MISetTrap(16, 0x0c00'004c, no_read, nullptr);
    MISetTrap(16, 0x0c00'004e, no_read, nullptr);

    // Pixel Engine
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_POKE_ZMODE_ID), PERegRead, PERegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_POKE_CMODE0_ID), PERegRead, PERegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_POKE_CMODE1_ID), PERegRead, PERegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_POKE_AMODE0_ID), PERegRead, PERegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_POKE_AMODE1_ID), PERegRead, PERegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_SR_ID), PERegRead, PERegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_TOKEN_ID), PERegRead, PERegWrite);

    // TODO: Refactoring hacks
    GXSetDrawCallbacks(CPDrawDoneCallback, CPDrawTokenCallback);
}

void CP_PEClose()
{
}
