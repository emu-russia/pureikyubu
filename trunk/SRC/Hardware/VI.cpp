// VI - video interface (TV stuff).
// 0.09 was very stupid, so I completely rewrote whole module;
// now "wait retrace" will never hung in homedev demos.
#include "dolphin.h"

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
        u16         regs[59];       // regs are copied to shdwRegs
        u16         shdwRegs[59];   // shdwRegs are copied to hardware registers
        VIHorVer    HorVer;         // used for temporary calculations
    } vi;

    useful VI regs : VI_DISP_CR, VI_TFBL, VI_DISP_POS, VI_INT0.
    dont care about others. Nintendo promised to add HCOUNT (line) interrupt,
    but until it is not used by IPL, we will not use it too. there is not
    much time before GC farewell (year 2006), so if N will not update
    production boards, this will never happen.

--------------------------------------------------------------------------- */

// VI state (registers and other data)
VIControl vi;

// ---------------------------------------------------------------------------
// drawing of XFB

// YUV to RGB conversion
#define yuv2rs(y, u, v) ( (u32)bound((76283*(y - 16) + 104595*(v - 128))>>16) )
#define yuv2gs(y, u, v) ( (u32)bound((76283*(y - 16) - 53281 *(v - 128) - 25624*(u - 128))>>16) << 8 )
#define yuv2bs(y, u, v) ( (u32)bound((76283*(y - 16) + 132252*(u - 128))>>16) << 16 )

// clamping routine
static inline s32 bound(s32 x)
{
    if(x < 0) x = 0;
    if(x > 255) x = 255;
    return x;
}

// copy XFB to screen
void YUVBlit(u8 *yuvbuf, RGBQUAD *dib)
{
    u32 *rgbbuf = (u32 *)dib;
    s32 count = 320 * 480;
    
    if(yuvbuf == NULL) return;

    // simple blitting, without effects
    BeginProfileGfx();
    while(count--)
    {
        s32 y1 = *yuvbuf++,
            v  = *yuvbuf++,
            y2 = *yuvbuf++,
            u  = *yuvbuf++;

        *rgbbuf++ = yuv2bs(y1, u, v) | yuv2gs(y1, u, v) | yuv2rs(y1, u, v);
        *rgbbuf++ = yuv2bs(y2, u, v) | yuv2gs(y2, u, v) | yuv2rs(y2, u, v);
    }
    
    GDIRefresh();
    EndProfileGfx();
}

// ---------------------------------------------------------------------------
// frame timing

// reset VI timing
static void vi_set_timing()
{
    u16 reg  = vi.disp_cr;
    vi.inter = (reg & VI_CR_NIN) ? 0 : 1;
    vi.mode  = VI_CR_FMT(reg);
    if(vi.mode == 2) vi.mode = VI_NTSC_LIKE; // MPAL same as NTSC
    vi.vtime = TBR;

    switch(vi.mode)
    {
        case VI_NTSC_LIKE:
            vi.one_frame = cpu.one_second / 30;
            if(vi.auto_vcnt) vi.vcount = (vi.inter) ? VI_NTSC_INTER : VI_NTSC_NON_INTER;
            break;
        case VI_PAL_LIKE:
            vi.one_frame = cpu.one_second / 25;
            if(vi.auto_vcnt) vi.vcount = (vi.inter) ? VI_PAL_INTER : VI_PAL_NON_INTER;
            break;
    }
}

// step line counter(s), update GUI and poll controller
void VIUpdate()
{
    if((TBR - vi.vtime) >= vi.one_frame / vi.vcount)
    {
        vi.vtime = TBR;

        u32 currentBeamPos = VI_POS_VCT(vi.pos);
        u32 triggerBeamPos = VI_INT_VCT(vi.int0);

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
        if(currentBeamPos > vi.vcount)
        {
            currentBeamPos = 1;

            // patch memory every frame
            ApplyPatches();

            // poll controllers
            SIPoll();

            // update DVD audio
            DIStreamUpdate();
            
            // draw XFB
            if(vi.xfb) YUVBlit(vi.xfbbuf, vi.gfxbuf);
            vi.frames++;

            // show system time and do some win32 update
            SetStatusText(STATUS_TIME, OSTimeFormat(UTBR));
            UpdateProfiler();
            UpdateMainWindow(1);
        }
        vi.pos &= ~0x07ff0000;
        vi.pos |= (currentBeamPos & 0x7ff) << 16;

        // sync CPU clock with VI
    }
}

// ---------------------------------------------------------------------------
// accessing VI registers.

static void __fastcall vi_read8(u32 addr, u32 *reg)
{
    // TODO
    DolwinReport("VI READ8");
    *reg = 0;
}

static void __fastcall vi_write8(u32 addr, u32 data)
{
    DolwinReport("VI WRITE8");
}

static void __fastcall vi_read16(u32 addr, u32 *reg)
{
    switch(addr & 0x7f)
    {
        case 0x02:      // display control
            *reg = vi.disp_cr;
            return;
        case 0x1C:      // video buffer hi (TOP)
            *reg = vi.tfbl >> 16;
            return;
        case 0x1E:      // video buffer low (TOP)
            *reg = (u16)vi.tfbl;
            return;
        case 0x24:      // video buffer hi (BOTTOM)
            *reg = vi.bfbl >> 16;
            return;
        case 0x26:      // video buffer low (BOTTOM)
            *reg = (u16)vi.bfbl;
            return;
        case 0x2C:      // beam position hi
            *reg = vi.pos >> 16;
            return;
        case 0x2E:      // beam position low
            *reg = (u16)vi.pos;
            return;
        case 0x30:      // int0 control hi
            *reg = vi.int0 >> 16;
            return;
        case 0x32:      // int0 control low
            *reg = (u16)vi.int0;
            return;
    }
    *reg = 0;
}

static void __fastcall vi_write16(u32 addr, u32 data)
{
    switch(addr & 0x7f)
    {
        case 0x02:      // display control
            vi.disp_cr = (u16)data;
            vi_set_timing();
            return;
        case 0x1C:      // video buffer hi (TOP)
            vi.tfbl &= 0x0000ffff;
            vi.tfbl |= data << 16;
            DBReport(VI "TFBL set to %08X (xof=%i)\n", vi.tfbl, (vi.tfbl >> 24) & 0xf);
            vi.tfbl &= 0xffffff;
            if(vi.tfbl >= RAMSIZE) vi.xfbbuf = NULL;
            else vi.xfbbuf = &RAM[vi.tfbl];
            return;
        case 0x1E:      // video buffer low (TOP)
            vi.tfbl &= 0xffff0000;
            vi.tfbl |= (u16)data;
            DBReport(VI "TFBL set to %08X (xof=%i)\n", vi.tfbl, (vi.tfbl >> 24) & 0xf);
            vi.tfbl &= 0xffffff;
            if(vi.tfbl >= RAMSIZE) vi.xfbbuf = NULL;
            else vi.xfbbuf = &RAM[vi.tfbl];
            return;
        case 0x24:      // video buffer hi (BOTTOM)
            vi.bfbl &= 0x0000ffff;
            vi.bfbl |= data << 16;
            vi.bfbl &= 0xffffff;
            DBReport(VI "BFBL set to %08X\n", vi.bfbl);
            //if(vi.bfbl >= RAMSIZE) vi.xfbbuf = NULL;
            //else vi.xfbbuf = &RAM[vi.bfbl];
            return;
        case 0x26:      // video buffer low (BOTTOM)
            vi.bfbl &= 0xffff0000;
            vi.bfbl |= (u16)data;
            vi.bfbl &= 0xffffff;
            DBReport(VI "BFBL set to %08X\n", vi.bfbl);
            //if(vi.bfbl >= RAMSIZE) vi.xfbbuf = NULL;
            //else vi.xfbbuf = &RAM[vi.bfbl];
            return;
        case 0x2C:      // beam position hi
            vi.pos &= 0x0000ffff;
            vi.pos |= data << 16;
            return;
        case 0x2E:      // beam position low
            vi.pos &= 0xffff0000;
            vi.pos |= (u16)data;
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
            vi.int0 |= (u16)data;
            return;
    }
}

static void __fastcall vi_read32(u32 addr, u32 *reg)
{
    switch(addr & 0x7f)
    {
        case 0x00:      // display control
            *reg = (u32)vi.disp_cr;
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

static void __fastcall vi_write32(u32 addr, u32 data)
{
    switch(addr & 0x7f)
    {
        case 0x00:      // display control
            vi.disp_cr = (u16)data;
            vi_set_timing();
            return;
        case 0x1C:      // video buffer (TOP)
            vi.tfbl = data & 0xffffff;
            DBReport(VI "TFBL set to %08X (xof=%i)\n", vi.tfbl, (data >> 24) & 0xf);
            if(vi.tfbl >= RAMSIZE) vi.xfbbuf = NULL;
            else vi.xfbbuf = &RAM[vi.tfbl];
            return;
        case 0x24:      // video buffer (BOTTOM)
            vi.bfbl = data & 0xffffff;
            DBReport(VI "BFBL set to %08X\n", vi.bfbl);
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
    u32 currentBeamPos = VI_POS_VCT(vi.pos);
    u32 triggerBeamPos = VI_INT_VCT(vi.int0);

    DBReport(GREEN "    VI interrupt : [%i x x x]\n", vi.int0 >> 31);
    DBReport(GREEN "    VI int mask  : [%i x x x]\n", (vi.int0 >> 28) & 1);
    DBReport(GREEN "    VI int pos   : %i == %i, x == x, x == x, x == x (line)\n", currentBeamPos, triggerBeamPos);
    DBReport(GREEN "    VI XFB       : T%08X B%08X (phys), enabled: %i\n", vi.tfbl, vi.bfbl, vi.xfb);
}

// ---------------------------------------------------------------------------
// init

void VIOpen()
{
    DBReport(CYAN "VI: Video-out hardware interface\n");

    // clear VI regs
    memset(&vi, 0, sizeof(VIControl));

    // read VI settings
    vi.log = GetConfigInt(USER_VI_LOG, USER_VI_LOG_DEFAULT) & 1;
    vi.xfb = GetConfigInt(USER_VI_XFB, USER_VI_XFB_DEFAULT) & 1;
    vi.stretch = GetConfigInt(USER_VI_STRETCH, USER_VI_STRETCH_DEFAULT) & 1;

    // vertical count value
    vi.vcount = GetConfigInt(USER_VI_COUNT, USER_VI_COUNT_DEFAULT);
    vi.auto_vcnt = (vi.vcount == 0);
    if(!vi.auto_vcnt)
    {
        if(vi.log) DBReport(VI "manual timing enabled (vcount: %i)\n", vi.vcount);
    }

    // reset VI timing
    vi_set_timing();

    // XFB is not yet specified
    vi.gfxbuf = NULL;
    vi.xfbbuf = NULL;

    // open GDI (if need)
    if(vi.xfb)
    {
        BOOL res = GDIOpen(wnd.hMainWindow, 640, 480, &vi.gfxbuf);
        if(res == FALSE)
        {
            DolwinReport("VI cant startup GDI");
            vi.xfb = FALSE;
        }
    }

    // set traps to VI registers
    for(u32 ofs=0; ofs<0x80; ofs++)
    {
        HWSetTrap(8, 0x0C002000 + ofs, vi_read8, vi_write8);
        if((ofs % 2) == 0) HWSetTrap(16, 0x0C002000 + ofs, vi_read16, vi_write16);
        if((ofs % 4) == 0) HWSetTrap(32, 0x0C002000 + ofs, vi_read32, vi_write32);
    }
}

void VIClose()
{
    // XFB can be enabled during emulation,
    // so we must be sure, that GDI is closed
    // even if XFB wasn't enabled, before start
    GDIClose(wnd.hMainWindow);
}
