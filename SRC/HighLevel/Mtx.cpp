// GC Vector/Matrix math library emulation, powered by MMX/SSE
// MMX is used for fast copy, SSE for paired-single math
#include "pch.h"

#define PARAM(n)    GPR[3+n]
#define RET_VAL     GPR[3]
#define SWAP        MEMSwap

// pre-swapped 1.0f and 0.0f
#define ONE         0x803f
#define ZERO        0

typedef struct Matrix
{
    uint32_t     data[3][4];
} Matrix, *MatrixPtr;
typedef struct MatrixF
{
    float     data[3][4];
} MatrixF, *MatrixFPtr;

#define MTX(mx)  mx->data

static void print_mtx(MatrixPtr ptr, const char *name="")
{
    MatrixFPtr m = (MatrixFPtr)ptr;

    MEMSwapArea((uint32_t *)ptr, 3*4*4);
    DolwinReport( "%s\n"
                  "%f %f %f %f\n"
                  "%f %f %f %f\n"
                  "%f %f %f %f\n",

                  name,
                  MTX(m)[0][0], MTX(m)[0][1], MTX(m)[0][2], MTX(m)[0][3],
                  MTX(m)[1][0], MTX(m)[1][1], MTX(m)[1][2], MTX(m)[1][3],
                  MTX(m)[2][0], MTX(m)[2][1], MTX(m)[2][2], MTX(m)[2][3] );
    MEMSwapArea((uint32_t *)ptr, 3*4*4);
}

/* ---------------------------------------------------------------------------
    Init layer
--------------------------------------------------------------------------- */

void MTXOpen()
{
    BOOL flag = GetConfigInt(USER_HLE_MTX, USER_HLE_MTX_DEFAULT);
    if(flag == FALSE) return;

    DBReport( GREEN "Geometry library install (extensions MMX:%i, SSE:%i).\n",
              cpu.mmx, cpu.sse );

    // select between multimedia extension and C
    if(cpu.mmx)
    {
        HLESetCall("C_MTXIdentity",             SIMD_MTXIdentity);
        HLESetCall("PSMTXIdentity",             SIMD_MTXIdentity);
        HLESetCall("C_MTXCopy",                 SIMD_MTXCopy);
        HLESetCall("PSMTXCopy",                 SIMD_MTXCopy);
    }
    else
    {
        HLESetCall("C_MTXIdentity",             C_MTXIdentity);
        HLESetCall("PSMTXIdentity",             C_MTXIdentity);
        HLESetCall("C_MTXCopy",                 C_MTXCopy);
        HLESetCall("PSMTXCopy",                 C_MTXCopy);
    }

    // select between streamed SIMD extension and C
#ifdef  __VCNET__
    if(cpu.sse)
    {
        HLESetCall("C_MTXConcat",               SIMD_MTXConcat);
        HLESetCall("PSMTXConcat",               SIMD_MTXConcat);
        HLESetCall("C_MTXTranspose",            SIMD_MTXTranspose);
        HLESetCall("PSMTXTranspose",            SIMD_MTXTranspose);
    }
    else
#endif  // __VCNET__
    {
        HLESetCall("C_MTXConcat",               C_MTXConcat);
        HLESetCall("PSMTXConcat",               C_MTXConcat);
        HLESetCall("C_MTXTranspose",            C_MTXTranspose);
        HLESetCall("PSMTXTranspose",            C_MTXTranspose);
    }

    DBReport("\n");
}

/* ---------------------------------------------------------------------------
    General stuff
--------------------------------------------------------------------------- */

static Matrix tmpMatrix[4];

void C_MTXIdentity(void)
{
    HLEHit(HLE_MTX_IDENTITY);

    MatrixPtr m = (MatrixPtr)(&RAM[PARAM(0) & RAMMASK]);

    MTX(m)[0][0] = ONE;  MTX(m)[0][1] = ZERO; MTX(m)[0][2] = ZERO; MTX(m)[0][3] = ZERO;
    MTX(m)[1][0] = ZERO; MTX(m)[1][1] = ONE;  MTX(m)[1][2] = ZERO; MTX(m)[1][3] = ZERO;
    MTX(m)[2][0] = ZERO; MTX(m)[2][1] = ZERO; MTX(m)[2][2] = ONE;  MTX(m)[2][3] = ZERO;
}

void C_MTXCopy(void)
{
    HLEHit(HLE_MTX_COPY);

    MatrixPtr src = (MatrixPtr)(&RAM[PARAM(0) & RAMMASK]);
    MatrixPtr dst = (MatrixPtr)(&RAM[PARAM(1) & RAMMASK]);

    if(src == dst) return;

    MTX(dst)[0][0] = MTX(src)[0][0]; MTX(dst)[0][1] = MTX(src)[0][1]; MTX(dst)[0][2] = MTX(src)[0][2]; MTX(dst)[0][3] = MTX(src)[0][3];
    MTX(dst)[1][0] = MTX(src)[1][0]; MTX(dst)[1][1] = MTX(src)[1][1]; MTX(dst)[1][2] = MTX(src)[1][2]; MTX(dst)[1][3] = MTX(src)[1][3];
    MTX(dst)[2][0] = MTX(src)[2][0]; MTX(dst)[2][1] = MTX(src)[2][1]; MTX(dst)[2][2] = MTX(src)[2][2]; MTX(dst)[2][3] = MTX(src)[2][3];

    //print_mtx((MatrixPtr)src, "src C");
    //print_mtx((MatrixPtr)dst, "dst C");
}

void C_MTXConcat(void)
{
    HLEHit(HLE_MTX_CONCAT);

    MatrixFPtr a = (MatrixFPtr)(&RAM[PARAM(0) & RAMMASK]);
    MatrixFPtr b = (MatrixFPtr)(&RAM[PARAM(1) & RAMMASK]);
    MatrixFPtr axb = (MatrixFPtr)(&RAM[PARAM(2) & RAMMASK]);
    MatrixFPtr t = (MatrixFPtr)(&tmpMatrix[0]), m;

    if( (axb == a) || (axb == b) ) m = t;
    else m = axb;

    //print_mtx((MatrixPtr)a, "a C");
    //print_mtx((MatrixPtr)b, "b C");

    MEMSwapArea((uint32_t *)a, 3*4*4);
    MEMSwapArea((uint32_t *)b, 3*4*4);

    // m = a x b
    MTX(m)[0][0] = MTX(a)[0][0]*MTX(b)[0][0] + MTX(a)[0][1]*MTX(b)[1][0] + MTX(a)[0][2]*MTX(b)[2][0];
    MTX(m)[0][1] = MTX(a)[0][0]*MTX(b)[0][1] + MTX(a)[0][1]*MTX(b)[1][1] + MTX(a)[0][2]*MTX(b)[2][1];
    MTX(m)[0][2] = MTX(a)[0][0]*MTX(b)[0][2] + MTX(a)[0][1]*MTX(b)[1][2] + MTX(a)[0][2]*MTX(b)[2][2];
    MTX(m)[0][3] = MTX(a)[0][0]*MTX(b)[0][3] + MTX(a)[0][1]*MTX(b)[1][3] + MTX(a)[0][2]*MTX(b)[2][3] + MTX(a)[0][3];

    MTX(m)[1][0] = MTX(a)[1][0]*MTX(b)[0][0] + MTX(a)[1][1]*MTX(b)[1][0] + MTX(a)[1][2]*MTX(b)[2][0];
    MTX(m)[1][1] = MTX(a)[1][0]*MTX(b)[0][1] + MTX(a)[1][1]*MTX(b)[1][1] + MTX(a)[1][2]*MTX(b)[2][1];
    MTX(m)[1][2] = MTX(a)[1][0]*MTX(b)[0][2] + MTX(a)[1][1]*MTX(b)[1][2] + MTX(a)[1][2]*MTX(b)[2][2];
    MTX(m)[1][3] = MTX(a)[1][0]*MTX(b)[0][3] + MTX(a)[1][1]*MTX(b)[1][3] + MTX(a)[1][2]*MTX(b)[2][3] + MTX(a)[1][3];

    MTX(m)[2][0] = MTX(a)[2][0]*MTX(b)[0][0] + MTX(a)[2][1]*MTX(b)[1][0] + MTX(a)[2][2]*MTX(b)[2][0];
    MTX(m)[2][1] = MTX(a)[2][0]*MTX(b)[0][1] + MTX(a)[2][1]*MTX(b)[1][1] + MTX(a)[2][2]*MTX(b)[2][1];
    MTX(m)[2][2] = MTX(a)[2][0]*MTX(b)[0][2] + MTX(a)[2][1]*MTX(b)[1][2] + MTX(a)[2][2]*MTX(b)[2][2];
    MTX(m)[2][3] = MTX(a)[2][0]*MTX(b)[0][3] + MTX(a)[2][1]*MTX(b)[1][3] + MTX(a)[2][2]*MTX(b)[2][3] + MTX(a)[2][3];

    // restore A and B
    MEMSwapArea((uint32_t *)a, 3*4*4);
    MEMSwapArea((uint32_t *)b, 3*4*4);
    MEMSwapArea((uint32_t *)m, 3*4*4);

    //print_mtx((MatrixPtr)m, "m C");

    // overwrite a (b)
    if(m == t)
    {
        MatrixPtr src = (MatrixPtr)t, dst = (MatrixPtr)axb;
        if(src != dst)
        {
            MTX(dst)[0][0] = MTX(src)[0][0]; MTX(dst)[0][1] = MTX(src)[0][1]; MTX(dst)[0][2] = MTX(src)[0][2]; MTX(dst)[0][3] = MTX(src)[0][3];
            MTX(dst)[1][0] = MTX(src)[1][0]; MTX(dst)[1][1] = MTX(src)[1][1]; MTX(dst)[1][2] = MTX(src)[1][2]; MTX(dst)[1][3] = MTX(src)[1][3];
            MTX(dst)[2][0] = MTX(src)[2][0]; MTX(dst)[2][1] = MTX(src)[2][1]; MTX(dst)[2][2] = MTX(src)[2][2]; MTX(dst)[2][3] = MTX(src)[2][3];
        }
    }
}

void C_MTXTranspose(void)
{
    HLEHit(HLE_MTX_TRANSPOSE);

    MatrixPtr src = (MatrixPtr)(&RAM[PARAM(0) & RAMMASK]);
    MatrixPtr xPose = (MatrixPtr)(&RAM[PARAM(1) & RAMMASK]);
    MatrixPtr t = (&tmpMatrix[0]), m;

    if(src == xPose) m = t;
    else m = xPose;

    MTX(m)[0][0] = MTX(src)[0][0]; MTX(m)[0][1] = MTX(src)[1][0]; MTX(m)[0][2] = MTX(src)[2][0]; MTX(m)[0][3] = ZERO;
    MTX(m)[1][0] = MTX(src)[0][1]; MTX(m)[1][1] = MTX(src)[1][1]; MTX(m)[1][2] = MTX(src)[2][1]; MTX(m)[1][3] = ZERO;
    MTX(m)[2][0] = MTX(src)[0][2]; MTX(m)[2][1] = MTX(src)[1][2]; MTX(m)[2][2] = MTX(src)[2][2]; MTX(m)[2][3] = ZERO;

    //print_mtx((MatrixPtr)m, "m C");

    // overwrite
    if(m == t)
    {
        MatrixPtr src = t, dst = xPose;
        if(src != dst)
        {
            MTX(dst)[0][0] = MTX(src)[0][0]; MTX(dst)[0][1] = MTX(src)[0][1]; MTX(dst)[0][2] = MTX(src)[0][2]; MTX(dst)[0][3] = MTX(src)[0][3];
            MTX(dst)[1][0] = MTX(src)[1][0]; MTX(dst)[1][1] = MTX(src)[1][1]; MTX(dst)[1][2] = MTX(src)[1][2]; MTX(dst)[1][3] = MTX(src)[1][3];
            MTX(dst)[2][0] = MTX(src)[2][0]; MTX(dst)[2][1] = MTX(src)[2][1]; MTX(dst)[2][2] = MTX(src)[2][2]; MTX(dst)[2][3] = MTX(src)[2][3];
        }
    }
}

void C_MTXInverse(void)
{
    HLEHit(HLE_MTX_INVERSE);

}

void C_MTXInvXpose(void)
{
    HLEHit(HLE_MTX_INVXPOSE);

}

// SIMD version

void SIMD_MTXIdentity(void)
{
    HLEHit(HLE_MTX_IDENTITY);

    static uint64_t konst10 = 0x803f;
    static uint64_t konst01 = 0x803f00000000;
    MatrixPtr m = (MatrixPtr)(&RAM[PARAM(0) & RAMMASK]);

    __asm   mov     eax, dword ptr m
    __asm   pxor    mm0, mm0        // 0.0f  0.0f
    __asm   movq    mm1, konst10    // 1.0f  0.0f
    __asm   movq    mm2, konst01    // 0.0f  1.0f
    __asm   movq    [eax+0], mm1
    __asm   movq    [eax+8], mm0
    __asm   movq    [eax+16], mm2
    __asm   movq    [eax+24], mm0
    __asm   movq    [eax+32], mm0
    __asm   movq    [eax+40], mm1
    __asm   emms
}

// eax is used because of smaller code size and speed
#define COPY_MTX(s, d)                  \
{                                       \
    __asm   mov     eax, dword ptr s    \
    __asm   mov     edx, dword ptr d    \
    __asm   movq    mm0, [eax+0]        \
    __asm   movq    [edx+0], mm0        \
    __asm   movq    mm0, [eax+8]        \
    __asm   movq    [edx+8], mm0        \
    __asm   movq    mm0, [eax+16]       \
    __asm   movq    [edx+16], mm0       \
    __asm   movq    mm0, [eax+24]       \
    __asm   movq    [edx+24], mm0       \
    __asm   movq    mm0, [eax+32]       \
    __asm   movq    [edx+32], mm0       \
    __asm   movq    mm0, [eax+40]       \
    __asm   movq    [edx+40], mm0       \
    __asm   emms                        \
}

void SIMD_MTXCopy(void)
{
    HLEHit(HLE_MTX_COPY);

    MatrixPtr src = (MatrixPtr)(&RAM[PARAM(0) & RAMMASK]);
    MatrixPtr dst = (MatrixPtr)(&RAM[PARAM(1) & RAMMASK]);

    if(src == dst) return;

    COPY_MTX(src, dst);

    //print_mtx((MatrixPtr)src, "src");
    //print_mtx((MatrixPtr)dst, "dst");
}

#ifdef  __VCNET__

#define CONCAT_ROW(ofs)                         \
{                                               \
    __asm   movss   xmm0, [eax+ofs]             \
    __asm   shufps  xmm0, xmm0, 0               \
    __asm   movups  xmm1, [ecx]                 \
    __asm   mulps   xmm0, xmm1                  \
    __asm   movaps  xmm2, xmm0                  \
                                                \
    __asm   movss   xmm0, [eax+ofs+4]           \
    __asm   shufps  xmm0, xmm0, 0               \
    __asm   movups  xmm1, [ecx+16]              \
    __asm   mulps   xmm0, xmm1                  \
    __asm   addps   xmm2, xmm0                  \
                                                \
    __asm   movss   xmm0, [eax+ofs+8]           \
    __asm   shufps  xmm0, xmm0, 0               \
    __asm   movups  xmm1, [ecx+32]              \
    __asm   mulps   xmm0, xmm1                  \
    __asm   addps   xmm2, xmm0                  \
    __asm   movups  [edx+ofs], xmm2             \
}

void SIMD_MTXConcat(void)
{
    HLEHit(HLE_MTX_CONCAT);

    MatrixFPtr a = (MatrixFPtr)(&RAM[PARAM(0) & RAMMASK]);
    MatrixFPtr b = (MatrixFPtr)(&RAM[PARAM(1) & RAMMASK]);
    MatrixFPtr axb = (MatrixFPtr)(&RAM[PARAM(2) & RAMMASK]);
    MatrixFPtr t = (MatrixFPtr)(&tmpMatrix[0]), m;

    if( (axb == a) || (axb == b) ) m = t;
    else m = axb;

    //print_mtx((MatrixPtr)a, "a");
    //print_mtx((MatrixPtr)b, "b");

    MEMSwapArea((uint32_t *)a, 3*4*4);
    MEMSwapArea((uint32_t *)b, 3*4*4);

    // m = a x b. SSE gives 7 movs, 3 adds, 3 muls (shufps swaps are neglegible)
    __asm   mov     eax, dword ptr a
    __asm   mov     ecx, dword ptr b
    __asm   mov     edx, dword ptr m

    CONCAT_ROW(0);
    CONCAT_ROW(16);
    CONCAT_ROW(32);
    __asm   emms

    MTX(m)[0][3] += MTX(a)[0][3];
    MTX(m)[1][3] += MTX(a)[1][3];
    MTX(m)[2][3] += MTX(a)[2][3];

    // restore A and B
    MEMSwapArea((uint32_t *)a, 3*4*4);
    MEMSwapArea((uint32_t *)b, 3*4*4);
    MEMSwapArea((uint32_t *)m, 3*4*4);

    //print_mtx((MatrixPtr)m, "m");

    // overwrite a (b)
    if(m == t)
    {
        MatrixPtr src = (MatrixPtr)t, dst = (MatrixPtr)axb;
        if(src != dst)
        {
            COPY_MTX(src, dst);
        }
    }
}

void SIMD_MTXTranspose(void)
{
    HLEHit(HLE_MTX_TRANSPOSE);

    MatrixPtr src = (MatrixPtr)(&RAM[PARAM(0) & RAMMASK]);
    MatrixPtr xPose = (MatrixPtr)(&RAM[PARAM(1) & RAMMASK]);
    MatrixPtr t = (&tmpMatrix[0]), m;

    if(src == xPose) m = t;
    else m = xPose;

    __asm   mov     eax, dword ptr src
    __asm   mov     edx, dword ptr m

    // src : 
    // [0]  [1]  [2]  [3]
    // [4]  [5]  [6]  [7]
    // [8]  [9]  [10] [11]

    // m should be:
    // [0]  [4]  [8]  [x]
    // [1]  [5]  [9]  [x]
    // [2]  [6]  [10] [x]

    // 3x4 matrix transpose in SSE, only 12 movs :)
    __asm   movlps  xmm4, [eax]
    __asm   movhps  xmm4, [eax+4*4]     // tmp : [5] [4] [1] [0]
    __asm   movaps  xmm5, xmm4
    __asm   movlps  xmm1, [eax+8*4]     // row1: [?] [?] [9] [8]
    __asm   shufps  xmm4, xmm1, 0x88
    __asm   movaps  xmm0, xmm4          // row0: [?] [8] [4] [0]        [0]
    __asm   shufps  xmm1, xmm5, 0xDD    // row1: [?] [9] [5] [1]        [1]

    __asm   movlps  xmm4, [eax+2*4]
    __asm   movhps  xmm4, [eax+6*4]     // tmp : [7] [6] [3] [2]
    __asm   movlps  xmm3, [eax+10*4]    // row3: [?] [?] [11] [10]
    __asm   shufps  xmm4, xmm3, 0x88
    __asm   movaps  xmm2, xmm4          // row2: [?] [10] [6] [2]       [2]

    __asm   movups  [edx], xmm0
    __asm   movups  [edx+4*4], xmm1
    __asm   movups  [edx+8*4], xmm2

    __asm   emms

    MTX(m)[0][3] = ZERO;
    MTX(m)[1][3] = ZERO;
    MTX(m)[2][3] = ZERO;

    //print_mtx((MatrixPtr)m, "m");

    // overwrite
    if(m == t)
    {
        MatrixPtr src = t, dst = xPose;
        if(src != dst)
        {
            COPY_MTX(src, dst);
        }
    }
}

void SIMD_MTXInverse(void)
{
}

void SIMD_MTXInvXpose(void)
{
}

#endif  // __VCNET__
