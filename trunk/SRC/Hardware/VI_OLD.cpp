/******* OLDER VI FROM 0.09 *******/

// VI emulation, XFB copying
// TODO : time to rewrite whole VI,
// currently this module is looks like one big hack.
// because its oldest Dolwin hardware stuff =)
#include "dolphin.h"

VIControl vi;       // VI state

BOOL    VIBLUR;     // enable blur effect
BOOL    VIBW;       // black-white VI

u8      gamma = 0;  // linear gamma
u32*    rgbbuf;

// shifted fixed-point YUV to RGB-component conversion
#define yuv2rs(y, u, v) ( (u32)bound((76283*(y - 16) + 104595*(v - 128))>>16) )
#define yuv2gs(y, u, v) ( (u32)bound((76283*(y - 16) - 53281 *(v - 128) - 25624*(u - 128))>>16) << 8 )
#define yuv2bs(y, u, v) ( (u32)bound((76283*(y - 16) + 132252*(u - 128))>>16) << 16 )

// clamping routine
static __declspec(naked) int __fastcall bound(int x)
{
    __asm
    {
        xor     eax, eax
        cmp     ecx, 0
        jl      bound_ret
        mov     eax, 255
        cmp     ecx, eax
        jg      bound_ret
        mov     eax, ecx
bound_ret:
        ret
    }
}

// copy YCbCr image from memory to window DC
// xfb is pointing to image data
void YUVBlit(void *xfb)
{
    u32 *rgb = rgbbuf;
    u8  *yuvbuf;
    int count = 320 * 480;
    
    if((yuvbuf = (u8 *)xfb) == NULL)
    {
        return;
    }

    // hung checks for home-brewn
#ifdef  DOLWIN_HUNG_WORKAROUND
    first_draw_done = TRUE;
    hung_counter = R_IC;
#endif

    while(count--)
    {
        int y1 = *yuvbuf++, 
            v  = *yuvbuf++, 
            y2 = *yuvbuf++, 
            u  = *yuvbuf++;

        // adjust gamma settings
        if(gamma)
        {
            y1 = (y1 + gamma) & 0xff;
            y2 = (y2 + gamma) & 0xff;
        }

        // nice-looking blur effect (thanks to EJ for solution)
        // id like to see it in form of pixel shader
        if(VIBLUR)
        {
            u32 pi, bi;

            if(VIBW == TRUE)
            {
                pi = *rgb;
                bi = (y1 << 16) | (y1 << 8) | y1;
                *rgb++ = (0x7f7f7f & (pi >> 1)) + (0x7f7f7f & (bi >> 1));

                pi = *rgb;
                bi = (y2 << 16) | (y2 << 8) | y2;
                *rgb++ = (0x7f7f7f & (pi >> 1)) + (0x7f7f7f & (bi >> 1));
            }
            else
            {
                pi = *rgb;
                bi = yuv2bs(y1, u, v) | yuv2gs(y1, u, v) | yuv2rs(y1, u, v);
                *rgb++ = (0x7f7f7f & (pi >> 1)) + (0x7f7f7f & (bi >> 1));

                pi = *rgb;
                bi = yuv2bs(y2, u, v) | yuv2gs(y2, u, v) | yuv2rs(y2, u, v);
                *rgb++ = (0x7f7f7f & (pi >> 1)) + (0x7f7f7f & (bi >> 1));
            }
        }
        else
        {
            if(VIBW == TRUE)
            {
                *rgb++ = (y1 << 16) | (y1 << 8) | y1;
                *rgb++ = (y2 << 16) | (y2 << 8) | y2;
            }
            else
            {
                *rgb++ = yuv2bs(y1, u, v) | yuv2gs(y1, u, v) | yuv2rs(y1, u, v);
                *rgb++ = yuv2bs(y2, u, v) | yuv2gs(y2, u, v) | yuv2rs(y2, u, v);
            }
        }
    }
    
    GDIRefresh();
    vi.frames++;
    //UpdateProfiler();
}

// ---------------------------------------------------------------------------

/*
 *  CC002002 - for video mode (determining maximum hline value)     FMT
 *  CC00201C - for XFB address (if VI blitting is enabled)          TFBB
 *  CC00202C - hline count. returns 1, 525, 1, 525 etc.             VCOUNT
 *  CC002030 - always OR with 0x80000000, when reading              INT
 */

static int  hline = 1, vimaxline;
static u32 tfbl;
       u8  *vixfb;          // current xfb translated address
static s64  vic[2];         // vi instruction counters for bcb
static u16  viclk = 0;      // vi clock select register
static u32  viint[4];       // four line interrupt regs
// 0 = 27 MHz video CLK  pal50/pal60/ntsc: 0x0000, 0x0000, 0x0000
// 1 = 54 MHz video CLK (used in Progressive Mode)

// select max hline
static void VISetMode(int mode)
{
    switch(mode)
    {
        case 0: vimaxline = 639; break;
        case 1: vimaxline = 573; break;
        case 2: vimaxline = 639; break;
        case 3:
        case 4: vimaxline =   1; break;
        case 5: vimaxline = 639; break;
    }
}

static void __fastcall write_vmode16(u32 addr, u32 data)
{
    u16 *buf = (u16 *)&data; 
    VISetMode(*buf >> 8);
}

static void __fastcall write_vmode32(u32 addr, u32 data)
{
    u32 *buf = (u32 *)&data; 
    VISetMode((*buf & 0xffff) >> 8);
}

// frame buffer address
static void __fastcall write_tfbb(u32 addr, u32 data)
{
    tfbl = data & RAMMASK;
    vixfb = &RAM[tfbl];

    DBReport(VI "xfb address now 0x%.8X\n", data);
}

// line counter
static void __fastcall read_line16(u32 addr, u32 *reg)
{
    u16 *buf = (u16 *)reg;
    buf[0] = hline;
    buf[1] = 0;
}

// line interrupt register (int always enabled)
#define VIINT       0x80000000
#define VIINTEN     0x10000000
static void __fastcall read_int16(u32 addr, u32 *reg) { *reg = viint[(addr & 0xf) / 4] >> 16; }
static void __fastcall read_int32(u32 addr, u32 *reg) { *reg = viint[(addr & 0xf) / 4]; }
static void __fastcall write_int16(u32 addr, u32 data)
{
    viint[(addr & 0xf) / 4] = data << 16;
    if((data & 0x8000) == 0)        // clear INT
    {
        viint[0] &= ~VIINT;
        PIClearInt(PI_INTERRUPT_VI);
    }
}
static void __fastcall write_int32(u32 addr, u32 data)
{
    viint[(addr & 0xf) / 4] = data;
    if((data & 0x80000000) == 0)    // clear INT
    {
        viint[0] &= ~VIINT;
        PIClearInt(PI_INTERRUPT_VI);
    }
}

// TODO : check this !
static void __fastcall write_viclk_status(u32 addr, u32 data) { viclk = ((u16)data) & 0x0001; }
static void __fastcall read_viclk_status(u32 addr, u32 *reg)  { *reg = viclk; }
static void __fastcall write_vidtv_status(u32 addr, u32 data) { vi.dtv = (u16)data; }
static void __fastcall read_vidtv_status(u32 addr, u32 *reg)  { *reg = vi.dtv; }

// dummies
static void __fastcall write_dummy(u32 addr, u32 data) {}
static void __fastcall read_dummy(u32 addr, u32 *reg)  { *reg = 0; }

// ---------------------------------------------------------------------------

static void VIRefresh()
{
    if(hline) hline = 0;
    else hline = vimaxline;
    
    if(vi.xfb == TRUE) YUVBlit(vixfb);
}

void VIUpdate()
{
    // software checking
    if((TBR - vic[0]) >= VI_ONE_FRAME)
    {
        vic[0] = TBR;

        VIRefresh();
        SIPoll();

        // vi retrace interrupt
        viint[0] |= VIINT;
        if(viint[0] & VIINTEN)
        {
            PIAssertInt(PI_INTERRUPT_VI);
        }

        // update DVD audio
        DIStreamUpdate();

        // show os time and do some win32 update
        SetStatusText(STATUS_TIME, OSTimeFormat(UTBR));
        UpdateProfiler();
        UpdateMainWindow(1);
    }
    
    // line counter
/*/
    if((TBR - vic[1]) >= VI_ONE_FRAME / vimaxline)
    {
        vic[1] = TBR;
        hline++;
        if(hline > vimaxline) hline = 0;
    }
/*/
}

void VIOpen()
{
    DBReport(CYAN "VI OLD\n");

    // get VI settings
    vi.xfb = GetConfigInt("VI_XFB", TRUE);
    VIBLUR = GetConfigInt("VI_BLUR", FALSE);
    VIBW = GetConfigInt("VI_BW", FALSE);

    rgbbuf = NULL;

    // if DIB still active during OGL/D3D rendering
    // screen will just flash between primary buffer and XFB
    if(vi.xfb == TRUE)
    {
        BOOL res = GDIOpen(wnd.hMainWindow, 640, 480, (RGBQUAD **)&rgbbuf);
        ASSERT(res == FALSE, "VI cant startup GDI");
    }

    // hide current xfb
    vixfb = NULL;

    // set frame branch callback
    vic[0] = vic[1] = TBR;

    // clear registers
    vi.mode = VI_NTSC;
    viint[0] = viint[1] = viint[2] = viint[3] = 0;
    viclk = vi.dtv = 0;
    hline = 0;
    vimaxline = 639;

    // set dummy hooks
    for(u32 ofs=0; ofs<0x80; ofs++)
    {
        HWSetTrap(8, 0x0C002000 + ofs, read_dummy, write_dummy);
        if((ofs % 2) == 0) HWSetTrap(16, 0x0C002000 + ofs, read_dummy, write_dummy);
        if((ofs % 4) == 0) HWSetTrap(32, 0x0C002000 + ofs, read_dummy, write_dummy);
    }

    // set hooks to important VI regs
    HWSetTrap(16, 0x0c002002, read_dummy, write_vmode16);
    HWSetTrap(16, 0x0c00202c, read_line16, write_dummy);
    HWSetTrap(32, 0x0c002000, read_dummy, write_vmode32);
    HWSetTrap(32, 0x0c00201c, read_dummy, write_tfbb);
    HWSetTrap(16, 0x0c002030, read_int16, write_int16);
    HWSetTrap(32, 0x0c002030, read_int32, write_int32);
    HWSetTrap(16, 0x0c002034, read_int16, write_int16);
    HWSetTrap(32, 0x0c002034, read_int32, write_int32);
    HWSetTrap(16, 0x0c002038, read_int16, write_int16);
    HWSetTrap(32, 0x0c002038, read_int32, write_int32);
    HWSetTrap(16, 0x0c00203c, read_int16, write_int16);
    HWSetTrap(32, 0x0c00203c, read_int32, write_int32);
    HWSetTrap(16, 0x0c00206c, read_viclk_status, write_viclk_status);
    HWSetTrap(16, 0x0c00206e, read_vidtv_status, write_vidtv_status);
}

void VIClose()
{
    // XFB can be changed, during emulation
    // so we need to be sure, that DIB is closed
    // even if XFB wasn't enabled, before start
    GDIClose(wnd.hMainWindow);
}

// show VI info
void VIStats()
{
    DBReport(GREEN "    VI interrupt : %i\n", viint[0] >> 31);
    DBReport(GREEN "    VI int mask  : %i\n", (viint[0] >> 28) & 1);
    DBReport(GREEN "    VI int time  : %i (clock)\n", VI_ONE_FRAME);
    DBReport(GREEN "    VI XFB       : %08X (phys), enabled: %i\n", tfbl, vi.xfb);
}
