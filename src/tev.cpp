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

out vec4 fragColor;

// Texture coordinates of the current fragment. They are plain locals: mutable globals are not
// handled reliably by every GLSL compiler.
vec2 g_tc[8];

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
float KonstComponent(uint sel, int component)
{
    if (sel < 8u)
    {
        float f[8] = float[8](255.0, 223.0, 191.0, 159.0, 128.0, 96.0, 64.0, 32.0);
        return f[int(sel)];
    }
    if (sel < 12u)
        return 0.0;
    if (sel < 16u) return tevKReg[int(sel) - 12][component];
    if (sel < 20u) return tevKReg[int(sel) - 16].r;
    if (sel < 24u) return tevKReg[int(sel) - 20].g;
    if (sel < 28u) return tevKReg[int(sel) - 24].b;
    return tevKReg[int(sel) - 28].a;
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

        // Texel of this stage
        vec4 tx = (te != 0) ? (SampleTexMap(ti, tc[tcindex] * texScale[ti]) * 255.0) : vec4(255.0);

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

        if (tevFogFsel == 2) fog = f;
        else if (tevFogFsel == 4) fog = 1.0 - exp2(-8.0 * f);
        else if (tevFogFsel == 5) fog = 1.0 - exp2(-8.0 * f * f);
        else if (tevFogFsel == 6) fog = exp2(-8.0 * (1.0 - f));
        else if (tevFogFsel == 7) fog = exp2(-8.0 * (1.0 - f) * (1.0 - f));

        result.rgb = mix(result.rgb, tevFogColor.rgb, fog);
    }

    // ---- alpha function (gfx-tev.md 3.8) ----

    float alpha255 = result.a * 255.0;
    bool p0 = TevAlphaCompare(tevAlphaOp0, alpha255, tevAlphaRef0);
    bool p1 = TevAlphaCompare(tevAlphaOp1, alpha255, tevAlphaRef1);
    bool pass;

    if (tevAlphaLogic == 0) pass = p0 && p1;
    else if (tevAlphaLogic == 1) pass = p0 || p1;
    else if (tevAlphaLogic == 2) pass = (p0 != p1);
    else pass = (p0 == p1);

    if (!pass)
        discard;

    // The whole TEV datapath works in units of 1/255 (the hardware stores 8-bit colours and 11-bit
    // signed colour registers), so the result is scaled down to the [0,1] range expected by GL.
    fragColor = result / 255.0;
}
)glsl";

	GLProgram* TextureEnvironmentUnit::GetTevProgram()
	{
		if (program != nullptr)
			return program;

		GLuint vertShader = gfx->xf->VertexShader();
		if (vertShader == 0)
			return nullptr;

		program = new GLProgram();

		if (!program->Link(vertShader, TEVFragmentShader, "TEV FRAGMENT"))
		{
			Report(Channel::GP, "TEV fragment shader failed to link\n");
			delete program;
			program = nullptr;
			return nullptr;
		}

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
				"tevAlphaLogic", "texScale[0]", "texMap0"
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

			kreg[i][0] = (float)((tev.regl[i].bits & 0x800000) ? (tev.regl[i].bits & 0xFF) : tev.regl[i].r);
			kreg[i][1] = (float)((tev.regh[i].bits & 0x800000) ? ((tev.regh[i].bits >> 12) & 0xFF) : tev.regh[i].g);
			kreg[i][2] = (float)((tev.regh[i].bits & 0x800000) ? (tev.regh[i].bits & 0xFF) : tev.regh[i].b);
			kreg[i][3] = (float)((tev.regl[i].bits & 0x800000) ? ((tev.regl[i].bits >> 12) & 0xFF) : tev.regl[i].a);
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

		// Per-map texture coordinate scales
		gfx->tx->UploadTexScales(p);
	}

	TextureEnvironmentUnit::TextureEnvironmentUnit(HWConfig* config, GFXCore* parent_gfx)
	{
		gfx = parent_gfx;

		// Reset state: alpha test passes unconditionally
		for (int i = 0; i < 16; i++)
		{
			tev.color_env[i].clamp = 1;
			tev.alpha_env[i].clamp = 1;
			tev.alpha_env[i].mode = 0;
		}

		for (int i = 0; i < 8; i++)
		{
			tev.ksel[i].kcsel0 = tev.ksel[i].kcsel1 = 0;
			tev.ksel[i].kasel0 = tev.ksel[i].kasel1 = 0;
		}

		tev.alpha_func.op0 = 7;
		tev.alpha_func.op1 = 7;
		tev.alpha_func.logic = 0;
	}

	TextureEnvironmentUnit::~TextureEnvironmentUnit()
	{
		DisposePrograms();
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

			case TEV_REGISTERL_0_ID: tev.regl[0].bits = value; break;
			case TEV_REGISTERH_0_ID: tev.regh[0].bits = value; break;
			case TEV_REGISTERL_1_ID: tev.regl[1].bits = value; break;
			case TEV_REGISTERH_1_ID: tev.regh[1].bits = value; break;
			case TEV_REGISTERL_2_ID: tev.regl[2].bits = value; break;
			case TEV_REGISTERH_2_ID: tev.regh[2].bits = value; break;
			case TEV_REGISTERL_3_ID: tev.regl[3].bits = value; break;
			case TEV_REGISTERH_3_ID: tev.regh[3].bits = value; break;
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
