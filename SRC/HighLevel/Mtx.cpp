// DolphinSDK Vector/Matrix math library emulation.

// The modern x64 SSE optimizer should optimize such code pretty well, without any ritual squats.

#include "pch.h"

using namespace Debug;

#define PARAM(n)    Core->regs.gpr[3+n]
#define RET_VAL     Core->regs.gpr[3]
#define SWAP        _BYTESWAP_UINT32

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

static void print_mtx(MatrixPtr ptr)
{
    MatrixFPtr m = (MatrixFPtr)ptr;

    Gekko::GekkoCore::SwapArea((uint32_t *)ptr, 3*4*4);
    Report(Channel::Norm,
        "%f %f %f %f\n"
        "%f %f %f %f\n"
        "%f %f %f %f\n",

        MTX(m)[0][0], MTX(m)[0][1], MTX(m)[0][2], MTX(m)[0][3],
        MTX(m)[1][0], MTX(m)[1][1], MTX(m)[1][2], MTX(m)[1][3],
        MTX(m)[2][0], MTX(m)[2][1], MTX(m)[2][2], MTX(m)[2][3] );
    Gekko::GekkoCore::SwapArea((uint32_t *)ptr, 3*4*4);
}

/* ---------------------------------------------------------------------------
    Init layer
--------------------------------------------------------------------------- */

void MTXOpen()
{
    bool flag = false;//GetConfigInt(USER_HLE_MTX, USER_HLE_MTX_DEFAULT);
    if(flag == false) return;

    Report( Channel::HLE, "Geometry library install\n");

    HLESetCall("C_MTXIdentity",             C_MTXIdentity);
    HLESetCall("PSMTXIdentity",             C_MTXIdentity);
    HLESetCall("C_MTXCopy",                 C_MTXCopy);
    HLESetCall("PSMTXCopy",                 C_MTXCopy);

    HLESetCall("C_MTXConcat",               C_MTXConcat);
    HLESetCall("PSMTXConcat",               C_MTXConcat);
    HLESetCall("C_MTXTranspose",            C_MTXTranspose);
    HLESetCall("PSMTXTranspose",            C_MTXTranspose);

    Report(Channel::Norm, "\n");
}

/* ---------------------------------------------------------------------------
    General stuff
--------------------------------------------------------------------------- */

static Matrix tmpMatrix[4];

void C_MTXIdentity(void)
{
    MatrixPtr m = (MatrixPtr)(&mi.ram[PARAM(0) & RAMMASK]);

    MTX(m)[0][0] = ONE;  MTX(m)[0][1] = ZERO; MTX(m)[0][2] = ZERO; MTX(m)[0][3] = ZERO;
    MTX(m)[1][0] = ZERO; MTX(m)[1][1] = ONE;  MTX(m)[1][2] = ZERO; MTX(m)[1][3] = ZERO;
    MTX(m)[2][0] = ZERO; MTX(m)[2][1] = ZERO; MTX(m)[2][2] = ONE;  MTX(m)[2][3] = ZERO;
}

void C_MTXCopy(void)
{
    MatrixPtr src = (MatrixPtr)(&mi.ram[PARAM(0) & RAMMASK]);
    MatrixPtr dst = (MatrixPtr)(&mi.ram[PARAM(1) & RAMMASK]);

    if(src == dst) return;

    MTX(dst)[0][0] = MTX(src)[0][0]; MTX(dst)[0][1] = MTX(src)[0][1]; MTX(dst)[0][2] = MTX(src)[0][2]; MTX(dst)[0][3] = MTX(src)[0][3];
    MTX(dst)[1][0] = MTX(src)[1][0]; MTX(dst)[1][1] = MTX(src)[1][1]; MTX(dst)[1][2] = MTX(src)[1][2]; MTX(dst)[1][3] = MTX(src)[1][3];
    MTX(dst)[2][0] = MTX(src)[2][0]; MTX(dst)[2][1] = MTX(src)[2][1]; MTX(dst)[2][2] = MTX(src)[2][2]; MTX(dst)[2][3] = MTX(src)[2][3];

    //print_mtx((MatrixPtr)src, "src C");
    //print_mtx((MatrixPtr)dst, "dst C");
}

void C_MTXConcat(void)
{
    MatrixFPtr a = (MatrixFPtr)(&mi.ram[PARAM(0) & RAMMASK]);
    MatrixFPtr b = (MatrixFPtr)(&mi.ram[PARAM(1) & RAMMASK]);
    MatrixFPtr axb = (MatrixFPtr)(&mi.ram[PARAM(2) & RAMMASK]);
    MatrixFPtr t = (MatrixFPtr)(&tmpMatrix[0]), m;

    if( (axb == a) || (axb == b) ) m = t;
    else m = axb;

    //print_mtx((MatrixPtr)a, "a C");
    //print_mtx((MatrixPtr)b, "b C");

    Gekko::GekkoCore::SwapArea((uint32_t *)a, 3*4*4);
    Gekko::GekkoCore::SwapArea((uint32_t *)b, 3*4*4);

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
    Gekko::GekkoCore::SwapArea((uint32_t *)a, 3*4*4);
    Gekko::GekkoCore::SwapArea((uint32_t *)b, 3*4*4);
    Gekko::GekkoCore::SwapArea((uint32_t *)m, 3*4*4);

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
    MatrixPtr src = (MatrixPtr)(&mi.ram[PARAM(0) & RAMMASK]);
    MatrixPtr xPose = (MatrixPtr)(&mi.ram[PARAM(1) & RAMMASK]);
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
}

void C_MTXInvXpose(void)
{
}
