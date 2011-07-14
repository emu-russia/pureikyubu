// some math calculations
#include "GX.h"

// normalize (clamp vector to 1.0 length)
void VECNormalize(float *vec)
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
