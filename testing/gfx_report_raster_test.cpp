// Rendered-image galleries for the Pixel Engine (PE), the Setup Unit (SU) and the rasterizers (RAS).
//
// The PE gallery varies the per-pixel operations at the end of the pipeline: the blend factors and
// equations, the logic operations, the depth comparison, the colour update mask and the copy
// engine's clear. The rasterizer gallery varies what reaches the PE at all: the primitive, the
// culling mode and the texture map a TEV stage is bound to.
//
// See specs: gfx-pe.md 6 (the registers, the blend factor tables, the copy engine), gfx-su.md 4.1
// (the culling mode), gfx-ras1.md 4.4 (the per-stage texture bindings).

#include "pch.h"
#include "gfx_test_common.h"
#include "gfx_report_common.h"

#include <vector>

using namespace GfxUnitTest;

namespace pureikyubutest
{
	namespace
	{
		using namespace GfxGallery;

		//! Where the rasterizer gallery textures live in the emulated main memory.
		const uint32_t MapTexAddr = 0x00300000;

		//! A quad in clip space at a given depth, so a gallery can put two quads at two depths.
		void DrawClipQuadZ(GfxTestMachine& m, float x0, float y0, float x1, float y1, float z,
			const Rgba& bl, const Rgba& br, const Rgba& tr, const Rgba& tl)
		{
			GFX::Vertex quad[4];
			quad[0] = GfxTestMachine::MakeVertex(x0, y0, z, bl.R, bl.G, bl.B, bl.A);
			quad[1] = GfxTestMachine::MakeVertex(x1, y0, z, br.R, br.G, br.B, br.A);
			quad[2] = GfxTestMachine::MakeVertex(x1, y1, z, tr.R, tr.G, tr.B, tr.A);
			quad[3] = GfxTestMachine::MakeVertex(x0, y1, z, tl.R, tl.G, tl.B, tl.A);
			m.DrawQuad(quad);
		}

		//! The destination of the blend galleries: a four-corner gradient whose alpha ramps as well, so
		//! that the destination-alpha blend factors have something to work with.
		void DrawBlendDestination(GfxTestMachine& m)
		{
			DrawScreenQuad(m, MakeRgba(0xC0, 0x30, 0x30, 0xFF), MakeRgba(0x30, 0xC0, 0x40, 0xC0),
				MakeRgba(0x30, 0x40, 0xC0, 0x80), MakeRgba(0xC0, 0xC0, 0x30, 0x40));
		}

		//! The source of the blend galleries: a gradient with an alpha ramp of its own, drawn over the
		//! middle of the screen so that the destination stays visible around it.
		void DrawBlendSource(GfxTestMachine& m)
		{
			DrawClipQuad(m, -0.75f, -0.75f, 0.75f, 0.75f,
				MakeRgba(0x20, 0x80, 0xFF, 0x20), MakeRgba(0xFF, 0xFF, 0x40, 0xFF),
				MakeRgba(0xFF, 0x20, 0x80, 0xE0), MakeRgba(0x40, 0xFF, 0xC0, 0x60));
		}

		//! The PE_CMODE0 word: the blend and logic controls, the two update masks and the dither bit.
		uint32_t ColorMode(int blendEn, int sfactor, int dfactor, int blendOp, int logopEn, int logop,
			int colMask = 1, int alphaMask = 1, int dither = 0)
		{
			return (uint32_t)blendEn | ((uint32_t)logopEn << 1) | ((uint32_t)dither << 2) |
				((uint32_t)colMask << 3) | ((uint32_t)alphaMask << 4) |
				((uint32_t)(dfactor & 7) << 5) | ((uint32_t)(sfactor & 7) << 8) |
				((uint32_t)blendOp << 11) | ((uint32_t)(logop & 0xF) << 12);
		}

		//! A full screen quad whose eight texture coordinate pairs are shifted copies of the first one,
		//! used by the RAS1_TREF gallery to make every coordinate pair select a different texel.
		void DrawTexCoordQuad(GfxTestMachine& m)
		{
			GFX::Vertex quad[4];
			quad[0] = GfxTestMachine::MakeVertex(-1, -1, 0, 0xFF, 0xFF, 0xFF, 0xFF);
			quad[1] = GfxTestMachine::MakeVertex(1, -1, 0, 0xFF, 0xFF, 0xFF, 0xFF);
			quad[2] = GfxTestMachine::MakeVertex(1, 1, 0, 0xFF, 0xFF, 0xFF, 0xFF);
			quad[3] = GfxTestMachine::MakeVertex(-1, 1, 0, 0xFF, 0xFF, 0xFF, 0xFF);

			for (int i = 0; i < 8; i++)
			{
				float offset = (float)i * 0.125f;
				quad[0].TexCoord[i][0] = 0.0f + offset;  quad[0].TexCoord[i][1] = 0.0f + offset;
				quad[1].TexCoord[i][0] = 1.0f + offset;  quad[1].TexCoord[i][1] = 0.0f + offset;
				quad[2].TexCoord[i][0] = 1.0f + offset;  quad[2].TexCoord[i][1] = 1.0f + offset;
				quad[3].TexCoord[i][0] = 0.0f + offset;  quad[3].TexCoord[i][1] = 1.0f + offset;
			}

			m.DrawQuad(quad);
		}
	}

	TEST_CLASS(GfxReportPeTests)
	{
	public:

		// One picture per (source factor, destination factor) pair of PE_CMODE0 (gfx-pe.md 6.2). The
		// scene is always the same: a gradient destination covering the screen and a gradient source
		// with an alpha ramp of its own drawn over the middle of it.
		TEST_METHOD(Report_PeBlendFactors)
		{
			GfxTestMachine& m = M();

			Report::Section("PE blend factors",
				"The same scene through twelve source/destination factor pairs: a four-corner destination gradient\n"
				"covering the whole screen and a source gradient with its own alpha ramp in the middle of it. The\n"
				"factors are the eight Flipper ones (zero, one, source colour, one minus source colour, source alpha,\n"
				"one minus source alpha, destination alpha, one minus destination alpha); the four that differ between\n"
				"the source and the destination table are the interesting ones.");

			struct Case
			{
				const char* file;
				const char* title;
				int sfactor;
				int dfactor;
				const char* note;
			};

			const Case cases[] = {
				{ "pe_blend_zero_one.png", "src * 0 + dst * 1", 0, 1,
					"The source is invisible: a blend that writes nothing." },
				{ "pe_blend_one_zero.png", "src * 1 + dst * 0", 1, 0,
					"The destination is replaced outright, so this is the source picture." },
				{ "pe_blend_one_one.png", "src + dst", 1, 1,
					"Both factors are one: the two pictures are added and the bright corners saturate." },
				{ "pe_blend_srccolor_zero.png", "src * src", 2, 0,
					"The source colour multiplies itself, which darkens it." },
				{ "pe_blend_srccolor_invdst.png", "src * src + dst * (1 - dst)", 2, 3,
					"The destination's own colour weights the source, and its complement weights the destination." },
				{ "pe_blend_srcalpha_invsrcalpha.png", "src * a + dst * (1 - a)", 4, 5,
					"The classic alpha blend: the source alpha of every corner decides the mix." },
				{ "pe_blend_srcalpha_one.png", "src * a + dst", 4, 1,
					"The additive form of the alpha blend, which saturates where the source is bright." },
				{ "pe_blend_invsrcalpha_srcalpha.png", "src * (1 - a) + dst * a", 5, 4,
					"The mix the other way round: the source shows through where its alpha is low." },
				{ "pe_blend_dstalpha_zero.png", "src * dst_a", 6, 0,
					"The destination alpha, which ramps from the top of the screen to the bottom, scales the source." },
				{ "pe_blend_zero_dstcolor.png", "dst * dst", 0, 2,
					"The destination multiplies itself, which darkens the background." },
				{ "pe_blend_invsrc_one.png", "src * (1 - src) + dst", 3, 1,
					"The source colour weights itself with its complement: mid-tones survive, extremes vanish." },
				{ "pe_blend_invdstalpha_one.png", "src * (1 - dst_a) + dst", 7, 1,
					"The destination alpha again, inverted, so the source fades in from the top." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0x20, 0x20, 0x28));

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BeginFrame();

				// The destination first, with the blending off
				m.BpLoad(PE_CMODE0_ID, ColorMode(0, 0, 0, 0, 0, 0));
				DrawBlendDestination(m);

				// ... and then the source with the factors of this picture
				m.BpLoad(PE_CMODE0_ID, ColorMode(1, c.sfactor, c.dfactor, 0, 0, 0));
				DrawBlendSource(m);

				Publish(m, c.title, c.file, c.note);
			}
		}

		// The blend equation: the same factors added or subtracted (gfx-pe.md 6.2, PE_CMODE0.blendop).
		TEST_METHOD(Report_PeBlendEquations)
		{
			GfxTestMachine& m = M();

			Report::Section("PE blend equations",
				"The same factors (source colour and one) through the two blend equations the Flipper defines: add and\n"
				"subtract, where the subtract equation is the destination minus the source. The scene is the one of the\n"
				"blend factor gallery.");

			struct Case
			{
				const char* file;
				const char* title;
				int blendOp;
				const char* note;
			};

			const Case cases[] = {
				{ "pe_blendop_add.png", "add: src * src + dst", 0,
					"The destination is added to the squared source, so the bright parts of the source saturate." },
				{ "pe_blendop_subtract.png", "subtract: dst - src * src", 1,
					"The squared source is taken away from the destination: the bright parts of the source cut the\n"
					"deepest hole in the picture." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0x20, 0x20, 0x28));

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BeginFrame();

				m.BpLoad(PE_CMODE0_ID, ColorMode(0, 0, 0, 0, 0, 0));
				DrawBlendDestination(m);

				m.BpLoad(PE_CMODE0_ID, ColorMode(1, 2, 1, c.blendOp, 0, 0));
				DrawBlendSource(m);

				Publish(m, c.title, c.file, c.note);
			}
		}

		// PE_CMODE1.const_alpha replaces the destination alpha factor by a constant (gfx-pe.md 6.3).
		TEST_METHOD(Report_PeConstAlpha)
		{
			GfxTestMachine& m = M();

			Report::Section("The constant alpha factor",
				"The destination-alpha factor of the alpha blend, replaced by the PE_CMODE1 constant. With the\n"
				"constant enabled the mix is the same everywhere; without it the destination alpha of the scene\n"
				"decides, which is why only the first picture changes from the top of the screen to the bottom.");

			struct Case
			{
				const char* file;
				const char* title;
				int constAlphaEn;
				int constAlpha;
				const char* note;
			};

			const Case cases[] = {
				{ "pe_constalpha_off.png", "the constant is disabled", 0, 0,
					"The destination alpha (which ramps down the screen) is the factor." },
				{ "pe_constalpha_40.png", "constant alpha 0x40", 1, 0x40,
					"A quarter of the source reaches the result, evenly over the whole source quad." },
				{ "pe_constalpha_c0.png", "constant alpha 0xC0", 1, 0xC0,
					"Three quarters of the source: the same mix, much closer to the source picture." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0x20, 0x20, 0x28));

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BeginFrame();

				m.BpLoad(PE_CMODE0_ID, ColorMode(0, 0, 0, 0, 0, 0));
				DrawBlendDestination(m);

				// sfactor = the source alpha, dfactor = the destination alpha (or the constant)
				m.BpLoad(PE_CMODE1_ID, (uint32_t)c.constAlpha | ((uint32_t)c.constAlphaEn << 8));
				m.BpLoad(PE_CMODE0_ID, ColorMode(1, 4, 6, 0, 0, 0));
				DrawBlendSource(m);

				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// The logic operations
		// =========================================================================================

		// One picture per logic operation (gfx-pe.md 6.2). The operation replaces the blend and combines
		// the source with the destination bit by bit.
		TEST_METHOD(Report_PeLogicOps)
		{
			GfxTestMachine& m = M();

			Report::Section("PE logic operations",
				"One picture per logic op of PE_CMODE0. The operation combines the source with the destination bit by\n"
				"bit; the destination here is a colour gradient with a checkerboard of bright tiles over it and the\n"
				"source is a gradient quad in the middle of the screen, so the sixteen operations produce sixteen\n"
				"different pictures. Clear and set are the two that ignore both operands, noop and copy are the two\n"
				"that keep only one of them.");

			const char* names[16] = {
				"clear", "and", "and reverse", "copy", "and inverted", "noop", "xor", "or",
				"nor", "equiv", "invert", "or reverse", "copy inverted", "or inverted", "nand", "set",
			};
			const char* files[16] = {
				"pe_logop_clear.png", "pe_logop_and.png", "pe_logop_andrev.png", "pe_logop_copy.png",
				"pe_logop_andinv.png", "pe_logop_noop.png", "pe_logop_xor.png", "pe_logop_or.png",
				"pe_logop_nor.png", "pe_logop_equiv.png", "pe_logop_invert.png", "pe_logop_orrev.png",
				"pe_logop_copyinv.png", "pe_logop_orinv.png", "pe_logop_nand.png", "pe_logop_set.png",
			};

			for (int op = 0; op < 16; op++)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BeginFrame();

				// The destination: a gradient with a checkerboard of bright tiles over it
				m.BpLoad(PE_CMODE0_ID, ColorMode(0, 0, 0, 0, 0, 0));

				DrawScreenQuad(m, MakeRgba(0x18, 0x18, 0x40), MakeRgba(0x80, 0x18, 0x18),
					MakeRgba(0x18, 0x80, 0x18), MakeRgba(0x80, 0x80, 0x18));

				for (int cy = 0; cy < 4; cy++)
					for (int cx = 0; cx < 4; cx++)
						if (((cx + cy) & 1) == 0)
							DrawPixelRect(m, (float)(cx * 160), (float)(cy * 120),
								(float)(cx * 160 + 160), (float)(cy * 120 + 120), MakeRgba(0xE0, 0xE0, 0xE0));

				// ... and the source with the logic op under test
				m.BpLoad(PE_CMODE0_ID, ColorMode(0, 0, 0, 0, 1, op));
				DrawClipQuad(m, -0.7f, -0.7f, 0.7f, 0.7f, MakeRgba(0x30, 0x30, 0xFF),
					MakeRgba(0xFF, 0x30, 0x30), MakeRgba(0xFF, 0xFF, 0x30), MakeRgba(0x30, 0xFF, 0x30));

				Publish(m, std::string("logic op: ") + names[op], files[op],
					std::string("PE_CMODE0.logop = ") + std::to_string(op) + " (" + names[op] +
					"). The source quad covers the middle of the picture, so the two operands are the\n"
					"source gradient and the checkerboard destination under it.");
			}
		}

		// =========================================================================================
		// The depth test
		// =========================================================================================

		// One picture per comparison function (gfx-pe.md 6.1). Every picture is a grid of six cells, each
		// with a destination quad and a source quad at two depths, so one picture is the whole truth table
		// of the function.
		TEST_METHOD(Report_PeZCompare)
		{
			GfxTestMachine& m = M();

			Report::Section("PE depth comparison functions",
				"One picture per PE_ZMODE.func value. Every picture is the same grid of six cells; in each cell a warm\n"
				"destination quad is drawn first and a cool source quad after it, at two different depths. The\n"
				"comparison function decides whether the source replaces the destination, so the pattern of cells where\n"
				"the cool colour survived is the truth table of the function:\n"
				"  0 never, 1 less, 2 equal, 3 less or equal, 4 greater, 5 not equal, 6 greater or equal, 7 always.");

			// The six depth pairs of the grid, as clip-space z (the window depth is (z + 1) / 2)
			const float srcDepth[6] = { -0.6f, 0.0f, 0.6f, 0.0f, 0.6f, -0.6f };
			const float dstDepth[6] = { 0.6f, 0.0f, -0.6f, 0.6f, 0.0f, 0.0f };

			for (int func = 0; func < 8; func++)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0, 0, 0), 0xFFFFFF);

				m.BeginFrame();

				for (int cell = 0; cell < 6; cell++)
				{
					float cx = -1.0f + 0.34f + (float)(cell % 3) * 0.66f;
					float cy = -1.0f + 0.34f + (float)(cell / 3) * 1.0f;

					// The destination: the depth test writes it unconditionally
					m.BpLoad(PE_ZMODE_ID, 1u | (7u << 1) | (1u << 4));
					DrawClipQuadZ(m, cx - 0.3f, cy - 0.44f, cx + 0.3f, cy + 0.44f, dstDepth[cell],
						MakeRgba(0xC0, 0x40, 0x20), MakeRgba(0xFF, 0xC0, 0x40),
						MakeRgba(0xE0, 0x60, 0x20), MakeRgba(0xFF, 0x80, 0x30));

					// ... and the source with the function under test
					m.BpLoad(PE_ZMODE_ID, 1u | ((uint32_t)func << 1) | (1u << 4));
					DrawClipQuadZ(m, cx - 0.3f, cy - 0.44f, cx + 0.3f, cy + 0.44f, srcDepth[cell],
						MakeRgba(0x20, 0x60, 0xFF), MakeRgba(0x20, 0xFF, 0xFF),
						MakeRgba(0x40, 0x40, 0xE0), MakeRgba(0x80, 0xFF, 0xFF));
				}

				char title[64];
				sprintf_s(title, "PE_ZMODE.func = %d", func);
				Publish(m, title, std::string("pe_zfunc_") + std::to_string(func) + ".png",
					"The cells are, left to right and top to bottom: the source nearer than the destination, the two\n"
					"at the same depth, the source farther away, then the same three pairs with the other depth\n"
					"order. A cool cell is one where the source replaced the destination.");
			}
		}

		// =========================================================================================
		// The colour update mask
		// =========================================================================================

		// PE_CMODE0.col_mask is an "update enabled" flag: with it clear, a draw leaves the colour plane
		// of the EFB untouched (gfx-pe.md 6.2).
		TEST_METHOD(Report_PeWriteMask)
		{
			GfxTestMachine& m = M();

			Report::Section("The colour update mask",
				"Three pictures of the same two-quad scene, drawn with different PE_CMODE0.col_mask settings. The\n"
				"first quad covers the left half of the screen and the second one the right half; with the colour\n"
				"update disabled for a draw, that draw leaves no trace at all in the EFB.");

			struct Case
			{
				const char* file;
				const char* title;
				int maskFirst;
				int maskSecond;
				const char* note;
			};

			const Case cases[] = {
				{ "pe_colmask_both.png", "colour update enabled for both draws", 1, 1,
					"Both quads are written, so the left and the right half of the picture are painted." },
				{ "pe_colmask_first_off.png", "colour update disabled for the first draw", 0, 1,
					"The left quad was drawn with the update off, so only the right half of the picture exists." },
				{ "pe_colmask_second_off.png", "colour update disabled for the second draw", 1, 0,
					"The other way round: the left half survives and the right one is missing." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0x18, 0x18, 0x20));

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BeginFrame();

				m.BpLoad(PE_CMODE0_ID, ColorMode(0, 0, 0, 0, 0, 0, c.maskFirst));
				DrawClipQuad(m, -1, -1, 0, 1, MakeRgba(0xC0, 0x20, 0x20), MakeRgba(0xFF, 0x80, 0x20),
					MakeRgba(0xE0, 0x40, 0x20), MakeRgba(0x90, 0x10, 0x10));

				m.BpLoad(PE_CMODE0_ID, ColorMode(0, 0, 0, 0, 0, 0, c.maskSecond));
				DrawClipQuad(m, 0, -1, 1, 1, MakeRgba(0x20, 0x20, 0xC0), MakeRgba(0x20, 0xA0, 0xFF),
					MakeRgba(0x30, 0xFF, 0xE0), MakeRgba(0x10, 0x10, 0x90));

				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// The copy engine's clear
		// =========================================================================================

		// PE_COPY_CMD with the clear bit fills the rectangle of PE_XBOUND / PE_YBOUND with the PE clear
		// colours (gfx-pe.md 6.15, 6.17). In this backend the clear of the frame a copy command belongs
		// to is performed by the next frame begin, which is what the pictures show: the clear is in the
		// frame and the scene is drawn over it.
		TEST_METHOD(Report_PeCopyClear)
		{
			GfxTestMachine& m = M();

			Report::Section("The copy engine's clear",
				"PE_COPY_CMD with the clear bit set fills the rectangle of PE_XBOUND / PE_YBOUND with the PE clear\n"
				"colours. Each test sets the bounds and the colour, issues the command and then draws one gradient quad\n"
				"over the middle of the screen, so every picture shows the cleared rectangle, the frame clear colour\n"
				"around it and the quad on top of both.");

			struct Case
			{
				const char* file;
				const char* title;
				int left, top, right, bottom;
				Rgba clearColor;
				const char* note;
			};

			const Case cases[] = {
				{ "pe_copyclear_full.png", "the whole EFB", 0, 0, 639, 479, MakeRgba(0x20, 0x80, 0x40),
					"Bounds 0,0 - 639,479: the clear covers the whole frame, so the quad is the only thing drawn\n"
					"over a green field." },
				{ "pe_copyclear_left.png", "the left half", 0, 0, 319, 479, MakeRgba(0x90, 0x30, 0x20),
					"The right edge of the cleared region is a straight vertical line at the middle of the screen." },
				{ "pe_copyclear_rect.png", "a centred rectangle", 160, 120, 480, 360, MakeRgba(0x20, 0x40, 0x90),
					"All four bounds are inside the screen, so the cleared rectangle has four visible edges." },
				{ "pe_copyclear_quadrant.png", "the top right quadrant", 320, 0, 639, 239, MakeRgba(0x90, 0x80, 0x20),
					"The bounds do not have to start at the origin." },
				{ "pe_copyclear_strip.png", "a horizontal strip", 0, 200, 639, 280, MakeRgba(0x80, 0x20, 0x80),
					"A strip across the whole width: its two horizontal edges are visible." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0x18, 0x28, 0x30));

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BpLoad(PE_CMODE0_ID, ColorMode(0, 0, 0, 0, 0, 0));

				// The copy command arms the clear; the frame begin that follows performs it
				m.BpLoad(PE_COPY_CLEAR_AR_ID, (uint32_t)c.clearColor.R | (0xFFu << 8));
				m.BpLoad(PE_COPY_CLEAR_GB_ID, (uint32_t)c.clearColor.B | ((uint32_t)c.clearColor.G << 8));
				m.BpLoad(PE_XBOUND_ID, (uint32_t)c.left | ((uint32_t)c.right << 10));
				m.BpLoad(PE_YBOUND_ID, (uint32_t)c.top | ((uint32_t)c.bottom << 10));
				m.BpLoad(PE_COPY_CMD_ID, 1u << 11);

				m.BeginFrame();

				// The scene on top of the cleared frame
				DrawClipQuad(m, -0.7f, -0.5f, 0.7f, 0.5f, MakeRgba(0x40, 0x40, 0x40),
					MakeRgba(0xC0, 0xC0, 0xC0), MakeRgba(0xF0, 0xF0, 0xF0), MakeRgba(0x90, 0x90, 0x90));

				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// The primitives
		// =========================================================================================

		// The eight primitives the rasterizer accepts (gfx-xf.md 2.2, ras.cpp). Every picture draws the
		// same eight vertices as the primitive under test, over the gallery background.
		TEST_METHOD(Report_RasPrimitives)
		{
			GfxTestMachine& m = M();

			Report::Section("Rasterizer primitives",
				"One picture per primitive type. The background is the gallery gradient; on top of it the same eight\n"
				"vertices - the corners of two squares, each one in its own colours - are drawn as the primitive under\n"
				"test. Lines and points are one pixel wide here because SU_LPSIZE resets to zero, which the backend\n"
				"floors at the one pixel GL can draw; the line/point size gallery next to this one shows the register\n"
				"in use.");

			struct Case
			{
				const char* file;
				const char* title;
				GFX::RAS_Primitive prim;
				const char* note;
			};

			const Case cases[] = {
				{ "ras_prim_quads.png", "quads (two per draw)", GFX::RAS_QUAD,
					"Every group of four vertices is one quad, in the fan order the hardware uses." },
				{ "ras_prim_quadstrip.png", "quad strip", GFX::RAS_QUAD_STRIP,
					"Every pair of vertices starts a new quad with the pair before it, so the two squares are joined\n"
					"by a bridge of two more quads." },
				{ "ras_prim_triangles.png", "triangles", GFX::RAS_TRIANGLE,
					"Every three vertices are one triangle: the second triangle is the bridge between the two\n"
					"squares, and the last two vertices are left over." },
				{ "ras_prim_tristrip.png", "triangle strip", GFX::RAS_TRIANGLE_STRIP,
					"Every vertex after the second closes a new triangle with the two before it." },
				{ "ras_prim_trifan.png", "triangle fan", GFX::RAS_TRIANGLE_FAN,
					"Every vertex after the second closes a triangle with the previous one and the first." },
				{ "ras_prim_lines.png", "lines", GFX::RAS_LINE,
					"Each pair of vertices is one line segment, so there are four of them." },
				{ "ras_prim_linestrip.png", "line strip", GFX::RAS_LINE_STRIP,
					"A polyline: every vertex after the first extends the line." },
				{ "ras_prim_points.png", "points", GFX::RAS_POINT,
					"Eight single pixels, one per vertex." },
			};

			// The eight vertices: the corners of two squares, in the fan order of a quad
			std::vector<GFX::Vertex> vertices;

			for (int group = 0; group < 2; group++)
			{
				static const float px[4] = { 0.0f, 0.7f, 0.7f, 0.0f };
				static const float py[4] = { 0.0f, 0.0f, 0.8f, 0.8f };

				for (int i = 0; i < 4; i++)
				{
					Rgba color = GridColor(group * 4 + i);
					vertices.push_back(GfxTestMachine::MakeVertex(-0.85f + group * 0.95f + px[i],
						-0.45f + py[i], 0, color.R, color.G, color.B, color.A));
				}
			}

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BpLoad(PE_CMODE0_ID, ColorMode(0, 0, 0, 0, 0, 0));
				m.BeginFrame();

				DrawBackground(m);
				m.DrawPrimitive(c.prim, vertices.data(), vertices.size());

				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// Culling
		// =========================================================================================

		// GEN_MODE.reject_en rejects the triangles of one winding (gfx-su.md 4.1, su.cpp). The scene is
		// two triangles that share their shape but are wound the other way round, so which colour
		// survives tells which winding the mode rejects.
		TEST_METHOD(Report_RasCullModes)
		{
			GfxTestMachine& m = M();

			Report::Section("Culling modes",
				"GEN_MODE.reject_en, the front/back rejection of the setup unit. The scene has two triangles: the\n"
				"same three vertices in the two possible orders, so exactly one of them is the winding a mode\n"
				"rejects. The pictures show which colour survives, and therefore which winding each mode keeps.");

			struct Case
			{
				const char* file;
				const char* title;
				int reject;
				const char* note;
			};

			const Case cases[] = {
				{ "ras_cull_none.png", "reject_en = 0 (no culling)", 0,
					"Both triangles are drawn, so the picture shows one colour over the other." },
				{ "ras_cull_front.png", "reject_en = 1 (reject front)", 1,
					"One winding disappears; the colour that is left is the winding this mode keeps." },
				{ "ras_cull_back.png", "reject_en = 2 (reject back)", 2,
					"The other winding disappears." },
				{ "ras_cull_all.png", "reject_en = 3 (reject both)", 3,
					"Nothing survives, so the picture is the background alone." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BpLoad(PE_CMODE0_ID, ColorMode(0, 0, 0, 0, 0, 0));
				m.BeginFrame();

				// This gallery uses the second background: its "everything culled" picture is otherwise
				// the same pixels as the alpha function gallery's "never passes" picture
				DrawBackgroundAlt(m);

				// reject_en lives in GEN_MODE, which also carries the TEV stage count, so both have to be
				// written in the same register load
				m.BpLoad(GEN_MODE_ID, (uint32_t)c.reject << 14);

				// The two windings of the same triangle: the first one is counter-clockwise in window
				// coordinates, the second one (the same shape, the two base vertices swapped) clockwise
				GFX::Vertex ccw[3] = {
					GfxTestMachine::MakeVertex(-0.8f, -0.6f, 0, 0xFF, 0xFF, 0x20, 0xFF),
					GfxTestMachine::MakeVertex(0.1f, -0.6f, 0, 0xFF, 0xFF, 0x20, 0xFF),
					GfxTestMachine::MakeVertex(-0.35f, 0.5f, 0, 0xFF, 0xFF, 0x20, 0xFF),
				};
				GFX::Vertex cw[3] = {
					GfxTestMachine::MakeVertex(-0.1f, -0.6f, 0, 0xFF, 0x20, 0xFF, 0xFF),
					GfxTestMachine::MakeVertex(0.35f, 0.5f, 0, 0xFF, 0x20, 0xFF, 0xFF),
					GfxTestMachine::MakeVertex(0.8f, -0.6f, 0, 0xFF, 0x20, 0xFF, 0xFF),
				};

				m.DrawPrimitive(GFX::RAS_TRIANGLE, ccw, 3);
				m.DrawPrimitive(GFX::RAS_TRIANGLE, cw, 3);

				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// The per-stage texture bindings (RAS1_TREF)
		// =========================================================================================

		// RAS1_TREFn holds the texture binding of the stage pair 2n/2n+1: ti0/ti1 select the texture map
		// the stage samples and tc0/tc1 which of the eight texture coordinates it uses (gfx-ras1.md 4.4).
		// The gallery draws one picture per texture coordinate selector, which is the selector that can
		// be varied without loading several maps (the map selector has a problem of its own, see the
		// notes on the report).
		TEST_METHOD(Report_RasTrefBindings)
		{
			GfxTestMachine& m = M();

			Report::Section("The texture binding of a TEV stage (RAS1_TREF)",
				"RAS1_TREF0.tc0 selects which of the eight texture coordinates a stage samples. The quad of these\n"
				"pictures carries eight texture coordinate pairs and every pair is the first one shifted by 1/8 of\n"
				"the texture, so the picture of a selector is the pattern of that coordinate pair. The pictures of\n"
				"RAS1_TREF0.ti0 program several maps with a texture of their own and vary the map the stage samples,\n"
				"one of them from the second register block (0xA0-0xBB) that programs the maps 4-7. The last picture\n"
				"draws two stages, bound to two different coordinate pairs of the same map, and mixes them.");

			// The texture coordinate selector: an 8x8 texture with a different colour in every texel, and a
			// quad whose eight texture coordinate pairs walk across it
			{
				std::vector<uint8_t> raw;

				for (int t = 0; t < 8; t += 4)
					for (int s = 0; s < 8; s += 4)
						for (int v = 0; v < 4; v++)
							for (int u = 0; u < 4; u++)
							{
								int x = s + u, y = t + v;
								int r = (x * 32) >> 3;			// the 5-bit red field
								int g = (y * 32) >> 2;			// the 6-bit green field
								int b = (128 + ((x + y) & 7) * 16) >> 3;
								uint16_t texel = (uint16_t)((r << 11) | ((g & 0x3F) << 5) | (b & 0x1F));
								raw.push_back((uint8_t)(texel >> 8));
								raw.push_back((uint8_t)texel);
							}

				// The sampler has to repeat: the shifted coordinate pairs run past the edge of the texture
				SetupTexture(m, 0, MapTexAddr, 8, 8, GFX::TF_RGB565, raw.data(), raw.size(),
					1u | (1u << 2));						// wrap_s = wrap_t = repeat
			}

			for (int tc = 0; tc < 8; tc++)
			{
				SetupPassThroughXF(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				m.BpLoad(GEN_MODE_ID, 0);
				m.BpLoad(RAS1_TREF0_ID, 0u | ((uint32_t)tc << 3) | (1u << 6));	// ti0 = 0, tc0 = tc, te0 = 1
				m.BpLoad(TEV_COLOR_ENV_0_ID, ColorEnv(CZERO, CTEX, CONE, CZERO));
				m.BpLoad(TEV_ALPHA_ENV_0_ID, AlphaEnv(ATEX, AZERO, AZERO, ATEX));
				m.BpLoad(PE_ZMODE_ID, 0);
				m.BpLoad(PE_CMODE0_ID, ColorMode(0, 0, 0, 0, 0, 0));
				m.BeginFrame();

				DrawTexCoordQuad(m);

				Publish(m, std::string("RAS1_TREF0.tc0 = ") + std::to_string(tc),
					std::string("ras_tref_tc") + std::to_string(tc) + ".png",
					"The stage sampled the texture coordinate pair number " + std::to_string(tc) +
					", which is the first one shifted by\n" + std::to_string(tc) +
					"/8 of the texture: the picture is the 8x8 texel pattern of that coordinate pair.");
			}

			// Two stages, two texture coordinates of the same map
			SetupPassThroughXF(m);
			SetClearColor(m, MakeRgba(0, 0, 0));

			m.BpLoad(GEN_MODE_ID, 1u << 10);										// two TEV stages
			m.BpLoad(RAS1_TREF0_ID, 0u | (1u << 6) | (3u << 15) | (1u << 18));	// stage 0: tc 0, stage 1: tc 3
			m.BpLoad(TEV_COLOR_ENV_0_ID, ColorEnv(CZERO, CTEX, CONE, CZERO));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, AlphaEnv(ATEX, AZERO, AZERO, ATEX));
			m.BpLoad(TEV_COLOR_ENV_1_ID, ColorEnv(CREG0, CTEX, CHALF, CZERO));
			m.BpLoad(TEV_ALPHA_ENV_1_ID, AlphaEnv(ATEX, AZERO, AZERO, ATEX));
			m.BpLoad(PE_ZMODE_ID, 0);
			m.BpLoad(PE_CMODE0_ID, ColorMode(0, 0, 0, 0, 0, 0));
			m.BeginFrame();

			DrawTexCoordQuad(m);

			Publish(m, "two stages, two texture coordinates", "ras_tref_two_stages.png",
				"Both stages sample the same map, but the second one was bound to the texture coordinate pair 3\n"
				"while the first one uses pair 0; the second stage mixes its texel into the result of the first one\n"
				"with the half constant, so the picture is a blend of two differently shifted patterns.");

			// The texture map selector: one picture per map, each map carrying a texture of its own. The map
			// 5 is programmed through the I4-I7 register block (0xA0-0xBB), so that picture also shows that
			// the block addresses the maps 4-7 and not the maps 0-3 again.
			{
				const int maps[] = { 0, 1, 2, 3, 5 };

				for (int mi = 0; mi < (int)(sizeof(maps) / sizeof(maps[0])); mi++)
				{
					int map = maps[mi];
					std::vector<uint8_t> raw;

					for (int y = 0; y < 8; y++)
						for (int x = 0; x < 8; x++)
						{
							int r, g, b;

							switch (map)
							{
								case 0:  r = x * 36; g = 0; b = 0; break;						// red ramp
								case 1:  r = 0; g = y * 36; b = 0; break;						// green ramp
								case 2:  r = 0; g = 0; b = ((x + y) & 7) * 36; break;			// blue diagonal
								case 3:  r = ((x ^ y) & 1) ? 255 : x * 32; g = r; b = r; break;	// checker over a ramp
								default: r = 255; g = ((x + y) & 1) ? 255 : y * 32; b = 255; break;	// magenta checker
							}

							uint16_t texel = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
							raw.push_back((uint8_t)(texel >> 8));
							raw.push_back((uint8_t)texel);
						}

					SetupTexture(m, map, MapTexAddr + map * 0x10000, 8, 8, GFX::TF_RGB565,
						raw.data(), raw.size(), 0);

					SetupPassThroughXF(m);
					SetClearColor(m, MakeRgba(0, 0, 0));

					SetupTextureStage0(m, map);						// ti0 = map, tc0 = 0, te0 = 1
					m.BpLoad(PE_ZMODE_ID, 0);
					m.BpLoad(PE_CMODE0_ID, ColorMode(0, 0, 0, 0, 0, 0));
					m.BeginFrame();

					DrawTexCoordQuad(m);

					Publish(m, std::string("RAS1_TREF0.ti0 = ") + std::to_string(map),
						std::string("ras_tref_ti") + std::to_string(map) + ".png",
						"The stage sampled the texture map " + std::to_string(map) + ", which holds the pattern of "
						"this\npicture: the map selector of the stage is what decides which texture is sampled.");
				}
			}
		}

		// =========================================================================================
		// The scissor rectangle of the setup unit
		// =========================================================================================

		// SU_SCIS0/SU_SCIS1 hold the scissor rectangle in screen coordinates (the origin is the top left
		// corner of the display) with a bias of 342 on every register value; everything outside the
		// rectangle is culled by the rasterizer (gfx-su.md 4.1, GX_SetScissor). The scene is the
		// gallery background with a full screen gradient quad on top of it, so where the quad survives
		// and where the background shows through is the rectangle itself.
		TEST_METHOD(Report_SuScissor)
		{
			GfxTestMachine& m = M();

			Report::Section("The scissor rectangle",
				"Every picture draws the gallery background and then a full screen colour quad through a scissor\n"
				"rectangle programmed in SU_SCIS0/SU_SCIS1. The rectangle is in screen coordinates with the origin at\n"
				"the top left corner of the display, so the first picture clips the quad to the middle of the screen:\n"
				"the background around it is what the scissor removed. The other pictures move and resize the box -\n"
				"an off-centre box shows that the Y axis is measured downwards, and the last one is the box hugging\n"
				"the top left corner with the register values at their smallest useful setting.");

			struct Case
			{
				const char* file;
				const char* title;
				int x, y, w, h;			// The scissor rectangle in screen coordinates
				const char* note;
			};

			const Case cases[] = {
				{ "su_scissor_centre.png", "a box in the middle of the screen", 150, 100, 340, 280,
					"The quad survives inside the box and the background shows through everywhere else." },
				{ "su_scissor_band.png", "a wide horizontal band", 0, 180, 640, 120,
					"A band that reaches both screen edges: only the top and the bottom are clipped." },
				{ "su_scissor_high.png", "a box in the upper right quadrant", 320, 40, 300, 160,
					"The box sits high on the screen, so the clip is at the top and on the left: the screen Y of the\n"
					"scissor counts downwards from the top edge." },
				{ "su_scissor_low.png", "a box in the lower left quadrant", 20, 280, 300, 160,
					"The mirrored box of the picture before: the same numbers put the clip at the bottom of the screen\n"
					"when they are counted from the top." },
				{ "su_scissor_wide.png", "a box that covers the whole screen", 0, 0, 640, 480,
					"The default of a scene that programs the rectangle to the screen: the quad is not clipped at all." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BpLoad(PE_CMODE0_ID, ColorMode(0, 0, 0, 0, 0, 0));
				m.BeginFrame();

				// The background first, so the clipped part of the quad is easy to see
				DrawBackground(m);

				// SU_SCIS0 packs suy in bits 11:0 and sux in bits 23:12, SU_SCIS1 packs suh and suw
				m.BpLoad(SU_SCIS0_ID, (uint32_t)(c.y + 342) | ((uint32_t)(c.x + 342) << 12));
				m.BpLoad(SU_SCIS1_ID, (uint32_t)(c.y + c.h - 1 + 342) | ((uint32_t)(c.x + c.w - 1 + 342) << 12));

				// The quad the scissor clips, in one strong colour pair
				DrawScreenQuad(m, MakeRgba(0xF0, 0xF0, 0x20), MakeRgba(0x20, 0xF0, 0xF0),
					MakeRgba(0xF0, 0x20, 0xF0), MakeRgba(0x20, 0xF0, 0x40));

				Publish(m, c.title, c.file, c.note);

				// Put the default scissor back for the next picture (the register writes above would
				// otherwise be inherited by it, which is exactly what a scissor picture must not do)
				m.BpLoad(SU_SCIS0_ID, 342 | (342 << 12));
				m.BpLoad(SU_SCIS1_ID, (342 + EfbHeight - 1) | ((342 + EfbWidth - 1) << 12));
			}
		}

		// =========================================================================================
		// The line and point size of the setup unit
		// =========================================================================================

		// SU_LPSIZE holds the line and point size in 1/16 pixel increments (GX_SetLineWidth /
		// GX_SetPointSize, gfx-su.md 4.1 and 5.2). The lines are drawn as a fan of segments of every
		// colour of the palette and the points as a row of dots of growing size, so the width of the
		// register is visible on both primitive classes.
		TEST_METHOD(Report_SuLinePointSize)
		{
			GfxTestMachine& m = M();

			Report::Section("The line and point size",
				"SU_LPSIZE.lsize and SU_LPSIZE.psize are the size of a line and of a point in 1/16 pixel\n"
				"increments. The first two pictures draw the same fan of coloured line segments with a size of one\n"
				"pixel and of eight pixels; the last two draw the same row of points at four and at sixteen pixels.\n"
				"The size register resets to zero, which the backend floors at the one pixel GL always rasterizes.");

			struct Case
			{
				const char* file;
				const char* title;
				GFX::RAS_Primitive prim;
				int lsize;
				int psize;
				const char* note;
			};

			const Case cases[] = {
				{ "su_lpsize_line1.png", "lines, a size of 1 pixel", GFX::RAS_LINE, 16, 16,
					"lsize = 16 is one pixel: the segments are as thin as the rasterizer can draw them." },
				{ "su_lpsize_line8.png", "lines, a size of 8 pixels", GFX::RAS_LINE, 128, 16,
					"lsize = 128 is eight pixels, so the same fan is drawn with fat segments." },
				{ "su_lpsize_point4.png", "points, a size of 4 pixels", GFX::RAS_POINT, 16, 64,
					"psize = 64 is four pixels: every vertex becomes a 4 x 4 square." },
				{ "su_lpsize_point16.png", "points, a size of 16 pixels", GFX::RAS_POINT, 16, 255,
					"psize = 255 is 15.9 pixels, the largest size the 8-bit field can express: the squares overlap." },
			};

			// A fan of segments: eight vertices on a circle around the middle of the screen
			std::vector<GFX::Vertex> vertices;

			for (int i = 0; i < 8; i++)
			{
				float angle = (float)i * 3.14159265f / 4.0f;
				Rgba color = GridColor(i);
				vertices.push_back(GfxTestMachine::MakeVertex(0.75f * cosf(angle), 0.75f * sinf(angle), 0,
					color.R, color.G, color.B, color.A));
			}

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0x10, 0x10, 0x18));

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BpLoad(PE_CMODE0_ID, ColorMode(0, 0, 0, 0, 0, 0));
				m.BpLoad(SU_LPSIZE_ID, (uint32_t)(c.lsize & 0xFF) | ((uint32_t)(c.psize & 0xFF) << 8));

				m.BeginFrame();
				m.DrawPrimitive(c.prim, vertices.data(), vertices.size());

				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// Flat shading
		// =========================================================================================

		// GEN_MODE.flat_en gives the colour planes of a primitive zero gradients (gfx-ras2.md 3.1), so
		// its rasterized colour is constant (SetupUnit / the TEV program). Two triangles with three
		// different vertex colours each are drawn with and without the bit: interpolated the pictures
		// are gradients, flat they are blocks of one vertex colour.
		TEST_METHOD(Report_SuFlatShading)
		{
			GfxTestMachine& m = M();

			Report::Section("Flat shading",
				"GEN_MODE.flat_en makes the rasterized colour of a primitive constant, which the backend expresses\n"
				"by taking it from the provoking vertex instead of interpolating it. Four triangles with a different\n"
				"colour per vertex are drawn twice over the same background gradient: interpolated, the interior of\n"
				"every triangle is a blend of its corners; flat, each triangle is a single one of its vertex colours.");

			struct Case
			{
				const char* file;
				const char* title;
				int flat;
				const char* note;
			};

			const Case cases[] = {
				{ "su_flat_off.png", "flat_en = 0 (interpolated)", 0,
					"The colour of every pixel is the interpolation of the three vertex colours of its triangle." },
				{ "su_flat_on.png", "flat_en = 1 (flat shaded)", 1,
					"Each triangle is a single colour, taken from its provoking vertex, so the fan breaks into flat\n"
					"blocks instead of a gradient." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BpLoad(PE_CMODE0_ID, ColorMode(0, 0, 0, 0, 0, 0));

				m.BeginFrame();

				// The background is drawn interpolated in both pictures, so what changes is the triangles
				m.BpLoad(GEN_MODE_ID, 0);
				DrawBackground(m);

				m.BpLoad(GEN_MODE_ID, (uint32_t)c.flat << 8);

				for (int t = 0; t < 4; t++)
				{
					float x = -1.0f + 0.5f * (float)t;
					GFX::Vertex triangle[3] = {
						GfxTestMachine::MakeVertex(x, -0.8f, 0, 0xF0, 0x10, 0x10, 0xFF),
						GfxTestMachine::MakeVertex(x + 0.45f, -0.8f, 0, 0x10, 0xF0, 0x10, 0xFF),
						GfxTestMachine::MakeVertex(x + 0.22f, 0.8f, 0, 0x10, 0x10, 0xF0, 0xFF),
					};
					m.DrawPrimitive(GFX::RAS_TRIANGLE, triangle, 3);
				}

				Publish(m, c.title, c.file, c.note);
			}
		}
	};
}
