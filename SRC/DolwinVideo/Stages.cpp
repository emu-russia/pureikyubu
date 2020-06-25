// pipeline callbacks implementation
#include "pch.h"

extern float   fracDenom[8][VTX_MAX_ATTR];             // fraction denominant
extern Vertex* vtx;
extern unsigned usevat;                                // current VAT

// ---------------------------------------------------------------------------

// Matrix index

void pos_idx(GX::FifoProcessor* fifo)
{
    xfRegs.posidx = fifo->Read8();
    //GFXError("fifo posidx : %i", xfRegs.posidx);
}

void t0_idx(GX::FifoProcessor* fifo)
{
    xfRegs.texidx[0] = fifo->Read8();
}

void t1_idx(GX::FifoProcessor* fifo)
{
    xfRegs.texidx[1] = fifo->Read8();
}

void t2_idx(GX::FifoProcessor* fifo)
{
    xfRegs.texidx[2] = fifo->Read8();
}

void t3_idx(GX::FifoProcessor* fifo)
{
    xfRegs.texidx[3] = fifo->Read8();
}

void t4_idx(GX::FifoProcessor* fifo)
{
    xfRegs.texidx[4] = fifo->Read8();
}

void t5_idx(GX::FifoProcessor* fifo)
{
    xfRegs.texidx[5] = fifo->Read8();
}

void t6_idx(GX::FifoProcessor* fifo)
{
    xfRegs.texidx[6] = fifo->Read8();
}

void t7_idx(GX::FifoProcessor* fifo)
{
    xfRegs.texidx[7] = fifo->Read8();
}

// ---------------------------------------------------------------------------

// Position

void pos_xy_u8_d(GX::FifoProcessor* fifo)
{
    uint8_t u[3];
    u[0] = fifo->Read8();
    u[1] = fifo->Read8();

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;
}

void pos_xy_s8_d(GX::FifoProcessor* fifo)
{
    int8_t u[3];
    u[0] = fifo->Read8();
    u[1] = fifo->Read8();

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;
}

void pos_xy_u16_d(GX::FifoProcessor* fifo)
{
    // swap position data
    uint16_t u[2];
    u[0] = fifo->Read16();
    u[1] = fifo->Read16();

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;
}

void pos_xy_s16_d(GX::FifoProcessor* fifo)
{
    // swap position data
    int16_t u[2];
    u[0] = fifo->Read16();
    u[1] = fifo->Read16();

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;
}

void pos_xy_f32_d(GX::FifoProcessor* fifo)
{
    // we need to swap "floats", so "u" is used as temporary buffer
    uint32_t u[2];
    u[0] = fifo->Read32();
    u[1] = fifo->Read32();

    // save swapped data in vertex descriptor
    float *v = (float *)u;
    vtx->pos[0] = v[0];
    vtx->pos[1] = v[1];
    vtx->pos[2] = 1.0f;
}

void pos_xyz_u8_d(GX::FifoProcessor* fifo)
{
    uint8_t u[3];
    u[0] = fifo->Read8();
    u[1] = fifo->Read8();
    u[2] = fifo->Read8();

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = ((float)u[2]) / fracDenom[usevat][VTX_POS];
}

void pos_xyz_s8_d(GX::FifoProcessor* fifo)
{
    int8_t u[3];
    u[0] = fifo->Read8();
    u[1] = fifo->Read8();
    u[2] = fifo->Read8();

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = ((float)u[2]) / fracDenom[usevat][VTX_POS];
}

void pos_xyz_u16_d(GX::FifoProcessor* fifo)
{
    // swap position data
    uint16_t  u[3];
    u[0] = fifo->Read16();
    u[1] = fifo->Read16();
    u[2] = fifo->Read16();

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = ((float)u[2]) / fracDenom[usevat][VTX_POS];
}

void pos_xyz_s16_d(GX::FifoProcessor* fifo)
{
    // swap position data
    int16_t u[3];
    u[0] = fifo->Read16();
    u[1] = fifo->Read16();
    u[2] = fifo->Read16();

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = ((float)u[2]) / fracDenom[usevat][VTX_POS];
}

void pos_xyz_f32_d(GX::FifoProcessor* fifo)
{
    // we need to swap "floats", so "u" is used as temporary buffer
    uint32_t u[3];
    u[0] = fifo->Read32();
    u[1] = fifo->Read32();
    u[2] = fifo->Read32();

    // save swapped data in vertex descriptor
    float *v = (float *)u;
    vtx->pos[0] = v[0];
    vtx->pos[1] = v[1];
    vtx->pos[2] = v[2];
}

void pos_xy_u8_x8(GX::FifoProcessor* fifo)
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_POS];
    unsigned n = (cpRegs.arstride[VTX_POS] / sizeof(uint8_t)) * (fifo->Read8());

    // swap position data
    uint8_t u[2];
    u[0] = ptr8[n + 0];
    u[1] = ptr8[n + 1];

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;
}

void pos_xy_s8_x8(GX::FifoProcessor* fifo)
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_POS];
    unsigned n = (cpRegs.arstride[VTX_POS] / sizeof(int8_t)) * (fifo->Read8());

    // swap position data
    int8_t u[2];
    u[0] = ptr8[n + 0];
    u[1] = ptr8[n + 1];

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;
}

void pos_xy_s16_x8(GX::FifoProcessor* fifo)
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_POS];
    unsigned n = (cpRegs.arstride[VTX_POS] / sizeof(int16_t)) * (fifo->Read8());

    // swap position data
    int16_t u[2];
    u[0] = _byteswap_ushort(ptr16[n + 0]);
    u[1] = _byteswap_ushort(ptr16[n + 1]);

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;
}

void pos_xyz_s16_x8(GX::FifoProcessor* fifo)
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_POS];
    unsigned n = (cpRegs.arstride[VTX_POS] / sizeof(int16_t)) * (fifo->Read8());

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

void pos_xyz_f32_x8(GX::FifoProcessor* fifo)
{
    uint32_t *ptr32 = (uint32_t *)cpRegs.arbase[VTX_POS];
    unsigned n = (cpRegs.arstride[VTX_POS] / sizeof(float)) * (fifo->Read8());

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

void pos_xyz_s16_x16(GX::FifoProcessor* fifo)
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_POS];
    unsigned n = fifo->Read16();
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

void pos_xyz_f32_x16(GX::FifoProcessor* fifo)
{
    uint32_t *ptr32 = (uint32_t *)cpRegs.arbase[VTX_POS];
    unsigned n = fifo->Read16();
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

void nrm_xyz_u8_d(GX::FifoProcessor* fifo)
{
    uint8_t u[3];
    u[0] = fifo->Read8();
    u[1] = fifo->Read8();
    u[2] = fifo->Read8();

    // fractional convertion
    vtx->nrm[0] = ((float)u[0])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[1] = ((float)u[1])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[2] = ((float)u[2])/* / fracDenom[usevat][VTX_NRM]*/;
}

void nrm_xyz_s8_d(GX::FifoProcessor* fifo)
{
    int8_t u[3];
    u[0] = fifo->Read8();
    u[1] = fifo->Read8();
    u[2] = fifo->Read8();

    // fractional convertion
    vtx->nrm[0] = ((float)u[0])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[1] = ((float)u[1])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[2] = ((float)u[2])/* / fracDenom[usevat][VTX_NRM]*/;
}

void nrm_xyz_u16_d(GX::FifoProcessor* fifo)
{
    // swap position data
    uint16_t u[3];
    u[0] = fifo->Read16();
    u[1] = fifo->Read16();
    u[2] = fifo->Read16();

    // fractional convertion
    vtx->nrm[0] = ((float)u[0])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[1] = ((float)u[1])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[2] = ((float)u[2])/* / fracDenom[usevat][VTX_NRM]*/;
}

void nrm_xyz_s16_d(GX::FifoProcessor* fifo)
{
    // swap position data
    int16_t u[3];
    u[0] = fifo->Read16();
    u[1] = fifo->Read16();
    u[2] = fifo->Read16();

    // fractional convertion
    vtx->nrm[0] = ((float)u[0])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[1] = ((float)u[1])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[2] = ((float)u[2])/* / fracDenom[usevat][VTX_NRM]*/;
}

void nrm_xyz_f32_d(GX::FifoProcessor* fifo)
{
    // we need to swap "floats", so "u" is used as temporary buffer
    uint32_t u[3];
    u[0] = fifo->Read32();
    u[1] = fifo->Read32();
    u[2] = fifo->Read32();

    // save swapped data in vertex descriptor
    float *v = (float *)u;
    vtx->nrm[0] = v[0];
    vtx->nrm[1] = v[1];
    vtx->nrm[2] = v[2];
}

// normal only
// TODO : make ortho for nbt
void nrm_nbt_f32_d(GX::FifoProcessor* fifo)
{
    // we need to swap "floats", so "u" is used as temporary buffer
    uint32_t u[3];
    u[0] = fifo->Read32();
    u[1] = fifo->Read32();
    u[2] = fifo->Read32();

    // save swapped data in vertex descriptor
    float *v = (float *)u;
    vtx->nrm[0] = v[0];
    vtx->nrm[1] = v[1];
    vtx->nrm[2] = v[2];
}

void nrm_xyz_u8_x8(GX::FifoProcessor* fifo)
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_NRM];
    unsigned n = (cpRegs.arstride[VTX_NRM] / sizeof(uint8_t)) * (fifo->Read8());

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

void nrm_xyz_s8_x8(GX::FifoProcessor* fifo)
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_NRM];
    unsigned n = (cpRegs.arstride[VTX_NRM] / sizeof(uint8_t)) * (fifo->Read8());

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

void nrm_xyz_s16_x8(GX::FifoProcessor* fifo)
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_NRM];
    unsigned n = (cpRegs.arstride[VTX_NRM] / sizeof(int16_t)) * (fifo->Read8());

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

void nrm_xyz_f32_x8(GX::FifoProcessor* fifo)
{
    uint32_t *ptr32 = (uint32_t *)cpRegs.arbase[VTX_NRM];
    unsigned n = (cpRegs.arstride[VTX_NRM] / sizeof(float)) * (fifo->Read8());

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

void nrm_nbt_s16_x8(GX::FifoProcessor* fifo)
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_NRM];
    unsigned n = (cpRegs.arstride[VTX_NRM] / sizeof(int16_t)) * (fifo->Read8());

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

void nrm_xyz_s16_x16(GX::FifoProcessor* fifo)
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_NRM];
    unsigned n = fifo->Read16();
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

void nrm_xyz_f32_x16(GX::FifoProcessor* fifo)
{
    uint32_t *ptr32 = (uint32_t *)cpRegs.arbase[VTX_NRM];
    unsigned n = fifo->Read16();
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

void clr0_rgb_rgb565_d(GX::FifoProcessor* fifo)
{
    uint16_t p = fifo->Read16();

    uint8_t r = p >> 11;
    uint8_t g = (p >> 5) & 0x3f;
    uint8_t b = p & 0x1f;

    vtx->col[0].R = (r << 3) | (r >> 2);
    vtx->col[0].G = (g << 2) | (g >> 4);
    vtx->col[0].B = (b << 3) | (b >> 2);
    vtx->col[0].A = 255;
}

void clr0_rgb_rgb8_d(GX::FifoProcessor* fifo)
{
    vtx->col[0].R = fifo->Read8();
    vtx->col[0].G = fifo->Read8();
    vtx->col[0].B = fifo->Read8();
    vtx->col[0].A = 255;
}

void clr0_rgba_rgba4_d(GX::FifoProcessor* fifo)
{
    uint16_t p = fifo->Read16();

    uint8_t r = (p >>12) & 0xf;
    uint8_t g = (p >> 8) & 0xf;
    uint8_t b = (p >> 4) & 0xf;
    uint8_t a = (p >> 0) & 0xf;

    vtx->col[0].R = (r << 4) | r;
    vtx->col[0].G = (g << 4) | g;
    vtx->col[0].B = (b << 4) | b;
    vtx->col[0].A = (a << 4) | a;
}

void clr0_rgba_rgba8_d(GX::FifoProcessor* fifo)
{
    vtx->col[0].R = fifo->Read8();
    vtx->col[0].G = fifo->Read8();
    vtx->col[0].B = fifo->Read8();
    vtx->col[0].A = fifo->Read8();
}

void clr0_rgb_rgba8_x8(GX::FifoProcessor* fifo)
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_COLOR0];
    unsigned n = (cpRegs.arstride[VTX_COLOR0] / sizeof(uint8_t)) * (fifo->Read8());

    vtx->col[0].R = ptr8[n + 0];
    vtx->col[0].G = ptr8[n + 1];
    vtx->col[0].B = ptr8[n + 2];
    vtx->col[0].A = 255;
}

void clr0_rgba_rgba8_x8(GX::FifoProcessor* fifo)
{
    uint8_t *ptr8 = (uint8_t *)cpRegs.arbase[VTX_COLOR0];
    unsigned n = (cpRegs.arstride[VTX_COLOR0] / sizeof(uint8_t)) * (fifo->Read8());

    vtx->col[0].R = ptr8[n + 0];
    vtx->col[0].G = ptr8[n + 1];
    vtx->col[0].B = ptr8[n + 2];
    vtx->col[0].A = ptr8[n + 3];
}

void clr0_rgb_rgb8_x16(GX::FifoProcessor* fifo)
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_COLOR0];
    unsigned n = fifo->Read16();
    n *= (cpRegs.arstride[VTX_COLOR0] / sizeof(uint8_t));

    vtx->col[0].R = ptr8[n + 0];
    vtx->col[0].G = ptr8[n + 1];
    vtx->col[0].B = ptr8[n + 2];
    vtx->col[0].A = 255;
}

void clr0_rgba_rgba8_x16(GX::FifoProcessor* fifo)
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_COLOR0];
    unsigned n = fifo->Read16();
    n *= (cpRegs.arstride[VTX_COLOR0] / sizeof(uint8_t));

    vtx->col[0].R = ptr8[n + 0];
    vtx->col[0].G = ptr8[n + 1];
    vtx->col[0].B = ptr8[n + 2];
    vtx->col[0].A = ptr8[n + 3];
}

// ---------------------------------------------------------------------------

// Color 1

void clr1_rgba_rgba8_x8(GX::FifoProcessor* fifo)
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_COLOR1];
    unsigned n = (cpRegs.arstride[VTX_COLOR1] / sizeof(uint8_t)) * (fifo->Read8());

    vtx->col[1].R = ptr8[n + 0];
    vtx->col[1].G = ptr8[n + 1];
    vtx->col[1].B = ptr8[n + 2];
    vtx->col[1].A = ptr8[n + 3];
}

// ---------------------------------------------------------------------------

// Texture coord 0

void tex0_st_u8_d(GX::FifoProcessor* fifo)
{
    // swap position data
    uint8_t u[2];
    u[0] = fifo->Read8();
    u[1] = fifo->Read8();

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];
}

void tex0_st_s8_d(GX::FifoProcessor* fifo)
{
    // swap position data
    int8_t u[2];
    u[0] = fifo->Read8();
    u[1] = fifo->Read8();

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];
}

void tex0_st_u16_d(GX::FifoProcessor* fifo)
{
    // swap position data
    uint16_t u[2];
    u[0] = fifo->Read16();
    u[1] = fifo->Read16();

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];
}

void tex0_st_s16_d(GX::FifoProcessor* fifo)
{
    // swap position data
    uint16_t u[2];
    u[0] = fifo->Read16();
    u[1] = fifo->Read16();

    //GFXError("u[0]:%04X, u[1]:%04X, %i", u[0], u[1], cpRegs.vatA[usevat].tex0shft);

    // fractional convertion
    vtx->tcoord[0][0] = ((float)(int16_t)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)(int16_t)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];

    //GFXError("s:%f, t:%f", vtx->tcoord[0][0], vtx->tcoord[0][1]);
}

void tex0_st_f32_d(GX::FifoProcessor* fifo)
{
    // we need to swap "floats", so "u" is used as temporary buffer
    uint32_t u[2];
    u[0] = fifo->Read32();
    u[1] = fifo->Read32();

    // save swapped data in vertex descriptor
    float *v = (float *)u;
    vtx->tcoord[0][0] = v[0];
    vtx->tcoord[0][1] = v[1];
}

void tex0_st_s8_x8(GX::FifoProcessor* fifo)
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(int8_t)) * (fifo->Read8());

    // swap position data
    int8_t u[2];
    u[0] = ptr8[n + 0];
    u[1] = ptr8[n + 1];

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];
}

void tex0_st_u16_x8(GX::FifoProcessor* fifo)
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(uint16_t)) * (fifo->Read8());

    // swap position data
    uint16_t u[2];
    u[0] = _byteswap_ushort(ptr16[n + 0]);
    u[1] = _byteswap_ushort(ptr16[n + 1]);

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];
}

void tex0_st_s16_x8(GX::FifoProcessor* fifo)
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(int16_t)) * (fifo->Read8());

    // swap position data
    int16_t u[2];
    u[0] = _byteswap_ushort(ptr16[n + 0]);
    u[1] = _byteswap_ushort(ptr16[n + 1]);

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];
}

void tex0_st_f32_x8(GX::FifoProcessor* fifo)
{
    uint32_t *ptr32 = (uint32_t *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(float)) * (fifo->Read8());

    // we need to swap "floats", so "u" is used as temporary buffer
    uint32_t u[2];
    u[0] = _byteswap_ulong(ptr32[n + 0]);
    u[1] = _byteswap_ulong(ptr32[n + 1]);

    // save swapped data in vertex descriptor
    float *fu = (float *)u;
    vtx->tcoord[0][0] = fu[0];
    vtx->tcoord[0][1] = fu[1];
}

void tex0_s_u8_x16(GX::FifoProcessor* fifo)
{
    uint8_t* ptr8 = (uint8_t *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = fifo->Read16();
    n *= (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(uint8_t));

    uint8_t u[2];
    u[0] = ptr8[n + 0];

    // save swapped data in vertex descriptor
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = 1.0f;
}

void tex0_st_u16_x16(GX::FifoProcessor* fifo)
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = fifo->Read16();
    n *= (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(uint16_t));

    // we need to swap "floats", so "u" is used as temporary buffer
    uint16_t u[2];
    u[0] = _byteswap_ushort(ptr16[n + 0]);
    u[1] = _byteswap_ushort(ptr16[n + 1]);

    // save swapped data in vertex descriptor
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];
}

void tex0_st_s16_x16(GX::FifoProcessor* fifo)
{
    uint16_t *ptr16 = (uint16_t *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = fifo->Read16();
    n *= (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(int16_t));

    // we need to swap "floats", so "u" is used as temporary buffer
    int16_t u[2];
    u[0] = _byteswap_ushort(ptr16[n + 0]);
    u[1] = _byteswap_ushort(ptr16[n + 1]);

    // save swapped data in vertex descriptor
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];
}

void tex0_st_f32_x16(GX::FifoProcessor* fifo)
{
    uint32_t *ptr32 = (uint32_t *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = fifo->Read16();
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

void tex1_s_u8_x8(GX::FifoProcessor* fifo)
{
    fifo->Read8();
}

void tex1_s_s8_x8(GX::FifoProcessor* fifo)
{
    fifo->Read8();
}

void tex1_s_u16_x8(GX::FifoProcessor* fifo)
{
    fifo->Read8();
}

void tex1_s_s16_x8(GX::FifoProcessor* fifo)
{
    fifo->Read8();
}

void tex1_s_f32_x8(GX::FifoProcessor* fifo)
{
    fifo->Read8();
}

void tex1_st_u8_x8(GX::FifoProcessor* fifo)
{
    fifo->Read8();
}

void tex1_st_s8_x8(GX::FifoProcessor* fifo)
{
    fifo->Read8();
}

void tex1_st_u16_x8(GX::FifoProcessor* fifo)
{
    fifo->Read8();
}

void tex1_st_s16_x8(GX::FifoProcessor* fifo)
{
    fifo->Read8();
}

void tex1_st_f32_x8(GX::FifoProcessor* fifo)
{
    fifo->Read8();
}

void tex1_s_u8_x16(GX::FifoProcessor* fifo)
{
    fifo->Read16();
}

void tex1_s_s8_x16(GX::FifoProcessor* fifo)
{
    fifo->Read16();
}

void tex1_s_u16_x16(GX::FifoProcessor* fifo)
{
    fifo->Read16();
}

void tex1_s_s16_x16(GX::FifoProcessor* fifo)
{
    fifo->Read16();
}

void tex1_s_f32_x16(GX::FifoProcessor* fifo)
{
    fifo->Read16();
}

void tex1_st_u8_x16(GX::FifoProcessor* fifo)
{
    fifo->Read16();
}

void tex1_st_s8_x16(GX::FifoProcessor* fifo)
{
    fifo->Read16();
}

void tex1_st_u16_x16(GX::FifoProcessor* fifo)
{
    fifo->Read16();
}

void tex1_st_s16_x16(GX::FifoProcessor* fifo)
{
    fifo->Read16();
}

void tex1_st_f32_x16(GX::FifoProcessor* fifo)
{
    fifo->Read16();
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

void (__fastcall *posattr[4][2][5])(GX::FifoProcessor* fifo) = {
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

void (__fastcall *nrmattr[4][3][5])(GX::FifoProcessor* fifo) = {
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

void (__fastcall *col0attr[4][2][6])(GX::FifoProcessor* fifo) = {
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

void (__fastcall *col1attr[4][2][6])(GX::FifoProcessor* fifo) = {
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

void (__fastcall *tex0attr[4][2][5])(GX::FifoProcessor* fifo) = {
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

void (__fastcall *tex1attr[4][2][5])(GX::FifoProcessor* fifo) = {
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

void (__fastcall *tex2attr[4][2][5])(GX::FifoProcessor* fifo) = {
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

void (__fastcall *tex3attr[4][2][5])(GX::FifoProcessor* fifo) = {
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

void (__fastcall *tex4attr[4][2][5])(GX::FifoProcessor* fifo) = {
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

void (__fastcall *tex5attr[4][2][5])(GX::FifoProcessor* fifo) = {
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

void (__fastcall *tex6attr[4][2][5])(GX::FifoProcessor* fifo) = {
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

void (__fastcall *tex7attr[4][2][5])(GX::FifoProcessor* fifo) = {
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
