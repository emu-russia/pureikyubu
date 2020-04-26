// pipeline callbacks implementation
#include "pch.h"

extern float   fracDenom[8][VTX_MAX_ATTR];             // fraction denominant
extern Vertex* vtx;
extern unsigned usevat;                                // current VAT

// ---------------------------------------------------------------------------

// Matrix index

void __fastcall pos_idx()
{
    xfRegs.posidx = GxFifo.Read8();
    //GFXError("fifo posidx : %i", xfRegs.posidx);
}

void __fastcall t0_idx()
{
    xfRegs.texidx[0] = GxFifo.Read8();
}

void __fastcall t1_idx()
{
    xfRegs.texidx[1] = GxFifo.Read8();
}

void __fastcall t2_idx()
{
    xfRegs.texidx[2] = GxFifo.Read8();
}

void __fastcall t3_idx()
{
    xfRegs.texidx[3] = GxFifo.Read8();
}

void __fastcall t4_idx()
{
    xfRegs.texidx[4] = GxFifo.Read8();
}

void __fastcall t5_idx()
{
    xfRegs.texidx[5] = GxFifo.Read8();
}

void __fastcall t6_idx()
{
    xfRegs.texidx[6] = GxFifo.Read8();
}

void __fastcall t7_idx()
{
    xfRegs.texidx[7] = GxFifo.Read8();
}

// ---------------------------------------------------------------------------

// Position

void __fastcall pos_xy_u8_d()
{
    uint8_t u[3];
    u[0] = GxFifo.Read8();
    u[1] = GxFifo.Read8();

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;
}

void __fastcall pos_xy_s8_d()
{
    int8_t u[3];
    u[0] = GxFifo.Read8();
    u[1] = GxFifo.Read8();

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;
}

void __fastcall pos_xy_u16_d()
{
    // swap position data
    uint16_t u[2];
    u[0] = GxFifo.Read16();
    u[1] = GxFifo.Read16();

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;
}

void __fastcall pos_xy_s16_d()
{
    // swap position data
    int16_t u[2];
    u[0] = GxFifo.Read16();
    u[1] = GxFifo.Read16();

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;
}

void __fastcall pos_xy_f32_d()
{
    // we need to swap "floats", so "u" is used as temporary buffer
    uint32_t u[2];
    u[0] = GxFifo.Read32();
    u[1] = GxFifo.Read32();

    // save swapped data in vertex descriptor
    float *v = (float *)u;
    vtx->pos[0] = v[0];
    vtx->pos[1] = v[1];
    vtx->pos[2] = 1.0f;
}

void __fastcall pos_xyz_u8_d()
{
    uint8_t u[3];
    u[0] = GxFifo.Read8();
    u[1] = GxFifo.Read8();
    u[2] = GxFifo.Read8();

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = ((float)u[2]) / fracDenom[usevat][VTX_POS];
}

void __fastcall pos_xyz_s8_d()
{
    int8_t u[3];
    u[0] = GxFifo.Read8();
    u[1] = GxFifo.Read8();
    u[2] = GxFifo.Read8();

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = ((float)u[2]) / fracDenom[usevat][VTX_POS];
}

void __fastcall pos_xyz_u16_d()
{
    // swap position data
    uint16_t  u[3];
    u[0] = GxFifo.Read16();
    u[1] = GxFifo.Read16();
    u[2] = GxFifo.Read16();

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = ((float)u[2]) / fracDenom[usevat][VTX_POS];
}

void __fastcall pos_xyz_s16_d()
{
    // swap position data
    int16_t u[3];
    u[0] = GxFifo.Read16();
    u[1] = GxFifo.Read16();
    u[2] = GxFifo.Read16();

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = ((float)u[2]) / fracDenom[usevat][VTX_POS];
}

void __fastcall pos_xyz_f32_d()
{
    // we need to swap "floats", so "u" is used as temporary buffer
    uint32_t u[3];
    u[0] = GxFifo.Read32();
    u[1] = GxFifo.Read32();
    u[2] = GxFifo.Read32();

    // save swapped data in vertex descriptor
    float *v = (float *)u;
    vtx->pos[0] = v[0];
    vtx->pos[1] = v[1];
    vtx->pos[2] = v[2];
}

void __fastcall pos_xy_u8_x8()
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_POS];
    unsigned n = (cpRegs.arstride[VTX_POS] / sizeof(uint8_t)) * (GxFifo.Read8());

    // swap position data
    uint8_t u[2];
    u[0] = ptr8[n + 0];
    u[1] = ptr8[n + 1];

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;
}

void __fastcall pos_xy_s8_x8()
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_POS];
    unsigned n = (cpRegs.arstride[VTX_POS] / sizeof(int8_t)) * (GxFifo.Read8());

    // swap position data
    int8_t u[2];
    u[0] = ptr8[n + 0];
    u[1] = ptr8[n + 1];

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;
}

void __fastcall pos_xy_s16_x8()
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_POS];
    unsigned n = (cpRegs.arstride[VTX_POS] / sizeof(int16_t)) * (GxFifo.Read8());

    // swap position data
    int16_t u[2];
    u[0] = _byteswap_ushort(ptr16[n + 0]);
    u[1] = _byteswap_ushort(ptr16[n + 1]);

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;
}

void __fastcall pos_xyz_s16_x8()
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_POS];
    unsigned n = (cpRegs.arstride[VTX_POS] / sizeof(int16_t)) * (GxFifo.Read8());

    // swap position data
    int16_t u[3];
    u[0] = _byteswap_ushort(ptr16[n + 0]);
    u[1] = _byteswap_ushort(ptr16[n + 1]);
    u[2] = _byteswap_ushort(ptr16[n + 2]);

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = ((float)u[2]) / fracDenom[usevat][VTX_POS];
}

void __fastcall pos_xyz_f32_x8()
{
    uint32_t *ptr32 = (uint32_t *)cpRegs.arbase[VTX_POS];
    unsigned n = (cpRegs.arstride[VTX_POS] / sizeof(float)) * (GxFifo.Read8());

    // we need to swap "floats", so "u" is used as temporary buffer
    uint32_t u[3];
    u[0] = _byteswap_ulong(ptr32[n + 0]);
    u[1] = _byteswap_ulong(ptr32[n + 1]);
    u[2] = _byteswap_ulong(ptr32[n + 2]);

    // save swapped data in vertex descriptor
    float *fu = (float *)u;
    vtx->pos[0] = fu[0];
    vtx->pos[1] = fu[1];
    vtx->pos[2] = fu[2];
}

void __fastcall pos_xyz_s16_x16()
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_POS];
    unsigned n = GxFifo.Read16();
    n *= (cpRegs.arstride[VTX_POS] / sizeof(int16_t));

    // we need to swap "floats", so "u" is used as temporary buffer
    int16_t u[3];
    u[0] = _byteswap_ushort(ptr16[n + 0]);
    u[1] = _byteswap_ushort(ptr16[n + 1]);
    u[2] = _byteswap_ushort(ptr16[n + 2]);

    // save swapped data in vertex descriptor
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = ((float)u[2]) / fracDenom[usevat][VTX_POS];
}

void __fastcall pos_xyz_f32_x16()
{
    uint32_t *ptr32 = (uint32_t *)cpRegs.arbase[VTX_POS];
    unsigned n = GxFifo.Read16();
    n *= (cpRegs.arstride[VTX_POS] / sizeof(float));

    // we need to swap "floats", so "u" is used as temporary buffer
    uint32_t u[3];
    u[0] = _byteswap_ulong(ptr32[n + 0]);
    u[1] = _byteswap_ulong(ptr32[n + 1]);
    u[2] = _byteswap_ulong(ptr32[n + 2]);

    // save swapped data in vertex descriptor
    float *fu = (float *)u;
    vtx->pos[0] = fu[0];
    vtx->pos[1] = fu[1];
    vtx->pos[2] = fu[2];
}

// ---------------------------------------------------------------------------

// Normal data

void __fastcall nrm_xyz_u8_d()
{
    uint8_t u[3];
    u[0] = GxFifo.Read8();
    u[1] = GxFifo.Read8();
    u[2] = GxFifo.Read8();

    // fractional convertion
    vtx->nrm[0] = ((float)u[0])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[1] = ((float)u[1])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[2] = ((float)u[2])/* / fracDenom[usevat][VTX_NRM]*/;
}

void __fastcall nrm_xyz_s8_d()
{
    int8_t u[3];
    u[0] = GxFifo.Read8();
    u[1] = GxFifo.Read8();
    u[2] = GxFifo.Read8();

    // fractional convertion
    vtx->nrm[0] = ((float)u[0])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[1] = ((float)u[1])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[2] = ((float)u[2])/* / fracDenom[usevat][VTX_NRM]*/;
}

void __fastcall nrm_xyz_u16_d()
{
    // swap position data
    uint16_t u[3];
    u[0] = GxFifo.Read16();
    u[1] = GxFifo.Read16();
    u[2] = GxFifo.Read16();

    // fractional convertion
    vtx->nrm[0] = ((float)u[0])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[1] = ((float)u[1])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[2] = ((float)u[2])/* / fracDenom[usevat][VTX_NRM]*/;
}

void __fastcall nrm_xyz_s16_d()
{
    // swap position data
    int16_t u[3];
    u[0] = GxFifo.Read16();
    u[1] = GxFifo.Read16();
    u[2] = GxFifo.Read16();

    // fractional convertion
    vtx->nrm[0] = ((float)u[0])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[1] = ((float)u[1])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[2] = ((float)u[2])/* / fracDenom[usevat][VTX_NRM]*/;
}

void __fastcall nrm_xyz_f32_d()
{
    // we need to swap "floats", so "u" is used as temporary buffer
    uint32_t u[3];
    u[0] = GxFifo.Read32();
    u[1] = GxFifo.Read32();
    u[2] = GxFifo.Read32();

    // save swapped data in vertex descriptor
    float *v = (float *)u;
    vtx->nrm[0] = v[0];
    vtx->nrm[1] = v[1];
    vtx->nrm[2] = v[2];
}

// normal only
// TODO : make ortho for nbt
void __fastcall nrm_nbt_f32_d()
{
    // we need to swap "floats", so "u" is used as temporary buffer
    uint32_t u[3];
    u[0] = GxFifo.Read32();
    u[1] = GxFifo.Read32();
    u[2] = GxFifo.Read32();

    // save swapped data in vertex descriptor
    float *v = (float *)u;
    vtx->nrm[0] = v[0];
    vtx->nrm[1] = v[1];
    vtx->nrm[2] = v[2];
}

void __fastcall nrm_xyz_u8_x8()
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_NRM];
    unsigned n = (cpRegs.arstride[VTX_NRM] / sizeof(uint8_t)) * (GxFifo.Read8());

    // swap position data
    uint8_t u[3];
    u[0] = ptr8[n + 0];
    u[1] = ptr8[n + 1];
    u[2] = ptr8[n + 2];

    // fractional convertion
    vtx->nrm[0] = ((float)u[0])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[1] = ((float)u[1])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[2] = ((float)u[2])/* / fracDenom[usevat][VTX_NRM]*/;
}

void __fastcall nrm_xyz_s8_x8()
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_NRM];
    unsigned n = (cpRegs.arstride[VTX_NRM] / sizeof(uint8_t)) * (GxFifo.Read8());

    // swap position data
    int8_t u[3];
    u[0] = ptr8[n + 0];
    u[1] = ptr8[n + 1];
    u[2] = ptr8[n + 2];

    // fractional convertion
    vtx->nrm[0] = ((float)u[0])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[1] = ((float)u[1])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[2] = ((float)u[2])/* / fracDenom[usevat][VTX_NRM]*/;
}

void __fastcall nrm_xyz_s16_x8()
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_NRM];
    unsigned n = (cpRegs.arstride[VTX_NRM] / sizeof(int16_t)) * (GxFifo.Read8());

    // swap position data
    int16_t u[3];
    u[0] = _byteswap_ushort(ptr16[n + 0]);
    u[1] = _byteswap_ushort(ptr16[n + 1]);
    u[2] = _byteswap_ushort(ptr16[n + 2]);

    // fractional convertion
    vtx->nrm[0] = ((float)u[0]);
    vtx->nrm[1] = ((float)u[1]);
    vtx->nrm[2] = ((float)u[2]);
}

void __fastcall nrm_xyz_f32_x8()
{
    uint32_t *ptr32 = (uint32_t *)cpRegs.arbase[VTX_NRM];
    unsigned n = (cpRegs.arstride[VTX_NRM] / sizeof(float)) * (GxFifo.Read8());

    // we need to swap "floats", so "u" is used as temporary buffer
    uint32_t u[3];
    u[0] = _byteswap_ulong(ptr32[n + 0]);
    u[1] = _byteswap_ulong(ptr32[n + 1]);
    u[2] = _byteswap_ulong(ptr32[n + 2]);

    // save swapped data in vertex descriptor
    float *fu = (float *)u;
    vtx->nrm[0] = fu[0];
    vtx->nrm[1] = fu[1];
    vtx->nrm[2] = fu[2];
}

void __fastcall nrm_nbt_s16_x8()
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_NRM];
    unsigned n = (cpRegs.arstride[VTX_NRM] / sizeof(int16_t)) * (GxFifo.Read8());

    // swap normal data
    int16_t u[3];
    u[0] = _byteswap_ushort(ptr16[n + 0]);
    u[1] = _byteswap_ushort(ptr16[n + 1]);
    u[2] = _byteswap_ushort(ptr16[n + 2]);

    // fractional convertion
    vtx->nrm[0] = ((float)u[0]);
    vtx->nrm[1] = ((float)u[1]);
    vtx->nrm[2] = ((float)u[2]);
}

void __fastcall nrm_xyz_s16_x16()
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_NRM];
    unsigned n = GxFifo.Read16();
    n *= (cpRegs.arstride[VTX_NRM] / sizeof(int16_t));

    // swap normal data
    int16_t u[3];
    u[0] = _byteswap_ushort(ptr16[n + 0]);
    u[1] = _byteswap_ushort(ptr16[n + 1]);
    u[2] = _byteswap_ushort(ptr16[n + 2]);

    // save swapped data in vertex descriptor
    vtx->nrm[0] = (float)u[0];
    vtx->nrm[1] = (float)u[1];
    vtx->nrm[2] = (float)u[2];
}

void __fastcall nrm_xyz_f32_x16()
{
    uint32_t *ptr32 = (uint32_t *)cpRegs.arbase[VTX_NRM];
    unsigned n = GxFifo.Read16();
    n *= (cpRegs.arstride[VTX_NRM] / sizeof(float));

    // we need to swap "floats", so "u" is used as temporary buffer
    uint32_t u[3];
    u[0] = _byteswap_ulong(ptr32[n + 0]);
    u[1] = _byteswap_ulong(ptr32[n + 1]);
    u[2] = _byteswap_ulong(ptr32[n + 2]);

    // save swapped data in vertex descriptor
    float *fu = (float *)u;
    vtx->nrm[0] = fu[0];
    vtx->nrm[1] = fu[1];
    vtx->nrm[2] = fu[2];
}

// ---------------------------------------------------------------------------

// Color 0

void __fastcall clr0_rgb_rgb565_d()
{
    uint16_t p = GxFifo.Read16();

    uint8_t r = p >> 11;
    uint8_t g = (p >> 5) & 0x3f;
    uint8_t b = p & 0x1f;

    vtx->col[0].R = (r << 3) | (r >> 2);
    vtx->col[0].G = (g << 2) | (g >> 4);
    vtx->col[0].B = (b << 3) | (b >> 2);
    vtx->col[0].A = 255;
}

void __fastcall clr0_rgb_rgb8_d()
{
    vtx->col[0].R = GxFifo.Read8();
    vtx->col[0].G = GxFifo.Read8();
    vtx->col[0].B = GxFifo.Read8();
    vtx->col[0].A = 255;
}

void __fastcall clr0_rgba_rgba4_d()
{
    uint16_t p = GxFifo.Read16();

    uint8_t r = (p >>12) & 0xf;
    uint8_t g = (p >> 8) & 0xf;
    uint8_t b = (p >> 4) & 0xf;
    uint8_t a = (p >> 0) & 0xf;

    vtx->col[0].R = (r << 4) | r;
    vtx->col[0].G = (g << 4) | g;
    vtx->col[0].B = (b << 4) | b;
    vtx->col[0].A = (a << 4) | a;
}

void __fastcall clr0_rgba_rgba8_d()
{
    vtx->col[0].R = GxFifo.Read8();
    vtx->col[0].G = GxFifo.Read8();
    vtx->col[0].B = GxFifo.Read8();
    vtx->col[0].A = GxFifo.Read8();
}

void __fastcall clr0_rgb_rgba8_x8()
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_COLOR0];
    unsigned n = (cpRegs.arstride[VTX_COLOR0] / sizeof(uint8_t)) * (GxFifo.Read8());

    vtx->col[0].R = ptr8[n + 0];
    vtx->col[0].G = ptr8[n + 1];
    vtx->col[0].B = ptr8[n + 2];
    vtx->col[0].A = 255;
}

void __fastcall clr0_rgba_rgba8_x8()
{
    uint8_t *ptr8 = (uint8_t *)cpRegs.arbase[VTX_COLOR0];
    unsigned n = (cpRegs.arstride[VTX_COLOR0] / sizeof(uint8_t)) * (GxFifo.Read8());

    vtx->col[0].R = ptr8[n + 0];
    vtx->col[0].G = ptr8[n + 1];
    vtx->col[0].B = ptr8[n + 2];
    vtx->col[0].A = ptr8[n + 3];
}

void __fastcall clr0_rgb_rgb8_x16()
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_COLOR0];
    unsigned n = GxFifo.Read16();
    n *= (cpRegs.arstride[VTX_COLOR0] / sizeof(uint8_t));

    vtx->col[0].R = ptr8[n + 0];
    vtx->col[0].G = ptr8[n + 1];
    vtx->col[0].B = ptr8[n + 2];
    vtx->col[0].A = 255;
}

void __fastcall clr0_rgba_rgba8_x16()
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_COLOR0];
    unsigned n = GxFifo.Read16();
    n *= (cpRegs.arstride[VTX_COLOR0] / sizeof(uint8_t));

    vtx->col[0].R = ptr8[n + 0];
    vtx->col[0].G = ptr8[n + 1];
    vtx->col[0].B = ptr8[n + 2];
    vtx->col[0].A = ptr8[n + 3];
}

// ---------------------------------------------------------------------------

// Color 1

void __fastcall clr1_rgba_rgba8_x8()
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_COLOR1];
    unsigned n = (cpRegs.arstride[VTX_COLOR1] / sizeof(uint8_t)) * (GxFifo.Read8());

    vtx->col[1].R = ptr8[n + 0];
    vtx->col[1].G = ptr8[n + 1];
    vtx->col[1].B = ptr8[n + 2];
    vtx->col[1].A = ptr8[n + 3];
}

// ---------------------------------------------------------------------------

// Texture coord 0

void __fastcall tex0_st_u8_d()
{
    // swap position data
    uint8_t u[2];
    u[0] = GxFifo.Read8();
    u[1] = GxFifo.Read8();

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];
}

void __fastcall tex0_st_s8_d()
{
    // swap position data
    int8_t u[2];
    u[0] = GxFifo.Read8();
    u[1] = GxFifo.Read8();

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];
}

void __fastcall tex0_st_u16_d()
{
    // swap position data
    uint16_t u[2];
    u[0] = GxFifo.Read16();
    u[1] = GxFifo.Read16();

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];
}

void __fastcall tex0_st_s16_d()
{
    // swap position data
    uint16_t u[2];
    u[0] = GxFifo.Read16();
    u[1] = GxFifo.Read16();

    //GFXError("u[0]:%04X, u[1]:%04X, %i", u[0], u[1], cpRegs.vatA[usevat].tex0shft);

    // fractional convertion
    vtx->tcoord[0][0] = ((float)(int16_t)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)(int16_t)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];

    //GFXError("s:%f, t:%f", vtx->tcoord[0][0], vtx->tcoord[0][1]);
}

void __fastcall tex0_st_f32_d()
{
    // we need to swap "floats", so "u" is used as temporary buffer
    uint32_t u[2];
    u[0] = GxFifo.Read32();
    u[1] = GxFifo.Read32();

    // save swapped data in vertex descriptor
    float *v = (float *)u;
    vtx->tcoord[0][0] = v[0];
    vtx->tcoord[0][1] = v[1];
}

void __fastcall tex0_st_s8_x8()
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(int8_t)) * (GxFifo.Read8());

    // swap position data
    int8_t u[2];
    u[0] = ptr8[n + 0];
    u[1] = ptr8[n + 1];

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];
}

void __fastcall tex0_st_u16_x8()
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(uint16_t)) * (GxFifo.Read8());

    // swap position data
    uint16_t u[2];
    u[0] = _byteswap_ushort(ptr16[n + 0]);
    u[1] = _byteswap_ushort(ptr16[n + 1]);

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];
}

void __fastcall tex0_st_s16_x8()
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(int16_t)) * (GxFifo.Read8());

    // swap position data
    int16_t u[2];
    u[0] = _byteswap_ushort(ptr16[n + 0]);
    u[1] = _byteswap_ushort(ptr16[n + 1]);

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];
}

void __fastcall tex0_st_f32_x8()
{
    uint32_t *ptr32 = (uint32_t *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(float)) * (GxFifo.Read8());

    // we need to swap "floats", so "u" is used as temporary buffer
    uint32_t u[2];
    u[0] = _byteswap_ulong(ptr32[n + 0]);
    u[1] = _byteswap_ulong(ptr32[n + 1]);

    // save swapped data in vertex descriptor
    float *fu = (float *)u;
    vtx->tcoord[0][0] = fu[0];
    vtx->tcoord[0][1] = fu[1];
}

void __fastcall tex0_s_u8_x16()
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = GxFifo.Read16();
    n *= (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(uint8_t));

    uint8_t u[2];
    u[0] = ptr8[n + 0];

    // save swapped data in vertex descriptor
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = 1.0f;
}

void __fastcall tex0_st_u16_x16()
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = GxFifo.Read16();
    n *= (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(uint16_t));

    // we need to swap "floats", so "u" is used as temporary buffer
    uint16_t u[2];
    u[0] = _byteswap_ushort(ptr16[n + 0]);
    u[1] = _byteswap_ushort(ptr16[n + 1]);

    // save swapped data in vertex descriptor
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];
}

void __fastcall tex0_st_s16_x16()
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = GxFifo.Read16();
    n *= (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(int16_t));

    // we need to swap "floats", so "u" is used as temporary buffer
    int16_t u[2];
    u[0] = _byteswap_ushort(ptr16[n + 0]);
    u[1] = _byteswap_ushort(ptr16[n + 1]);

    // save swapped data in vertex descriptor
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];
}

void __fastcall tex0_st_f32_x16()
{
    uint32_t *ptr32 = (uint32_t *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = GxFifo.Read16();
    n *= (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(float));

    // we need to swap "floats", so "u" is used as temporary buffer
    uint32_t u[2];
    u[0] = _byteswap_ulong(ptr32[n + 0]);
    u[1] = _byteswap_ulong(ptr32[n + 1]);

    // save swapped data in vertex descriptor
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];
}

// ---------------------------------------------------------------------------

// Texture coord 1

void __fastcall tex1_s_u8_x8()
{
    GxFifo.Read8();
}

void __fastcall tex1_s_s8_x8()
{
    GxFifo.Read8();
}

void __fastcall tex1_s_u16_x8()
{
    GxFifo.Read8();
}

void __fastcall tex1_s_s16_x8()
{
    GxFifo.Read8();
}

void __fastcall tex1_s_f32_x8()
{
    GxFifo.Read8();
}

void __fastcall tex1_st_u8_x8()
{
    GxFifo.Read8();
}

void __fastcall tex1_st_s8_x8()
{
    GxFifo.Read8();
}

void __fastcall tex1_st_u16_x8()
{
    GxFifo.Read8();
}

void __fastcall tex1_st_s16_x8()
{
    GxFifo.Read8();
}

void __fastcall tex1_st_f32_x8()
{
    GxFifo.Read8();
}

void __fastcall tex1_s_u8_x16()
{
    GxFifo.Read16();
}

void __fastcall tex1_s_s8_x16()
{
    GxFifo.Read16();
}

void __fastcall tex1_s_u16_x16()
{
    GxFifo.Read16();
}

void __fastcall tex1_s_s16_x16()
{
    GxFifo.Read16();
}

void __fastcall tex1_s_f32_x16()
{
    GxFifo.Read16();
}

void __fastcall tex1_st_u8_x16()
{
    GxFifo.Read16();
}

void __fastcall tex1_st_s8_x16()
{
    GxFifo.Read16();
}

void __fastcall tex1_st_u16_x16()
{
    GxFifo.Read16();
}

void __fastcall tex1_st_s16_x16()
{
    GxFifo.Read16();
}

void __fastcall tex1_st_f32_x16()
{
    GxFifo.Read16();
}

// ---------------------------------------------------------------------------

// Texture coord 2

// ---------------------------------------------------------------------------

// Texture coord 3

// ---------------------------------------------------------------------------

// Texture coord 4

// ---------------------------------------------------------------------------

// Texture coord 5

// ---------------------------------------------------------------------------

// Texture coord 6

// ---------------------------------------------------------------------------

// Texture coord 7


//
// fifo stages set
// attr[TYPE][CNT][FMT]
//

void (__fastcall *posattr[4][2][5])() = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // XY
    { NULL, NULL, NULL, NULL, NULL }    // XYZ
    },
        
    {   // direct
    { pos_xy_u8_d, pos_xy_s8_d, pos_xy_u16_d, pos_xy_s16_d, pos_xy_f32_d },   // XY
    { pos_xyz_u8_d, pos_xyz_s8_d, pos_xyz_u16_d, pos_xyz_s16_d, pos_xyz_f32_d }    // XYZ
    },

    {   // x8
    { pos_xy_u8_x8, pos_xy_s8_x8, NULL, pos_xy_s16_x8, NULL },   // XY
    { NULL, NULL, NULL, pos_xyz_s16_x8, pos_xyz_f32_x8 }    // XYZ
    },

    {   // x16
    { NULL, NULL, NULL, NULL, NULL },   // XY
    { NULL, NULL, NULL, pos_xyz_s16_x16, pos_xyz_f32_x16 }    // XYZ
    }

    // U8    S8   U16   S16   F32
};

void (__fastcall *nrmattr[4][3][5])() = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // XYZ
    { NULL, NULL, NULL, NULL, NULL },   // NBT
    { NULL, NULL, NULL, NULL, NULL }    // NBT3
    },

    {   // direct
    { nrm_xyz_u8_d, nrm_xyz_s8_d, nrm_xyz_u16_d, nrm_xyz_s16_d, nrm_xyz_f32_d },   // XYZ
    { NULL, NULL, NULL, NULL, nrm_nbt_f32_d },   // NBT
    { NULL, NULL, NULL, NULL, NULL }    // NBT3
    },

    {   // x8
    { nrm_xyz_u8_x8, nrm_xyz_s8_x8, NULL, nrm_xyz_s16_x8, nrm_xyz_f32_x8 },   // XYZ
    { NULL, NULL, NULL, nrm_nbt_s16_x8, NULL },   // NBT
    { NULL, NULL, NULL, NULL, NULL }    // NBT3
    },

    {   // x16
    { NULL, NULL, NULL, nrm_xyz_s16_x16, nrm_xyz_f32_x16 },   // XYZ
    { NULL, NULL, NULL, NULL, NULL },   // NBT
    { NULL, NULL, NULL, NULL, NULL }    // NBT3
    },

    // U8    S8   U16   S16   F32
};

// ---------------------------------------------------------------------------

void (__fastcall *col0attr[4][2][6])() = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL, NULL },     // RGB
    { NULL, NULL, NULL, NULL, NULL, NULL }      // RGBA
    },
        
    {   // direct
    { clr0_rgb_rgb565_d, clr0_rgb_rgb8_d, NULL, NULL, NULL, NULL },     // RGB
    { NULL, NULL, NULL, clr0_rgba_rgba4_d, NULL, clr0_rgba_rgba8_d }      // RGBA
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL, clr0_rgb_rgba8_x8 },     // RGB
    { NULL, NULL, NULL, NULL, NULL, clr0_rgba_rgba8_x8 }      // RGBA
    },

    {   // x16
    { NULL, clr0_rgb_rgb8_x16, NULL, NULL, NULL, NULL },     // RGB
    { NULL, NULL, NULL, NULL, NULL, clr0_rgba_rgba8_x16 }      // RGBA
    }

//  RGB565  RGB8  RGBX8 RGBA4 RGBA6 RGBA8
};

void (__fastcall *col1attr[4][2][6])() = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL, NULL },     // RGB
    { NULL, NULL, NULL, NULL, NULL, NULL }      // RGBA
    },
        
    {   // direct
    { NULL, NULL, NULL, NULL, NULL, NULL },     // RGB
    { NULL, NULL, NULL, NULL, NULL, NULL }      // RGBA
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL, NULL },     // RGB
    { NULL, NULL, NULL, NULL, NULL, NULL }      // RGBA
    },

    {   // x16
    { NULL, NULL, NULL, NULL, NULL, NULL },     // RGB
    { NULL, NULL, NULL, NULL, NULL, NULL }      // RGBA
    }

//  RGB565  RGB8  RGBX8 RGBA4 RGBA6 RGBA8
};

// ---------------------------------------------------------------------------

void (__fastcall *tex0attr[4][2][5])() = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // direct
    { NULL, NULL, NULL, NULL, NULL },   // S
    { tex0_st_u8_d, tex0_st_s8_d, tex0_st_u16_d, tex0_st_s16_d, tex0_st_f32_d }    // ST
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, tex0_st_s8_x8, tex0_st_u16_x8, tex0_st_s16_x8, tex0_st_f32_x8 }    // ST
    },

    {   // x16
    { tex0_s_u8_x16, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, tex0_st_u16_x16, tex0_st_s16_x16, tex0_st_f32_x16 }    // ST
    },

    // U8    S8   U16   S16   F32
};

void (__fastcall *tex1attr[4][2][5])() = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // direct
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x8
    { tex1_s_u8_x8, tex1_s_s8_x8, tex1_s_u16_x8, tex1_s_s16_x8, tex1_s_f32_x8 },   // S
    { tex1_st_u8_x8, tex1_st_s8_x8, tex1_st_u16_x8, tex1_st_s16_x8, tex1_st_f32_x8 }    // ST
    },

    {   // x16
    { tex1_s_u8_x16, tex1_s_s8_x16, tex1_s_u16_x16, tex1_s_s16_x16, tex1_s_f32_x16 },   // S
    { tex1_st_u8_x16, tex1_st_s8_x16, tex1_st_u16_x16, tex1_st_s16_x16, tex1_st_f32_x16 }    // ST
    },

    // U8    S8   U16   S16   F32
};

void (__fastcall *tex2attr[4][2][5])() = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // direct
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x16
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    // U8    S8   U16   S16   F32
};

void (__fastcall *tex3attr[4][2][5])() = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // direct
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x16
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    // U8    S8   U16   S16   F32
};

void (__fastcall *tex4attr[4][2][5])() = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // direct
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x16
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    // U8    S8   U16   S16   F32
};

void (__fastcall *tex5attr[4][2][5])() = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // direct
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x16
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    // U8    S8   U16   S16   F32
};

void (__fastcall *tex6attr[4][2][5])() = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // direct
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x16
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    // U8    S8   U16   S16   F32
};

void (__fastcall *tex7attr[4][2][5])() = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // direct
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x16
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    // U8    S8   U16   S16   F32
};
