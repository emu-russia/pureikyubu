#pragma once

// VI registers (can be accessed from any offset and by any size, 1, 2 or 4 bytes)

#define VI_VERT_TIMING          0x0C002000      // Vertical Timing Register
#define VI_DISP_CR              0x0C002002      // Display Configuration Register
#define VI_HORZ_TIMING0         0x0C002004      // Horizontal Timing 0 Register
#define VI_HORZ_TIMING1         0x0C002008      // Horizontal Timing 1 Register
#define VI_VERT_TIMING_ODD      0x0C00200C      // Odd Field Vertical Timing Register
#define VI_VERT_TIMING_EVEN     0x0C002010      // Even Field Vertical Timing Register
#define VI_BBINT_ODD            0x0C002014      // Odd Field Burst Blanking Interval Register
#define VI_BBINT_EVEN           0x0C002018      // Even Field Burst Blanking Interval Register
#define VI_TFBL                 0x0C00201C      // Top Field Base Register L
#define VI_TFBR                 0x0C002020      // Top Field Base Register R
#define VI_BFBL                 0x0C002024      // Bottom Field Base Register L
#define VI_BFBR                 0x0C002028      // Bottom Field Base Register R
#define VI_DISP_POS             0x0C00202C      // Display Position Register
#define VI_INT0                 0x0C002030      // Display Interrupt Register 0
#define VI_INT1                 0x0C002034      // Display Interrupt Register 1
#define VI_INT2                 0x0C002038      // Display Interrupt Register 2
#define VI_INT3                 0x0C00203C      // Display Interrupt Register 3
// ... unknown gap [32 * 3]
#define VI_TAP0                 0x0C00204C      // Filter Coefficient Table 0
#define VI_TAP1                 0x0C002050      // Filter Coefficient Table 1
#define VI_TAP2                 0x0C002054      // Filter Coefficient Table 2
#define VI_TAP3                 0x0C002058      // Filter Coefficient Table 3
#define VI_TAP4                 0x0C00205C      // Filter Coefficient Table 4
#define VI_TAP5                 0x0C002060      // Filter Coefficient Table 5
#define VI_TAP6                 0x0C002064      // Filter Coefficient Table 6
// ... unknown gap [32]
#define VI_CLK_SEL              0x0C00206C      // VI Clock Select Register
#define VI_DTV                  0x0C00206E      // VI DTV Status Register
// ... unknown gap [16]
#define VI_BRDR_HBE             0x0C002072      // Border HBE
#define VI_BRDR_HBS             0x0C002074      // Border HBS

// mapping is unknown for regs :
#define VI_PICT_CR              0               // [16] Picture Configuration Register
#define VI_DISP_LATCH0          0               // [32] Display Latch Register 0
#define VI_DISP_LATCH1          0               // [32] Display Latch Register 1
#define VI_OUT_POL              0               // [8?] Output Polarity Register
#define VI_HORZ_SCALE           0               // [16] Horizontal Scale Register
#define VI_SCALE_WIDTH          0               // [16] Scaling Width Register

// Display Configuration Register mask (for 16-bit register)
#define VI_CR_ENB       0x0001          // enable the video timing generation
#define VI_CR_RST       0x0002          // puts VI into its idle state
#define VI_CR_NIN       0x0004          // 0: interlace, 1: non-interlace
#define VI_CR_DLR       0x0008          // this bit selects the 3D display mode
#define VI_CR_LE0(r)    ((r>>4)&3)      // gun trigger mode
#define VI_CR_LE1(r)    ((r>>6)&3)      // to enable Display Latch Register 1
#define VI_CR_FMT(r)    ((r>>8)&3)      // indicates current video format

// Display Position Register mask (for 32-bit register)
#define VI_POS_VCT(r)   ((r>>16)&0x7ff) // vertical count (1...vcount in emu)
#define VI_POS_HCT(r)   (r & 0x7ff)     // horizontal count (always 1 in emu)

// Display Interrupt Register mask (for 32-bit register)
#define VI_INT_INT      0x80000000      // interrupt status. "1" indicates that an interrupt is active
#define VI_INT_ENB      0x10000000      // interrupt is enabled if this bit is set
#define VI_INT_VCT(r)   ((r>>16)&0x7ff) // vertical count to generate interrupt
#define VI_INT_HCT(r)   (r & 0x7ff)     // horizontal count to generate interrupt (ignored in emu)

// video modes
#define VI_NTSC_LIKE        0
#define VI_PAL_LIKE         1

// max vertical line count
#define VI_NTSC_INTER       525         // 60 Hz
#define VI_NTSC_NON_INTER   263         // 30 Hz
#define VI_PAL_INTER        625         // 50 Hz
#define VI_PAL_NON_INTER    313         // 25 Hz

// ---------------------------------------------------------------------------
// hardware API

#pragma pack(push, 1)

struct RGB
{
    uint8_t Blue;
    uint8_t Green;
    uint8_t Red;
    uint8_t Reserved;
};

#pragma pack(pop)

// VI state (registers and other data)
struct VIControl
{
    volatile uint16_t    disp_cr;    // display configuration register
    volatile uint32_t    tfbl;       // video buffer (top field)
    volatile uint32_t    bfbl;       // video buffer (bottom field)
    volatile uint32_t    pos;        // beam position
    volatile uint32_t    int0;       // INT0 status

    volatile uint32_t    mode;       // see VI modes
    bool        inter;      // 1, if interlace
    volatile uint32_t    vcount;     // number of lines for single frame
    int64_t     vtime;      // frame timer
    int64_t     one_frame;  // frame length in CPU timer ticks

    bool        xfb;        // enable video frame buffer (GDI)
    uint8_t*    xfbbuf;     // translated TFBL pointer
    RGB*        gfxbuf;     // DIB

    bool        log;        // do debugger log output
    size_t      frames;     // frames rendered by VI

    int64_t     one_second;     // one CPU second in timer ticks

    int         videoEncoderFuse;
};

extern  VIControl vi;

void    VIUpdate();
void    VIStats();
void    VIOpen(HWConfig* config);
void    VIClose();

void    VISetEncoderFuse(int value);
