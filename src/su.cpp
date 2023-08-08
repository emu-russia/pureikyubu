// Setup Unit
#include "pch.h"

// Processing of BP address space registers Load Commands

using namespace Debug;

#define NO_VIEWPORT

namespace GX
{

    void GXCore::GL_SetScissor(int x, int y, int w, int h)
    {
        //h += 32;
#ifndef NO_VIEWPORT
        glScissor(x, scr_h - (h + y), w, h);
#endif
    }

    void GXCore::GL_SetCullMode(int mode)
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
        if (texvalid[0][id] && texvalid[3][id])
        {
            texvalid[0][id] =
            texvalid[3][id] = false;

#ifndef WIREFRAME
            LoadTexture(
                teximg3[id].base << 5,
                id,
                teximg0[id].fmt,
                teximg0[id].width + 1,
                teximg0[id].height + 1
            );
#endif
        }
    }

    // index range = 00..FF
    // reg size = 24 bit (value is already masked)
    void GXCore::loadBPReg(size_t index, uint32_t value)
    {
        state.bpLoads++;

        if (GpRegsLog)
        {
            Report(Channel::GP, "Load BP: index: 0x%02X, data: 0x%08X\n", index, value);
        }

        switch (index)
        {
            // draw done
            case PE_DONE_ID:
            {
                GPFrameDone();
                CPDrawDone();
            }
            return;

            // token
            case PE_TOKEN_INT_ID:
            {
                tokint = (uint16_t)value;
            }
            return;

            case PE_TOKEN_ID:
            {
                if ((uint16_t)value == tokint)
                {
                    GPFrameDone();
                    CPDrawToken(tokint);
                }
            }
            return;

            //
            // gen mode
            // application : clipping
            //

            case GEN_MODE_ID:
            {
                static int cull_modes[4] = {
                    GFX_CULL_NONE,
                    GFX_CULL_BACK,
                    GFX_CULL_FRONT,
                    GFX_CULL_ALL,
                };
                genmode.bits = value;
                //GFXError("%i", bpRegs.genmode.cull);

                GL_SetCullMode(cull_modes[genmode.cull]);
            }
            return;

            //
            // set scissor box
            //

            case SU_SCIS0_ID:
            {
                int x, y, w, h;

                scis0.bits = value;

                x = scis0.sux - 342;
                y = scis0.suy - 342;
                w = scis1.suw - scis0.sux + 1;
                h = scis1.suh - scis0.suy + 1;

                //GFXError("scissor (%i, %i)-(%i, %i)", x, y, w, h);
                GL_SetScissor(x, y, w, h);
            }
            return;

            case SU_SCIS1_ID:
            {
                int x, y, w, h;

                scis1.bits = value;

                x = scis0.sux - 342;
                y = scis0.suy - 342;
                w = scis1.suw - scis0.sux + 1;
                h = scis1.suh - scis0.suy + 1;

                //GFXError("scissor (%i, %i)-(%i, %i)", x, y, w, h);
                GL_SetScissor(x, y, w, h);
            }
            return;

            //
            // set copy clear color/z
            //

            case PE_COPY_CLEAR_AR_ID:
            {
                copyClearRGBA.A = (uint8_t)(value >> 8);
                copyClearRGBA.R = (uint8_t)(value);
                return;
            }

            case PE_COPY_CLEAR_GB_ID:
            {
                copyClearRGBA.G = (uint8_t)(value >> 8);
                copyClearRGBA.B = (uint8_t)(value);
            }
            return;

            case PE_COPY_CLEAR_Z_ID:
            {
                copyClearZ = value & 0xffffff;
                GL_SetClear(copyClearRGBA, copyClearZ);
            }
            return;

            //
            // texture image width, height, format
            //

            case TX_SETIMAGE0_I0_ID:
            {
                teximg0[0].bits = value;
                texvalid[0][0] = true;
                tryLoadTex(0);
            }
            return;

            //
            // texture image base
            //

            case TX_SETIMAGE3_I0_ID:
            {
                teximg3[0].bits = value;
                texvalid[3][0] = true;
                tryLoadTex(0);
            }
            return;

            //
            // load tlut
            //

            case TX_LOADTLUT0_ID:
            {
                loadtlut0.bits = value;

                LoadTlut(
                    (loadtlut0.base << 5),   // ram address
                    (loadtlut1.tmem << 9),   // tlut offset
                    loadtlut1.count          // tlut size
                );
            }
            return;

            case TX_LOADTLUT1_ID:
            {
                loadtlut1.bits = value;

                LoadTlut(
                    (loadtlut0.base << 5),   // ram address
                    (loadtlut1.tmem << 9),   // tlut offset
                    loadtlut1.count          // tlut size
                );
            }
            return;

            //
            // set tlut
            //

            case TX_SETTLUT_I0_ID:
            {
                settlut[0].bits = value;
            }
            return;

            //
            // set texture modes
            //

            case TX_SETMODE0_I0_ID:
            {
                texmode0[0].bits = value;
            }
            return;

            //
            // set blending rules
            //

            case PE_CMODE0_ID:
            {
                cmode0.bits = value;

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
                if (cmode0.blend_en)
                {
                    glEnable(GL_BLEND);
                    glBlendFunc(glsf[cmode0.sfactor], gldf[cmode0.dfactor]);
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
                if (cmode0.logop_en)
                {
                    glEnable(GL_COLOR_LOGIC_OP);
                    glLogicOp(logop[cmode0.logop]);
                }
                else glDisable(GL_COLOR_LOGIC_OP);
            }
            return;

            case PE_CMODE1_ID:
            {
                cmode1.bits = value;
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

                zmode.bits = value;

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

                if (zmode.enable)
                {
                    glEnable(GL_DEPTH_TEST);
                    glDepthFunc(glzf[zmode.func]);
                    glDepthMask(zmode.mask);
                }
                else glDisable(GL_DEPTH_TEST);
            }
            return;

            //
            // texture coord scale
            //

            case SU_SSIZE0_ID:
            case SU_SSIZE1_ID:
            case SU_SSIZE2_ID:
            case SU_SSIZE3_ID:
            case SU_SSIZE4_ID:
            case SU_SSIZE5_ID:
            case SU_SSIZE6_ID:
            case SU_SSIZE7_ID:
            {
                int num = (index >> 1) & 1;
                ssize[num].bits = value;
            }
            return;

            case SU_TSIZE0_ID:
            case SU_TSIZE1_ID:
            case SU_TSIZE2_ID:
            case SU_TSIZE3_ID:
            case SU_TSIZE4_ID:
            case SU_TSIZE5_ID:
            case SU_TSIZE6_ID:
            case SU_TSIZE7_ID:
            {
                int num = (index >> 1) & 1;
                tsize[num].bits = value;
            }
            return;

            default:
            {
                Report(Channel::GP, "Unknown BP load, index: 0x%02X\n", index);
            }
        }
    }
}
