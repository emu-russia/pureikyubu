// GXInit.c (Debug version)
//

//
// TODO : Add neat GX defines instead constants
//

BOOL __GXinBegin;

GXFifoObj FifoObj;

BOOL IsWriteGatherBufferEmpty (void)
{
    __asm {
        sync
        mfspr   r3, WPAR        // WPAR_BNE
        andi.   r3, r3, 1
    }
}

void EnableWriteGatherPipe (void)
{
    u32 hid2 = PPCMfhid2 ();
    PPCMtwpar (0x0C080000);
    PPCMthid2 (hid2 | HID2_WPE);
}

void DisableWriteGatherPipe (void)
{
    u32 hid2 = PPCMfhid2 ();
    PPCMthid2 (hid2 & ~HID2_WPE);
}

GXTexRegion * __GXDefaultTexRegionCallback (GXTexObj* t_obj, GXTexMapID id)
{
    GXTexFmt fmt;
    GXTexRegion texReg;

    fmt = GXGetTexObjFmt ( t_obj );

    switch ( fmt )
    {
        case GX_TF_C4:
        case GX_TF_C8:
        case GX_TF_C14X2:
            texReg = &gx.TexRegionsCI[ gx.nextTexRgnCI & 3];
            gx.nextTexRgnCI++;
            break;

        default:
            texReg = &gx.TexRegions[ gx.nextTexRgn & 7];
            gx.nextTexRgn++;
            break;
    }

    return texReg;
}

GXTlutRegion * __GXDefaultTlutRegionCallback (u32 idx)
{
    if (idx < 20)
    {
        return &gx.TlutRegions[idx];
    }
    else return NULL;
}

void __GXDefaultVerifyCallback (GXWarningLevel level, 
                                u32            id, 
                                char*          msg )
{
    OSReport ( "Level %1d, Warning %3d: %s\n", level, id, msg );
}

GXFifoObj * GXInit (void *base, u32 size)
{
    int i;
    int reg;
    freqBase
    regAddr
    reg1 
    reg2

    gx.inDispList = FALSE;
    gx.dlSaveContext = TRUE;
    __GXinBegin = FALSE;
    gx.tcsManEnab = 0;
    gx.tevTcEnab = 0;
    gx.bpSentNot = 0;

    //
    // Set hardware regs pointers
    // 

    __piReg = OSPhysicalToUncached (0x0C003000);
    __cpReg = OSPhysicalToUncached (0x0C000000);
    __peReg = OSPhysicalToUncached (0x0C001000);
    __memReg = OSPhysicalToUncached (0x0C004000);

    //
    // Init Fifo
    //

    __GXFifoInit ();

    GXInitFifoBase ( &FifoObj, base, size );
    GXSetCPUFifo ( &FifoObj );
    GXSetGPFifo ( &FifoObj );

    __GXPEInit ();

    EnableWriteGatherPipe ();

    gx.genMode = 0;
    gx.genMode = (gx.genMode & 0xffffff) | 0x00000000;

    gx.bpMask = 0xff;
    gx.bpMask = (gx.bpMask & 0xffffff) | 0xF0000000;

    gx.lpSize = 0;
    gx.lpSize = (gx.lpSize & 0xffffff) | 0x22000000;

    //
    // Loop 1: Setup TEV
    //

    for (i=0; i<16; i++)
    {
        gx.tevc[i] = 0;
        gx.teva[i] = 0;
        gx.tref[i / 2] = 0;
        gx.texmapId[i] = 0xff;

        reg = (0xC0 + i * 2);
        if ( reg & 0xffffff00 )
            OSHalt ( "GX Internal: Register field out of range" );
        gx.tevc[i] = (gx.tevc[i] & 0xffffff) | ((0xC0 + i * 2) << 24);

        reg = (0xC1 + i * 2);
        if ( reg & 0xffffff00 )
            OSHalt ( "GX Internal: Register field out of range" );
        gx.teva[i] = (gx.teva[i] & 0xffffff) | ((0xC1 + i * 2) << 24);

        reg = (0xF6 + i / 2);
        if ( reg & 0xffffff00 )
            OSHalt ( "GX Internal: Register field out of range" );
        gx.tevKsel[i/2] = (gx.tevKsel[i/2] & 0xffffff) | ((0xF6 + i / 2) << 24);

        reg = (0x28 + i / 2);
        if ( reg & 0xffffff00 )
            OSHalt ( "GX Internal: Register field out of range" );
        gx.tref[i/2] = (gx.tref[i/2] & 0xffffff) | ((0x28 + i / 2) << 24);
    }

    gx.iref = 0;
    gx.iref = (gx.iref & 0xffffff) | 0x27000000;

    //
    // Loop 2: SU
    //

    for (i=0; i<8; i++)
    {
        gx.suTs0[i] = 0;
        gx.suTs1[i] = 0;

        reg = (0x30 + i * 2);
        if ( reg & 0xffffff00 )
            OSHalt ( "GX Internal: Register field out of range" );
        gx.suTs0[i] = (gx.suTs0[i] & 0xffffff) | ((0x30 + i * 2) << 24);

        reg = (0x31 + i * 2);
        if ( reg & 0xffffff00 )
            OSHalt ( "GX Internal: Register field out of range" );        
        gx.suTs1[i] = (gx.suTs1[i] & 0xffffff) | ((0x31 + i * 2) << 24);
    }

    gx.suScis0 = (gx.suScis0 & 0xffffff) | 0x20000000;
    gx.suScis1 = (gx.suScis1 & 0xffffff) | 0x21000000;

    gx.cmode0 = (gx.cmode0 & 0xffffff) | 0x41000000;
    gx.cmode1 = (gx.cmode1 & 0xffffff) | 0x42000000;
    gx.zmode = (gx.zmode & 0xffffff) | 0x43000000;

    gx.cpTex &= ~0x180;         // rlwinm cpTex, 25, 22

    gx.dirtyState = 0;
    gx.dirtyVAT = FALSE;

    //
    // Verifier Init
    //

    __gxVerif.verifyLevel = GX_WARN_ALL;

    GXSetVerifyCallback ( __GXDefaultVerifyCallback );

    for (i=0; i<0x100; i++)
    {
        __gxVerif.rasRegs[i] = (__gxVerif.rasRegs[i] & 0xffffff) | 0xFF000000;
    }

    memset ( __gxVerif.xfRegsDirty, 0, 0x50 );
    memset ( __gxVerif.xfMtxDirty, 0, 0x100 );
    memset ( __gxVerif.xfNrmDirty, 0, 0x60 );
    memset ( __gxVerif.xfLightDirty, 0, 0x80 );

    r20 = __OSBusClock / 500;

    __GXFlushTextureState ();

    r3 = (r20 >> 11) | 0x400;
    r26 = r3 | 0x69000000;

    GXWGFifo.u8 = 0x61;
    GXWGFifo.u32 = r26;
    __gxVerif.rasRegs[r26 >> 24] = r26;

    __GXFlushTextureState ();

    r3 = r20 / 0x1080;
    r26 = r3 | 0x200 | 0x46000000;

    GXWGFifo.u8 = 0x61;
    GXWGFifo.u32 = r26;
    __gxVerif.rasRegs[r26 >> 24] = r26;

    //
    // VAT
    //

    for (i=0; i<8; i++)
    {
        gx.vatA[i] = (gx.vatA[i] & ~0xC0000000) | 0x40000000;
        gx.vatB[i] = (gx.vatB[i] & ~0x80000000) | 0x80000000;

        GXWGFifo.u8 = 4;
        GXWGFifo.u8 = 0x80 | i;
        GXWGFifo.u32 = vatB[i];
    }

    //
    // Chunk 1
    //

    r29 = 0;

    r29 |= 1;
    r29 |= 2;
    r29 |= 4;
    r29 |= 8;
    r29 |= 0x10;
    r29 |= 0x20;

    GXWGFifo.u8 = 0x10;
    GXWGFifo.u32 = 0x1000;
    GXWGFifo.u32 = r29;

    reg = 0;
    if ( reg >= 0 && reg < 0x50)
    {
        __gxVerif.xfRegs[reg] = r29;
        __gxVerif.xfRegsDirty[reg] = TRUE;
    }

    //
    // Chunk 2
    //

    r24 = 0;

    r24 |= 1;

    GXWGFifo.u8 = 0x10;
    GXWGFifo.u32 = 0x1012;      // XF_DUALTEX
    GXWGFifo.u32 = r29;

    reg = 0x12;
    if ( reg >= 0 && reg < 0x50)
    {
        __gxVerif.xfRegs[reg] = r29;
        __gxVerif.xfRegsDirty[reg] = TRUE;
    }

    xfRegsDirty[0] = FALSE;

    //
    // Chunk 3
    //

    r27 = 0;

    r27 |= 1;
    r27 |= 2;
    r27 |= 4;
    r27 |= 8;
    r27 |= 0x58000000;

    GXWGFifo.u8 = 0x61;
    GXWGFifo.u32 = r27;
    __gxVerif.rasRegs[r27 >> 24] = r27;

    //
    // TexCache
    //

    for (i=0; i<8; i++)
    {
        GXInitTexCacheRegion ( &gx.TexRegions[i],
                               GX_FALSE,
                               i << 15,
                               GX_TEXCACHE_32K,
                               (i << 15) + 8,
                               GX_TEXCACHE_32K );
    }

    for (i=0; i<4; i++)
    {
        GXInitTexCacheRegion ( &gx.TexRegionsCI[i],
                               GX_FALSE,
                               ((i << 1) + 8) << 15,
                               GX_TEXCACHE_32K,
                               ((i << 1) + 9) << 15,
                               GX_TEXCACHE_32K );
    }

    //
    // TlutRegion
    //

    for (i=0; i<0x10; i++)
    {
        GXInitTlutRegion ( &gx.TlutRegions[i], 
                           (i << 13) + 0xc0000,
                           GX_TLUT_256 );
    }

    for (i=0; i<4; i++)
    {
        GXInitTlutRegion ( &gx.TlutRegions[16 + i], 
                           (i << 15) + 0xe0000,
                           GX_TLUT_1K );
    }

    r30 = 0;
    u16.[__cpReg + 6] = (u16)r30;

    gx.perfSel = gx.perfSel & ~0xF0;        // rlwinm    r0, r0, 0,28,23

    GXWGFifo.u8 = 8;
    GXWGFifo.u8 = 0x20;
    GXWGFifo.u32 = gx.perfSel;

    GXWGFifo.u8 = 0x10;
    GXWGFifo.u32 = 0x1006;
    GXWGFifo.u32 = 0;

    reg = 6;
    if ( reg >= 0 && reg < 0x50)
    {
        __gxVerif.xfRegs[reg] = r29;
        __gxVerif.xfRegsDirty[reg] = TRUE;
    }

    r30 = 0x23000000;

    GXWGFifo.u8 = 0x61;
    GXWGFifo.u32 = r30;
    __gxVerif.rasRegs[r30 >> 24] = r30;

    r30 = 0x24000000;
    GXWGFifo.u32 = r30;
    __gxVerif.rasRegs[r30 >> 24] = r30;

    r30 = 0x67000000;
    GXWGFifo.u32 = r30;
    __gxVerif.rasRegs[r30 >> 24] = r30;

    __GXSetTmemConfig (0);

    __GXInitGX ();

    return &FifoObj;
}

void __GXInitGX (void)
{
    GXRenderModeObj * rmode;
    f32 identity_mtx[3][4];
    GXColor clear;
    black;
    white;
    int i;

    var_50 = 0x404040FF;
    var_54 = 0;
    var_58 = 0xFFFFFFFF;

    switch (VIGetTvFormat ())
    {
        case VI_NTSC:
            rmode = &GXNtsc480IntDf;
            break;

        case VI_PAL:
            rmode = &GXPal528IntDf;
            break;            

        case VI_MPAL:
            rmode = &GXMpal480IntDf;
            break;

        case VI_EURGB60:
            rmode = &GXEurgb60Hz480IntDf;
            break;

        default:
            OSHalt ( "GXInit: invalid TV format" );
    }

    clear = var_50;

    GXSetCopyClear ( clear, 0xFFFFFF );

    //
    // TexGen
    //

    GXSetTexCoordGen ( GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY );
    GXSetTexCoordGen ( GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1, GX_IDENTITY );
    GXSetTexCoordGen ( GX_TEXCOORD2, GX_TG_MTX2x4, GX_TG_TEX2, GX_IDENTITY );
    GXSetTexCoordGen ( GX_TEXCOORD3, GX_TG_MTX2x4, GX_TG_TEX3, GX_IDENTITY );
    GXSetTexCoordGen ( GX_TEXCOORD4, GX_TG_MTX2x4, GX_TG_TEX4, GX_IDENTITY );
    GXSetTexCoordGen ( GX_TEXCOORD5, GX_TG_MTX2x4, GX_TG_TEX5, GX_IDENTITY );
    GXSetTexCoordGen ( GX_TEXCOORD6, GX_TG_MTX2x4, GX_TG_TEX6, GX_IDENTITY );
    GXSetTexCoordGen ( GX_TEXCOORD7, GX_TG_MTX2x4, GX_TG_TEX7, GX_IDENTITY );

    GXSetNumTexGens (1);

    //
    // Vtx
    //

    GXClearVtxDesc ();
    GXInvalidateVtxCache ();

    for (i=GX_VA_POS; i<=GX_LIGHT_ARRAY; i++)
    {
        GXSetArray (i, &gx, 0);
    }

    //
    // Geom width
    //

    GXSetLineWidth (6, GX_TO_ZERO);
    GXSetPointSize (6, GX_TO_ZERO);

    GXEnableTexOffsets (GX_TEXCOORD0, FALSE, FALSE);
    GXEnableTexOffsets (GX_TEXCOORD1, FALSE, FALSE);
    GXEnableTexOffsets (GX_TEXCOORD2, FALSE, FALSE);
    GXEnableTexOffsets (GX_TEXCOORD3, FALSE, FALSE);
    GXEnableTexOffsets (GX_TEXCOORD4, FALSE, FALSE);
    GXEnableTexOffsets (GX_TEXCOORD5, FALSE, FALSE);
    GXEnableTexOffsets (GX_TEXCOORD6, FALSE, FALSE);
    GXEnableTexOffsets (GX_TEXCOORD7, FALSE, FALSE);

    //
    // Pos/Normal matrix
    //

    identity_mtx[0][0] = 1.0;
    identity_mtx[0][1] = 0.0;
    identity_mtx[0][2] = 0.0;
    identity_mtx[0][3] = 0.0;

    identity_mtx[1][0] = 0.0;
    identity_mtx[1][1] = 1.0;
    identity_mtx[1][2] = 0.0;
    identity_mtx[1][3] = 0.0;

    identity_mtx[2][0] = 0.0;
    identity_mtx[2][1] = 0.0;
    identity_mtx[2][2] = 1.0;
    identity_mtx[2][3] = 0.0;

    GXLoadPosMtxImm ( identity_mtx, GX_PNMTX0 );
    GXLoadNrmMtxImm ( identity_mtx, GX_PNMTX0 );
    GXSetCurrentMtx ( GX_PNMTX0 );

    GXLoadTexMtxImm ( identity_mtx, GX_IDENTITY, GX_MTX3x4 );
    GXLoadTexMtxImm ( identity_mtx, GX_PTIDENTITY, GX_MTX3x4 );

    //
    // Culling
    //

    GXSetViewport ( 0.0, 0.0, (f32)rmode->fbWidth, (f32)rmode->xfbHeight, 0.0, 1.0 );
    GXSetCoPlanar (GX_FALSE);
    GXSetCullMode (GX_CULL_BACK);
    GXSetClipMode (GX_CLIP_ENABLE);
    GXSetScissor ( 0, 0, rmode->fbWidth, rmode->efbHeight );
    GXSetScissorBoxOffset (0, 0);

    //
    // Lighting
    //

    GXSetNumChans (0);
    GXSetChanCtrl ( 4, 0, 0, 1, 0, 0, 2);
    var_60 = var_54;
    GXSetChanAmbColor (4, var_60);
    var_64 = var_58;
    GXSetChanMatColor (4, var_64);
    GXSetChanCtrl ( 5, 0, 0, 1, 0, 0, 2);
    var_68 = var_54;
    GXSetChanAmbColor (5, var_68);
    var_6C = var_58;
    GXSetChanMatColor (5, var_6C);

    //
    // Texture
    //

    GXInvalidateTexAll ();

    gx.nextTexRgn = 0;
    gx.nextTexRgnCI = 0;

    GXSetTexRegionCallback (__GXDefaultTexRegionCallback);
    GXSetTlutRegionCallback (__GXDefaultTlutRegionCallback);

    //
    // Tev
    //

    GXSetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GXSetTevOrder (GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR0A0);
    GXSetTevOrder (GX_TEVSTAGE2, GX_TEXCOORD2, GX_TEXMAP2, GX_COLOR0A0);
    GXSetTevOrder (GX_TEVSTAGE3, GX_TEXCOORD3, GX_TEXMAP3, GX_COLOR0A0);
    GXSetTevOrder (GX_TEVSTAGE4, GX_TEXCOORD4, GX_TEXMAP4, GX_COLOR0A0);
    GXSetTevOrder (GX_TEVSTAGE5, GX_TEXCOORD5, GX_TEXMAP5, GX_COLOR0A0);
    GXSetTevOrder (GX_TEVSTAGE6, GX_TEXCOORD6, GX_TEXMAP6, GX_COLOR0A0);
    GXSetTevOrder (GX_TEVSTAGE7, GX_TEXCOORD7, GX_TEXMAP7, GX_COLOR0A0);
    GXSetTevOrder (GX_TEVSTAGE8, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevOrder (GX_TEVSTAGE9, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevOrder (GX_TEVSTAGE10, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevOrder (GX_TEVSTAGE11, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevOrder (GX_TEVSTAGE12, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevOrder (GX_TEVSTAGE13, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevOrder (GX_TEVSTAGE14, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevOrder (GX_TEVSTAGE15, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);

    GXSetNumTevStages (1);

    GXSetTevOp (0, 3);

    GXSetAlphaCompare (7, 0, 0, 7, 0);

    GXSetZTexture (0, 0x11, 0);

    for (i=0; i<16; i++)
    {
        GXSetTevKColorSel (i, 6);
        GXSetTevKAlphaSel (i, 0);
        GXSetTevSwapMode (i, 0, 0);
    }

    GXSetTevSwapModeTable (0, 0, 1, 2, 3);
    GXSetTevSwapModeTable (1, 0, 0, 0, 3);
    GXSetTevSwapModeTable (2, 1, 1, 1, 3);
    GXSetTevSwapModeTable (3, 2, 2, 2, 3);

    for (i=0; i<16; i++)
    {
        GXSetTevDirect (i);
    }

    //
    // Indirect texturing
    //

    GXSetNumIndStages (0);
    GXSetIndTexCoordScale (0, 0, 0);
    GXSetIndTexCoordScale (1, 0, 0);
    GXSetIndTexCoordScale (2, 0, 0);
    GXSetIndTexCoordScale (3, 0, 0);

    //
    // Pixel engine
    //

    var_70 = var_54;
    GXSetFog ( 0.0, 1.0, 0.1, 1.0, var_70);
    GXSetFogRangeAdj (0, 0, 0);
    GXSetBlendMode (0, 4, 5, 0);
    GXSetColorUpdate (1);
    GXSetAlphaUpdate (1);
    GXSetZMode (1, 3, 1);
    GXSetZCompLoc (1);
    GXSetDither (1);
    GXSetDstAlpha (0, 0);
    GXSetPixelFmt (0, 0);
    GXSetFieldMask (1, 1);
    GXSetFieldMode ( rmode->field_rendering, 
                     rmode->viHeight != rmode->xfbHeight ? 0 : 1 );
    GXSetDispCopySrc (0, 0, rmode->fbWidth, rmode->efbHeight);
    GXSetDispCopyDst (rmode->fbWidth, rmode->efbHeight);
    GXSetDispCopyYScale ( (f32)rmode->xfbHeight / (f32)rmode->efbHeight );
    GXSetCopyClamp (3);
    GXSetCopyFilter (rmode->aa, rmode->sample_pattern, 1, rmode->vfilter);
    GXSetDispCopyGamma (0);
    GXSetDispCopyFrame2Field (0);
    GXClearBoundingBox ();

    //
    // Efb Copy
    //

    GXPokeColorUpdate (1);
    GXPokeAlphaUpdate (1);
    GXPokeDither (0);
    GXPokeBlendMode (0, 0, 1, 0xF);
    GXPokeAlphaMode (7, 0);
    GXPokeAlphaRead (1);
    GXPokeDstAlpha (0, 0);
    GXPokeZMode (1, 7, 1);
    
    GXSetGPMetric (GX_PERF0_NONE, GX_PERF1_NONE);
    GXClearGPMetric ();
}
