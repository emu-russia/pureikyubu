// pipeline callbacks implementation

// ---------------------------------------------------------------------------

// Matrix index

u8 * __fastcall pos_idx(u8 *ptr)
{
    xfRegs.posidx = *ptr++;
    //GFXError("fifo posidx : %i", xfRegs.posidx);
    return ptr;
}

u8 * __fastcall t0_idx(u8 *ptr)
{
    xfRegs.texidx[0] = *ptr++;
    return ptr;
}

u8 * __fastcall t1_idx(u8 *ptr)
{
    xfRegs.texidx[1] = *ptr++;
    return ptr;
}

u8 * __fastcall t2_idx(u8 *ptr)
{
    xfRegs.texidx[2] = *ptr++;
    return ptr;
}

u8 * __fastcall t3_idx(u8 *ptr)
{
    xfRegs.texidx[3] = *ptr++;
    return ptr;
}

u8 * __fastcall t4_idx(u8 *ptr)
{
    xfRegs.texidx[4] = *ptr++;
    return ptr;
}

u8 * __fastcall t5_idx(u8 *ptr)
{
    xfRegs.texidx[5] = *ptr++;
    return ptr;
}

u8 * __fastcall t6_idx(u8 *ptr)
{
    xfRegs.texidx[6] = *ptr++;
    return ptr;
}

u8 * __fastcall t7_idx(u8 *ptr)
{
    xfRegs.texidx[7] = *ptr++;
    return ptr;
}

// ---------------------------------------------------------------------------

// Position

u8 * __fastcall pos_xy_u8_d(u8 *ptr)
{
    u8 u[3];
    u[0] = ptr[0];
    u[1] = ptr[1];

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;

    return (ptr += 2 * sizeof(u8));
}

u8 * __fastcall pos_xy_s8_d(u8 *ptr)
{
    s8 u[3];
    u[0] = ptr[0];
    u[1] = ptr[1];

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;

    return (ptr += 2 * sizeof(s8));
}

u8 * __fastcall pos_xy_u16_d(u8 *ptr)
{
    u16 *ptr16 = (u16 *)ptr;

    // swap position data
    u16 u[2];
    u[0] = swap16(ptr16[0]);
    u[1] = swap16(ptr16[1]);

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;

    return (ptr += 2 * sizeof(u16));
}

u8 * __fastcall pos_xy_s16_d(u8 *ptr)
{
    u16 *ptr16 = (u16 *)ptr;

    // swap position data
    s16 u[2];
    u[0] = swap16(ptr16[0]);
    u[1] = swap16(ptr16[1]);

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;

    return (ptr += 2 * sizeof(s16));
}

u8 * __fastcall pos_xy_f32_d(u8 *ptr)
{
    u32 *ptr32 = (u32 *)ptr;

    // we need to swap "floats", so "u" is used as temporary buffer
    u32 u[2];
    u[0] = swap32(ptr32[0]);
    u[1] = swap32(ptr32[1]);

    // save swapped data in vertex descriptor
    float *v = (float *)u;
    vtx->pos[0] = v[0];
    vtx->pos[1] = v[1];
    vtx->pos[2] = 1.0f;

    return (ptr += 2 * sizeof(float));
}

u8 * __fastcall pos_xyz_u8_d(u8 *ptr)
{
    u8 u[3];
    u[0] = ptr[0];
    u[1] = ptr[1];
    u[2] = ptr[2];

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = ((float)u[2]) / fracDenom[usevat][VTX_POS];

    return (ptr += 3 * sizeof(u8));
}

u8 * __fastcall pos_xyz_s8_d(u8 *ptr)
{
    s8 u[3];
    u[0] = ptr[0];
    u[1] = ptr[1];
    u[2] = ptr[2];

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = ((float)u[2]) / fracDenom[usevat][VTX_POS];

    return (ptr += 3 * sizeof(s8));
}

u8 * __fastcall pos_xyz_u16_d(u8 *ptr)
{
    u16 *ptr16 = (u16 *)ptr;

    // swap position data
    u16 u[3];
    u[0] = swap16(ptr16[0]);
    u[1] = swap16(ptr16[1]);
    u[2] = swap16(ptr16[2]);

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = ((float)u[2]) / fracDenom[usevat][VTX_POS];

    return (ptr += 3 * sizeof(u16));
}

u8 * __fastcall pos_xyz_s16_d(u8 *ptr)
{
    u16 *ptr16 = (u16 *)ptr;

    // swap position data
    s16 u[3];
    u[0] = swap16(ptr16[0]);
    u[1] = swap16(ptr16[1]);
    u[2] = swap16(ptr16[2]);

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = ((float)u[2]) / fracDenom[usevat][VTX_POS];

    return (ptr += 3 * sizeof(s16));
}

u8 * __fastcall pos_xyz_f32_d(u8 *ptr)
{
    u32 *ptr32 = (u32 *)ptr;

    // we need to swap "floats", so "u" is used as temporary buffer
    u32 u[3];
    u[0] = swap32(ptr32[0]);
    u[1] = swap32(ptr32[1]);
    u[2] = swap32(ptr32[2]);

    // save swapped data in vertex descriptor
    float *v = (float *)u;
    vtx->pos[0] = v[0];
    vtx->pos[1] = v[1];
    vtx->pos[2] = v[2];

    return (ptr += 3 * sizeof(float));
}

u8 * __fastcall pos_xy_u8_x8(u8 *ptr)
{
    u8 *ptr8 = (u8 *)cpRegs.arbase[VTX_POS];
    unsigned n = (cpRegs.arstride[VTX_POS] / sizeof(u8)) * (*ptr++);

    // swap position data
    u8 u[2];
    u[0] = ptr8[n + 0];
    u[1] = ptr8[n + 1];

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;

    return ptr;
}

u8 * __fastcall pos_xy_s8_x8(u8 *ptr)
{
    u8 *ptr8 = (u8 *)cpRegs.arbase[VTX_POS];
    unsigned n = (cpRegs.arstride[VTX_POS] / sizeof(s8)) * (*ptr++);

    // swap position data
    s8 u[2];
    u[0] = ptr8[n + 0];
    u[1] = ptr8[n + 1];

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;

    return ptr;
}

u8 * __fastcall pos_xy_s16_x8(u8 *ptr)
{
    u16 *ptr16 = (u16 *)cpRegs.arbase[VTX_POS];
    unsigned n = (cpRegs.arstride[VTX_POS] / sizeof(s16)) * (*ptr++);

    // swap position data
    s16 u[2];
    u[0] = swap16(ptr16[n + 0]);
    u[1] = swap16(ptr16[n + 1]);

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = 1.0f;

    return ptr;
}

u8 * __fastcall pos_xyz_s16_x8(u8 *ptr)
{
    u16 *ptr16 = (u16 *)cpRegs.arbase[VTX_POS];
    unsigned n = (cpRegs.arstride[VTX_POS] / sizeof(s16)) * (*ptr++);

    // swap position data
    s16 u[3];
    u[0] = swap16(ptr16[n + 0]);
    u[1] = swap16(ptr16[n + 1]);
    u[2] = swap16(ptr16[n + 2]);

    // fractional convertion
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = ((float)u[2]) / fracDenom[usevat][VTX_POS];

    return ptr;
}

u8 * __fastcall pos_xyz_f32_x8(u8 *ptr)
{
    u32 *ptr32 = (u32 *)cpRegs.arbase[VTX_POS];
    unsigned n = (cpRegs.arstride[VTX_POS] / sizeof(float)) * (*ptr++);

    // we need to swap "floats", so "u" is used as temporary buffer
    u32 u[3];
    u[0] = swap32(ptr32[n + 0]);
    u[1] = swap32(ptr32[n + 1]);
    u[2] = swap32(ptr32[n + 2]);

    // save swapped data in vertex descriptor
    float *fu = (float *)u;
    vtx->pos[0] = fu[0];
    vtx->pos[1] = fu[1];
    vtx->pos[2] = fu[2];

    return ptr;
}

u8 * __fastcall pos_xyz_s16_x16(u8 *ptr)
{
    u16 *ptr16 = (u16 *)cpRegs.arbase[VTX_POS];
    unsigned n = swap16(*((u16 *)ptr));
    n *= (cpRegs.arstride[VTX_POS] / sizeof(s16));
    ptr += 2;

    // we need to swap "floats", so "u" is used as temporary buffer
    s16 u[3];
    u[0] = swap16(ptr16[n + 0]);
    u[1] = swap16(ptr16[n + 1]);
    u[2] = swap16(ptr16[n + 2]);

    // save swapped data in vertex descriptor
    vtx->pos[0] = ((float)u[0]) / fracDenom[usevat][VTX_POS];
    vtx->pos[1] = ((float)u[1]) / fracDenom[usevat][VTX_POS];
    vtx->pos[2] = ((float)u[2]) / fracDenom[usevat][VTX_POS];

    return ptr;
}

u8 * __fastcall pos_xyz_f32_x16(u8 *ptr)
{
    u32 *ptr32 = (u32 *)cpRegs.arbase[VTX_POS];
    unsigned n = swap16(*((u16 *)ptr));
    n *= (cpRegs.arstride[VTX_POS] / sizeof(float));
    ptr += 2;

    // we need to swap "floats", so "u" is used as temporary buffer
    u32 u[3];
    u[0] = swap32(ptr32[n + 0]);
    u[1] = swap32(ptr32[n + 1]);
    u[2] = swap32(ptr32[n + 2]);

    // save swapped data in vertex descriptor
    float *fu = (float *)u;
    vtx->pos[0] = fu[0];
    vtx->pos[1] = fu[1];
    vtx->pos[2] = fu[2];

    return ptr;
}

// ---------------------------------------------------------------------------

// Normal data

u8 * __fastcall nrm_xyz_u8_d(u8 *ptr)
{
    u8 u[3];
    u[0] = ptr[0];
    u[1] = ptr[1];
    u[2] = ptr[2];

    // fractional convertion
    vtx->nrm[0] = ((float)u[0])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[1] = ((float)u[1])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[2] = ((float)u[2])/* / fracDenom[usevat][VTX_NRM]*/;

    return (ptr += 3 * sizeof(u8));
}

u8 * __fastcall nrm_xyz_s8_d(u8 *ptr)
{
    s8 u[3];
    u[0] = ptr[0];
    u[1] = ptr[1];
    u[2] = ptr[2];

    // fractional convertion
    vtx->nrm[0] = ((float)u[0])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[1] = ((float)u[1])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[2] = ((float)u[2])/* / fracDenom[usevat][VTX_NRM]*/;

    return (ptr += 3 * sizeof(s8));
}

u8 * __fastcall nrm_xyz_u16_d(u8 *ptr)
{
    u16 *ptr16 = (u16 *)ptr;

    // swap position data
    u16 u[3];
    u[0] = swap16(ptr16[0]);
    u[1] = swap16(ptr16[1]);
    u[2] = swap16(ptr16[2]);

    // fractional convertion
    vtx->nrm[0] = ((float)u[0])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[1] = ((float)u[1])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[2] = ((float)u[2])/* / fracDenom[usevat][VTX_NRM]*/;

    return (ptr += 3 * sizeof(u16));
}

u8 * __fastcall nrm_xyz_s16_d(u8 *ptr)
{
    u16 *ptr16 = (u16 *)ptr;

    // swap position data
    s16 u[3];
    u[0] = swap16(ptr16[0]);
    u[1] = swap16(ptr16[1]);
    u[2] = swap16(ptr16[2]);

    // fractional convertion
    vtx->nrm[0] = ((float)u[0])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[1] = ((float)u[1])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[2] = ((float)u[2])/* / fracDenom[usevat][VTX_NRM]*/;

    return (ptr += 3 * sizeof(s16));
}

u8 * __fastcall nrm_xyz_f32_d(u8 *ptr)
{
    u32 *ptr32 = (u32 *)ptr;

    // we need to swap "floats", so "u" is used as temporary buffer
    u32 u[3];
    u[0] = swap32(ptr32[0]);
    u[1] = swap32(ptr32[1]);
    u[2] = swap32(ptr32[2]);

    // save swapped data in vertex descriptor
    float *v = (float *)u;
    vtx->nrm[0] = v[0];
    vtx->nrm[1] = v[1];
    vtx->nrm[2] = v[2];

    return (ptr += 3 * sizeof(float));
}

// normal only
// TODO : make ortho for nbt
u8 * __fastcall nrm_nbt_f32_d(u8 *ptr)
{
    u32 *ptr32 = (u32 *)ptr;

    // we need to swap "floats", so "u" is used as temporary buffer
    u32 u[3];
    u[0] = swap32(ptr32[0]);
    u[1] = swap32(ptr32[1]);
    u[2] = swap32(ptr32[2]);

    // save swapped data in vertex descriptor
    float *v = (float *)u;
    vtx->nrm[0] = v[0];
    vtx->nrm[1] = v[1];
    vtx->nrm[2] = v[2];

    return (ptr += 3 * 3 * sizeof(float));
}

u8 * __fastcall nrm_xyz_u8_x8(u8 *ptr)
{
    u8 *ptr8 = (u8 *)cpRegs.arbase[VTX_NRM];
    unsigned n = (cpRegs.arstride[VTX_NRM] / sizeof(u8)) * (*ptr++);

    // swap position data
    u8 u[3];
    u[0] = ptr8[n + 0];
    u[1] = ptr8[n + 1];
    u[2] = ptr8[n + 2];

    // fractional convertion
    vtx->nrm[0] = ((float)u[0])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[1] = ((float)u[1])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[2] = ((float)u[2])/* / fracDenom[usevat][VTX_NRM]*/;

    return ptr;
}

u8 * __fastcall nrm_xyz_s8_x8(u8 *ptr)
{
    u8 *ptr8 = (u8 *)cpRegs.arbase[VTX_NRM];
    unsigned n = (cpRegs.arstride[VTX_NRM] / sizeof(u8)) * (*ptr++);

    // swap position data
    s8 u[3];
    u[0] = ptr8[n + 0];
    u[1] = ptr8[n + 1];
    u[2] = ptr8[n + 2];

    // fractional convertion
    vtx->nrm[0] = ((float)u[0])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[1] = ((float)u[1])/* / fracDenom[usevat][VTX_NRM]*/;
    vtx->nrm[2] = ((float)u[2])/* / fracDenom[usevat][VTX_NRM]*/;

    return ptr;
}

u8 * __fastcall nrm_xyz_s16_x8(u8 *ptr)
{
    u16 *ptr16 = (u16 *)cpRegs.arbase[VTX_NRM];
    unsigned n = (cpRegs.arstride[VTX_NRM] / sizeof(s16)) * (*ptr++);

    // swap position data
    s16 u[3];
    u[0] = swap16(ptr16[n + 0]);
    u[1] = swap16(ptr16[n + 1]);
    u[2] = swap16(ptr16[n + 2]);

    // fractional convertion
    vtx->nrm[0] = ((float)u[0]);
    vtx->nrm[1] = ((float)u[1]);
    vtx->nrm[2] = ((float)u[2]);

    return ptr;
}

u8 * __fastcall nrm_xyz_f32_x8(u8 *ptr)
{
    u32 *ptr32 = (u32 *)cpRegs.arbase[VTX_NRM];
    unsigned n = (cpRegs.arstride[VTX_NRM] / sizeof(float)) * (*ptr++);

    // we need to swap "floats", so "u" is used as temporary buffer
    u32 u[3];
    u[0] = swap32(ptr32[n + 0]);
    u[1] = swap32(ptr32[n + 1]);
    u[2] = swap32(ptr32[n + 2]);

    // save swapped data in vertex descriptor
    float *fu = (float *)u;
    vtx->nrm[0] = fu[0];
    vtx->nrm[1] = fu[1];
    vtx->nrm[2] = fu[2];

    return ptr;
}

u8 * __fastcall nrm_nbt_s16_x8(u8 *ptr)
{
    u16 *ptr16 = (u16 *)cpRegs.arbase[VTX_NRM];
    unsigned n = (cpRegs.arstride[VTX_NRM] / sizeof(s16)) * (*ptr++);

    // swap normal data
    s16 u[3];
    u[0] = swap16(ptr16[n + 0]);
    u[1] = swap16(ptr16[n + 1]);
    u[2] = swap16(ptr16[n + 2]);

    // fractional convertion
    vtx->nrm[0] = ((float)u[0]);
    vtx->nrm[1] = ((float)u[1]);
    vtx->nrm[2] = ((float)u[2]);

    return ptr;
}

u8 * __fastcall nrm_xyz_s16_x16(u8 *ptr)
{
    u16 *ptr16 = (u16 *)cpRegs.arbase[VTX_NRM];
    unsigned n = swap16(*((u16 *)ptr));
    n *= (cpRegs.arstride[VTX_NRM] / sizeof(s16));
    ptr += 2;

    // swap normal data
    s16 u[3];
    u[0] = swap16(ptr16[n + 0]);
    u[1] = swap16(ptr16[n + 1]);
    u[2] = swap16(ptr16[n + 2]);

    // save swapped data in vertex descriptor
    vtx->nrm[0] = (float)u[0];
    vtx->nrm[1] = (float)u[1];
    vtx->nrm[2] = (float)u[2];

    return ptr;

}

u8 * __fastcall nrm_xyz_f32_x16(u8 *ptr)
{
    u32 *ptr32 = (u32 *)cpRegs.arbase[VTX_NRM];
    unsigned n = swap16(*((u16 *)ptr));
    n *= (cpRegs.arstride[VTX_NRM] / sizeof(float));
    ptr += 2;

    // we need to swap "floats", so "u" is used as temporary buffer
    u32 u[3];
    u[0] = swap32(ptr32[n + 0]);
    u[1] = swap32(ptr32[n + 1]);
    u[2] = swap32(ptr32[n + 2]);

    // save swapped data in vertex descriptor
    float *fu = (float *)u;
    vtx->nrm[0] = fu[0];
    vtx->nrm[1] = fu[1];
    vtx->nrm[2] = fu[2];

    return ptr;
}

// ---------------------------------------------------------------------------

// Color 0

u8 * __fastcall clr0_rgb_rgb565_d(u8 *ptr)
{
    u16 p = swap16(*(u16 *)ptr);

    u8 r = p >> 11;
    u8 g = (p >> 5) & 0x3f;
    u8 b = p & 0x1f;

    vtx->col[0].R = (r << 3) | (r >> 2);
    vtx->col[0].G = (g << 2) | (g >> 4);
    vtx->col[0].B = (b << 3) | (b >> 2);
    vtx->col[0].A = 255;

    return (ptr += 2);
}

u8 * __fastcall clr0_rgb_rgb8_d(u8 *ptr)
{
    vtx->col[0].R = ptr[0];
    vtx->col[0].G = ptr[1];
    vtx->col[0].B = ptr[2];
    vtx->col[0].A = 255;

    return (ptr += 3);
}

u8 * __fastcall clr0_rgba_rgba4_d(u8 *ptr)
{
    u16 p = swap16(*(u16 *)ptr);

    u8 r = (p >>12) & 0xf;
    u8 g = (p >> 8) & 0xf;
    u8 b = (p >> 4) & 0xf;
    u8 a = (p >> 0) & 0xf;

    vtx->col[0].R = (r << 4) | r;
    vtx->col[0].G = (g << 4) | g;
    vtx->col[0].B = (b << 4) | b;
    vtx->col[0].A = (a << 4) | a;

    return (ptr += 2);
}

u8 * __fastcall clr0_rgba_rgba8_d(u8 *ptr)
{
    vtx->col[0].R = ptr[0];
    vtx->col[0].G = ptr[1];
    vtx->col[0].B = ptr[2];
    vtx->col[0].A = ptr[3];

    return (ptr += 4);
}

u8 * __fastcall clr0_rgb_rgba8_x8(u8 *ptr)
{
    u8 *ptr8 = (u8 *)cpRegs.arbase[VTX_COLOR0];
    unsigned n = (cpRegs.arstride[VTX_COLOR0] / sizeof(u8)) * (*ptr++);

    vtx->col[0].R = ptr8[n + 0];
    vtx->col[0].G = ptr8[n + 1];
    vtx->col[0].B = ptr8[n + 2];
    vtx->col[0].A = 255;

    return ptr;
}

u8 * __fastcall clr0_rgba_rgba8_x8(u8 *ptr)
{
    u8 *ptr8 = (u8 *)cpRegs.arbase[VTX_COLOR0];
    unsigned n = (cpRegs.arstride[VTX_COLOR0] / sizeof(u8)) * (*ptr++);

    vtx->col[0].R = ptr8[n + 0];
    vtx->col[0].G = ptr8[n + 1];
    vtx->col[0].B = ptr8[n + 2];
    vtx->col[0].A = ptr8[n + 3];

    return ptr;
}

u8 * __fastcall clr0_rgb_rgb8_x16(u8 *ptr)
{
    u8 *ptr8 = (u8 *)cpRegs.arbase[VTX_COLOR0];
    unsigned n = swap16(*((u16 *)ptr));
    n *= (cpRegs.arstride[VTX_COLOR0] / sizeof(u8));
    ptr += 2;

    vtx->col[0].R = ptr8[n + 0];
    vtx->col[0].G = ptr8[n + 1];
    vtx->col[0].B = ptr8[n + 2];
    vtx->col[0].A = 255;

    return ptr;
}

u8 * __fastcall clr0_rgba_rgba8_x16(u8 *ptr)
{
    u8 *ptr8 = (u8 *)cpRegs.arbase[VTX_COLOR0];
    unsigned n = swap16(*((u16 *)ptr));
    n *= (cpRegs.arstride[VTX_COLOR0] / sizeof(u8));
    ptr += 2;

    vtx->col[0].R = ptr8[n + 0];
    vtx->col[0].G = ptr8[n + 1];
    vtx->col[0].B = ptr8[n + 2];
    vtx->col[0].A = ptr8[n + 3];

    return ptr;
}

// ---------------------------------------------------------------------------

// Color 1

u8 * __fastcall clr1_rgba_rgba8_x8(u8 *ptr)
{
    u8 *ptr8 = (u8 *)cpRegs.arbase[VTX_COLOR1];
    unsigned n = (cpRegs.arstride[VTX_COLOR1] / sizeof(u8)) * (*ptr++);

    vtx->col[1].R = ptr8[n + 0];
    vtx->col[1].G = ptr8[n + 1];
    vtx->col[1].B = ptr8[n + 2];
    vtx->col[1].A = ptr8[n + 3];

    return ptr;
}

// ---------------------------------------------------------------------------

// Texture coord 0

u8 * __fastcall tex0_st_u8_d(u8 *ptr)
{
    u8 *ptru = (u8 *)ptr;

    // swap position data
    u8 u[2];
    u[0] = ptru[0];
    u[1] = ptru[1];

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];

    return (ptr += 2 * sizeof(u8));
}

u8 * __fastcall tex0_st_s8_d(u8 *ptr)
{
    s8 *ptrs = (s8 *)ptr;

    // swap position data
    s8 u[2];
    u[0] = ptrs[0];
    u[1] = ptrs[1];

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];

    return (ptr += 2 * sizeof(s8));
}

u8 * __fastcall tex0_st_u16_d(u8 *ptr)
{
    u16 *ptr16 = (u16 *)ptr;

    // swap position data
    u16 u[2];
    u[0] = swap16(ptr16[0]);
    u[1] = swap16(ptr16[1]);

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];

    return (ptr += 2 * sizeof(u16));
}

u8 * __fastcall tex0_st_s16_d(u8 *ptr)
{
    u16 *ptr16 = (u16 *)ptr;

    // swap position data
    u16 u[2];
    u[0] = swap16(ptr16[0]);
    u[1] = swap16(ptr16[1]);

    //GFXError("u[0]:%04X, u[1]:%04X, %i", u[0], u[1], cpRegs.vatA[usevat].tex0shft);

    // fractional convertion
    vtx->tcoord[0][0] = ((float)(s16)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)(s16)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];

    //GFXError("s:%f, t:%f", vtx->tcoord[0][0], vtx->tcoord[0][1]);

    return (ptr += 2 * sizeof(s16));
}

u8 * __fastcall tex0_st_f32_d(u8 *ptr)
{
    u32 *ptr32 = (u32 *)ptr;

    // we need to swap "floats", so "u" is used as temporary buffer
    u32 u[2];
    u[0] = swap32(ptr32[0]);
    u[1] = swap32(ptr32[1]);

    // save swapped data in vertex descriptor
    float *v = (float *)u;
    vtx->tcoord[0][0] = v[0];
    vtx->tcoord[0][1] = v[1];

    return (ptr += 2 * sizeof(float));
}

u8 * __fastcall tex0_st_s8_x8(u8 *ptr)
{
    u8 *ptr8 = (u8 *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(s8)) * (*ptr++);

    // swap position data
    s8 u[2];
    u[0] = ptr8[n + 0];
    u[1] = ptr8[n + 1];

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];

    return ptr;
}

u8 * __fastcall tex0_st_u16_x8(u8 *ptr)
{
    u16 *ptr16 = (u16 *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(u16)) * (*ptr++);

    // swap position data
    u16 u[2];
    u[0] = swap16(ptr16[n + 0]);
    u[1] = swap16(ptr16[n + 1]);

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];

    return ptr;
}

u8 * __fastcall tex0_st_s16_x8(u8 *ptr)
{
    u16 *ptr16 = (u16 *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(s16)) * (*ptr++);

    // swap position data
    s16 u[2];
    u[0] = swap16(ptr16[n + 0]);
    u[1] = swap16(ptr16[n + 1]);

    // fractional convertion
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];

    return ptr;
}

u8 * __fastcall tex0_st_f32_x8(u8 *ptr)
{
    u32 *ptr32 = (u32 *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(float)) * (*ptr++);

    // we need to swap "floats", so "u" is used as temporary buffer
    u32 u[2];
    u[0] = swap32(ptr32[n + 0]);
    u[1] = swap32(ptr32[n + 1]);

    // save swapped data in vertex descriptor
    float *fu = (float *)u;
    vtx->tcoord[0][0] = fu[0];
    vtx->tcoord[0][1] = fu[1];

    return ptr;
}

u8 * __fastcall tex0_s_u8_x16(u8 *ptr)
{
    u8 *ptr8 = (u8 *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = swap16(*((u16 *)ptr));
    n *= (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(u8));
    ptr += 2;

    u8 u[2];
    u[0] = ptr8[n + 0];

    // save swapped data in vertex descriptor
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = 1.0f;

    return ptr;
}

u8 * __fastcall tex0_st_u16_x16(u8 *ptr)
{
    u16 *ptr16 = (u16 *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = swap16(*((u16 *)ptr));
    n *= (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(u16));
    ptr += 2;

    // we need to swap "floats", so "u" is used as temporary buffer
    u16 u[2];
    u[0] = swap16(ptr16[n + 0]);
    u[1] = swap16(ptr16[n + 1]);

    // save swapped data in vertex descriptor
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];

    return ptr;
}

u8 * __fastcall tex0_st_s16_x16(u8 *ptr)
{
    u16 *ptr16 = (u16 *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = swap16(*((u16 *)ptr));
    n *= (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(s16));
    ptr += 2;

    // we need to swap "floats", so "u" is used as temporary buffer
    s16 u[2];
    u[0] = swap16(ptr16[n + 0]);
    u[1] = swap16(ptr16[n + 1]);

    // save swapped data in vertex descriptor
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];

    return ptr;
}

u8 * __fastcall tex0_st_f32_x16(u8 *ptr)
{
    u32 *ptr32 = (u32 *)cpRegs.arbase[VTX_TEXCOORD0];
    unsigned n = swap16(*((u16 *)ptr));
    n *= (cpRegs.arstride[VTX_TEXCOORD0] / sizeof(float));
    ptr += 2;

    // we need to swap "floats", so "u" is used as temporary buffer
    u32 u[2];
    u[0] = swap32(ptr32[n + 0]);
    u[1] = swap32(ptr32[n + 1]);

    // save swapped data in vertex descriptor
    vtx->tcoord[0][0] = ((float)u[0]) / fracDenom[usevat][VTX_TEXCOORD0];
    vtx->tcoord[0][1] = ((float)u[1]) / fracDenom[usevat][VTX_TEXCOORD0];

    return ptr;
}

// ---------------------------------------------------------------------------

// Texture coord 1

u8 * __fastcall tex1_s_u8_x8(u8 *ptr)
{
    return ++ptr;
}

u8 * __fastcall tex1_s_s8_x8(u8 *ptr)
{
    return ++ptr;
}

u8 * __fastcall tex1_s_u16_x8(u8 *ptr)
{
    return ++ptr;
}

u8 * __fastcall tex1_s_s16_x8(u8 *ptr)
{
    return ++ptr;
}

u8 * __fastcall tex1_s_f32_x8(u8 *ptr)
{
    return ++ptr;
}

u8 * __fastcall tex1_st_u8_x8(u8 *ptr)
{
    return ++ptr;
}

u8 * __fastcall tex1_st_s8_x8(u8 *ptr)
{
    return ++ptr;
}

u8 * __fastcall tex1_st_u16_x8(u8 *ptr)
{
    return ++ptr;
}

u8 * __fastcall tex1_st_s16_x8(u8 *ptr)
{
    return ++ptr;
}

u8 * __fastcall tex1_st_f32_x8(u8 *ptr)
{
    return ++ptr;
}

u8 * __fastcall tex1_s_u8_x16(u8 *ptr)
{
    ptr += 2;
    return ptr;
}

u8 * __fastcall tex1_s_s8_x16(u8 *ptr)
{
    ptr += 2;
    return ptr;
}

u8 * __fastcall tex1_s_u16_x16(u8 *ptr)
{
    ptr += 2;
    return ptr;
}

u8 * __fastcall tex1_s_s16_x16(u8 *ptr)
{
    ptr += 2;
    return ptr;
}

u8 * __fastcall tex1_s_f32_x16(u8 *ptr)
{
    ptr += 2;
    return ptr;
}

u8 * __fastcall tex1_st_u8_x16(u8 *ptr)
{
    ptr += 2;
    return ptr;
}

u8 * __fastcall tex1_st_s8_x16(u8 *ptr)
{
    ptr += 2;
    return ptr;
}

u8 * __fastcall tex1_st_u16_x16(u8 *ptr)
{
    ptr += 2;
    return ptr;
}

u8 * __fastcall tex1_st_s16_x16(u8 *ptr)
{
    ptr += 2;
    return ptr;
}

u8 * __fastcall tex1_st_f32_x16(u8 *ptr)
{
    ptr += 2;
    return ptr;
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
