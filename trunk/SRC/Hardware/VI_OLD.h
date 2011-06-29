/******* OLDER VI FROM 0.09 *******/

#define VI_OLD

// os time between two retrace interrupts
//#define VI_ONE_FRAME    ((GEKKO_CLK / (vmode ? 60 : 50)) / BIAS)
#define VI_ONE_FRAME    0xbffff

extern  BOOL XFB;
extern  BOOL VIBLUR;
extern  BOOL VIBW;

#define VI_NTSC 0x0          
#define VI_PAL  0x1

typedef struct
{
    int     mode;           // 0=NTSC, 1=PAL like
    u16     dtv;            // vi DTV status
    u32     frames;         // frame counter
    BOOL    xfb;            // enable XFB blitting for homebrewn demos
    BOOL    stretch;
} VIControl;

extern  VIControl vi;

void    VIUpdate();
void    VIOpen();
void    VIClose();
void    VIStats();