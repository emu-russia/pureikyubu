//
// Private GX library data
//

struct __GXData
{
    vNumNot             +0x00
    u16 bpSentNot;              // +0x02
    vNum                +0x04
    vLim                +0x06
    cpEnable            +0x08
    cpStatus            +0x0c
    cpClr               +0x10
    vcdLo               +0x14
    vcdHi               +0x18
    u32 vatA[8];                // +0x1c
    u32 vatB[8];                // +0x3c
    u32 vatC[8];                // +0x5c
    u32 lpSize;                 // +0x7c
    matIdxA             +0x80
    matIdxB             +0x84
    indexBase           +0x88
    indexStride         +0x98
    ambColor            +0xa8
    matColor            +0xb0
    u32 suTs0[8];               // +0xb8
    u32 suTs1[8];               // +0xd8
    u32 suScis0;                // +0xf8
    u32 suScis1;                // +0xfc
    u32 tref[8];                // +0x100
    u32 iref;                   // +0x120
    u32 bpMask;                 // +0x124
    IndTexScale0        +0x128
    IndTexScale1        +0x12c
    u32 tevc[16];               // +0x130
    u32 teva[16];               // +0x170
    u32 tevKsel[8];             // +0x1b0
    u32 cmode0;                 // +0x1d0
    u32 cmode1;                 // +0x1d4
    u32 zmode;                  // +0x1d8
    peCtrl              +0x1dc
    cpDispSrc           +0x1e0
    cpDispSize          +0x1e4
    cpDispStride        +0x1e8
    cpDisp              +0x1ec
    cpTexSrc            +0x1f0
    cpTexSize           +0x1f4
    cpTexStride         +0x1f8
    u32 cpTex;                  // +0x1fc
    cpTexZ              +0x200
    u32 genMode;                // +0x204
    GXTexRegion TexRegions[8];  // +0x208
    GXTexRegion TexRegionsCI[4]; // +0x288
    u32 nextTexRgn;                 // +0x2c8
    u32 nextTexRgnCI;               // +0x2cc
    GXTlutRegion TlutRegions[16+4]; // +0x2d0
    texRegionCallback   +0x410
    tlutRegionCallback  +0x414
    nrmType             +0x418
    hasNrms             +0x41c
    hasBiNrms           +0x41d
    projType            +0x420
    projMtx             +0x424
    vpLeft              +0x43c
    vpTop               +0x440
    vpWd                +0x444
    vpHt                +0x448
    vpNearz             +0x44c
    vpFarz              +0x450
    fgRange             +0x454
    fgSideX             +0x458
    tImage0             +0x45c
    tMode0              +0x47c
    u32 texmapId[16];           // +0x49c
    u32 tcsManEnab;             // +0x4dc
    u32 tevTcEnab;              // +0x4e0
    perf0               +0x4e4
    perf1               +0x4e8
    u32 perfSel;                // +0x4ec
    BOOL inDispList;            // +0x4f0
    BOOL dlSaveContext;         // +0x4f1
    BOOL dirtyVAT;              // +0x4f2
    u32 dirtyState;             // +0x4f4
} gxData;

static __GXData gx;

//
// Verify state
//

struct __GXVerifyData
{
    cb                  +0x00
    verifyLevel         +0x04
    u32 xfRegs[0x5];            // +0x08
    xfMtx               +0x148
    xfNrm               +0x548
    xfDMtx              +0x6c8
    xfLight             +0xac8
    u32 rasRegs[0x100];         // +0xcc8
    BOOL xfRegsDirty[0x50];     // +0x10c8
    xfMtxDirty          +0x1118
    xfNrmDirty          +0x1218
    xfDMtxDirty         +0x1278
    xfLightDirty        +0x1378
} __gxVerif;
