// Texture Environment Unit (TEV)
//
// The TEV is the per-pixel colour/alpha combine stage of Flipper. It is emulated here by a single
// static fragment shader that walks up to 16 combine stages; the whole register state (stage count,
// operand selectors, texture bindings, constants) is passed as uniforms, so nothing has to be
// recompiled when the game reconfigures the TEV.
//
// The combine follows gfx-tev.md 3.2:
//
//     result = ( D  +/-  lerp(A, B, C)  + bias ) << shift , then clamped
//
// Colour arithmetic is carried in units of 1/255 (the hardware works with 8-bit colour values and
// 11-bit signed colour registers) and each stage result is quantised to an integer because the
// hardware stores it back into an 11-bit signed colour register.

#include "pch.h"

using namespace Debug;

namespace GFX
{
	static const char* TEVFragmentShader =
R"glsl(#version 330 core

// The varyings are matched to the vertex shader by name (explicit input locations
// in the fragment stage are not available in GLSL 330).
in vec2 v_TexCoord0;
in vec2 v_TexCoord1;
in vec2 v_TexCoord2;
in vec2 v_TexCoord3;
in vec2 v_TexCoord4;
in vec2 v_TexCoord5;
in vec2 v_TexCoord6;
in vec2 v_TexCoord7;
in vec4 v_Color0;
in vec4 v_Color1;

uniform sampler2D texMap0;
uniform sampler2D texMap1;
uniform sampler2D texMap2;
uniform sampler2D texMap3;
uniform sampler2D texMap4;
uniform sampler2D texMap5;
uniform sampler2D texMap6;
uniform sampler2D texMap7;
uniform vec2 texScale[8];

// TEV register state (raw register payloads, the register id byte is masked off)
uniform uvec4 tevColorEnv[16];      // 0xC0, 0xC2, ... 0xDE
uniform uvec4 tevAlphaEnv[16];      // 0xC1, 0xC3, ... 0xDF
uniform uvec4 tevTref[8];           // RAS1_TREF0..7 (texture bindings of two stages each)
uniform uvec2 tevKsel[8];           // .x = kcsel0 | kasel0 << 5, .y = kcsel1 | kasel1 << 5
uniform int   tevStages;            // number of active combine stages (from GEN_MODE.ntev)

uniform vec4  tevReg[4];            // colour registers (11-bit signed, in 1/255 units)
uniform vec4  tevKReg[4];           // K constants (Rev B, unsigned bytes)

uniform vec4  tevFogColor;
uniform float tevFogA;
uniform float tevFogC;
uniform float tevFogBMag;
uniform float tevFogBShf;
uniform int   tevFogProj;
uniform int   tevFogFsel;
uniform int   tevRangeAdjEnb;
uniform float tevRangeAdjCenter;
uniform float tevRangeAdjCoef[10];

uniform float tevAlphaRef0;
uniform float tevAlphaRef1;
uniform int   tevAlphaOp0;
uniform int   tevAlphaOp1;
uniform int   tevAlphaLogic;

// Z-texture environment (TEV_Z_ENV_0 / TEV_Z_ENV_1, gfx-tev.md 3.5, 4.9)
uniform int   tevZEnvOp;            // 0 = off, 1 = add the Z texel to the reference depth, 2 = replace
uniform int   tevZEnvType;          // 0 = u8 (texel alpha), 1 = u16, 2 = u24 (texel RGB)
uniform float tevZEnvOffset;        // zoff, in 1/16777215 units of the 24-bit depth

// Bump / indirect state (gfx-bump.md). `bumpCmd` holds the sixteen per-stage indirect commands
// packed four to a word; the three matrices are (ma,mb), (mc,md) and (me,mf) plus the scale bits.
uniform uvec4 bumpCmd[4];
uniform vec4  bumpMtxA[3];
uniform vec4  bumpMtxB[3];
uniform vec4  bumpMtxC[3];

// The real size of every texture map, in texels. The indirect arithmetic works on the S17.7
// coordinate the texture unit uses, i.e. texel units with seven fraction bits.
uniform vec2  texSize[8];

// The texture coordinate scale of the setup unit, one pair per texture coordinate (SU_SSIZE/SU_TSIZE).
// A component of zero means "no manual scale programmed", see CoordScale below.
uniform vec2  suScale[8];

// The coordinate shift scale of the four indirect stages (RAS1_SS0/SS1): 1, 1/2, 1/4 ... 1/256.
uniform vec2  indScale[4];

out vec4 fragColor;

// 1.0 in the S17.7 texture coordinate format (gfx-bump.md 5.1)
const float kCoordOne = 128.0;
// Bits [44:20] of the scaled dot product: the shift of the 25-bit coordinate window
const float kCoordShift = 1048576.0;		// 2^20

//! One component of an indirect command word.
uint BumpBits(uint stage, int off, int len)
{
    uint word = bumpCmd[stage >> 2u][stage & 3u];
    return (word >> uint(off)) & ((1u << uint(len)) - 1u);
}

//! The wrap mask of a bp_wrap value, as the AND counterpart (gfx-bump.md 3.4): mask + 1.
float BumpWrapSize(int wrap)
{
    if (wrap == 0) return 33554432.0;		// 2^25, no wrapping
    if (wrap == 1) return 32768.0;			// 256 texels
    if (wrap == 2) return 16384.0;			// 128
    if (wrap == 3) return 8192.0;			// 64
    if (wrap == 4) return 4096.0;			// 32
    if (wrap == 5) return 2048.0;			// 16
    return 0.0;								// bp_wrap_zero (and the undefined value 7)
}

//! A 25-bit signed value out of a float (two's complement, like the hardware window).
float BumpWindow25(float value)
{
    float m = mod(floor(value), 33554432.0);
    return (m >= 16777216.0) ? (m - 33554432.0) : m;
}

//! The coordinate window bits [15:5] of an S17.7 coordinate, used by the special matrix modes.
float BumpCoordWindow(float coord17)
{
    float m = mod(floor(coord17), 33554432.0);
    return floor(m / 32.0) - floor(m / 65536.0) * 2048.0;
}


// The Z texel of the last active stage (the one the Z environment is applied to)
vec4 g_lastTexel;

uint Bits(uint v, int off, int len)
{
    return (v >> uint(off)) & ((1u << uint(len)) - 1u);
}

vec4 SampleTexMap(int map, vec2 uv)
{
    if (map == 0) return texture(texMap0, uv);
    if (map == 1) return texture(texMap1, uv);
    if (map == 2) return texture(texMap2, uv);
    if (map == 3) return texture(texMap3, uv);
    if (map == 4) return texture(texMap4, uv);
    if (map == 5) return texture(texMap5, uv);
    if (map == 6) return texture(texMap6, uv);
    return texture(texMap7, uv);
}

// ------------------------------------------------------------------ the coordinate scale of the SU
//
// A texture coordinate is scaled after texgen and before the texture lookup: the SU_SSIZE/SU_TSIZE
// register of the coordinate pair holds the size of the texture minus one (gfx-su.md 4.5/4.6,
// GX_SetTexCoordScaleManually stores `size - 1` in the same field), so the coordinate the texture
// unit receives is `texcoord * (ssize + 1)` texels. The texcoord the emulator carries in a varying
// is normalised over the real size of the texture (the automatic scale), which is why the scale is
// a *ratio* here: it is composed with `texScale` (the padding correction between the real texture
// size and the power-of-two GL image) rather than replacing it.
//
// The hardware has no "manual scale" bit: the GX API writes the size of the texture bound to the
// coordinate whenever the texture order is set up. A register that was never written is uploaded as
// zero, which means "the automatic scale", i.e. the real size of the texture being sampled; the
// ratio is then exactly 1.0 and the coordinate is left untouched.
vec2 CoordScale(int coordIndex, int map)
{
    vec2 manual = suScale[coordIndex];
    vec2 size = max(texSize[map], vec2(1.0));

    return vec2(manual.x > 0.0 ? (manual.x / size.x) : 1.0,
                manual.y > 0.0 ? (manual.y / size.y) : 1.0);
}

float TevBlend(float a, float b, float c)
{
    return a + (c / 255.0) * (b - a);
}

float TevShift(float v, int shift)
{
    if (shift == 1) return v * 2.0;
    if (shift == 2) return v * 4.0;
    if (shift == 3) return v * 0.5;
    return v;
}

float TevBias(float v, int bias)
{
    if (bias == 1) return v + 128.0;
    if (bias == 2) return v - 128.0;
    return v;
}

float TevSat(float v, bool low)
{
    v = low ? clamp(v, 0.0, 255.0) : clamp(v, -1024.0, 1023.0);
    return floor(v + 0.5);
}

// Component of a K constant (gfx-tev.md 3.4). component: 0 = r, 1 = g, 2 = b, 3 = a.
//
// The 5-bit selector is decoded as documented: 0..7 are the fixed 1.0..1/8 fractions, 8..11 are
// black, 12..15 select a whole K register and 16..31 select one *channel* of K0..K3. The four
// groups of the single-channel range are the channels (16..19 red, 20..23 green, 24..27 blue,
// 28..31 alpha) and the low two bits of the selector pick the K register, so the channel is the
// high part of `sel - 16` and the register the low part. Because a single channel is what those
// selectors name, the value is the same for every `component`: the colour operand replicates it
// into r, g and b, and the alpha operand uses it as is.
float KonstComponent(uint sel, int component)
{
    if (sel < 8u)
    {
        float f[8] = float[8](255.0, 223.0, 191.0, 159.0, 128.0, 96.0, 64.0, 32.0);
        return f[int(sel)];
    }
    if (sel < 12u)
        return 0.0;
    if (sel < 16u)
        return tevKReg[int(sel) - 12][component];

    uint channel = (sel - 16u) >> 2;
    uint reg = (sel - 16u) & 3u;
    return tevKReg[int(reg)][int(channel)];
}

// Colour operand (vec3, 1/255 units). The D operand keeps the full 11-bit register width, all other
// operands only use the low 8 bits of a stored component (gfx-tev.md 3.2).
vec3 TevColorOperand(uint sel, bool isD, vec4 tx, vec4 rs, uint kcsel,
                     vec4 r0, vec4 r1, vec4 r2, vec4 r3)
{
    if (sel < 8u)
    {
        vec4 reg;
        int idx = int(sel) >> 1;
        if (idx == 0) reg = r0;
        else if (idx == 1) reg = r1;
        else if (idx == 2) reg = r2;
        else reg = r3;

        vec3 e = ((sel & 1u) != 0u) ? vec3(reg.a) : reg.rgb;
        return isD ? e : mod(e, vec3(256.0));
    }
    if (sel == 8u) return tx.rgb;
    if (sel == 9u) return vec3(tx.a);
    if (sel == 10u) return rs.rgb;
    if (sel == 11u) return vec3(rs.a);
    if (sel == 12u) return vec3(255.0);
    if (sel == 13u) return vec3(128.0);
    if (sel == 14u) return vec3(KonstComponent(kcsel, 0), KonstComponent(kcsel, 1), KonstComponent(kcsel, 2));
    return vec3(0.0);
}

float TevAlphaOperand(uint sel, bool isD, vec4 tx, vec4 rs, uint kasel,
                      vec4 r0, vec4 r1, vec4 r2, vec4 r3)
{
    if (sel < 4u)
    {
        vec4 reg;
        if (sel == 0u) reg = r0;
        else if (sel == 1u) reg = r1;
        else if (sel == 2u) reg = r2;
        else reg = r3;

        float e = reg.a;
        return isD ? e : mod(e, 256.0);
    }
    if (sel == 4u) return tx.a;
    if (sel == 5u) return rs.a;
    if (sel == 6u) return KonstComponent(kasel, 3);
    return 0.0;
}

bool TevAlphaCompare(int op, float a, float ref)
{
    if (op == 0) return false;          // never
    if (op == 1) return a < ref;        // less
    if (op == 2) return a == ref;       // equal
    if (op == 3) return a <= ref;       // less or equal
    if (op == 4) return a > ref;        // greater
    if (op == 5) return a != ref;       // not equal
    if (op == 6) return a >= ref;       // greater or equal
    return true;                        // always
}

// Decode the 24-bit Z texel out of the stage texel (gfx-tev.md 3.5, 4.9 `type`).
float ZTexel(vec4 texel, int type)
{
    if (type == 0)
        return texel.a;                             // u8 from the alpha byte, in 0..255
    if (type == 1)
        return texel.g * 256.0 + texel.b;           // u16
    return texel.r * 65536.0 + texel.g * 256.0 + texel.b;   // u24
}

// ------------------------------------------------------------------ bump / indirect texturing
//
// gfx-bump.md 3.3, per pixel and stage:
//
//     wrap   : sw = s & smsk, tw = t & tmsk
//     decode : s, t, u = the texel fields per fmt (bias adjusted)
//     dot    : ds = s*ma' + t*mc' + u*me'   dt = s*mb' + t*md' + u*mf'
//     scale  : ds, dt = (dot << scale)[44:20]
//     add    : s' = sw + ds + sf,  t' = tw + dt + tf
//
// The coordinates are S17.7 texel units, so 1.0 is 128. Everything is done in floats with the
// integer fields extracted explicitly, because the offsets are bit windows and not products.

//! The per-format texel fields (gfx-bump.md 5.2). `fmt`: 0 = 8 bit, 1 = 5 bit, 2 = 4 bit, 3 = 3 bit.
ivec3 BumpDecodeTexel(vec4 texel, int fmt)
{
    ivec3 c = ivec3(int(texel.r + 0.5), int(texel.g + 0.5), int(texel.b + 0.5));

    if (fmt == 0) return c;
    if (fmt == 1) return c >> 3;
    if (fmt == 2) return c >> 4;
    return c >> 5;
}

//! The per-component bias (gfx-bump.md 3.5): bit 0 = s, bit 1 = t, bit 2 = u.
ivec3 BumpBiasFields(ivec3 v, int fmt, int bias)
{
    if ((bias & 1) != 0) v.x = (fmt == 0) ? (v.x - 128) : (v.x + 1);
    if ((bias & 2) != 0) v.y = (fmt == 0) ? (v.y - 128) : (v.y + 1);
    if ((bias & 4) != 0) v.z = (fmt == 0) ? (v.z - 128) : (v.z + 1);
    return v;
}

//! A signed 11-bit matrix entry out of the raw register field.
float BumpMatrixEntry(uint raw)
{
    int v = int(raw & 0x7FFu);
    if ((v & 0x400) != 0) v -= 0x800;
    return float(v);
}

//! The 5-bit scale shift of one matrix: 2 + 2 bits from the first two registers, 1 from the third.
float BumpMatrixScale(int matrix)
{
    uint s0 = uint(bumpMtxA[matrix].z);
    uint s1 = uint(bumpMtxB[matrix].z);
    uint s2 = uint(bumpMtxC[matrix].z);
    return float(s0 | (s1 << 2) | ((s2 & 1u) << 4));
}

//! The offset the indirect stage adds to the coordinate, in S17.7 units.
vec2 BumpIndirectOffset(int stage, vec2 coord17)
{
    int fmt = int(BumpBits(uint(stage), 2, 2));
    int bias = int(BumpBits(uint(stage), 4, 3));
    int mode = int(BumpBits(uint(stage), 9, 4));
    int bt = int(BumpBits(uint(stage), 0, 2));

    if (mode == 0)
    {
        return vec2(0.0);			// bp_m_off: the datapath forces the offsets to zero
    }

    // The indirect texel: the *indirect* map (bt) is sampled at this stage's coordinate, which is
    // the S17.7 one converted back to the normalized coordinates the sampler wants.
    int map = clamp(bt, 0, 7);
    vec2 size = max(texSize[map], vec2(1.0));
    vec2 uv = coord17 / (size * kCoordOne);
    vec4 texel = SampleTexMap(map, uv * texScale[map]) * 255.0;

    ivec3 field = BumpBiasFields(BumpDecodeTexel(texel, fmt), fmt, bias);

    // The matrix selection (gfx-bump.md 3.7). The plain modes 1..3 use one of the three stored
    // matrices; the special groups replace two entries by the coordinate windows.
    int group = mode >> 2;				// 0 = stored, 1 = "A", 2 = "B"
    int scaleIndex = (mode & 3) - 1;	// 0..2 inside the group, 0 is the neutral constant

    vec2 ab = vec2(0.0), cd = vec2(0.0), ef = vec2(0.0);
    float scale = 0.0;

    if (group == 0)
    {
        int matrix = scaleIndex;
        ab = vec2(BumpMatrixEntry(uint(bumpMtxA[matrix].x)), BumpMatrixEntry(uint(bumpMtxA[matrix].y)));
        cd = vec2(BumpMatrixEntry(uint(bumpMtxB[matrix].x)), BumpMatrixEntry(uint(bumpMtxB[matrix].y)));
        ef = vec2(BumpMatrixEntry(uint(bumpMtxC[matrix].x)), BumpMatrixEntry(uint(bumpMtxC[matrix].y)));
        scale = BumpMatrixScale(matrix);
    }
    else
    {
        // The coordinate windows of the incoming coordinate: bits [15:5] of the S17.7 word
        float sw = BumpCoordWindow(coord17.x);
        float tw = BumpCoordWindow(coord17.y);

        if (group == 1)
        {
            ab = vec2(sw, tw);			// ma = s window, mb = t window
        }
        else
        {
            cd = vec2(sw, tw);			// mc = s window, md = t window
        }

        // The scale comes from the matrix the group names (scale 0/1/2 of that matrix)
        scale = (scaleIndex >= 0) ? BumpMatrixScale(scaleIndex) : 0.0;
    }

    float ds = float(field.x) * ab.x + float(field.y) * cd.x + float(field.z) * ef.x;
    float dt = float(field.x) * ab.y + float(field.y) * cd.y + float(field.z) * ef.y;

    // (dot << scale)[44:20]
    return vec2(BumpWindow25(floor(ds * exp2(scale) / kCoordShift)),
                BumpWindow25(floor(dt * exp2(scale) / kCoordShift)));
}


// Texture coordinates of the current fragment. They are plain locals: mutable globals are not
// handled reliably by every GLSL compiler.
vec2 g_tc[8];

void main()
{
    vec2 tc[8];
    tc[0] = v_TexCoord0;  tc[1] = v_TexCoord1;
    tc[2] = v_TexCoord2;  tc[3] = v_TexCoord3;
    tc[4] = v_TexCoord4;  tc[5] = v_TexCoord5;
    tc[6] = v_TexCoord6;  tc[7] = v_TexCoord7;

    // Colour register file. It is kept in plain local variables: dynamically indexed writes into a
    // writable array are not handled reliably by every GLSL compiler.
    vec4 r0 = tevReg[0];
    vec4 r1 = tevReg[1];
    vec4 r2 = tevReg[2];
    vec4 r3 = tevReg[3];

    // The feedback path of the bump unit: the offset of the previous indirect stage, in S17.7 units
    vec2 bumpFeedback = vec2(0.0);

    for (int stage = 0; stage < 16; stage++)
    {
        if (stage >= tevStages)
            break;

        uint ce = tevColorEnv[stage].x & 0xFFFFFFu;
        uint ae = tevAlphaEnv[stage].x & 0xFFFFFFu;

        int pair = (stage >> 1) & 7;
        bool odd = (stage & 1) != 0;

        uint treg = tevTref[pair].x;
        int ti = odd ? int(Bits(treg, 12, 3)) : int(Bits(treg, 0, 3));
        int tcindex = odd ? int(Bits(treg, 15, 3)) : int(Bits(treg, 3, 3));
        int te = odd ? int(Bits(treg, 18, 1)) : int(Bits(treg, 6, 1));
        int cc = odd ? int(Bits(treg, 19, 3)) : int(Bits(treg, 7, 3));

        // ---- indirect texture stage (gfx-bump.md 3.3) ----
        //
        // The stage's coordinate is wrapped, the offset the indirect texel produces is added to it
        // and the perturbed coordinate is what the stage's own texture is sampled with.
        if (BumpBits(uint(stage), 9, 4) != 0u)
        {
            int map = clamp(ti, 0, 7);
            vec2 size = max(texSize[map], vec2(1.0));
            vec2 coord17Unit = CoordScale(tcindex, map) * size * kCoordOne;
            vec2 coord17 = tc[tcindex] * coord17Unit;

            // The coordinate shift scale of this indirect stage (RAS1_SS0/SS1): the field the
            // indirect command names (`bt`) is the bump-stage id, whose coordinates the rasterizer
            // scales by 1/2^ras1_sts before the fetch (gfx-ras1.md 4.2). The scaled coordinate is the
            // one the offset is added to as well, because the offset is the perturbation of *that*
            // coordinate.
            vec2 iscale = indScale[clamp(int(BumpBits(uint(stage), 0, 2)), 0, 3)];
            if (iscale.x != 1.0)
                coord17.x = floor(coord17.x * iscale.x);
            if (iscale.y != 1.0)
                coord17.y = floor(coord17.y * iscale.y);

            // The coordinate is masked to its wrap window before the offset is added
            float ws = BumpWrapSize(int(BumpBits(uint(stage), 13, 3)));
            float wt = BumpWrapSize(int(BumpBits(uint(stage), 16, 3)));
            coord17 = vec2((ws > 0.0) ? mod(coord17.x, ws) : 0.0,
                           (wt > 0.0) ? mod(coord17.y, wt) : 0.0);

            vec2 offset = BumpIndirectOffset(stage, coord17);

            // bp_fb: add the offset of the previous indirect stage as well
            vec2 feedback = (BumpBits(uint(stage), 20, 1) != 0u) ? bumpFeedback : vec2(0.0);
            bumpFeedback = offset;

            vec2 result17 = vec2(BumpWindow25(coord17.x + offset.x + feedback.x),
                                 BumpWindow25(coord17.y + offset.y + feedback.y));

            tc[tcindex] = result17 / coord17Unit;
        }

        // Texel of this stage
        vec4 tx = (te != 0) ? (SampleTexMap(ti, tc[tcindex] * CoordScale(tcindex, ti) * texScale[ti]) * 255.0) : vec4(255.0);
        g_lastTexel = tx;

        // Texel component swap (alpha environment)
        uint swap = Bits(ae, 2, 2);
        if (swap == 1u) tx.rgb = vec3(tx.r);
        else if (swap == 2u) tx.rgb = vec3(tx.g);
        else if (swap == 3u) tx.rgb = vec3(tx.b);

        // Rasterized colour of this stage
        vec4 rs = vec4(0.0);
        if (cc == 0) rs = v_Color0 * 255.0;
        else if (cc == 1) rs = v_Color1 * 255.0;

        uint kpair = odd ? tevKsel[pair].y : tevKsel[pair].x;
        uint kcsel = kpair & 31u;
        uint kasel = (kpair >> 5) & 31u;

        // ---- colour combine ----

        vec3 cA = TevColorOperand(Bits(ce, 12, 4), false, tx, rs, kcsel, r0, r1, r2, r3);
        vec3 cB = TevColorOperand(Bits(ce, 8, 4), false, tx, rs, kcsel, r0, r1, r2, r3);
        vec3 cC = TevColorOperand(Bits(ce, 4, 4), false, tx, rs, kcsel, r0, r1, r2, r3);
        vec3 cD = TevColorOperand(Bits(ce, 0, 4), true, tx, rs, kcsel, r0, r1, r2, r3);

        bool csub = Bits(ce, 18, 1) != 0u;
        int cbias = int(Bits(ce, 16, 2));
        int cshift = int(Bits(ce, 20, 2));
        bool cclamp = Bits(ce, 19, 1) != 0u;

        vec3 cRes;
        cRes.r = TevSat(TevShift(TevBias(csub ? (cD.r - TevBlend(cA.r, cB.r, cC.r)) : (cD.r + TevBlend(cA.r, cB.r, cC.r)), cbias), cshift), cclamp);
        cRes.g = TevSat(TevShift(TevBias(csub ? (cD.g - TevBlend(cA.g, cB.g, cC.g)) : (cD.g + TevBlend(cA.g, cB.g, cC.g)), cbias), cshift), cclamp);
        cRes.b = TevSat(TevShift(TevBias(csub ? (cD.b - TevBlend(cA.b, cB.b, cC.b)) : (cD.b + TevBlend(cA.b, cB.b, cC.b)), cbias), cshift), cclamp);

        int cdest = int(Bits(ce, 22, 2));
        if (cdest == 0) r0.rgb = cRes;
        else if (cdest == 1) r1.rgb = cRes;
        else if (cdest == 2) r2.rgb = cRes;
        else r3.rgb = cRes;

        // ---- alpha combine ----

        float aA = TevAlphaOperand(Bits(ae, 13, 3), false, tx, rs, kasel, r0, r1, r2, r3);
        float aB = TevAlphaOperand(Bits(ae, 10, 3), false, tx, rs, kasel, r0, r1, r2, r3);
        float aC = TevAlphaOperand(Bits(ae, 7, 3), false, tx, rs, kasel, r0, r1, r2, r3);
        float aD = TevAlphaOperand(Bits(ae, 4, 3), true, tx, rs, kasel, r0, r1, r2, r3);

        bool asub = Bits(ae, 18, 1) != 0u;
        int abias = int(Bits(ae, 16, 2));
        int ashift = int(Bits(ae, 20, 2));
        bool aclamp = Bits(ae, 19, 1) != 0u;

        float rawA = TevShift(TevBias(asub ? (aD - TevBlend(aA, aB, aC)) : (aD + TevBlend(aA, aB, aC)), abias), ashift);

        uint amode = Bits(ae, 0, 2);
        float resA;

        if (amode == 1u) resA = (rawA >= 0.0) ? 255.0 : 0.0;
        else if (amode == 2u) resA = (rawA == 0.0) ? 255.0 : 0.0;
        else if (amode == 3u) resA = (rawA <= 0.0) ? 255.0 : 0.0;
        else resA = TevSat(rawA, aclamp);

        int adest = int(Bits(ae, 22, 2));
        if (adest == 0) r0.a = resA;
        else if (adest == 1) r1.a = resA;
        else if (adest == 2) r2.a = resA;
        else r3.a = resA;
    }

    vec4 result = r0;

    // ---- fog (gfx-tev.md 3.6) ----

    if (tevFogFsel != 0)
    {
        float z24 = gl_FragCoord.z * 16777215.0;
        float view_z;

        if (tevFogProj != 0)
        {
            // Orthographic projection: the depth is used directly
            view_z = z24 / 16777215.0;
        }
        else
        {
            // Perspective projection: remap the depth (b_mag - (z >> b_shf)) and take the reciprocal
            float b = tevFogBMag - floor(z24 / exp2(tevFogBShf));
            view_z = (b > 0.0) ? (1.0 / b) : 0.0;
        }

        float eye = tevFogA * view_z;

        // Range adjustment (gfx-tev.md 3.6.1)
        if (tevRangeAdjEnb != 0)
        {
            float t = clamp(abs(gl_FragCoord.x - tevRangeAdjCenter) / 256.0, 0.0, 8.999);
            int i0 = int(t);
            eye *= mix(tevRangeAdjCoef[i0], tevRangeAdjCoef[i0 + 1], t - float(i0));
        }

        float f = clamp(eye - tevFogC, 0.0, 1.0);
        float fog = 0.0;

        // The fog select of TEV_FOG_PARAM_3 (bits [23:21], gfx-tev.md 3.6 / 5.3) names one of the six
        // fog laws; the module description ("subtract C, clamp, backward/complement and square
        // selects, the 2^-x exponentiation") decodes the three bits as a family in [2:1] plus a
        // "square the value" select in bit 0:
        //
        //   fsel[2:1]  family          bit 0 = square the (complemented) value
        //   00         off             -
        //   01         linear          x^2
        //   10         1 - 2^(-8x)     fsel 5 = 1 - 2^(-8x^2)
        //   11         2^(-8(1-x))     fsel 7 = 2^(-8(1-x)^2)
        //
        // which is exactly the documented set (0 off, 2 linear, 4 exp, 5 exp^2, 6 backward exp,
        // 7 backward exp^2) and leaves the two encodings the GX API cannot produce (GX_FOG_ORTHO_*
        // maps to fsel 2/4/5/6/7 with the orthographic bit in bit 20): fsel 1 is the "off" family
        // with the square bit set, i.e. no fog, and fsel 3 is the linear law applied to the squared
        // value, which is the "linear-squared" curve.
        int family = (tevFogFsel >> 1) & 3;
        bool square = (tevFogFsel & 1) != 0;

        if (family == 0)
        {
            fog = 0.0;
        }
        else
        {
            float x = f;

            if (family == 3)
                x = 1.0 - x;			// the backward exponentials complement the value first
            if (square)
                x = x * x;

            if (family == 2) fog = 1.0 - exp2(-8.0 * x);
            else if (family == 3) fog = exp2(-8.0 * x);
            else fog = x;				// family 1: linear
        }

        result.rgb = mix(result.rgb, tevFogColor.rgb, fog);
    }

    // ---- alpha function (gfx-tev.md 3.8) ----
    //
    // The whole datapath is in 1/255 units, so the final alpha is already the 0..255 value the
    // reference arguments are compared against.

    float alphaValue = result.a;
    bool p0 = TevAlphaCompare(tevAlphaOp0, alphaValue, tevAlphaRef0);
    bool p1 = TevAlphaCompare(tevAlphaOp1, alphaValue, tevAlphaRef1);
    bool pass;

    if (tevAlphaLogic == 0) pass = p0 && p1;
    else if (tevAlphaLogic == 1) pass = p0 || p1;
    else if (tevAlphaLogic == 2) pass = (p0 != p1);
    else pass = (p0 == p1);

    if (!pass)
        discard;

    // ---- Z-texture environment (gfx-tev.md 3.5, 4.9; patent US6664958 FIG. 8) ----
    //
    // FIG. 8 of the patent: the first adder level receives either the reference depth z0 or the
    // constant 0 (the add/replace select), the second level adds the optional bias, and both are
    // 24-bit screen-space adders. The bias therefore applies to *both* operations:
    //
    //     add     : z = z0 + ztexel + bias
    //     replace : z =  0 + ztexel + bias

    if (tevZEnvOp != 0)
    {
        float base = (tevZEnvOp == 2) ? 0.0 : (gl_FragCoord.z * 16777215.0);
        float ztex = ZTexel(g_lastTexel, tevZEnvType);

        float z = base + ztex + tevZEnvOffset;
        z = clamp(z, 0.0, 16777215.0);

        gl_FragDepth = z / 16777215.0;
    }

    // The whole TEV datapath works in units of 1/255 (the hardware stores 8-bit colours and 11-bit
    // signed colour registers), so the result is scaled down to the [0,1] range expected by GL.
    fragColor = result / 255.0;
}
)glsl";

	const char* TextureEnvironmentUnit::FragmentShaderSource()
	{
		return TEVFragmentShader;
	}

	// The fragment shader of one colour-interpolation variant. For flat shading (GEN_MODE.flat_en)
	// the rasterized colour varyings carry the `flat` qualifier, which has to match the declaration in
	// the vertex shader (see TransformUnit::VertexShaderSource(bool)): the value then reaches the TEV
	// stages from the provoking vertex of the primitive instead of being interpolated across it.
	std::string TextureEnvironmentUnit::FragmentShaderSource(bool flat)
	{
		std::string src = TEVFragmentShader;

		if (flat)
		{
			const char* names[] = { "in vec4 v_Color0;", "in vec4 v_Color1;" };

			for (const char* name : names)
			{
				size_t pos = src.find(name);
				if (pos != std::string::npos)
				{
					src.replace(pos, strlen(name), std::string("flat ") + name);
				}
			}
		}

		return src;
	}

	GLProgram* TextureEnvironmentUnit::GetTevProgram()
	{
		bool flat = (gfx->genmode.flat_en != 0);

		// The flat-shading bit changes the *program*: the rasterized colour varyings have to be
		// declared flat on both sides of the link, so the cached program is thrown away and linked
		// again when GEN_MODE.flat_en flips.
		if (program != nullptr && programFlat != flat)
		{
			delete program;
			program = nullptr;
		}

		if (program != nullptr)
			return program;

		GLuint vertShader = gfx->xf->VertexShader(flat);
		if (vertShader == 0)
			return nullptr;

		std::string fragment = FragmentShaderSource(flat);

		program = new GLProgram();

		if (!program->Link(vertShader, fragment.c_str(), "TEV FRAGMENT"))
		{
			Report(Channel::GP, "TEV fragment shader failed to link\n");
			delete program;
			program = nullptr;
			return nullptr;
		}

		programFlat = flat;

		// Sampler bindings never change
		program->Use();
		char name[32];
		for (int i = 0; i < 8; i++)
		{
			sprintf(name, "texMap%d", i);
			glUniform1i(program->Uniform(name), i);
		}

		Report(Channel::GP, "TEV fragment shader compiled\n");

		// Report any uniform that the compiler did not keep (a missing uniform is silently ignored
		// by glUniform, which is a common source of "the shader ignores this register" bugs)
		{
			const char* names[] = {
				"tevStages", "tevColorEnv[0]", "tevAlphaEnv[0]", "tevTref[0]", "tevKsel[0]",
				"tevReg[0]", "tevKReg[0]", "tevFogColor", "tevFogA", "tevFogC", "tevFogBMag",
				"tevFogBShf", "tevFogProj", "tevFogFsel", "tevRangeAdjEnb", "tevRangeAdjCenter",
				"tevRangeAdjCoef[0]", "tevAlphaRef0", "tevAlphaRef1", "tevAlphaOp0", "tevAlphaOp1",
				"tevAlphaLogic", "tevZEnvOp", "tevZEnvType", "tevZEnvOffset", "texScale[0]", "texSize[0]",
				"suScale[0]", "indScale[0]",
				"bumpCmd[0]", "bumpMtxA[0]", "bumpMtxB[0]", "bumpMtxC[0]", "texMap0"
			};
			for (const char* n : names)
			{
				GLint loc = program->Uniform(n);
				if (loc < 0)
					Report(Channel::GP, "TEV: uniform %s was optimized out\n", n);
			}
		}

		return program;
	}

	void TextureEnvironmentUnit::DisposePrograms()
	{
		if (program != nullptr)
		{
			if (gfx != nullptr && gfx->backend_started)
				delete program;
			else
				program->prog = 0;		// no GL context anymore, only release the host memory
			program = nullptr;
		}

		programFlat = false;
	}

	void TextureEnvironmentUnit::UploadUniforms(GLProgram& p)
	{
		// Stage environments and texture bindings, in the raw register layout
		uint32_t colorEnv[16][4];
		uint32_t alphaEnv[16][4];

		for (int i = 0; i < 16; i++)
		{
			colorEnv[i][0] = tev.color_env[i].bits & 0xFFFFFF;
			colorEnv[i][1] = colorEnv[i][2] = colorEnv[i][3] = 0;
			alphaEnv[i][0] = tev.alpha_env[i].bits & 0xFFFFFF;
			alphaEnv[i][1] = alphaEnv[i][2] = alphaEnv[i][3] = 0;
		}

		glUniform4uiv(p.Uniform("tevColorEnv[0]"), 16, (GLuint*)colorEnv);
		glUniform4uiv(p.Uniform("tevAlphaEnv[0]"), 16, (GLuint*)alphaEnv);

		uint32_t tref[8][4];
		uint32_t ksel[8][2];

		for (int i = 0; i < 8; i++)
		{
			tref[i][0] = gfx->ras->tref[i].bits & 0xFFFFFF;
			tref[i][1] = tref[i][2] = tref[i][3] = 0;

			const TEV_KSel& k = tev.ksel[i];
			ksel[i][0] = (k.kcsel0 & 31) | ((k.kasel0 & 31) << 5);
			ksel[i][1] = (k.kcsel1 & 31) | ((k.kasel1 & 31) << 5);
		}

		glUniform4uiv(p.Uniform("tevTref[0]"), 8, (GLuint*)tref);
		glUniform2uiv(p.Uniform("tevKsel[0]"), 8, (GLuint*)ksel);

		int stages = (gfx->genmode.ntev & 0xF) + 1;
		glUniform1i(p.Uniform("tevStages"), stages > 16 ? 16 : stages);

		// Colour registers (11-bit signed, in 1/255 units) and K constants (Rev B, unsigned bytes).
		// Both live in the same registers on real hardware; the write path decides which form is meant
		// (gfx-tev.md 4.4), so both views are uploaded.
		float reg[4][4];
		float kreg[4][4];

		for (int i = 0; i < 4; i++)
		{
			auto sign11 = [](unsigned v) -> float {
				int value = (int)(v & 0x7FF);
				if (value & 0x400)
					value -= 0x800;
				return (float)value;
			};

			reg[i][0] = sign11(tev.regl[i].r);
			reg[i][1] = sign11(tev.regh[i].g);
			reg[i][2] = sign11(tev.regh[i].b);
			reg[i][3] = sign11(tev.regl[i].a);

			kreg[i][0] = (float)tev.kregl[i].r;
			kreg[i][1] = (float)tev.kregh[i].g;
			kreg[i][2] = (float)tev.kregh[i].b;
			kreg[i][3] = (float)tev.kregl[i].a;
		}

		glUniform4fv(p.Uniform("tevReg[0]"), 4, (float*)reg);
		glUniform4fv(p.Uniform("tevKReg[0]"), 4, (float*)kreg);

		// The fog colour is used inside the combine datapath, so it is in the same 1/255 units
		glUniform4f(p.Uniform("tevFogColor"),
			(float)tev.fog_color.r,
			(float)tev.fog_color.g,
			(float)tev.fog_color.b, 255.0f);

		// Fog parameters A and C are stored as 20-bit s11e8 floats
		auto s11e8 = [](unsigned sign, unsigned expn, unsigned mant) -> float {
			float m = 1.0f + (float)mant / 2048.0f;
			float v = (float)ldexp((double)m, (int)expn - 127);
			return sign ? -v : v;
		};

		glUniform1f(p.Uniform("tevFogA"),
			s11e8(tev.fog_param0.a_sign, tev.fog_param0.a_expn, tev.fog_param0.a_mant));
		glUniform1f(p.Uniform("tevFogC"),
			s11e8(tev.fog_param3.c_sign, tev.fog_param3.c_expn, tev.fog_param3.c_mant));
		glUniform1f(p.Uniform("tevFogBMag"), (float)tev.fog_param1.b_mag);
		glUniform1f(p.Uniform("tevFogBShf"), (float)tev.fog_param2.b_shft);
		glUniform1i(p.Uniform("tevFogProj"), (GLint)tev.fog_param3.proj);
		glUniform1i(p.Uniform("tevFogFsel"), (GLint)tev.fog_param3.fsel);

		glUniform1i(p.Uniform("tevRangeAdjEnb"), (GLint)tev.rangeadj_control.enb);
		glUniform1f(p.Uniform("tevRangeAdjCenter"), (float)tev.rangeadj_control.center);

		float coef[10];
		for (int i = 0; i < 5; i++)
		{
			coef[i * 2 + 0] = (float)tev.range_adj[i].r0 / 256.0f;
			coef[i * 2 + 1] = (float)tev.range_adj[i].r1 / 256.0f;
		}
		glUniform1fv(p.Uniform("tevRangeAdjCoef[0]"), 10, coef);

		// Alpha function
		glUniform1f(p.Uniform("tevAlphaRef0"), (float)tev.alpha_func.a0);
		glUniform1f(p.Uniform("tevAlphaRef1"), (float)tev.alpha_func.a1);
		glUniform1i(p.Uniform("tevAlphaOp0"), (GLint)tev.alpha_func.op0);
		glUniform1i(p.Uniform("tevAlphaOp1"), (GLint)tev.alpha_func.op1);
		glUniform1i(p.Uniform("tevAlphaLogic"), (GLint)tev.alpha_func.logic);

		// Z-texture environment
		glUniform1i(p.Uniform("tevZEnvOp"), (GLint)tev.zenv1.op);
		glUniform1i(p.Uniform("tevZEnvType"), (GLint)tev.zenv1.type);
		glUniform1f(p.Uniform("tevZEnvOffset"), (float)tev.zenv0.zoff);

		// Bump / indirect state
		{
			const BUMPState& bump = gfx->bump->State();

			uint32_t cmd[4][4];
			for (int i = 0; i < 16; i++)
			{
				cmd[i >> 2][i & 3] = bump.cmd[i].bits & 0x1FFFFF;
			}
			glUniform4uiv(p.Uniform("bumpCmd[0]"), 4, (GLuint*)cmd);

			float mtxA[3][4], mtxB[3][4], mtxC[3][4];
			for (int i = 0; i < 3; i++)
			{
				mtxA[i][0] = (float)bump.matrix[i].a.ma;
				mtxA[i][1] = (float)bump.matrix[i].a.mb;
				mtxA[i][2] = (float)bump.matrix[i].a.s;
				mtxA[i][3] = 0.0f;

				mtxB[i][0] = (float)bump.matrix[i].b.mc;
				mtxB[i][1] = (float)bump.matrix[i].b.md;
				mtxB[i][2] = (float)bump.matrix[i].b.s;
				mtxB[i][3] = 0.0f;

				mtxC[i][0] = (float)bump.matrix[i].c.me;
				mtxC[i][1] = (float)bump.matrix[i].c.mf;
				mtxC[i][2] = (float)bump.matrix[i].c.s;
				mtxC[i][3] = 0.0f;
			}

			glUniform4fv(p.Uniform("bumpMtxA[0]"), 3, (float*)mtxA);
			glUniform4fv(p.Uniform("bumpMtxB[0]"), 3, (float*)mtxB);
			glUniform4fv(p.Uniform("bumpMtxC[0]"), 3, (float*)mtxC);
		}

		// Per-map texture coordinate scales
		gfx->tx->UploadTexScales(p);

		// The coordinate shift scales of the four indirect stages (RAS1_SS0/SS1, gfx-ras1.md 4.2)
		{
			float indScale[4][2];

			for (int i = 0; i < 4; i++)
			{
				indScale[i][0] = gfx->ras->IndirectScale(i, false);
				indScale[i][1] = gfx->ras->IndirectScale(i, true);
			}

			glUniform2fv(p.Uniform("indScale[0]"), 4, (float*)indScale);
		}
	}

	TextureEnvironmentUnit::TextureEnvironmentUnit(HWConfig* config, GFXCore* parent_gfx)
	{
		gfx = parent_gfx;
		Reset();
	}

	TextureEnvironmentUnit::~TextureEnvironmentUnit()
	{
		DisposePrograms();
	}

	// The register values the hardware comes up with (gfx-tev.md 4.2, 4.8): the combines are clamped,
	// the alpha function passes everything and the Rev-B constants are off.
	void TextureEnvironmentUnit::Reset()
	{
		tev = TEVState{};

		for (int i = 0; i < 16; i++)
		{
			tev.color_env[i].clamp = 1;
			tev.alpha_env[i].clamp = 1;
			tev.alpha_env[i].mode = 0;
		}

		tev.alpha_func.op0 = 7;			// always
		tev.alpha_func.op1 = 7;			// always
		tev.alpha_func.logic = 0;		// and
	}

	// A write to TEV_REGISTERL/H belongs to the Rev B 8-bit K form when payload bit 23 is set and
	// bit 11 is clear (gfx-tev.md 4.4); otherwise it is the original 11-bit colour register. The
	// K form keeps its components in different places than the colour form (r/b in [7:0], a/g in
	// [19:12]), so each form is unpacked into its own storage.
	static bool TEVIsKonstForm(uint32_t value)
	{
		return ((value & 0x800000) != 0) && ((value & 0x800) == 0);
	}

	static void TEVLoadRegisterL(TEV_RegisterL* colour, TEV_KonstRegisterL* konst, uint32_t value)
	{
		if (TEVIsKonstForm(value))
		{
			konst->r = value & 0xFF;
			konst->a = (value >> 12) & 0xFF;
		}
		else colour->bits = value;
	}

	static void TEVLoadRegisterH(TEV_RegisterH* colour, TEV_KonstRegisterH* konst, uint32_t value)
	{
		if (TEVIsKonstForm(value))
		{
			konst->b = value & 0xFF;
			konst->g = (value >> 12) & 0xFF;
		}
		else colour->bits = value;
	}

	void TextureEnvironmentUnit::loadTEVReg(size_t index, uint32_t value)
	{
		switch (index)
		{
			case TEV_COLOR_ENV_0_ID: tev.color_env[0].bits = value; break;
			case TEV_ALPHA_ENV_0_ID: tev.alpha_env[0].bits = value; break;
			case TEV_COLOR_ENV_1_ID: tev.color_env[1].bits = value; break;
			case TEV_ALPHA_ENV_1_ID: tev.alpha_env[1].bits = value; break;
			case TEV_COLOR_ENV_2_ID: tev.color_env[2].bits = value; break;
			case TEV_ALPHA_ENV_2_ID: tev.alpha_env[2].bits = value; break;
			case TEV_COLOR_ENV_3_ID: tev.color_env[3].bits = value; break;
			case TEV_ALPHA_ENV_3_ID: tev.alpha_env[3].bits = value; break;
			case TEV_COLOR_ENV_4_ID: tev.color_env[4].bits = value; break;
			case TEV_ALPHA_ENV_4_ID: tev.alpha_env[4].bits = value; break;
			case TEV_COLOR_ENV_5_ID: tev.color_env[5].bits = value; break;
			case TEV_ALPHA_ENV_5_ID: tev.alpha_env[5].bits = value; break;
			case TEV_COLOR_ENV_6_ID: tev.color_env[6].bits = value; break;
			case TEV_ALPHA_ENV_6_ID: tev.alpha_env[6].bits = value; break;
			case TEV_COLOR_ENV_7_ID: tev.color_env[7].bits = value; break;
			case TEV_ALPHA_ENV_7_ID: tev.alpha_env[7].bits = value; break;
			case TEV_COLOR_ENV_8_ID: tev.color_env[8].bits = value; break;
			case TEV_ALPHA_ENV_8_ID: tev.alpha_env[8].bits = value; break;
			case TEV_COLOR_ENV_9_ID: tev.color_env[9].bits = value; break;
			case TEV_ALPHA_ENV_9_ID: tev.alpha_env[9].bits = value; break;
			case TEV_COLOR_ENV_A_ID: tev.color_env[0xa].bits = value; break;
			case TEV_ALPHA_ENV_A_ID: tev.alpha_env[0xa].bits = value; break;
			case TEV_COLOR_ENV_B_ID: tev.color_env[0xb].bits = value; break;
			case TEV_ALPHA_ENV_B_ID: tev.alpha_env[0xb].bits = value; break;
			case TEV_COLOR_ENV_C_ID: tev.color_env[0xc].bits = value; break;
			case TEV_ALPHA_ENV_C_ID: tev.alpha_env[0xc].bits = value; break;
			case TEV_COLOR_ENV_D_ID: tev.color_env[0xd].bits = value; break;
			case TEV_ALPHA_ENV_D_ID: tev.alpha_env[0xd].bits = value; break;
			case TEV_COLOR_ENV_E_ID: tev.color_env[0xe].bits = value; break;
			case TEV_ALPHA_ENV_E_ID: tev.alpha_env[0xe].bits = value; break;
			case TEV_COLOR_ENV_F_ID: tev.color_env[0xf].bits = value; break;
			case TEV_ALPHA_ENV_F_ID: tev.alpha_env[0xf].bits = value; break;

			// The colour registers and the Rev B K constants share the register ids; the RTL routes
			// the write by the tag bit of the payload: bit 23 set with bit 11 clear means the 8-bit
			// K form, everything else is the original 11-bit colour form (gfx-tev.md 4.4). The two
			// forms are separate storages, so a K write does not disturb the colour registers.
			case TEV_REGISTERL_0_ID: TEVLoadRegisterL(&tev.regl[0], &tev.kregl[0], value); break;
			case TEV_REGISTERH_0_ID: TEVLoadRegisterH(&tev.regh[0], &tev.kregh[0], value); break;
			case TEV_REGISTERL_1_ID: TEVLoadRegisterL(&tev.regl[1], &tev.kregl[1], value); break;
			case TEV_REGISTERH_1_ID: TEVLoadRegisterH(&tev.regh[1], &tev.kregh[1], value); break;
			case TEV_REGISTERL_2_ID: TEVLoadRegisterL(&tev.regl[2], &tev.kregl[2], value); break;
			case TEV_REGISTERH_2_ID: TEVLoadRegisterH(&tev.regh[2], &tev.kregh[2], value); break;
			case TEV_REGISTERL_3_ID: TEVLoadRegisterL(&tev.regl[3], &tev.kregl[3], value); break;
			case TEV_REGISTERH_3_ID: TEVLoadRegisterH(&tev.regh[3], &tev.kregh[3], value); break;
			case TEV_RANGE_ADJ_C_ID: tev.rangeadj_control.bits = value; break;
			case TEV_RANGE_ADJ_0_ID: tev.range_adj[0].bits = value; break;
			case TEV_RANGE_ADJ_1_ID: tev.range_adj[1].bits = value; break;
			case TEV_RANGE_ADJ_2_ID: tev.range_adj[2].bits = value; break;
			case TEV_RANGE_ADJ_3_ID: tev.range_adj[3].bits = value; break;
			case TEV_RANGE_ADJ_4_ID: tev.range_adj[4].bits = value; break;
			case TEV_FOG_PARAM_0_ID: tev.fog_param0.bits = value; break;
			case TEV_FOG_PARAM_1_ID: tev.fog_param1.bits = value; break;
			case TEV_FOG_PARAM_2_ID: tev.fog_param2.bits = value; break;
			case TEV_FOG_PARAM_3_ID: tev.fog_param3.bits = value; break;
			case TEV_FOG_COLOR_ID: tev.fog_color.bits = value; break;
			case TEV_ALPHAFUNC_ID: tev.alpha_func.bits = value; break;
			case TEV_Z_ENV_0_ID: tev.zenv0.bits = value; break;
			case TEV_Z_ENV_1_ID: tev.zenv1.bits = value; break;
			case TEV_KSEL_0_ID: tev.ksel[0].bits = value; break;
			case TEV_KSEL_1_ID: tev.ksel[1].bits = value; break;
			case TEV_KSEL_2_ID: tev.ksel[2].bits = value; break;
			case TEV_KSEL_3_ID: tev.ksel[3].bits = value; break;
			case TEV_KSEL_4_ID: tev.ksel[4].bits = value; break;
			case TEV_KSEL_5_ID: tev.ksel[5].bits = value; break;
			case TEV_KSEL_6_ID: tev.ksel[6].bits = value; break;
			case TEV_KSEL_7_ID: tev.ksel[7].bits = value; break;

			default:
			{
				Debug::Report(Debug::Channel::GP, "Unknown reg load, index: 0x%02X\n", index);
				break;
			}
		}
	}
}
