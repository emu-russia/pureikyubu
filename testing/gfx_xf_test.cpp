// Transform Unit (XF) tests.
//
// The XF is emulated by the vertex shader in xf.cpp, so the tests run the real vertex program
// through transform feedback (GfxTestMachine::RunVertexShader) and look at the varyings it
// produced: the clip position, the eight texture coordinates and the two colour channels.
//
// The register file itself (the CP -> XF block writes and the register read-back path) is checked
// as well, because that is the interface the Command Processor drives.
//
// See specs: gfx-xf.md (registers 0x0000-0x1057, sections 3.1-3.5).

#include "pch.h"
#include "gfx_test_common.h"

using namespace GfxUnitTest;

namespace pureikyubutest
{
	TEST_CLASS(GfxXfTest)
	{
		static GfxTestMachine& M()
		{
			GfxTestMachine& m = Machine();
			m.Reset();
			return m;
		}

		static float AsFloat(uint32_t bits)
		{
			float value;
			memcpy(&value, &bits, 4);
			return value;
		}

		// GFX::Color keeps its bytes in (A, B, G, R) order in memory, so a register value with the
		// components in the natural R, G, B, A order has to be assembled like this.
		static uint32_t Rgba(uint32_t r, uint32_t g, uint32_t b, uint32_t a)
		{
			return (r << 24) | (g << 16) | (b << 8) | a;
		}

		static uint32_t AsBits(float value)
		{
			uint32_t bits;
			memcpy(&bits, &value, 4);
			return bits;
		}

		//! The identity geometry matrix in word 0 of the matrix RAM.
		static void IdentityMatrix(GfxTestMachine& m)
		{
			float identity[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
			m.XfLoadFloats(GFX::XF_MATRIX_MEMORY_ID, identity, 16);
		}

	public:

		// =========================================================================================
		// The register file and the CP interface
		// =========================================================================================

		TEST_METHOD(Xf_MatrixRamIsWrittenInBlocksAndReadBack)
		{
			GfxTestMachine& m = M();

			std::vector<uint32_t> words;
			for (int i = 0; i < 20; i++)
			{
				words.push_back(AsBits((float)(i * 1.5f)));
			}

			m.XfLoadBlock(GFX::XF_MATRIX_MEMORY_ID + 4, words.data(), words.size());

			for (int i = 0; i < 20; i++)
			{
				Assert::AreEqual<uint32_t>(words[i], m.XfRead(GFX::XF_MATRIX_MEMORY_ID + 4 + i),
					L"matrix RAM read-back");
			}
		}

		TEST_METHOD(Xf_LightRecordsAreWrittenWordByWord)
		{
			GfxTestMachine& m = M();

			// Light 3 occupies 0x0630-0x063F: 3 reserved, RGBA, a0-a2, k0-k2, lpx, dhx
			unsigned base = GFX::XF_LIGHT3_ID;
			m.XfLoad(base + 3, 0x11223344);					// RGBA
			m.XfLoad(base + 4, AsBits(0.25f));				// a0
			m.XfLoad(base + 7, AsBits(2.0f));				// k0
			m.XfLoad(base + 0xa, AsBits(10.0f));			// lpx
			m.XfLoad(base + 0xd, AsBits(-1.0f));			// dhx

			const GFX::Light& light = m.gfx->xf->xf.light[3];
			Assert::AreEqual<uint32_t>(0x11223344, light.rgba.RGBA, L"light rgba");
			Assert::AreEqual(0.25f, light.a[0], 0.0001f, L"a0");
			Assert::AreEqual(2.0f, light.k[0], 0.0001f, L"k0");
			Assert::AreEqual(10.0f, light.lpx[0], 0.0001f, L"lpx");
			Assert::AreEqual(-1.0f, light.dhx[0], 0.0001f, L"dhx");

			// ... and the read-back path returns exactly what was written
			Assert::AreEqual<uint32_t>(0x11223344, m.XfRead(base + 3), L"light rgba read-back");
			Assert::AreEqual(0.25f, AsFloat(m.XfRead(base + 4)), 0.0001f, L"a0 read-back");
		}

		TEST_METHOD(Xf_ControlRegistersAreWrittenAndReadBack)
		{
			GfxTestMachine& m = M();

			m.XfLoad(GFX::XF_NUMCOLS_ID, 2);
			m.XfLoad(GFX::XF_NUMTEX_ID, 5);
			m.XfLoad(GFX::XF_DUALTEX_ID, 1);
			m.XfLoad(GFX::XF_MATINDEX_A_ID, 0x00123456);
			m.XfLoad(GFX::XF_INVTXSPEC_ID, 0x123);
			m.XfLoad(GFX::XF_CLIP_DISABLE_ID, 0x7);

			Assert::AreEqual<uint32_t>(2, m.XfRead(GFX::XF_NUMCOLS_ID), L"numColors");
			Assert::AreEqual<uint32_t>(5, m.XfRead(GFX::XF_NUMTEX_ID), L"numTex");
			Assert::AreEqual<uint32_t>(1, m.XfRead(GFX::XF_DUALTEX_ID), L"dualTexTran");
			Assert::AreEqual<uint32_t>(0x00123456, m.XfRead(GFX::XF_MATINDEX_A_ID), L"matrix index A");
			Assert::AreEqual<uint32_t>(0x123, m.XfRead(GFX::XF_INVTXSPEC_ID), L"invtxspec");
			Assert::AreEqual<uint32_t>(0x7, m.XfRead(GFX::XF_CLIP_DISABLE_ID), L"clip disable");
		}

		// XFready is deasserted while the CP has not taken the read-back value yet.
		TEST_METHOD(Xf_ReadyIsHeldOffUntilTheReadDataIsTaken)
		{
			GfxTestMachine& m = M();

			Assert::IsTrue(m.gfx->xf->CPReady(), L"the XF is ready before any read");

			m.gfx->xf->CPRegRead(GFX::XF_NUMTEX_ID);
			Assert::IsFalse(m.gfx->xf->CPReady(), L"the XF must hold the CP off while XFrdValid is set");

			uint32_t value = 0;
			Assert::IsTrue(m.gfx->xf->CPTakeReadData(&value), L"the read data must be available");
			Assert::IsTrue(m.gfx->xf->CPReady(), L"the XF is ready again");

			Assert::IsFalse(m.gfx->xf->CPTakeReadData(&value), L"the read data must be taken only once");
		}

		// =========================================================================================
		// The geometry transform
		// =========================================================================================

		TEST_METHOD(Xf_GeometryMatrixTransformsThePosition)
		{
			GfxTestMachine& m = M();

			// Row 0 = (2, 0, 0, 10), row 1 = (0, 3, 0, 20), row 2 = (0, 0, 4, 30)
			float matrix[16] = {
				2,0,0,10,
				0,3,0,20,
				0,0,4,30,
				0,0,0,1,
			};
			m.XfLoadFloats(GFX::XF_MATRIX_MEMORY_ID, matrix, 16);

			// Orthographic projection with A=C=E=1 and B=D=F=0: the clip position is the eye position
			float proj[6] = { 1,0, 1,0, 1,0 };
			m.XfLoadFloats(GFX::XF_PROJECTION_A_ID, proj, 6);
			m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, AsBits(1.0f));

			m.XfLoad(GFX::XF_NUMCOLS_ID, 0);
			m.XfLoad(GFX::XF_NUMTEX_ID, 0);

			std::vector<GFX::Vertex> in = { GfxTestMachine::MakeVertex(1, 2, 3) };
			std::vector<GfxTestMachine::XFVertex> out;

			Assert::IsTrue(m.RunVertexShader(in, out), Widen(m.LastError()).c_str());

			// row0 . (1,2,3,1) = 2 + 10 = 12; row1 . p = 6 + 20 = 26; row2 . p = 12 + 30 = 42
			Assert::AreEqual(12.0f, out[0].Position[0], 0.0001f, L"x");
			Assert::AreEqual(26.0f, out[0].Position[1], 0.0001f, L"y");
			Assert::AreEqual(42.0f, out[0].Position[2], 0.0001f, L"z");
			Assert::AreEqual(1.0f, out[0].Position[3], 0.0001f, L"w");
		}

		// gfx-xf.md 3.2 / the emulator's shader: the perspective combine uses A and B for x, C and D
		// for y, E and F for z, and the homogeneous component is -z.
		TEST_METHOD(Xf_PerspectiveProjectionCombinesTheEyePosition)
		{
			GfxTestMachine& m = M();
			IdentityMatrix(m);

			m.XfLoad(GFX::XF_PROJECTION_A_ID, AsBits(2.0f));
			m.XfLoad(GFX::XF_PROJECTION_B_ID, AsBits(0.5f));
			m.XfLoad(GFX::XF_PROJECTION_C_ID, AsBits(3.0f));
			m.XfLoad(GFX::XF_PROJECTION_D_ID, AsBits(0.25f));
			m.XfLoad(GFX::XF_PROJECTION_E_ID, AsBits(4.0f));
			m.XfLoad(GFX::XF_PROJECTION_F_ID, AsBits(0.125f));
			m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, 0);

			m.XfLoad(GFX::XF_NUMCOLS_ID, 0);
			m.XfLoad(GFX::XF_NUMTEX_ID, 0);

			std::vector<GFX::Vertex> in = { GfxTestMachine::MakeVertex(1, 2, -4) };
			std::vector<GfxTestMachine::XFVertex> out;

			Assert::IsTrue(m.RunVertexShader(in, out), Widen(m.LastError()).c_str());

			Assert::AreEqual(2.0f * 1 + 0.5f * -4, out[0].Position[0], 0.0001f, L"clip x");
			Assert::AreEqual(3.0f * 2 + 0.25f * -4, out[0].Position[1], 0.0001f, L"clip y");
			Assert::AreEqual(4.0f * -4 + 0.125f, out[0].Position[2], 0.0001f, L"clip z");
			Assert::AreEqual(4.0f, out[0].Position[3], 0.0001f, L"clip w = -eye z");
		}

		// =========================================================================================
		// Lighting
		// =========================================================================================

		// Channel control with LightFunc = 0: the illumination is 1.0 and the output colour is the
		// material colour (gfx-xf.md 3.3).
		TEST_METHOD(Xf_ChannelWithLightingOffOutputsTheMaterialColour)
		{
			GfxTestMachine& m = M();
			IdentityMatrix(m);
			m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, AsBits(1.0f));
			m.XfLoad(GFX::XF_NUMTEX_ID, 0);

			m.XfLoad(GFX::XF_NUMCOLS_ID, 1);
			m.XfLoad(GFX::XF_MATERIAL0_ID, Rgba(0x80, 0x40, 0x00, 0x00));
			m.XfLoad(GFX::XF_AMBIENT0_ID, 0);
			// MatSrc = 0 (the material register), LightFunc = 0 (1.0), AmbSrc = 0, DiffuseAtten = 0
			m.XfLoad(GFX::XF_COLOR0CNTL_ID, 0);
			m.XfLoad(GFX::XF_ALPHA0CNTL_ID, 0);

			std::vector<GFX::Vertex> in = { GfxTestMachine::MakeVertex(0, 0, 0, 0x10, 0x20, 0x30, 0x40) };
			std::vector<GfxTestMachine::XFVertex> out;

			Assert::IsTrue(m.RunVertexShader(in, out), Widen(m.LastError()).c_str());

			Assert::AreEqual(0x80 / 255.0f, out[0].Color0[0], 0.002f, L"red");
			Assert::AreEqual(0x40 / 255.0f, out[0].Color0[1], 0.002f, L"green");
			Assert::AreEqual(0.0f, out[0].Color0[2], 0.002f, L"blue");
		}

		// A light that points along the normal contributes its colour at full strength; a light that
		// is perpendicular to the normal contributes nothing when N.L is clamped to [0,1].
		TEST_METHOD(Xf_DiffuseLightingFollowsTheNormalLightAngle)
		{
			for (int perpendicular = 0; perpendicular < 2; perpendicular++)
			{
				GfxTestMachine& m = M();
				IdentityMatrix(m);
				m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, AsBits(1.0f));
				m.XfLoad(GFX::XF_NUMTEX_ID, 0);

				// An identity normal matrix so that the vertex normal arrives unchanged
				float identityNrm[9] = { 1,0,0, 0,1,0, 0,0,1 };
				m.XfLoadFloats(GFX::XF_NORMAL_MATRIX_MEMORY_ID, identityNrm, 9);

				m.XfLoad(GFX::XF_NUMCOLS_ID, 1);
				m.XfLoad(GFX::XF_MATERIAL0_ID, Rgba(0xff, 0xff, 0xff, 0xff));	// white material
				m.XfLoad(GFX::XF_AMBIENT0_ID, 0);
				// MatSrc = 0, LightFunc = 1 (use illumination), AmbSrc = 0, DiffuseAtten = 2 (N.L clamped)
				m.XfLoad(GFX::XF_COLOR0CNTL_ID, (1u << 1) | (2u << 7) | (1u << 2));
				m.XfLoad(GFX::XF_ALPHA0CNTL_ID, 0);

				// Light 0: colour 0x808080, positioned along +z or along +x of the vertex
				m.XfLoad(GFX::XF_LIGHT0_RGBA_ID, Rgba(0x80, 0x80, 0x80, 0x80));
				float position[3] = { perpendicular ? 10.0f : 0.0f, 0.0f, perpendicular ? 0.0f : 10.0f };
				m.XfLoad(GFX::XF_LIGHT0_LPX_ID, AsBits(position[0]));
				m.XfLoad(GFX::XF_LIGHT0_LPY_ID, AsBits(position[1]));
				m.XfLoad(GFX::XF_LIGHT0_LPZ_ID, AsBits(position[2]));
				m.XfLoad(GFX::XF_LIGHT0_K0_ID, AsBits(1.0f));

				std::vector<GFX::Vertex> in;
				GFX::Vertex v = GfxTestMachine::MakeVertex(0, 0, 0);
				v.Normal[0] = 0; v.Normal[1] = 0; v.Normal[2] = 1;
				in.push_back(v);

				std::vector<GfxTestMachine::XFVertex> out;
				Assert::IsTrue(m.RunVertexShader(in, out), Widen(m.LastError()).c_str());

				if (perpendicular)
				{
					Assert::AreEqual(0.0f, out[0].Color0[0], 0.002f,
						L"a light perpendicular to the normal must contribute nothing");
				}
				else
				{
					Assert::AreEqual(0x80 / 255.0f, out[0].Color0[0], 0.002f,
						L"a light along the normal contributes its full colour");
				}
			}
		}

		// =========================================================================================
		// Texture coordinate generation
		// =========================================================================================

		// texgen type 0 (regular) with a 2x4 matrix: (s, t) = the two matrix rows dotted with the
		// source row (gfx-xf.md 3.4).
		TEST_METHOD(Xf_RegularTexgenUsesTheTextureMatrix)
		{
			GfxTestMachine& m = M();
			IdentityMatrix(m);

			// Matrix 1 (words 4-7): row 0 = (1, 0, 0, 2), row 1 = (0, 1, 0, 3)
			float matrix[8] = {
				1,0,0,2,
				0,1,0,3,
			};
			m.XfLoadFloats(GFX::XF_MATRIX_MEMORY_ID + 4, matrix, 8);

			m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, AsBits(1.0f));
			m.XfLoad(GFX::XF_NUMCOLS_ID, 0);

			// One texgen: type 0 (regular), source row 0 (the position), no projection,
			// in_form = 1 ((A,B,C,1.0) - the input is the full position)
			m.XfLoad(GFX::XF_NUMTEX_ID, 1);
			m.XfLoad(GFX::XF_TEXGEN0_ID, (0u << 4) | (0u << 7) | (0u << 1) | (1u << 2));

			std::vector<GFX::Vertex> in;
			GFX::Vertex v = GfxTestMachine::MakeVertex(4, 5, 6);
			v.matIdx0.Tex0MatIdx = 1;			// use matrix 1
			in.push_back(v);

			std::vector<GfxTestMachine::XFVertex> out;
			Assert::IsTrue(m.RunVertexShader(in, out), Widen(m.LastError()).c_str());

			Assert::AreEqual(4.0f + 2.0f, out[0].TexCoord[0][0], 0.0001f, L"s");
			Assert::AreEqual(5.0f + 3.0f, out[0].TexCoord[0][1], 0.0001f, L"t");
		}

		// texgen type 0 with the projection bit: the 3x4 matrix divides (s, t) by q.
		TEST_METHOD(Xf_ProjectedTexgenDividesByQ)
		{
			GfxTestMachine& m = M();
			IdentityMatrix(m);

			float matrix[12] = {
				1,0,0,0,
				0,1,0,0,
				0,0,1,0,
			};
			m.XfLoadFloats(GFX::XF_MATRIX_MEMORY_ID + 4, matrix, 12);

			m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, AsBits(1.0f));
			m.XfLoad(GFX::XF_NUMCOLS_ID, 0);
			m.XfLoad(GFX::XF_NUMTEX_ID, 1);
			// type 0, source row 0, projection = 1, in_form = 1
			m.XfLoad(GFX::XF_TEXGEN0_ID, (0u << 4) | (0u << 7) | (1u << 1) | (1u << 2));

			std::vector<GFX::Vertex> in;
			GFX::Vertex v = GfxTestMachine::MakeVertex(4, 2, 2);
			v.matIdx0.Tex0MatIdx = 1;
			in.push_back(v);

			std::vector<GfxTestMachine::XFVertex> out;
			Assert::IsTrue(m.RunVertexShader(in, out), Widen(m.LastError()).c_str());

			// s = 4 / 2 = 2, t = 2 / 2 = 1
			Assert::AreEqual(2.0f, out[0].TexCoord[0][0], 0.0001f, L"s / q");
			Assert::AreEqual(1.0f, out[0].TexCoord[0][1], 0.0001f, L"t / q");
		}

		// texgen type 2 (colour texgen): (s, t) = (r, g:b concatenated) (gfx-xf.md 3.4).
		TEST_METHOD(Xf_ColorTexgenConcatenatesGreenAndBlue)
		{
			GfxTestMachine& m = M();
			IdentityMatrix(m);
			m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, AsBits(1.0f));
			m.XfLoad(GFX::XF_NUMTEX_ID, 1);
			m.XfLoad(GFX::XF_TEXGEN0_ID, (2u << 4));		// type 2 = colour texgen from colour 0

			std::vector<GFX::Vertex> in = { GfxTestMachine::MakeVertex(0, 0, 0, 0x40, 0x80, 0x20, 0xff) };
			std::vector<GfxTestMachine::XFVertex> out;

			Assert::IsTrue(m.RunVertexShader(in, out), Widen(m.LastError()).c_str());

			// s = r = 0x40/255, t = (g << 8 | b) / 65535 -> the shader scales it as (g*256 + b)/257
			Assert::AreEqual(0x40 / 255.0f, out[0].TexCoord[0][0], 0.002f, L"s = red");
			Assert::AreEqual((0x80 * 256.0f + 0x20) / (255.0f * 257.0f / 255.0f) / 255.0f, out[0].TexCoord[0][1], 0.01f,
				L"t = green:blue");
		}

		// A texgen whose index is at or above numTex is not generated at all: the host coordinate
		// passes through (gfx-xf.md 3.4, GEN_MODE/XF_NUMTEX).
		TEST_METHOD(Xf_TexgensAboveNumTexPassTheCoordinateThrough)
		{
			GfxTestMachine& m = M();
			IdentityMatrix(m);
			m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, AsBits(1.0f));
			m.XfLoad(GFX::XF_NUMCOLS_ID, 0);

			// Only texgen 0 is active, but its constant term would move the coordinate
			float matrix[8] = { 1,0,0,9, 0,1,0,9 };
			m.XfLoadFloats(GFX::XF_MATRIX_MEMORY_ID, matrix, 8);
			m.XfLoad(GFX::XF_NUMTEX_ID, 1);
			m.XfLoad(GFX::XF_TEXGEN0_ID, (0u << 4) | (0u << 7) | (1u << 2));

			std::vector<GFX::Vertex> in;
			GFX::Vertex v = GfxTestMachine::MakeVertex(1, 1, 1);
			v.TexCoord[0][0] = 0.25f; v.TexCoord[0][1] = 0.5f;
			v.TexCoord[1][0] = 0.75f; v.TexCoord[1][1] = 0.125f;
			in.push_back(v);

			std::vector<GfxTestMachine::XFVertex> out;
			Assert::IsTrue(m.RunVertexShader(in, out), Widen(m.LastError()).c_str());

			Assert::AreEqual(1.0f + 9.0f, out[0].TexCoord[0][0], 0.0001f, L"texgen 0 is transformed");
			Assert::AreEqual(0.75f, out[0].TexCoord[1][0], 0.0001f, L"texgen 1 passes through");
			Assert::AreEqual(0.125f, out[0].TexCoord[1][1], 0.0001f, L"texgen 1 passes through");
		}

		// The Rev B dual texture transform applies a second matrix to every generated coordinate
		// (gfx-xf.md, XF_DUALTEX / XF_DUALGEN).
		TEST_METHOD(Xf_DualTextureTransformAppliesTheDualMatrix)
		{
			GfxTestMachine& m = M();
			IdentityMatrix(m);
			m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, AsBits(1.0f));
			m.XfLoad(GFX::XF_NUMCOLS_ID, 0);
			m.XfLoad(GFX::XF_NUMTEX_ID, 1);
			m.XfLoad(GFX::XF_TEXGEN0_ID, 0);			// regular transformation, source row 0

			// Dual matrix 0 (words 0-7 of the dual RAM): (s, t) -> (s + 1, t + 2)
			float dual[8] = {
				1,0,0,1,
				0,1,0,2,
			};
			m.XfLoadFloats(GFX::XF_DUALTEX_MATRIX_MEMORY_ID, dual, 8);

			m.XfLoad(GFX::XF_DUALTEX_ID, 1);
			m.XfLoad(GFX::XF_DUALGEN0_ID, 0);			// dual matrix 0, no normalisation

			std::vector<GFX::Vertex> in;
			GFX::Vertex v = GfxTestMachine::MakeVertex(0.5f, 0.25f, 0);
			in.push_back(v);

			std::vector<GfxTestMachine::XFVertex> out;
			Assert::IsTrue(m.RunVertexShader(in, out), Widen(m.LastError()).c_str());

			// The regular stage copies the position (identity matrix), then the dual transform adds
			Assert::AreEqual(0.5f + 1.0f, out[0].TexCoord[0][0], 0.0001f, L"dual s");
			Assert::AreEqual(0.25f + 2.0f, out[0].TexCoord[0][1], 0.0001f, L"dual t");
		}
	};
}
