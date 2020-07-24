// VI - video interface (TV stuff).
#include "pch.h"

/* ---------------------------------------------------------------------------

    Timing
    ------

    small note for VI frame timing :

     ---------------------------------------------------
    |       |         PAL         | NTSC, MPAL, EURGB60 |
    | value +---------------------|---------------------|
    |       |   NIN    |   INT    |   NIN    |   INT    |
    |=======+==========+==========+==========+==========|
    |  hz   |   50     |    25    |    60    |    30    |
    |-------+----------+----------+----------+----------|
    | lines |  312.5   |   625    |  262.5   |   525    |
    |-------+----------+----------+----------+----------|
    | active|   262    |   574    |   218    |   480    |
     -------+----------+----------+----------+----------

    NIN - non-intelaced (double-strike) mode (both odd and even lines in one frame)
    INT - interlaced mode (odd and even lines in alternating frames)
    25/50 hz refer to the frequency of which a full videoframe (==all lines of framebuffer) is displayed
    progressive = double-strike = non-interlaced;
    "field" is the tv-frame as in "odd- and even- field" makes one frame

    Registers
    ---------

    VI registers block takes 0x76 bytes (from reversing of VI library) :

    static struct
    {
        uint16_t         regs[59];       // regs are copied to shdwRegs
        uint16_t         shdwRegs[59];   // shdwRegs are copied to hardware registers
        VIHorVer    HorVer;         // used for temporary calculations
    } vi;

    useful VI regs : VI_DISP_CR, VI_TFBL, VI_DISP_POS, VI_INT0.
    dont care about others. Nintendo promised to add HCOUNT (line) interrupt,
    but until it is not used by IPL, we will not use it too. there is not
    much time before GC farewell (year 2006), so if N will not update
    production boards, this will never happen.

--------------------------------------------------------------------------- */

using namespace Debug;

// VI state (registers and other data)
VIControl vi;

// ---------------------------------------------------------------------------
// drawing of XFB

// YUV to RGB conversion
#define yuv2rs(y, u, v) ( (uint32_t)bound((76283*(y - 16) + 104595*(v - 128))>>16) )
#define yuv2gs(y, u, v) ( (uint32_t)bound((76283*(y - 16) - 53281 *(v - 128) - 25624*(u - 128))>>16) << 8 )
#define yuv2bs(y, u, v) ( (uint32_t)bound((76283*(y - 16) + 132252*(u - 128))>>16) << 16 )

// clamping routine
static inline int bound(int x)
{
    if(x < 0) x = 0;
    if(x > 255) x = 255;
    return x;
}

// copy XFB to screen
void YUVBlit(uint8_t *yuvbuf, RGB *dib)
{
    uint32_t *rgbbuf = (uint32_t *)dib;
    int count = 320 * 480;
    
    if(!yuvbuf || !rgbbuf) return;

    // simple blitting, without effects
    while(count--)
    {
        int y1 = *yuvbuf++,
            v  = *yuvbuf++,
            y2 = *yuvbuf++,
            u  = *yuvbuf++;

        *rgbbuf++ = yuv2bs(y1, u, v) | yuv2gs(y1, u, v) | yuv2rs(y1, u, v);
        *rgbbuf++ = yuv2bs(y2, u, v) | yuv2gs(y2, u, v) | yuv2rs(y2, u, v);
    }
    
    VideoOutRefresh();
}

// ---------------------------------------------------------------------------
// frame timing

// reset VI timing
static void vi_set_timing()
{
    uint16_t reg  = vi.disp_cr;
    vi.inter = (reg & VI_CR_NIN) ? 0 : 1;
    vi.mode  = VI_CR_FMT(reg);
    if(vi.mode == 2) vi.mode = VI_NTSC_LIKE; // MPAL same as NTSC
    vi.vtime = Gekko::Gekko->GetTicks();

    switch(vi.mode)
    {
        case VI_NTSC_LIKE:
            vi.one_frame = vi.one_second / 30;
            vi.vcount = (vi.inter) ? VI_NTSC_INTER : VI_NTSC_NON_INTER;
            break;
        case VI_PAL_LIKE:
            vi.one_frame = vi.one_second / 25;
            vi.vcount = (vi.inter) ? VI_PAL_INTER : VI_PAL_NON_INTER;
            break;
    }
}

// step line counter(s), update GUI and poll controller
void VIUpdate()
{
    if((Gekko::Gekko->GetTicks() - vi.vtime) >= (vi.one_frame / vi.vcount))
    {
        vi.vtime = Gekko::Gekko->GetTicks();

        uint32_t currentBeamPos = VI_POS_VCT(vi.pos);
        uint32_t triggerBeamPos = VI_INT_VCT(vi.int0);

        // generate VIINT ?
        currentBeamPos++;
        if(currentBeamPos == triggerBeamPos)
        {
            vi.int0 |= VI_INT_INT;
            if(vi.int0 & VI_INT_ENB)
            {
                PIAssertInt(PI_INTERRUPT_VI);
            }
        }

        // vertical counter
        if (currentBeamPos >= vi.vcount)
        {
            currentBeamPos = 1;

            // draw XFB
            if (vi.xfb)
            {
                YUVBlit(vi.xfbbuf, vi.gfxbuf);
                vi.frames++;
            }
        }

        vi.pos &= ~0x07ff0000;
        vi.pos |= (currentBeamPos & 0x7ff) << 16;
    }
}

// ---------------------------------------------------------------------------
// accessing VI registers.

static void vi_read8(uint32_t addr, uint32_t *reg)
{
    // TODO
    Report(Channel::VI, "VI READ8");
    *reg = 0;
}

static void vi_write8(uint32_t addr, uint32_t data)
{
    Report(Channel::VI, "VI WRITE8");
}

static void vi_read16(uint32_t addr, uint32_t *reg)
{
    switch(addr & 0x7f)
    {
        case 0x02:      // display control
            *reg = vi.disp_cr & ~1;
            *reg |= vi.videoEncoderFuse & 1;
            return;
        case 0x1C:      // video buffer hi (TOP)
            *reg = vi.tfbl >> 16;
            return;
        case 0x1E:      // video buffer low (TOP)
            *reg = (uint16_t)vi.tfbl;
            return;
        case 0x24:      // video buffer hi (BOTTOM)
            *reg = vi.bfbl >> 16;
            return;
        case 0x26:      // video buffer low (BOTTOM)
            *reg = (uint16_t)vi.bfbl;
            return;
        case 0x2C:      // beam position hi
            *reg = vi.pos >> 16;
            return;
        case 0x2E:      // beam position low
            *reg = (uint16_t)vi.pos;
            return;
        case 0x30:      // int0 control hi
            *reg = vi.int0 >> 16;
            return;
        case 0x32:      // int0 control low
            *reg = (uint16_t)vi.int0;
            return;
    }
    *reg = 0;
}

static void vi_write16(uint32_t addr, uint32_t data)
{
    switch(addr & 0x7f)
    {
        case 0x02:      // display control
            vi.disp_cr = (uint16_t)data;
            vi_set_timing();
            return;
        case 0x1C:      // video buffer hi (TOP)
            vi.tfbl &= 0x0000ffff;
            vi.tfbl |= data << 16;
            if (vi.log)
            {
                Report(Channel::VI, "TFBL set to %08X (xof=%i)\n", vi.tfbl, (vi.tfbl >> 24) & 0xf);
            }
            vi.tfbl &= 0xffffff;
            if(vi.tfbl >= mi.ramSize) vi.xfbbuf = NULL;
            else vi.xfbbuf = &mi.ram[vi.tfbl & RAMMASK];
            return;
        case 0x1E:      // video buffer low (TOP)
            vi.tfbl &= 0xffff0000;
            vi.tfbl |= (uint16_t)data;
            if (vi.log)
            {
                Report(Channel::VI, "TFBL set to %08X (xof=%i)\n", vi.tfbl, (vi.tfbl >> 24) & 0xf);
            }
            vi.tfbl &= 0xffffff;
            if(vi.tfbl >= mi.ramSize) vi.xfbbuf = NULL;
            else vi.xfbbuf = &mi.ram[vi.tfbl & RAMMASK];
            return;
        case 0x24:      // video buffer hi (BOTTOM)
            vi.bfbl &= 0x0000ffff;
            vi.bfbl |= data << 16;
            vi.bfbl &= 0xffffff;
            if (vi.log)
            {
                Report(Channel::VI, "BFBL set to %08X\n", vi.bfbl);
            }
            //if(vi.bfbl >= RAMSIZE) vi.xfbbuf = NULL;
            //else vi.xfbbuf = &RAM[vi.bfbl];
            return;
        case 0x26:      // video buffer low (BOTTOM)
            vi.bfbl &= 0xffff0000;
            vi.bfbl |= (uint16_t)data;
            vi.bfbl &= 0xffffff;
            if (vi.log)
            {
                Report(Channel::VI, "BFBL set to %08X\n", vi.bfbl);
            }
            //if(vi.bfbl >= RAMSIZE) vi.xfbbuf = NULL;
            //else vi.xfbbuf = &RAM[vi.bfbl];
            return;
        case 0x2C:      // beam position hi
            vi.pos &= 0x0000ffff;
            vi.pos |= data << 16;
            return;
        case 0x2E:      // beam position low
            vi.pos &= 0xffff0000;
            vi.pos |= (uint16_t)data;
            return;
        case 0x30:      // int0 control hi
            vi.int0 &= 0x0000ffff;
            vi.int0 |= data << 16;
            if((vi.int0 & VI_INT_INT) == 0)
            {
                PIClearInt(PI_INTERRUPT_VI);
            }
            return;
        case 0x32:      // int0 control low
            vi.int0 &= 0xffff0000;
            vi.int0 |= (uint16_t)data;
            return;
    }
}

static void vi_read32(uint32_t addr, uint32_t *reg)
{
    switch(addr & 0x7f)
    {
        case 0x00:      // display control
            *reg = (uint32_t)(vi.disp_cr & ~1);
            *reg |= vi.videoEncoderFuse & 1;
            return;
        case 0x1C:      // video buffer (TOP)
            *reg = vi.tfbl;
            return;
        case 0x24:      // video buffer (BOTTOM)
            *reg = vi.bfbl;
            return;
        case 0x2C:      // beam position
            *reg = vi.pos;
            return;
        case 0x30:      // int0 control
            *reg = vi.int0;
            return;
    }
    *reg = 0;
}

static void vi_write32(uint32_t addr, uint32_t data)
{
    switch(addr & 0x7f)
    {
        case 0x00:      // display control
            vi.disp_cr = (uint16_t)data;
            vi_set_timing();
            return;
        case 0x1C:      // video buffer (TOP)
            vi.tfbl = data & 0xffffff;
            if (vi.log)
            {
                Report(Channel::VI, "TFBL set to %08X (xof=%i)\n", vi.tfbl, (data >> 24) & 0xf);
            }
            if(vi.tfbl >= mi.ramSize) vi.xfbbuf = NULL;
            else vi.xfbbuf = &mi.ram[vi.tfbl & RAMMASK];
            return;
        case 0x24:      // video buffer (BOTTOM)
            vi.bfbl = data & 0xffffff;
            if (vi.log)
            {
                Report(Channel::VI, "BFBL set to %08X\n", vi.bfbl);
            }
            //if(vi.bfbl >= RAMSIZE) vi.xfbbuf = NULL;
            //else vi.xfbbuf = &RAM[vi.bfbl];
            return;
        case 0x2C:      // beam position
            vi.pos = data;
            return;
        case 0x30:      // int0 control
            vi.int0 = data;
            if((vi.int0 & VI_INT_INT) == 0)
            {
                PIClearInt(PI_INTERRUPT_VI);
            }
            return;
    }
}

// show VI info
void VIStats()
{
    uint32_t currentBeamPos = VI_POS_VCT(vi.pos);
    uint32_t triggerBeamPos = VI_INT_VCT(vi.int0);

    Report(Channel::Norm, "    VI interrupt : [%i x x x]\n", vi.int0 >> 31);
    Report(Channel::Norm, "    VI int mask  : [%i x x x]\n", (vi.int0 >> 28) & 1);
    Report(Channel::Norm, "    VI int pos   : %i == %i, x == x, x == x, x == x (line)\n", currentBeamPos, triggerBeamPos);
    Report(Channel::Norm, "    VI XFB       : T%08X B%08X (phys), enabled: %i\n", vi.tfbl, vi.bfbl, vi.xfb);
}

// ---------------------------------------------------------------------------
// init

void VIOpen(HWConfig * config)
{
    Report(Channel::VI, "Video-out hardware interface\n");

    // clear VI regs
    memset(&vi, 0, sizeof(VIControl));

    vi.one_second = Gekko::Gekko->OneSecond();

    // read VI settings
    vi.log = config->vi_log;
    vi.xfb = config->vi_xfb;
    vi.videoEncoderFuse = config->videoEncoderFuse;

    // reset VI timing
    vi_set_timing();

    // XFB is not yet specified
    vi.gfxbuf = NULL;
    vi.xfbbuf = NULL;

    // open GDI (if need)
    if(vi.xfb)
    {
        bool res = VideoOutOpen(config, 640, 480, &vi.gfxbuf);
        if(!res)
        {
            Report(Channel::VI, "VI cant startup VideoOut backend!");
            vi.xfb = false;
        }
    }

    // set traps to VI registers
    for(uint32_t ofs=0; ofs<0x80; ofs++)
    {
        MISetTrap(8, 0x0C002000 + ofs, vi_read8, vi_write8);
        if((ofs % 2) == 0) MISetTrap(16, 0x0C002000 + ofs, vi_read16, vi_write16);
        if((ofs % 4) == 0) MISetTrap(32, 0x0C002000 + ofs, vi_read32, vi_write32);
    }
}

void VIClose()
{
    // XFB can be enabled during emulation,
    // so we must be sure, that GDI is closed
    // even if XFB wasn't enabled, before start
    VideoOutClose();
}

void VISetEncoderFuse(int value)
{
    vi.videoEncoderFuse = value;
}
