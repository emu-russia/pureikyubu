// Bump / indirect texturing tests.
//
// The bump unit is the indirect stage of the texture unit: it samples an "indirect" texture, turns
// the texel fields into a coordinate offset through one of the three 3x2 matrices and adds that
// offset to the stage's texture coordinate before the TEV samples the stage's own texture
// (gfx-bump.md 3.3).
//
// The test below programs the whole path with a texture whose texels are a known ramp, so the
// sampled texel (and therefore the pixel colour) only changes if the offset arithmetic works.

#include "pch.h"
#include "gfx_test_common.h"

using namespace GfxUnitTest;

namespace pureikyubutest
{
	TEST_CLASS(GfxBumpTest)
	{
		//! Where the test texture lives in the emulated main memory.
		static const uint32_t TextureAddr = 0x00080000;

		static GfxTestMachine& M()
		{
			GfxTestMachine& m = Machine();
			m.Reset();
			return m;
		}

		static uint32_t ColorEnv(int sela, int selb, int selc, int seld, int clamp, int dest)
		{
			return (uint32_t)seld | ((uint32_t)selc << 4) | ((uint32_t)selb << 8) | ((uint32_t)sela << 12) |
				((uint32_t)clamp << 19) | ((uint32_t)dest << 22);
		}

		static uint32_t AlphaEnv(int sela, int selb, int selc, int seld, int clamp, int dest)
		{
			return (uint32_t)seld << 4 | (uint32_t)selc << 7 | (uint32_t)selb << 10 | (uint32_t)sela << 13 |
				((uint32_t)clamp << 19) | ((uint32_t)dest << 22);
		}

		//! A 4x4 intensity (I8) texture: texel (x, y) carries 0x10 + x * 0x40.
		static void SetupRampTexture(GfxTestMachine& m)
		{
			uint8_t image[32];

			for (int y = 0; y < 4; y++)
			{
				for (int x = 0; x < 8; x++)
				{
					// The I8 tile is 4 rows of 8 texels; only the first four columns exist here
					image[y * 8 + x] = (x < 4) ? (uint8_t)(0x10 + x * 0x40) : 0;
				}
			}

			WriteMainMemory(TextureAddr, image, sizeof(image));

			// TX_SETIMAGE0_0: width-1, height-1, format = I8 (1)
			m.BpLoad(TX_SETIMAGE0_I0_ID, 3u | (3u << 10) | ((uint32_t)GFX::TF_I8 << 20));

			// TX_SETIMAGE3_0: the physical base in bits 25:5
			m.BpLoad(TX_SETIMAGE3_I0_ID, TextureAddr >> 5);

			// TX_SETMODE0_0: nearest filtering and clamping
			m.BpLoad(TX_SETMODE0_I0_ID, 0);
			m.BpLoad(TX_SETMODE1_I0_ID, 0);
		}

		//! A pass-through XF: the vertex position and the texture coordinate arrive unchanged.
		static void SetupPassThrough(GfxTestMachine& m)
		{
			float identity[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
			m.XfLoadFloats(GFX::XF_MATRIX_MEMORY_ID, identity, 16);
			float proj[6] = { 1,0, 1,0, 1,0 };
			m.XfLoadFloats(GFX::XF_PROJECTION_A_ID, proj, 6);
			m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, 0x3f800000);
			m.XfLoad(GFX::XF_NUMCOLS_ID, 0);
			m.XfLoad(GFX::XF_NUMTEX_ID, 0);			// the host coordinates pass through

			m.BpLoad(PE_ZMODE_ID, 0);
			m.BpLoad(PE_CMODE0_ID, 0x18);
		}

		//! Stage 0 samples texture 0 with the coordinate texgen handed over; the texel is the output.
		static void SetupStage0(GfxTestMachine& m)
		{
			m.BpLoad(GEN_MODE_ID, 0);				// one TEV stage
			m.BpLoad(RAS1_TREF0_ID, 1u << 6);		// te0 = 1, ti0 = 0, tc0 = 0, cc0 = 0
			m.BpLoad(TEV_COLOR_ENV_0_ID, ColorEnv(15, 15, 15, 8, 1, 0));	// d = texel colour
			m.BpLoad(TEV_ALPHA_ENV_0_ID, AlphaEnv(4, 7, 7, 4, 1, 0));		// alpha = texel alpha
		}

		//! Program the indirect command of stage 0 and the matrix it uses.
		static void SetupIndirect(GfxTestMachine& m, uint32_t mode, uint32_t fmt, uint32_t bias,
			uint32_t wrapS = 0, uint32_t wrapT = 0)
		{
			// bt = 0 (the indirect map is texture 0 as well), and the matrix select `mode`
			m.BpLoad(BUMP_CMD_ID + 0, 0u | (fmt << 2) | (bias << 4) | (mode << 9) |
				(wrapS << 13) | (wrapT << 16));
		}

		//! Matrix 0: ma = 0x3FF (the largest positive S1.10 entry), mc = md = me = mf = 0, and a
		//! scale of 14 (2 + 3 << 2 + 0 << 4).
		static void SetupMatrix0(GfxTestMachine& m, uint32_t ma, uint32_t scaleA, uint32_t scaleB)
		{
			m.BpLoad(BUMP_MATRIX_A0_ID, (ma & 0x7ff) | (0u << 11) | ((scaleA & 3) << 22));
			m.BpLoad(BUMP_MATRIX_B0_ID, 0u | (0u << 11) | ((scaleB & 3) << 22));
			m.BpLoad(BUMP_MATRIX_C0_ID, 0);
		}

		//! Draw one quad that covers the whole EFB, sampling the texture at (0.125, 0.125) =
		//! texel (0, 0).
		static void DrawQuad(GfxTestMachine& m)
		{
			GFX::Vertex quad[4];
			for (int i = 0; i < 4; i++)
			{
				quad[i] = GfxTestMachine::MakeVertex(0, 0, 0, 0xff, 0xff, 0xff, 0xff);
				quad[i].TexCoord[0][0] = 0.125f;
				quad[i].TexCoord[0][1] = 0.125f;
			}

			quad[0].Position[0] = -1; quad[0].Position[1] = -1;
			quad[1].Position[0] = 1; quad[1].Position[1] = -1;
			quad[2].Position[0] = 1; quad[2].Position[1] = 1;
			quad[3].Position[0] = -1; quad[3].Position[1] = 1;

			m.DrawQuad(quad);
		}

	public:

		// An image of the offset: the same ramp texture is sampled by two quads, the left one without
		// the indirect stage and the right one with it.
		TEST_METHOD(Bump_Image_IndirectOffset)
		{
			RequireGL();
			GfxTestMachine& m = M();

			Report::Section("Indirect texturing",
				"The bump unit turns a texel of an indirect texture into a coordinate offset. Below, the same 4x4\n"
				"ramp texture is sampled twice: on the left through a plain TEV stage, on the right through an\n"
				"indirect stage whose matrix shifts the coordinate by about two texels.");

			SetupPassThrough(m);
			SetupRampTexture(m);
			SetupStage0(m);
			SetupMatrix0(m, 0x3FF, 2, 3);

			// The left half has no indirect command, the right half uses matrix 1
			SetupIndirect(m, 0, 0, 0);

			m.BeginFrame();

			GFX::Vertex quad[4];
			for (int i = 0; i < 4; i++)
			{
				quad[i] = GfxTestMachine::MakeVertex(0, 0, 0, 0xff, 0xff, 0xff, 0xff);
				// The texture coordinate spans the whole ramp so the shift is visible as a slide
				quad[i].TexCoord[0][0] = (i == 0 || i == 3) ? 0.0f : 1.0f;
				quad[i].TexCoord[0][1] = 0.0f;
			}

			// The left half, without the indirect offset
			quad[0].Position[0] = -1; quad[0].Position[1] = -1;
			quad[1].Position[0] = 0; quad[1].Position[1] = -1;
			quad[2].Position[0] = 0; quad[2].Position[1] = 1;
			quad[3].Position[0] = -1; quad[3].Position[1] = 1;
			m.DrawQuad(quad);

			// The right half, with the indirect offset of matrix 1
			SetupIndirect(m, 1, 0, 0);
			quad[0].Position[0] = 0; quad[0].Position[1] = -1;
			quad[1].Position[0] = 1; quad[1].Position[1] = -1;
			quad[2].Position[0] = 1; quad[2].Position[1] = 1;
			quad[3].Position[0] = 0; quad[3].Position[1] = 1;
			m.DrawQuad(quad);

			std::string file = "bump_indirect.png";
			Assert::IsTrue(m.SaveScreenshot(OutputDir() + "/" + file, 0, 0, 640, 480, 2),
				L"the screenshot was not saved");

			Report::Image("Ramp texture with and without the indirect offset", file,
				"Left: the ramp as it is. Right: the same ramp after the indirect stage shifted the coordinate");
		}

		// =========================================================================================
		// The register file
		// =========================================================================================

		TEST_METHOD(Bump_TheTevProgramStillCompilesWithTheIndirectCode)
		{
			RequireGL();
			GfxTestMachine& m = M();

			ClearTestLog();
			EnableTestLog(true);
			GFX::GLProgram* program = m.gfx->tev->GetTevProgram();
			std::string log = TestLogText();
			EnableTestLog(false);
			Logger::WriteMessage(log.c_str());

			Assert::IsNotNull(program, L"the TEV program did not build (the log is above)");
		}

		TEST_METHOD(Bump_TheIndirectCommandIsDecodedFieldByField)
		{
			GfxTestMachine& m = M();

			uint32_t value = 2u | (1u << 2) | (5u << 4) | (3u << 7) | (11u << 9) |
				(4u << 13) | (2u << 16) | (1u << 19) | (1u << 20);
			m.BpLoad(BUMP_CMD_ID + 7, value);

			const GFX::BumpCommand& cmd = m.gfx->bump->State().cmd[7];
			Assert::AreEqual<unsigned>(2, cmd.bt, L"bt");
			Assert::AreEqual<unsigned>(1, cmd.fmt, L"fmt");
			Assert::AreEqual<unsigned>(5, cmd.bias, L"bias");
			Assert::AreEqual<unsigned>(3, cmd.bs, L"bs");
			Assert::AreEqual<unsigned>(11, cmd.m, L"m");
			Assert::AreEqual<unsigned>(4, cmd.sw, L"sw");
			Assert::AreEqual<unsigned>(2, cmd.tw, L"tw");
			Assert::AreEqual<unsigned>(1, cmd.lb, L"lb");
			Assert::AreEqual<unsigned>(1, cmd.fb, L"fb");

			Assert::IsTrue(m.gfx->bump->IndirectActive(), L"the unit reports an active indirect stage");
		}

		TEST_METHOD(Bump_TheMatrixRegistersAreDecoded)
		{
			GfxTestMachine& m = M();

			// A: ma = 0x123, mb = 0x456, scale = 2; B: mc = 0x0AB, md = 0x7FF, scale = 3;
			// C: me = 0x1, mf = 0x2, scale bit = 1
			m.BpLoad(BUMP_MATRIX_A0_ID, 0x123 | (0x456u << 11) | (2u << 22));
			m.BpLoad(BUMP_MATRIX_B0_ID, 0x0AB | (0x7FFu << 11) | (3u << 22));
			m.BpLoad(BUMP_MATRIX_C0_ID, 0x1 | (0x2u << 11) | (1u << 22));
			m.BpLoad(BUMP_IMASK_ID, 0x55);

			const GFX::BUMPState& bump = m.gfx->bump->State();
			Assert::AreEqual<unsigned>(0x123, bump.matrix[0].a.ma, L"ma");
			Assert::AreEqual<unsigned>(0x456, bump.matrix[0].a.mb, L"mb");
			Assert::AreEqual<unsigned>(2, bump.matrix[0].a.s, L"scale A");
			Assert::AreEqual<unsigned>(0x0AB, bump.matrix[0].b.mc, L"mc");
			Assert::AreEqual<unsigned>(0x7FF, bump.matrix[0].b.md, L"md");
			Assert::AreEqual<unsigned>(3, bump.matrix[0].b.s, L"scale B");
			Assert::AreEqual<unsigned>(0x1, bump.matrix[0].c.me, L"me");
			Assert::AreEqual<unsigned>(0x2, bump.matrix[0].c.mf, L"mf");
			Assert::AreEqual<unsigned>(1, bump.matrix[0].c.s, L"scale C");
			Assert::AreEqual<unsigned>(0x55, bump.imask.imask, L"imask");
		}

		// =========================================================================================
		// The coordinate arithmetic (rendered)
		// =========================================================================================

		// Without an indirect command the stage samples texel 0 of the ramp texture.
		TEST_METHOD(Bump_WithoutAnIndirectStageTheCoordinateIsUntouched)
		{
			RequireGL();
			GfxTestMachine& m = M();

			SetupPassThrough(m);
			SetupRampTexture(m);
			SetupStage0(m);

			m.BeginFrame();
			DrawQuad(m);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);

			const GFX::TexMap& map = m.gfx->tx->Map(0);
			char diag[256];
			sprintf_s(diag, "valid=%d %dx%d gl=%dx%d scale=%.2f tref=0x%X",
				map.valid ? 1 : 0, map.width, map.height, map.dw, map.dh, map.ds,
				m.gfx->ras->Tref(0).bits);

			Assert::AreEqual<int>(0x10, rgb[0], Widen(diag).c_str());
		}

		// With the indirect stage the coordinate is shifted by the offset the texel produces, so a
		// different texel of the ramp is sampled.
		//
		// The arithmetic of the setup (gfx-bump.md 3.3, all in S17.7 units where 1.0 = 128):
		//   s        = the texel field, ch0 of an 8-bit indirect texel = 0x10 = 16
		//   dot      = s * ma = 16 * 1023 = 16368
		//   offset   = (dot << scale)[44:20] = floor(16368 * 2^14 / 2^20) = 255
		//   255 units of 1/128 texel = about two texels to the right
		TEST_METHOD(Bump_TheIndirectStageShiftsTheSampledTexel)
		{
			RequireGL();
			GfxTestMachine& m = M();

			SetupPassThrough(m);
			SetupRampTexture(m);
			SetupStage0(m);

			// fmt = 0 (8 bit, no bias), matrix 1, no wrap, scale = 2 + (3 << 2) = 14
			SetupIndirect(m, 1, 0, 0);
			SetupMatrix0(m, 0x3FF, 2, 3);

			m.BeginFrame();
			DrawQuad(m);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);

			// The ramp is 0x10 + x * 0x40, and the offset moves the sample to x = 2
			Assert::AreEqual<int>(0x90, rgb[0], L"texel 2 of the ramp (0x10 + 2 * 0x40)");
		}

		// The matrix off mode leaves the coordinate alone even though the stage is programmed.
		TEST_METHOD(Bump_TheMatrixOffModeLeavesTheCoordinateAlone)
		{
			RequireGL();
			GfxTestMachine& m = M();

			SetupPassThrough(m);
			SetupRampTexture(m);
			SetupStage0(m);

			SetupIndirect(m, 0, 0, 0);				// bp_m_off
			SetupMatrix0(m, 0x3FF, 2, 3);

			m.BeginFrame();
			DrawQuad(m);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);
			Assert::AreEqual<int>(0x10, rgb[0], L"no offset with the matrix off");
		}

		// Negative offsets come from the biased 8-bit form: with the bias bit set the texel field is
		// centred on zero, so a texel of 0x10 becomes -0x70 and the coordinate moves the other way.
		TEST_METHOD(Bump_TheBiasedTexelFieldShiftsTheCoordinateTheOtherWay)
		{
			RequireGL();
			GfxTestMachine& m = M();

			SetupPassThrough(m);
			SetupRampTexture(m);
			SetupStage0(m);

			// The sampler must not clamp the negative coordinate away: wrap instead
			m.BpLoad(TX_SETMODE0_I0_ID, (1u << 0) | (1u << 2));		// wrap_s = wrap_t = repeat

			// fmt = 0, bias = 1 (the s component is centred), matrix 1, scale 14
			SetupIndirect(m, 1, 0, 1);
			SetupMatrix0(m, 0x3FF, 2, 3);

			m.BeginFrame();
			DrawQuad(m);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);

			// s = 0x10 - 0x80 = -112, dot = -112 * 1023 = -114576, offset = floor(-114576 * 2^14 / 2^20)
			// = -1790, i.e. about -14 texels; with REPEAT on a 4-texel texture that lands on texel 2
			Assert::AreEqual<int>(0x90, rgb[0], L"the negative offset wraps around the texture");
		}
	};
}
