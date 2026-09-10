// Shared plumbing for the rendered-image galleries.
//
// The gfx_report_*_test.cpp files each walk a table of graphics-pipeline configurations, render one
// full-EFB picture per configuration and publish it into the HTML report (the report writer itself
// lives in gfx_test_support.cpp). This header holds what all of them need:
//
//   * the register packs of the TEV stage environment and the pass-through setups of the XF, the
//     rasterizer/TEV and the pixel engine - the same ones gfx_tev_test.cpp / gfx_texture_test.cpp
//     use, so a gallery image and a unit test draw through the very same path;
//   * the quad helpers (clip space and EFB pixel space);
//   * Publish(), which validates the picture before it reaches the report: a blank, uniform or
//     duplicated image must never be published, so the check is part of the gallery itself.
//
// Everything here is header-only and inline: the galleries are separate translation units.

#pragma once

#include "pch.h"
#include "gfx_test_common.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace GfxGallery
{
	using GfxUnitTest::GfxTestMachine;
	using GfxUnitTest::Machine;
	using GfxUnitTest::OutputDir;
	using GfxUnitTest::Report;
	using GfxUnitTest::WriteMainMemory;
	using GfxUnitTest::Widen;

	//! The size of the EFB the galleries render into (the test machine's render target).
	const int EfbWidth = 640;
	const int EfbHeight = 480;

	//! The test machine, started and reset. Fails the test when the GL backend is not available.
	inline GfxTestMachine& M()
	{
		GfxUnitTest::RequireGL();
		return Machine();
	}

	// -------------------------------------------------------------------------------------------
	// Small value helpers
	// -------------------------------------------------------------------------------------------

	struct Rgba
	{
		uint8_t R = 0, G = 0, B = 0, A = 0xff;
	};

	inline Rgba MakeRgba(int r, int g, int b, int a = 0xff)
	{
		Rgba c;
		c.R = (uint8_t)r;
		c.G = (uint8_t)g;
		c.B = (uint8_t)b;
		c.A = (uint8_t)a;
		return c;
	}

	inline uint32_t AsBits(float value)
	{
		uint32_t bits;
		memcpy(&bits, &value, 4);
		return bits;
	}

	//! The vertex colour of column `index` of a gallery grid: bright, well separated hues.
	inline Rgba GridColor(int index)
	{
		static const Rgba colors[8] = {
			{ 0xff, 0x20, 0x20, 0xff },		// red
			{ 0x20, 0xff, 0x20, 0xff },		// green
			{ 0x30, 0x60, 0xff, 0xff },		// blue
			{ 0xff, 0xff, 0x20, 0xff },		// yellow
			{ 0x20, 0xff, 0xff, 0xff },		// cyan
			{ 0xff, 0x20, 0xff, 0xff },		// magenta
			{ 0xff, 0xff, 0xff, 0xff },		// white
			{ 0x18, 0x18, 0x18, 0xff },		// near black
		};

		return colors[index & 7];
	}

	// -------------------------------------------------------------------------------------------
	// TEV register packs (gfx-tev.md 3.2, 4.2)
	// -------------------------------------------------------------------------------------------

	//! The sixteen operand selects of the colour environment: the four colour registers (rgb and alpha
	//! forms), the texel, the rasterized colour and the four constants.
	enum COperand
	{
		CREG0 = 0, CREG0A = 1, CREG1 = 2, CREG1A = 3, CREG2 = 4, CREG2A = 5, CREG3 = 6, CREG3A = 7,
		CTEX = 8, CTEXA = 9, CRAST = 10, CRASTA = 11, CONE = 12, CHALF = 13, CKONST = 14, CZERO = 15,
	};

	//! The eight operand selects of the alpha environment (three bits each).
	enum AOperand
	{
		AREG0 = 0, AREG1 = 1, AREG2 = 2, AREG3 = 3, ATEX = 4, ARAST = 5, AKONST = 6, AZERO = 7,
	};

	//! TEV_COLOR_ENV_n: the four operand selects, the arithmetic controls and the destination.
	inline uint32_t ColorEnv(int sela, int selb, int selc, int seld, int bias = 0, int sub = 0,
		int clamp = 1, int shift = 0, int dest = 0)
	{
		return (uint32_t)seld | ((uint32_t)selc << 4) | ((uint32_t)selb << 8) | ((uint32_t)sela << 12) |
			((uint32_t)bias << 16) | ((uint32_t)sub << 18) | ((uint32_t)clamp << 19) |
			((uint32_t)shift << 20) | ((uint32_t)dest << 22);
	}

	//! TEV_ALPHA_ENV_n: same controls, plus the alpha compare mode and the texel component swap.
	inline uint32_t AlphaEnv(int sela, int selb, int selc, int seld, int bias = 0, int sub = 0,
		int clamp = 1, int shift = 0, int dest = 0, int mode = 0, int swap = 0)
	{
		return (uint32_t)mode | ((uint32_t)swap << 2) | ((uint32_t)seld << 4) | ((uint32_t)selc << 7) |
			((uint32_t)selb << 10) | ((uint32_t)sela << 13) | ((uint32_t)bias << 16) |
			((uint32_t)sub << 18) | ((uint32_t)clamp << 19) | ((uint32_t)shift << 20) |
			((uint32_t)dest << 22);
	}

	//! TEV_KSEL_n: the K constant selectors of the stages 2n (x) and 2n+1 (y).
	inline uint32_t KSel(int kcsel0, int kasel0, int kcsel1, int kasel1)
	{
		return ((uint32_t)kcsel0 << 4) | ((uint32_t)kasel0 << 9) |
			((uint32_t)kcsel1 << 14) | ((uint32_t)kasel1 << 19);
	}

	//! TEV_ALPHAFUNC: the two reference values, the two operators and the combining logic.
	inline uint32_t AlphaFunc(int ref0, int ref1, int op0, int op1, int logic)
	{
		return (uint32_t)ref0 | ((uint32_t)ref1 << 8) | ((uint32_t)op0 << 16) |
			((uint32_t)op1 << 19) | ((uint32_t)logic << 22);
	}

	// -------------------------------------------------------------------------------------------
	// Pipeline setups
	// -------------------------------------------------------------------------------------------

	//! A pass-through XF: the host position reaches clip space, the host colours are handed to the
	//! TEV without lighting and the host texture coordinates pass through (no texgen).
	inline void SetupPassThroughXF(GfxTestMachine& m)
	{
		float identity[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
		m.XfLoadFloats(GFX::XF_MATRIX_MEMORY_ID, identity, 16);

		float identityNrm[9] = { 1,0,0, 0,1,0, 0,0,1 };
		m.XfLoadFloats(GFX::XF_NORMAL_MATRIX_MEMORY_ID, identityNrm, 9);

		float proj[6] = { 1,0, 1,0, 1,0 };
		m.XfLoadFloats(GFX::XF_PROJECTION_A_ID, proj, 6);
		m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, AsBits(1.0f));

		m.XfLoad(GFX::XF_NUMCOLS_ID, 0);
		m.XfLoad(GFX::XF_NUMTEX_ID, 0);
		m.XfLoad(GFX::XF_MATINDEX_A_ID, 0);
		m.XfLoad(GFX::XF_MATINDEX_B_ID, 0);
	}

	//! The pixel state every gallery starts from: no depth test, no blending, no logic op, both write
	//! masks enabled.
	inline void SetupDefaultPixelState(GfxTestMachine& m)
	{
		m.BpLoad(PE_ZMODE_ID, 0);
		m.BpLoad(PE_CMODE0_ID, (1u << 3) | (1u << 4));
		m.BpLoad(PE_CMODE1_ID, 0);
	}

	//! The copy engine's clear colours: the EFB is filled with them by BeginFrame().
	inline void SetClearColor(GfxTestMachine& m, const Rgba& c, uint32_t depth = 0xFFFFFF)
	{
		m.BpLoad(PE_COPY_CLEAR_AR_ID, (uint32_t)c.R | ((uint32_t)c.A << 8));
		m.BpLoad(PE_COPY_CLEAR_GB_ID, (uint32_t)c.B | ((uint32_t)c.G << 8));
		m.BpLoad(PE_COPY_CLEAR_Z_ID, depth);
	}

	//! One TEV stage that outputs the rasterized (host) colour and its alpha.
	inline void SetupRasterStage0(GfxTestMachine& m)
	{
		m.BpLoad(GEN_MODE_ID, 0);						// one TEV stage
		m.BpLoad(RAS1_TREF0_ID, 0);						// te0 = 0, cc0 = 0 (the rasterized colour 0)
		m.BpLoad(TEV_COLOR_ENV_0_ID, ColorEnv(15, 15, 15, 10));
		m.BpLoad(TEV_ALPHA_ENV_0_ID, AlphaEnv(5, 5, 5, 5));
	}

	//! One TEV stage that outputs the texel of texture map 0, sampled with texcoord 0.
	inline void SetupTextureStage0(GfxTestMachine& m, int map = 0)
	{
		m.BpLoad(GEN_MODE_ID, 0);
		m.BpLoad(RAS1_TREF0_ID, (uint32_t)map | (1u << 6));		// ti0 = map, te0 = 1, tc0 = 0
		m.BpLoad(TEV_COLOR_ENV_0_ID, ColorEnv(15, 15, 15, 8));	// d = the texel colour
		m.BpLoad(TEV_ALPHA_ENV_0_ID, AlphaEnv(4, 7, 7, 4));		// alpha = the texel alpha
	}

	//! Program texture map `map` (0..7): an image of `width` x `height` texels of the given format. The
	//! maps 0-3 are programmed through the I0-I3 block of the TX registers and the maps 4-7 through the
	//! I4-I7 block, which is laid out identically.
	//!
	//! `mode1` is the TX_SETMODE1 word (the LOD limits). Its default is minlod 0 / maxlod 10, i.e. the
	//! whole mip chain, because that is the sampler behaviour the galleries are written against; a
	//! picture that shows the limits programs its own value (the fields are 4.4 fixed point, see
	//! tx.cpp ApplyTextureParams).
	inline void SetupTexture(GfxTestMachine& m, int map, uint32_t addr, int width, int height,
		GFX::TexFormat format, const uint8_t* raw, size_t rawSize, uint32_t mode0 = 0,
		uint32_t mode1 = (10u << 8))
	{
		unsigned base = (map < 4) ? (unsigned)map : (unsigned)(map - 4);
		unsigned block = (map < 4) ? 0u : 0x20u;		// the I4-I7 registers sit 0x20 above the I0-I3 ones

		WriteMainMemory(addr, raw, rawSize);

		m.BpLoad(block + TX_SETIMAGE0_I0_ID + base,
			(uint32_t)(width - 1) | ((uint32_t)(height - 1) << 10) | ((uint32_t)format << 20));
		m.BpLoad(block + TX_SETIMAGE3_I0_ID + base, addr >> 5);
		m.BpLoad(block + TX_SETMODE0_I0_ID + base, mode0);
		m.BpLoad(block + TX_SETMODE1_I0_ID + base, mode1);
	}

	//! Load `count` palette entries (already encoded in the palette format) at the start of the TLUT
	//! and bind them to texture map `map`.
	inline void SetupTlut(GfxTestMachine& m, int map, uint32_t addr, GFX::TlutFormat format,
		const uint16_t* entries, size_t count)
	{
		unsigned base = (map < 4) ? (unsigned)map : (unsigned)(map - 4);
		unsigned block = (map < 4) ? 0u : 0x20u;

		std::vector<uint8_t> raw(count * 2);
		for (size_t i = 0; i < count; i++)
		{
			raw[i * 2 + 0] = (uint8_t)(entries[i] >> 8);
			raw[i * 2 + 1] = (uint8_t)entries[i];
		}

		WriteMainMemory(addr, raw.data(), raw.size());

		m.BpLoad(TX_LOADTLUT0_ID, addr >> 5);
		m.BpLoad(TX_LOADTLUT1_ID, 0u | ((uint32_t)(count - 1) << 10));
		m.BpLoad(block + TX_SETTLUT_I0_ID + base, 0u | ((uint32_t)format << 10));
	}

	// -------------------------------------------------------------------------------------------
	// Drawing
	// -------------------------------------------------------------------------------------------

	//! The clip-space position of an EFB pixel (the EFB origin is its top left corner).
	inline void PixelToClip(float px, float py, float* x, float* y)
	{
		*x = (px / (float)EfbWidth) * 2.0f - 1.0f;
		*y = 1.0f - (py / (float)EfbHeight) * 2.0f;
	}

	//! A quad in clip space, given in the fan order the rasterizer expects (bottom left first).
	inline void DrawClipQuad(GfxTestMachine& m, float x0, float y0, float x1, float y1,
		const Rgba& bl, const Rgba& br, const Rgba& tr, const Rgba& tl,
		float s0 = 0.0f, float t0 = 0.0f, float s1 = 1.0f, float t1 = 1.0f)
	{
		GFX::Vertex quad[4];
		quad[0] = GfxTestMachine::MakeVertex(x0, y0, 0, bl.R, bl.G, bl.B, bl.A);
		quad[1] = GfxTestMachine::MakeVertex(x1, y0, 0, br.R, br.G, br.B, br.A);
		quad[2] = GfxTestMachine::MakeVertex(x1, y1, 0, tr.R, tr.G, tr.B, tr.A);
		quad[3] = GfxTestMachine::MakeVertex(x0, y1, 0, tl.R, tl.G, tl.B, tl.A);

		// s runs left to right, t runs bottom to top
		quad[0].TexCoord[0][0] = s0; quad[0].TexCoord[0][1] = t0;
		quad[1].TexCoord[0][0] = s1; quad[1].TexCoord[0][1] = t0;
		quad[2].TexCoord[0][0] = s1; quad[2].TexCoord[0][1] = t1;
		quad[3].TexCoord[0][0] = s0; quad[3].TexCoord[0][1] = t1;

		m.DrawQuad(quad);
	}

	//! A rectangle of the EFB, given in pixels with the origin at the top left, in one colour.
	inline void DrawPixelRect(GfxTestMachine& m, float px0, float py0, float px1, float py1,
		const Rgba& c, float s0 = 0.0f, float t0 = 0.0f, float s1 = 1.0f, float t1 = 1.0f)
	{
		float x0, y0, x1, y1;
		PixelToClip(px0, py0, &x0, &y0);		// top left
		PixelToClip(px1, py1, &x1, &y1);		// bottom right

		DrawClipQuad(m, x0, y1, x1, y0, c, c, c, c, s0, t0, s1, t1);
	}

	//! A full-screen quad in clip space with one colour (or four, one per corner).
	inline void DrawScreenQuad(GfxTestMachine& m, const Rgba& c)
	{
		DrawClipQuad(m, -1, -1, 1, 1, c, c, c, c);
	}

	inline void DrawScreenQuad(GfxTestMachine& m, const Rgba& bl, const Rgba& br, const Rgba& tr, const Rgba& tl)
	{
		DrawClipQuad(m, -1, -1, 1, 1, bl, br, tr, tl);
	}

	//! A diagonal background: a dark blue-to-magenta field the galleries draw their subject on, so
	//! that "nothing was drawn" is visible as such and no picture is ever a flat colour.
	inline void DrawBackground(GfxTestMachine& m)
	{
		DrawScreenQuad(m,
			MakeRgba(0x18, 0x18, 0x40),
			MakeRgba(0x40, 0x10, 0x30),
			MakeRgba(0x20, 0x20, 0x50),
			MakeRgba(0x50, 0x18, 0x28));
	}

	//! A second background, for the galleries whose "nothing was drawn" picture would otherwise be the
	//! same pixels as another gallery's.
	inline void DrawBackgroundAlt(GfxTestMachine& m)
	{
		DrawScreenQuad(m, MakeRgba(0x30, 0x20, 0x10), MakeRgba(0x30, 0x20, 0x10),
			MakeRgba(0xB0, 0x90, 0x40), MakeRgba(0xB0, 0x90, 0x40));
	}

	//! Draw the gallery background *with the raster stage* and then put the texture stage back. The
	//! TEV program is part of the pipeline state, so a background quad drawn while the stage reads its
	//! texel would paint the texture instead of the vertex colours.
	inline void DrawBackgroundThenTextureStage(GfxTestMachine& m, int map = 0)
	{
		SetupRasterStage0(m);
		m.BpLoad(PE_CMODE0_ID, (1u << 3) | (1u << 4));
		DrawBackground(m);
		SetupTextureStage0(m, map);
	}

	// -------------------------------------------------------------------------------------------
	// Publishing
	// -------------------------------------------------------------------------------------------

	//! Read the whole EFB back and check that the picture is worth publishing.
	inline void CheckPicture(GfxTestMachine& m, const std::string& file)
	{
		std::vector<uint8_t> rgb;
		m.ReadColor(0, 0, EfbWidth, EfbHeight, rgb);

		// How many distinct colours the picture is made of, on a coarse grid
		std::set<uint32_t> distinct;
		for (int y = 0; y < EfbHeight; y += 5)
		{
			for (int x = 0; x < EfbWidth; x += 5)
			{
				const uint8_t* p = &rgb[((size_t)y * EfbWidth + x) * 3];
				distinct.insert(((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2]);
			}
		}

		Assert::IsTrue(distinct.size() >= 6,
			Widen(file + ": the picture is (nearly) a flat colour - " +
				std::to_string(distinct.size()) + " distinct colours").c_str());

		// A 64-bit FNV-1a over the whole EFB: no two published pictures may have the same pixels. The
		// file that was published first is kept, so a collision names both pictures.
		uint64_t hash = 1469598103934665603ull;
		for (uint8_t value : rgb)
		{
			hash ^= value;
			hash *= 1099511628211ull;
		}

		static std::map<uint64_t, std::string> published;

		auto seen = published.find(hash);

		Assert::IsTrue(seen == published.end(),
			Widen(file + ": the picture has exactly the same pixels as " +
				(seen == published.end() ? std::string() : seen->second)).c_str());

		published[hash] = file;
	}

	//! Save the current EFB as a PNG and put it into the report.
	inline void Publish(GfxTestMachine& m, const std::string& title, const std::string& file,
		const std::string& text)
	{
		CheckPicture(m, file);

		Assert::IsTrue(m.SaveScreenshot(OutputDir() + "/" + file, 0, 0, EfbWidth, EfbHeight, 2),
			Widen("the screenshot " + file + " was not saved").c_str());

		Report::Image(title, file, text);
	}
}
