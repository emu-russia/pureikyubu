// timing
typedef struct
{
    u8      equ;
    u16     acv;
    u16     prbOdd;
    u16     prbEven;
    u16     psbOdd;
    u16     psbEven;    
    u8      bs1;
    u8      bs2;
    u8      bs3;
    u8      bs4;
    u16     be1;
    u16     be2;
    u16     be3;
    u16     be4;
    s16     nhlines;
    u16     hlw;
    u8      hsy;
    u8      hcs;
    u8      hce;
    u8      hbe640;
    u16     hbs640;
    u8      hbeCCIR656;
    u16     hbsCCIR656;
} VITiming;

// timing configuration for every mode
static VITiming timing[8] = { ... };

typedef struct
{
    u16     DispPosX;
    u16     DispPosY;
    u16     DispSizeX;
    u16     DispSizeY;
    s16     AdjustedDispPosX;
    s16     AdjustedDispPosY;
    s16     AdjustedDispSizeY;
    s16     AdjustedPanSizeY;
    u16     FBSizeX;
    u16     FBSizeY;
    u16     PanPosX;
    u16     PanPosY;
    u16     PanSizeX;
    u16     PanSizeY;
    u32     FBMode;
    u32     nonInter;
    u32     tv;
    u8      wordPerLine;
    u8      std;
    u16     wpl;
    void    *bufAddr;
    void    *tfbb;
    void    *bfbb;
    u8      xof;
    u32     black;
    u32     threeD;
    void    *rbufAddr;
    void    *rtfbb;
    void    *rbfbb;
    VITiming *timing; 
} VIHorVer;

// VI Control Block. located at bss...
static struct
{
    u16         regs[59];       // regs are copied to shdwRegs
    u16         shdwRegs[59];   // shdwRegs are copied to hardware registers
    VIHorVer    HorVer;         // used for temporary calculations
} vi;
