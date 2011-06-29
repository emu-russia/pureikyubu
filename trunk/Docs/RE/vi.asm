// VI LIBRARY REVERSING

// return first "1" bit entry from u64 value.
// as example  : 0x40000000F0000000, returns 1.
// PPC have same instruction for 32-bit values : cntlzw
int cntlzd(u64 val) { ... }

// 0 - Panasonic, 1 - Normal GC
int getEncoderType() { // HARDCODED 1 }

// VI library have "shadow" registers, placed in RAM.
// VIFlush() is used to update real HW VI registers, from the shadows.

// "changed" contains bit mask, where each bit says, what kind of
// VI registers must be updated inside VI interrupt handler.
u64 changed, shdwChanged;

// this selects mode ()
// 0 - for interlace, 1 - for non-interlace ?
// lets say, that if mode = 1 and current field is odd, then
// dont flush shadows (see VISetRegs()).
u32 changeMode, shdwChangeMode;

//
// VI library registers
//

u16 regs[...], shdwRegs[...];   // see VIFlush
// theese ^^^^^^^ registers actually located there : ---------
                                                              |
&bss[0] struct:                                               |
                                                              |
offset    size    description                 <---------------
-------------------------------------
0000      0x76    regs
0078      0x76    shdwRegs
00F0      0x58    HorVer
-------------------------------------

// "pre"- and "post"- VI interrupt callbacks.
// (note, there is one and only one actual VI master interrupt,
// see intrrupt.txt, there is no stand alone "PRE" VI interrupt
// or "POST" VI either)
static VIRetraceCallback PreCB, PostCB;

//
// thanks N for info :)
//

  VI interrupt raised
        |
        |
        V
  interrupt handler
        |
        |
        V      yes
     pre CB? -----> callback
        |                 |
        | no              |
        V                 |
  update shadows <--------
        |
        |
        V     yes
    post CB? -----> callback
        |                 |
        | no              |
        V                 |
      finish <------------
       

VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback callback)
{
    VIRetraceCallback old = PreCB;
    BOOL level = OSDisableInterrupts();

    PreCB = callback;
    OSRestoreInterrupts(level);

    return old;
}

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback callback)
{
    VIRetraceCallback old = PostCB;
    BOOL level = OSDisableInterrupts();

    PostCB = callback;
    OSRestoreInterrupts(level);

    return old;
}

static volatile u32 retraceCount;
static OSThreadQueue retraceQueue;

void VIWaitForRetrace()
{
    BOOL level = OSDisableInterrupts();
    u32 cnt = retraceCount;

    while(retraceCount == cnt)
    {
        OSSleepThread(&retraceQueue);
    }

    OSRestoreInterrupts(level);
}

u32 flushFlag;

void VIFlush()
{
    BOOL level = OSDisableInterrupts();
    int reg;

    shdwChangeMode |= changeMode;
    changeMode = 0;

    shdwChanged |= changed;

    do
    {   
        // get next bit
        reg = cntlzd(changed);

        // prepare shadows
        shdwRegs[reg] = regs[reg];
    } while(changed <<= 1);

    // ask handler to flush shadows.
    flushFlag = 1;

    OSRestoreInterrupts(level);
}

//
// *** VI INTERRUPT HANDLER ***
// 

__OSExceptionHandler __VIRetraceHandler;

void __VIRetraceHandler(context, exceptionContext);
{
    u32 mask = 0;
    static int dbgCount;

    //
    // gather all VI interrupts (retrace, scan-line, not yet clear),
    // any VI interrupt cause VI master interrupt (in PI).
    // currently SDK have support only for retrace interrupt.
    //

    if((u16)[CC002030] & 0x8000)
    {
        (u16)[CC002030] &= 0x8000;  // clear .... VI interrupt
        mask |= 1;
    }

    if((u16)[CC002034] & 0x8000)
    {
        (u16)[CC002034] &= 0x8000;  // clear .... VI interrupt
        mask |= 2;
    }

    if((u16)[CC002038] & 0x8000)
    {
        (u16)[CC002038] &= 0x8000;  // clear .... VI interrupt
        mask |= 4;
    }

    if((u16)[CC00203C] & 0x8000)
    {
        (u16)[CC00203C] &= 0x8000;  // clear .... VI interrupt
        mask |= 8;
    }

    // ??? until unknown meaning of registers above.
    if(((u16)[CC00203C] & 4) || ((u16)[CC00203C] & 8))
    {
        OSSetCurrentContext(exceptionContext);
        return;
    }

    // this will be fired, when VI master interrupt raised,
    // but other VI interrupts didnt.
    // i.e. should never happen.
    ASSERT(mask == 0);

    // increase vsync counter
    retraceCount++;

    OSClearContext(sp+16);
    OSSetCurrentContext(sp+16);

    // PRE CALLBACK
    if(PreCB) PreCB(retraceCount);

    if(flushFlag)
    {
        dbgCount = 0;

        if(VISetRegs())
        {
            flushFlag = 0;
            __PADRefreshSamplingRate();
        }
    }
    else
    {
        //
        // this happens, when developers dont want to read carefully VI
        // documentation. since, its only fired, when mode changed, its
        // not critical error :)
        //
        if(changed)
        {
            if(dbgCount++ == 60)
            {
                OSReport("Warning: VIFlush() was not called for 60 frames although VI settings were changed");
            }
        }
    }

    OSClearContext(sp+16);

    // POST CALLBACK
    if(PostCB) PostCB(retraceCount);

    // finish VIWaitForRetrace()
    OSWakeupThread(retraceQueue);

    OSClearContext(sp+16);
    OSSetCurrentContext(exceptionContext);
}

// >>>>>> VI ACCESSED ONLY HERE <<<<<<

// and also in VIInit(), of cause.

VITiming *CurrTiming;
u32 CurrTvMode;

int VISetRegs()
{
    int regIndex;

    // check mode
    if(shdwChangeMode == 1)
    {
        if(getCurrentFieldEvenOdd() == 0) return 0;
    }

    while(shdwChanged)
    {
        regIndex = cntlzd(shdwChanged);

        (u16)[CC002000][regIndex] = (u16)shdwRegs[regIndex];

        shdwChanged <<= 1;
    }

    shdwChangeMode = 0;

    CurrTiming = (u32)[bss + 0x0144];
    CurrTvMode = (u32)[bss + 0x0118];

    return 1;
}

u16 taps[] = {
 01f0 01dc
 01ae 0174
 0129 00db
 008e 0046
 000c 00e2
 00cb 00c0
 00c4 00cf
 00de 00ec
 00fc 0008
 000f 0013
 0013 000f
 000c 0008
 0001 0000
};

void VIInit()
{
    encoderType = getEncoderType();     // always 1.

    if(((u16)[CC002002] & 1) == 0)
    {
        __VIInit(0);
    }

    retraceCount = 0;

    changed = shdwChanged = 0;
    changeMode = shdwChangeMode = 0;

    flushFlag = 0;

    //
    // antialiasing stuff
    //

    (u16)[CC00204C] = (taps[1] >> 6) | (taps[2] << 4);
    (u16)[CC00204E] = taps[0] | (taps[1] << 10);
    (u16)[CC002050] = (taps[4] >> 6) | (taps[5] << 4);
    (u16)[CC002052] = taps[3] | (taps[4] << 10);
    (u16)[CC002054] = (taps[7] >> 6) | (taps[8] << 4);
    (u16)[CC002056] = taps[6] | (taps[7] << 10);
    (u16)[CC002058] = taps[11] | (taps[12] << 8);
    (u16)[CC00205A] = taps[ 9] | (taps[10] << 8);
    (u16)[CC00205C] = taps[15] | (taps[16] << 8);
    (u16)[CC00205E] = taps[13] | (taps[14] << 8);
    (u16)[CC002060] = taps[19] | (taps[20] << 8);
    (u16)[CC002062] = taps[17] | (taps[18] << 8);
    (u16)[CC002064] = taps[23] | (taps[24] << 8);
    (u16)[CC002066] = taps[21] | (taps[22] << 8);

    (u16)[CC002070] = 640;

    ImportAdjustingValues();    // WOW! now we know more SRAM variables!

    //
    // Init VI control block structure (bss[0])
    //

    vi.HorVer.nonInter = ((u16)[CC002002] >> 1) & 1;
    vi.HorVer.tv = ((u16)[CC002002] >> 6) & 3;

    vi.HorVer.timing = getTiming(
        vi.HorVer.nonInter +
        (vi.HorVer.tv == VI_DEBUG) ? (0) : (vi.HorVer.tv << 2);

    vi.regs[1] = (u16)[CC002002];

    CurrTiming = vi.HorVer.timing;
    CurrTvMode = vi.HorVer.tv;

    (u16)[bss + 0x00F4] = 640;
    (u16)[bss + 0x00F6] = (u16)CurrTiming[1] * 2;
    (u16)[bss + 0x00F0] = 
        bcc:   a0 1e 00 f4     lhz r0,244(r30)
        bd0:   20 00 02 d0     subfic  r0,r0,720
        bd4:   7c 03 0e 70     srawi   r3,r0,1
        bd8:   7c 63 01 94     addze   r3,r3
        bdc:   b0 7e 00 f0     sth r3,240(r30)
    (u16)[bss + 0x00F2] = 0;
    AdjustPosition((u16)CurrTiming[1]);

    (u16)[bss + 0x0102] = 640;
    (u16)[bss + 0x0104] = (u16)CurrTiming[1] * 2;
    (u16)[bss + 0x0106] = 0;
    (u16)[bss + 0x0108] = 0;
    (u16)[bss + 0x010A] = 640;
    (u16)[bss + 0x010C] = (u16)CurrTiming[1] * 2;
    (u32)[bss + 0x0110] = 0;
    ( u8)[bss + 0x011C] = 0x28;
    ( u8)[bss + 0x011D] = 0x28;
    ( u8)[bss + 0x011E] = 0x28;
    ( u8)[bss + 0x012C] = 0;
    (u32)[bss + 0x0130] = 1;
    (u32)[bss + 0x0134] = 0;

    //
    // install VI retrace handler
    //

    OSInitThreadQueue(&retraceQueue);

    (u16)[CC002030] &= 0x8000;
    (u16)[CC002034] &= 0x8000;

    PreCB = PostCB = 0;

    __OSSetInterruptHandler(VI, __VIRetraceHandler);
    __OSUnmaskInterrupts(VI);
}

s16/u16 displayOffsetH, displayOffsetV;

void ImportAdjustingValues()
{
    OSSram *sram = __OSLockSram();

    // error, if sram == NULL
    ASSERT(sram);

    displayOffsetH = *(s8 *)(sram + 0x10);
    displayOffsetV = 0;

    __OSUnlockSram(0);
}

void __VIInit(VITVMode mode)
{
    VITiming *tm;
    int nonInter, tv, a;
    int encoderType = getEncoderType();
    u16 hct, vct;

    if(encoderType == 0)
    {
        __VIInitPhilips();
    }

    nonInter = mode & 2;
    tv = mode >> 2;

    OSTvMode = tv;      // [800000CC]

    if(encoderType == 0)
    {
        tv = VI_DEBUG;
    }

    tm = getTiming(mode);

    //
    // wait a little
    //

    *(u16 *)(0xCC002002) = 2;
    for(a=0; a<1000; a++) ;
    *(u16 *)(0xCC002002) = 0;

    //
    // setup timing registers
    //

    *(u16 *)(0xCC002006) = tm->hlw;
    *(u16 *)(0xCC002004) = (tm->hcs << 8) | tm->hce;
    *(u16 *)(0xCC00200A) = (tm->hbe640 << 7) | tm->hsy;
    *(u16 *)(0xCC002008) = (tm->hbe640 >> 9) | (tm->hbs640 << 1);

    if(encoderType == 0)
    {
        *(u16 *)(0xCC002072) = (tm->hbeCCIR656 | 0x8000);
        *(u16 *)(0xCC002074) = tm->hbsCCIR656;
    }

    *(u16 *)(0xCC002000) = tm->equ;
    *(u16 *)(0xCC00200E) = tm->prbOdd + tm->acv * 2 - 2;
    *(u16 *)(0xCC00200C) = tm->psbOdd + 2;
    *(u16 *)(0xCC002012) = tm->prbEven + tm->acv * 2 - 2;
    *(u16 *)(0xCC002010) = tm->psbEven + 2;

    *(u16 *)(0xCC002016) = (tm->be1 << 5) | tm->bs1;
    *(u16 *)(0xCC002014) = (tm->be3 << 5) | tm->bs3;
    *(u16 *)(0xCC00201A) = (tm->be2 << 5) | tm->bs2;
    *(u16 *)(0xCC002018) = (tm->be4 << 5) | tm->bs4;

    *(u16 *)(0xCC002048) = 0x2828;
    *(u16 *)(0xCC002036) = 1;
    *(u16 *)(0xCC002034) = 0x1001;

    hct = tm->hlw + 1;
    vct = tm->nhlines * 2 + 1;

    *(u16 *)(0xCC002032) = hct;
    *(u16 *)(0xCC002030) = vct | 0x1000;

    if((mode != 2) || (mode != 3))
    {
        *(u16 *)(0xCC002002) = (tv << 8) | (nonInter << 2) | 1;
        *(u16 *)(0xCC00206C) = 0;
    }
    else
    {
        *(u16 *)(0xCC002002) = (tv << 8) | 5;
        *(u16 *)(0xCC00206C) = 1;
    }
}

//
// predefined "Timing" registers for different modes.
//

static u8 timing[] = {
 0x06,0x00,0x00,0xf0,0x00,0x18,0x00,0x19,       // +0
 0x00,0x03,0x00,0x02,0x0c,0x0d,0x0c,0x0d,
 0x02,0x08,0x02,0x07,0x02,0x08,0x02,0x07,
 0x02,0x0d,0x01,0xad,0x40,0x47,0x69,0xa2,
 0x01,0x75,0x7a,0x00,

 0x01,0x9c,0x06,0x00,0x00,0xf0,0x00,0x18,       // +38
 0x00,0x18,0x00,0x04,0x00,0x04,0x0c,0x0c,
 0x0c,0x0c,0x02,0x08,0x02,0x08,0x02,0x08,
 0x02,0x08,0x02,0x0e,0x01,0xad,0x40,0x47,
 0x69,0xa2,0x01,0x75,

 0x7a,0x00,0x01,0x9c,0x05,0x00,0x01,0x1f,       // +76
 0x00,0x23,0x00,0x24,0x00,0x01,0x00,0x00,
 0x0d,0x0c,0x0b,0x0a,0x02,0x6b,0x02,0x6a,
 0x02,0x69,0x02,0x6c,0x02,0x71,0x01,0xb0,
 0x40,0x4b,0x6a,0xac,

 0x01,0x7c,0x85,0x00,0x01,0xa4,0x05,0x00,       // +114
 0x01,0x1f,0x00,0x21,0x00,0x21,0x00,0x02,
 0x00,0x02,0x0d,0x0b,0x0d,0x0b,0x02,0x6b,
 0x02,0x6d,0x02,0x6b,0x02,0x6d,0x02,0x70,
 0x01,0xb0,0x40,0x4b,

 0x6a,0xac,0x01,0x7c,0x85,0x00,0x01,0xa4,       // +152
 0x06,0x00,0x00,0xf0,0x00,0x18,0x00,0x19,
 0x00,0x03,0x00,0x02,0x10,0x0f,0x0e,0x0d,
 0x02,0x06,0x02,0x05,0x02,0x04,0x02,0x07,
 0x02,0x0d,0x01,0xad,

 0x40,0x4e,0x70,0xa2,0x01,0x75,0x7a,0x00,       // +190
 0x01,0x9c,0x06,0x00,0x00,0xf0,0x00,0x18,
 0x00,0x18,0x00,0x04,0x00,0x04,0x10,0x0e,
 0x10,0x0e,0x02,0x06,0x02,0x08,0x02,0x06,
 0x02,0x08,0x02,0x0e,

 0x01,0xad,0x40,0x4e,0x70,0xa2,0x01,0x75,       // +228
 0x7a,0x00,0x01,0x9c,0x0c,0x00,0x01,0xe0,
 0x00,0x30,0x00,0x30,0x00,0x06,0x00,0x06,
 0x18,0x18,0x18,0x18,0x04,0x0e,0x04,0x0e,
 0x04,0x0e,0x04,0x0e,

 0x04,0x1a,0x01,0xad,0x40,0x47,0x69,0xa2,       // +266
 0x01,0x75,0x7a,0x00,0x01,0x9c,0x0c,0x00,
 0x01,0xe0,0x00,0x2c,0x00,0x2c,0x00,0x0a,
 0x00,0x0a,0x18,0x18,0x18,0x18,0x04,0x0e,
 0x04,0x0e,0x04,0x0e,0x04,0x0e,0x04,0x1a,
 0x01,0xad,0x40,0x47,0x69,0xa8,0x01,0x7b,
 0x7a,0x00,0x01,0x9c
};

// return timing registers.
void *getTiming(VITVMode mode)
{
    u8 *ptr;

    ptr = timing;

    switch(mode)
    {
        //
        // normal modes
        //

        case VI_TVMODE_NTSC_INT:        return ptr;
        case VI_TVMODE_NTSC_DS:         return &ptr[38];
        case VI_TVMODE_PAL_INT:         return &ptr[76];
        case VI_TVMODE_PAL_DS:          return &ptr[114];
        case VI_TVMODE_EURGB60_INT:     return ptr;
        case VI_TVMODE_EURGB60_DS:      return &ptr[38];
        case VI_TVMODE_MPAL_INT:        return &ptr[152];
        case VI_TVMODE_MPAL_DS:         return &ptr[190];

        //
        // specific modes
        //

        case VI_TVMODE_NTSC_PROG:       return &ptr[228];
        case 3 <<-- WTF Panasonic ??:   return &ptr[266];
        case VI_TVMODE_DEBUG_PAL_INT:   return &ptr[76];
        case VI_TVMODE_DEBUG_PAL_DS:    return &ptr[114];

        default:
            break;
    }

    return 0;
}

Render Mode :
-------------

    u32     viTVmode;                 +0
        INTERLACE = 0
        NON-INTERLACE = 1
        PROGRESSIVE = 2
    u16     fbWidth;                  +4
    u16     efbHeight;                +6
    u16     xfbHeight;                +8
    u16     viXOrigin;                +10
    u16     viYOrigin;                +12
    u16     viWidth;                  +14
    u16     viHeight;                 +16
    u32     xFBmode;                  +20
        XFB-SF = 0
        XFB-DF = 1
    u8      field_rendering;          +
    u8      aa;                       +
    u8      sample_pattern[12][2];    +
    u8      vfilter[7];               +

static BOOL FBSet;

void VISetNextFrameBuffer(void *fb)
{
    BOOL level;

    if(fb & 0x1f)
    {
        OSPanic("VISetNextFrameBuffer(): Frame buffer address(0x%08x) is not 32byte aligned", fb);
    }

    level = OSDisableInterrupts();

    vi.HorVer.bufAddr = fb;
    FBSet = 1;

    setFbbRegs(
        &vi.HorVer,
        &vi.HorVer.tfbb,
        &vi.HorVer.bfbb,
        &vi.HorVer.rtfbb,
        &vi.HorVer.rbfbb,
    );

    OSRestoreInterrupts(level);
}

void setFbbRegs(VIHorVer *HorVer, void **tfbb, void **bfbb, void **rtfbb, void **rbfbb)
{
    int shifted;

    calcFbbs(
        HorVer->bufAddr,
        HorVer->PanPosX,
        HorVer->AdjustedPanPosY,
        HorVer->wordPerLine,
        HorVer->FBMode,
        HorVer->AdjustedDispPosY,
        tfbb,
        bfbb
    );

    if(HorVer->threeD)
    {
        calcFbbs(
            HorVer->rbufAddr,
            HorVer->PanPosX,
            HorVer->AdjustedPanPosY,
            HorVer->wordPerLine,
            HorVer->FBMode,
            HorVer->AdjustedDispPosY,
            rtfbb,
            rbfbb
        );
    }

    if( (*tfbb  < 0x01000000) &&
        (*bfbb  < 0x01000000) &&
        (*rtfbb < 0x01000000) &&
        (*rbfbb < 0x01000000) )
    {
        shifted = 0;
    }
    else shifted = 1;

    if(shifted)
    {
        *tfbb  >>= 5;
        *bfbb  >>= 5;
        *rtfbb >>= 5;
        *rbfbb >>= 5;
    }

    vi.regs[14] = (*tfbb >> 16) | (HorVer->xof << 8) | (shifted << 12);
    changed |= bit14;
    vi.regs[15] = (*tfbb & 0xffff);
    changed |= bit15;

    vi.regs[18] = (*bfbb >> 16);
    changed |= bit18;
    vi.regs[19] = (*bfbb & 0xffff);
    changed |= bit19;

    if(HorVer->threeD)
    {
        vi.regs[16] = (*rtfbb >> 16);
        changed |= bit16;
        vi.regs[17] = (*rtfbb & 0xffff);
        changed |= bit17;

        vi.regs[20] = (*rbfbb >> 16);
        changed |= bit20;
        vi.regs[21] = (*rbfbb & 0xffff);
        changed |= bit21;
    }
}

void calcFbbs(
    void    *bufAddr,
    u16     panPosX,
    u16     panPosY,
    u8      wordPerLine,
    u32     xfbMode,
    s16     dispPosY,
    void    **tfbb,
    void    **bfbb)
{
    int bytesPerLine, xoffInWords;
    void *tmp;

    bytesPerLine = wordPerLine * 32;
    xoffInWords  = panPosX * 2;

    *tfbb = bufAddr + (bytesPerLine * panPosY) + xoffInWords;
    *bfbb = (xfbMode == VI_XFBMODE_SF) ? (*tfbb) : (*tfbb + bytesPerLine);

    if((dispPosY * 2 - dispPosY) == 1)
    {
        tmp   = *bfbb;
        *bfbb = *tfbb;
        *tfbb = tmp;
    }

    *tfbb &= 0x3fffffff;
    *bfbb &= 0x3fffffff;
}

void setHorizontalRegs(VITiming *tm, u16 dispPosX, u16 dispSizeX)
{
    int hbe, hbs, hbeHi, hbeLo;

    vi.regs[2] = tm->hcs << 8;
    changed |= bit2;
    vi.regs[3] = tm->hlw;
    changed |= bit3;

    hbe = tm->hbe640 + dispPosX - 40;
    hbs = (720 - dispSizeX) - (tm->hbs640 + dispPosX + 40);
    hbeHi = (hbe & 0x1ff) << 9;
    hbeLo = hbe >> 9;

    vi.regs[4] = hbeLo | (hbs * 2);
    changed |= bit4;
    vi.regs[5] = hbeHi | tm->hsy;
    changed |= bit5;
}

void setInterruptRegs(VITiming *tm)
{
    u16 vct = (tm->nhlines >> 1) + 1;
    u16 hct = ((tm->nhlines >> 1) << 1) - (tm->nhlines);
    int borrow = (hct) ? (0) : (tm->hlw) + 1;

    vi.regs[24] = vct | 0x1000;
    changed |= bit24;
    vi.regs[25] = borrow;
    changed |= bit25;
}
