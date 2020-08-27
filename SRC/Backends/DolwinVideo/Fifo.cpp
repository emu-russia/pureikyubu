// fifo parser engine
#include "pch.h"

using namespace Debug;

//
// local data
//

Vertex  *vtx;                                   // current vertex to 
                                                        // collect data
int     VtxSize[8];

unsigned usevat;                                // current VAT

GX_FromFuture::FifoProcessor GxFifo;

bool frame_done = true;

// ---------------------------------------------------------------------------

// stage callbacks

// Helper function
static std::string AttrToString(VTX_ATTR attr)
{
    switch(attr)
    {
        case VTX_POSMATIDX:     return "Position Matrix Index";
        case VTX_TEX0MTXIDX:    return "Texture Coordinate 0 Matrix Index";
        case VTX_TEX1MTXIDX:    return "Texture Coordinate 1 Matrix Index";
        case VTX_TEX2MTXIDX:    return "Texture Coordinate 2 Matrix Index";
        case VTX_TEX3MTXIDX:    return "Texture Coordinate 3 Matrix Index";
        case VTX_TEX4MTXIDX:    return "Texture Coordinate 4 Matrix Index";
        case VTX_TEX5MTXIDX:    return "Texture Coordinate 5 Matrix Index";
        case VTX_TEX6MTXIDX:    return "Texture Coordinate 6 Matrix Index";
        case VTX_TEX7MTXIDX:    return "Texture Coordinate 7 Matrix Index";
        case VTX_POS:           return "Position";
        case VTX_NRM:           return "Normal or Normal/Binormal/Tangent";
        case VTX_COLOR0:        return "Color 0";
        case VTX_COLOR1:        return "Color 1";
        case VTX_TEXCOORD0:     return "Texture Coordinate 0";
        case VTX_TEXCOORD1:     return "Texture Coordinate 1";
        case VTX_TEXCOORD2:     return "Texture Coordinate 2";
        case VTX_TEXCOORD3:     return "Texture Coordinate 3";
        case VTX_TEXCOORD4:     return "Texture Coordinate 4";
        case VTX_TEXCOORD5:     return "Texture Coordinate 5";
        case VTX_TEXCOORD6:     return "Texture Coordinate 6";
        case VTX_TEXCOORD7:     return "Texture Coordinate 7";
        case VTX_MAX_ATTR :     return "MAX attr";
    }
    return "Unknown attribute";
}

// calculate size of current vertex
static int gx_vtxsize(unsigned v)
{
    int vtxsize = 0;
    static int cntp[] = { 2, 3 };
    static int cntn[] = { 3, 9 };
    static int cntt[] = { 1, 2 };
    static int fmtsz[] = { 1, 1, 2, 2, 4 };
    static int cfmtsz[] = { 2, 3, 4, 2, 4, 4 };

    if (cpRegs.vcdLo.pmidx)  vtxsize++;
    if (cpRegs.vcdLo.t0midx) vtxsize++;
    if (cpRegs.vcdLo.t1midx) vtxsize++;
    if (cpRegs.vcdLo.t2midx) vtxsize++;
    if (cpRegs.vcdLo.t3midx) vtxsize++;
    if (cpRegs.vcdLo.t4midx) vtxsize++;
    if (cpRegs.vcdLo.t5midx) vtxsize++;
    if (cpRegs.vcdLo.t6midx) vtxsize++;
    if (cpRegs.vcdLo.t7midx) vtxsize++;

    // Position

    switch (cpRegs.vcdLo.pos)
    {
        case VCD_DIRECT:
            vtxsize += fmtsz[cpRegs.vatA[v].posfmt] * cntp[cpRegs.vatA[v].poscnt];
            break;
        case VCD_INDX8:
            vtxsize += 1;
            break;
        case VCD_INDX16:
            vtxsize += 2;
            break;
    }

    // Normal

    switch (cpRegs.vcdLo.nrm)
    {
        case VCD_DIRECT:
            vtxsize += fmtsz[cpRegs.vatA[v].nrmfmt] * cntn[cpRegs.vatA[v].nrmcnt];
            break;
        case VCD_INDX8:
            if (cpRegs.vatA[v].nrmidx3) vtxsize += 3;
            else vtxsize += 1;
            break;
        case VCD_INDX16:
            if (cpRegs.vatA[v].nrmidx3) vtxsize += 2 * 3;
            else vtxsize += 2;
            break;
    }

    // Colors

    switch (cpRegs.vcdLo.col0)
    {
        case VCD_DIRECT:
            vtxsize += cfmtsz[cpRegs.vatA[v].col0fmt];
            break;
        case VCD_INDX8:
            vtxsize += 1;
            break;
        case VCD_INDX16:
            vtxsize += 2;
            break;
    }

    switch (cpRegs.vcdLo.col1)
    {
        case VCD_DIRECT:
            vtxsize += cfmtsz[cpRegs.vatA[v].col1fmt];
            break;
        case VCD_INDX8:
            vtxsize += 1;
            break;
        case VCD_INDX16:
            vtxsize += 2;
            break;
    }

    // TexCoords

    switch (cpRegs.vcdHi.tex0)
    {
        case VCD_DIRECT:
            vtxsize += fmtsz[cpRegs.vatA[v].tex0fmt] * cntt[cpRegs.vatA[v].tex0cnt];
            break;
        case VCD_INDX8:
            vtxsize += 1;
            break;
        case VCD_INDX16:
            vtxsize += 2;
            break;
    }

    switch (cpRegs.vcdHi.tex1)
    {
        case VCD_DIRECT:
            vtxsize += fmtsz[cpRegs.vatB[v].tex1fmt] * cntt[cpRegs.vatB[v].tex1cnt];
            break;
        case VCD_INDX8:
            vtxsize += 1;
            break;
        case VCD_INDX16:
            vtxsize += 2;
            break;
    }

    switch (cpRegs.vcdHi.tex2)
    {
        case VCD_DIRECT:
            vtxsize += fmtsz[cpRegs.vatB[v].tex2fmt] * cntt[cpRegs.vatB[v].tex2cnt];
            break;
        case VCD_INDX8:
            vtxsize += 1;
            break;
        case VCD_INDX16:
            vtxsize += 2;
            break;
    }

    switch (cpRegs.vcdHi.tex3)
    {
        case VCD_DIRECT:
            vtxsize += fmtsz[cpRegs.vatB[v].tex3fmt] * cntt[cpRegs.vatB[v].tex3cnt];
            break;
        case VCD_INDX8:
            vtxsize += 1;
            break;
        case VCD_INDX16:
            vtxsize += 2;
            break;
    }

    switch (cpRegs.vcdHi.tex4)
    {
        case VCD_DIRECT:
            vtxsize += fmtsz[cpRegs.vatB[v].tex4fmt] * cntt[cpRegs.vatB[v].tex4cnt];
            break;
        case VCD_INDX8:
            vtxsize += 1;
            break;
        case VCD_INDX16:
            vtxsize += 2;
            break;
    }

    switch (cpRegs.vcdHi.tex5)
    {
        case VCD_DIRECT:
            vtxsize += fmtsz[cpRegs.vatC[v].tex5fmt] * cntt[cpRegs.vatC[v].tex5cnt];
            break;
        case VCD_INDX8:
            vtxsize += 1;
            break;
        case VCD_INDX16:
            vtxsize += 2;
            break;
    }

    switch (cpRegs.vcdHi.tex6)
    {
        case VCD_DIRECT:
            vtxsize += fmtsz[cpRegs.vatC[v].tex6fmt] * cntt[cpRegs.vatC[v].tex6cnt];
            break;
        case VCD_INDX8:
            vtxsize += 1;
            break;
        case VCD_INDX16:
            vtxsize += 2;
            break;
    }

    switch (cpRegs.vcdHi.tex7)
    {
        case VCD_DIRECT:
            vtxsize += fmtsz[cpRegs.vatC[v].tex7fmt] * cntt[cpRegs.vatC[v].tex7cnt];
            break;
        case VCD_INDX8:
            vtxsize += 1;
            break;
        case VCD_INDX16:
            vtxsize += 2;
            break;
    }

    return vtxsize;
}

void FifoReconfigure()
{
    for (unsigned v = 0; v < 8; v++)
    {
        VtxSize[v] = gx_vtxsize(v);
    }
}

// ---------------------------------------------------------------------------

void * GetArrayPtr(ArrayId arrayId, int idx, int compSize)
{
    uint32_t address = cpRegs.arbase[(size_t)arrayId] + (uint32_t)idx * cpRegs.arstride[(size_t)arrayId];
    return &RAM[address & 0x03ff'ffff];
}

void FetchComp(float* comp, int count, int type, int fmt, int shft, GX_FromFuture::FifoProcessor* fifo, ArrayId arrayId)
{
    void* ptr;
    static int fmtsz[] = { 1, 1, 2, 2, 4 };

    union
    {
        uint8_t u8[3];
        uint16_t u16[3];
        int8_t s8[3];
        int16_t s16[3];
        uint32_t u32[3];
    } Comp;

    switch (type)
    {
        case VCD_NONE:      // Skip attribute
            return;
        case VCD_INDX8:
            ptr = GetArrayPtr(arrayId, fifo->Read8(), fmtsz[fmt]);
            break;
        case VCD_INDX16:
            ptr = GetArrayPtr(arrayId, fifo->Read16(), fmtsz[fmt]);
            break;
        default:
            ptr = nullptr;
            break;
    }

    switch (fmt)
    {
        case VFMT_U8:
            if (type == VCD_DIRECT)
            {
                for (int i = 0; i < count; i++)
                {
                    Comp.u8[i] = fifo->Read8();
                }
            }
            else
            {
                for (int i = 0; i < count; i++)
                {
                    Comp.u8[i] = ((uint8_t*)ptr)[i];
                }
            }

            for (int i = 0; i < count; i++)
            {
                comp[i] = (float)(Comp.u8[i]) / pow(2.0, shft);
            }
            break;

        case VFMT_S8:
            if (type == VCD_DIRECT)
            {
                for (int i = 0; i < count; i++)
                {
                    Comp.s8[i] = fifo->Read8();
                }
            }
            else
            {
                for (int i = 0; i < count; i++)
                {
                    Comp.s8[i] = ((uint8_t*)ptr)[i];
                }
            }

            for (int i = 0; i < count; i++)
            {
                comp[i] = (float)(Comp.s8[i]) / pow(2.0, shft);
            }
            break;

        case VFMT_U16:
            if (type == VCD_DIRECT)
            {
                for (int i = 0; i < count; i++)
                {
                    Comp.u16[i] = fifo->Read16();
                }
            }
            else
            {
                for (int i = 0; i < count; i++)
                {
                    Comp.u16[i] = _byteswap_ushort(((uint16_t*)ptr)[i]);
                }
            }

            for (int i = 0; i < count; i++)
            {
                comp[i] = (float)(Comp.u16[i]) / pow(2.0, shft);
            }
            break;

        case VFMT_S16:
            if (type == VCD_DIRECT)
            {
                for (int i = 0; i < count; i++)
                {
                    Comp.s16[i] = fifo->Read16();
                }
            }
            else
            {
                for (int i = 0; i < count; i++)
                {
                    Comp.s16[i] = _byteswap_ushort(((uint16_t*)ptr)[i]);
                }
            }

            for (int i = 0; i < count; i++)
            {
                comp[i] = (float)(Comp.s16[i]) / pow(2.0, shft);
            }
            break;

        case VFMT_F32:
            if (type == VCD_DIRECT)
            {
                for (int i = 0; i < count; i++)
                {
                    Comp.u32[i] = fifo->Read32();
                }
            }
            else
            {
                for (int i = 0; i < count; i++)
                {
                    Comp.u32[i] = _byteswap_ulong(((uint32_t*)ptr)[i]);
                }
            }

            for (int i = 0; i < count; i++)
            {
                comp[i] = *(float*)&Comp.u32[i];
            }
            break;

        default:
            Halt("FetchComp: Invalid combination of VAT settings\n");
            break;
    }
}

void FetchNorm(float* comp, int count, int type, int fmt, int shft, GX_FromFuture::FifoProcessor* fifo, ArrayId arrayId, bool nrmidx3)
{
    void* ptr1;
    void* ptr2;
    void* ptr3;

    void** ptrptr[3] = { &ptr1, &ptr2, &ptr3 };
    static int fmtsz[] = { 1, 1, 2, 2, 4 };

    union
    {
        uint8_t u8[9];
        uint16_t u16[9];
        int8_t s8[9];
        int16_t s16[9];
        uint32_t u32[9];
    } Comp;

    switch (type)
    {
        case VCD_NONE:      // Skip attribute
            return;
        case VCD_INDX8:
            ptr1 = GetArrayPtr(arrayId, fifo->Read8(), fmtsz[fmt]);
            if (count == 9 && nrmidx3)
            {
                ptr2 = GetArrayPtr(arrayId, fifo->Read8(), fmtsz[fmt]);
                ptr3 = GetArrayPtr(arrayId, fifo->Read8(), fmtsz[fmt]);
            }
            break;
        case VCD_INDX16:
            ptr1 = GetArrayPtr(arrayId, fifo->Read16(), fmtsz[fmt]);
            if (count == 9 && nrmidx3)
            {
                ptr2 = GetArrayPtr(arrayId, fifo->Read16(), fmtsz[fmt]);
                ptr3 = GetArrayPtr(arrayId, fifo->Read16(), fmtsz[fmt]);
            }
            break;
    }

    switch (fmt)
    {
        case VFMT_S8:
            if (type == VCD_DIRECT)
            {
                for (int i = 0; i < count; i++)
                {
                    Comp.s8[i] = fifo->Read8();
                }
            }
            else
            {
                for (int i = 0; i < count; i++)
                {
                    void* ptr;
                    if (count == 9 && nrmidx3)
                    {
                        ptr = *ptrptr[i / 3];
                    }
                    else
                    {
                        ptr = ptr1;
                    }
                    Comp.s8[i] = ((uint8_t*)ptr)[i];
                }
            }

            for (int i = 0; i < count; i++)
            {
                comp[i] = (float)(Comp.s8[i]) / pow(2.0, shft);
            }
            break;

        case VFMT_S16:
            if (type == VCD_DIRECT)
            {
                for (int i = 0; i < count; i++)
                {
                    Comp.s16[i] = fifo->Read16();
                }
            }
            else
            {
                for (int i = 0; i < count; i++)
                {
                    void* ptr;
                    if (count == 9 && nrmidx3)
                    {
                        ptr = *ptrptr[i / 3];
                    }
                    else
                    {
                        ptr = ptr1;
                    }
                    Comp.s16[i] = _byteswap_ushort(((uint16_t*)ptr)[i]);
                }
            }

            for (int i = 0; i < count; i++)
            {
                comp[i] = (float)(Comp.s16[i]) / pow(2.0, shft);
            }
            break;

        case VFMT_F32:
            if (type == VCD_DIRECT)
            {
                for (int i = 0; i < count; i++)
                {
                    Comp.u32[i] = fifo->Read32();
                }
            }
            else
            {
                for (int i = 0; i < count; i++)
                {
                    void* ptr;
                    if (count == 9 && nrmidx3)
                    {
                        ptr = *ptrptr[i / 3];
                    }
                    else
                    {
                        ptr = ptr1;
                    }
                    Comp.u32[i] = _byteswap_ulong(((uint32_t*)ptr)[i]);
                }
            }

            for (int i = 0; i < count; i++)
            {
                comp[i] = *(float*)&Comp.u32[i];
            }
            break;

        default:
            Halt("FetchComp: Invalid combination of VAT settings (normals)\n");
            break;
    }
}

Color FetchColor(int type, int fmt, GX_FromFuture::FifoProcessor* fifo, ArrayId arrayId)
{
    void* ptr;
    Color col;
    static int cfmtsz[] = { 2, 3, 4, 2, 4, 4 };

    col.R = 0;
    col.G = 0;
    col.B = 0;
    col.A = 255;

    uint16_t p16;
    uint32_t p32;

    uint8_t r, g, b, a;

    switch (type)
    {
        case VCD_NONE:      // Skip attribute
            return col;
        case VCD_INDX8:
            ptr = GetArrayPtr(arrayId, fifo->Read8(), cfmtsz[fmt]);
            break;
        case VCD_INDX16:
            ptr = GetArrayPtr(arrayId, fifo->Read16(), cfmtsz[fmt]);
            break;
        default:
            ptr = nullptr;
            break;
    }

    switch (fmt)
    {
        case VFMT_RGB565:

            if (type == VCD_DIRECT)
            {
                p16 = fifo->Read16();
            }
            else
            {
                p16 = _byteswap_ushort(((uint16_t*)ptr)[0]);
            }

            r = p16 >> 11;
            g = (p16 >> 5) & 0x3f;
            b = p16 & 0x1f;

            col.R = (r << 3) | (r >> 2);
            col.G = (g << 2) | (g >> 4);
            col.B = (b << 3) | (b >> 2);
            col.A = 255;

            break;

        case VFMT_RGB8:
            if (type == VCD_DIRECT)
            {
                col.R = fifo->Read8();
                col.G = fifo->Read8();
                col.B = fifo->Read8();
            }
            else
            {
                col.R = ((uint8_t*)ptr)[0];
                col.G = ((uint8_t*)ptr)[1];
                col.B = ((uint8_t*)ptr)[2];
            }
            col.A = 255;
            break;

        case VFMT_RGBX8:
            if (type == VCD_DIRECT)
            {
                col.R = fifo->Read8();
                col.G = fifo->Read8();
                col.B = fifo->Read8();
                fifo->Read8();
            }
            else
            {
                col.R = ((uint8_t*)ptr)[0];
                col.G = ((uint8_t*)ptr)[1];
                col.B = ((uint8_t*)ptr)[2];
            }
            col.A = 255;
            break;

        case VFMT_RGBA4:

            if (type == VCD_DIRECT)
            {
                p16 = fifo->Read16();
            }
            else
            {
                p16 = _byteswap_ushort(((uint16_t*)ptr)[0]);
            }

            r = (p16 >> 12) & 0xf;
            g = (p16 >> 8) & 0xf;
            b = (p16 >> 4) & 0xf;
            a = (p16 >> 0) & 0xf;

            col.R = (r << 4) | r;
            col.G = (g << 4) | g;
            col.B = (b << 4) | b;
            col.A = (a << 4) | a;

            break;

        case VFMT_RGBA6:

            if (type == VCD_DIRECT)
            {
                p32 = ((uint32_t)fifo->Read8() << 16) | ((uint32_t)fifo->Read8() << 8) | fifo->Read8();
            }
            else
            {
                p32 = ((uint32_t)((uint8_t*)ptr)[0] << 16) | ((uint32_t)((uint8_t*)ptr)[1] << 8) | ((uint8_t*)ptr)[2];
            }

            r = (p32 >> 18) & 0x3f;
            g = (p32 >> 12) & 0x3f;
            b = (p32 >> 6) & 0x3f;
            a = (p32 >> 0) & 0x3f;

            col.R = (r << 6) | r;
            col.G = (g << 6) | g;
            col.B = (b << 6) | b;
            col.A = (a << 6) | a;

            break;

        case VFMT_RGBA8:

            if (type == VCD_DIRECT)
            {
                col.R = fifo->Read8();
                col.G = fifo->Read8();
                col.B = fifo->Read8();
                col.A = fifo->Read8();
            }
            else
            {
                col.R = ((uint8_t*)ptr)[0];
                col.G = ((uint8_t*)ptr)[1];
                col.B = ((uint8_t*)ptr)[2];
                col.A = ((uint8_t*)ptr)[3];
            }

            break;

        default:
            Halt("FetchComp: Invalid combination of VAT settings (color)\n");
            break;
    }

    return col;
}

// collect vertex data
static void FifoWalk(unsigned vatnum, GX_FromFuture::FifoProcessor * fifo)
{
    // overrided by 'mtxidx' attributes
    xfRegs.posidx = xfRegs.matidxA.pos;
    xfRegs.texidx[0] = xfRegs.matidxA.tex0;
    xfRegs.texidx[1] = xfRegs.matidxA.tex1;
    xfRegs.texidx[2] = xfRegs.matidxA.tex2;
    xfRegs.texidx[3] = xfRegs.matidxA.tex3;
    xfRegs.texidx[4] = xfRegs.matidxB.tex4;
    xfRegs.texidx[5] = xfRegs.matidxB.tex5;
    xfRegs.texidx[6] = xfRegs.matidxB.tex6;
    xfRegs.texidx[7] = xfRegs.matidxB.tex7;

    // Matrix Index

    if (cpRegs.vcdLo.pmidx)
    {
        xfRegs.posidx = fifo->Read8();
    }

    if (cpRegs.vcdLo.t0midx)
    {
        xfRegs.texidx[0] = fifo->Read8();
    }

    if (cpRegs.vcdLo.t1midx)
    {
        xfRegs.texidx[1] = fifo->Read8();
    }

    if (cpRegs.vcdLo.t2midx)
    {
        xfRegs.texidx[2] = fifo->Read8();
    }

    if (cpRegs.vcdLo.t3midx)
    {
        xfRegs.texidx[3] = fifo->Read8();
    }

    if (cpRegs.vcdLo.t4midx)
    {
        xfRegs.texidx[4] = fifo->Read8();
    }

    if (cpRegs.vcdLo.t5midx)
    {
        xfRegs.texidx[5] = fifo->Read8();
    }

    if (cpRegs.vcdLo.t6midx)
    {
        xfRegs.texidx[6] = fifo->Read8();
    }

    if (cpRegs.vcdLo.t7midx)
    {
        xfRegs.texidx[7] = fifo->Read8();
    }

    // Position

    vtx->pos[0] = vtx->pos[1] = vtx->pos[2] = 1.0f;

    FetchComp(vtx->pos,
        cpRegs.vatA[vatnum].poscnt == VCNT_POS_XYZ ? 3 : 2,
        cpRegs.vcdLo.pos,
        cpRegs.vatA[vatnum].posfmt,
        cpRegs.vatA[vatnum].bytedeq ? cpRegs.vatA[vatnum].posshft : 0,
        fifo,
        ArrayId::Pos);

    // Normal

    vtx->nrm[0] = vtx->nrm[1] = vtx->nrm[2] = 1.0f;
    vtx->nrm[3] = vtx->nrm[4] = vtx->nrm[5] = 1.0f;
    vtx->nrm[6] = vtx->nrm[7] = vtx->nrm[8] = 1.0f;

    int nrmshft = 0;

    switch (cpRegs.vatA[vatnum].nrmfmt)
    {
        case VFMT_S8:
            nrmshft = 6;
            break;
        case VFMT_S16:
            nrmshft = 14;
            break;
    }

    FetchNorm(vtx->nrm,
        cpRegs.vatA[vatnum].nrmcnt == VCNT_NRM_NBT ? 9 : 3,
        cpRegs.vcdLo.nrm,
        cpRegs.vatA[vatnum].nrmfmt,
        nrmshft,
        fifo,
        ArrayId::Nrm,
        cpRegs.vatA[vatnum].nrmidx3 ? true : false);

    // Color0 

    vtx->col[0] = FetchColor(cpRegs.vcdLo.col0, cpRegs.vatA[vatnum].col0fmt, fifo, ArrayId::Color0);

    // Color1

    vtx->col[1] = FetchColor(cpRegs.vcdLo.col1, cpRegs.vatA[vatnum].col1fmt, fifo, ArrayId::Color1);

    // TexNCoord

    vtx->tcoord[0][0] = vtx->tcoord[0][1] = 1.0f;

    FetchComp(vtx->tcoord[0],
        cpRegs.vatA[vatnum].tex0cnt == VCNT_TEX_ST ? 2 : 1,
        cpRegs.vcdHi.tex0,
        cpRegs.vatA[vatnum].tex0fmt,
        cpRegs.vatA[vatnum].bytedeq ? cpRegs.vatA[vatnum].tex0shft : 0,
        fifo,
        ArrayId::Tex0Coord);

    vtx->tcoord[1][0] = vtx->tcoord[1][1] = 1.0f;

    FetchComp(vtx->tcoord[1],
        cpRegs.vatB[vatnum].tex1cnt == VCNT_TEX_ST ? 2 : 1,
        cpRegs.vcdHi.tex1,
        cpRegs.vatB[vatnum].tex1fmt,
        cpRegs.vatA[vatnum].bytedeq ? cpRegs.vatB[vatnum].tex1shft : 0,
        fifo,
        ArrayId::Tex1Coord);

    vtx->tcoord[2][0] = vtx->tcoord[2][1] = 1.0f;

    FetchComp(vtx->tcoord[2],
        cpRegs.vatB[vatnum].tex2cnt == VCNT_TEX_ST ? 2 : 1,
        cpRegs.vcdHi.tex2,
        cpRegs.vatB[vatnum].tex2fmt,
        cpRegs.vatA[vatnum].bytedeq ? cpRegs.vatB[vatnum].tex2shft : 0,
        fifo,
        ArrayId::Tex2Coord);

    vtx->tcoord[3][0] = vtx->tcoord[3][1] = 1.0f;

    FetchComp(vtx->tcoord[3],
        cpRegs.vatB[vatnum].tex3cnt == VCNT_TEX_ST ? 2 : 1,
        cpRegs.vcdHi.tex3,
        cpRegs.vatB[vatnum].tex3fmt,
        cpRegs.vatA[vatnum].bytedeq ? cpRegs.vatB[vatnum].tex3shft : 0,
        fifo,
        ArrayId::Tex3Coord);

    vtx->tcoord[4][0] = vtx->tcoord[4][1] = 1.0f;

    FetchComp(vtx->tcoord[4],
        cpRegs.vatB[vatnum].tex4cnt == VCNT_TEX_ST ? 2 : 1,
        cpRegs.vcdHi.tex4,
        cpRegs.vatB[vatnum].tex4fmt,
        cpRegs.vatA[vatnum].bytedeq ? cpRegs.vatC[vatnum].tex4shft : 0,
        fifo,
        ArrayId::Tex4Coord);

    vtx->tcoord[5][0] = vtx->tcoord[5][1] = 1.0f;

    FetchComp(vtx->tcoord[5],
        cpRegs.vatC[vatnum].tex5cnt == VCNT_TEX_ST ? 2 : 1,
        cpRegs.vcdHi.tex5,
        cpRegs.vatC[vatnum].tex5fmt,
        cpRegs.vatA[vatnum].bytedeq ? cpRegs.vatC[vatnum].tex5shft : 0,
        fifo,
        ArrayId::Tex5Coord);

    vtx->tcoord[6][0] = vtx->tcoord[6][1] = 1.0f;

    FetchComp(vtx->tcoord[6],
        cpRegs.vatC[vatnum].tex6cnt == VCNT_TEX_ST ? 2 : 1,
        cpRegs.vcdHi.tex6,
        cpRegs.vatC[vatnum].tex6fmt,
        cpRegs.vatA[vatnum].bytedeq ? cpRegs.vatC[vatnum].tex6shft : 0,
        fifo,
        ArrayId::Tex6Coord);

    vtx->tcoord[7][0] = vtx->tcoord[7][1] = 1.0f;

    FetchComp(vtx->tcoord[7],
        cpRegs.vatC[vatnum].tex7cnt == VCNT_TEX_ST ? 2 : 1,
        cpRegs.vcdHi.tex7,
        cpRegs.vatC[vatnum].tex7fmt,
        cpRegs.vatA[vatnum].bytedeq ? cpRegs.vatC[vatnum].tex7shft : 0,
        fifo,
        ArrayId::Tex7Coord);
}

static void GxBadFifo(uint8_t command)
{
    Halt(
        "Unimplemented command : 0x%02X\n"
        "VCD configuration :\n"
        "pmidx:%i\n"
        "t0idx:%i\t tex0:%i\n"
        "t1idx:%i\t tex1:%i\n"
        "t2idx:%i\t tex2:%i\n"
        "t3idx:%i\t tex3:%i\n"
        "t4idx:%i\t tex4:%i\n"
        "t5idx:%i\t tex5:%i\n"
        "t6idx:%i\t tex6:%i\n"
        "t7idx:%i\t tex7:%i\n"
        "pos:%i\n"
        "nrm:%i\n"
        "col0:%i\n"
        "col1:%i\n",
        command,
        cpRegs.vcdLo.pmidx,
        cpRegs.vcdLo.t0midx, cpRegs.vcdHi.tex0,
        cpRegs.vcdLo.t1midx, cpRegs.vcdHi.tex1,
        cpRegs.vcdLo.t2midx, cpRegs.vcdHi.tex2,
        cpRegs.vcdLo.t3midx, cpRegs.vcdHi.tex3,
        cpRegs.vcdLo.t4midx, cpRegs.vcdHi.tex4,
        cpRegs.vcdLo.t5midx, cpRegs.vcdHi.tex5,
        cpRegs.vcdLo.t6midx, cpRegs.vcdHi.tex6,
        cpRegs.vcdLo.t7midx, cpRegs.vcdHi.tex7,
        cpRegs.vcdLo.pos,
        cpRegs.vcdLo.nrm,
        cpRegs.vcdLo.col0,
        cpRegs.vcdLo.col1
    );
}

static void GxCommand(GX_FromFuture::FifoProcessor * fifo)
{
    if(frame_done)
    {
        GL_OpenSubsystem();
        GL_BeginFrame();
        frame_done = 0;
    }

    uint8_t cmd = fifo->Read8();

    //DBReport2(DbgChannel::GP, "GxCommand: 0x%02X\n", cmd);

    switch(cmd)
    {
        // do nothing
        case OP_CMD_NOP:
            break;

        case OP_CMD_INV | 0:
        case OP_CMD_INV | 1:
        case OP_CMD_INV | 2:
        case OP_CMD_INV | 3:
        case OP_CMD_INV | 4:
        case OP_CMD_INV | 5:
        case OP_CMD_INV | 6:
        case OP_CMD_INV | 7:
            //DBReport2(DbgChannel::GP, "Invalidate V$\n");
            break;

        case OP_CMD_CALL_DL | 0:
        case OP_CMD_CALL_DL | 1:
        case OP_CMD_CALL_DL | 2:
        case OP_CMD_CALL_DL | 3:
        case OP_CMD_CALL_DL | 4:
        case OP_CMD_CALL_DL | 5:
        case OP_CMD_CALL_DL | 6:
        case OP_CMD_CALL_DL | 7:
        {
            uint32_t physAddress = fifo->Read32() & RAMMASK;
            uint8_t* fifoPtr = &RAM[physAddress];
            size_t size = fifo->Read32() & ~0x1f;

            Report(Channel::GP, "OP_CMD_CALL_DL: addr: 0x%08X, size: %i\n", physAddress, size);

            GX_FromFuture::FifoProcessor* callDlFifo = new GX_FromFuture::FifoProcessor(fifoPtr, size);

            while (callDlFifo->EnoughToExecute())
            {
                GxCommand(callDlFifo);
            }

            delete callDlFifo;
            break;
        }

        // ---------------------------------------------------------------
        // loading of internal regs
            
        case OP_CMD_LOAD_BPREG | 0:
        case OP_CMD_LOAD_BPREG | 1:
        case OP_CMD_LOAD_BPREG | 2:
        case OP_CMD_LOAD_BPREG | 3:
        case OP_CMD_LOAD_BPREG | 4:
        case OP_CMD_LOAD_BPREG | 5:
        case OP_CMD_LOAD_BPREG | 6:
        case OP_CMD_LOAD_BPREG | 7:
        case OP_CMD_LOAD_BPREG | 8:
        case OP_CMD_LOAD_BPREG | 0xa:
        case OP_CMD_LOAD_BPREG | 0xb:
        case OP_CMD_LOAD_BPREG | 0xc:
        case OP_CMD_LOAD_BPREG | 0xd:
        case OP_CMD_LOAD_BPREG | 0xe:
        case OP_CMD_LOAD_BPREG | 0xf:
        {
            uint32_t word = fifo->Read32();
            loadBPReg(word >> 24, word & 0xffffff);
            break;
        }

        case OP_CMD_LOAD_CPREG | 0:
        case OP_CMD_LOAD_CPREG | 1:
        case OP_CMD_LOAD_CPREG | 2:
        case OP_CMD_LOAD_CPREG | 3:
        case OP_CMD_LOAD_CPREG | 4:
        case OP_CMD_LOAD_CPREG | 5:
        case OP_CMD_LOAD_CPREG | 6:
        case OP_CMD_LOAD_CPREG | 7:
        {
            uint8_t index = fifo->Read8();
            uint32_t word = fifo->Read32();
            loadCPReg(index, word);
            break;
        }

        case OP_CMD_LOAD_XFREG | 0:
        case OP_CMD_LOAD_XFREG | 1:
        case OP_CMD_LOAD_XFREG | 2:
        case OP_CMD_LOAD_XFREG | 3:
        case OP_CMD_LOAD_XFREG | 4:
        case OP_CMD_LOAD_XFREG | 5:
        case OP_CMD_LOAD_XFREG | 6:
        case OP_CMD_LOAD_XFREG | 7:
        {
            uint16_t len, index;

            len = fifo->Read16() + 1;
            index = fifo->Read16();

            loadXFRegs(index, len, fifo);
            break;
        }

        case OP_CMD_LOAD_INDXA | 0:
        case OP_CMD_LOAD_INDXA | 1:
        case OP_CMD_LOAD_INDXA | 2:
        case OP_CMD_LOAD_INDXA | 3:
        case OP_CMD_LOAD_INDXA | 4:
        case OP_CMD_LOAD_INDXA | 5:
        case OP_CMD_LOAD_INDXA | 6:
        case OP_CMD_LOAD_INDXA | 7:
        {
            uint16_t idx, start, len;
            idx = fifo->Read16();
            start = fifo->Read16();
            len = (start >> 12) + 1;
            start &= 0xfff;
            Report(Channel::GP, "OP_CMD_LOAD_INDXA: idx: %i, start: %i, len: %i\n", idx, start, len);
            break;
        }

        case OP_CMD_LOAD_INDXB | 0:
        case OP_CMD_LOAD_INDXB | 1:
        case OP_CMD_LOAD_INDXB | 2:
        case OP_CMD_LOAD_INDXB | 3:
        case OP_CMD_LOAD_INDXB | 4:
        case OP_CMD_LOAD_INDXB | 5:
        case OP_CMD_LOAD_INDXB | 6:
        case OP_CMD_LOAD_INDXB | 7:
        {
            uint16_t idx, start, len;
            idx = fifo->Read16();
            start = fifo->Read16();
            len = (start >> 12) + 1;
            start &= 0xfff;
            Report(Channel::GP, "OP_CMD_LOAD_INDXB: idx: %i, start: %i, len: %i\n", idx, start, len);
            break;
        }

        case OP_CMD_LOAD_INDXC | 0:
        case OP_CMD_LOAD_INDXC | 1:
        case OP_CMD_LOAD_INDXC | 2:
        case OP_CMD_LOAD_INDXC | 3:
        case OP_CMD_LOAD_INDXC | 4:
        case OP_CMD_LOAD_INDXC | 5:
        case OP_CMD_LOAD_INDXC | 6:
        case OP_CMD_LOAD_INDXC | 7:
        {
            uint16_t idx, start, len;
            idx = fifo->Read16();
            start = fifo->Read16();
            len = (start >> 12) + 1;
            start &= 0xfff;
            Report(Channel::GP, "OP_CMD_LOAD_INDXC: idx: %i, start: %i, len: %i\n", idx, start, len);
            break;
        }

        case OP_CMD_LOAD_INDXD | 0:
        case OP_CMD_LOAD_INDXD | 1:
        case OP_CMD_LOAD_INDXD | 2:
        case OP_CMD_LOAD_INDXD | 3:
        case OP_CMD_LOAD_INDXD | 4:
        case OP_CMD_LOAD_INDXD | 5:
        case OP_CMD_LOAD_INDXD | 6:
        case OP_CMD_LOAD_INDXD | 7:
        {
            uint16_t idx, start, len;
            idx = fifo->Read16();
            start = fifo->Read16();
            len = (start >> 12) + 1;
            start &= 0xfff;
            Report(Channel::GP, "OP_CMD_LOAD_INDXD: idx: %i, start: %i, len: %i\n", idx, start, len);
            break;
        }

        // ---------------------------------------------------------------
        // draw commands

        // 0x80
        case OP_CMD_DRAW_QUAD | 0:
        case OP_CMD_DRAW_QUAD | 1:
        case OP_CMD_DRAW_QUAD | 2:
        case OP_CMD_DRAW_QUAD | 3:
        case OP_CMD_DRAW_QUAD | 4:
        case OP_CMD_DRAW_QUAD | 5:
        case OP_CMD_DRAW_QUAD | 6:
        case OP_CMD_DRAW_QUAD | 7:
        {
            static   Vertex   quad[4];
            unsigned vatnum = cmd & 7;
            unsigned vtxnum = fifo->Read16();
            usevat = vatnum;
            Report(Channel::GP, "OP_CMD_DRAW_QUAD: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
                                                        /*/
                1---2       tri1: 0-1-2
                |  /|       tri2: 0-2-3
                | / |
                |/  |
                0---3
                                                        /*/

            while(vtxnum > 0)
            {
                for(unsigned n=0; n<4; n++)
                {
                    vtx = &quad[n];
                    FifoWalk(vatnum, fifo);
                }
                GL_RenderTriangle(&quad[0], &quad[1], &quad[2]);
                GL_RenderTriangle(&quad[0], &quad[2], &quad[3]);
                vtxnum -= 4;
            }
            break;
        }

        // 0x90
        case OP_CMD_DRAW_TRIANGLE | 0:
        case OP_CMD_DRAW_TRIANGLE | 1:
        case OP_CMD_DRAW_TRIANGLE | 2:
        case OP_CMD_DRAW_TRIANGLE | 3:
        case OP_CMD_DRAW_TRIANGLE | 4:
        case OP_CMD_DRAW_TRIANGLE | 5:
        case OP_CMD_DRAW_TRIANGLE | 6:
        case OP_CMD_DRAW_TRIANGLE | 7:
        {
            static   Vertex   tri[3];
            unsigned vatnum = cmd & 7;
            unsigned vtxnum = fifo->Read16();
            usevat = vatnum;
            Report(Channel::GP, "OP_CMD_DRAW_TRIANGLE: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
                                                        /*/
                1---2       tri: 0-1-2
                |  /
                | /
                |/
                0  
                                                        /*/

            while(vtxnum > 0)
            {
                for(unsigned n=0; n<3; n++)
                {
                    vtx = &tri[n];
                    FifoWalk(vatnum, fifo);
                }
                GL_RenderTriangle(&tri[0], &tri[1], &tri[2]);
                vtxnum -= 3;
            }
            break;
        }

        // 0x98 
        case OP_CMD_DRAW_STRIP | 0:
        case OP_CMD_DRAW_STRIP | 1:
        case OP_CMD_DRAW_STRIP | 2:
        case OP_CMD_DRAW_STRIP | 3:
        case OP_CMD_DRAW_STRIP | 4:
        case OP_CMD_DRAW_STRIP | 5:
        case OP_CMD_DRAW_STRIP | 6:
        case OP_CMD_DRAW_STRIP | 7:
        {
            static   Vertex   tri[3];
            unsigned c = 2, order[3] = { 0, 1, 2 }, tmp;
            unsigned vatnum = cmd & 7;
            unsigned vtxnum = fifo->Read16();
            usevat = vatnum;
            Report(Channel::GP, "OP_CMD_DRAW_STRIP: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
                                                        /*/
                    1---3---5   tri1: 0-1-2
                   /|  /|  /    tri2: 1-2-3
                  / | / | /     tri3: 2-3-4
                 /  |/  |/      tri4: 3-4-5
                0---2---4       ...
                                                        /*/
            if(vtxnum == 0) break;
            assert(vtxnum >= 3);

            vtx = &tri[0];
            FifoWalk(vatnum, fifo);
            vtx = &tri[1];
            FifoWalk(vatnum, fifo);
            vtxnum -= 2;

            while(vtxnum-- > 0)
            {
                vtx = &tri[c++];
                FifoWalk(vatnum, fifo);
                if(c > 2) c = 0;

                GL_RenderTriangle(
                    &tri[order[0]],
                    &tri[order[1]], 
                    &tri[order[2]]
                );

                tmp      = order[0];
                order[0] = order[1];
                order[1] = order[2];
                order[2] = tmp;
            }
            break;
        }

        // 0xA0
        case OP_CMD_DRAW_FAN | 0:
        case OP_CMD_DRAW_FAN | 1:
        case OP_CMD_DRAW_FAN | 2:
        case OP_CMD_DRAW_FAN | 3:
        case OP_CMD_DRAW_FAN | 4:
        case OP_CMD_DRAW_FAN | 5:
        case OP_CMD_DRAW_FAN | 6:
        case OP_CMD_DRAW_FAN | 7:
        {
            static   Vertex   tri[3];
            unsigned c = 2, order[2] = { 1, 2 }, tmp;
            unsigned vatnum = cmd & 7;
            unsigned vtxnum = fifo->Read16();
            usevat = vatnum;
            Report(Channel::GP, "OP_CMD_DRAW_FAN: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
                                                        /*/
                1---2---3   tri1: 0-1-2
                |  /  _/    tri2: 0-2-3
                | / _/      trin: 0-[n-1]-n
                |/_/    
                0/
                                                        /*/
            if(vtxnum == 0) break;
            assert(vtxnum >= 3);

            vtx = &tri[0];
            FifoWalk(vatnum, fifo);
            vtx = &tri[1];
            FifoWalk(vatnum, fifo);
            vtxnum -= 2;

            while(vtxnum-- > 0)
            {
                vtx = &tri[c];
                FifoWalk(vatnum, fifo);
                c = (c == 2) ? (c = 1) : (c = 2);

                GL_RenderTriangle(
                    &tri[0],
                    &tri[order[0]],
                    &tri[order[1]]
                );

                // order[0] <-> order[1]
                tmp      = order[0];
                order[0] = order[1];
                order[1] = tmp;
            }
            break;
        }

        // 0xA8
        case OP_CMD_DRAW_LINE | 0:
        case OP_CMD_DRAW_LINE | 1:
        case OP_CMD_DRAW_LINE | 2:
        case OP_CMD_DRAW_LINE | 3:
        case OP_CMD_DRAW_LINE | 4:
        case OP_CMD_DRAW_LINE | 5:
        case OP_CMD_DRAW_LINE | 6:
        case OP_CMD_DRAW_LINE | 7:
        {
            static   Vertex   v[2];
            unsigned vatnum = cmd & 7;
            unsigned vtxnum = fifo->Read16();
            usevat = vatnum;
            Report(Channel::GP, "OP_CMD_DRAW_LINE: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
                                                        /*/
                    1   3   5
                   /   /   / 
                  /   /   / 
                 /   /   /      
                0   2   4       
                                                        /*/
            if(vtxnum == 0) break;

            while(vtxnum > 0)
            {
                vtx = &v[0];
                FifoWalk(vatnum, fifo);
                vtx = &v[1];
                FifoWalk(vatnum, fifo);
                GL_RenderLine(&v[0], &v[1]);
                vtxnum -= 2;
            }
            break;
        }

        // 0xB0
        case OP_CMD_DRAW_LINESTRIP | 0:
        case OP_CMD_DRAW_LINESTRIP | 1:
        case OP_CMD_DRAW_LINESTRIP | 2:
        case OP_CMD_DRAW_LINESTRIP | 3:
        case OP_CMD_DRAW_LINESTRIP | 4:
        case OP_CMD_DRAW_LINESTRIP | 5:
        case OP_CMD_DRAW_LINESTRIP | 6:
        case OP_CMD_DRAW_LINESTRIP | 7:
        {
            static   Vertex   v[2];
            unsigned c = 1, order[2] = { 0, 1 }, tmp;
            unsigned vatnum = cmd & 7;
            unsigned vtxnum = fifo->Read16();
            usevat = vatnum;
            Report(Channel::GP, "OP_CMD_DRAW_LINESTRIP: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
                                                        /*/
                    1   3   5
                   /|  /|  / 
                  / | / | /  
                 /  |/  |/   
                0   2   4    
                                                        /*/
            if(vtxnum == 0) break;
            assert(vtxnum >= 2);

            vtx = &v[0];
            FifoWalk(vatnum, fifo);
            vtxnum--;

            while(vtxnum-- > 0)
            {
                vtx = &v[c++];
                FifoWalk(vatnum, fifo);
                if(c > 1) c = 0;

                GL_RenderLine(
                    &v[order[0]],
                    &v[order[1]]
                );

                tmp      = order[0];
                order[0] = order[1];
                order[1] = tmp;
            }
            break;
        }

        // 0xB8
        case OP_CMD_DRAW_POINT | 0:
        case OP_CMD_DRAW_POINT | 1:
        case OP_CMD_DRAW_POINT | 2:
        case OP_CMD_DRAW_POINT | 3:
        case OP_CMD_DRAW_POINT | 4:
        case OP_CMD_DRAW_POINT | 5:
        case OP_CMD_DRAW_POINT | 6:
        case OP_CMD_DRAW_POINT | 7:
        {
            static  Vertex  p;
            unsigned vatnum = cmd & 7;
            unsigned vtxnum = fifo->Read16();
            usevat = vatnum;
            Report(Channel::GP, "OP_CMD_DRAW_POINT: vtxnum: %i, vat: %i\n", vtxnum, vatnum);
                                                        /*/
                0---0       tri: 0-0-0 (1x1x1 tri)
                |  /
                | /
                |/
                0  
                                                        /*/

            while(vtxnum-- > 0)
            {
                vtx = &p;
                FifoWalk(vatnum, fifo);
                GL_RenderPoint(vtx);
            }
            break;
        }

        // ---------------------------------------------------------------
        // Unknown/unsupported fifo command
            
        default:
        {
            GxBadFifo(cmd);
            break;
        }
    }
}


void GXWriteFifo(uint8_t dataPtr[32])
{
    GxFifo.WriteBytes(dataPtr);

    while (GxFifo.EnoughToExecute())
    {
        GxCommand(&GxFifo);
    }
}
