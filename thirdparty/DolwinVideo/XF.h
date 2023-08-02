#pragma once

extern  Color   rasca[2];

void    DoLights(const Vertex* v);

typedef struct
{
    float   out[4];
} TexGenOut;

extern  TexGenOut   tgout[8];

void    DoTexGen(const Vertex* v);

void    VECNormalize(float vec[3]);
void    ApplyModelview(float *out, const float *in);
void    NormalTransform(float *out, const float *in);
