// TEV (Texture Environment Unit) tests.
//
// Two independent views of the same block:
//
//   * the register file is checked word by word (all 0xC0-0xFD ids must land in the right slot and
//     in the right field);
//   * the combine itself is executed for real - the pipeline draws a quad and the pixel the TEV
//     produced is read back from the EFB - and compared against the reference model in this file,
//     which is written from the specification (gfx-tev.md 3.2, 4.2, 4.3, 4.8, 4.10) instead of from
//     the emulator's shader.
//
// The tests that render also publish their images into the HTML report (Report::Image).

#include "pch.h"
#include "gfx_test_common.h"

using namespace GfxUnitTest;

namespace pureikyubutest
{
	// =============================================================================================
	// The reference model
	//
	// gfx-tev.md 3.2:   result = ( D +/- lerp(A, B, C) + bias ) << shift , then clamped
	//                   lerp(A, B, C) = (1 - C) * A + C * B
	//
	// Everything is carried in the same units the hardware uses: the colour values are 0..255 (the
	// design treats 255 as 1.0), the blend factor C is an 8-bit fraction and the bias is a half unit
	// of the u12.8 intermediate, i.e. 128. The result is quantised to an integer because it is
	// stored back into an 11-bit signed colour register.
	// =============================================================================================

	namespace TevRef
	{
		float Combine(float a, float b, float c, float d, int bias, int shift, bool sub, bool clampLow)
		{
			float lerp = a + (c / 255.0f) * (b - a);

			float r = sub ? (d - lerp) : (d + lerp);

			if (bias == 1) r += 128.0f;
			else if (bias == 2) r -= 128.0f;

			if (shift == 1) r *= 2.0f;
			else if (shift == 2) r *= 4.0f;
			else if (shift == 3) r *= 0.5f;

			r = clampLow ? max(0.0f, min(255.0f, r)) : max(-1024.0f, min(1023.0f, r));

			return floorf(r + 0.5f);
		}

		//! The four 8-bit operand values of a colour register: .rgb = colour, .a = alpha.
		struct Reg
		{
			float rgb[3];
			float a;
		};

		//! Colour operand select (tev_csel, gfx-tev.md 3.2 table).
		float Csel(int sel, int component, const Reg reg[4], const float texel[4], const float raster[4])
		{
			if (sel < 8)
			{
				const Reg& r = reg[sel >> 1];
				float v = ((sel & 1) == 0) ? r.rgb[component] : r.a;
				// A/B/C use the low 8 bits of the stored 11-bit component
				return v - floorf(v / 256.0f) * 256.0f;
			}

			switch (sel)
			{
				case 8: return texel[component];
				case 9: return texel[3];
				case 10: return raster[component];
				case 11: return raster[3];
				case 12: return 255.0f;		// 1.0
				case 13: return 128.0f;		// 0.5
				case 15: return 0.0f;		// 0.0
			}

			return 0.0f;					// 14 = KONST: not part of this model
		}

		//! Alpha operand select (tev_asel).
		float Asel(int sel, const Reg reg[4], const float texel[4], const float raster[4])
		{
			if (sel < 4)
			{
				return reg[sel].a;
			}

			switch (sel)
			{
				case 4: return texel[3];
				case 5: return raster[3];
				case 6: return 255.0f;		// KONST: not part of this model
				case 7: return 0.0f;
			}

			return 0.0f;
		}

		//! One stage: returns the colour result (rgb) and the alpha result.
		void Stage(const GFX::TEV_ColorEnv& ce, const GFX::TEV_AlphaEnv& ae,
			const Reg reg[4], const float texel[4], const float raster[4],
			float outRgb[3], float* outA)
		{
			for (int i = 0; i < 3; i++)
			{
				float a = Csel(ce.sela, i, reg, texel, raster);
				float b = Csel(ce.selb, i, reg, texel, raster);
				float c = Csel(ce.selc, i, reg, texel, raster);
				float d = Csel(ce.seld, i, reg, texel, raster);

				outRgb[i] = Combine(a, b, c, d, ce.bias, ce.shift, ce.sub != 0, ce.clamp != 0);
			}

			float a = Asel(ae.sela, reg, texel, raster);
			float b = Asel(ae.selb, reg, texel, raster);
			float c = Asel(ae.selc, reg, texel, raster);
			float d = Asel(ae.seld, reg, texel, raster);

			*outA = Combine(a, b, c, d, ae.bias, ae.shift, ae.sub != 0, ae.clamp != 0);
		}
	}

	TEST_CLASS(GfxTevTest)
	{
		static GfxTestMachine& M()
		{
			GfxTestMachine& m = Machine();
			m.Reset();
			return m;
		}

		// -----------------------------------------------------------------------------------------
		// Pipeline setup used by every rendering test
		// -----------------------------------------------------------------------------------------

		//! A pass-through XF: the vertex position reaches the clip space unchanged and the host
		//! colours are handed over to the TEV without lighting.
		static void SetupPassThroughXF(GfxTestMachine& m)
		{
			float identity[16] = {
				1,0,0,0,
				0,1,0,0,
				0,0,1,0,
				0,0,0,1,
			};
			m.XfLoadFloats(GFX::XF_MATRIX_MEMORY_ID, identity, 16);

			float identityNrm[9] = { 1,0,0, 0,1,0, 0,0,1 };
			m.XfLoadFloats(GFX::XF_NORMAL_MATRIX_MEMORY_ID, identityNrm, 9);

			float proj[6] = { 1,0, 1,0, 1,0 };
			m.XfLoadFloats(GFX::XF_PROJECTION_A_ID, proj, 6);
			m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, 0x3f800000);		// 1.0f

			m.XfLoad(GFX::XF_NUMCOLS_ID, 0);					// host colours pass through
			m.XfLoad(GFX::XF_NUMTEX_ID, 0);
			m.XfLoad(GFX::XF_MATINDEX_A_ID, 0);
		}

		//! The TEV stage 0 reads its "rasterized colour" operand from the vertex colour 0.
		static void SetupRasterColorSource(GfxTestMachine& m)
		{
			m.BpLoad(RAS1_TREF0_ID, 0);		// te = 0, cc0 = 0 -> rasterized colour 0
		}

		static void SetupDefaultPixelState(GfxTestMachine& m)
		{
			m.BpLoad(PE_ZMODE_ID, 0);		// no depth test
			m.BpLoad(PE_CMODE0_ID, 0x18);		// no blend, no logic op
		}

		//! A full screen quad that covers the whole EFB, in fan order.
		static void DrawFullScreenQuad(GfxTestMachine& m, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
		{
			GFX::Vertex quad[4] = {
				GfxTestMachine::MakeVertex(-1, -1, 0, r, g, b, a),
				GfxTestMachine::MakeVertex(1, -1, 0, r, g, b, a),
				GfxTestMachine::MakeVertex(1, 1, 0, r, g, b, a),
				GfxTestMachine::MakeVertex(-1, 1, 0, r, g, b, a),
			};
			m.DrawQuad(quad);
		}

		//! One TEV stage that outputs the rasterized colour and its alpha unchanged.
		static void SetupPassThroughTev(GfxTestMachine& m)
		{
			m.BpLoad(GEN_MODE_ID, 0 << 9);					// ntev = 0 -> one stage
			m.BpLoad(TEV_COLOR_ENV_0_ID,
				(15u << 12) |		// sela = 0.0
				(15u << 8) |		// selb = 0.0
				(15u << 4) |		// selc = 0.0
				(10u << 0) |		// seld = rasterized colour
				(1u << 19) |		// clamp
				(0u << 22));		// dest = register 0
			m.BpLoad(TEV_ALPHA_ENV_0_ID,
				(5u << 13) | (5u << 10) | (5u << 7) | (5u << 4) |	// all operands = rasterized alpha
				(1u << 19) | (0u << 22));
		}

		static uint32_t PackColorEnv(int sela, int selb, int selc, int seld, int bias, int sub, int clamp,
			int shift, int dest)
		{
			return (uint32_t)seld | ((uint32_t)selc << 4) | ((uint32_t)selb << 8) | ((uint32_t)sela << 12) |
				((uint32_t)bias << 16) | ((uint32_t)sub << 18) | ((uint32_t)clamp << 19) |
				((uint32_t)shift << 20) | ((uint32_t)dest << 22);
		}

		// TEV_KSEL_n packs, per stage pair: xrb 1:0, xga 3:2, kcsel0 8:4, kasel0 13:9,
		// kcsel1 18:14, kasel1 23:19
		static uint32_t PackKsel(int kcsel0, int kasel0, int kcsel1, int kasel1)
		{
			return ((uint32_t)kcsel0 << 4) | ((uint32_t)kasel0 << 9) |
				((uint32_t)kcsel1 << 14) | ((uint32_t)kasel1 << 19);
		}

		static uint32_t PackAlphaEnv(int sela, int selb, int selc, int seld, int bias, int sub, int clamp,
			int shift, int dest, int mode = 0, int swap = 0)
		{
			return (uint32_t)mode | ((uint32_t)swap << 2) | ((uint32_t)seld << 4) | ((uint32_t)selc << 7) |				((uint32_t)selb << 10) | ((uint32_t)sela << 13) | ((uint32_t)bias << 16) |
				((uint32_t)sub << 18) | ((uint32_t)clamp << 19) | ((uint32_t)shift << 20) |
				((uint32_t)dest << 22);
		}

		static GFX::TEV_ColorEnv DecodeColorEnv(uint32_t bits)
		{
			GFX::TEV_ColorEnv e = {};
			e.bits = bits;
			return e;
		}

		static GFX::TEV_AlphaEnv DecodeAlphaEnv(uint32_t bits)
		{
			GFX::TEV_AlphaEnv e = {};
			e.bits = bits;
			return e;
		}

	public:

		// =========================================================================================
		// The register file
		// =========================================================================================

		TEST_METHOD(Tev_EveryStageEnvironmentRegisterIsDecoded)
		{
			GfxTestMachine& m = M();

			for (unsigned stage = 0; stage < 16; stage++)
			{
				uint32_t colorValue = PackColorEnv(stage & 0xf, (stage + 1) & 0xf, (stage + 2) & 0xf,
					(stage + 3) & 0xf, stage & 3, (stage >> 2) & 1, 1, (stage >> 3) & 3, stage & 3);
				uint32_t alphaValue = PackAlphaEnv((stage + 4) & 7, (stage + 5) & 7, (stage + 6) & 7,
					stage & 7, (stage + 1) & 3, (stage >> 1) & 1, 1, (stage >> 2) & 3, stage & 3);

				m.BpLoad(TEV_COLOR_ENV_0_ID + stage * 2, colorValue);
				m.BpLoad(TEV_ALPHA_ENV_0_ID + stage * 2, alphaValue);

				const GFX::TEVState& tev = m.gfx->tev->State();

				wchar_t msg[64];
				swprintf_s(msg, L"stage %u colour environment", stage);
				Assert::AreEqual<uint32_t>(colorValue & 0xFFFFFF, tev.color_env[stage].bits & 0xFFFFFF, msg);

				swprintf_s(msg, L"stage %u alpha environment", stage);
				Assert::AreEqual<uint32_t>(alphaValue & 0xFFFFFF, tev.alpha_env[stage].bits & 0xFFFFFF, msg);
			}
		}

		TEST_METHOD(Tev_ColourAndConstantRegistersAreDecoded)
		{
			GfxTestMachine& m = M();

			for (unsigned i = 0; i < 4; i++)
			{
				uint32_t l = (0x100 + i) | ((0x200 + i) << 12);
				uint32_t h = (0x300 + i) | ((0x400 + i) << 12);

				m.BpLoad(TEV_REGISTERL_0_ID + i * 2, l);
				m.BpLoad(TEV_REGISTERH_0_ID + i * 2, h);

				const GFX::TEVState& tev = m.gfx->tev->State();
				Assert::AreEqual<unsigned>(0x100 + i, tev.regl[i].r, L"REGISTERL.r");
				Assert::AreEqual<unsigned>(0x200 + i, tev.regl[i].a, L"REGISTERL.a");
				Assert::AreEqual<unsigned>(0x300 + i, tev.regh[i].b, L"REGISTERH.b");
				Assert::AreEqual<unsigned>(0x400 + i, tev.regh[i].g, L"REGISTERH.g");
			}
		}

		TEST_METHOD(Tev_FogRangeAndAlphaRegistersAreDecoded)
		{
			GfxTestMachine& m = M();

			m.BpLoad(TEV_FOG_PARAM_0_ID, 0x00123456);
			m.BpLoad(TEV_FOG_PARAM_1_ID, 0x00654321);
			m.BpLoad(TEV_FOG_PARAM_2_ID, 0x00000003);
			m.BpLoad(TEV_FOG_PARAM_3_ID, 0x00100000);
			m.BpLoad(TEV_FOG_COLOR_ID, 0x00112233);
			m.BpLoad(TEV_RANGE_ADJ_C_ID, 0x00000200);
			m.BpLoad(TEV_RANGE_ADJ_0_ID, 0x00000123);
			m.BpLoad(TEV_Z_ENV_0_ID, 0x0000abcd);
			m.BpLoad(TEV_Z_ENV_1_ID, 0x00000005);

			const GFX::TEVState& tev = m.gfx->tev->State();
			Assert::AreEqual<unsigned>(0x456, tev.fog_param0.a_mant, L"fog a_mant");
			Assert::AreEqual<uint32_t>(0x00654321, tev.fog_param1.b_mag, L"fog b_mag");
			Assert::AreEqual<unsigned>(3, tev.fog_param2.b_shft, L"fog b_shft");
			Assert::AreEqual<unsigned>(0, tev.fog_param3.c_mant, L"fog c_mant");
			Assert::AreEqual<unsigned>(0, tev.fog_param3.c_expn, L"fog c_expn");
			Assert::AreEqual<unsigned>(0, tev.fog_param3.c_sign, L"fog c_sign");
			Assert::AreEqual<unsigned>(1, tev.fog_param3.proj, L"fog proj");
			Assert::AreEqual<unsigned>(0, tev.fog_param3.fsel, L"fog fsel");
			Assert::AreEqual<unsigned>(0x33, tev.fog_color.b, L"fog colour b");
			Assert::AreEqual<unsigned>(0x22, tev.fog_color.g, L"fog colour g");
			Assert::AreEqual<unsigned>(0x11, tev.fog_color.r, L"fog colour r");
			Assert::AreEqual<unsigned>(0x200, tev.rangeadj_control.center, L"range adj centre");
			Assert::AreEqual<unsigned>(0x123, tev.range_adj[0].r0, L"range adj r0");
			Assert::AreEqual<unsigned>(0xabcd, tev.zenv0.zoff, L"z env zoff");
			Assert::AreEqual<unsigned>(1, tev.zenv1.type, L"z env type");
			Assert::AreEqual<unsigned>(1, tev.zenv1.op, L"z env op");
		}

		TEST_METHOD(Tev_AlphaFuncAndKSelectRegistersAreDecoded)
		{
			GfxTestMachine& m = M();

			// a0 = 0x40, a1 = 0x80, op0 = 3, op1 = 6, logic = 2
			m.BpLoad(TEV_ALPHAFUNC_ID, 0x40 | (0x80 << 8) | (3u << 16) | (6u << 19) | (2u << 22));

			const GFX::TEVState& tev = m.gfx->tev->State();
			Assert::AreEqual<unsigned>(0x40, tev.alpha_func.a0, L"a0");
			Assert::AreEqual<unsigned>(0x80, tev.alpha_func.a1, L"a1");
			Assert::AreEqual<unsigned>(3, tev.alpha_func.op0, L"op0");
			Assert::AreEqual<unsigned>(6, tev.alpha_func.op1, L"op1");
			Assert::AreEqual<unsigned>(2, tev.alpha_func.logic, L"logic");

			// TEV_KSEL_n carries the constant selectors of stages 2n and 2n+1
			for (unsigned i = 0; i < 8; i++)
			{
				uint32_t value = PackKsel(i & 31, (i + 7) & 31, (i + 13) & 31, (i + 19) & 31);
				m.BpLoad(TEV_KSEL_0_ID + i, value);

				Assert::AreEqual<unsigned>(i & 31, tev.ksel[i].kcsel0, L"kcsel0");
				Assert::AreEqual<unsigned>((i + 7) & 31, tev.ksel[i].kasel0, L"kasel0");
				Assert::AreEqual<unsigned>((i + 13) & 31, tev.ksel[i].kcsel1, L"kcsel1");
				Assert::AreEqual<unsigned>((i + 19) & 31, tev.ksel[i].kasel1, L"kasel1");
			}
		}

		// =========================================================================================
		// The combine (rendered)
		// =========================================================================================

		// The whole pipeline must reproduce the host colour: it proves that the draw reached the TEV
		// and that the pass-through stage keeps the rasterized colour intact.
		TEST_METHOD(Tev_TheRasterizedColourReachesTheEfb)
		{
			RequireGL();
			GfxTestMachine& m = M();

			SetupPassThroughXF(m);
			SetupRasterColorSource(m);
			SetupDefaultPixelState(m);
			SetupPassThroughTev(m);

			m.BeginFrame();
			DrawFullScreenQuad(m, 0x80, 0x40, 0x20, 0xff);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);

			Assert::AreEqual<int>(0x80, rgb[0], L"red");
			Assert::AreEqual<int>(0x40, rgb[1], L"green");
			Assert::AreEqual<int>(0x20, rgb[2], L"blue");
		}

		// The colour registers are the two-argument sources of the combine, so a stage that adds two
		// constants exercises lerp, the accumulator and the clamp at once.
		TEST_METHOD(Tev_LerpBetweenTwoColourRegistersMatchesTheReferenceModel)
		{
			RequireGL();
			GfxTestMachine& m = M();

			SetupPassThroughXF(m);
			SetupRasterColorSource(m);
			SetupDefaultPixelState(m);

			// Register 0 = (32, 64, 96), register 1 = (192, 160, 128)
			m.BpLoad(TEV_REGISTERL_0_ID, 32 | (0xff << 12));
			m.BpLoad(TEV_REGISTERH_0_ID, 96 | (64 << 12));
			m.BpLoad(TEV_REGISTERL_1_ID, 192 | (0xff << 12));
			m.BpLoad(TEV_REGISTERH_1_ID, 128 | (160 << 12));

			// The interpolated value depends on the C operand of the *previous* stage, so the chain is
			// two stages: the first one produces a known blend factor by using the rasterized colour as
			// C, the second one lerps register 0 and register 1 with it.
			m.BpLoad(GEN_MODE_ID, 1u << 10);		// ntev = 1 -> two stages

			// Stage 0: colour = rasterized colour (which doubles as the blend factor), stored in reg 2
			m.BpLoad(TEV_COLOR_ENV_0_ID, PackColorEnv(15, 15, 15, 10, 0, 0, 1, 0, 2));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, PackAlphaEnv(7, 7, 7, 7, 0, 0, 1, 0, 2));

			// Stage 1: d = reg0 (accumulator), a = reg0, b = reg1, c = reg2 (the blend factor)
			m.BpLoad(TEV_COLOR_ENV_1_ID, PackColorEnv(0, 2, 4, 0, 0, 0, 1, 0, 0));
			m.BpLoad(TEV_ALPHA_ENV_1_ID, PackAlphaEnv(0, 0, 0, 0, 0, 0, 1, 0, 0));

			const int factor = 0x60;		// 96/255 = 0.376...

			m.BeginFrame();
			DrawFullScreenQuad(m, (uint8_t)factor, (uint8_t)factor, (uint8_t)factor, 0xff);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);

			float reg0[3] = { 32, 64, 96 };
			float reg1[3] = { 192, 160, 128 };

			for (int i = 0; i < 3; i++)
			{
				float expected = TevRef::Combine(reg0[i], reg1[i], (float)factor, reg0[i], 0, 0, false, true);
				wchar_t msg[32];
				swprintf_s(msg, L"channel %d", i);
				Assert::AreEqual((int)expected, (int)rgb[i], msg);
			}
		}

		// Bias, shift, sub and clamp are the arithmetic controls of the stage; the sweep below runs
		// every combination through the real shader and compares the pixel with the reference model.
		//
		// One stage is enough: d and a come from colour register 0, b and c from the rasterized colour,
		// so every channel has its own blend factor and the model can be evaluated channel by channel.
		TEST_METHOD(Tev_BiasShiftSubAndClampMatchTheReferenceModel)
		{
			RequireGL();
			GfxTestMachine& m = M();

			SetupPassThroughXF(m);
			SetupRasterColorSource(m);
			SetupDefaultPixelState(m);

			// reg 0 = (100, 60, 20)
			m.BpLoad(TEV_REGISTERL_0_ID, 100 | (0xff << 12));
			m.BpLoad(TEV_REGISTERH_0_ID, 20 | (60 << 12));

			const float raster[3] = { 90, 40, 10 };
			const float reg0[3] = { 100, 60, 20 };

			int checked = 0;

			for (int bias = 0; bias < 3; bias++)
			{
				for (int shift = 0; shift < 4; shift++)
				{
					for (int sub = 0; sub < 2; sub++)
					{
						// sela = reg0, selb = raster, selc = raster, seld = reg0
						m.BpLoad(TEV_COLOR_ENV_0_ID, PackColorEnv(0, 10, 10, 0, bias, sub, 1, shift, 0));
						m.BpLoad(TEV_ALPHA_ENV_0_ID, PackAlphaEnv(7, 7, 7, 7, 0, 0, 1, 0, 0));

						m.BeginFrame();
						DrawFullScreenQuad(m, (uint8_t)raster[0], (uint8_t)raster[1], (uint8_t)raster[2], 0xff);

						uint8_t rgb[3];
						m.ReadColorPixel(320, 240, rgb);

						for (int i = 0; i < 3; i++)
						{
							float expected = TevRef::Combine(reg0[i], raster[i], raster[i], reg0[i],
								bias, shift, sub != 0, true);

							wchar_t msg[128];
							swprintf_s(msg, L"bias %d shift %d sub %d channel %d", bias, shift, sub, i);
							Assert::AreEqual((int)expected, (int)rgb[i], msg);
							checked++;
						}
					}
				}
			}

			Assert::AreEqual(72, checked, L"the sweep must cover every combination");
		}

		// gfx-tev.md 3.2: without the clamp bit the wide signed result is kept, so a stage can
		// subtract below zero and the next stage still sees the negative value.
		TEST_METHOD(Tev_TheClampBitKeepsTheWideSignedResultWhenClear)
		{
			RequireGL();
			GfxTestMachine& m = M();

			SetupPassThroughXF(m);
			SetupRasterColorSource(m);
			SetupDefaultPixelState(m);

			// reg 0 = 10, the rasterized colour is 200
			m.BpLoad(TEV_REGISTERL_0_ID, 10 | (0xff << 12));
			m.BpLoad(TEV_REGISTERH_0_ID, 10 | (10 << 12));

			m.BpLoad(GEN_MODE_ID, 2u << 10);		// three stages

			// Stage 0 -> reg 1: reg0 - raster. sela is the rasterized colour and selc = 1.0 makes the
			// lerp pass B through, so the stage really subtracts the rasterized colour; with the clamp
			// bit clear the result stays at -190.
			m.BpLoad(TEV_COLOR_ENV_0_ID, PackColorEnv(10, 15, 15, 0, 0, 1, 0, 0, 1));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, PackAlphaEnv(7, 7, 7, 7, 0, 0, 0, 0, 1));

			// Stage 1 -> reg 2: add the +0.5 bias, still unclamped (-62)
			m.BpLoad(TEV_COLOR_ENV_1_ID, PackColorEnv(15, 15, 15, 2, 1, 0, 0, 0, 2));
			m.BpLoad(TEV_ALPHA_ENV_1_ID, PackAlphaEnv(7, 7, 7, 7, 1, 0, 0, 0, 2));

			// Stage 2 -> reg 0: clamp into the 0..255 range for the framebuffer
			m.BpLoad(TEV_COLOR_ENV_2_ID, PackColorEnv(15, 15, 15, 4, 0, 0, 1, 0, 0));
			m.BpLoad(TEV_ALPHA_ENV_2_ID, PackAlphaEnv(7, 7, 7, 7, 0, 0, 1, 0, 0));

			m.BeginFrame();
			DrawFullScreenQuad(m, 200, 200, 200, 0xff);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);

			float stage0 = TevRef::Combine(10, 200, 255, 10, 0, 0, true, false);
			float stage1 = TevRef::Combine(0, 0, 0, stage0, 1, 0, false, false);

			Assert::AreEqual(-190.0f, stage0, L"the unclamped stage must keep the signed value");
			Assert::AreEqual(-62.0f, stage1, L"the biased unclamped stage");
			Assert::AreEqual(0, (int)rgb[0], L"the negative result must reach the framebuffer as zero");
			Assert::AreEqual(0, (int)rgb[1], L"...");
			Assert::AreEqual(0, (int)rgb[2], L"...");
		}

		// =========================================================================================
		// Rev B K constants
		// =========================================================================================

		// A Rev B program can put a K constant in the combine through the selector 14 (KONST) and
		// TEV_KSEL. The K registers are written with the same ids as the colour registers, told apart
		// by the payload tag.
		TEST_METHOD(Tev_KonstSelectsTheRevBConstant)
		{
			RequireGL();
			GfxTestMachine& m = M();

			SetupPassThroughXF(m);
			SetupRasterColorSource(m);
			SetupDefaultPixelState(m);

			// K0 = (0x40, 0x80, 0xc0, 0x20): the K form packs the bytes into bits 7:0 and 19:12
			const uint32_t k0l = 0x40 | (0x20u << 12) | 0x800000;
			const uint32_t k0h = 0xc0 | (0x80u << 12) | 0x800000;
			m.BpLoad(TEV_REGISTERL_0_ID, k0l);
			m.BpLoad(TEV_REGISTERH_0_ID, k0h);

			// Stage 0: colour = KONST, alpha = KONST; kcsel = 12 (whole K0), kasel = 12
			m.BpLoad(TEV_KSEL_0_ID, PackKsel(12, 12, 0, 0));
			m.BpLoad(TEV_COLOR_ENV_0_ID, PackColorEnv(14, 15, 15, 15, 0, 0, 1, 0, 0));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, PackAlphaEnv(6, 7, 7, 7, 0, 0, 1, 0, 0));

			m.BeginFrame();
			DrawFullScreenQuad(m, 0xff, 0xff, 0xff, 0xff);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);

			Assert::AreEqual<int>(0x40, rgb[0], L"K0 red");
			Assert::AreEqual<int>(0x80, rgb[1], L"K0 green");
			Assert::AreEqual<int>(0xc0, rgb[2], L"K0 blue");
		}

		// kcsel values 16..31 name a single channel of K0..K3; for a colour operand that channel is
		// replicated into r, g and b.
		TEST_METHOD(Tev_KonstChannelSelectorsReplicateTheChannel)
		{
			RequireGL();
			GfxTestMachine& m = M();

			SetupPassThroughXF(m);
			SetupRasterColorSource(m);
			SetupDefaultPixelState(m);

			// K0 = (0x11, 0x22, 0x33, 0x44), K1 = (0x55, 0x66, 0x77, 0x88)
			m.BpLoad(TEV_REGISTERL_0_ID, 0x11 | (0x44u << 12) | 0x800000);
			m.BpLoad(TEV_REGISTERH_0_ID, 0x33 | (0x22u << 12) | 0x800000);
			m.BpLoad(TEV_REGISTERL_1_ID, 0x55 | (0x88u << 12) | 0x800000);
			m.BpLoad(TEV_REGISTERH_1_ID, 0x77 | (0x66u << 12) | 0x800000);

			// kcsel = 21 -> K1 green (0x66), kasel = 19 -> K1 red (0x55)
			m.BpLoad(TEV_KSEL_0_ID, PackKsel(21, 19, 0, 0));
			m.BpLoad(TEV_COLOR_ENV_0_ID, PackColorEnv(14, 15, 15, 15, 0, 0, 1, 0, 0));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, PackAlphaEnv(6, 7, 7, 7, 0, 0, 1, 0, 0));

			m.BeginFrame();
			DrawFullScreenQuad(m, 0, 0, 0, 0xff);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);

			Assert::AreEqual<int>(0x66, rgb[0], L"the selected K channel is replicated into red");
			Assert::AreEqual<int>(0x66, rgb[1], L"... green");
			Assert::AreEqual<int>(0x66, rgb[2], L"... blue");
		}

		// =========================================================================================
		// Alpha function
		// =========================================================================================

		TEST_METHOD(Tev_AlphaFunctionGatesThePixel)
		{
			RequireGL();
			GfxTestMachine& m = M();

			SetupPassThroughXF(m);
			SetupRasterColorSource(m);
			SetupDefaultPixelState(m);
			SetupPassThroughTev(m);

			// op0 = greater (4) against 0x40, op1 = always (7), logic = and
			m.BpLoad(TEV_ALPHAFUNC_ID, 0x40 | (0 << 8) | (4u << 16) | (7u << 19) | (0u << 22));

			// The pass-through stage leaves the alpha of the rasterized colour alone
			m.BeginFrame();
			DrawFullScreenQuad(m, 0x10, 0x20, 0x30, 0x80);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);
			Assert::AreEqual<int>(0x10, rgb[0], L"a pixel whose alpha passes the test must be drawn");
		}

		TEST_METHOD(Tev_AlphaFunctionDiscardsThePixelWhenItFails)
		{
			RequireGL();
			GfxTestMachine& m = M();

			SetupPassThroughXF(m);
			SetupRasterColorSource(m);
			SetupDefaultPixelState(m);
			SetupPassThroughTev(m);

			// op0 = greater (4) against 0x40, so an alpha of 0x20 fails
			m.BpLoad(TEV_ALPHAFUNC_ID, 0x40 | (0 << 8) | (4u << 16) | (7u << 19) | (0u << 22));

			m.BeginFrame();
			DrawFullScreenQuad(m, 0x10, 0x20, 0x30, 0x20);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);

			Assert::AreEqual<int>(0, rgb[0], L"the pixel must have been discarded");
			Assert::AreEqual<int>(0, rgb[1], L"the pixel must have been discarded");
			Assert::AreEqual<int>(0, rgb[2], L"the pixel must have been discarded");
		}

		// gfx-tev.md 3.2: the alpha compare modes turn the alpha combine into a full-scale mask that is
		// decided by the sign of the pre-clamp stage value. The test observes the mask through the
		// alpha function, which discards the pixel when the mask is zero.
		TEST_METHOD(Tev_AlphaCompareModesProduceAFullScaleMask)
		{
			RequireGL();

			struct Case
			{
				int mode;
				uint8_t rasterAlpha;
				bool drawn;
				const wchar_t* name;
			};

			// alpha = ca0 - rsa with ca0 = 0x80: a bright alpha makes the stage value negative.
			const Case cases[] = {
				{ 1, 0x10, true,  L"ge0 with a positive stage value" },
				{ 1, 0xf0, false, L"ge0 with a negative stage value" },
				{ 3, 0xf0, true,  L"le0 with a negative stage value" },
				{ 3, 0x10, false, L"le0 with a positive stage value" },
			};

			for (const Case& c : cases)
			{
				GfxTestMachine& m = M();

				SetupPassThroughXF(m);
				SetupRasterColorSource(m);
				SetupDefaultPixelState(m);

				// reg 0 alpha = 0x80
				m.BpLoad(TEV_REGISTERL_0_ID, 0xff | (0x80u << 12));

				// KONST selector 0 is the fixed 1.0 constant, which makes the lerp pass B through
				m.BpLoad(TEV_KSEL_0_ID, PackKsel(0, 0, 0, 0));

				// Stage 0: colour = rasterized colour; alpha = ca0 - rsa, computed on the signed,
				// pre-clamp value (the clamp bit of the alpha environment is clear)
				m.BpLoad(TEV_COLOR_ENV_0_ID, PackColorEnv(15, 15, 15, 10, 0, 0, 1, 0, 0));
				m.BpLoad(TEV_ALPHA_ENV_0_ID,
					PackAlphaEnv(5, 7, 7, 0, 0, 1, 0, 0, 0, c.mode));

				// The alpha function rejects a zero mask: "alpha > 0"
				m.BpLoad(TEV_ALPHAFUNC_ID, 0 | (0 << 8) | (4u << 16) | (7u << 19) | (0u << 22));

				m.BeginFrame();
				DrawFullScreenQuad(m, 0x40, 0x40, 0x40, c.rasterAlpha);

				uint8_t rgb[3];
				m.ReadColorPixel(320, 240, rgb);

				bool drawn = (rgb[0] != 0 || rgb[1] != 0 || rgb[2] != 0);
				Assert::AreEqual(c.drawn, drawn, c.name);
			}
		}

		// =========================================================================================
		// Images for the report
		// =========================================================================================

		// A picture of the arithmetic: the same interpolated input goes through four different stage
		// programs, one per column.
		TEST_METHOD(Tev_Image_CombineGallery)
		{
			RequireGL();
			GfxTestMachine& m = M();

			Report::Section("TEV combine",
				"Every image below is a draw command executed by the emulator's own pipeline: the XF vertex shader\n"
				"transforms the quads, the TEV fragment shader combines the operands and the pixel engine writes the\n"
				"result into the emulated EFB, from where the test reads it back with glReadPixels.");

			SetupPassThroughXF(m);
			SetupRasterColorSource(m);
			SetupDefaultPixelState(m);

			m.BpLoad(GEN_MODE_ID, 1u << 10);		// two stages

			// Stage 0 copies the rasterized colour into colour register 3, which stage 1 then uses;
			// stage 1 is programmed once per column:
			//   0: pass through        1: shift left 1 (x2)
			//   2: add the +0.5 bias   3: 0.5 - raster (inverted)
			m.BpLoad(TEV_COLOR_ENV_0_ID, PackColorEnv(15, 15, 15, 10, 0, 0, 1, 0, 3));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, PackAlphaEnv(5, 5, 5, 5, 0, 0, 1, 0, 3));

			const uint32_t stage1[4] = {
				PackColorEnv(15, 15, 15, 6, 0, 0, 1, 0, 0),		// d = reg3
				PackColorEnv(15, 15, 15, 6, 0, 0, 1, 1, 0),		// d = reg3, shift left 1
				PackColorEnv(15, 15, 15, 6, 1, 0, 1, 0, 0),		// d = reg3, bias +0.5
				PackColorEnv(15, 6, 15, 13, 0, 0, 1, 0, 0),		// d = 0.5, minus reg3
			};

			m.BeginFrame();

			const int cols = 4, rows = 4;
			float cw = 2.0f / cols;
			float ch = 2.0f / rows;

			for (int col = 0; col < cols; col++)
			{
				for (int row = 0; row < rows; row++)
				{
					m.BpLoad(TEV_COLOR_ENV_1_ID, stage1[col]);

					float x0 = -1.0f + col * cw;
					float y0 = -1.0f + row * ch;

					uint8_t r = (uint8_t)(0x20 + row * 0x30);
					uint8_t g = (uint8_t)(0x10 + col * 0x30);
					uint8_t b = (uint8_t)(0x80 - row * 0x18);

					// A horizontal gradient: the vertex colours interpolate across the quad
					GFX::Vertex quad[4] = {
						GfxTestMachine::MakeVertex(x0, y0, 0, 0, 0, 0, 0xff),
						GfxTestMachine::MakeVertex(x0 + cw, y0, 0, r, g, b, 0xff),
						GfxTestMachine::MakeVertex(x0 + cw, y0 + ch, 0, r, g, b, 0xff),
						GfxTestMachine::MakeVertex(x0, y0 + ch, 0, 0, 0, 0, 0xff),
					};
					m.DrawQuad(quad);
				}
			}

			std::string file = "tev_combine_gallery.png";
			Assert::IsTrue(m.SaveScreenshot(OutputDir() + "/" + file, 0, 0, 640, 480, 2),
				L"the screenshot was not saved");

			Report::Image("Four stage programs over the same interpolated input", file,
				"Columns, left to right: pass through, shift left 1 (x2), bias +0.5, and 0.5 - raster.\n"
				"Rows: four different vertex colours. The gradient inside every cell is the interpolated\n"
				"rasterized colour the TEV received.");
		}

		// =========================================================================================
		// The Z-texture environment (TEV_Z_ENV_0 / TEV_Z_ENV_1)
		// =========================================================================================

		//! A 4x4 intensity texture: texel (0, 0) carries `value` in all four components, which is
		//! what a u8 Z texture uses.
		static void SetupZTexture(GfxTestMachine& m, uint8_t value)
		{
			uint8_t raw[32] = { 0 };
			raw[0] = value;
			WriteMainMemory(0x00040000, raw, sizeof(raw));

			m.BpLoad(TX_SETIMAGE0_I0_ID, 3u | (3u << 10) | ((uint32_t)GFX::TF_I8 << 20));
			m.BpLoad(TX_SETIMAGE3_I0_ID, 0x00040000 >> 5);
			m.BpLoad(TX_SETMODE0_I0_ID, 0);
			m.BpLoad(TX_SETMODE1_I0_ID, 0);

			// The stage samples the Z texture
			m.BpLoad(RAS1_TREF0_ID, 1u << 6);				// te0 = 1, ti0 = 0, tc0 = 0
		}

		//! A quad at the given clip depth whose texture coordinate is texel (0, 0).
		static void DrawZQuad(GfxTestMachine& m, float clipZ)
		{
			GFX::Vertex quad[4];
			for (int i = 0; i < 4; i++)
			{
				quad[i] = GfxTestMachine::MakeVertex(0, 0, clipZ, 0xff, 0xff, 0xff, 0xff);
				quad[i].TexCoord[0][0] = 0.125f;
				quad[i].TexCoord[0][1] = 0.125f;
			}

			quad[0].Position[0] = -1; quad[0].Position[1] = -1;
			quad[1].Position[0] = 1; quad[1].Position[1] = -1;
			quad[2].Position[0] = 1; quad[2].Position[1] = 1;
			quad[3].Position[0] = -1; quad[3].Position[1] = 1;

			m.BeginFrame();
			m.DrawQuad(quad);
		}

		// The "add" operation offsets the rasterized depth by the Z texel and the bias.
		TEST_METHOD(Tev_ZTextureAddOffsetsTheDepth)
		{
			RequireGL();
			GfxTestMachine& m = M();

			SetupPassThroughXF(m);
			// The depth buffer has to be cleared to the far value and the quad has to be allowed to
			// write depth, otherwise the Z tests would read the cleared buffer
			m.BpLoad(PE_COPY_CLEAR_Z_ID, 0xFFFFFF);
			m.BpLoad(PE_ZMODE_ID, 1u | (7u << 1) | (1u << 4));		// always, write enabled
			SetupZTexture(m, 0xFF);					// the Z texel is 255

			m.BpLoad(GEN_MODE_ID, 0);
			m.BpLoad(TEV_COLOR_ENV_0_ID, PackColorEnv(15, 15, 15, 8, 0, 0, 1, 0, 0));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, PackAlphaEnv(7, 7, 7, 7, 0, 0, 1, 0, 0));

			m.BpLoad(TEV_Z_ENV_0_ID, 1000);			// the bias
			m.BpLoad(TEV_Z_ENV_1_ID, 0u | (1u << 2));	// type = u8, op = add

			DrawZQuad(m, 0.0f);						// the window depth of the quad is 0.5

			float depth = m.ReadDepthPixel(320, 240);

			// z0 + ztexel + bias = 8388608 + 255 + 1000
			float expected = (0.5f * 16777215.0f + 255.0f + 1000.0f) / 16777215.0f;
			Assert::AreEqual(expected, depth, 0.0002f, L"the added depth");
		}

		// The "replace" operation throws the rasterized depth away - but, as the patent's FIG. 8
		// shows (the bias adder sits downstream of the add/replace mux), the bias still applies.
		TEST_METHOD(Tev_ZTextureReplaceKeepsTheBias)
		{
			RequireGL();
			GfxTestMachine& m = M();

			SetupPassThroughXF(m);
			// The depth buffer has to be cleared to the far value and the quad has to be allowed to
			// write depth, otherwise the Z tests would read the cleared buffer
			m.BpLoad(PE_COPY_CLEAR_Z_ID, 0xFFFFFF);
			m.BpLoad(PE_ZMODE_ID, 1u | (7u << 1) | (1u << 4));		// always, write enabled
			SetupZTexture(m, 0xFF);

			m.BpLoad(GEN_MODE_ID, 0);
			m.BpLoad(TEV_COLOR_ENV_0_ID, PackColorEnv(15, 15, 15, 8, 0, 0, 1, 0, 0));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, PackAlphaEnv(7, 7, 7, 7, 0, 0, 1, 0, 0));

			m.BpLoad(TEV_Z_ENV_0_ID, 1000);
			m.BpLoad(TEV_Z_ENV_1_ID, 0u | (2u << 2));	// type = u8, op = replace

			DrawZQuad(m, 0.0f);

			float depth = m.ReadDepthPixel(320, 240);

			// 0 + ztexel + bias, i.e. the rasterized depth is gone but the bias is not
			float expected = (255.0f + 1000.0f) / 16777215.0f;
			Assert::AreEqual(expected, depth, 0.0002f, L"the replaced depth keeps the bias");

			// ... and it is nowhere near the depth the quad was drawn at
			Assert::IsTrue(depth < 0.01f, L"the reference depth must have been replaced");
		}

		// The environment is off by default, so the depth buffer keeps the rasterized depth.
		TEST_METHOD(Tev_ZTextureDisabledKeepsTheRasterizedDepth)
		{
			RequireGL();
			GfxTestMachine& m = M();

			SetupPassThroughXF(m);
			// The depth buffer has to be cleared to the far value and the quad has to be allowed to
			// write depth, otherwise the Z tests would read the cleared buffer
			m.BpLoad(PE_COPY_CLEAR_Z_ID, 0xFFFFFF);
			m.BpLoad(PE_ZMODE_ID, 1u | (7u << 1) | (1u << 4));		// always, write enabled
			SetupZTexture(m, 0xFF);

			m.BpLoad(GEN_MODE_ID, 0);
			m.BpLoad(TEV_COLOR_ENV_0_ID, PackColorEnv(15, 15, 15, 8, 0, 0, 1, 0, 0));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, PackAlphaEnv(7, 7, 7, 7, 0, 0, 1, 0, 0));
			m.BpLoad(TEV_Z_ENV_1_ID, 0);				// op = disable

			DrawZQuad(m, 0.0f);

			Assert::AreEqual(0.5f, m.ReadDepthPixel(320, 240), 0.001f, L"the rasterized depth");
		}

		// A picture of the alpha function: the rasterized alpha ramps across the screen, so the alpha
		// test cuts the quad into a visible edge.
		TEST_METHOD(Tev_Image_AlphaTestEdge)
		{
			RequireGL();
			GfxTestMachine& m = M();

			SetupPassThroughXF(m);
			SetupRasterColorSource(m);
			SetupDefaultPixelState(m);
			SetupPassThroughTev(m);

			// Keep the pixels whose alpha is above 0x80
			m.BpLoad(TEV_ALPHAFUNC_ID, 0x80 | (0 << 8) | (4u << 16) | (7u << 19) | (0u << 22));

			m.BeginFrame();

			GFX::Vertex quad[4] = {
				GfxTestMachine::MakeVertex(-1, -1, 0, 0xff, 0xff, 0xff, 0x00),
				GfxTestMachine::MakeVertex(1, -1, 0, 0xff, 0xff, 0xff, 0xff),
				GfxTestMachine::MakeVertex(1, 1, 0, 0xff, 0xff, 0xff, 0xff),
				GfxTestMachine::MakeVertex(-1, 1, 0, 0xff, 0xff, 0xff, 0x00),
			};
			m.DrawQuad(quad);

			uint8_t left[3], right[3];
			m.ReadColorPixel(60, 240, left);
			m.ReadColorPixel(580, 240, right);

			Assert::AreEqual<int>(0, left[0], L"the low-alpha side must be cut away");
			Assert::AreEqual<int>(0xff, right[0], L"the high-alpha side must be drawn");

			std::string file = "tev_alpha_test.png";
			Assert::IsTrue(m.SaveScreenshot(OutputDir() + "/" + file, 0, 0, 640, 480, 2),
				L"the screenshot was not saved");

			Report::Image("Alpha function (alpha > 0x80)", file,
				"The vertex alpha interpolates from 0 (left) to 255 (right) and the alpha function discards\n"
				"everything below the reference value, which produces the vertical edge in the middle.");
		}
	};
}
