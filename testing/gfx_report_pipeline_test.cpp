// Rendered-image galleries for the TEV (Texture Environment Unit) and the indirect (bump) stages.
//
// Each test walks a table of TEV configurations and renders one full-EFB picture per configuration.
// The combine galleries draw a grid of cells: the column of a cell picks the host (rasterized) colour
// and its row picks the texel of the 8x8 test texture, so one picture shows the programmed combine
// over dozens of different operand pairs at once.
//
// See specs: gfx-tev.md 3.2 (the combine), 3.4 (K constants), 3.5 (the Z environment), 3.6 (fog),
// 3.8 (the alpha function), 4.2/4.8 (the registers) and gfx-bump.md 3.3-3.7 (indirect texturing).

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

		//! Where the TEV gallery textures live in the emulated main memory.
		const uint32_t GridTexAddr = 0x00100000;
		const uint32_t RampTexAddr = 0x00110000;

		// ---------------------------------------------------------------------------------------
		// The operand grid
		// ---------------------------------------------------------------------------------------

		//! The host colour of the cell (i, j): the column walks the red/blue balance, the row the green.
		Rgba GridHostColor(int i, int j)
		{
			return MakeRgba(24 + i * 30, 24 + j * 42, 230 - i * 28);
		}

		//! The host colour of the alpha galleries: the column carries the alpha (a multiple of 32, so the
		//! alpha compare modes have operand pairs that are exactly equal), the row the colour.
		Rgba GridAlphaColor(int i, int j)
		{
			return MakeRgba(60 + i * 24, 30 + j * 40, 200 - i * 20, 16 + i * 32);
		}

		//! Texel (x, y) of the 8x8 operand texture: the opposite ramp of the host colours, so the two
		//! operands of a cell are always different. The texel alpha walks with the row (permuted, so
		//! that its values meet the host alphas on a scattered set of cells).
		//!
		//! With `lowRange` every channel stays inside the low quarter of the range instead: a picture
		//! that multiplies the texel by four then keeps the steps of the ramp instead of saturating all
		//! of it, which is what the "shift left 2" case of the bias/shift gallery has to show.
		Rgba GridTexel(int x, int y, int w, int h, bool lowRange = false)
		{
			if (lowRange)
			{
				return MakeRgba(8 + (x * 56) / (w - 1), 8 + (y * 56) / (h - 1),
					8 + ((x + y) * 56) / (w + h - 2), 255);
			}

			int alpha = 16 + ((y * 3) % 8) * 32;

			return MakeRgba(230 - x * 28, 230 - y * 40, 24 + x * 30, alpha);
		}

		//! Encode the 8x8 operand texture in RGBA8 (the alpha/red tile pair comes first in memory).
		std::vector<uint8_t> EncodeGridTexture(int w, int h, bool lowRange = false)
		{
			std::vector<uint8_t> raw;

			for (int t = 0; t < h; t += 4)
				for (int s = 0; s < w; s += 4)
				{
					for (int v = 0; v < 4; v++)
						for (int u = 0; u < 4; u++)
						{
							Rgba c = GridTexel(s + u, t + v, w, h, lowRange);
							raw.push_back(c.A);
							raw.push_back(c.R);
						}

					for (int v = 0; v < 4; v++)
						for (int u = 0; u < 4; u++)
						{
							Rgba c = GridTexel(s + u, t + v, w, h, lowRange);
							raw.push_back(c.G);
							raw.push_back(c.B);
						}
				}

			return raw;
		}

		//! Load the 8x8 operand texture into texture map 0.
		void SetupGridTexture(GfxTestMachine& m, int w = 8, int h = 8, bool lowRange = false)
		{
			std::vector<uint8_t> raw = EncodeGridTexture(w, h, lowRange);
			SetupTexture(m, 0, GridTexAddr, w, h, GFX::TF_RGBA8, raw.data(), raw.size());
		}

		//! Draw the operand grid: `cols` x `rows` cells, each one a quad whose four vertices carry the
		//! same host colour and the same texel centre, so a cell stands for one operand pair.
		void DrawOperandGrid(GfxTestMachine& m, bool alphaColors = false, int cols = 8, int rows = 6)
		{
			float cw = 2.0f / cols;
			float ch = 2.0f / rows;

			for (int i = 0; i < cols; i++)
			{
				for (int j = 0; j < rows; j++)
				{
					Rgba c = alphaColors ? GridAlphaColor(i, j) : GridHostColor(i, j);
					float s = ((float)i + 0.5f) / (float)cols;
					float t = ((float)j + 0.5f) / (float)rows;

					float x0 = -1.0f + i * cw;
					float y0 = -1.0f + j * ch;

					DrawClipQuad(m, x0, y0, x0 + cw, y0 + ch, c, c, c, c, s, t, s, t);
				}
			}
		}

		//! A full screen quad whose window depth ramps from 0 at the top of the screen to 1 at the
		//! bottom, with one colour per corner, so the picture stays a gradient even where the depth
		//! test removed one of the two quads. The fog gallery uses it as the fog factor's input, the Z
		//! gallery as the probe.
		void DrawDepthRampQuad(GfxTestMachine& m, const Rgba& bl, const Rgba& br, const Rgba& tr, const Rgba& tl)
		{
			GFX::Vertex quad[4];
			quad[0] = GfxTestMachine::MakeVertex(-1, -1, 1, bl.R, bl.G, bl.B, bl.A);
			quad[1] = GfxTestMachine::MakeVertex(1, -1, 1, br.R, br.G, br.B, br.A);
			quad[2] = GfxTestMachine::MakeVertex(1, 1, -1, tr.R, tr.G, tr.B, tr.A);
			quad[3] = GfxTestMachine::MakeVertex(-1, 1, -1, tl.R, tl.G, tl.B, tl.A);
			m.DrawQuad(quad);
		}

		//! The 8x8 I8 ramp the Z gallery samples as its Z texture: the texel of column x is 36 * x.
		std::vector<uint8_t> EncodeZRampTexture(int w, int h)
		{
			std::vector<uint8_t> raw;

			for (int t = 0; t < h; t += 4)
				for (int s = 0; s < w; s += 8)
					for (int v = 0; v < 4; v++)
						for (int u = 0; u < 8; u++)
							raw.push_back((uint8_t)((s + u) * 36));

			return raw;
		}
	}

	TEST_CLASS(GfxReportTevTests)
	{
	public:

		// =========================================================================================
		// The colour combine
		// =========================================================================================

		// One picture per combine program (gfx-tev.md 3.2). Every picture is the same grid of operand
		// pairs, so a picture shows what the program does over the whole range of inputs.
		TEST_METHOD(Report_TevColorCombines)
		{
			GfxTestMachine& m = M();

			Report::Section("TEV colour combine",
				"One picture per colour program of TEV stage 0. Every picture is the same grid of operand pairs:\n"
				"the column of a cell is the host (rasterized) colour and its row the texel of the 8x8 test texture,\n"
				"so one picture shows the programmed arithmetic over forty-eight different input pairs. The programs\n"
				"are the GX combine modes (modulate, decal, blend, replace, pass), the arithmetic controls (bias,\n"
				"shift, subtract, clamp) and the constants, and the last one chains two stages.");

			struct Case
			{
				const char* file;
				const char* title;
				uint32_t colorEnv;
				uint32_t colorEnv1;			// the second stage, when the program needs one
				bool alphaGrid;				// the operand grid whose host colours carry an alpha ramp
				const char* note;
			};

			const Case cases[] = {
				{ "tev_cc_modulate.png", "modulate: Cv * Ct",
					ColorEnv(CZERO, CRAST, CTEX, CZERO), 0, false,
					"d=0, a=0, b=Cv, c=Ct: the lerp is Cv*Ct, so every cell multiplies its two inputs." },
				{ "tev_cc_decal.png", "decal: lerp(Ct, Cv, At)",
					ColorEnv(CTEX, CRAST, CTEXA, CZERO), 0, false,
					"The texel alpha is the blend factor, so the host colour takes over where the texel is opaque." },
				{ "tev_cc_blend.png", "blend: lerp(Cv, Ct, At)",
					ColorEnv(CRAST, CTEX, CTEXA, CZERO), 0, false,
					"The other way round: the texel is mixed into the host colour by the texel alpha." },
				{ "tev_cc_replace.png", "replace: Ct",
					ColorEnv(CZERO, CTEX, CONE, CZERO), 0, false,
					"c=1.0 passes b (the texel) through, so the host colour is gone and the cells of a row are equal." },
				{ "tev_cc_passthru.png", "pass: Cv",
					ColorEnv(CZERO, CRAST, CONE, CZERO), 0, false,
					"The host colour only, so the cells of a column are equal." },
				{ "tev_cc_add.png", "add: Cv + Ct",
					ColorEnv(CZERO, CTEX, CONE, CRAST), 0, false,
					"d=Cv, a=0, b=Ct, c=1.0: the lerp is the texel and it is added to the host colour." },
				{ "tev_cc_subtract.png", "subtract: Cv - Ct",
					ColorEnv(CZERO, CTEX, CONE, CRAST, 0, 1), 0, false,
					"The same with the subtract bit: the texel is taken away and the result is clamped at zero." },
				{ "tev_cc_addsigned.png", "add signed: Cv + Ct - 0.5",
					ColorEnv(CZERO, CTEX, CONE, CRAST, 2), 0, false,
					"The -0.5 bias of the signed add, which is what makes it a signed operation." },
				{ "tev_cc_scale2.png", "shift left 1: 2 * Cv",
					ColorEnv(CZERO, CRAST, CONE, CZERO, 0, 0, 1, 1), 0, false,
					"The shift doubles the host colour, so the bright half of the grid saturates." },
				{ "tev_cc_scale4.png", "shift left 2: 4 * Cv",
					ColorEnv(CZERO, CRAST, CONE, CZERO, 0, 0, 1, 2), 0, false,
					"Four times: only the darkest cells survive the clamp." },
				{ "tev_cc_div2.png", "shift right 1: Cv / 2",
					ColorEnv(CZERO, CRAST, CONE, CZERO, 0, 0, 1, 3), 0, false,
					"Halving the host colour, which is the shift the blending-style effects use." },
				{ "tev_cc_biasplus.png", "bias +0.5: Cv + 0.5",
					ColorEnv(CZERO, CRAST, CONE, CZERO, 1), 0, false,
					"The +0.5 bias of the u12.8 accumulator: everything above half scale saturates." },
				{ "tev_cc_texelalpha.png", "the texel alpha as the colour",
					ColorEnv(CZERO, CTEXA, CONE, CZERO), 0, false,
					"selb = the texel alpha, so the rows (which carry the alpha ramp) become grey bands." },
				{ "tev_cc_rastalpha.png", "the raster alpha as the colour",
					ColorEnv(CZERO, CRASTA, CONE, CZERO), 0, true,
					"The same for the host alpha: here the columns become the bands." },
				{ "tev_cc_doublesub.png", "Ct - Cv / 2",
					ColorEnv(CZERO, CRAST, CHALF, CTEX, 0, 1), 0, false,
					"d = the texel, and the lerp of the half constant takes half of the host colour away: an\n"
					"operation in which the two operands have different weights." },
				{ "tev_cc_unclamped.png", "an unclamped stage keeps the wide result",
					ColorEnv(CZERO, CTEX, CONE, CRAST, 0, 0, 0, 0, 3),
					ColorEnv(CZERO, CREG3, CONE, CZERO, 0, 0, 1, 3, 0), false,
					"Stage 0 adds without the clamp bit, so the u12.8 sum keeps its headroom above 255; stage 1\n"
					"halves it and clamps. A stage that clamped its own result would lose that headroom, and the\n"
					"bright cells would end up half as bright as they are here." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetupGridTexture(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				m.BpLoad(RAS1_TREF0_ID, 1u << 6);				// te0 = 1, ti0 = 0, tc0 = 0
				m.BpLoad(GEN_MODE_ID, c.colorEnv1 ? (1u << 10) : 0);

				m.BpLoad(TEV_COLOR_ENV_0_ID, c.colorEnv);
				m.BpLoad(TEV_ALPHA_ENV_0_ID, AlphaEnv(ATEX, AZERO, AZERO, ATEX));

				if (c.colorEnv1)
				{
					m.BpLoad(TEV_COLOR_ENV_1_ID, c.colorEnv1);
					m.BpLoad(TEV_ALPHA_ENV_1_ID, AlphaEnv(ATEX, AZERO, AZERO, ATEX));
				}

				m.BeginFrame();
				DrawOperandGrid(m, c.alphaGrid);
				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// The alpha combine
		// =========================================================================================

		// The alpha result is invisible in an RGB readback, so these pictures blend the grid over a
		// background with the source-alpha factor: the alpha the combine produced decides how much of
		// the background survives. gfx-tev.md 3.2, gfx-pe.md 6.2.
		TEST_METHOD(Report_TevAlphaCombines)
		{
			GfxTestMachine& m = M();

			Report::Section("TEV alpha combine",
				"The same grid, drawn with the source-alpha blend over a background, so the alpha of every cell is\n"
				"visible as the amount of background that survives. In this grid the column carries the host alpha and\n"
				"the row the texel alpha, so each picture shows the program over the whole range of alpha pairs.");

			struct Case
			{
				const char* file;
				const char* title;
				uint32_t alphaEnv;
				const char* note;
			};

			const Case cases[] = {
				{ "tev_ac_texel.png", "alpha = the texel alpha",
					AlphaEnv(ATEX, AZERO, AZERO, ATEX),
					"The texel alpha only: the fade follows the rows." },
				{ "tev_ac_raster.png", "alpha = the host alpha",
					AlphaEnv(ARAST, AZERO, AZERO, ARAST),
					"The host alpha only: the fade follows the columns." },
				{ "tev_ac_add.png", "alpha = host + texel",
					AlphaEnv(AZERO, ATEX, AKONST, ARAST),
					"c = the KONST 1.0 constant passes the texel through, and d adds the host alpha to it." },
				{ "tev_ac_subtract.png", "alpha = host - texel",
					AlphaEnv(AZERO, ATEX, AKONST, ARAST, 0, 1),
					"The subtract bit: the cells whose host alpha is below the texel alpha end up fully transparent." },
				{ "tev_ac_modulate.png", "alpha = host * texel",
					AlphaEnv(AZERO, ARAST, ATEX, AZERO),
					"c = the texel alpha turns the lerp into the product of the two alphas." },
				{ "tev_ac_invert.png", "alpha = 1 - texel",
					AlphaEnv(AZERO, ATEX, AKONST, AKONST, 0, 1),
					"d = 1.0 minus the texel alpha: the fade of the first picture, inverted." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupGridTexture(m);
				SetClearColor(m, MakeRgba(0x30, 0x18, 0x28));

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BpLoad(RAS1_TREF0_ID, 1u << 6);
				m.BpLoad(GEN_MODE_ID, 0);

				m.BeginFrame();

				// The background is drawn with the raster stage, then the grid blends over it
				DrawBackgroundThenTextureStage(m);

				m.BpLoad(TEV_COLOR_ENV_0_ID, ColorEnv(CZERO, CRAST, CONE, CZERO));
				m.BpLoad(TEV_ALPHA_ENV_0_ID, c.alphaEnv);

				m.BpLoad(PE_CMODE0_ID, (1u << 3) | (1u << 4) | 1u | (5u << 5) | (4u << 8));
				DrawOperandGrid(m, true);

				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// The alpha compare modes and the alpha function
		// =========================================================================================

		// gfx-tev.md 3.2: the alpha compare modes 1..3 replace the alpha result by a full scale mask
		// that is decided by the *sign* of the pre-clamp stage value. The mask is made visible by the
		// source-alpha blend: a full mask is opaque, an empty one lets the background through.
		TEST_METHOD(Report_TevAlphaCompareModes)
		{
			GfxTestMachine& m = M();

			Report::Section("Alpha compare modes (the alpha mask)",
				"The alpha combine has three compare modes that turn the stage value into a full scale mask, decided\n"
				"by the sign of the unclamped result. The pictures blend the grid over a background, so a set mask\n"
				"bit is an opaque cell and a clear one lets the background through. Two of the programs compute\n"
				"host - texel and two texel - host, i.e. the two halves of the operand grid.");

			struct Case
			{
				const char* file;
				const char* title;
				uint32_t alphaEnv;
				const char* note;
			};

			const Case cases[] = {
				{ "tev_acmp_ge0_hostminus.png", "mask = (host - texel >= 0)",
					AlphaEnv(AZERO, ATEX, AKONST, ARAST, 0, 1, 0, 0, 0, 1),
					"Mode 1 keeps the pixels whose stage value is not negative: the cells at or above the diagonal\n"
					"of the operand grid." },
				{ "tev_acmp_le0.png", "mask = (host - texel <= 0)",
					AlphaEnv(AZERO, ATEX, AKONST, ARAST, 0, 1, 0, 0, 0, 3),
					"Mode 3 is the complement of the first picture - the two differ only in the cells where the two\n"
					"alphas are exactly equal, and those are the cells the next picture keeps." },
				{ "tev_acmp_eq0.png", "mask = (host - texel == 0)",
					AlphaEnv(AZERO, ATEX, AKONST, ARAST, 0, 1, 0, 0, 0, 2),
					"Mode 2 is the equality mask. The host alphas are multiples of 32 and the texel alphas are the\n"
					"same values in a permuted order, so exactly one cell per row is equal." },
				{ "tev_acmp_ge0_sum.png", "mask = (host + texel - 0.5 >= 0)",
					AlphaEnv(AZERO, ATEX, AKONST, ARAST, 2, 0, 0, 0, 0, 1),
					"A different operand program with the same mode: the sum of the two alphas against the half\n"
					"constant, which is negative only for the darkest cells." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupGridTexture(m);
				SetClearColor(m, MakeRgba(0x20, 0x28, 0x18));

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BpLoad(RAS1_TREF0_ID, 1u << 6);
				m.BpLoad(GEN_MODE_ID, 0);

				m.BeginFrame();

				DrawBackgroundThenTextureStage(m);

				m.BpLoad(TEV_COLOR_ENV_0_ID, ColorEnv(CZERO, CRAST, CONE, CZERO));
				m.BpLoad(TEV_ALPHA_ENV_0_ID, c.alphaEnv);

				m.BpLoad(PE_CMODE0_ID, (1u << 3) | (1u << 4) | 1u | (5u << 5) | (4u << 8));
				DrawOperandGrid(m, true);

				Publish(m, c.title, c.file, c.note);
			}
		}

		// The alpha function compares the final alpha against two reference values with two operators
		// (gfx-tev.md 3.8). The tested quad carries a horizontal alpha ramp, so the operator decides
		// where the quad is cut, and the background drawn under it is what stays visible.
		TEST_METHOD(Report_TevAlphaFunction)
		{
			GfxTestMachine& m = M();

			Report::Section("The alpha function operators",
				"A quad whose host alpha ramps from 0 on the left to 255 on the right is drawn over the gallery\n"
				"background. The first operator of the alpha function is set to one of the eight comparison modes and\n"
				"the second one to always, so the picture shows exactly where the ramp survived the test against the\n"
				"reference value 0x80.");

			const char* files[8] = {
				"tev_af_never.png", "tev_af_less.png", "tev_af_equal.png", "tev_af_lequal.png",
				"tev_af_greater.png", "tev_af_nequal.png", "tev_af_gequal.png", "tev_af_always.png",
			};
			const char* titles[8] = {
				"op0 = never", "op0 = less", "op0 = equal", "op0 = less or equal",
				"op0 = greater", "op0 = not equal", "op0 = greater or equal", "op0 = always",
			};
			const char* notes[8] = {
				"Nothing passes, so the whole picture is the background.",
				"alpha < 0x80: the dark half of the ramp is kept.",
				"alpha == 0x80: only the column where the ramp crosses the reference value survives.",
				"alpha <= 0x80: the dark half plus the crossing column.",
				"alpha > 0x80: the bright half of the ramp is kept.",
				"alpha != 0x80: everything except the crossing column.",
				"alpha >= 0x80: the bright half plus the crossing column.",
				"Everything passes, so the gradient of the quad itself is the picture.",
			};

			for (int op = 0; op < 8; op++)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				m.BpLoad(PE_ZMODE_ID, 0);

				m.BeginFrame();

				// The background is unaffected by the test that is being shown
				m.BpLoad(TEV_ALPHAFUNC_ID, AlphaFunc(0, 0, 7, 7, 0));
				DrawBackground(m);

				// The tested quad: a horizontal alpha ramp, with the host colour as the output
				m.BpLoad(TEV_ALPHAFUNC_ID, AlphaFunc(0x80, 0, op, 7, 0));
				DrawClipQuad(m, -1, -1, 1, 1,
					MakeRgba(0xFF, 0xFF, 0xFF, 0x00), MakeRgba(0xFF, 0xFF, 0xFF, 0xFF),
					MakeRgba(0xC0, 0xC0, 0x40, 0xFF), MakeRgba(0xC0, 0xC0, 0x40, 0x00));

				Publish(m, titles[op], files[op], notes[op]);
			}
		}

		// =========================================================================================
		// The K constants
		// =========================================================================================

		// The 5-bit K selector names one of the eight fixed fractions (gfx-tev.md 3.4). Each picture
		// draws the same four column programs, so one constant can be seen as a colour, as a factor of
		// the texel, added to and taken away from the host colour.
		TEST_METHOD(Report_TevKonst)
		{
			GfxTestMachine& m = M();

			Report::Section("TEV K constants (Rev B)",
				"Eight pictures, one per fixed K constant (1.0, 7/8 ... 1/8). Every picture draws the operand grid\n"
				"with four column programs, left to right: the constant as the colour, the constant times the texel,\n"
				"the host colour plus the constant and the host colour minus the constant.");

			const uint32_t columnEnv[4] = {
				ColorEnv(CZERO, CZERO, CZERO, CKONST),			// the constant as the colour
				ColorEnv(CZERO, CKONST, CTEX, CZERO),			// the constant times the texel
				ColorEnv(CZERO, CKONST, CONE, CRAST),			// host + the constant
				ColorEnv(CZERO, CKONST, CONE, CRAST, 0, 1),		// host - the constant
			};

			const char* titles[8] = {
				"K = 1.0", "K = 7/8", "K = 3/4", "K = 5/8",
				"K = 1/2", "K = 3/8", "K = 1/4", "K = 1/8",
			};
			const char* files[8] = {
				"tev_konst_1.png", "tev_konst_7_8.png", "tev_konst_3_4.png", "tev_konst_5_8.png",
				"tev_konst_1_2.png", "tev_konst_3_8.png", "tev_konst_1_4.png", "tev_konst_1_8.png",
			};

			for (int k = 0; k < 8; k++)
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetupGridTexture(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				m.BpLoad(RAS1_TREF0_ID, 1u << 6);
				m.BpLoad(GEN_MODE_ID, 0);
				m.BpLoad(TEV_KSEL_0_ID, KSel(k, k, 0, 0));
				m.BpLoad(TEV_ALPHA_ENV_0_ID, AlphaEnv(ATEX, AZERO, AZERO, ATEX));

				m.BeginFrame();

				const int cols = 8, rows = 6;
				float cw = 2.0f / cols;
				float ch = 2.0f / rows;

				for (int col = 0; col < 4; col++)
				{
					m.BpLoad(TEV_COLOR_ENV_0_ID, columnEnv[col]);

					for (int i = col * 2; i < col * 2 + 2; i++)
					{
						for (int j = 0; j < rows; j++)
						{
							Rgba c = GridHostColor(i, j);
							float s = ((float)i + 0.5f) / (float)cols;
							float t = ((float)j + 0.5f) / (float)rows;
							float x0 = -1.0f + i * cw;
							float y0 = -1.0f + j * ch;

							DrawClipQuad(m, x0, y0, x0 + cw, y0 + ch, c, c, c, c, s, t, s, t);
						}
					}
				}

				Publish(m, titles[k], files[k],
					"The four column pairs are, left to right: the constant as the colour, the constant times the\n"
					"texel, the host colour plus the constant, and the host colour minus the constant.");
			}
		}

		// =========================================================================================
		// Bias, shift and clamp
		// =========================================================================================

		// The arithmetic controls of a stage, applied to the same half-and-half mix of the two operands:
		// the combination that makes a level shift or a saturation obvious.
		TEST_METHOD(Report_TevBiasShift)
		{
			GfxTestMachine& m = M();

			Report::Section("TEV bias and shift",
				"The same operand grid through eight arithmetic settings of one stage program, which outputs the texel\n"
				"colour. The +0.5 and -0.5 biases of the u12.8 accumulator brighten and darken the texel, the shifts\n"
				"scale it, and the clamp at the end of the stage turns the scaling into saturation; the last two\n"
				"pictures apply a bias and a shift together, in the order the hardware applies them.");

			struct Case
			{
				const char* file;
				const char* title;
				int bias;
				int shift;
				const char* note;
			};

			const Case cases[] = {
				{ "tev_bs_biasplus.png", "bias +0.5", 1, 0,
					"Half a unit is added to the texel, so the dark rows lose most of their range." },
				{ "tev_bs_biasminus.png", "bias -0.5", 2, 0,
					"Half a unit is taken away, which pushes the dark half of the texel range to black." },
				{ "tev_bs_shift1.png", "shift left 1 (x2)", 0, 1,
					"The texel is doubled and then clamped, so everything above half scale saturates." },
				{ "tev_bs_shift2.png", "shift left 2 (x4)", 0, 2,
					"Four times: with the operand grid inside the low quarter of the range, the steps of the ramp\n"
					"survive the scaling and only its brightest cell saturates (the same scaling applied to the full\n"
					"range of the other pictures would be white almost everywhere)." },
				{ "tev_bs_shift3.png", "shift right 1 (x0.5)", 0, 3,
					"Halved: the picture gets darker without losing any steps." },
				{ "tev_bs_biasplus_shift3.png", "bias +0.5 then shift right 1", 1, 3,
					"The two controls together, in the order the hardware applies them." },
				{ "tev_bs_biasminus_shift1.png", "bias -0.5 then shift left 1", 2, 1,
					"A negative bias scaled up: the dark cells are already black, the bright ones lose half of\n"
					"their range." },
				{ "tev_bs_biasminus_shift3.png", "bias -0.5 then shift right 1", 2, 3,
					"A negative bias halved, which keeps a narrow band of the range alive." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);

				// The four-fold scaling saturates a full-range operand grid into one flat colour, so that
				// picture uses a grid whose texels are all in the low quarter of the range: the shift is
				// then visible as the ramp of the grid scaled up, which is the same feature.
				SetupGridTexture(m, 8, 8, c.shift == 2);
				SetClearColor(m, MakeRgba(0, 0, 0));

				m.BpLoad(RAS1_TREF0_ID, 1u << 6);
				m.BpLoad(GEN_MODE_ID, 0);
				m.BpLoad(TEV_COLOR_ENV_0_ID, ColorEnv(CZERO, CTEX, CONE, CZERO, c.bias, 0, 1, c.shift));
				m.BpLoad(TEV_ALPHA_ENV_0_ID, AlphaEnv(ATEX, AZERO, AZERO, ATEX));

				m.BeginFrame();
				DrawOperandGrid(m);
				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// Fog
		// =========================================================================================

		// The fog factor is a function of the depth of the pixel, so the gallery draws a quad whose depth
		// ramps from the near to the far plane across the screen: the picture is then the fog curve
		// itself. gfx-tev.md 3.6 (the F-select functions, the projection bit and the range adjustment).
		TEST_METHOD(Report_TevFog)
		{
			GfxTestMachine& m = M();

			Report::Section("TEV fog",
				"One picture per fog function. The quad carries a depth ramp from the near plane at the bottom of the\n"
				"screen to the far plane at the top, so the vertical axis of every picture is the fog factor's input and\n"
				"the colour shows the curve: a warm base colour fading into the fog colour. The F-select is the three-bit\n"
				"field of TEV_FOG_PARAM_3: the five functions the GX API programs are 2 (linear), 4 (exponential),\n"
				"5 (exponential squared), 6 (backward exponential) and 7 (backward exponential squared); the two\n"
				"encodings it cannot produce are 1, the \"off\" family with the square bit set, which leaves the picture\n"
				"unfogged like F-select 0, and 3, the linear law applied to the squared value - a quadratic curve.");

			struct Case
			{
				const char* file;
				const char* title;
				int fsel;
				int proj;
				uint32_t fogParam1;
				uint32_t fogParam2;
				uint32_t fogColor;
				bool rangeAdj;
				const char* note;
			};

			// C = 0: the s11e8 form has no zero mantissa, so the exponent 0 is the smallest offset it can
			// express. b_mag / b_shft belong to the remap of the perspective case.
			const uint32_t fogC = 0;

			const Case cases[] = {
				{ "tev_fog_linear.png", "F-select 2: linear", 2, 1, 0, 0, 0x0000FF,
					false, "The fog factor is the depth itself (minus C): a linear ramp into the blue fog colour." },
				{ "tev_fog_off1.png", "F-select 1: the off family", 1, 1, 0, 0, 0x0000FF,
					false, "F-select 1 is the \"off\" family with the square bit set: the primitive stays unfogged at\n"
					"every depth, exactly like F-select 0." },
				{ "tev_fog_quadratic.png", "F-select 3: the linear law on the squared value", 3, 1, 0, 0, 0x0000FF,
					false, "F-select 3 is the linear law applied to the squared clamped value, i.e. a quadratic curve:\n"
					"the near half of the picture stays clear much longer than the linear one." },
				{ "tev_fog_exp.png", "F-select 4: exponential", 4, 1, 0, 0, 0x0000FF,
					false, "1 - 2^(-8f): the fog comes in quickly and then flattens out." },
				{ "tev_fog_exp2.png", "F-select 5: exponential squared", 5, 1, 0, 0, 0x0000FF,
					false, "1 - 2^(-8f*f): the curve is even steeper at the far end." },
				{ "tev_fog_expinv.png", "F-select 6: inverse exponential", 6, 1, 0, 0, 0x0000FF,
					false, "2^(-8(1-f)): the fog is nearly absent on the left and full on the right." },
				{ "tev_fog_expinv2.png", "F-select 7: inverse exponential squared", 7, 1, 0, 0, 0x0000FF,
					false, "2^(-8(1-f)^2), the last of the five functions the hardware defines." },
				{ "tev_fog_perspective.png", "F-select 2 with the projection bit clear", 2, 0, 2, 24, 0x0000FF,
					false, "Without the projection bit the depth is remapped as b_mag - (z >> b_shft) and the\n"
					"reciprocal of that is the view distance, so this is a different curve over the same ramp." },
				{ "tev_fog_rangeadj.png", "range adjustment enabled", 4, 1, 0, 0, 0x0000FF,
					true, "The range adjustment multiplies the eye distance by a coefficient that depends on the\n"
					"horizontal distance from the centre, so the fog gets a pattern of its own." },
				{ "tev_fog_color.png", "a red fog colour with F-select 6", 6, 1, 0, 0, 0xFF2000,
					false, "The fog colour is a TEV register: the same curve as above, a different colour." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BpLoad(TEV_FOG_PARAM_0_ID, 127u << 11);			// A = 1.0
				m.BpLoad(TEV_FOG_PARAM_1_ID, c.fogParam1);
				m.BpLoad(TEV_FOG_PARAM_2_ID, c.fogParam2);

				// TEV_FOG_PARAM_3: c_mant 10:0, c_expn 18:11, c_sign 19, proj 20, fsel 23:21
				m.BpLoad(TEV_FOG_PARAM_3_ID, fogC | (c.proj ? (1u << 20) : 0u) | ((uint32_t)c.fsel << 21));
				m.BpLoad(TEV_FOG_COLOR_ID, c.fogColor);

				// The range adjustment: one coefficient per 256 pixels of horizontal distance from the centre
				if (c.rangeAdj)
				{
					m.BpLoad(TEV_RANGE_ADJ_C_ID, 320u | (1u << 10));
					m.BpLoad(TEV_RANGE_ADJ_0_ID, 0x40 | (0x200u << 12));
					m.BpLoad(TEV_RANGE_ADJ_1_ID, 0x20 | (0x180u << 12));
					m.BpLoad(TEV_RANGE_ADJ_2_ID, 0x100 | (0x10u << 12));
				}

				m.BeginFrame();
				DrawDepthRampQuad(m, MakeRgba(0xFF, 0xB0, 0x60), MakeRgba(0xFF, 0xE0, 0x90),
					MakeRgba(0xFF, 0xF8, 0xF0), MakeRgba(0xE0, 0xC0, 0x80));

				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// The Z texture environment
		// =========================================================================================

		// TEV_Z_ENV replaces the depth of the pixel by a function of the Z texel (gfx-tev.md 3.5). The
		// gallery makes that depth visible: the Z quad is drawn first (writing depth) and a probe quad
		// whose own depth ramps from the top of the screen to the bottom is drawn after it with the
		// "less" test, so the area where the probe survives is bounded by the depth the Z environment
		// produced.
		TEST_METHOD(Report_TevZTexture)
		{
			GfxTestMachine& m = M();

			Report::Section("The Z texture environment",
				"A quad samples a Z texture whose texels ramp from 0 to 255 across the picture, and the Z environment\n"
				"turns that texel into the pixel's depth: with the add operation it is the rasterized depth plus the\n"
				"texel plus the bias, with replace it is the texel plus the bias alone. A probe quad is then drawn with\n"
				"its own depth ramping from 0 at the top to 1 at the bottom and with the \"less\" test, so it survives\n"
				"exactly where the depth the Z environment produced is farther than the probe's own depth: the upper\n"
				"edge of the bright probe area traces the resulting depth curve.");

			struct Case
			{
				const char* file;
				const char* title;
				int type;
				int op;
				uint32_t zoff;
				const char* note;
			};

			// The bias values are chosen so that every case puts the resulting depth inside the screen:
			// a u8 texel only reaches 255, a u16 one 65535 and a u24 one all 16.7M depth units.
			const Case cases[] = {
				{ "tev_zenv_u8_add.png", "type u8, op add", 0, 1, 3000000,
					"The texel is the alpha byte of the Z image, so the bias dominates and the edge is nearly flat." },
				{ "tev_zenv_u16_add.png", "type u16, op add", 1, 1, 1200000,
					"The texel is the green/blue pair of the image: it reaches 65535, so the bias is smaller and the\nedge sits lower." },
				{ "tev_zenv_u24_add.png", "type u24, op add", 2, 1, 0,
					"The full 24-bit texel plus the rasterized depth of 0.5: the edge is a rising diagonal, because\n"
					"the texel ramp is the only thing that moves." },
				{ "tev_zenv_u8_replace.png", "type u8, op replace", 0, 2, 5000000,
					"Replace throws the rasterized depth away: the depth is the texel plus the bias, with the texel\n"
					"so small that the bias decides." },
				{ "tev_zenv_u16_replace.png", "type u16, op replace", 1, 2, 11000000,
					"The same for the 16-bit texel." },
				{ "tev_zenv_u24_replace.png", "type u24, op replace", 2, 2, 0,
					"The 24-bit texel replaces the depth outright, so the depth buffer becomes the Z image and the\n"
					"edge follows its ramp from the top of the screen to the bottom." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupTextureStage0(m);
				SetClearColor(m, MakeRgba(0, 0, 0), 0xFFFFFF);

				std::vector<uint8_t> raw = EncodeZRampTexture(8, 8);
				SetupTexture(m, 0, RampTexAddr, 8, 8, GFX::TF_I8, raw.data(), raw.size());

				m.BpLoad(GEN_MODE_ID, 0);
				m.BpLoad(TEV_Z_ENV_0_ID, c.zoff);
				m.BpLoad(TEV_Z_ENV_1_ID, (uint32_t)c.type | ((uint32_t)c.op << 2));

				m.BeginFrame();

				// The Z quad: a window depth of 0.5, depth writes on, its depth modified by the Z environment
				m.BpLoad(PE_ZMODE_ID, 1u | (7u << 1) | (1u << 4));
				DrawClipQuad(m, -1, -1, 1, 1, MakeRgba(0xFF, 0xFF, 0xFF), MakeRgba(0xFF, 0xFF, 0xFF),
					MakeRgba(0xFF, 0xFF, 0xFF), MakeRgba(0xFF, 0xFF, 0xFF), 0.0f, 0.0f, 1.0f, 1.0f);

				// The probe quad: the Z environment is off for it, its colour comes from the host colours
				// (the texture stage would paint the ramp again) and it does not write depth
				m.BpLoad(TEV_Z_ENV_1_ID, 0);
				SetupRasterStage0(m);
				m.BpLoad(PE_ZMODE_ID, 1u | (1u << 1) | (0u << 4));
				DrawDepthRampQuad(m, MakeRgba(0x20, 0x40, 0xFF), MakeRgba(0xFF, 0x30, 0x20),
					MakeRgba(0x20, 0xFF, 0x80), MakeRgba(0xFF, 0xC0, 0x20));

				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// Indirect (bump) texturing
		// =========================================================================================

		// The indirect stage samples an indirect texture, turns the texel fields into a coordinate
		// offset through one of the three 3x2 matrices and adds that offset to the stage's own texture
		// coordinate before the sampler sees it (gfx-bump.md 3.3). The gallery samples a ramp texture
		// whose texels step by a known amount, so a different offset lands on a different texel and the
		// bands of the ramp move by exactly that much.
		TEST_METHOD(Report_TevIndirect)
		{
			GfxTestMachine& m = M();

			Report::Section("Indirect (bump) texturing",
				"A 64x64 texture is sampled once across the screen through an indirect stage, which adds a coordinate\n"
				"offset before the sampler sees the coordinate. The texture has a coarse staircase (the red component\n"
				"steps by 16 every four texels, the green one by 32 every eight) plus a fine one-texel pattern in the\n"
				"blue component, so both a large offset and a sub-texel one are visible. The offset comes from the\n"
				"texel of the indirect texture - the same map - through one of the three 3x2 matrices; the three\n"
				"matrices hold different coefficients (0x3FF with scale 14, 0x200 with scale 13 and the negative\n"
				"0x401 with scale 15), so selecting a matrix selects an offset. The pictures of the wrap window\n"
				"show the coordinate being masked before the offset is added.");

			struct Case
			{
				const char* file;
				const char* title;
				int mode;				// 0 = no indirect command, 1..3 = a matrix, 4 = two indirect stages
				int fmt;
				int bias;
				int wrapS;
				int wrapT;
				uint32_t ma0;			// matrix 0 overrides
				int scale0;
				const char* note;
			};

			const Case cases[] = {
				{ "bump_off.png", "no indirect command", 0, 0, 0, 0, 0, 0x3FF, 14,
					"The reference picture: the ramp as the sampler sees it without any offset." },
				{ "bump_matrix1.png", "matrix 0: ma = 0x3FF, scale 14", 1, 0, 0, 0, 0, 0x3FF, 14,
					"ma = 0x3FF with a scale of 14 shifts the coordinate by up to about thirty texels, so the\n"
					"staircase is smeared into a fan towards the bright end of the ramp." },
				{ "bump_matrix2.png", "matrix 1: ma = 0x200, scale 13", 2, 0, 0, 0, 0, 0x3FF, 14,
					"The second matrix has a quarter of the offset of the first one, so the fan is much narrower." },
				{ "bump_matrix3.png", "matrix 2: ma = 0x401 (negative), scale 15", 3, 0, 0, 0, 0, 0x3FF, 14,
					"0x401 is -1023 as an S1.10 coefficient: the offset is negative and, with the largest scale,\n"
					"the staircase slides the other way about twice as far as the first matrix." },
				{ "bump_fmt4.png", "the 4-bit texel format", 1, 2, 0, 0, 0, 0x3FF, 14,
					"fmt = 2 keeps only the top four bits of the texel, so the offset steps are coarse and the\n"
					"fan breaks into a few discrete bands." },
				{ "bump_fmt3.png", "the 3-bit texel format", 1, 3, 0, 0, 0, 0x3FF, 14,
					"fmt = 3 keeps three bits, which is coarser still." },
				{ "bump_bias.png", "the biased 8-bit texel", 1, 0, 1, 0, 0, 0x3FF, 14,
					"bias = 1 centres the 8-bit field on zero, so the dark half of the ramp produces a negative\n"
					"offset and the fan opens the other way." },
				{ "bump_scale_lo.png", "matrix 0 with scale 11", 1, 0, 0, 0, 0, 0x3FF, 11,
					"Three scale steps lower: the offset shrinks eight-fold and the fan barely opens." },
				{ "bump_scale_hi.png", "matrix 0 with scale 15", 1, 0, 0, 0, 0, 0x3FF, 15,
					"One step higher: the bright end of the ramp is shifted by more than a whole texture." },
				{ "bump_ma_neg.png", "matrix 0 with ma = 0x401 (negative)", 1, 0, 0, 0, 0, 0x401, 14,
					"0x401 is -1023 as an S1.10 coefficient: the offset is negative, so the staircase slides the\n"
					"other way. (The other negative coefficient, 0x7FF, is -1 and produces no visible offset at\n"
					"all, which is a good way to see how small the matrix step can be.)" },
				{ "bump_wrap32.png", "the coordinate wrapped to 32 texels", 1, 0, 0, 4, 4, 0x3FF, 14,
					"bp_wrap = 4 masks the coordinate to a 32 texel window before the offset is added, so the right\n"
					"half of every repeat starts again from the wrapped coordinate." },
				{ "bump_wrap16.png", "the coordinate wrapped to 16 texels", 1, 0, 0, 5, 5, 0x3FF, 14,
					"A 16 texel window, i.e. two texels of the ramp: the staircase is cut every second band." },
				{ "bump_wrap_mixed.png", "32 texels in S, 16 in T", 1, 0, 0, 4, 5, 0x3FF, 14,
					"The two axes are programmed independently; the offset itself only moves in S." },
				{ "bump_two_stages.png", "two indirect stages with the feedback bit", 4, 0, 0, 0, 0, 0x3FF, 14,
					"Both stages are indirect and the second one has bp_fb set, so it adds the offset of the first\n"
					"stage as well: the offsets of two matrices accumulate." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				// The sampled texture: a 64x64 image with a coarse staircase and a fine one-texel pattern
				std::vector<uint8_t> ramp;
				for (int t = 0; t < 64; t += 4)
					for (int s = 0; s < 64; s += 4)
						for (int v = 0; v < 4; v++)
							for (int u = 0; u < 4; u++)
							{
								int x = s + u, y = t + v;
								int r = ((x / 4) * 16) >> 3;			// the 5-bit red field
								int g = ((y / 8) * 32) >> 2;			// the 6-bit green field
								int b = (64 + ((x + y) & 15) * 8) >> 3;	// the 5-bit blue field
								uint16_t texel = (uint16_t)((r << 11) | ((g & 0x3F) << 5) | (b & 0x1F));
								ramp.push_back((uint8_t)(texel >> 8));
								ramp.push_back((uint8_t)texel);
							}

				SetupTexture(m, 0, RampTexAddr, 64, 64, GFX::TF_RGB565, ramp.data(), ramp.size(),
					1u | (1u << 2));						// wrap_s = wrap_t = repeat

				// The three matrices. Matrix 2 is loaded with the negative coefficient even when this case
				// uses another one, so that selecting a matrix is the only thing that changes.
				auto loadMatrix = [&m](int matrix, uint32_t ma, int scaleA, int scaleB)
				{
					m.BpLoad(BUMP_MATRIX_A0_ID + matrix * 3 + 0, (ma & 0x7FF) | ((uint32_t)(scaleA & 3) << 22));
					m.BpLoad(BUMP_MATRIX_B0_ID + matrix * 3 + 0, 0u | ((uint32_t)(scaleB & 3) << 22));
					m.BpLoad(BUMP_MATRIX_C0_ID + matrix * 3 + 0, 0u);
				};

				loadMatrix(0, c.ma0, c.scale0 & 3, (c.scale0 >> 2) & 3);
				loadMatrix(1, 0x200, 1, 3);
				loadMatrix(2, 0x401, 3, 3);
				m.BpLoad(BUMP_IMASK_ID, 0xFF);

				// The indirect command of stage 0 (and of stage 1 for the two-stage case)
				m.BpLoad(BUMP_CMD_ID, 0);
				m.BpLoad(BUMP_CMD_ID + 1, 0);

				if (c.mode == 4)
				{
					m.BpLoad(BUMP_CMD_ID + 0, (1u << 9) | ((uint32_t)c.fmt << 2) |
						((uint32_t)c.bias << 4) | (4u << 13) | (4u << 16));
					m.BpLoad(BUMP_CMD_ID + 1, (2u << 9) | (1u << 20) | (4u << 13) | (4u << 16));
				}
				else if (c.mode != 0)
				{
					m.BpLoad(BUMP_CMD_ID + 0, ((uint32_t)c.mode << 9) | ((uint32_t)c.fmt << 2) |
						((uint32_t)c.bias << 4) | ((uint32_t)c.wrapS << 13) | ((uint32_t)c.wrapT << 16));
				}

				// Stage 0 samples the ramp; the two-stage case enables the texture on stage 1 as well
				SetupTextureStage0(m, 0);
				m.BpLoad(RAS1_TREF0_ID, (1u << 6) | (c.mode == 4 ? (1u << 18) : 0u));
				m.BpLoad(GEN_MODE_ID, (c.mode == 4) ? (1u << 10) : 0);

				if (c.mode == 4)
				{
					m.BpLoad(TEV_COLOR_ENV_1_ID, ColorEnv(CZERO, CTEX, CONE, CZERO));
					m.BpLoad(TEV_ALPHA_ENV_1_ID, AlphaEnv(ATEX, AZERO, AZERO, ATEX));
				}

				m.BeginFrame();

				// One quad samples the whole texture, so the offset shows as a slide of both staircases
				DrawClipQuad(m, -1, -1, 1, 1, MakeRgba(0xFF, 0xFF, 0xFF), MakeRgba(0xFF, 0xFF, 0xFF),
					MakeRgba(0xFF, 0xFF, 0xFF), MakeRgba(0xFF, 0xFF, 0xFF), 0.0f, 0.0f, 1.0f, 1.0f);

				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// The coordinate shift scale of an indirect stage
		// =========================================================================================

		// RAS1_SS0/SS1 give the four indirect stages a shift scale for the coordinate they fetch
		// with: the `ras1_sts` field divides it by 1, 2, 4 ... 256 (gfx-ras1.md 4.2,
		// GX_SetIndTexCoordScale). The indirect command's `bt` field is the bump stage the TEV stage
		// consumes, so `bt` selects which of the four scales applies. The picture samples a 64 x 64
		// texture with a coarse staircase through an indirect stage whose matrix produces no offset at
		// all, so the only thing that moves is the coordinate the fetch starts from: a shift of one
		// halves it, which brings the middle of the texture to the right edge of the screen.
		TEST_METHOD(Report_TevIndirectCoordScale)
		{
			GfxTestMachine& m = M();

			Report::Section("The coordinate shift scale of an indirect stage (RAS1_SS0/SS1)",
				"A 64 x 64 texture with a staircase of red steps and a one-texel blue pattern is sampled through an\n"
				"indirect stage. Its matrix is all zero, so the indirect offset is zero and the picture is the texture\n"
				"itself - what the shift scale changes is the coordinate the fetch and the lookup start from. A shift\n"
				"of 1 halves the coordinate, so only the left half of the texture is reachable; a shift of 3 divides\n"
				"it by eight.");

			struct Case
			{
				const char* file;
				const char* title;
				int shiftS;				// RAS1_SS0.ss0, the shift of indirect stage 0 in S
				int shiftT;				// RAS1_SS0.ts0
				const char* note;
			};

			const Case cases[] = {
				{ "bump_shift_off.png", "no shift scale", 0, 0,
					"The reference picture: the shift of zero leaves the coordinate alone, so the texture fills the\n"
					"screen once." },
				{ "bump_shift_1.png", "a shift of 1 (divide by 2)", 1, 1,
					"The coordinate of the fetch is halved, so the right half of the picture repeats the left half of\n"
					"the texture." },
				{ "bump_shift_3.png", "a shift of 3 (divide by 8)", 3, 3,
					"Divided by eight: only an eighth of the texture is reachable, stretched over the whole screen." },
				{ "bump_shift_mixed.png", "a shift of 2 in S, none in T", 2, 0,
					"The two axes are programmed independently: the horizontal staircase is compressed while the\n"
					"vertical one is not." },
			};

			// The sampled texture: a fine red/green staircase with a one-texel blue pattern, so that even
			// an eighth of it (the picture of the largest shift) is not a flat colour
			std::vector<uint8_t> ramp;
			for (int t = 0; t < 64; t += 4)
				for (int s = 0; s < 64; s += 4)
					for (int v = 0; v < 4; v++)
						for (int u = 0; u < 4; u++)
						{
							int x = s + u, y = t + v;
							int r = ((x * 4) & 0xFF) >> 3;
							int g = ((y * 4) & 0xFF) >> 2;
							int b = (64 + (((x + y) & 3) * 48)) >> 3;
							uint16_t texel = (uint16_t)((r << 11) | ((g & 0x3F) << 5) | (b & 0x1F));
							ramp.push_back((uint8_t)(texel >> 8));
							ramp.push_back((uint8_t)texel);
						}

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				SetupTexture(m, 0, RampTexAddr, 64, 64, GFX::TF_RGB565, ramp.data(), ramp.size(),
					1u | (1u << 2));						// wrap_s = wrap_t = repeat

				// A matrix of zeros: the indirect stage produces no offset of its own
				m.BpLoad(BUMP_MATRIX_A0_ID, 0u | (2u << 22));
				m.BpLoad(BUMP_MATRIX_B0_ID, 0u | (3u << 22));
				m.BpLoad(BUMP_MATRIX_C0_ID, 0u);
				m.BpLoad(BUMP_IMASK_ID, 0xFF);

				// The indirect command of stage 0: a matrix mode, the bump stage 0 (bt), no wrap
				m.BpLoad(BUMP_CMD_ID, (1u << 9) | (0u << 13) | (0u << 16));

				// RAS1_SS0: ss0 in bits 3:0, ts0 in 7:4 (the shifts of the indirect stage 0)
				m.BpLoad(RAS1_SS0_ID, (uint32_t)(c.shiftS & 0xF) | ((uint32_t)(c.shiftT & 0xF) << 4));

				SetupTextureStage0(m, 0);
				m.BeginFrame();

				DrawClipQuad(m, -1, -1, 1, 1, MakeRgba(0xFF, 0xFF, 0xFF), MakeRgba(0xFF, 0xFF, 0xFF),
					MakeRgba(0xFF, 0xFF, 0xFF), MakeRgba(0xFF, 0xFF, 0xFF), 0.0f, 0.0f, 1.0f, 1.0f);

				Publish(m, c.title, c.file, c.note);
			}
		}
	};
}
