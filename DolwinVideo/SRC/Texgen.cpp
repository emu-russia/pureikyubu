// texture generators
#include "GX.h"

TexGenOut   tgout[8];

// generate NUMTEX coordinates
void DoTexGen(const Vertex *v)
{
    float   in[4], q;
    float   *mx;

    if(xfRegs.numtex == 0)
    {
        mx = &xfRegs.posmtx[xfRegs.texidx[0]][0];
        in[0] = v->tcoord[0][0];
        in[1] = v->tcoord[0][1];
        in[2] = 1.0f;
        in[3] = 1.0f;
    }

    for(unsigned n=0; n<xfRegs.numtex; n++)
    {
        if(xfRegs.texgen[n].type == 0)
        {
            // select inrow
            switch(xfRegs.texgen[n].src_row)
            {
                case XF_TEXGEN_GEOM_INROW:
                {
                    in[0] = v->pos[0];
                    in[1] = v->pos[1];
                    in[2] = v->pos[2];
                    in[3] = 1.0f;
                }
                break;

                case XF_TEXGEN_NORMAL_INROW:
                {
                    in[0] = v->nrm[0];
                    in[1] = v->nrm[1];
                    in[2] = v->nrm[2];
                    in[3] = 1.0f;
                }
                break;

                case XF_TEXGEN_TEX0_INROW:
                {
                    mx = &xfRegs.posmtx[xfRegs.texidx[0]][0];
                    in[0] = v->tcoord[0][0];
                    in[1] = v->tcoord[0][1];
                    in[2] = 1.0f;
                    in[3] = 1.0f;
                }
                break;

                case XF_TEXGEN_TEX1_INROW:
                {
                    mx = &xfRegs.posmtx[xfRegs.texidx[1]][0];
                    in[0] = v->tcoord[1][0];
                    in[1] = v->tcoord[1][1];
                    in[2] = 1.0f;
                    in[3] = 1.0f;
                }
                break;

                case XF_TEXGEN_TEX2_INROW:
                {
                    mx = &xfRegs.posmtx[xfRegs.texidx[2]][0];
                    in[0] = v->tcoord[2][0];
                    in[1] = v->tcoord[2][1];
                    in[2] = 1.0f;
                    in[3] = 1.0f;
                }
                break;

                case XF_TEXGEN_TEX3_INROW:
                {
                    mx = &xfRegs.posmtx[xfRegs.texidx[3]][0];
                    in[0] = v->tcoord[3][0];
                    in[1] = v->tcoord[3][1];
                    in[2] = 1.0f;
                    in[3] = 1.0f;
                }
                break;

                case XF_TEXGEN_TEX4_INROW:
                {
                    mx = &xfRegs.posmtx[xfRegs.texidx[4]][0];
                    in[0] = v->tcoord[4][0];
                    in[1] = v->tcoord[4][1];
                    in[2] = 1.0f;
                    in[3] = 1.0f;
                }
                break;

                case XF_TEXGEN_TEX5_INROW:
                {
                    mx = &xfRegs.posmtx[xfRegs.texidx[5]][0];
                    in[0] = v->tcoord[5][0];
                    in[1] = v->tcoord[5][1];
                    in[2] = 1.0f;
                    in[3] = 1.0f;
                }
                break;

                case XF_TEXGEN_TEX6_INROW:
                {
                    mx = &xfRegs.posmtx[xfRegs.texidx[6]][0];
                    in[0] = v->tcoord[6][0];
                    in[1] = v->tcoord[6][1];
                    in[2] = 1.0f;
                    in[3] = 1.0f;
                }
                break;

                case XF_TEXGEN_TEX7_INROW:
                {
                    mx = &xfRegs.posmtx[xfRegs.texidx[7]][0];
                    in[0] = v->tcoord[7][0];
                    in[1] = v->tcoord[7][1];
                    in[2] = 1.0f;
                    in[3] = 1.0f;
                }
                break;
            }

            mx = &xfRegs.posmtx[xfRegs.texidx[n]][0];

            // st or stq ?
            if(xfRegs.texgen[n].pojection)
            {
                tgout[n].out[0] = in[0] * mx[0] + in[1] * mx[1] + in[2] * mx[2] + mx[3];
                tgout[n].out[1] = in[0] * mx[4] + in[1] * mx[5] + in[2] * mx[6] + mx[7];
                q = in[0] * mx[8] + in[1] * mx[9] + in[2] * mx[10] + mx[11];
                tgout[n].out[0] /= q;
                tgout[n].out[1] /= q;
            }
            else
            {
                tgout[n].out[0] = in[0] * mx[0] + in[1] * mx[1] + mx[2] + mx[3];
                tgout[n].out[1] = in[0] * mx[4] + in[1] * mx[5] + mx[6] + mx[7];
            }

            // dual-transform
        }
    }

    //tgout[0].out[1] /= 1.33333;
}
