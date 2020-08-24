// some math calculations
#include "pch.h"

TexGenOut   tgout[8];

// lighting stage output colors
Color   rasca[2];

// normalize (clamp vector to 1.0 length)
void VECNormalize(float vec[3])
{
    float d = (float)sqrt(
        vec[0] * vec[0] + 
        vec[1] * vec[1] + 
        vec[2] * vec[2]
    );

    vec[0] /= d;
    vec[1] /= d;
    vec[2] /= d;
}

// perform position transform
void ApplyModelview(float *out, const float *in)
{
    float *mx = &xfRegs.posmtx[xfRegs.posidx][0];
  
    // TODO : use sse if possible
    out[0] = in[0] * mx[0] + in[1] * mx[1] + in[2] * mx[2] + mx[3];
    out[1] = in[0] * mx[4] + in[1] * mx[5] + in[2] * mx[6] + mx[7];
    out[2] = in[0] * mx[8] + in[1] * mx[9] + in[2] * mx[10] + mx[11];
}

// perform normal transform
// matrix must be the inverse transpose of the modelview matrix
void NormalTransform(float *out, const float *in)
{
    float *mx = &xfRegs.nrmmtx[xfRegs.posidx][0];

    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
  
    // TODO : use sse if possible
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
void DoLights(const Vertex *v)
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
    if(xfRegs.color[0].MatSrc == 0)
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
    if(xfRegs.color[0].LightFunc)
    {
        int n;

        // select ambient color
        if(xfRegs.color[0].AmbSrc == 0)
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
        for(n=0; n<8; n++)
        {
            // check light mask
            if(xfRegs.colmask[n][0])
            {
                // light color
                col[0] = (float)xfRegs.light[n].color.R / 255.0f;
                col[1] = (float)xfRegs.light[n].color.G / 255.0f;
                col[2] = (float)xfRegs.light[n].color.B / 255.0f;
                
                // calculate diffuse lighting
                switch(xfRegs.color[0].DiffuseAtten)
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
                             vnrm[2] * dir[2] ;

                        // clamp dot product
                        if(xfRegs.color[0].DiffuseAtten == 2)
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
    if(xfRegs.alpha[0].MatSrc == 0)
    {
        mat[0] = (float)xfRegs.material[0].A / 255.0f;
    }
    else
    {
        mat[0] = col[0];
    }

    // calculate light function
    if(xfRegs.alpha[0].LightFunc)
    {
        int n;

        // select ambient color
        if(xfRegs.alpha[0].AmbSrc == 0)
        {
            amb[0] = (float)xfRegs.ambient[0].A / 255.0f;
        }
        else
        {
            amb[0] = col[0];
        }

        illum[0] = 0.0f;

        // calculate lights
        for(n=0; n<8; n++)
        {
            // check light mask
            if(xfRegs.amask[n][0])
            {
                // light color
                col[0] = (float)xfRegs.light[n].color.A / 255.0f;
                
                // calculate diffuse lighting
                switch(xfRegs.alpha[0].DiffuseAtten)
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
                             vnrm[2] * dir[2] ;

                        // clamp dot product
                        if(xfRegs.alpha[0].DiffuseAtten == 2)
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
    if(xfRegs.color[1].MatSrc == 0)
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
    if(xfRegs.color[1].LightFunc)
    {
        int n;

        // select ambient color
        if(xfRegs.color[1].AmbSrc == 0)
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
        for(n=0; n<8; n++)
        {
            // check light mask
            if(xfRegs.colmask[n][1])
            {
                // light color
                col[0] = (float)xfRegs.light[n].color.R / 255.0f;
                col[1] = (float)xfRegs.light[n].color.G / 255.0f;
                col[2] = (float)xfRegs.light[n].color.B / 255.0f;
                
                // calculate diffuse lighting
                switch(xfRegs.color[1].DiffuseAtten)
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
                             vnrm[2] * dir[2] ;

                        // clamp dot product
                        if(xfRegs.color[1].DiffuseAtten == 2)
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
    if(xfRegs.alpha[1].MatSrc == 0)
    {
        mat[0] = (float)xfRegs.material[1].A / 255.0f;
    }
    else
    {
        mat[0] = col[0];
    }

    // calculate light function
    if(xfRegs.alpha[1].LightFunc)
    {
        int n;

        // select ambient color
        if(xfRegs.alpha[1].AmbSrc == 0)
        {
            amb[0] = (float)xfRegs.ambient[1].A / 255.0f;
        }
        else
        {
            amb[0] = col[0];
        }

        illum[0] = 0.0f;

        // calculate lights
        for(n=0; n<8; n++)
        {
            // check light mask
            if(xfRegs.amask[n][1])
            {
                // light color
                col[0] = (float)xfRegs.light[n].color.A / 255.0f;
                
                // calculate diffuse lighting
                switch(xfRegs.alpha[1].DiffuseAtten)
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
                             vnrm[2] * dir[2] ;

                        // clamp dot product
                        if(xfRegs.alpha[1].DiffuseAtten == 2)
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
void DoTexGen(const Vertex* v)
{
    float   in[4], q;
    float* mx;

    if (xfRegs.numtex == 0)
    {
        mx = &xfRegs.posmtx[xfRegs.texidx[0]][0];
        in[0] = v->tcoord[0][0];
        in[1] = v->tcoord[0][1];
        in[2] = 1.0f;
        in[3] = 1.0f;
    }

    for (unsigned n = 0; n < xfRegs.numtex; n++)
    {
        if (xfRegs.texgen[n].type == 0)
        {
            // select inrow
            switch (xfRegs.texgen[n].src_row)
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
