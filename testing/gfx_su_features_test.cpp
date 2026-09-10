// SU / RAS / TX / TEV: the registers of the setup unit and of the texture sampler that the backend
// has to apply (the scissor rectangle, the line and point size, the texture coordinate scale, the
// mip selection of the sampler) and the TEV features the fragment program has to honour.
//
// The tests that render read their pixels back from the EFB with ReadPixel(), which takes a *screen*
// coordinate (the origin is the top left corner of the display, as the GX registers use it) and maps
// it onto the GL readback, which is bottom-up.
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

		// The rows of the EFB the tests may sample. The test window is a little smaller than the
		// 640x480 render target the pipeline draws into (it is created with WS_OVERLAPPEDWINDOW), so
		// the outermost rows and columns of the window are not part of the drawable and read back as
		// black; the background checks stay inside this margin.
		const int SafeMargin = 48;

		//! One EFB pixel in screen coordinates (the origin is the top left corner of the display).
		void ReadPixel(GfxTestMachine& m, int x, int y, uint8_t rgb[3])
		{
			// glReadPixels (and ReadColorPixel) address the rows from the bottom of the window
			m.ReadColorPixel(x, EfbHeight - 1 - y, rgb);
		}

		//! The green channel of one EFB pixel in screen coordinates.
		int PixelGreen(GfxTestMachine& m, int x, int y)
		{
			uint8_t rgb[3];
			ReadPixel(m, x, y, rgb);
			return rgb[1];
		}

		//! True when two pixels differ by no more than `tolerance` in every channel.
		bool PixelsMatch(GfxTestMachine& m, int x0, int y0, int x1, int y1, int tolerance)
		{
			uint8_t a[3], b[3];
			ReadPixel(m, x0, y0, a);
			ReadPixel(m, x1, y1, b);

			for (int i = 0; i < 3; i++)
			{
				if (abs((int)a[i] - (int)b[i]) > tolerance)
					return false;
			}

			return true;
		}

		//! The SU_SSIZE/SU_TSIZE pair of one texture coordinate: the registers hold `size - 1`, which
		//! is what GX_SetTexCoordScaleManually stores as well.
		void SetCoordScale(GfxTestMachine& m, int pair, int sSize, int tSize)
		{
			m.BpLoad(SU_SSIZE0_ID + pair * 2, (uint32_t)(sSize - 1));
			m.BpLoad(SU_TSIZE0_ID + pair * 2, (uint32_t)(tSize - 1));
		}

		//! An I8 texture whose column `x` has the intensity `columns[x]`, in the tile order the
		//! decoder of tx.cpp expects (an I8 tile is 8 texels wide and 4 tall).
		std::vector<uint8_t> EncodeI8Columns(int width, int height, const std::vector<int>& columns,
			int blankValue = 0)
		{
			std::vector<uint8_t> raw;

			for (int t = 0; t < height; t += 4)
				for (int s = 0; s < width; s += 8)
					for (int v = 0; v < 4; v++)
						for (int u = 0; u < 8; u++)
						{
							int x = s + u;
							raw.push_back((uint8_t)((x < (int)columns.size()) ? columns[x] : blankValue));
						}

			return raw;
		}

		//! A black/white checkerboard in the I8 format, `size` x `size` texels, `cell` texels per
		//! square.
		std::vector<uint8_t> EncodeI8Checker(int size, int cell)
		{
			std::vector<uint8_t> raw;

			for (int t = 0; t < size; t += 4)
				for (int s = 0; s < size; s += 8)
					for (int v = 0; v < 4; v++)
						for (int u = 0; u < 8; u++)
						{
							bool white = ((((s + u) / cell) + ((t + v) / cell)) & 1) != 0;
							raw.push_back(white ? 0xFF : 0x00);
						}

			return raw;
		}

		//! The spread (max - min) of the green channel over a rectangle of the EFB: the mip level
		//! selection tests ask whether a high frequency texture is still resolved or has been averaged
		//! into a flat colour.
		int GreenSpread(GfxTestMachine& m, int x, int y, int w, int h)
		{
			std::vector<uint8_t> rgb;
			m.ReadColor(x, EfbHeight - y - h, w, h, rgb);

			int lo = 255, hi = 0;

			for (size_t i = 1; i < rgb.size(); i += 3)
			{
				lo = (rgb[i] < lo) ? rgb[i] : lo;
				hi = (rgb[i] > hi) ? rgb[i] : hi;
			}

			return hi - lo;
		}

		//! The number of distinct runs of the green channel along one row of the EFB.
		int GreenSteps(GfxTestMachine& m, int y, int x0, int x1)
		{
			std::vector<uint8_t> rgb;
			m.ReadColor(x0, EfbHeight - 1 - y, x1 - x0, 1, rgb);

			int previous = -1;
			int steps = 0;

			for (size_t i = 1; i < rgb.size(); i += 3)
			{
				if (rgb[i] != previous)
				{
					steps++;
					previous = rgb[i];
				}
			}

			return steps;
		}

		//! A full screen quad whose window depth ramps from 1 (the far plane) at the bottom of the
		//! screen to 0 (the near plane) at the top, in one colour. The window depth of a screen row is
		//! then `(row + 0.5) / 480`.
		void DrawDepthRampQuad(GfxTestMachine& m, const Rgba& c)
		{
			GFX::Vertex quad[4];
			quad[0] = GfxTestMachine::MakeVertex(-1, -1, 1, c.R, c.G, c.B, c.A);		// bottom: depth 1
			quad[1] = GfxTestMachine::MakeVertex(1, -1, 1, c.R, c.G, c.B, c.A);
			quad[2] = GfxTestMachine::MakeVertex(1, 1, -1, c.R, c.G, c.B, c.A);		// top: depth 0
			quad[3] = GfxTestMachine::MakeVertex(-1, 1, -1, c.R, c.G, c.B, c.A);
			m.DrawQuad(quad);
		}

		//! The TEV fog registers of one fog picture: A = 1.0, C = 0, the given select and projection.
		void SetFog(GfxTestMachine& m, int fsel, int proj, uint32_t color)
		{
			m.BpLoad(TEV_FOG_PARAM_0_ID, 127u << 11);			// A = 1.0 (s11e8)
			m.BpLoad(TEV_FOG_PARAM_1_ID, 0);
			m.BpLoad(TEV_FOG_PARAM_2_ID, 0);

			// TEV_FOG_PARAM_3: c_mant 10:0, c_expn 18:11, c_sign 19, proj 20, fsel 23:21
			m.BpLoad(TEV_FOG_PARAM_3_ID, (uint32_t)(proj ? (1u << 20) : 0u) | ((uint32_t)fsel << 21));
			m.BpLoad(TEV_FOG_COLOR_ID, color);
		}
	}

	TEST_CLASS(GfxSuFeaturesTest)
	{
		static GfxTestMachine& M()
		{
			GfxTestMachine& m = Machine();
			m.Reset();
			return m;
		}

	public:

		// =========================================================================================
		// The scissor rectangle (SU_SCIS0/SU_SCIS1, 0x20/0x21)
		// =========================================================================================

		// The register pair holds the top left and the bottom right corner of the rectangle in screen
		// coordinates, biased by 342 (gfx-su.md 4.1, GX_SetScissor: "the screen origin is at the top
		// left corner of the display"). SU_SCIS0 packs suy in bits 11:0 and sux in bits 23:12, SU_SCIS1
		// packs suh and suw the same way. The backend has to convert that into a GL scissor box, whose
		// origin is the bottom left corner, and the rectangle is inclusive on both corners.
		TEST_METHOD(Su_ScissorClipsTheDrawing)
		{
			GfxTestMachine& m = M();

			SetupPassThroughXF(m);
			SetupRasterStage0(m);
			SetupDefaultPixelState(m);
			SetClearColor(m, MakeRgba(0x10, 0x10, 0x10));

			m.BeginFrame();

			// The scissor box: screen (100, 100) to (299, 299), i.e. a 200 x 200 rectangle that does
			// not touch any screen border
			m.BpLoad(SU_SCIS0_ID, (100 + 342) | ((100 + 342) << 12));
			m.BpLoad(SU_SCIS1_ID, (299 + 342) | ((299 + 342) << 12));

			// The unbiased rectangle the emulator reports
			int x, y, w, h;
			m.gfx->su->Scissor(&x, &y, &w, &h);
			Assert::AreEqual(100, x, L"scissor x");
			Assert::AreEqual(100, y, L"scissor y");
			Assert::AreEqual(200, w, L"scissor w");
			Assert::AreEqual(200, h, L"scissor h");

			// A full screen quad in one colour, so every pixel outside the box keeps the clear colour
			DrawScreenQuad(m, MakeRgba(0xFF, 0x40, 0x20));

			// The corners of the rectangle belong to it and the pixels just outside it do not
			Assert::AreEqual(0x40, PixelGreen(m, 100, 100), L"the top left corner is inside the scissor box");
			Assert::AreEqual(0x40, PixelGreen(m, 299, 299), L"the bottom right corner is inside the scissor box");
			Assert::AreEqual(0x10, PixelGreen(m, 99, 200), L"one pixel left of the box is clipped");
			Assert::AreEqual(0x10, PixelGreen(m, 300, 200), L"one pixel right of the box is clipped");

			// The Y axis of the scissor is measured from the top of the screen, while GL measures it
			// from the bottom, so the box covers the screen rows 100 to 299 and nothing else: a backend
			// that forgets the flip draws the mirrored rows 180 to 379 instead.
			Assert::AreEqual(0x40, PixelGreen(m, 200, 110), L"the picture shows the top edge of the box");
			Assert::AreEqual(0x10, PixelGreen(m, 200, 99), L"one pixel above the box is clipped");
			Assert::AreEqual(0x10, PixelGreen(m, 200, 300), L"one pixel below the box is clipped");
			Assert::AreEqual(0x10, PixelGreen(m, 200, SafeMargin), L"the top of the screen is outside the box");
			Assert::AreEqual(0x10, PixelGreen(m, 200, EfbHeight - SafeMargin), L"the bottom of the screen is outside the box");

			// The box really is 200 x 200 pixels
			int drawn = 0;
			for (int column = 0; column < EfbWidth; column++)
			{
				if (PixelGreen(m, column, 200) == 0x40)
					drawn++;
			}
			Assert::AreEqual(200, drawn, L"the width of the scissor box");

			drawn = 0;
			for (int row = 0; row < EfbHeight; row++)
			{
				if (PixelGreen(m, 200, row) == 0x40)
					drawn++;
			}
			Assert::AreEqual(200, drawn, L"the height of the scissor box");
		}

		// A rectangle that reaches outside the screen and an inverted one must not be handed to GL as
		// it is: GL rejects a negative width or height outright (a scissor box with a negative size is
		// an error, not an empty box), so the conversion clamps it.
		TEST_METHOD(Su_AScissorRectangleOutsideTheScreenIsClamped)
		{
			GfxTestMachine& m = M();

			SetupPassThroughXF(m);
			SetupRasterStage0(m);
			SetupDefaultPixelState(m);
			SetClearColor(m, MakeRgba(0x10, 0x10, 0x10));

			m.BeginFrame();

			// A rectangle whose first corner is left at the register reset value, i.e. at the internal
			// origin (-342, -342), which is off the screen: the visible part of the box is the screen
			// rectangle (0, 0)-(200, 200).
			m.BpLoad(SU_SCIS0_ID, 0);
			m.BpLoad(SU_SCIS1_ID, (200 + 342) | ((200 + 342) << 12));

			DrawScreenQuad(m, MakeRgba(0xFF, 0x40, 0x20));

			Assert::AreEqual(0x40, PixelGreen(m, 5, 60), L"the clamped box reaches the top left of the screen");
			Assert::AreEqual(0x40, PixelGreen(m, 200, 200), L"the box reaches (200, 200)");
			Assert::AreEqual(0x10, PixelGreen(m, 201, 200), L"the box ends at x = 200");
			Assert::AreEqual(0x10, PixelGreen(m, 200, 201), L"the box ends at y = 200");

			CheckGLError("a scissor box that starts outside the screen");

			// The register state is not modified by the clamping: it is what the game programmed
			int x, y, w, h;
			m.gfx->su->Scissor(&x, &y, &w, &h);
			Assert::AreEqual(-342, x, L"the reported corner is the register value");
			Assert::AreEqual(-342, y, L"the reported corner Y is the register value");
			Assert::AreEqual(543, w, L"the reported width");
			Assert::AreEqual(543, h, L"the reported height");

			// An inverted rectangle is empty: nothing may be drawn, and GL may not be handed a
			// negative size either
			m.BpLoad(SU_SCIS0_ID, (500 + 342) | ((500 + 342) << 12));
			m.BpLoad(SU_SCIS1_ID, (100 + 342) | ((100 + 342) << 12));

			DrawScreenQuad(m, MakeRgba(0xFF, 0x40, 0x20));

			Assert::AreEqual(0x10, PixelGreen(m, 320, 240), L"an inverted scissor box rejects everything");

			CheckGLError("an inverted scissor box");
		}

		// The scissor has to default to the whole screen: a rectangle left at the register reset value
		// would be the 1x1 box at the internal origin (-342, -342) and would reject every fragment of a
		// scene that never programs the scissor (see SetupUnit::Reset).
		TEST_METHOD(Su_TheScissorDefaultsToTheWholeScreen)
		{
			GfxTestMachine& m = M();

			int x, y, w, h;
			m.gfx->su->Scissor(&x, &y, &w, &h);

			Assert::AreEqual(0, x, L"reset scissor x");
			Assert::AreEqual(0, y, L"reset scissor y");
			Assert::AreEqual(EfbWidth, w, L"reset scissor w");
			Assert::AreEqual(EfbHeight, h, L"reset scissor h");

			SetupPassThroughXF(m);
			SetupRasterStage0(m);
			SetupDefaultPixelState(m);
			SetClearColor(m, MakeRgba(0x10, 0x10, 0x10));

			m.BeginFrame();
			DrawScreenQuad(m, MakeRgba(0xFF, 0x40, 0x20));

			Assert::AreEqual(0x40, PixelGreen(m, 5, SafeMargin), L"the default scissor covers the top left of the screen");
			Assert::AreEqual(0x40, PixelGreen(m, EfbWidth - SafeMargin, SafeMargin), L"the default scissor covers the top right");
			Assert::AreEqual(0x40, PixelGreen(m, 5, EfbHeight - SafeMargin), L"the default scissor covers the bottom left");
			Assert::AreEqual(0x40, PixelGreen(m, EfbWidth - SafeMargin, EfbHeight - SafeMargin), L"the default scissor covers the bottom right");
			Assert::AreEqual(0x40, PixelGreen(m, EfbWidth / 2, EfbHeight / 2), L"the default scissor covers the middle");

			// The registers hold what GX_SetScissor(0, 0, 640, 480) would write
			const GFX::SUState& su = m.gfx->su->State();
			Assert::AreEqual<unsigned>(342, su.scis0.sux, L"reset sux");
			Assert::AreEqual<unsigned>(342, su.scis0.suy, L"reset suy");
			Assert::AreEqual<unsigned>(342 + EfbWidth - 1, su.scis1.suw, L"reset suw");
			Assert::AreEqual<unsigned>(342 + EfbHeight - 1, su.scis1.suh, L"reset suh");
		}

		// The reset rectangle follows the render target when it changes size (a VI mode switch): the
		// registers hold the rectangle in screen coordinates, so the target height is part of the
		// conversion. A rectangle a game programmed is not resized, it is only converted again.
		TEST_METHOD(Su_TheResetScissorFollowsTheRenderTargetSize)
		{
			GfxTestMachine& m = M();

			int x = 0, y = 0, w = 0, h = 0;

			// The reset state at a smaller target
			m.gfx->ResizeRenderTarget(320, 240);
			m.gfx->su->Scissor(&x, &y, &w, &h);
			int resetX = x, resetY = y, resetW = w, resetH = h;

			// A rectangle the game programmed, at the small target
			m.BpLoad(SU_SCIS0_ID, (10 + 342) | ((20 + 342) << 12));
			m.BpLoad(SU_SCIS1_ID, (109 + 342) | ((119 + 342) << 12));

			// Back to the target the rest of the suite uses (this happens before any assertion, so the
			// size is restored even when one of them fails)
			m.gfx->ResizeRenderTarget(EfbWidth, EfbHeight);
			m.gfx->su->Scissor(&x, &y, &w, &h);
			int progX = x, progY = y, progW = w, progH = h;

			Assert::AreEqual(0, resetX, L"the reset rectangle starts at the left edge");
			Assert::AreEqual(0, resetY, L"the reset rectangle starts at the top edge");
			Assert::AreEqual(320, resetW, L"the reset rectangle is as wide as the target");
			Assert::AreEqual(240, resetH, L"the reset rectangle is as tall as the target");

			Assert::AreEqual(20, progX, L"a programmed rectangle keeps its x");
			Assert::AreEqual(10, progY, L"a programmed rectangle keeps its y");
			Assert::AreEqual(100, progW, L"a programmed rectangle keeps its width");
			Assert::AreEqual(100, progH, L"a programmed rectangle keeps its height");
		}

		// =========================================================================================
		// The line and point size (SU_LPSIZE, 0x22)
		// =========================================================================================

		// gfx-su.md 4.1/5.2 with GX_SetLineWidth / GX_SetPointSize: both fields are the size of the
		// primitive in 1/16 pixel increments, and the SU expands the line/point geometry by it. The
		// backend applies them as the GL line width and point size.
		TEST_METHOD(Su_TheLineWidthComesFromTheLinePointSizeRegister)
		{
			GfxTestMachine& m = M();

			// A horizontal line through the middle of the screen, drawn once per line size. The
			// measured thickness of the line is the number of screen rows it covers.
			auto thickness = [&m](int lsize)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetupDefaultPixelState(m);
				SetClearColor(m, MakeRgba(0x10, 0x10, 0x10));

				m.BpLoad(SU_LPSIZE_ID, (uint32_t)(lsize & 0xFF));

				m.BeginFrame();

				// The line lies exactly on the centre of screen row 240 (a window Y of 240.5), so the
				// rows it covers are the rows the line size asks for
				const float y = 1.0f / (float)EfbHeight;

				GFX::Vertex line[2] = {
					GfxTestMachine::MakeVertex(-0.8f, y, 0, 0xFF, 0x40, 0x20, 0xFF),
					GfxTestMachine::MakeVertex(0.8f, y, 0, 0xFF, 0x40, 0x20, 0xFF),
				};
				m.DrawPrimitive(GFX::RAS_LINE, line, 2);

				int count = 0;
				for (int row = 220; row < 260; row++)
				{
					if (PixelGreen(m, EfbWidth / 2, row) > 0x30)
						count++;
				}

				return count;
			};

			// The size is in 1/16 of a pixel: 16 is one pixel, 64 is four, 128 is eight
			Assert::AreEqual(1, thickness(16), L"a line size of one pixel");
			Assert::AreEqual(4, thickness(64), L"a line size of four pixels");
			Assert::AreEqual(8, thickness(128), L"a line size of eight pixels");

			// The register is reset to zero, which is a zero width line; GL always rasterizes at least
			// one pixel, so the floor keeps the line visible instead of turning it into an error
			Assert::AreEqual(1, thickness(0), L"a line size of zero still draws the GL one pixel line");
		}

		TEST_METHOD(Su_ThePointSizeComesFromTheLinePointSizeRegister)
		{
			GfxTestMachine& m = M();

			auto size = [&m](int psize)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetupDefaultPixelState(m);
				SetClearColor(m, MakeRgba(0x10, 0x10, 0x10));

				m.BpLoad(SU_LPSIZE_ID, (uint32_t)((psize & 0xFF) << 8));

				m.BeginFrame();

				GFX::Vertex point = GfxTestMachine::MakeVertex(0.0f, 0.0f, 0, 0xFF, 0x40, 0x20, 0xFF);
				m.DrawPrimitive(GFX::RAS_POINT, &point, 1);

				int count = 0;
				for (int row = 220; row < 260; row++)
				{
					for (int column = 300; column < 340; column++)
					{
						if (PixelGreen(m, column, row) > 0x30)
							count++;
					}
				}

				return count;
			};

			Assert::AreEqual(1, size(16), L"a point size of one pixel");
			Assert::AreEqual(16, size(64), L"a point size of four pixels covers 4 x 4 pixels");
			Assert::AreEqual(64, size(128), L"a point size of eight pixels covers 8 x 8 pixels");
		}

		// =========================================================================================
		// The mip selection of the sampler (TX_SETMODE0.lodbias, TX_SETMODE1.minlod/maxlod)
		// =========================================================================================

		// gfx-tc.md 3.3/4.3/4.4: `lodbias` is an s2.5 value in 1/32 of a level (GX_InitTexObjLOD
		// stores 32 * bias), `minlod`/`maxlod` are 4.4 values in 1/16 of a level (GX_InitTexObjLOD
		// stores 16 * lod). The bias is added to the computed level of detail and the result is
		// clamped between the two limits, so a picture of a high frequency texture shows which level
		// the sampler picked: at level 0 a one-texel checkerboard is still resolved, at any coarser
		// level the texels have been averaged into a flat grey.
		TEST_METHOD(Tex_TheLodLimitsAndTheLodBiasSelectTheMipLevel)
		{
			GfxTestMachine& m = M();

			struct Case
			{
				const char* what;
				int drawSize;			// The size of the quad in EFB pixels (the texture is 64x64)
				int lodBias;			// TX_SETMODE0.lodbias (1/32 of a level)
				int minLod;				// TX_SETMODE1.minlod (1/16 of a level)
				int maxLod;				// TX_SETMODE1.maxlod (1/16 of a level)
				bool expectedDetailed;
			};

			// The texture is drawn 1:1 (a level of detail of about 0) or 3-fold minified (a level of
			// about 1.6; the odd ratio keeps the nearest sampler stepping through both parities of the
			// checkerboard, so a level 0 picture is resolved and a coarser one is not).
			const Case cases[] = {
				{ "1:1, no bias, the whole chain", 64, 0, 0, 160, true },
				{ "1:1, minlod 4", 64, 0, 64, 160, false },
				{ "1:1, bias +4, the whole chain", 64, 127, 0, 160, false },
				{ "1:1, bias +4 but maxlod 0", 64, 127, 0, 0, true },
				{ "3:1, no bias, the whole chain", 21, 0, 0, 160, false },
				{ "3:1, maxlod 0", 21, 0, 0, 0, true },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetupTextureStage0(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				const int size = 64;
				std::vector<uint8_t> raw = EncodeI8Checker(size, 1);

				// TX_SETMODE0: mag filter nearest, min filter 2 = nearest mipmap (an integer level)
				uint32_t mode0 = 0u | (0u << 4) | (2u << 5) | ((uint32_t)(c.lodBias & 0xFF) << 8);
				uint32_t mode1 = (uint32_t)(c.minLod & 0xFF) | ((uint32_t)(c.maxLod & 0xFF) << 8);

				SetupTexture(m, 0, 0x00090000, size, size, GFX::TF_I8, raw.data(), raw.size(), mode0, mode1);

				m.BeginFrame();

				const int x = 320 - c.drawSize / 2;
				const int y = 240 - c.drawSize / 2;
				DrawPixelRect(m, (float)x, (float)y, (float)(x + c.drawSize), (float)(y + c.drawSize),
					MakeRgba(0xFF, 0xFF, 0xFF));

				int spread = GreenSpread(m, x, y, c.drawSize, c.drawSize);

				if (c.expectedDetailed)
				{
					Assert::IsTrue(spread > 100,
						Widen(std::string(c.what) + ": the level 0 checkerboard is not resolved (spread " +
							std::to_string(spread) + ")").c_str());
				}
				else
				{
					Assert::IsTrue(spread < 16,
						Widen(std::string(c.what) + ": the sampler did not pick a coarser level (spread " +
							std::to_string(spread) + ")").c_str());
				}
			}
		}

		// =========================================================================================
		// The texture coordinate scale (SU_SSIZE/SU_TSIZE, 0x30-0x3F)
		// =========================================================================================

		// gfx-su.md 4.5/4.6 and GX_SetTexCoordScaleManually: the register holds the size of the
		// texture minus one, and the SU multiplies the coordinate of that pair by it before the
		// lookup. A smaller scale therefore samples a smaller part of the texture - the picture is
		// magnified - and the scale has to be *composed* with the sampler's padding correction, which
		// maps the real texture size onto the (padded) GL image.
		TEST_METHOD(Su_TheCoordScaleMagnifiesTheTexture)
		{
			GfxTestMachine& m = M();

			// A 12 x 4 texture whose twelve columns are a ramp: the map is padded to 16 x 4 in GL, so
			// the padding correction (ds = 12/16) is part of the picture.
			const int width = 12, height = 4;
			std::vector<int> columns(width);
			for (int i = 0; i < width; i++)
				columns[i] = i * 20;

			std::vector<uint8_t> raw = EncodeI8Columns(width, height, columns, 0);

			auto draw = [&m, &raw, width, height](int scaleS)
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetupTextureStage0(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				SetupTexture(m, 0, 0x000A0000, width, height, GFX::TF_I8, raw.data(), raw.size());

				if (scaleS > 0)
					SetCoordScale(m, 0, scaleS, height);

				m.BeginFrame();
				DrawPixelRect(m, 0, 0, (float)EfbWidth, (float)EfbHeight, MakeRgba(0xFF, 0xFF, 0xFF));
			};

			// 1. The automatic scale: the twelve columns of the real texture are visible and the
			// padded texels (12..15) are not sampled. (The rightmost column of the screen may leak into
			// the padding by a fraction of a texel, which is a property of the sampler's clamp mode and
			// not of the coordinate scale, so the count allows one extra run.)
			draw(0);

			int automatic[8];
			for (int i = 0; i < 8; i++)
			{
				automatic[i] = PixelGreen(m, 40 + i * 80, 240);
			}

			int automaticSteps = GreenSteps(m, 240, 0, EfbWidth);
			Assert::IsTrue(automaticSteps >= 12 && automaticSteps <= 13,
				Widen("the automatic scale shows twelve columns (" + std::to_string(automaticSteps) + ")").c_str());
			Assert::AreEqual(0, automatic[0], L"the left edge of the texture is the first column");
			Assert::AreEqual(220, automatic[7], L"the right edge of the texture is the last column");

			// 2. The scale programmed to the size of the texture is the automatic scale: this is what
			// the GX API writes when the texture order is set up, so it has to leave the picture alone
			// (the padding correction must not be replaced by the scale).
			draw(width);

			for (int i = 0; i < 8; i++)
			{
				Assert::AreEqual(automatic[i], PixelGreen(m, 40 + i * 80, 240),
					Widen("the programmed scale " + std::to_string(width) + " is the automatic scale").c_str());
			}

			// 3. Half the scale: only the first six columns are reachable, stretched over the screen.
			// The sampled column is floor(s * scale), so the picture shows the values 0, 20, ... 100.
			draw(6);

			int half[8];
			for (int i = 0; i < 8; i++)
			{
				half[i] = PixelGreen(m, 40 + i * 80, 240);
			}

			int halfSteps = GreenSteps(m, 240, 0, EfbWidth);
			Assert::IsTrue(halfSteps >= 6 && halfSteps <= 7,
				Widen("the scaled picture shows six columns (" + std::to_string(halfSteps) + ")").c_str());
			Assert::AreEqual(automatic[0], half[0], L"the left edge is the first column either way");
			Assert::IsTrue(abs(half[7] - 100) <= 1,
				Widen("the right edge samples the sixth column (" + std::to_string(half[7]) + ")").c_str());
			Assert::IsTrue(abs(half[4] - 60) <= 1,
				Widen("the middle of the screen samples the third column (" + std::to_string(half[4]) + ")").c_str());
		}

		// =========================================================================================
		// Flat shading (GEN_MODE.flat_en, the rasterized colour path)
		// =========================================================================================

		// GEN_MODE.flat_en gives the colour planes of the primitive zero gradients (gfx-ras2.md 3.1),
		// so the rasterized colour is the same for every pixel of the primitive. The backend expresses
		// that with the `flat` varying qualifier, whose value comes from the provoking vertex of the
		// primitive. One triangle with three different vertex colours is enough to tell the two modes
		// apart: interpolated, the interior is a blend of all three, flat it is exactly one of them.
		TEST_METHOD(Su_FlatShadingGivesThePrimitiveOneColour)
		{
			GfxTestMachine& m = M();

			const uint8_t corners[3][3] = {
				{ 0xF0, 0x10, 0x10 }, { 0x10, 0xF0, 0x10 }, { 0x10, 0x10, 0xF0 },
			};

			// Three points well inside the triangle (-0.8, -0.8), (0.8, -0.8), (0.0, 0.8)
			const float probe[3][2] = { { 0.0f, -0.4f }, { -0.25f, -0.1f }, { 0.25f, -0.05f } };

			auto draw = [&m](bool flat)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetupDefaultPixelState(m);
				SetClearColor(m, MakeRgba(0x00, 0x00, 0x00));

				m.BpLoad(GEN_MODE_ID, flat ? (1u << 8) : 0u);

				m.BeginFrame();

				GFX::Vertex triangle[3] = {
					GfxTestMachine::MakeVertex(-0.8f, -0.8f, 0, 0xF0, 0x10, 0x10, 0xFF),
					GfxTestMachine::MakeVertex(0.8f, -0.8f, 0, 0x10, 0xF0, 0x10, 0xFF),
					GfxTestMachine::MakeVertex(0.0f, 0.8f, 0, 0x10, 0x10, 0xF0, 0xFF),
				};
				m.DrawPrimitive(GFX::RAS_TRIANGLE, triangle, 3);
			};

			// The screen column and row of a clip space point
			auto column = [](float clipX) { return (int)((clipX + 1.0f) * 0.5f * (float)EfbWidth); };
			auto row = [](float clipY) { return (int)((1.0f - clipY) * 0.5f * (float)EfbHeight); };

			// Interpolated: the three points have three different colours
			draw(false);

			uint8_t smooth[3][3];
			for (int i = 0; i < 3; i++)
			{
				ReadPixel(m, column(probe[i][0]), row(probe[i][1]), smooth[i]);
			}

			Assert::IsFalse(PixelsMatch(m, column(probe[0][0]), row(probe[0][1]),
				column(probe[1][0]), row(probe[1][1]), 4),
				L"without flat shading the colour of a triangle is a gradient");

			// Flat: one colour over the whole triangle, and that colour is a vertex colour
			draw(true);

			uint8_t flat[3][3];
			for (int i = 0; i < 3; i++)
			{
				ReadPixel(m, column(probe[i][0]), row(probe[i][1]), flat[i]);
			}

			Assert::IsTrue(PixelsMatch(m, column(probe[0][0]), row(probe[0][1]),
				column(probe[1][0]), row(probe[1][1]), 0),
				L"with flat shading the whole triangle has one colour");
			Assert::IsTrue(PixelsMatch(m, column(probe[1][0]), row(probe[1][1]),
				column(probe[2][0]), row(probe[2][1]), 0),
				L"and the third point of the triangle agrees");

			bool found = false;
			for (const uint8_t* c : corners)
			{
				if (abs((int)flat[0][0] - c[0]) <= 1 && abs((int)flat[0][1] - c[1]) <= 1 &&
					abs((int)flat[0][2] - c[2]) <= 1)
					found = true;
			}

			Assert::IsTrue(found, L"the flat colour of the primitive is one of its vertex colours");
		}

		// =========================================================================================
		// The fog function select (TEV_FOG_PARAM_3.fsel, bits 23:21)
		// =========================================================================================

		// gfx-tev.md 3.6 (the fog law table): the five documented functions are the selects 2, 4, 5, 6
		// and 7. The two encodings the GX API cannot produce are decoded as the family in bits [2:1]
		// plus the "square the value" select in bit 0: 1 is the "off" family with the square bit set,
		// i.e. no fog like 0, and 3 is the linear law applied to the squared value.
		TEST_METHOD(Tev_FogSelectOneIsOff)
		{
			GfxTestMachine& m = M();

			// The fog factor is a function of the window depth of the pixel: the quad used here ramps
			// from depth 0 at the top of the screen to depth 1 at the bottom, A = 1.0 and C = 0.
			auto redOf = [&m](int fsel, int row)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetupDefaultPixelState(m);
				SetClearColor(m, MakeRgba(0, 0, 0));
				SetFog(m, fsel, 1, 0x0000FF);

				m.BeginFrame();
				DrawDepthRampQuad(m, MakeRgba(0xFF, 0xFF, 0xFF));

				uint8_t rgb[3];
				ReadPixel(m, 320, row, rgb);
				return (int)rgb[0];
			};

			// F-select 1: no fog at all, so the quad keeps its own colour at every depth
			for (int row : { 60, 240, 420 })
			{
				Assert::AreEqual(0xFF, redOf(1, row),
					Widen("f-select 1 is unfogged at row " + std::to_string(row)).c_str());
			}

			// ... which is the same picture as with the fog switched off (f-select 0)
			for (int row : { 60, 240, 420 })
			{
				Assert::AreEqual(redOf(1, row), redOf(0, row),
					Widen("f-select 1 and the fog off are the same picture at row " + std::to_string(row)).c_str());
			}
		}

		TEST_METHOD(Tev_FogSelectThreeSquaresTheFogFactor)
		{
			GfxTestMachine& m = M();

			// The fog colour is pure blue, the quad is white, so the red channel of a pixel is
			// 255 * (1 - fog) and identifies the factor directly.
			auto redOf = [&m](int fsel, int row)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetupDefaultPixelState(m);
				SetClearColor(m, MakeRgba(0, 0, 0));
				SetFog(m, fsel, 1, 0x0000FF);

				m.BeginFrame();
				DrawDepthRampQuad(m, MakeRgba(0xFF, 0xFF, 0xFF));

				uint8_t rgb[3];
				ReadPixel(m, 320, row, rgb);
				return (int)rgb[0];
			};

			const int rows[3] = { 120, 240, 360 };

			for (int row : rows)
			{
				// The window depth of a screen row of the ramp quad is (row + 0.5) / 480
				float d = ((float)row + 0.5f) / (float)EfbHeight;
				float expected = 255.0f * (1.0f - d * d);
				int squared = redOf(3, row);

				Assert::IsTrue(abs((float)squared - expected) <= 3.0f,
					Widen("f-select 3 at row " + std::to_string(row) + ": the fog factor is the squared depth (" +
						std::to_string(squared) + " vs " + std::to_string((int)expected) + ")").c_str());

				// The same value on the linear curve (f-select 2) is 1 - d, which is a different curve
				int linear = redOf(2, row);
				Assert::IsTrue(abs(linear - 255.0f * (1.0f - d)) <= 3.0f,
					Widen("f-select 2 at row " + std::to_string(row) + " is the linear law").c_str());
				Assert::IsTrue(squared > linear + 10,
					Widen("the squared curve fogs less than the linear one at row " + std::to_string(row)).c_str());
			}
		}

		// =========================================================================================
		// BUMP_IMASK
		// =========================================================================================

		// BUMP_IMASK (0x0F) is the bump unit's stream-classification mask, not a mask of the indirect
		// texel components: the entry stage routes a word of the texture group on the short path when
		// the mask bit selected by the word's tag is set (gfx-bump.md 3.2/4.3), and the GX API fills
		// it with the texture maps its indirect stages fetch from (libogc __GX_UpdateBPMask). The
		// backend has no command-stream router - every BP word lands in the register file at the
		// moment it arrives - so the mask cannot change the picture. This test pins that down: the
		// register is decoded field by field and the indirect picture is the same for any mask.
		TEST_METHOD(Bump_ImaskIsDecodedButDoesNotChangeTheIndirectPicture)
		{
			GfxTestMachine& m = M();

			// A 64 x 64 texture with a coarse red staircase, sampled through an indirect stage whose
			// matrix shifts the coordinate, so the picture depends on the indirect arithmetic.
			std::vector<uint8_t> ramp;
			for (int t = 0; t < 64; t += 4)
				for (int s = 0; s < 64; s += 4)
					for (int v = 0; v < 4; v++)
						for (int u = 0; u < 4; u++)
						{
							int x = s + u;
							int r = ((x / 4) * 16) >> 3;
							uint16_t texel = (uint16_t)((r << 11) | 0x1Fu);
							ramp.push_back((uint8_t)(texel >> 8));
							ramp.push_back((uint8_t)texel);
						}

			auto draw = [&](uint32_t imask)
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				SetupTexture(m, 0, 0x000B0000, 64, 64, GFX::TF_RGB565, ramp.data(), ramp.size(),
					1u | (1u << 2));

				// Matrix 0: ma = 0x3FF with scale 14, i.e. a visible coordinate offset
				m.BpLoad(BUMP_MATRIX_A0_ID, 0x3FFu | (2u << 22));
				m.BpLoad(BUMP_MATRIX_B0_ID, 0u | (3u << 22));
				m.BpLoad(BUMP_MATRIX_C0_ID, 0u);

				m.BpLoad(BUMP_IMASK_ID, imask);
				m.BpLoad(BUMP_CMD_ID, (1u << 9) | (4u << 13) | (4u << 16));

				SetupTextureStage0(m, 0);
				m.BeginFrame();

				DrawClipQuad(m, -1, -1, 1, 1, MakeRgba(0xFF, 0xFF, 0xFF), MakeRgba(0xFF, 0xFF, 0xFF),
					MakeRgba(0xFF, 0xFF, 0xFF), MakeRgba(0xFF, 0xFF, 0xFF), 0.0f, 0.0f, 1.0f, 1.0f);

				std::vector<uint8_t> rgb;
				m.ReadColor(0, 0, EfbWidth, EfbHeight, rgb);
				return rgb;
			};

			// The mask is decoded into its register (gfx-bump.md 4.3: bits 7:0)
			m.BpLoad(BUMP_IMASK_ID, 0x000000A5);
			Assert::AreEqual<unsigned>(0xA5, m.gfx->bump->State().imask.imask, L"imask");

			std::vector<uint8_t> off = draw(0x00);
			std::vector<uint8_t> all = draw(0xFF);

			Assert::IsTrue(off == all,
				L"the indirect mask is a routing control: it does not change the picture");
		}
	};
}
