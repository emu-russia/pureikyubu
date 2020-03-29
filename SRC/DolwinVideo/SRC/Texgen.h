typedef struct
{
    float   out[4];
} TexGenOut;

extern  TexGenOut   tgout[8];

void    DoTexGen(const Vertex *v);
