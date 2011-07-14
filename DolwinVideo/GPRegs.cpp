// all gfx state registers here
#include "GX.h"

static      FILE *gplog;

long            *peDrawDone, *peToken;
unsigned short  *tokenVal;

void GXSetTokens(long *drawdone, long *token, unsigned short *val)
{
    peDrawDone = drawdone;
    peToken = token;
    tokenVal = val;
}

CPMemory    cpRegs;
BPMemory    bpRegs;
XFMemory    xfRegs;

// emulator main memory buffer (RAM)
u8          *RAM;

u32         cpLoads, bpLoads, xfLoads;

// ---------------------------------------------------------------------------

// index range = 00..FF
// reg size = 32 bit
void loadCPReg(unsigned index, u32 value)
{
    cpLoads++;

    //GFXError("unknown CP load, index: %02X, data: %08X\n", index, value);

    switch(index)
    {
        case CP_MATIDX_A:
        {
            cpRegs.matidxA.matidx = value;
            //GFXError("cp posidx : %i", cpRegs.matidxA.pos);
        }
        return;

        case CP_MATIDX_B:
        {
            cpRegs.matidxB.matidx = value;
        }
        return;

        case CP_VCD_LO:
        {
            cpRegs.vcdLo.vcdlo = value;

            // update pipeline, using all VATs
            for(unsigned vatnum=0; vatnum<8; vatnum++)
            {
                FifoReconfigure(
                    VTX_POSMATIDX,
                    vatnum,
                    cpRegs.vcdLo.pmidx,
                    0, 0, 0
                );

                FifoReconfigure(
                    VTX_TEX0MTXIDX,
                    vatnum,
                    cpRegs.vcdLo.t0midx,
                    0, 0, 0
                );

                FifoReconfigure(
                    VTX_TEX1MTXIDX,
                    vatnum,
                    cpRegs.vcdLo.t1midx,
                    0, 0, 0
                );

                FifoReconfigure(
                    VTX_TEX2MTXIDX,
                    vatnum,
                    cpRegs.vcdLo.t2midx,
                    0, 0, 0
                );

                FifoReconfigure(
                    VTX_TEX3MTXIDX,
                    vatnum,
                    cpRegs.vcdLo.t3midx,
                    0, 0, 0
                );

                FifoReconfigure(
                    VTX_TEX4MTXIDX,
                    vatnum,
                    cpRegs.vcdLo.t4midx,
                    0, 0, 0
                );

                FifoReconfigure(
                    VTX_TEX5MTXIDX,
                    vatnum,
                    cpRegs.vcdLo.t5midx,
                    0, 0, 0
                );

                FifoReconfigure(
                    VTX_TEX6MTXIDX,
                    vatnum,
                    cpRegs.vcdLo.t6midx,
                    0, 0, 0
                );

                FifoReconfigure(
                    VTX_TEX7MTXIDX,
                    vatnum,
                    cpRegs.vcdLo.t7midx,
                    0, 0, 0
                );

                FifoReconfigure(
                    VTX_POS,
                    vatnum,
                    cpRegs.vcdLo.pos,
                    cpRegs.vatA[vatnum].poscnt,
                    cpRegs.vatA[vatnum].posfmt,
                    cpRegs.vatA[vatnum].posshft
                );

                FifoReconfigure(
                    VTX_NRM,
                    vatnum,
                    cpRegs.vcdLo.nrm,
                    cpRegs.vatA[vatnum].nrmcnt,
                    cpRegs.vatA[vatnum].nrmfmt,
                    0
                );

                FifoReconfigure(
                    VTX_COLOR0,
                    vatnum,
                    cpRegs.vcdLo.col0,
                    cpRegs.vatA[vatnum].col0cnt,
                    cpRegs.vatA[vatnum].col0fmt,
                    0
                );

                FifoReconfigure(
                    VTX_COLOR1,
                    vatnum,
                    cpRegs.vcdLo.col1,
                    cpRegs.vatA[vatnum].col1cnt,
                    cpRegs.vatA[vatnum].col1fmt,
                    0
                );
            }
        }
        return;

        case CP_VCD_HI:
        {
            cpRegs.vcdHi.vcdhi = value;

            // update pipeline, using all VATs
            for(unsigned vatnum=0; vatnum<8; vatnum++)
            {
                FifoReconfigure(
                    VTX_TEXCOORD0,
                    vatnum,
                    cpRegs.vcdHi.tex0,
                    cpRegs.vatA[vatnum].tex0cnt,
                    cpRegs.vatA[vatnum].tex0fmt,
                    cpRegs.vatA[vatnum].tex0shft
                );

                FifoReconfigure(
                    VTX_TEXCOORD1,
                    vatnum,
                    cpRegs.vcdHi.tex1,
                    cpRegs.vatB[vatnum].tex1cnt,
                    cpRegs.vatB[vatnum].tex1fmt,
                    cpRegs.vatB[vatnum].tex1shft
                );
            
                FifoReconfigure(
                    VTX_TEXCOORD2,
                    vatnum,
                    cpRegs.vcdHi.tex2,
                    cpRegs.vatB[vatnum].tex2cnt,
                    cpRegs.vatB[vatnum].tex2fmt,
                    cpRegs.vatB[vatnum].tex2shft
                );

                FifoReconfigure(
                    VTX_TEXCOORD3,
                    vatnum,
                    cpRegs.vcdHi.tex3,
                    cpRegs.vatB[vatnum].tex3cnt,
                    cpRegs.vatB[vatnum].tex3fmt,
                    cpRegs.vatB[vatnum].tex3shft
                );

                FifoReconfigure(
                    VTX_TEXCOORD4,
                    vatnum,
                    cpRegs.vcdHi.tex4,
                    cpRegs.vatB[vatnum].tex4cnt,
                    cpRegs.vatB[vatnum].tex4fmt,
                    cpRegs.vatC[vatnum].tex4shft
                );

                FifoReconfigure(
                    VTX_TEXCOORD5,
                    vatnum,
                    cpRegs.vcdHi.tex5,
                    cpRegs.vatC[vatnum].tex5cnt,
                    cpRegs.vatC[vatnum].tex5fmt,
                    cpRegs.vatC[vatnum].tex5shft
                );

                FifoReconfigure(
                    VTX_TEXCOORD6,
                    vatnum,
                    cpRegs.vcdHi.tex6,
                    cpRegs.vatC[vatnum].tex6cnt,
                    cpRegs.vatC[vatnum].tex6fmt,
                    cpRegs.vatC[vatnum].tex6shft
                );

                FifoReconfigure(
                    VTX_TEXCOORD7,
                    vatnum,
                    cpRegs.vcdHi.tex7,
                    cpRegs.vatC[vatnum].tex7cnt,
                    cpRegs.vatC[vatnum].tex7fmt,
                    cpRegs.vatC[vatnum].tex7shft
                );
            }
        }
        return;

        case CP_VAT0_A:
        case CP_VAT1_A:
        case CP_VAT2_A:
        case CP_VAT3_A:
        case CP_VAT4_A:
        case CP_VAT5_A:
        case CP_VAT6_A:
        case CP_VAT7_A:
        {
            unsigned vatnum = index & 7;
            cpRegs.vatA[vatnum].vata = value;

            FifoReconfigure(
                VTX_POS,
                vatnum,
                cpRegs.vcdLo.pos,
                cpRegs.vatA[vatnum].poscnt,
                cpRegs.vatA[vatnum].posfmt,
                cpRegs.vatA[vatnum].posshft
            );

            FifoReconfigure(
                VTX_NRM,
                vatnum,
                cpRegs.vcdLo.nrm,
                cpRegs.vatA[vatnum].nrmcnt,
                cpRegs.vatA[vatnum].nrmfmt,
                0
            );

            FifoReconfigure(
                VTX_COLOR0,
                vatnum,
                cpRegs.vcdLo.col0,
                cpRegs.vatA[vatnum].col0cnt,
                cpRegs.vatA[vatnum].col0fmt,
                0
            );

            FifoReconfigure(
                VTX_COLOR1,
                vatnum,
                cpRegs.vcdLo.col1,
                cpRegs.vatA[vatnum].col1cnt,
                cpRegs.vatA[vatnum].col1fmt,
                0
            );

            FifoReconfigure(
                VTX_TEXCOORD0,
                vatnum,
                cpRegs.vcdHi.tex0,
                cpRegs.vatA[vatnum].tex0cnt,
                cpRegs.vatA[vatnum].tex0fmt,
                cpRegs.vatA[vatnum].tex0shft
            );
        }
        return;

        case CP_VAT0_B:
        case CP_VAT1_B:
        case CP_VAT2_B:
        case CP_VAT3_B:
        case CP_VAT4_B:
        case CP_VAT5_B:
        case CP_VAT6_B:
        case CP_VAT7_B:
        {
            unsigned vatnum = index & 7;
            cpRegs.vatB[vatnum].vatb = value;

            FifoReconfigure(
                VTX_TEXCOORD1,
                vatnum,
                cpRegs.vcdHi.tex1,
                cpRegs.vatB[vatnum].tex1cnt,
                cpRegs.vatB[vatnum].tex1fmt,
                cpRegs.vatB[vatnum].tex1shft
            );
            
            FifoReconfigure(
                VTX_TEXCOORD2,
                vatnum,
                cpRegs.vcdHi.tex2,
                cpRegs.vatB[vatnum].tex2cnt,
                cpRegs.vatB[vatnum].tex2fmt,
                cpRegs.vatB[vatnum].tex2shft
            );

            FifoReconfigure(
                VTX_TEXCOORD3,
                vatnum,
                cpRegs.vcdHi.tex3,
                cpRegs.vatB[vatnum].tex3cnt,
                cpRegs.vatB[vatnum].tex3fmt,
                cpRegs.vatB[vatnum].tex3shft
            );

            FifoReconfigure(
                VTX_TEXCOORD4,
                vatnum,
                cpRegs.vcdHi.tex4,
                cpRegs.vatB[vatnum].tex4cnt,
                cpRegs.vatB[vatnum].tex4fmt,
                cpRegs.vatC[vatnum].tex4shft
            );
        }
        return;

        case CP_VAT0_C:
        case CP_VAT1_C:
        case CP_VAT2_C:
        case CP_VAT3_C:
        case CP_VAT4_C:
        case CP_VAT5_C:
        case CP_VAT6_C:
        case CP_VAT7_C:
        {
            unsigned vatnum = index & 7;
            cpRegs.vatC[vatnum].vatc = value;

            FifoReconfigure(
                VTX_TEXCOORD4,
                vatnum,
                cpRegs.vcdHi.tex4,
                cpRegs.vatB[vatnum].tex4cnt,
                cpRegs.vatB[vatnum].tex4fmt,
                cpRegs.vatC[vatnum].tex4shft
            );

            FifoReconfigure(
                VTX_TEXCOORD5,
                vatnum,
                cpRegs.vcdHi.tex5,
                cpRegs.vatC[vatnum].tex5cnt,
                cpRegs.vatC[vatnum].tex5fmt,
                cpRegs.vatC[vatnum].tex5shft
            );

            FifoReconfigure(
                VTX_TEXCOORD6,
                vatnum,
                cpRegs.vcdHi.tex6,
                cpRegs.vatC[vatnum].tex6cnt,
                cpRegs.vatC[vatnum].tex6fmt,
                cpRegs.vatC[vatnum].tex6shft
            );

            FifoReconfigure(
                VTX_TEXCOORD7,
                vatnum,
                cpRegs.vcdHi.tex7,
                cpRegs.vatC[vatnum].tex7cnt,
                cpRegs.vatC[vatnum].tex7fmt,
                cpRegs.vatC[vatnum].tex7shft
            );
        }
        return;

        case CP_ARRAY_BASE | (VTX_POS - VTX_POS):           // 0xA0
        case CP_ARRAY_BASE | (VTX_NRM - VTX_POS):           // 0xA1
        case CP_ARRAY_BASE | (VTX_COLOR0 - VTX_POS):        // 0xA2
        case CP_ARRAY_BASE | (VTX_COLOR1 - VTX_POS):        // 0xA3
        case CP_ARRAY_BASE | (VTX_TEXCOORD0 - VTX_POS):     // 0xA4
        case CP_ARRAY_BASE | (VTX_TEXCOORD1 - VTX_POS):     // 0xA5
        case CP_ARRAY_BASE | (VTX_TEXCOORD2 - VTX_POS):     // 0xA6
        case CP_ARRAY_BASE | (VTX_TEXCOORD3 - VTX_POS):     // 0xA7
        case CP_ARRAY_BASE | (VTX_TEXCOORD4 - VTX_POS):     // 0xA8
        case CP_ARRAY_BASE | (VTX_TEXCOORD5 - VTX_POS):     // 0xA9
        case CP_ARRAY_BASE | (VTX_TEXCOORD6 - VTX_POS):     // 0xAA
        case CP_ARRAY_BASE | (VTX_TEXCOORD7 - VTX_POS):     // 0xAB
        {
            // array base is already translated
            cpRegs.arbase[VTX_POS + (index & 0xf)] = (u8 *)&RAM[value & RAMMASK];
        }
        return;

        case CP_ARRAY_STRIDE | (VTX_POS - VTX_POS):         // 0xB0
        case CP_ARRAY_STRIDE | (VTX_NRM - VTX_POS):         // 0xB1
        case CP_ARRAY_STRIDE | (VTX_COLOR0 - VTX_POS):      // 0xB2
        case CP_ARRAY_STRIDE | (VTX_COLOR1 - VTX_POS):      // 0xB3
        case CP_ARRAY_STRIDE | (VTX_TEXCOORD0 - VTX_POS):   // 0xB4
        case CP_ARRAY_STRIDE | (VTX_TEXCOORD1 - VTX_POS):   // 0xB5
        case CP_ARRAY_STRIDE | (VTX_TEXCOORD2 - VTX_POS):   // 0xB6
        case CP_ARRAY_STRIDE | (VTX_TEXCOORD3 - VTX_POS):   // 0xB7
        case CP_ARRAY_STRIDE | (VTX_TEXCOORD4 - VTX_POS):   // 0xB8
        case CP_ARRAY_STRIDE | (VTX_TEXCOORD5 - VTX_POS):   // 0xB9
        case CP_ARRAY_STRIDE | (VTX_TEXCOORD6 - VTX_POS):   // 0xBA
        case CP_ARRAY_STRIDE | (VTX_TEXCOORD7 - VTX_POS):   // 0xBB
        {
            cpRegs.arstride[VTX_POS + (index & 0xf)] = value;
        }
        return;

        default:
        {
#ifdef  GPLOG
            fprintf(gplog, "unknown CP load, index: %02X, data: %08X\n", index, value);
            fflush(gplog);
#endif
//            GFXError("unknown CP load, index : %02X", index);
        }
    }
}

static void tryLoadTex(int id)
{
    if(bpRegs.valid[0][id] && bpRegs.valid[3][id])
    {
        bpRegs.valid[0][id] =
        bpRegs.valid[3][id] = FALSE;

#ifndef WIREFRAME
        LoadTexture(
            bpRegs.teximg3[id].base << 5,
            id,
            bpRegs.teximg0[id].fmt,
            bpRegs.teximg0[id].width + 1,
            bpRegs.teximg0[id].height + 1
        );
#endif
    }
}

// index range = 00..FF
// reg size = 24 bit (value is already masked)
void loadBPReg(unsigned index, u32 value)
{
    static Color    copyClearRGBA;
    static u32      copyClearZ;

    bpLoads++;

    //GFXError("unknown BP load, index: %02X, data: %08X\n", index, value);

    switch(index)
    {
        // draw done
        case PE_DONE:
        {
            GPFrameDone();
            *peDrawDone = 1;
        }
        return;

        // token
        case PE_TOKEN_INT:
        {
            bpRegs.tokint = (u16)value;
        }
        return;

        case PE_TOKEN:
        {
            *tokenVal = (u16)value;
            if(bpRegs.tokint == *tokenVal)
            {
                *peToken = 1;
            }
        }
        return;

        //
        // gen mode
        // application : clipping
        //

        case BP_GEN_MODE:
        {
            static int cull_modes[4] = { 
                GFX_CULL_NONE,
                GFX_CULL_BACK,
                GFX_CULL_FRONT,
                GFX_CULL_ALL,
            };
            bpRegs.genmode.hex = value;
            //GFXError("%i", bpRegs.genmode.cull);

            gfx->SetCullMode(cull_modes[bpRegs.genmode.cull]);
        }
        return;

        //
        // set scissor box
        //

        case BP_SU_SCIS0:
        {
            int x, y, w, h;

            bpRegs.scis0.scis0 = value;

            x = bpRegs.scis0.sux - 342;
            y = bpRegs.scis0.suy - 342;
            w = bpRegs.scis1.suw - bpRegs.scis0.sux + 1;
            h = bpRegs.scis1.suh - bpRegs.scis0.suy + 1;

            //GFXError("scissor (%i, %i)-(%i, %i)", x, y, w, h);
            gfx->SetScissor(x, y, w, h);
        }
        return;
        
        case BP_SU_SCIS1:
        {
            int x, y, w, h;

            bpRegs.scis1.scis1 = value;

            x = bpRegs.scis0.sux - 342;
            y = bpRegs.scis0.suy - 342;
            w = bpRegs.scis1.suw - bpRegs.scis0.sux + 1;
            h = bpRegs.scis1.suh - bpRegs.scis0.suy + 1;

            //GFXError("scissor (%i, %i)-(%i, %i)", x, y, w, h);
            gfx->SetScissor(x, y, w, h);
        }
        return;

        //
        // set copy clear color/z
        //

        case PE_COPY_CLEAR_AR:
        {
            copyClearRGBA.A = (u8)(value >> 8);
            copyClearRGBA.R = (u8)(value     );
            return;
        }

        case PE_COPY_CLEAR_GB:
        {
            copyClearRGBA.G = (u8)(value >> 8);
            copyClearRGBA.B = (u8)(value     );
        }
        return;

        case PE_COPY_CLEAR_Z:
        {
            copyClearZ = value & 0xffffff;
            gfx->SetClear(copyClearRGBA, copyClearZ);
        }
        return;

        //
        // texture image width, height, format
        //

        case TX_SETIMAGE_0_0:
        {
            bpRegs.teximg0[0].hex = value;
            bpRegs.valid[0][0] = TRUE;
            tryLoadTex(0);
        }
        return;

        //
        // texture image base
        //

        case TX_SETIMAGE_3_0:
        {
            bpRegs.teximg3[0].hex = value;
            bpRegs.valid[3][0] = TRUE;
            tryLoadTex(0);
        }
        return;

        //
        // load tlut
        //

        case TX_LOADTLUT0:
        {
            bpRegs.loadtlut0.hex = value;

            LoadTlut(
                (bpRegs.loadtlut0.base << 5),   // ram address
                (bpRegs.loadtlut1.tmem << 9),   // tlut offset
                bpRegs.loadtlut1.count          // tlut size
            );
        }
        return;

        case TX_LOADTLUT1:
        {
            bpRegs.loadtlut1.hex = value;

            LoadTlut(
                (bpRegs.loadtlut0.base << 5),   // ram address
                (bpRegs.loadtlut1.tmem << 9),   // tlut offset
                bpRegs.loadtlut1.count          // tlut size
            );
        }
        return;

        //
        // set tlut
        //

        case TX_SETTLUT_0:
        {
            bpRegs.settlut[0].hex = value;
        }
        return;

        //
        // set texture modes
        //

        case TX_SETMODE_0_0:
        {
            bpRegs.texmode0[0].hex = value;
        }
        return;

        //
        // set blending rules
        //

        case PE_CMODE0:
        {
            bpRegs.cmode0.hex = value;

            static char *logicop[] = {
                "clear",
                "and",
                "revand",
                "copy",
                "invand",
                "nop",
                "xor",
                "or",
                "nor",
                "eqv",
                "inv",
                "revor",
                "invcopy",
                "invor",
                "nand",
                "set"
            };

            static char *sfactor[] = {
                "zero",
                "one",
                "srcclr",
                "invsrcclr",
                "srcalpha",
                "invsrcalpha",
                "dstalpha",
                "invdstalpha"
            };

            static char *dfactor[] = {
                "zero",
                "one",
                "dstclr",
                "invdstclr",
                "srcalpha",
                "invsrcalpha",
                "dstalpha",
                "invdstalpha"
            };

/*/
            GFXError(
                "blend rules\n\n"
                "blend:%s, logic:%s\n"
                "logic op : %s\n"
                "sfactor : %s\n"
                "dfactor : %s\n",
                (bpRegs.cmode0.blend_en) ? ("on") : ("off"),
                (bpRegs.cmode0.logop_en) ? ("on") : ("off"),
                logicop[bpRegs.cmode0.logop],
                sfactor[bpRegs.cmode0.sfactor],
                dfactor[bpRegs.cmode0.dfactor]
            );
/*/

            static u32 glsf[] = {
                GL_ZERO,
                GL_ONE,
                GL_SRC_COLOR,
                GL_ONE_MINUS_SRC_COLOR,
                GL_SRC_ALPHA,
                GL_ONE_MINUS_SRC_ALPHA,
                GL_DST_ALPHA,
                GL_ONE_MINUS_DST_ALPHA
            };

            static u32 gldf[] = {
                GL_ZERO,
                GL_ONE,
                GL_DST_COLOR,
                GL_ONE_MINUS_DST_COLOR,
                GL_SRC_ALPHA,
                GL_ONE_MINUS_SRC_ALPHA,
                GL_DST_ALPHA,
                GL_ONE_MINUS_DST_ALPHA
            };

            // blend hack
            if(bpRegs.cmode0.blend_en)
            {
                glEnable(GL_BLEND);
                glBlendFunc(glsf[bpRegs.cmode0.sfactor], gldf[bpRegs.cmode0.dfactor]);
            }
            else glDisable(GL_BLEND);

            static u32 logop[] = {
                GL_CLEAR,
                GL_AND,
                GL_AND_REVERSE,
                GL_COPY,
                GL_AND_INVERTED,
                GL_NOOP,
                GL_XOR,
                GL_OR,
                GL_NOR,
                GL_EQUIV,
                GL_INVERT,
                GL_OR_REVERSE,
                GL_COPY_INVERTED,
                GL_OR_INVERTED,
                GL_NAND,
                GL_SET
            };

            // logic operations
            if(bpRegs.cmode0.logop_en)
            {
                glEnable(GL_COLOR_LOGIC_OP);
                glLogicOp(logop[bpRegs.cmode0.logop]);
            }
            else glDisable(GL_COLOR_LOGIC_OP);
        }
        return;

        case PE_CMODE1:
        {
            bpRegs.cmode1.hex = value;
        }
        return;

        case PE_ZMODE_ID:
        {
            static char *zf[] = {
                "NEVER",
                "LESS",
                "EQUAL",
                "LEQUAL",
                "GREATER",
                "NEQUAL",
                "GEQUAL",
                "ALWAYS"
            };

            static u32 glzf[] = {
                GL_NEVER,
                GL_LESS,
                GL_EQUAL,
                GL_LEQUAL,
                GL_GREATER,
                GL_NOTEQUAL,
                GL_GEQUAL,
                GL_ALWAYS
            };

            bpRegs.zmode.hex = value;
            
/*/
            GFXError(
                "z mode:\n"
                "compare: %s\n"
                "func: %s\n"
                "update: %s",
                (bpRegs.zmode.enable) ? ("yes") : ("no"),
                zf[bpRegs.zmode.func],
                (bpRegs.zmode.mask) ? ("yes") : ("no")
            );
/*/

            if(bpRegs.zmode.enable)
            {
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(glzf[bpRegs.zmode.func]);
                glDepthMask(bpRegs.zmode.mask);
            }
            else glDisable(GL_DEPTH_TEST);
        }
        return;

        //
        // texture coord scale
        //

        case SU_SSIZE_0:
        case SU_SSIZE_1:
        case SU_SSIZE_2:
        case SU_SSIZE_3:
        case SU_SSIZE_4:
        case SU_SSIZE_5:
        case SU_SSIZE_6:
        case SU_SSIZE_7:
        {
            int num = (index >> 1) & 1;
            bpRegs.ssize[num].hex = value;
        }
        return;

        case SU_TSIZE_0:
        case SU_TSIZE_1:
        case SU_TSIZE_2:
        case SU_TSIZE_3:
        case SU_TSIZE_4:
        case SU_TSIZE_5:
        case SU_TSIZE_6:
        case SU_TSIZE_7:
        {
            int num = (index >> 1) & 1;
            bpRegs.tsize[num].hex = value;
        }
        return;

        default:
        {
#ifdef  GPLOG
            fprintf(gplog, "unknown BP load, index: %02X, data: %08X\n", index, value);
            fflush(gplog);
#endif
//            GFXError("unknown BP load, index : %02X", index);
        }
    }
}

// index range = 0000..FFFF
// reg size = 32 bit
void loadXFRegs(unsigned startIdx, unsigned amount, u32 *regData)
{
    xfLoads += amount;

#if 0
    GFXError("unknown XF load, start index: %04X, n : %i\n", startIdx, amount);
    for(unsigned n=0; n<amount; n++)
    {
        GFXError("  %-2i: %08X\n", n, regData[n]);
    }
#endif

    // load geometry matrix
    if((startIdx >= 0x0000) && (startIdx < 0x0400))
    {
        memcpy(
            (void *)((u32)xfRegs.posmtx + (u32)4 * startIdx), 
            regData, 
            amount * sizeof(float)
        );

#ifdef  GPLOG
        fprintf(gplog, "load position matrix, start index: %04X\n", startIdx);
            
        for(unsigned n=0; n<amount; n++)
        {
            fprintf(gplog, "  %-2i: %08X\n", n, regData[n]);
        }

        fflush(gplog);
#endif
    }
    // load normal matrix
    else if((startIdx >= 0x0400) && (startIdx < 0x0500))
    {
        memcpy(
            (void *)((u32)xfRegs.nrmmtx + (u32)4 * startIdx), 
            regData, 
            amount * sizeof(float)
        );
    }
    // load post-trans matrix
    else if((startIdx >= 0x0500) && (startIdx < 0x0600))
    {
        memcpy(
            (void *)((u32)xfRegs.postmtx + (u32)4 * startIdx), 
            regData, 
            amount * sizeof(float)
        );

#ifdef  GPLOG
        fprintf(gplog, "load post-transform matrix, start index: %04X\n", startIdx);
            
        for(unsigned n=0; n<amount; n++)
        {
            fprintf(gplog, "  %-2i: %08X\n", n, regData[n]);
        }

        fflush(gplog);
#endif
    }
    else switch(startIdx)
    {
        //
        // set matrix index
        //

        case XF_MATINDEX_A:
        {
            xfRegs.matidxA.matidx = regData[0];
            xfRegs.posidx = xfRegs.matidxA.pos;
            xfRegs.texidx[0] = xfRegs.matidxA.tex0;
            xfRegs.texidx[1] = xfRegs.matidxA.tex1;
            xfRegs.texidx[2] = xfRegs.matidxA.tex2;
            xfRegs.texidx[3] = xfRegs.matidxA.tex3;
            //GFXError("xf posidx : %i", xfRegs.matidxA.pos);
        }
        return;

        case XF_MATINDEX_B:
        {
            xfRegs.matidxB.matidx = regData[0];
            xfRegs.texidx[4] = xfRegs.matidxB.tex4;
            xfRegs.texidx[5] = xfRegs.matidxB.tex5;
            xfRegs.texidx[6] = xfRegs.matidxB.tex6;
            xfRegs.texidx[7] = xfRegs.matidxB.tex7;
        }
        return;

        //
        // load projection matrix
        //

        case XF_PROJECTION:
        {
            float *pMatrix = (float *)regData;
            float Matrix[4][4];
            if (pMatrix[6] == 0)
            {
                Matrix[0][0] = pMatrix[0];
                Matrix[1][0] = 0.0f;
                Matrix[2][0] = pMatrix[1];
                Matrix[3][0] = 0.0f;
                Matrix[0][1] = 0.0f;
                Matrix[1][1] = pMatrix[2];
                Matrix[2][1] = pMatrix[3];
                Matrix[3][1] = 0.0f;
                Matrix[0][2] = 0.0f;
                Matrix[1][2] = 0.0f;
                Matrix[2][2] = pMatrix[4];
                Matrix[3][2] = pMatrix[5];
                Matrix[0][3] = 0.0f;
                Matrix[1][3] = 0.0f;
                Matrix[2][3] = -1.0f;
                Matrix[3][3] = 0.0f;
            }
            else
            {
                Matrix[0][0] = pMatrix[0];
                Matrix[1][0] = 0.0f;
                Matrix[2][0] = 0.0f;
                Matrix[3][0] = pMatrix[1];
                Matrix[0][1] = 0.0f;
                Matrix[1][1] = pMatrix[2];
                Matrix[2][1] = 0.0f;
                Matrix[3][1] = pMatrix[3];
                Matrix[0][2] = 0.0f;
                Matrix[1][2] = 0.0f;
                Matrix[2][2] = pMatrix[4];
                Matrix[3][2] = pMatrix[5];
                Matrix[0][3] = 0.0f;
                Matrix[1][3] = 0.0f;
                Matrix[2][3] = 0.0f;
                Matrix[3][3] = 1.0f;
            }

            gfx->SetProjection((float*)Matrix);
        }
        return;

        //
        // load viewport configuration
        // 

        case XF_VIEWPORT:
        {
            float *data = (float *)regData;
            float w, h, x, y, zf, zn;

            //
            // read coefficients
            //

            xfRegs.vp_scale[0] = data[0];   // w / 2
            xfRegs.vp_scale[1] = data[1];   // -h / 2
            xfRegs.vp_scale[2] = data[2];   // ZMAX * (zfar - znear)

            xfRegs.vp_offs[0] = data[3];    // x + w/2 + 342
            xfRegs.vp_offs[1] = data[4];    // y + h/2 + 342
            xfRegs.vp_offs[2] = data[5];    // ZMAX * zfar

            //
            // convert them to human usable form
            //

            w = xfRegs.vp_scale[0] * 2;
            h = -xfRegs.vp_scale[1] * 2;
            x = xfRegs.vp_offs[0] - xfRegs.vp_scale[0] - 342;
            y = xfRegs.vp_offs[1] + xfRegs.vp_scale[1] - 342;
            zf = xfRegs.vp_offs[2] / 16777215.0f;
            zn = -((xfRegs.vp_scale[2] / 16777215.0f) - zf);

            //GFXError("viewport (%.2f, %.2f)-(%.2f, %.2f), %f, %f", x, y, w, h, zn, zf);
            gfx->SetViewport((int)x, (int)y, (int)w, (int)h, zn, zf);
        }
        return;

        //
        // load light object (unaligned writes not supported)
        //

        case XF_LIGHT0:
        case XF_LIGHT1:
        case XF_LIGHT2:
        case XF_LIGHT3:
        case XF_LIGHT4:
        case XF_LIGHT5:
        case XF_LIGHT6:
        case XF_LIGHT7:
        {
            unsigned lnum = (startIdx >> 4) & 7;
            memcpy(&xfRegs.light[lnum], regData, sizeof(LightObj));
        }
        return;

        //
        // channel constant color registers
        //

        case XF_AMBIENT0:
        {
            xfRegs.ambient[0].RGBA = regData[0];
        }
        return;

        case XF_AMBIENT1:
        {
            xfRegs.ambient[1].RGBA = regData[0];
        }
        return;

        case XF_MATERIAL0:
        {
            xfRegs.material[0].RGBA = regData[0];
        }
        return;

        case XF_MATERIAL1:
        {
            xfRegs.material[1].RGBA = regData[0];
        }
        return;

        //
        // channel control registers
        //

        case XF_COLOR0CNTL:
        {
            xfRegs.color[0].Chan = regData[0];

            // change light mask
            xfRegs.colmask[0][0] = (xfRegs.color[0].Light0) ? (TRUE) : (FALSE);
            xfRegs.colmask[1][0] = (xfRegs.color[0].Light1) ? (TRUE) : (FALSE);
            xfRegs.colmask[2][0] = (xfRegs.color[0].Light2) ? (TRUE) : (FALSE);
            xfRegs.colmask[3][0] = (xfRegs.color[0].Light3) ? (TRUE) : (FALSE);
            xfRegs.colmask[4][0] = (xfRegs.color[0].Light4) ? (TRUE) : (FALSE);
            xfRegs.colmask[5][0] = (xfRegs.color[0].Light5) ? (TRUE) : (FALSE);
            xfRegs.colmask[6][0] = (xfRegs.color[0].Light6) ? (TRUE) : (FALSE);
            xfRegs.colmask[7][0] = (xfRegs.color[0].Light7) ? (TRUE) : (FALSE);
        }
        return;

        case XF_COLOR1CNTL:
        {
            xfRegs.color[1].Chan = regData[0];

            // change light mask
            xfRegs.colmask[0][1] = (xfRegs.color[1].Light0) ? (TRUE) : (FALSE);
            xfRegs.colmask[1][1] = (xfRegs.color[1].Light1) ? (TRUE) : (FALSE);
            xfRegs.colmask[2][1] = (xfRegs.color[1].Light2) ? (TRUE) : (FALSE);
            xfRegs.colmask[3][1] = (xfRegs.color[1].Light3) ? (TRUE) : (FALSE);
            xfRegs.colmask[4][1] = (xfRegs.color[1].Light4) ? (TRUE) : (FALSE);
            xfRegs.colmask[5][1] = (xfRegs.color[1].Light5) ? (TRUE) : (FALSE);
            xfRegs.colmask[6][1] = (xfRegs.color[1].Light6) ? (TRUE) : (FALSE);
            xfRegs.colmask[7][1] = (xfRegs.color[1].Light7) ? (TRUE) : (FALSE);
        }
        return;

        case XF_ALPHA0CNTL:
        {
            xfRegs.alpha[0].Chan = regData[0];

            // change light mask
            xfRegs.amask[0][0] = (xfRegs.alpha[0].Light0) ? (TRUE) : (FALSE);
            xfRegs.amask[1][0] = (xfRegs.alpha[0].Light1) ? (TRUE) : (FALSE);
            xfRegs.amask[2][0] = (xfRegs.alpha[0].Light2) ? (TRUE) : (FALSE);
            xfRegs.amask[3][0] = (xfRegs.alpha[0].Light3) ? (TRUE) : (FALSE);
            xfRegs.amask[4][0] = (xfRegs.alpha[0].Light4) ? (TRUE) : (FALSE);
            xfRegs.amask[5][0] = (xfRegs.alpha[0].Light5) ? (TRUE) : (FALSE);
            xfRegs.amask[6][0] = (xfRegs.alpha[0].Light6) ? (TRUE) : (FALSE);
            xfRegs.amask[7][0] = (xfRegs.alpha[0].Light7) ? (TRUE) : (FALSE);
        }
        return;

        case XF_ALPHA1CNTL:
        {
            xfRegs.alpha[1].Chan = regData[0];

            // change light mask
            xfRegs.amask[0][1] = (xfRegs.alpha[1].Light0) ? (TRUE) : (FALSE);
            xfRegs.amask[1][1] = (xfRegs.alpha[1].Light1) ? (TRUE) : (FALSE);
            xfRegs.amask[2][1] = (xfRegs.alpha[1].Light2) ? (TRUE) : (FALSE);
            xfRegs.amask[3][1] = (xfRegs.alpha[1].Light3) ? (TRUE) : (FALSE);
            xfRegs.amask[4][1] = (xfRegs.alpha[1].Light4) ? (TRUE) : (FALSE);
            xfRegs.amask[5][1] = (xfRegs.alpha[1].Light5) ? (TRUE) : (FALSE);
            xfRegs.amask[6][1] = (xfRegs.alpha[1].Light6) ? (TRUE) : (FALSE);
            xfRegs.amask[7][1] = (xfRegs.alpha[1].Light7) ? (TRUE) : (FALSE);
        }
        return;

        //
        // set dualtex enable / disable
        //

        case XF_DUALTEX:
        {
            xfRegs.dualtex = regData[0];
            //GFXError("dual texgen : %s", (regData[0]) ? ("on") : ("off"));
        }
        return;

        case XF_DUALGEN0:
        case XF_DUALGEN1:
        case XF_DUALGEN2:
        case XF_DUALGEN3:
        case XF_DUALGEN4:
        case XF_DUALGEN5:
        case XF_DUALGEN6:
        case XF_DUALGEN7:
        {
            unsigned n = startIdx - XF_DUALGEN0;
            //ASSERT(amount != 1);

            //xfRegs.dual[n].hex = regData[0];

/*/
            GFXError(
                "set dual for %i:\n"
                "raw: %08X\n"
                "index: %i\n"
                "normalize: %s",
                n,
                regData[0],
                xfRegs.dual[n].dualidx,
                (xfRegs.dual[n].norm) ? ("yes") : ("no")
            );
/*/
        }
        return;

        //
        // number of output colors
        //

        case XF_NUMCOLS:
        {
            xfRegs.numcol = regData[0];
        }
        return;

        //
        // set number of texgens
        //

        case XF_NUMTEX:
        {
            xfRegs.numtex = regData[0];
        }
        return;

        // 
        // set texgen configuration
        //

        case XF_TEXGEN0:
        case XF_TEXGEN1:
        case XF_TEXGEN2:
        case XF_TEXGEN3:
        case XF_TEXGEN4:
        case XF_TEXGEN5:
        case XF_TEXGEN6:
        case XF_TEXGEN7:
        {
            unsigned num = startIdx & 7;

            static  char *prj[] = { "2x4", "3x4" };
            static  char *inf[] = { "ab11", "abc1" };
            static  char *type[] = { "regular", "bump", "toon0", "toon1" };
            static  char *srcrow[] = {
                "xyz",
                "normal",
                "colors",
                "binormal t",
                "binormal b",
                "tex0",
                "tex1",
                "tex2",
                "tex3",
                "tex4",
                "tex5",
                "tex6",
                "tex7",
                "", "", ""
            };

			xfRegs.texgen[num].hex = regData[0];

/*/
            GFXError(
                "texgen %i set\n"
                "prj : %s\n"
                "in form : %s\n"
                "type : %s\n"
                "src row : %s\n"
                "emboss src : %i\n"
                "emboss lnum : %i\n",
                num,
                prj[xfRegs.texgen[num].pojection & 1],
                inf[xfRegs.texgen[num].in_form & 1],
                type[xfRegs.texgen[num].type & 3],
                srcrow[xfRegs.texgen[num].src_row & 0xf],
                xfRegs.texgen[num].emboss_src,
                xfRegs.texgen[num].emboss_light
            );
/*/
        }
        return;

        //
        // not implemented
        //

        default:
        {
#ifdef  GPLOG
            fprintf(gplog, "unknown XF load, start index: %04X\n", startIdx);
            
            for(unsigned n=0; n<amount; n++)
            {
                fprintf(gplog, "  %-2i: %08X\n", n, regData[n]);
            }

            fflush(gplog);
#endif
//            GFXError("unknown XF load, start index : %04X", startIdx);
        }
    }
}
