// Transform Unit
#include "pch.h"

using namespace Debug;

namespace GFX
{

	// -------------------------------------------------------------------------------------------
	// The XF vertex shader
	//
	// The XF is the geometry part of the Flipper GFX pipeline: it transforms the vertices that come
	// from the CP (geometry and texture matrix multiplies, the projection combine), lights them and
	// generates the texture coordinates. In this emulator all of that is done by the vertex shader
	// below; the register state is passed as uniforms (see UploadUniforms), so the shader never has
	// to be recompiled.
	//
	// See specs: gfx-xf.md (registers 0x0000-0x1057).

	static const char* XFVertexShader =
R"glsl(#version 330 core

layout(location = 0)  in vec3 in_Position;
layout(location = 1)  in vec3 in_Normal;
layout(location = 2)  in vec3 in_Binormal;
layout(location = 3)  in vec3 in_Tangent;
layout(location = 4)  in vec4 in_Color0;
layout(location = 5)  in vec4 in_Color1;
layout(location = 6)  in vec2 in_TexCoord0;
layout(location = 7)  in vec2 in_TexCoord1;
layout(location = 8)  in vec2 in_TexCoord2;
layout(location = 9)  in vec2 in_TexCoord3;
layout(location = 10) in vec2 in_TexCoord4;
layout(location = 11) in vec2 in_TexCoord5;
layout(location = 12) in vec2 in_TexCoord6;
layout(location = 13) in vec2 in_TexCoord7;
layout(location = 14) in uint in_MatIdx0;
layout(location = 15) in uint in_MatIdx1;

// Vertex shader stage outputs. Explicit output locations are not available in GLSL 330,
// so the varyings are matched to the fragment shader by name.
out vec2 v_TexCoord0;
out vec2 v_TexCoord1;
out vec2 v_TexCoord2;
out vec2 v_TexCoord3;
out vec2 v_TexCoord4;
out vec2 v_TexCoord5;
out vec2 v_TexCoord6;
out vec2 v_TexCoord7;
out vec4 v_Color0;
out vec4 v_Color1;

#define MAX_LIGHTS 8

// ------------------------------------------------------------------ XF register state

uniform float matrixMem[256];           // 0x0000-0x00FF: 64 rows x 4 words (geometry / texture matrices)
uniform float nrmMatrixMem[96];         // 0x0400-0x045F: 32 rows x 3 words (normal matrices)
uniform float dualTexMatrixMem[256];    // 0x0500-0x05FF: 64 rows x 4 words (dual texture matrices)

uniform vec4 lightRgba[MAX_LIGHTS];     // 0x0603 + n*0x10 : light colour (normalized RGBA)
uniform vec4 lightA[MAX_LIGHTS];        // .xyz = cosine attenuation a0,a1,a2
uniform vec4 lightK[MAX_LIGHTS];        // .xyz = distance attenuation k0,k1,k2
uniform vec4 lightLpx[MAX_LIGHTS];      // .xyz = light position (or infinite light direction)
uniform vec4 lightDhx[MAX_LIGHTS];      // .xyz = light direction (normalized)

uniform vec4 xfAmbient[2];              // 0x100A,0x100B
uniform vec4 xfMaterial[2];             // 0x100C,0x100D
uniform vec4 colorCtl[2];               // 0x100E,0x100F: x=material source, y=lightfunc, z=ambient source, w=diffuse attenuation
uniform vec4 colorAtten[2];             // 0x100E,0x100F: x=attenuation enable, y=attenuation select
uniform int  colorLightMask[2];         // 0x100E,0x100F: bit n = light n is used for colour
uniform vec4 alphaCtl[2];               // 0x1010,0x1011
uniform vec4 alphaAtten[2];
uniform int  alphaLightMask[2];

uniform int   xfNumColors;              // 0x1009
uniform int   xfNumTex;                 // 0x103F
uniform float xfProjParam[6];           // 0x1020-0x1025
uniform int   xfProjOrtho;              // 0x1026
uniform int   xfDualTexTran;            // 0x1012
uniform uvec4 xfTexGen[8];              // 0x1040-0x1047 (raw TexGenParam bits)
uniform uint  xfDualGen[8];             // 0x1050-0x1057 (raw DualGenParam bits)

// ------------------------------------------------------------------ helpers

vec4 MatrixRow(int base, int row)
{
    int o = base + row * 4;
    return vec4(matrixMem[o], matrixMem[o + 1], matrixMem[o + 2], matrixMem[o + 3]);
}

vec3 NormalMatrixRow(int base, int row)
{
    int o = base + row * 3;
    return vec3(nrmMatrixMem[o], nrmMatrixMem[o + 1], nrmMatrixMem[o + 2]);
}

// Cosine attenuation fraction for one light (gfx-xf.md 3.3).
// spec: cos = N.H (specular) or L.Ldir (spotlight), shaped by a0 + a1*cos + a2*cos^2.
float CosineAttenuation(int ch, vec3 n, vec3 ldir, bool isAlpha, int i)
{
    vec4 att = isAlpha ? alphaAtten[ch] : colorAtten[ch];
    if (att.x < 0.5)
        return 1.0;

    float cosAtten;
    if (att.y < 0.5)
        cosAtten = clamp(dot(n, lightDhx[i].xyz), 0.0, 1.0);
    else
        cosAtten = clamp(dot(ldir, -lightDhx[i].xyz), 0.0, 1.0);

    return clamp(lightA[i].x + lightA[i].y * cosAtten + lightA[i].z * cosAtten * cosAtten, 0.0, 1.0);
}

float DistanceAttenuation(int ch, float dist, bool isAlpha, int i)
{
    float d = lightK[i].x + lightK[i].y * dist + lightK[i].z * dist * dist;
    return clamp(1.0 / max(d, 0.00001), 0.0, 1.0);
}

vec3 IlluminateColor(int ch, vec3 vpos, vec3 n, vec3 hostColor)
{
    vec3 amb = (colorCtl[ch].z < 0.5) ? xfAmbient[ch].rgb : hostColor;
    vec3 illum = vec3(0.0);
    int mask = colorLightMask[ch];

    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (((mask >> i) & 1) == 0)
            continue;

        vec3 v = lightLpx[i].xyz - vpos;
        float dist = length(v);
        vec3 ldir = (dist > 0.00001) ? v / dist : vec3(0.0, 0.0, 1.0);

        float diff = 1.0;
        if (colorCtl[ch].w > 0.5)
        {
            float dp = dot(n, ldir);
            if (colorCtl[ch].w > 1.5)
                dp = clamp(dp, 0.0, 1.0);
            diff = dp;
        }

        float attn = CosineAttenuation(ch, n, ldir, false, i);
        if (colorAtten[ch].x > 0.5)
            attn *= DistanceAttenuation(ch, dist, false, i);

        illum += lightRgba[i].rgb * (diff * attn);
    }

    return clamp(clamp(illum, -1.0, 1.0) + amb, 0.0, 1.0);
}

float IlluminateAlpha(int ch, vec3 vpos, vec3 n, float hostAlpha)
{
    float amb = (alphaCtl[ch].z < 0.5) ? xfAmbient[ch].a : hostAlpha;
    float illum = 0.0;
    int mask = alphaLightMask[ch];

    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (((mask >> i) & 1) == 0)
            continue;

        vec3 v = lightLpx[i].xyz - vpos;
        float dist = length(v);
        vec3 ldir = (dist > 0.00001) ? v / dist : vec3(0.0, 0.0, 1.0);

        float diff = 1.0;
        if (alphaCtl[ch].w > 0.5)
        {
            float dp = dot(n, ldir);
            if (alphaCtl[ch].w > 1.5)
                dp = clamp(dp, 0.0, 1.0);
            diff = dp;
        }

        float attn = CosineAttenuation(ch, n, ldir, true, i);
        if (alphaAtten[ch].x > 0.5)
            attn *= DistanceAttenuation(ch, dist, true, i);

        illum += lightRgba[i].a * (diff * attn);
    }

    return clamp(clamp(illum, -1.0, 1.0) + amb, 0.0, 1.0);
}

vec4 LightChannel(int ch, vec3 vpos, vec3 n, vec4 host)
{
    vec4 ctl = colorCtl[ch];
    vec4 actl = alphaCtl[ch];

    vec3 matC = (ctl.x < 0.5) ? xfMaterial[ch].rgb : host.rgb;
    float matA = (actl.x < 0.5) ? xfMaterial[ch].a : host.a;

    vec3 illumC = vec3(1.0);
    if (ctl.y > 0.5)
        illumC = IlluminateColor(ch, vpos, n, host.rgb);

    float illumA = 1.0;
    if (actl.y > 0.5)
        illumA = IlluminateAlpha(ch, vpos, n, host.a);

    return vec4(clamp(matC * illumC, 0.0, 1.0), clamp(matA * illumA, 0.0, 1.0));
}

// ------------------------------------------------------------------ main

void main()
{
    vec2 rawTex[8];
    rawTex[0] = in_TexCoord0;  rawTex[1] = in_TexCoord1;
    rawTex[2] = in_TexCoord2;  rawTex[3] = in_TexCoord3;
    rawTex[4] = in_TexCoord4;  rawTex[5] = in_TexCoord5;
    rawTex[6] = in_TexCoord6;  rawTex[7] = in_TexCoord7;

    vec4 hostCol[2];
    // GFX::Color keeps its bytes in (A, B, G, R) order, so the attribute arrives reversed
    hostCol[0] = in_Color0.wzyx;
    hostCol[1] = in_Color1.wzyx;

    // ---- geometry transform ----

    int geomIdx = int(in_MatIdx0 & 0x3Fu);
    int mbase = geomIdx * 4;
    vec4 p = vec4(in_Position, 1.0);
    vec3 eye = vec3(
        dot(MatrixRow(mbase, 0), p),
        dot(MatrixRow(mbase, 1), p),
        dot(MatrixRow(mbase, 2), p));

    // ---- normal transform (inverse transpose matrix supplied by the host) ----

    int nbase = (geomIdx & 31) * 3;
    vec3 nrm = vec3(
        dot(NormalMatrixRow(nbase, 0), in_Normal),
        dot(NormalMatrixRow(nbase, 1), in_Normal),
        dot(NormalMatrixRow(nbase, 2), in_Normal));

    if (dot(nrm, nrm) > 0.0000001)
        nrm = normalize(nrm);
    else
        nrm = vec3(0.0, 0.0, 1.0);

    // ---- per-channel colour / alpha ----

    vec4 outCol[2];
    outCol[0] = hostCol[0];
    outCol[1] = hostCol[1];

    for (int ch = 0; ch < 2; ch++)
    {
        if (ch < xfNumColors)
            outCol[ch] = LightChannel(ch, eye, nrm, hostCol[ch]);
    }

    v_Color0 = outCol[0];
    v_Color1 = outCol[1];

    // ---- texture coordinate generation ----

    int texMatIdx[8];
    texMatIdx[0] = int((in_MatIdx0 >> 6) & 0x3Fu);
    texMatIdx[1] = int((in_MatIdx0 >> 12) & 0x3Fu);
    texMatIdx[2] = int((in_MatIdx0 >> 18) & 0x3Fu);
    texMatIdx[3] = int((in_MatIdx0 >> 24) & 0x3Fu);
    texMatIdx[4] = int((in_MatIdx1 >> 0) & 0x3Fu);
    texMatIdx[5] = int((in_MatIdx1 >> 6) & 0x3Fu);
    texMatIdx[6] = int((in_MatIdx1 >> 12) & 0x3Fu);
    texMatIdx[7] = int((in_MatIdx1 >> 18) & 0x3Fu);

    vec2 texOut[8];
    texOut[0] = rawTex[0];  texOut[1] = rawTex[1];
    texOut[2] = rawTex[2];  texOut[3] = rawTex[3];
    texOut[4] = rawTex[4];  texOut[5] = rawTex[5];
    texOut[6] = rawTex[6];  texOut[7] = rawTex[7];

    for (int i = 0; i < 8; i++)
    {
        if (i >= xfNumTex)
            continue;

        uint tp = xfTexGen[i].x;
        uint ttype = (tp >> 4) & 7u;        // texgen type
        uint srcRow = (tp >> 7) & 31u;      // source row
        uint projection = (tp >> 1) & 1u;
        uint inForm = (tp >> 2) & 1u;

        if (ttype == 0u)
        {
            // Regular transformation
            vec4 src;
            if (srcRow == 0u)
                src = vec4(in_Position, 1.0);
            else if (srcRow == 1u)
                src = vec4(in_Normal, 1.0);
            else if (srcRow == 2u)
                src = vec4(hostCol[0].rgb, 1.0);
            else if (srcRow == 3u)
                src = vec4(in_Binormal, 1.0);
            else if (srcRow == 4u)
                src = vec4(in_Tangent, 1.0);
            else
            {
                int trow = clamp(int(srcRow) - 5, 0, 7);
                src = vec4(rawTex[trow], 1.0, 1.0);
            }

            // input_form: ab01 -> (A, B, 1.0, 1.0), abc1 -> (A, B, C, 1.0)
            vec4 in4 = (inForm == 0u) ? vec4(src.x, src.y, 1.0, 1.0) : src;

            int mb = texMatIdx[i] * 4;
            float s = dot(MatrixRow(mb, 0), in4);
            float t = dot(MatrixRow(mb, 1), in4);

            if (projection != 0u)
            {
                float q = dot(MatrixRow(mb, 2), in4);
                if (abs(q) > 0.0000001)
                {
                    s /= q;
                    t /= q;
                }
            }

            texOut[i] = vec2(s, t);
        }
        else if (ttype == 2u || ttype == 3u)
        {
            // Colour texgen: (s,t) = (r, g:b concatenated)
            vec4 c = (ttype == 2u) ? hostCol[0] : hostCol[1];
            texOut[i] = vec2(c.r, (c.g * 256.0 + c.b) / 257.0);
        }
        // ttype == 1 (bump mapping) is not emulated: the incoming coordinate is passed through
    }

    // ---- dual texture transform (Rev B) ----

    if (xfDualTexTran != 0)
    {
        for (int i = 0; i < 8; i++)
        {
            if (i >= xfNumTex)
                continue;

            uint dp = xfDualGen[i];
            int dbase = int(dp & 0x3Fu) * 4;
            vec2 c = texOut[i];

            if (((dp >> 6) & 1u) != 0u)
            {
                float len = length(c);
                if (len > 0.0001)
                    c /= len;
            }

            vec4 in4 = vec4(c.x, c.y, 1.0, 1.0);

            vec4 r0 = vec4(dualTexMatrixMem[dbase + 0], dualTexMatrixMem[dbase + 1], dualTexMatrixMem[dbase + 2], dualTexMatrixMem[dbase + 3]);
            vec4 r1 = vec4(dualTexMatrixMem[dbase + 4], dualTexMatrixMem[dbase + 5], dualTexMatrixMem[dbase + 6], dualTexMatrixMem[dbase + 7]);

            texOut[i] = vec2(dot(r0, in4), dot(r1, in4));
        }
    }

    v_TexCoord0 = texOut[0];  v_TexCoord1 = texOut[1];
    v_TexCoord2 = texOut[2];  v_TexCoord3 = texOut[3];
    v_TexCoord4 = texOut[4];  v_TexCoord5 = texOut[5];
    v_TexCoord6 = texOut[6];  v_TexCoord7 = texOut[7];

    // ---- projection combine (gfx-xf.md 3.2) ----

    vec4 clip;
    if (xfProjOrtho != 0)
    {
        clip = vec4(
            xfProjParam[0] * eye.x + xfProjParam[1],
            xfProjParam[2] * eye.y + xfProjParam[3],
            xfProjParam[4] * eye.z + xfProjParam[5],
            1.0);
    }
    else
    {
        clip = vec4(
            xfProjParam[0] * eye.x + xfProjParam[1] * eye.z,
            xfProjParam[2] * eye.y + xfProjParam[3] * eye.z,
            xfProjParam[4] * eye.z + xfProjParam[5],
            -eye.z);
    }

    gl_Position = clip;
}
)glsl";

	// -------------------------------------------------------------------------------------------

	const char* TransformUnit::VertexShaderSource()
	{
		return XFVertexShader;
	}

	// The vertex shader of one colour-interpolation variant. GEN_MODE.flat_en asks for flat shading,
	// which the hardware implements by giving the colour planes zero gradients (gfx-ras2.md 3.1), so
	// the rasterized colour of the primitive is constant. GL expresses the same thing with the `flat`
	// qualifier on the colour varyings: the fragment shader receives the value of the provoking
	// vertex instead of an interpolated one. The qualifier has to appear on both sides of the link
	// (the vertex shader declares the varyings, the fragment shader consumes them), which is why both
	// programs have a flat variant. The texture coordinates stay interpolated.
	//
	// The provoking vertex is GL's default, the last vertex of the primitive; the available
	// specification does not state which vertex's colour the hardware's zero-gradient plane carries.
	std::string TransformUnit::VertexShaderSource(bool flat)
	{
		std::string src = XFVertexShader;

		if (flat)
		{
			const char* names[] = { "out vec4 v_Color0;", "out vec4 v_Color1;" };

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

	bool TransformUnit::CreateShader()
	{
		if (vert_shader != 0)
			return true;

		vert_shader = CompileShaderStage(GL_VERTEX_SHADER, XFVertexShader, "XF VERTEX");
		if (vert_shader == 0)
			return false;

		Report(Channel::GP, "XF vertex shader compiled\n");
		return true;
	}

	GLuint TransformUnit::VertexShader(bool flat)
	{
		if (!flat)
		{
			if (vert_shader == 0 && !CreateShader())
				return 0;

			return vert_shader;
		}

		if (vert_shader_flat == 0)
		{
			std::string source = VertexShaderSource(true);
			vert_shader_flat = CompileShaderStage(GL_VERTEX_SHADER, source.c_str(), "XF VERTEX (flat)");
		}

		return vert_shader_flat;
	}

	void TransformUnit::DisposeShader()
	{
		if (vert_shader != 0)
		{
			glDeleteShader(vert_shader);
			vert_shader = 0;
		}

		if (vert_shader_flat != 0)
		{
			glDeleteShader(vert_shader_flat);
			vert_shader_flat = 0;
		}
	}

	// Upload the XF register state to the XF (vertex) program.
	// The layout matches the declarations in the XF vertex shader above.

	void TransformUnit::UploadUniforms(GLProgram& p)
	{
		// --- matrices ---

		glUniform1fv(p.Uniform("matrixMem"), (GLsizei)XF_MATRIX_MEMORY_SIZE, xf.mvTexMtx);
		glUniform1fv(p.Uniform("nrmMatrixMem"), (GLsizei)XF_NORMAL_MATRIX_MEMORY_SIZE, xf.nrmMtx);
		glUniform1fv(p.Uniform("dualTexMatrixMem"), (GLsizei)XF_DUALTEX_MATRIX_MEMORY_SIZE, xf.dualTexMtx);

		// --- lights ---

		float rgba[8][4], a[8][4], k[8][4], lpx[8][4], dhx[8][4];

		for (int i = 0; i < 8; i++)
		{
			Light* l = &xf.light[i];

			rgba[i][0] = (float)l->rgba.R / 255.0f;
			rgba[i][1] = (float)l->rgba.G / 255.0f;
			rgba[i][2] = (float)l->rgba.B / 255.0f;
			rgba[i][3] = (float)l->rgba.A / 255.0f;

			for (int j = 0; j < 3; j++)
			{
				a[i][j] = l->a[j];
				k[i][j] = l->k[j];
				lpx[i][j] = l->lpx[j];
				dhx[i][j] = l->dhx[j];
			}
			a[i][3] = k[i][3] = lpx[i][3] = dhx[i][3] = 0.0f;

			// The light direction / half angle is used as a normalized vector
			float len = (float)sqrt(dhx[i][0] * dhx[i][0] + dhx[i][1] * dhx[i][1] + dhx[i][2] * dhx[i][2]);
			if (len > 0.00001f)
			{
				dhx[i][0] /= len;
				dhx[i][1] /= len;
				dhx[i][2] /= len;
			}

			// Distance attenuation must not divide by zero
			if (fabs(k[i][0]) < 0.00001f && fabs(k[i][1]) < 0.00001f && fabs(k[i][2]) < 0.00001f)
				k[i][0] = 0.00001f;
		}

		glUniform4fv(p.Uniform("lightRgba[0]"), 8, (float*)rgba);
		glUniform4fv(p.Uniform("lightA[0]"), 8, (float*)a);
		glUniform4fv(p.Uniform("lightK[0]"), 8, (float*)k);
		glUniform4fv(p.Uniform("lightLpx[0]"), 8, (float*)lpx);
		glUniform4fv(p.Uniform("lightDhx[0]"), 8, (float*)dhx);

		// --- colours and channel controls ---

		float clr[8][4];
		int idx = 0;

		for (int i = 0; i < 2; i++)
		{
			clr[idx][0] = (float)xf.ambient[i].R / 255.0f;
			clr[idx][1] = (float)xf.ambient[i].G / 255.0f;
			clr[idx][2] = (float)xf.ambient[i].B / 255.0f;
			clr[idx][3] = (float)xf.ambient[i].A / 255.0f;
			idx++;
		}
		glUniform4fv(p.Uniform("xfAmbient[0]"), 2, (float*)clr);

		idx = 0;
		for (int i = 0; i < 2; i++)
		{
			clr[idx][0] = (float)xf.material[i].R / 255.0f;
			clr[idx][1] = (float)xf.material[i].G / 255.0f;
			clr[idx][2] = (float)xf.material[i].B / 255.0f;
			clr[idx][3] = (float)xf.material[i].A / 255.0f;
			idx++;
		}
		glUniform4fv(p.Uniform("xfMaterial[0]"), 2, (float*)clr);

		float cctl[2][4], catten[2][4], actl[2][4], aatten[2][4];
		int cmask[2], amask[2];

		for (int i = 0; i < 2; i++)
		{
			ColorAlphaControl* cc = &xf.colorControl[i];
			ColorAlphaControl* ac = &xf.alphaControl[i];

			cctl[i][0] = (float)cc->MatSrc;
			cctl[i][1] = (float)cc->LightFunc;
			cctl[i][2] = (float)cc->AmbSrc;
			cctl[i][3] = (float)cc->DiffuseAtten;

			catten[i][0] = (float)cc->Atten;
			catten[i][1] = (float)cc->AttenSelect;
			catten[i][2] = catten[i][3] = 0.0f;

			actl[i][0] = (float)ac->MatSrc;
			actl[i][1] = (float)ac->LightFunc;
			actl[i][2] = (float)ac->AmbSrc;
			actl[i][3] = (float)ac->DiffuseAtten;

			aatten[i][0] = (float)ac->Atten;
			aatten[i][1] = (float)ac->AttenSelect;
			aatten[i][2] = aatten[i][3] = 0.0f;

			cmask[i] = (cc->Light0 ? 1 : 0) | (cc->Light1 ? 2 : 0) | (cc->Light2 ? 4 : 0) | (cc->Light3 ? 8 : 0) |
				(cc->Light4 ? 0x10 : 0) | (cc->Light5 ? 0x20 : 0) | (cc->Light6 ? 0x40 : 0) | (cc->Light7 ? 0x80 : 0);

			amask[i] = (ac->Light0 ? 1 : 0) | (ac->Light1 ? 2 : 0) | (ac->Light2 ? 4 : 0) | (ac->Light3 ? 8 : 0) |
				(ac->Light4 ? 0x10 : 0) | (ac->Light5 ? 0x20 : 0) | (ac->Light6 ? 0x40 : 0) | (ac->Light7 ? 0x80 : 0);
		}

		glUniform4fv(p.Uniform("colorCtl[0]"), 2, (float*)cctl);
		glUniform4fv(p.Uniform("colorAtten[0]"), 2, (float*)catten);
		glUniform1iv(p.Uniform("colorLightMask[0]"), 2, cmask);
		glUniform4fv(p.Uniform("alphaCtl[0]"), 2, (float*)actl);
		glUniform4fv(p.Uniform("alphaAtten[0]"), 2, (float*)aatten);
		glUniform1iv(p.Uniform("alphaLightMask[0]"), 2, amask);

		// --- scalars ---

		glUniform1i(p.Uniform("xfNumColors"), (GLint)(xf.numColors > 2 ? 2 : xf.numColors));
		glUniform1i(p.Uniform("xfNumTex"), (GLint)(xf.numTex > 8 ? 8 : xf.numTex));
		glUniform1fv(p.Uniform("xfProjParam"), 6, xf.projectionParam);
		glUniform1i(p.Uniform("xfProjOrtho"), xf.projectOrtho ? 1 : 0);
		glUniform1i(p.Uniform("xfDualTexTran"), xf.dualTexTran ? 1 : 0);

		uint32_t texgen[8][4];
		uint32_t dualgen[8];

		for (int i = 0; i < 8; i++)
		{
			texgen[i][0] = xf.tex[i].bits;
			texgen[i][1] = texgen[i][2] = texgen[i][3] = 0;
			dualgen[i] = xf.dualTex[i].bits;
		}

		glUniform4uiv(p.Uniform("xfTexGen[0]"), 8, (GLuint*)texgen);
		glUniform1uiv(p.Uniform("xfDualGen[0]"), 8, (GLuint*)dualgen);
	}

	void TransformUnit::GL_SetViewport(int x, int y, int w, int h, float znear, float zfar)
	{
		glViewport(x, gfx->scr_h - (h + y), w, h);
		glDepthRange(znear, zfar);
	}

	// -------------------------------------------------------------------------------------------
	// The XF register space (gfx-xf.md 4)
	//
	// The CP addresses the XF registers one word at a time over the CP -> XF interface. The matrix
	// RAMs and the light records are addressed word by word, so both a block write and a write of a
	// single register work on any register of the space.
	// -------------------------------------------------------------------------------------------

	// Write one word of the XF register space.
	void TransformUnit::WriteXFReg(size_t index, uint32_t value)
	{
		//
		// ModelView / Texture matrix RAM (0x0000-0x00FF)
		//

		if (index < XF_MATRIX_MEMORY_SIZE)
		{
			xf.mvTexMtx[index] = *(float*)&value;
			return;
		}

		//
		// Normal matrix RAM (0x0400-0x045F)
		//

		if (index >= XF_NORMAL_MATRIX_MEMORY_ID && index < (XF_NORMAL_MATRIX_MEMORY_ID + XF_NORMAL_MATRIX_MEMORY_SIZE))
		{
			xf.nrmMtx[index - XF_NORMAL_MATRIX_MEMORY_ID] = *(float*)&value;
			return;
		}

		//
		// Dual texture transform matrix RAM (0x0500-0x05FF)
		//

		if (index >= XF_DUALTEX_MATRIX_MEMORY_ID && index < (XF_DUALTEX_MATRIX_MEMORY_ID + XF_DUALTEX_MATRIX_MEMORY_SIZE))
		{
			xf.dualTexMtx[index - XF_DUALTEX_MATRIX_MEMORY_ID] = *(float*)&value;
			return;
		}

		//
		// Light records (0x0600-0x067F), XF_LIGHT_DATA_SIZE words per light
		//

		if (index >= XF_LIGHT_MEMORY_ID && index < (XF_LIGHT_MEMORY_ID + XF_LIGHT_MEMORY_SIZE))
		{
			Light* light = &xf.light[(index - XF_LIGHT_MEMORY_ID) / XF_LIGHT_DATA_SIZE];
			size_t ofs = (index - XF_LIGHT_MEMORY_ID) % XF_LIGHT_DATA_SIZE;

			if (ofs < 3)
				light->Reserved[ofs] = value;
			else if (ofs == 3)
				light->rgba.RGBA = value;
			else if (ofs < 7)
				light->a[ofs - 4] = *(float*)&value;
			else if (ofs < 0xa)
				light->k[ofs - 7] = *(float*)&value;
			else if (ofs < 0xd)
				light->lpx[ofs - 0xa] = *(float*)&value;
			else
				light->dhx[ofs - 0xd] = *(float*)&value;

			return;
		}

		switch (index)
		{
			case XF_ERROR_ID:			xf.error = value; break;
			case XF_DIAGNOSTICS_ID:		xf.diagnostics = value; break;
			case XF_STATE0_ID:			xf.state[0] = value; break;
			case XF_STATE1_ID:			xf.state[1] = value; break;
			case XF_CLOCK_ID:			xf.clock = value; break;

			case XF_CLIP_DISABLE_ID:
				// TODO: How does this affect Culling in the Setup Unit?
				xf.clipDisable.bits = value;
				break;

			case XF_PERF0_ID:			xf.perf[0] = value; break;
			case XF_PERF1_ID:			xf.perf[1] = value; break;

			//
			// set matrix index
			//

			case XF_MATINDEX_A_ID:		xf.matIdxA.bits = value; break;
			case XF_MATINDEX_B_ID:		xf.matIdxB.bits = value; break;

			//
			// viewport configuration. The emulator viewport is refreshed on every word, so a
			// viewport programmed with several block writes also ends up correct.
			//

			case XF_VIEWPORT_SCALE_X_ID:
			case XF_VIEWPORT_SCALE_Y_ID:
			case XF_VIEWPORT_SCALE_Z_ID:
				xf.viewportScale[index - XF_VIEWPORT_SCALE_X_ID] = *(float*)&value;
				ApplyViewport();
				break;

			case XF_VIEWPORT_OFFSET_X_ID:
			case XF_VIEWPORT_OFFSET_Y_ID:
			case XF_VIEWPORT_OFFSET_Z_ID:
				xf.viewportOffset[index - XF_VIEWPORT_OFFSET_X_ID] = *(float*)&value;
				ApplyViewport();
				break;

			//
			// projection matrix
			//

			case XF_PROJECTION_A_ID:
			case XF_PROJECTION_B_ID:
			case XF_PROJECTION_C_ID:
			case XF_PROJECTION_D_ID:
			case XF_PROJECTION_E_ID:
			case XF_PROJECTION_F_ID:
				xf.projectionParam[index - XF_PROJECTION_A_ID] = *(float*)&value;
				break;

			case XF_PROJECT_ORTHO_ID:
				xf.projectOrtho = (*(float*)&value) != 0.0f;
				break;

			//
			// channel constant color registers
			//

			case XF_AMBIENT0_ID:		xf.ambient[0].RGBA = value; break;
			case XF_AMBIENT1_ID:		xf.ambient[1].RGBA = value; break;
			case XF_MATERIAL0_ID:		xf.material[0].RGBA = value; break;
			case XF_MATERIAL1_ID:		xf.material[1].RGBA = value; break;

			//
			// channel control registers
			//

			case XF_COLOR0CNTL_ID:		xf.colorControl[0].bits = value; break;
			case XF_COLOR1CNTL_ID:		xf.colorControl[1].bits = value; break;
			case XF_ALPHA0CNTL_ID:		xf.alphaControl[0].bits = value; break;
			case XF_ALPHA1CNTL_ID:		xf.alphaControl[1].bits = value; break;

			//
			// set dualtex enable / disable
			//

			case XF_DUALTEX_ID:			xf.dualTexTran = value; break;

			case XF_DUALGEN0_ID:
			case XF_DUALGEN1_ID:
			case XF_DUALGEN2_ID:
			case XF_DUALGEN3_ID:
			case XF_DUALGEN4_ID:
			case XF_DUALGEN5_ID:
			case XF_DUALGEN6_ID:
			case XF_DUALGEN7_ID:
				xf.dualTex[index - XF_DUALGEN0_ID].bits = value;
				break;

			case XF_INVTXSPEC_ID:		xf.vtxSpec.bits = value; break;

			//
			// number of output colors
			//

			case XF_NUMCOLS_ID:			xf.numColors = value; break;

			//
			// set number of texgens
			//

			case XF_NUMTEX_ID:			xf.numTex = value; break;

			//
			// set texgen configuration
			//

			case XF_TEXGEN0_ID:
			case XF_TEXGEN1_ID:
			case XF_TEXGEN2_ID:
			case XF_TEXGEN3_ID:
			case XF_TEXGEN4_ID:
			case XF_TEXGEN5_ID:
			case XF_TEXGEN6_ID:
			case XF_TEXGEN7_ID:
				xf.tex[index - XF_TEXGEN0_ID].bits = value;
				break;

			//
			// not implemented
			//

			default:
				Report(Channel::GP, "Unknown XF load, index: 0x%04X\n", index);
				break;
		}
	}

	// Read one word of the XF register space out of the current register state.
	// The emulator keeps the matrix words as floats, so a read returns exactly what was written
	// (in the hardware the normal matrix and the light parameters are 20-bit words).
	uint32_t TransformUnit::ReadXFReg(size_t index)
	{
		float value;

		//
		// ModelView / Texture matrix RAM (0x0000-0x00FF)
		//

		if (index < XF_MATRIX_MEMORY_SIZE)
		{
			return *(uint32_t*)&xf.mvTexMtx[index];
		}

		//
		// Normal matrix RAM (0x0400-0x045F)
		//

		if (index >= XF_NORMAL_MATRIX_MEMORY_ID && index < (XF_NORMAL_MATRIX_MEMORY_ID + XF_NORMAL_MATRIX_MEMORY_SIZE))
		{
			return *(uint32_t*)&xf.nrmMtx[index - XF_NORMAL_MATRIX_MEMORY_ID];
		}

		//
		// Dual texture transform matrix RAM (0x0500-0x05FF)
		//

		if (index >= XF_DUALTEX_MATRIX_MEMORY_ID && index < (XF_DUALTEX_MATRIX_MEMORY_ID + XF_DUALTEX_MATRIX_MEMORY_SIZE))
		{
			return *(uint32_t*)&xf.dualTexMtx[index - XF_DUALTEX_MATRIX_MEMORY_ID];
		}

		//
		// Light records (0x0600-0x067F), XF_LIGHT_DATA_SIZE words per light
		//

		if (index >= XF_LIGHT_MEMORY_ID && index < (XF_LIGHT_MEMORY_ID + XF_LIGHT_MEMORY_SIZE))
		{
			const Light* light = &xf.light[(index - XF_LIGHT_MEMORY_ID) / XF_LIGHT_DATA_SIZE];
			size_t ofs = (index - XF_LIGHT_MEMORY_ID) % XF_LIGHT_DATA_SIZE;

			if (ofs < 3)
				return light->Reserved[ofs];
			if (ofs == 3)
				return light->rgba.RGBA;

			if (ofs < 7)
				value = light->a[ofs - 4];
			else if (ofs < 0xa)
				value = light->k[ofs - 7];
			else if (ofs < 0xd)
				value = light->lpx[ofs - 0xa];
			else
				value = light->dhx[ofs - 0xd];

			return *(uint32_t*)&value;
		}

		switch (index)
		{
			case XF_ERROR_ID:			return xf.error;
			case XF_DIAGNOSTICS_ID:		return xf.diagnostics;
			case XF_STATE0_ID:			return xf.state[0];
			case XF_STATE1_ID:			return xf.state[1];
			case XF_CLOCK_ID:			return xf.clock;
			case XF_CLIP_DISABLE_ID:	return xf.clipDisable.bits;
			case XF_PERF0_ID:			return xf.perf[0];
			case XF_PERF1_ID:			return xf.perf[1];
			case XF_MATINDEX_A_ID:		return xf.matIdxA.bits;
			case XF_MATINDEX_B_ID:		return xf.matIdxB.bits;

			case XF_VIEWPORT_SCALE_X_ID:
			case XF_VIEWPORT_SCALE_Y_ID:
			case XF_VIEWPORT_SCALE_Z_ID:
				return *(uint32_t*)&xf.viewportScale[index - XF_VIEWPORT_SCALE_X_ID];

			case XF_VIEWPORT_OFFSET_X_ID:
			case XF_VIEWPORT_OFFSET_Y_ID:
			case XF_VIEWPORT_OFFSET_Z_ID:
				return *(uint32_t*)&xf.viewportOffset[index - XF_VIEWPORT_OFFSET_X_ID];

			case XF_PROJECTION_A_ID:
			case XF_PROJECTION_B_ID:
			case XF_PROJECTION_C_ID:
			case XF_PROJECTION_D_ID:
			case XF_PROJECTION_E_ID:
			case XF_PROJECTION_F_ID:
				return *(uint32_t*)&xf.projectionParam[index - XF_PROJECTION_A_ID];

			case XF_PROJECT_ORTHO_ID:
				value = xf.projectOrtho ? 1.0f : 0.0f;
				return *(uint32_t*)&value;

			case XF_AMBIENT0_ID:		return xf.ambient[0].RGBA;
			case XF_AMBIENT1_ID:		return xf.ambient[1].RGBA;
			case XF_MATERIAL0_ID:		return xf.material[0].RGBA;
			case XF_MATERIAL1_ID:		return xf.material[1].RGBA;

			case XF_COLOR0CNTL_ID:		return xf.colorControl[0].bits;
			case XF_COLOR1CNTL_ID:		return xf.colorControl[1].bits;
			case XF_ALPHA0CNTL_ID:		return xf.alphaControl[0].bits;
			case XF_ALPHA1CNTL_ID:		return xf.alphaControl[1].bits;

			case XF_DUALTEX_ID:			return xf.dualTexTran;

			case XF_DUALGEN0_ID:
			case XF_DUALGEN1_ID:
			case XF_DUALGEN2_ID:
			case XF_DUALGEN3_ID:
			case XF_DUALGEN4_ID:
			case XF_DUALGEN5_ID:
			case XF_DUALGEN6_ID:
			case XF_DUALGEN7_ID:
				return xf.dualTex[index - XF_DUALGEN0_ID].bits;

			case XF_INVTXSPEC_ID:		return xf.vtxSpec.bits;
			case XF_NUMCOLS_ID:			return xf.numColors;
			case XF_NUMTEX_ID:			return xf.numTex;

			case XF_TEXGEN0_ID:
			case XF_TEXGEN1_ID:
			case XF_TEXGEN2_ID:
			case XF_TEXGEN3_ID:
			case XF_TEXGEN4_ID:
			case XF_TEXGEN5_ID:
			case XF_TEXGEN6_ID:
			case XF_TEXGEN7_ID:
				return xf.tex[index - XF_TEXGEN0_ID].bits;
		}

		Report(Channel::GP, "Unknown XF read, index: 0x%04X\n", index);
		return 0;
	}

	// The XF viewport registers hold hardware units; convert them into a GL viewport and depth
	// range. Called whenever one of 0x101A-0x101F is written.
	void TransformUnit::ApplyViewport()
	{
		float w, h, x, y, zf, zn;

		//
		// convert the coefficients to human usable form
		//

		w = xf.viewportScale[0] * 2;									// w / 2
		h = -xf.viewportScale[1] * 2;									// -h / 2
		x = xf.viewportOffset[0] - xf.viewportScale[0] - 342;			// x + w/2 + 342
		y = xf.viewportOffset[1] + xf.viewportScale[1] - 342;			// y + h/2 + 342
		zf = xf.viewportOffset[2] / 16777215.0f;						// ZMAX * zfar
		zn = -((xf.viewportScale[2] / 16777215.0f) - zf);				// ZMAX * (zfar - znear)

		GL_SetViewport((int)x, (int)y, (int)w, (int)h, zn, zf);
	}

	// -------------------------------------------------------------------------------------------
	// CP -> XF interface (gfx-xf.md 2.1)
	//
	// The CP pushes register loads, register read requests and vertex rows into the XF, and the XF
	// answers register reads on the read-back line. The XF is the entry point of the graphics
	// pipeline: the vertex stream that it produces leaves it towards the Setup Unit.
	// -------------------------------------------------------------------------------------------

	bool TransformUnit::CPReady()
	{
		// The XF takes a word every cycle; only a read-back value that the CP has not taken yet
		// holds it off (XFready is deasserted while XFrdValid is asserted).
		return !xfRdValid;
	}

	void TransformUnit::CPRegLoadBegin(size_t startIdx, size_t amount)
	{
		xfLoadIdx = startIdx;
		xfLoadAmount = amount;
	}

	void TransformUnit::CPRegLoadData(uint32_t value)
	{
		if (xfLoadAmount == 0)
		{
			// The block write is over; further data words would spill into the register space.
			Report(Channel::GP, "XF: unexpected register data word (address: 0x%04X)\n", xfLoadIdx);
			return;
		}

		xfLoadAmount--;
		WriteXFReg(xfLoadIdx++, value);
	}

	void TransformUnit::CPRegRead(size_t index)
	{
		xfRdData = ReadXFReg(index);
		xfRdValid = true;
	}

	bool TransformUnit::CPTakeReadData(uint32_t* data)
	{
		if (!xfRdValid)
			return false;

		*data = xfRdData;
		xfRdValid = false;
		return true;
	}

	void TransformUnit::CPSuCommand(size_t index, uint32_t value)
	{
		// The XF does not interpret the bypass words: they are forwarded to the SU verbatim.
		gfx->su->loadSUReg(index, value);
	}

	//
	// XF -> SU: the vertex stream (gfx-xf.md 2.2)
	//
	// The transform itself is done by the XF vertex shader (the register state is uploaded to it as
	// uniforms, see UploadUniforms), so the XF hands the vertex rows over to the Setup Unit, which
	// drives the rasterizers with them.
	//

	void TransformUnit::CPDrawBegin(RAS_Primitive prim, size_t vtx_num)
	{
		gfx->su->BeginPrimitive(prim, vtx_num);
	}

	void TransformUnit::CPVertex(const Vertex* v)
	{
		gfx->su->SendVertex(v);
	}

	void TransformUnit::CPDrawEnd()
	{
		gfx->su->EndPrimitive();
	}

	TransformUnit::TransformUnit(HWConfig* config, GFXCore* parent_gfx)
	{
		gfx = parent_gfx;

		// Before the first XF load the projection is an identity transform (like the GL default it replaces)
		xf.projectionParam[0] = 1.0f;
		xf.projectionParam[2] = 1.0f;
		xf.projectionParam[4] = 1.0f;
	}

	TransformUnit::~TransformUnit()
	{
	}

	void TransformUnit::Reset()
	{
		xf = XFState{};

		xfLoadIdx = 0;
		xfLoadAmount = 0;
		xfRdData = 0;
		xfRdValid = false;

		// Before the first XF load the projection is an identity transform (like the GL default it
		// replaces), see the constructor.
		xf.projectionParam[0] = 1.0f;
		xf.projectionParam[2] = 1.0f;
		xf.projectionParam[4] = 1.0f;

		// The GL viewport is not refreshed here: the zeroed viewport registers do not describe one.
		// It is restored by GFXCore::ApplyDefaultGLState().
	}
}

