// Std C runtime
#include "dolphin.h"

#define PARAM(n)    GPR[3+n]
#define RET_VAL     GPR[3]
#define SWAP        MEMSwap

// fast longlong swap, invented by org
static void swap_double(void *srcPtr)
{
    u8 *src = (u8 *)srcPtr;
    register u8 t;

    for(int i=0; i<4; i++)
    {
        t = src[7-i];
        src[7-i] = src[i];
        src[i] = t;
    }
}

/* ---------------------------------------------------------------------------
    Memory operations
--------------------------------------------------------------------------- */

// fast mmx version
static void mmx_memcpy(u8 *dest, u8 *src, u32 cnt)
{
    int tail = cnt % 8, qwords = cnt >> 3;

    if(qwords)
    {
        while(qwords--)
        {
            __asm   mov     eax, src
            __asm   mov     edx, dest
            __asm   movq    mm0, [eax]
            __asm   movq    [edx], mm0
            src += 8;
            dest += 8;
        }
        __asm       emms
    }

    if(tail) memcpy(dest, src, tail);
}

// fast sse version (well, not as fast, as I've seen in some asm sources, but enough)
#ifdef  __VCNET__
static void sse_memcpy_u(u8 *dest, u8 *src, u32 cnt)
{
    int tail = cnt % 16, dqwords = cnt >> 4;

    if(dqwords)
    {
        while(dqwords--)
        {
            __asm   mov     eax, src
            __asm   mov     edx, dest
            __asm   movups  xmm0, [eax]     // unaligned
            __asm   movups  [edx], xmm0
            src += 16;
            dest += 16;
        }
        __asm       emms
    }

    if(tail) memcpy(dest, src, tail);
}
#endif  // __VCNET__

// void *memcpy( void *dest, const void *src, size_t count );
void HLE_memcpy()
{
    HLEHit(HLE_MEMCPY);

    u32 eaDest = PARAM(0), eaSrc = PARAM(1), cnt = PARAM(2);
    u32 paDest = MEMEffectiveToPhysical(eaDest, 0);
    u32 paSrc = MEMEffectiveToPhysical(eaSrc, 0);

#ifdef  _DEBUG
    ASSERT(paDest == -1, "memcpy dest unmapped");
    ASSERT(paSrc == -1, "memcpy src unmapped");
    ASSERT( (paDest + cnt) > RAMSIZE, "memcpy dest. Out of bounds");
    ASSERT( (paSrc + cnt) > RAMSIZE, "memcpy src. Out of bounds");
#endif

//  DBReport( GREEN "memcpy(0x%08X, 0x%08X, %i(%s))\n", 
//            eaDest, eaSrc, cnt, FileSmartSize(cnt) );

#ifdef  __VCNET__
    if(cpu.sse) sse_memcpy_u(&RAM[paDest], &RAM[paSrc], cnt);
    else
#endif
    if(cpu.mmx) mmx_memcpy(&RAM[paDest], &RAM[paSrc], cnt);
    else
    memcpy(&RAM[paDest], &RAM[paSrc], cnt);
}

// void *memset( void *dest, int c, size_t count );
void HLE_memset()
{
    HLEHit(HLE_MEMSET);

    u32 eaDest = PARAM(0), c = PARAM(1), cnt = PARAM(2);
    u32 paDest = MEMEffectiveToPhysical(eaDest, 0);

#ifdef  _DEBUG
    ASSERT(paDest == -1, "memcpy dest unmapped");
    ASSERT( (paDest + cnt) > RAMSIZE, "memcpy dest. Out of bounds");
#endif

//  DBReport( GREEN "memcpy(0x%08X, %i(%c), %i(%s))\n", 
//            eaDest, c, cnt, FileSmartSize(cnt) );
    memset(&RAM[paDest], c, cnt);
}

/* ---------------------------------------------------------------------------
    String operations
--------------------------------------------------------------------------- */


/* ---------------------------------------------------------------------------
    FP Math
--------------------------------------------------------------------------- */

// double sin(double x)
void HLE_sin()
{
    HLEHit(HLE_SIN);

    FPRD(1) = sin(FPRD(1));
}

// double cos(double x)
void HLE_cos()
{
    HLEHit(HLE_COS);

    FPRD(1) = cos(FPRD(1));
}

// double modf(double x, double * intptr)
void HLE_modf()
{
    HLEHit(HLE_MODF);

    double * intptr = (double *)(&RAM[MEMEffectiveToPhysical(PARAM(0), 0)]);
    
    FPRD(1) = modf(FPRD(1), intptr);
    swap_double(intptr);
}

// double frexp(double x, int * expptr)
void HLE_frexp()
{
    HLEHit(HLE_FREXP);

    u32 * expptr = (u32 *)(&RAM[MEMEffectiveToPhysical(PARAM(0), 0)]);
    
    FPRD(1) = frexp(FPRD(1), (int *)expptr);
    *expptr = SWAP(*expptr);
}

// double ldexp(double x, int exp)
void HLE_ldexp()
{
    HLEHit(HLE_LDEXP);

    FPRD(1) = ldexp(FPRD(1), (int)PARAM(0));
}

// double floor(double x)
void HLE_floor()
{
    HLEHit(HLE_FLOOR);

    FPRD(1) = floor(FPRD(1));
}

// double ceil(double x)
void HLE_ceil()
{
    HLEHit(HLE_CEIL);

    FPRD(1) = ceil(FPRD(1));
}
