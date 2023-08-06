// Transform Unit
#include "pch.h"

using namespace Debug;

#define NO_VIEWPORT

namespace GX
{

    // normalize (clamp vector to 1.0 length)
    void GXCore::VECNormalize(float vec[3])
    {
        float d = (float)sqrt(vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2]);

        vec[0] /= d;
        vec[1] /= d;
        vec[2] /= d;
    }

    // perform position transform
    void GXCore::ApplyModelview(float* out, const float* in)
    {
        float* mx = &xfRegs.posmtx[xfRegs.posidx][0];

        out[0] = in[0] * mx[0] + in[1] * mx[1] + in[2] * mx[2] + mx[3];
        out[1] = in[0] * mx[4] + in[1] * mx[5] + in[2] * mx[6] + mx[7];
        out[2] = in[0] * mx[8] + in[1] * mx[9] + in[2] * mx[10] + mx[11];
    }

    // perform normal transform
    // matrix must be the inverse transpose of the modelview matrix
    void GXCore::NormalTransform(float* out, const float* in)
    {
        float* mx = &xfRegs.nrmmtx[xfRegs.posidx][0];

        out[0] = in[0];
        out[1] = in[1];
        out[2] = in[2];

        //out[0] = in[0] * mx[0] + in[1] * mx[1] + in[2] * mx[2];
        //out[1] = in[0] * mx[3] + in[1] * mx[4] + in[2] * mx[5];
        //out[2] = in[0] * mx[6] + in[1] * mx[7] + in[2] * mx[8];

        VECNormalize(out);
    }

    #define CLAMP(n)                \
    {                               \
        if(n <-1.0f) n =-1.0f;      \
        if(n > 1.0f) n = 1.0f;      \
    }

    #define CLAMP0(n)               \
    {                               \
        if(n < 0.0f) n = 0.0f;      \
        if(n > 1.0f) n = 1.0f;      \
    }

    // color0 only calculation
    void GXCore::DoLights(const Vertex* v)
    {
        float vpos[3], vnrm[3];
        float col[3], res[3];
        float mat[3], amb[3];
        float illum[3];

        ApplyModelview(vpos, v->pos);

        // -------------------------------------------------------------------

        //
        // calculate color for channel 0
        //

        // convert vertex color to [0, 1] interval
        col[0] = (float)v->col[0].R / 255.0f;
        col[1] = (float)v->col[0].G / 255.0f;
        col[2] = (float)v->col[0].B / 255.0f;

        // select material color
        if (xfRegs.color[0].MatSrc == 0)
        {
            mat[0] = (float)xfRegs.material[0].R / 255.0f;
            mat[1] = (float)xfRegs.material[0].G / 255.0f;
            mat[2] = (float)xfRegs.material[0].B / 255.0f;
        }
        else
        {
            mat[0] = col[0];
            mat[1] = col[1];
            mat[2] = col[2];
        }

        // calculate light function
        if (xfRegs.color[0].LightFunc)
        {
            int n;

            // select ambient color
            if (xfRegs.color[0].AmbSrc == 0)
            {
                amb[0] = (float)xfRegs.ambient[0].R / 255.0f;
                amb[1] = (float)xfRegs.ambient[0].G / 255.0f;
                amb[2] = (float)xfRegs.ambient[0].B / 255.0f;
            }
            else
            {
                amb[0] = col[0];
                amb[1] = col[1];
                amb[2] = col[2];
            }

            illum[0] = illum[1] = illum[2] = 0.0f;

            // calculate lights
            for (n = 0; n < 8; n++)
            {
                // check light mask
                if (xfRegs.colmask[n][0])
                {
                    // light color
                    col[0] = (float)xfRegs.light[n].color.R / 255.0f;
                    col[1] = (float)xfRegs.light[n].color.G / 255.0f;
                    col[2] = (float)xfRegs.light[n].color.B / 255.0f;

                    // calculate diffuse lighting
                    switch (xfRegs.color[0].DiffuseAtten)
                    {
                        case 0:         // identity
                            illum[0] += col[0];
                            illum[1] += col[1];
                            illum[2] += col[2];
                            break;

                        case 1:         // signed
                        case 2:         // clamped
                        {
                            float dp, dir[3];

                            // light direction vector
                            dir[0] = xfRegs.light[n].pos[0] - vpos[0];
                            dir[1] = xfRegs.light[n].pos[1] - vpos[1];
                            dir[2] = xfRegs.light[n].pos[2] - vpos[2];

                            // normalize light direction vector
                            VECNormalize(dir);

                            // normal transformation
                            NormalTransform(vnrm, v->nrm);

                            // dot product of normal and light
                            dp = vnrm[0] * dir[0] +
                                vnrm[1] * dir[1] +
                                vnrm[2] * dir[2];

                            // clamp dot product
                            if (xfRegs.color[0].DiffuseAtten == 2)
                            {
                                CLAMP0(dp);
                            }

                            // multiply by light color
                            illum[0] += dp * col[0];
                            illum[1] += dp * col[1];
                            illum[2] += dp * col[2];
                            break;
                        }
                    }

                    // diffuse angle and distance attenuation
                    // NOT USED !!

                    // specular
                    // NOT USED !!
                }
            }

            // clamp to [-1, 1] interval
            CLAMP(illum[0]);
            CLAMP(illum[1]);
            CLAMP(illum[2]);

            // add ambient color
            illum[0] += amb[0];
            illum[1] += amb[1];
            illum[2] += amb[2];

            // clamp total illum to [0, 1]
            CLAMP0(illum[0]);
            CLAMP0(illum[1]);
            CLAMP0(illum[2]);
        }
        else
        {
            // no light function, use material color
            illum[0] = illum[1] = illum[2] = 1.0f;
        }

        // finalize
        res[0] = mat[0] * illum[0];
        res[1] = mat[1] * illum[1];
        res[2] = mat[2] * illum[2];

        // clamp result to [0, 1]
        CLAMP0(res[0]);
        CLAMP0(res[1]);
        CLAMP0(res[2]);

        // write back result
        rasca[0].R = (uint8_t)(res[0] * 255.0f);
        rasca[0].G = (uint8_t)(res[1] * 255.0f);
        rasca[0].B = (uint8_t)(res[2] * 255.0f);

        // -------------------------------------------------------------------

        //
        // calculate alpha for channel 0
        //

        // convert vertex color to [0, 1] interval
        col[0] = (float)v->col[0].A / 255.0f;

        // select material color
        if (xfRegs.alpha[0].MatSrc == 0)
        {
            mat[0] = (float)xfRegs.material[0].A / 255.0f;
        }
        else
        {
            mat[0] = col[0];
        }

        // calculate light function
        if (xfRegs.alpha[0].LightFunc)
        {
            int n;

            // select ambient color
            if (xfRegs.alpha[0].AmbSrc == 0)
            {
                amb[0] = (float)xfRegs.ambient[0].A / 255.0f;
            }
            else
            {
                amb[0] = col[0];
            }

            illum[0] = 0.0f;

            // calculate lights
            for (n = 0; n < 8; n++)
            {
                // check light mask
                if (xfRegs.amask[n][0])
                {
                    // light color
                    col[0] = (float)xfRegs.light[n].color.A / 255.0f;

                    // calculate diffuse lighting
                    switch (xfRegs.alpha[0].DiffuseAtten)
                    {
                        case 0:         // identity
                            illum[0] += col[0];
                            break;

                        case 1:         // signed
                        case 2:         // clamped
                        {
                            float dp, dir[3];

                            // light direction vector
                            dir[0] = xfRegs.light[n].pos[0] - vpos[0];
                            dir[1] = xfRegs.light[n].pos[1] - vpos[1];
                            dir[2] = xfRegs.light[n].pos[2] - vpos[2];

                            // normalize light direction vector
                            VECNormalize(dir);

                            // normal transformation
                            NormalTransform(vnrm, v->nrm);

                            // dot product of normal and light
                            dp = vnrm[0] * dir[0] +
                                vnrm[1] * dir[1] +
                                vnrm[2] * dir[2];

                            // clamp dot product
                            if (xfRegs.alpha[0].DiffuseAtten == 2)
                            {
                                CLAMP0(dp);
                            }

                            // multiply by light color
                            illum[0] += dp * col[0];
                            break;
                        }
                    }

                    // diffuse angle and distance attenuation
                    // NOT USED !!

                    // specular
                    // NOT USED !!
                }
            }

            // clamp to [-1, 1] interval
            CLAMP(illum[0]);

            // add ambient color
            illum[0] += amb[0];

            // clamp total illum to [0, 1]
            CLAMP0(illum[0]);
        }
        else
        {
            // no light function, use material color
            illum[0] = 1.0f;
        }

        // finalize
        res[0] = mat[0] * illum[0];

        // clamp result to [0, 1]
        CLAMP0(res[0]);

        // write back result
        rasca[0].A = (uint8_t)(res[0] * 255.0f);

        // -------------------------------------------------------------------

        //
        // calculate color for channel 1
        //

        // convert vertex color to [0, 1] interval
        col[0] = (float)v->col[1].R / 255.0f;
        col[1] = (float)v->col[1].G / 255.0f;
        col[2] = (float)v->col[1].B / 255.0f;

        // select material color
        if (xfRegs.color[1].MatSrc == 0)
        {
            mat[0] = (float)xfRegs.material[1].R / 255.0f;
            mat[1] = (float)xfRegs.material[1].G / 255.0f;
            mat[2] = (float)xfRegs.material[1].B / 255.0f;
        }
        else
        {
            mat[0] = col[0];
            mat[1] = col[1];
            mat[2] = col[2];
        }

        // calculate light function
        if (xfRegs.color[1].LightFunc)
        {
            int n;

            // select ambient color
            if (xfRegs.color[1].AmbSrc == 0)
            {
                amb[0] = (float)xfRegs.ambient[1].R / 255.0f;
                amb[1] = (float)xfRegs.ambient[1].G / 255.0f;
                amb[2] = (float)xfRegs.ambient[1].B / 255.0f;
            }
            else
            {
                amb[0] = col[0];
                amb[1] = col[1];
                amb[2] = col[2];
            }

            illum[0] = illum[1] = illum[2] = 0.0f;

            // calculate lights
            for (n = 0; n < 8; n++)
            {
                // check light mask
                if (xfRegs.colmask[n][1])
                {
                    // light color
                    col[0] = (float)xfRegs.light[n].color.R / 255.0f;
                    col[1] = (float)xfRegs.light[n].color.G / 255.0f;
                    col[2] = (float)xfRegs.light[n].color.B / 255.0f;

                    // calculate diffuse lighting
                    switch (xfRegs.color[1].DiffuseAtten)
                    {
                        case 0:         // identity
                            illum[0] += col[0];
                            illum[1] += col[1];
                            illum[2] += col[2];
                            break;

                        case 1:         // signed
                        case 2:         // clamped
                        {
                            float dp, dir[3];

                            // light direction vector
                            dir[0] = xfRegs.light[n].pos[0] - vpos[0];
                            dir[1] = xfRegs.light[n].pos[1] - vpos[1];
                            dir[2] = xfRegs.light[n].pos[2] - vpos[2];

                            // normalize light direction vector
                            VECNormalize(dir);

                            // normal transformation
                            NormalTransform(vnrm, v->nrm);

                            // dot product of normal and light
                            dp = vnrm[0] * dir[0] +
                                vnrm[1] * dir[1] +
                                vnrm[2] * dir[2];

                            // clamp dot product
                            if (xfRegs.color[1].DiffuseAtten == 2)
                            {
                                CLAMP0(dp);
                            }

                            // multiply by light color
                            illum[0] += dp * col[0];
                            illum[1] += dp * col[1];
                            illum[2] += dp * col[2];
                            break;
                        }
                    }

                    // diffuse angle and distance attenuation
                    // NOT USED !!

                    // specular
                    // NOT USED !!
                }
            }

            // clamp to [-1, 1] interval
            CLAMP(illum[0]);
            CLAMP(illum[1]);
            CLAMP(illum[2]);

            // add ambient color
            illum[0] += amb[0];
            illum[1] += amb[1];
            illum[2] += amb[2];

            // clamp total illum to [0, 1]
            CLAMP0(illum[0]);
            CLAMP0(illum[1]);
            CLAMP0(illum[2]);
        }
        else
        {
            // no light function, use material color
            illum[0] = illum[1] = illum[2] = 1.0f;
        }

        // finalize
        res[0] = mat[0] * illum[0];
        res[1] = mat[1] * illum[1];
        res[2] = mat[2] * illum[2];

        // clamp result to [0, 1]
        CLAMP0(res[0]);
        CLAMP0(res[1]);
        CLAMP0(res[2]);

        // write back result
        rasca[1].R = (uint8_t)(res[0] * 255.0f);
        rasca[1].G = (uint8_t)(res[1] * 255.0f);
        rasca[1].B = (uint8_t)(res[2] * 255.0f);

        // -------------------------------------------------------------------

        //
        // calculate alpha for channel 0
        //

        // convert vertex color to [0, 1] interval
        col[0] = (float)v->col[1].A / 255.0f;

        // select material color
        if (xfRegs.alpha[1].MatSrc == 0)
        {
            mat[0] = (float)xfRegs.material[1].A / 255.0f;
        }
        else
        {
            mat[0] = col[0];
        }

        // calculate light function
        if (xfRegs.alpha[1].LightFunc)
        {
            int n;

            // select ambient color
            if (xfRegs.alpha[1].AmbSrc == 0)
            {
                amb[0] = (float)xfRegs.ambient[1].A / 255.0f;
            }
            else
            {
                amb[0] = col[0];
            }

            illum[0] = 0.0f;

            // calculate lights
            for (n = 0; n < 8; n++)
            {
                // check light mask
                if (xfRegs.amask[n][1])
                {
                    // light color
                    col[0] = (float)xfRegs.light[n].color.A / 255.0f;

                    // calculate diffuse lighting
                    switch (xfRegs.alpha[1].DiffuseAtten)
                    {
                        case 0:         // identity
                            illum[0] += col[0];
                            break;

                        case 1:         // signed
                        case 2:         // clamped
                        {
                            float dp, dir[3];

                            // light direction vector
                            dir[0] = xfRegs.light[n].pos[0] - vpos[0];
                            dir[1] = xfRegs.light[n].pos[1] - vpos[1];
                            dir[2] = xfRegs.light[n].pos[2] - vpos[2];

                            // normalize light direction vector
                            VECNormalize(dir);

                            // normal transformation
                            NormalTransform(vnrm, v->nrm);

                            // dot product of normal and light
                            dp = vnrm[0] * dir[0] +
                                vnrm[1] * dir[1] +
                                vnrm[2] * dir[2];

                            // clamp dot product
                            if (xfRegs.alpha[1].DiffuseAtten == 2)
                            {
                                CLAMP0(dp);
                            }

                            // multiply by light color
                            illum[0] += dp * col[0];
                            break;
                        }
                    }

                    // diffuse angle and distance attenuation
                    // NOT USED !!

                    // specular
                    // NOT USED !!
                }
            }

            // clamp to [-1, 1] interval
            CLAMP(illum[0]);

            // add ambient color
            illum[0] += amb[0];

            // clamp total illum to [0, 1]
            CLAMP0(illum[0]);
        }
        else
        {
            // no light function, use material color
            illum[0] = 1.0f;
        }

        // finalize
        res[0] = mat[0] * illum[0];

        // clamp result to [0, 1]
        CLAMP0(res[0]);

        // write back result
        rasca[1].A = (uint8_t)(res[0] * 255.0f);
    }

    // generate NUMTEX coordinates
    void GXCore::DoTexGen(const Vertex* v)
    {
        float   in[4], q;
        float* mx;

        if (state.xf.numTex == 0)
        {
            mx = &xfRegs.posmtx[xfRegs.texidx[0]][0];
            in[0] = v->tcoord[0][0];
            in[1] = v->tcoord[0][1];
            in[2] = 1.0f;
            in[3] = 1.0f;
        }

        for (unsigned n = 0; n < state.xf.numTex; n++)
        {
            if (xfRegs.texgen[n].type == 0)
            {
                // select inrow
                switch (xfRegs.texgen[n].src_row)
                {
                    case XF_TEXGEN_INROW_POSMTX:
                    {
                        in[0] = v->pos[0];
                        in[1] = v->pos[1];
                        in[2] = v->pos[2];
                        in[3] = 1.0f;
                    }
                    break;

                    case XF_TEXGEN_INROW_NORMAL:
                    {
                        in[0] = v->nrm[0];
                        in[1] = v->nrm[1];
                        in[2] = v->nrm[2];
                        in[3] = 1.0f;
                    }
                    break;

                    case XF_TEXGEN_INROW_TEX0:
                    {
                        mx = &xfRegs.posmtx[xfRegs.texidx[0]][0];
                        in[0] = v->tcoord[0][0];
                        in[1] = v->tcoord[0][1];
                        in[2] = 1.0f;
                        in[3] = 1.0f;
                    }
                    break;

                    case XF_TEXGEN_INROW_TEX1:
                    {
                        mx = &xfRegs.posmtx[xfRegs.texidx[1]][0];
                        in[0] = v->tcoord[1][0];
                        in[1] = v->tcoord[1][1];
                        in[2] = 1.0f;
                        in[3] = 1.0f;
                    }
                    break;

                    case XF_TEXGEN_INROW_TEX2:
                    {
                        mx = &xfRegs.posmtx[xfRegs.texidx[2]][0];
                        in[0] = v->tcoord[2][0];
                        in[1] = v->tcoord[2][1];
                        in[2] = 1.0f;
                        in[3] = 1.0f;
                    }
                    break;

                    case XF_TEXGEN_INROW_TEX3:
                    {
                        mx = &xfRegs.posmtx[xfRegs.texidx[3]][0];
                        in[0] = v->tcoord[3][0];
                        in[1] = v->tcoord[3][1];
                        in[2] = 1.0f;
                        in[3] = 1.0f;
                    }
                    break;

                    case XF_TEXGEN_INROW_TEX4:
                    {
                        mx = &xfRegs.posmtx[xfRegs.texidx[4]][0];
                        in[0] = v->tcoord[4][0];
                        in[1] = v->tcoord[4][1];
                        in[2] = 1.0f;
                        in[3] = 1.0f;
                    }
                    break;

                    case XF_TEXGEN_INROW_TEX5:
                    {
                        mx = &xfRegs.posmtx[xfRegs.texidx[5]][0];
                        in[0] = v->tcoord[5][0];
                        in[1] = v->tcoord[5][1];
                        in[2] = 1.0f;
                        in[3] = 1.0f;
                    }
                    break;

                    case XF_TEXGEN_INROW_TEX6:
                    {
                        mx = &xfRegs.posmtx[xfRegs.texidx[6]][0];
                        in[0] = v->tcoord[6][0];
                        in[1] = v->tcoord[6][1];
                        in[2] = 1.0f;
                        in[3] = 1.0f;
                    }
                    break;

                    case XF_TEXGEN_INROW_TEX7:
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
                if (xfRegs.texgen[n].pojection)
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



    // load projection matrix
    void GXCore::GL_SetProjection(float* mtx)
    {
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf((GLfloat*)mtx);
        glMatrixMode(GL_MODELVIEW);
    }

    void GXCore::GL_SetViewport(int x, int y, int w, int h, float znear, float zfar)
    {
        //h += 32;
#ifndef NO_VIEWPORT
        glViewport(x, scr_h - (h + y), w, h);
        glDepthRange(znear, zfar);
#endif
    }

    // index range = 0000..FFFF
    // reg size = 32 bit
    void GXCore::loadXFRegs(size_t startIdx, size_t amount, FifoProcessor* gxfifo)
    {
        state.xfLoads += (uint32_t)amount;

        if (GpRegsLog)
        {
            Report(Channel::GP, "XF load, start index: %04X, n : %i\n", startIdx, amount);
        }

        // load geometry matrix
        if ((startIdx >= 0x0000) && (startIdx < 0x0400))
        {
            for (size_t i = 0; i < amount; i++)
            {
                *(float*)(((uint8_t*)xfRegs.posmtx + 4 * startIdx) + 4 * i) = gxfifo->ReadFloat();
            }
        }
        // load normal matrix
        else if ((startIdx >= 0x0400) && (startIdx < 0x0500))
        {
            for (size_t i = 0; i < amount; i++)
            {
                *(float*)(((uint8_t*)xfRegs.nrmmtx + 4 * (startIdx - 0x400)) + 4 * i) = gxfifo->ReadFloat();
            }
        }
        // load post-trans matrix
        else if ((startIdx >= 0x0500) && (startIdx < 0x0600))
        {
            for (size_t i = 0; i < amount; i++)
            {
                *(float*)(((uint8_t*)xfRegs.postmtx + 4 * (startIdx - 0x500)) + 4 * i) = gxfifo->ReadFloat();
            }
        }
        else switch (startIdx)
        {
            //
            // set matrix index
            //

            case XF_MATINDEX_A:
            {
                xfRegs.matidxA.matidx = gxfifo->Read32();
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
                xfRegs.matidxB.matidx = gxfifo->Read32();
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
                float pMatrix[7];

                pMatrix[0] = gxfifo->ReadFloat();
                pMatrix[1] = gxfifo->ReadFloat();
                pMatrix[2] = gxfifo->ReadFloat();
                pMatrix[3] = gxfifo->ReadFloat();
                pMatrix[4] = gxfifo->ReadFloat();
                pMatrix[5] = gxfifo->ReadFloat();
                pMatrix[6] = gxfifo->ReadFloat();

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

                GL_SetProjection((float*)Matrix);
            }
            return;

            //
            // load viewport configuration
            // 

            case XF_VIEWPORT:
            {
                float w, h, x, y, zf, zn;

                //
                // read coefficients
                //

                xfRegs.vp_scale[0] = gxfifo->ReadFloat();   // w / 2
                xfRegs.vp_scale[1] = gxfifo->ReadFloat();   // -h / 2
                xfRegs.vp_scale[2] = gxfifo->ReadFloat();   // ZMAX * (zfar - znear)

                xfRegs.vp_offs[0] = gxfifo->ReadFloat();    // x + w/2 + 342
                xfRegs.vp_offs[1] = gxfifo->ReadFloat();    // y + h/2 + 342
                xfRegs.vp_offs[2] = gxfifo->ReadFloat();    // ZMAX * zfar

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
                GL_SetViewport((int)x, (int)y, (int)w, (int)h, zn, zf);
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

                xfRegs.light[lnum].rsrv[0] = gxfifo->Read32();
                xfRegs.light[lnum].rsrv[1] = gxfifo->Read32();
                xfRegs.light[lnum].rsrv[2] = gxfifo->Read32();
                xfRegs.light[lnum].color.RGBA = gxfifo->Read32();

                xfRegs.light[lnum].a[0] = gxfifo->ReadFloat();
                xfRegs.light[lnum].a[1] = gxfifo->ReadFloat();
                xfRegs.light[lnum].a[2] = gxfifo->ReadFloat();

                xfRegs.light[lnum].k[0] = gxfifo->ReadFloat();
                xfRegs.light[lnum].k[1] = gxfifo->ReadFloat();
                xfRegs.light[lnum].k[2] = gxfifo->ReadFloat();

                xfRegs.light[lnum].pos[0] = gxfifo->ReadFloat();
                xfRegs.light[lnum].pos[1] = gxfifo->ReadFloat();
                xfRegs.light[lnum].pos[2] = gxfifo->ReadFloat();

                xfRegs.light[lnum].dir[0] = gxfifo->ReadFloat();
                xfRegs.light[lnum].dir[1] = gxfifo->ReadFloat();
                xfRegs.light[lnum].dir[2] = gxfifo->ReadFloat();
            }
            return;

            case XF_INVTXSPEC:
            {
                xfRegs.vtxSpec.bits = gxfifo->Read32();
            }
            return;

            //
            // channel constant color registers
            //

            case XF_AMBIENT0:
            {
                xfRegs.ambient[0].RGBA = gxfifo->Read32();
            }
            return;

            case XF_AMBIENT1:
            {
                xfRegs.ambient[1].RGBA = gxfifo->Read32();
            }
            return;

            case XF_MATERIAL0:
            {
                xfRegs.material[0].RGBA = gxfifo->Read32();
            }
            return;

            case XF_MATERIAL1:
            {
                xfRegs.material[1].RGBA = gxfifo->Read32();
            }
            return;

            //
            // channel control registers
            //

            case XF_COLOR0CNTL:
            {
                xfRegs.color[0].Chan = gxfifo->Read32();

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
                xfRegs.color[1].Chan = gxfifo->Read32();

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
                xfRegs.alpha[0].Chan = gxfifo->Read32();

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
                xfRegs.alpha[1].Chan = gxfifo->Read32();

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
                xfRegs.dualtex = gxfifo->Read32();
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
                size_t n = startIdx - XF_DUALGEN0;

                gxfifo->Read32();

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
                xfRegs.numcol = gxfifo->Read32();
            }
            return;

            //
            // set number of texgens
            //

            case XF_NUMTEX:
            {
                xfRegs.numtex = gxfifo->Read32();
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

                static  const char* prj[] = { "2x4", "3x4" };
                static  const char* inf[] = { "ab11", "abc1" };
                static  const char* type[] = { "regular", "bump", "toon0", "toon1" };
                static  const char* srcrow[] = {
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

                xfRegs.texgen[num].hex = gxfifo->Read32();

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
                Report(Channel::GP, "Unknown XF load, start index: 0x%04X, count: %i\n", startIdx, amount);

                while (amount--)
                {
                    gxfifo->Read32();
                }
            }
        }
    }

}
