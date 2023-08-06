#include "pch.h"

// Processing of BP address space registers Load Commands

using namespace Debug;

#define NO_VIEWPORT

namespace GX
{

    void GL_SetScissor(int x, int y, int w, int h)
    {
        //h += 32;
#ifndef NO_VIEWPORT
        glScissor(x, scr_h - (h + y), w, h);
#endif
    }

    void GL_SetCullMode(int mode)
    {
        /*/
            switch(mode)
            {
                case GFX_CULL_NONE:
                    glDisable(GL_CULL_FACE);
                    break;
                case GFX_CULL_FRONT:
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_FRONT);
                    break;
                case GFX_CULL_BACK:
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_BACK);
                    break;
                case GFX_CULL_ALL:
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_FRONT_AND_BACK);
                    break;
            }
        /*/
    }

    void GXCore::tryLoadTex(int id)
    {
        if (bpRegs.valid[0][id] && bpRegs.valid[3][id])
        {
            bpRegs.valid[0][id] =
            bpRegs.valid[3][id] = false;

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
    void GXCore::loadBPReg(size_t index, uint32_t value)
    {
        static Color    copyClearRGBA;
        static uint32_t      copyClearZ;

        state.bpLoads++;

        if (GpRegsLog)
        {
            Report(Channel::GP, "Load BP: index: 0x%02X, data: 0x%08X", index, value);
        }

        switch (index)
        {
            // draw done
            case PE_DONE:
            {
                GPFrameDone();
                CPDrawDone();
            }
            return;

            // token
            case PE_TOKEN_INT:
            {
                bpRegs.tokint = (uint16_t)value;
            }
            return;

            case PE_TOKEN:
            {
                if ((uint16_t)value == bpRegs.tokint)
                {
                    GPFrameDone();
                    CPDrawToken(bpRegs.tokint);
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

                GL_SetCullMode(cull_modes[bpRegs.genmode.cull]);
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
                GL_SetScissor(x, y, w, h);
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
                GL_SetScissor(x, y, w, h);
            }
            return;

            //
            // set copy clear color/z
            //

            case PE_COPY_CLEAR_AR:
            {
                copyClearRGBA.A = (uint8_t)(value >> 8);
                copyClearRGBA.R = (uint8_t)(value);
                return;
            }

            case PE_COPY_CLEAR_GB:
            {
                copyClearRGBA.G = (uint8_t)(value >> 8);
                copyClearRGBA.B = (uint8_t)(value);
            }
            return;

            case PE_COPY_CLEAR_Z:
            {
                copyClearZ = value & 0xffffff;
                GL_SetClear(copyClearRGBA, copyClearZ);
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

                static const char* logicop[] = {
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

                static const char* sfactor[] = {
                    "zero",
                    "one",
                    "srcclr",
                    "invsrcclr",
                    "srcalpha",
                    "invsrcalpha",
                    "dstalpha",
                    "invdstalpha"
                };

                static const char* dfactor[] = {
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

                static uint32_t glsf[] = {
                    GL_ZERO,
                    GL_ONE,
                    GL_SRC_COLOR,
                    GL_ONE_MINUS_SRC_COLOR,
                    GL_SRC_ALPHA,
                    GL_ONE_MINUS_SRC_ALPHA,
                    GL_DST_ALPHA,
                    GL_ONE_MINUS_DST_ALPHA
                };

                static uint32_t gldf[] = {
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
                if (bpRegs.cmode0.blend_en)
                {
                    glEnable(GL_BLEND);
                    glBlendFunc(glsf[bpRegs.cmode0.sfactor], gldf[bpRegs.cmode0.dfactor]);
                }
                else glDisable(GL_BLEND);

                static uint32_t logop[] = {
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
                if (bpRegs.cmode0.logop_en)
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
                static const char* zf[] = {
                    "NEVER",
                    "LESS",
                    "EQUAL",
                    "LEQUAL",
                    "GREATER",
                    "NEQUAL",
                    "GEQUAL",
                    "ALWAYS"
                };

                static uint32_t glzf[] = {
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

                if (bpRegs.zmode.enable)
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
                Report(Channel::GP, "Unknown BP load, index: 0x%02X", index);
            }
        }
    }

}
