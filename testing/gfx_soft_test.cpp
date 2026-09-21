// Unit tests for the software GFX pipeline (issue #384).
//
// The software pipeline is a second rendering path of the same Flipper blocks - XF, SU, the three
// rasterizers, the texture unit with a real TMEM, the TEV and the pixel engine with a real EFB
// memory array. The tests drive it the way the console is programmed, through the hardware API and
// nothing else:
//
//   * the XF register space (XfLoad/XfLoadBlock) - the geometry matrix, the projection combine and
//     the viewport registers, exactly the registers GXSetProjection/GXSetViewport write;
//   * the bypass (BP) register space (BpLoad) - the shared GEN registers, the rasterizer texture
//     bindings, the texture load commands, the TEV stage environments and the pixel engine state;
//   * the primitive stream (DrawPrimitive) - object-space vertices, like the CP hands them over.
//
// What is observed is the hardware's own output: the EFB memory of the pixel engine (its colour
// and Z arrays) and the XFB the copy engine writes into main memory. No OpenGL call is involved -
// the software pipeline never opens a GL context.
//
// See specs: gfx-xf.md (the transform and the viewport), gfx-su.md (the setup), gfx-ras0.md (the
// quad walk), gfx-ras1.md/gfx-ras2.md (the interpolation), gfx-tc.md (TMEM), gfx-tf.md (the
// filter), gfx-tev.md (the combine), gfx-pe.md (the EFB and the copy engine).

#include "pch.h"
#include "gfx_test_common.h"

#include <array>
#include <set>
#include <string>
#include <vector>

using namespace GfxUnitTest;

namespace pureikyubutest
{
	namespace
	{
		using Rgba = std::array<uint8_t, 4>;

		Rgba Color(int r, int g, int b, int a = 255)
		{
			return Rgba{ (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a };
		}

		uint32_t AsBits(float value)
		{
			uint32_t bits;
			memcpy(&bits, &value, 4);
			return bits;
		}

		// -------------------------------------------------------------------------------------
		// The hardware setup (the register writes the GX API performs)
		// -------------------------------------------------------------------------------------

		//! GXInit's geometry state: the identity model-view/normal matrices in the slots the SDK's
		//! GX_IDENTITY names, the identity texture matrix and the white material.
		void SetupIdentityMatrices(GfxTestMachine& m)
		{
			float identity[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
			m.XfLoadFloats(GFX::XF_MATRIX_MEMORY_ID, identity, 16);

			float identityNrm[9] = { 1,0,0, 0,1,0, 0,0,1 };
			m.XfLoadFloats(GFX::XF_NORMAL_MATRIX_MEMORY_ID, identityNrm, 9);

			// The identity texture matrix the SDK reserves for GX_IDENTITY (60) and GX_DTTIDENTITY (61)
			float tex[12] = { 1,0,0,0, 0,1,0,0, 0,0,1,0 };
			m.XfLoadFloats(GFX::XF_MATRIX_MEMORY_ID + GX_IDENTITY * 4, tex, 12);
			m.XfLoadFloats(GFX::XF_DUALTEX_MATRIX_MEMORY_ID + GX_DTTIDENTITY * 4, tex, 12);

			m.XfLoad(GFX::XF_MATINDEX_A_ID, 0);
			m.XfLoad(GFX::XF_MATINDEX_B_ID, 0);
			m.XfLoad(GFX::XF_NUMCOLS_ID, 0);		// the host colours pass through
			m.XfLoad(GFX::XF_NUMTEX_ID, 0);
		}

		//! `GXSetProjection(orthographic)` with the identity coefficients: the eye-space X/Y reach
		//! the window through the viewport registers (gfx-xf.md 3.2, 4.6.10).
		void SetupOrthoProjection(GfxTestMachine& m)
		{
			float proj[6] = { 1,0, 1,0, 1,0 };
			m.XfLoadFloats(GFX::XF_PROJECTION_A_ID, proj, 6);
			m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, AsBits(1.0f));
		}

		//! `GXSetViewport(x, y, w, h, near, far)`: the scale is half the size with a negative Y and
		//! the offset carries the +342 origin bias of the window coordinate system (gfx-xf.md 4.6,
		//! gfx-su.md 4.1).
		void SetViewport(GfxTestMachine& m, float x, float y, float w, float h,
			float zNear = 0.0f, float zFar = 16777215.0f)
		{
			float scaleX = w / 2.0f;
			float scaleY = -h / 2.0f;
			float scaleZ = (zFar - zNear) / 2.0f;
			float offsetX = x + scaleX + 342.0f;
			float offsetY = y - scaleY + 342.0f;
			float offsetZ = (zFar + zNear) / 2.0f;

			m.XfLoad(GFX::XF_VIEWPORT_SCALE_X_ID, AsBits(scaleX));
			m.XfLoad(GFX::XF_VIEWPORT_SCALE_Y_ID, AsBits(scaleY));
			m.XfLoad(GFX::XF_VIEWPORT_SCALE_Z_ID, AsBits(scaleZ));
			m.XfLoad(GFX::XF_VIEWPORT_OFFSET_X_ID, AsBits(offsetX));
			m.XfLoad(GFX::XF_VIEWPORT_OFFSET_Y_ID, AsBits(offsetY));
			m.XfLoad(GFX::XF_VIEWPORT_OFFSET_Z_ID, AsBits(offsetZ));
		}

		//! The full-EFB viewport the tests draw through.
		void SetupFullViewport(GfxTestMachine& m)
		{
			SetViewport(m, 0.0f, 0.0f, 640.0f, 480.0f);
		}

		//! The pixel engine state a test starts from: no depth test, no blending or logic op, both
		//! write masks enabled (GXSetZMode(GX_FALSE, ...) / GXSetBlendMode(GX_BM_NONE) /
		//! GXSetColorUpdate(GX_TRUE) / GXSetAlphaUpdate(GX_TRUE)).
		void SetupDefaultPixelState(GfxTestMachine& m)
		{
			SetupQuadOffset(m);							// the screen origin the GX API programs
			m.BpLoad(PE_ZMODE_ID, (1u << 4));			// the Z update mask, no depth test
			m.BpLoad(PE_CMODE0_ID, (1u << 3) | (1u << 4));
			m.BpLoad(PE_CMODE1_ID, 0);
		}

		//! The copy engine's clear colour: the EFB a frame starts with.
		void SetClearColor(GfxTestMachine& m, const Rgba& c, uint32_t depth = 0)
		{
			m.BpLoad(PE_COPY_CLEAR_AR_ID, (uint32_t)c[0] | ((uint32_t)c[3] << 8));
			m.BpLoad(PE_COPY_CLEAR_GB_ID, (uint32_t)c[2] | ((uint32_t)c[1] << 8));
			m.BpLoad(PE_COPY_CLEAR_Z_ID, depth);
		}

		//! One TEV stage's colour environment word (gfx-tev.md 4.2). The four selects are the
		//! `tev_csel` codes; the clamp defaults to the low clamp, which is what a colour that
		//! reaches the EFB wants.
		uint32_t ColorEnv(int sela, int selb, int selc, int seld, int bias = 0, int sub = 0,
			int clamp = 1, int shift = 0, int dest = 0)
		{
			return (uint32_t)seld | ((uint32_t)selc << 4) | ((uint32_t)selb << 8) |
				((uint32_t)sela << 12) | ((uint32_t)bias << 16) | ((uint32_t)sub << 18) |
				((uint32_t)clamp << 19) | ((uint32_t)shift << 20) | ((uint32_t)dest << 22);
		}

		//! One TEV stage's alpha environment word (gfx-tev.md 4.3); the selects are `tev_asel`.
		uint32_t AlphaEnv(int sela, int selb, int selc, int seld, int bias = 0, int sub = 0,
			int clamp = 1, int shift = 0, int dest = 0, int mode = 0, int swap = 0)
		{
			return (uint32_t)mode | ((uint32_t)swap << 2) | ((uint32_t)seld << 4) |
				((uint32_t)selc << 7) | ((uint32_t)selb << 10) | ((uint32_t)sela << 13) |
				((uint32_t)bias << 16) | ((uint32_t)sub << 18) | ((uint32_t)clamp << 19) |
				((uint32_t)shift << 20) | ((uint32_t)dest << 22);
		}

		//! The operand codes of the two environments (gfx-tev.md 3.2)
		enum
		{
			RSC = 10, RSCA = 11, TEXC = 8, TEXA = 9, HALF = 13, ONE = 12, ZERO = 15,
			ARSCA = 5, AZERO = 7,
		};

		//! One TEV stage that writes the stage's rasterized colour (colour 0) and its alpha to the
		//! EFB: the colour result is `zero + lerp(0, 0, 0) + rsc` and the alpha result
		//! `zero + lerp(rsa, rsa, rsa)` (gfx-tev.md 3.2).
		void SetupRasterStage0(GfxTestMachine& m)
		{
			m.BpLoad(GEN_MODE_ID, 0);								// ntev = 1
			m.BpLoad(RAS1_TREF0_ID, 0);							// cc0 = 0: rasterized colour 0
			m.BpLoad(TEV_COLOR_ENV_0_ID, ColorEnv(ZERO, ZERO, ZERO, RSC));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, AlphaEnv(ARSCA, ARSCA, ARSCA, AZERO));
		}

		// -------------------------------------------------------------------------------------
		// Object-space geometry. The viewport maps the eye-space X/Y onto the EFB:
		//     px = (x + 1) * 320,  py = (1 - y) * 240   for the full-EFB viewport
		// -------------------------------------------------------------------------------------

		float ObjectX(float px) { return px / 320.0f - 1.0f; }
		float ObjectY(float py) { return 1.0f - py / 240.0f; }

		GFX::Vertex MakeVertex(float x, float y, float z, const Rgba& c)
		{
			return GfxTestMachine::MakeVertex(x, y, z, c[0], c[1], c[2], c[3]);
		}

		//! A rectangle of EFB pixels, in object space, in one colour. The fan order of a Flipper
		//! quad starts at the bottom left corner (gfx-xf.md 2.2).
		void DrawPixelRect(GfxTestMachine& m, float px0, float py0, float px1, float py1, const Rgba& c,
			float z = 0.0f)
		{
			GFX::Vertex quad[4];
			quad[0] = MakeVertex(ObjectX(px0), ObjectY(py1), z, c);		// bottom left
			quad[1] = MakeVertex(ObjectX(px1), ObjectY(py1), z, c);		// bottom right
			quad[2] = MakeVertex(ObjectX(px1), ObjectY(py0), z, c);		// top right
			quad[3] = MakeVertex(ObjectX(px0), ObjectY(py0), z, c);		// top left

			m.DrawQuad(quad);
		}

		//! One EFB pixel, read out of the pixel engine's own memory (not through OpenGL).
		void ReadEfbPixel(GfxTestMachine& m, int x, int y, uint8_t rgba[4])
		{
			uint32_t z = 0;
			Assert::IsTrue(m.gfx->pe->SoftPixel(x, y, rgba, &z),
				Widen("the EFB pixel (" + std::to_string(x) + ", " + std::to_string(y) +
					") could not be read").c_str());
		}
	}

	TEST_CLASS(GfxSoftTest)
	{
		//! The machine, switched to the software pipeline and put into a known state.
		static GfxTestMachine& M()
		{
			GfxTestMachine& m = Machine();
			m.SetPipeline(GFX_PIPELINE_SOFT);
			m.Reset();
			return m;
		}

		//! Leave the shared machine on the shader pipeline for the other tests.
		static void Restore()
		{
			Machine().SetPipeline(GFX_PIPELINE_SHADER);
		}

		// ---------------------------------------------------------------------------------------
		// The pipeline switch
		// ---------------------------------------------------------------------------------------

		TEST_METHOD(Soft_ThePipelineSwitchTakesEffectAndIsAConfigVariable)
		{
			GfxTestMachine& m = Machine();

			m.SetPipeline(GFX_PIPELINE_SOFT);
			Assert::IsTrue(m.gfx->SoftPipeline(), L"the software pipeline was not selected");
			Assert::AreEqual(1, GetConfigInt(USER_GFX_PIPELINE, USER_HW), L"the choice is not in the config");

			m.SetPipeline(GFX_PIPELINE_SHADER);
			Assert::IsFalse(m.gfx->SoftPipeline(), L"the shader pipeline was not selected");
			Assert::AreEqual(0, GetConfigInt(USER_GFX_PIPELINE, USER_HW), L"the config was not updated");

			// An unknown value must be rejected instead of leaving the pipeline in a broken state
			Assert::IsFalse(m.gfx->SetPipeline(42), L"an unknown pipeline was accepted");
			Assert::IsFalse(m.gfx->SoftPipeline(), L"the rejected switch changed the pipeline");
		}

		// ---------------------------------------------------------------------------------------
		// The XF and the setup unit
		// ---------------------------------------------------------------------------------------

		TEST_METHOD(Soft_TheViewportRegistersMapTheVerticesToTheEfb)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupDefaultPixelState(m);
			SetupRasterStage0(m);
			SetClearColor(m, Color(0, 0, 0));

			// An inset viewport: the object-space (-1,-1)..(1,1) square fills the rectangle
			// x = 100..300, y = 50..250 of the EFB (GXSetViewport).
			SetViewport(m, 100.0f, 50.0f, 200.0f, 200.0f);

			m.BeginFrame();

			GFX::Vertex quad[4];
			quad[0] = MakeVertex(-1.0f, -1.0f, 0.0f, Color(255, 255, 255));
			quad[1] = MakeVertex(1.0f, -1.0f, 0.0f, Color(255, 255, 255));
			quad[2] = MakeVertex(1.0f, 1.0f, 0.0f, Color(255, 255, 255));
			quad[3] = MakeVertex(-1.0f, 1.0f, 0.0f, Color(255, 255, 255));
			m.DrawQuad(quad);

			uint8_t pixel[4] = { 0 };

			ReadEfbPixel(m, 200, 150, pixel);
			Assert::AreEqual((int)255, (int)pixel[0], L"the centre of the viewport is not covered");

			ReadEfbPixel(m, 99, 150, pixel);
			Assert::AreEqual((int)0, (int)pixel[0], L"the pixel left of the viewport is covered");
			ReadEfbPixel(m, 300, 150, pixel);
			Assert::AreEqual((int)0, (int)pixel[0], L"the pixel right of the viewport is covered");
			ReadEfbPixel(m, 200, 49, pixel);
			Assert::AreEqual((int)0, (int)pixel[0], L"the pixel above the viewport is covered");
			ReadEfbPixel(m, 200, 250, pixel);
			Assert::AreEqual((int)0, (int)pixel[0], L"the pixel below the viewport is covered");

			m.EndFrame();
			Restore();
		}

		TEST_METHOD(Soft_TheFrameClearColourReachesTheEfb)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupDefaultPixelState(m);

			// GXSetCopyClear: the clear the frame begins with. The three channels are told apart so
			// that a channel order mistake cannot pass (the clear word is an EFB lane).
			SetClearColor(m, Color(32, 64, 128), 0xFFFFFF);

			m.BeginFrame();

			uint8_t pixel[4] = { 0 };
			ReadEfbPixel(m, 320, 240, pixel);

			Assert::AreEqual((int)32, (int)pixel[0], L"the clear red is wrong");
			Assert::AreEqual((int)64, (int)pixel[1], L"the clear green is wrong");
			Assert::AreEqual((int)128, (int)pixel[2], L"the clear blue is wrong");
			Assert::AreEqual((int)255, (int)pixel[3], L"the clear alpha is wrong");

			m.EndFrame();
			Restore();
		}

		// ---------------------------------------------------------------------------------------
		// The quad walk (RAS0)
		// ---------------------------------------------------------------------------------------

		TEST_METHOD(Soft_AQuadCoversExactlyItsPixels)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupDefaultPixelState(m);
			SetupRasterStage0(m);
			SetClearColor(m, Color(0, 0, 0));

			m.BeginFrame();

			// The rectangle covers the EFB pixels x = 100..139 and y = 50..89
			DrawPixelRect(m, 100.0f, 50.0f, 140.0f, 90.0f, Color(255, 0, 0));

			uint8_t pixel[4] = { 0 };

			ReadEfbPixel(m, 120, 70, pixel);
			Assert::AreEqual((int)255, (int)pixel[0], L"the covered pixel is not the vertex colour");
			Assert::AreEqual((int)0, (int)pixel[1], L"the covered pixel has a wrong green channel");

			ReadEfbPixel(m, 99, 70, pixel);
			Assert::AreEqual((int)0, (int)pixel[0], L"the pixel left of the quad is covered");
			ReadEfbPixel(m, 140, 70, pixel);
			Assert::AreEqual((int)0, (int)pixel[0], L"the pixel right of the quad is covered");
			ReadEfbPixel(m, 120, 49, pixel);
			Assert::AreEqual((int)0, (int)pixel[0], L"the pixel above the quad is covered");
			ReadEfbPixel(m, 120, 90, pixel);
			Assert::AreEqual((int)0, (int)pixel[0], L"the pixel below the quad is covered");

			// The corners belong to the quad as well: the interior of the edge functions is
			// inclusive and the top-left rule only rejects the shared edges.
			ReadEfbPixel(m, 100, 50, pixel);
			Assert::AreEqual((int)255, (int)pixel[0], L"the top left corner is missing");
			ReadEfbPixel(m, 139, 89, pixel);
			Assert::AreEqual((int)255, (int)pixel[0], L"the bottom right corner is missing");

			m.EndFrame();
			Restore();
		}

		TEST_METHOD(Soft_ATriangleCoversItsInteriorAndLeavesTheRestAlone)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupDefaultPixelState(m);
			SetupRasterStage0(m);
			SetClearColor(m, Color(0, 0, 0));

			m.BeginFrame();

			// A right triangle with the corners (100, 100), (300, 100) and (100, 300)
			GFX::Vertex tri[3];
			tri[0] = MakeVertex(ObjectX(100), ObjectY(100), 0.0f, Color(0, 255, 0));
			tri[1] = MakeVertex(ObjectX(300), ObjectY(100), 0.0f, Color(0, 255, 0));
			tri[2] = MakeVertex(ObjectX(100), ObjectY(300), 0.0f, Color(0, 255, 0));

			m.DrawPrimitive(GFX::RAS_TRIANGLE, tri, 3);

			uint8_t pixel[4] = { 0 };

			ReadEfbPixel(m, 150, 150, pixel);			// inside
			Assert::AreEqual((int)255, (int)pixel[1], L"the interior of the triangle is not covered");

			ReadEfbPixel(m, 280, 280, pixel);			// outside, across the hypotenuse
			Assert::AreEqual((int)0, (int)pixel[1], L"the pixel outside the hypotenuse is covered");

			ReadEfbPixel(m, 150, 120, pixel);			// inside, below the top edge
			Assert::AreEqual((int)255, (int)pixel[1], L"the pixel below the top edge is missing");

			m.EndFrame();
			Restore();
		}

		TEST_METHOD(Soft_TheRasterWalkIsDoneInTwoByTwoQuads)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupDefaultPixelState(m);
			SetupRasterStage0(m);
			SetClearColor(m, Color(0, 0, 0));

			m.BeginFrame();

			// A single pixel at an odd coordinate: the quad walker still visits its 2x2 quad, but
			// only the covered pixel of the quad is written (gfx-ras0.md 3.2).
			DrawPixelRect(m, 201.0f, 101.0f, 202.0f, 102.0f, Color(0, 0, 255));

			uint8_t pixel[4] = { 0 };

			ReadEfbPixel(m, 201, 101, pixel);
			Assert::AreEqual((int)255, (int)pixel[2], L"the single covered pixel was not written");

			ReadEfbPixel(m, 200, 100, pixel);
			Assert::AreEqual((int)0, (int)pixel[2], L"the uncovered pixel of the same quad was written");
			ReadEfbPixel(m, 200, 101, pixel);
			Assert::AreEqual((int)0, (int)pixel[2], L"the uncovered pixel of the same quad was written");
			ReadEfbPixel(m, 201, 100, pixel);
			Assert::AreEqual((int)0, (int)pixel[2], L"the uncovered pixel of the same quad was written");

			m.EndFrame();
			Restore();
		}

		TEST_METHOD(Soft_TheScissorRectangleClipsThePrimitive)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupDefaultPixelState(m);
			SetupRasterStage0(m);
			SetClearColor(m, Color(0, 0, 0));

			// GXSetScissor(200, 100, 100, 100): the rectangle x = 200..299, y = 100..199. The low
			// field of the register is the Y corner and the high one the X corner, and both carry
			// the +342 origin bias of the setup unit (gfx-su.md 4.1).
			m.BpLoad(SU_SCIS0_ID, (342u + 100u) | ((342u + 200u) << 12));
			m.BpLoad(SU_SCIS1_ID, (342u + 199u) | ((342u + 299u) << 12));

			m.BeginFrame();

			DrawPixelRect(m, 0.0f, 0.0f, 640.0f, 480.0f, Color(255, 255, 255));

			uint8_t pixel[4] = { 0 };

			ReadEfbPixel(m, 250, 150, pixel);
			Assert::AreEqual((int)255, (int)pixel[0], L"the pixel inside the scissor is missing");

			ReadEfbPixel(m, 199, 150, pixel);
			Assert::AreEqual((int)0, (int)pixel[0], L"the pixel left of the scissor is covered");
			ReadEfbPixel(m, 300, 150, pixel);
			Assert::AreEqual((int)0, (int)pixel[0], L"the pixel right of the scissor is covered");
			ReadEfbPixel(m, 250, 99, pixel);
			Assert::AreEqual((int)0, (int)pixel[0], L"the pixel above the scissor is covered");
			ReadEfbPixel(m, 250, 200, pixel);
			Assert::AreEqual((int)0, (int)pixel[0], L"the pixel below the scissor is covered");

			m.EndFrame();
			Restore();
		}

		TEST_METHOD(Soft_TheCullModeKeepsTheSameFacesAsTheShaderBackend)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupDefaultPixelState(m);
			SetClearColor(m, Color(0, 0, 0));

			// The triangle (100,100), (300,100), (100,300) winds with a positive signed area in the
			// window coordinate system of the EFB (Y grows downward), which is the winding the
			// shader backend keeps for GEN_MODE.reject_en = reject_front.
			GFX::Vertex tri[3];
			tri[0] = MakeVertex(ObjectX(100), ObjectY(100), 0.0f, Color(255, 0, 0));
			tri[1] = MakeVertex(ObjectX(300), ObjectY(100), 0.0f, Color(255, 0, 0));
			tri[2] = MakeVertex(ObjectX(100), ObjectY(300), 0.0f, Color(255, 0, 0));

			uint8_t pixel[4] = { 0 };

			// reject_front (GEN_MODE[15:14] = 1) keeps it
			SetupRasterStage0(m);
			m.BpLoad(GEN_MODE_ID, 1u << 14);
			m.BeginFrame();
			m.DrawPrimitive(GFX::RAS_TRIANGLE, tri, 3);
			ReadEfbPixel(m, 150, 150, pixel);
			Assert::AreEqual((int)255, (int)pixel[0], L"the front face was culled");

			// reject_back (GEN_MODE[15:14] = 2) drops it
			m.BpLoad(GEN_MODE_ID, 2u << 14);
			m.BeginFrame();
			m.DrawPrimitive(GFX::RAS_TRIANGLE, tri, 3);
			ReadEfbPixel(m, 150, 150, pixel);
			Assert::AreEqual((int)0, (int)pixel[0], L"the front face survived reject_back");

			// The opposite winding is kept by reject_back and dropped by reject_front
			GFX::Vertex flipped[3] = { tri[1], tri[0], tri[2] };

			m.BpLoad(GEN_MODE_ID, 2u << 14);
			m.BeginFrame();
			m.DrawPrimitive(GFX::RAS_TRIANGLE, flipped, 3);
			ReadEfbPixel(m, 150, 150, pixel);
			Assert::AreEqual((int)255, (int)pixel[0], L"the back face was culled");

			m.BpLoad(GEN_MODE_ID, 1u << 14);
			m.BeginFrame();
			m.DrawPrimitive(GFX::RAS_TRIANGLE, flipped, 3);
			ReadEfbPixel(m, 150, 150, pixel);
			Assert::AreEqual((int)0, (int)pixel[0], L"the back face survived reject_front");

			m.EndFrame();
			Restore();
		}

		TEST_METHOD(Soft_TheClipperCutsATriangleAtTheNearPlane)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupDefaultPixelState(m);
			SetupRasterStage0(m);
			SetClearColor(m, Color(0, 0, 0));

			// A perspective projection combine: `clip = (ex, ey, -ez, -ez)`, so the near plane sits
			// at ez = 0 and a vertex behind it has a negative w (gfx-xf.md 3.2, 3.5).
			float proj[6] = { 1, 0, 1, 0, -1, 0 };
			m.XfLoadFloats(GFX::XF_PROJECTION_A_ID, proj, 6);
			m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, AsBits(0.0f));
			SetupFullViewport(m);

			m.BeginFrame();

			// The first two vertices project to (160, 240) and (480, 240); the third one is behind
			// the near plane, where the perspective divide mirrors it through the origin - without
			// the clipper the triangle would cover the area *below* the first two, which is not
			// where the primitive really is.
			GFX::Vertex tri[3];
			tri[0] = MakeVertex(-5.0f, 0.0f, -10.0f, Color(0, 255, 0));
			tri[1] = MakeVertex(5.0f, 0.0f, -10.0f, Color(0, 255, 0));
			tri[2] = MakeVertex(0.0f, 10.0f, 10.0f, Color(0, 255, 0));
			m.DrawPrimitive(GFX::RAS_TRIANGLE, tri, 3);

			uint8_t pixel[4] = { 0 };

			// The part of the triangle in front of the near plane reaches the middle of the screen
			ReadEfbPixel(m, 320, 100, pixel);
			Assert::AreEqual((int)255, (int)pixel[1], L"the clipped part of the triangle is missing");

			// The mirrored projection of the vertex behind the eye would have covered this area
			ReadEfbPixel(m, 320, 400, pixel);
			Assert::AreEqual((int)0, (int)pixel[1], L"the triangle was drawn behind the eye");

			m.EndFrame();
			Restore();
		}

		// ---------------------------------------------------------------------------------------
		// Interpolation (RAS1/RAS2)
		// ---------------------------------------------------------------------------------------

		TEST_METHOD(Soft_TheVertexColoursAreInterpolatedAcrossTheQuad)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupDefaultPixelState(m);
			SetupRasterStage0(m);
			SetClearColor(m, Color(0, 0, 0));

			m.BeginFrame();

			// A rectangle from x = 100 to 300 with black on the left and white on the right
			GFX::Vertex quad[4];
			quad[0] = MakeVertex(ObjectX(100), ObjectY(200), 0.0f, Color(0, 0, 0));
			quad[1] = MakeVertex(ObjectX(300), ObjectY(200), 0.0f, Color(255, 255, 255));
			quad[2] = MakeVertex(ObjectX(300), ObjectY(100), 0.0f, Color(255, 255, 255));
			quad[3] = MakeVertex(ObjectX(100), ObjectY(100), 0.0f, Color(0, 0, 0));
			m.DrawQuad(quad);

			uint8_t pixel[4] = { 0 };

			// The middle of the rectangle interpolates to about half of the range
			ReadEfbPixel(m, 200, 150, pixel);
			Assert::IsTrue(pixel[0] > 110 && pixel[0] < 145, Widen("the interpolated colour is " +
				std::to_string((int)pixel[0]) + " instead of about 127").c_str());

			ReadEfbPixel(m, 101, 150, pixel);
			Assert::IsTrue(pixel[0] < 8, L"the left end is not the black vertex colour");
			ReadEfbPixel(m, 298, 150, pixel);
			Assert::IsTrue(pixel[0] > 246, L"the right end is not the white vertex colour");

			// The gradient is horizontal, so the value does not change along the vertical
			uint8_t middle[4] = { 0 };
			uint8_t lower[4] = { 0 };
			ReadEfbPixel(m, 200, 150, middle);
			ReadEfbPixel(m, 200, 110, lower);
			Assert::AreEqual((int)middle[0], (int)lower[0], L"the colour changes along the vertical");

			m.EndFrame();
			Restore();
		}

		// ---------------------------------------------------------------------------------------
		// The TEV combine
		// ---------------------------------------------------------------------------------------

		TEST_METHOD(Soft_TheTevStageCombinesTheRasterizedColourWithAConstant)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupDefaultPixelState(m);
			SetClearColor(m, Color(0, 0, 0));

			m.BeginFrame();

			// Stage 0: A = half (0.5 = 128), B = rasterized colour, C = half, D = zero, so the
			// stage outputs lerp(0.5, rsc, 0.5), i.e. the midpoint between half scale and the
			// rasterized colour (gfx-tev.md 3.2). Half of 255 is 128 in the register file.
			m.BpLoad(GEN_MODE_ID, 0);
			m.BpLoad(RAS1_TREF0_ID, 0);
			m.BpLoad(TEV_COLOR_ENV_0_ID, ColorEnv(HALF, RSC, HALF, ZERO));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, AlphaEnv(ARSCA, ARSCA, ARSCA, AZERO));

			DrawPixelRect(m, 100.0f, 100.0f, 200.0f, 200.0f, Color(255, 255, 255));

			uint8_t pixel[4] = { 0 };
			ReadEfbPixel(m, 150, 150, pixel);

			// lerp(128, 255, 128) = 128 + (128/255)*(255-128) = 128 + 63.7 = 191.7 -> 192
			Assert::IsTrue(pixel[0] >= 190 && pixel[0] <= 194, Widen("the combined colour is " +
				std::to_string((int)pixel[0]) + " instead of about 192").c_str());

			m.EndFrame();
			Restore();
		}

		TEST_METHOD(Soft_TheAlphaFunctionKillsTheFailingFragments)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupDefaultPixelState(m);

			// The raster stage writes the colour with the rasterized alpha; the alpha function
			// compares it against 128 with "greater" and kills everything below (gfx-tev.md 3.8).
			m.BpLoad(GEN_MODE_ID, 0);
			m.BpLoad(RAS1_TREF0_ID, 0);
			m.BpLoad(TEV_COLOR_ENV_0_ID, ColorEnv(ZERO, ZERO, ZERO, RSC));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, AlphaEnv(ARSCA, ARSCA, ARSCA, AZERO));
			m.BpLoad(TEV_ALPHAFUNC_ID,
				128u | (128u << 8) | (4u << 16) | (7u << 19) | (0u << 22));		// greater / always / and

			SetClearColor(m, Color(0, 0, 0, 0));
			m.BeginFrame();

			// The left half of the rectangle is opaque, the right half transparent: the alpha test
			// keeps only the left one.
			GFX::Vertex quad[4];
			quad[0] = MakeVertex(ObjectX(100), ObjectY(200), 0.0f, Color(255, 255, 255, 255));
			quad[1] = MakeVertex(ObjectX(150), ObjectY(200), 0.0f, Color(255, 255, 255, 128));
			quad[2] = MakeVertex(ObjectX(150), ObjectY(100), 0.0f, Color(255, 255, 255, 128));
			quad[3] = MakeVertex(ObjectX(100), ObjectY(100), 0.0f, Color(255, 255, 255, 255));
			m.DrawQuad(quad);

			GFX::Vertex quad2[4];
			quad2[0] = MakeVertex(ObjectX(150), ObjectY(200), 0.0f, Color(255, 255, 255, 0));
			quad2[1] = MakeVertex(ObjectX(200), ObjectY(200), 0.0f, Color(255, 255, 255, 0));
			quad2[2] = MakeVertex(ObjectX(200), ObjectY(100), 0.0f, Color(255, 255, 255, 0));
			quad2[3] = MakeVertex(ObjectX(150), ObjectY(100), 0.0f, Color(255, 255, 255, 0));
			m.DrawQuad(quad2);

			uint8_t pixel[4] = { 0 };

			ReadEfbPixel(m, 120, 150, pixel);
			Assert::AreEqual((int)255, (int)pixel[0], L"the opaque half was killed by the alpha test");

			ReadEfbPixel(m, 180, 150, pixel);
			Assert::AreEqual((int)0, (int)pixel[0], L"the transparent half passed the alpha test");

			m.EndFrame();
			Restore();
		}

		// ---------------------------------------------------------------------------------------
		// The pixel engine: the depth test
		// ---------------------------------------------------------------------------------------

		TEST_METHOD(Soft_TheDepthTestKeepsTheNearerPrimitive)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupRasterStage0(m);
			SetClearColor(m, Color(0, 0, 0), 0xFFFFFF);

			// GXSetZMode(GX_TRUE, GX_LESS, GX_TRUE)
			m.BpLoad(PE_ZMODE_ID, 1u /*enable*/ | (1u << 1) /*less*/ | (1u << 4) /*mask*/);

			m.BeginFrame();

			// The viewport maps the object Z through scale Z/2 and offset Z/2, so the rectangle at
			// the object Z of -0.5 gets the smaller 24-bit depth of the two: it is the near one.
			DrawPixelRect(m, 100.0f, 100.0f, 200.0f, 200.0f, Color(255, 0, 0), -0.5f);
			DrawPixelRect(m, 150.0f, 150.0f, 250.0f, 250.0f, Color(0, 255, 0), 0.5f);

			uint8_t pixel[4] = { 0 };

			// Where the two overlap the nearer one (red) survives, even though it was drawn first
			ReadEfbPixel(m, 180, 180, pixel);
			Assert::AreEqual((int)255, (int)pixel[0], L"the far primitive won the overlap");
			Assert::AreEqual((int)0, (int)pixel[1], L"the far primitive won the overlap");

			// Outside the overlap the far one is drawn on its own
			ReadEfbPixel(m, 220, 220, pixel);
			Assert::AreEqual((int)0, (int)pixel[0], L"the far primitive was not drawn");
			Assert::AreEqual((int)255, (int)pixel[1], L"the far primitive was not drawn");

			// The Z buffer holds the depths that survived: the near one is a quarter of the
			// 24-bit range, the far one three quarters.
			uint32_t z = 0;
			uint8_t rgba[4] = { 0 };
			Assert::IsTrue(m.gfx->pe->SoftPixel(180, 180, rgba, &z), L"the EFB pixel could not be read");
			Assert::IsTrue(z > 0x300000 && z < 0x500000, Widen("the near depth is 0x" +
				std::to_string(z) + " instead of about 0x400000").c_str());

			Assert::IsTrue(m.gfx->pe->SoftPixel(220, 220, rgba, &z), L"the EFB pixel could not be read");
			Assert::IsTrue(z > 0xB00000 && z < 0xD00000, Widen("the far depth is 0x" +
				std::to_string(z) + " instead of about 0xC00000").c_str());

			m.EndFrame();
			Restore();
		}

		TEST_METHOD(Soft_TheBlendFactorsCombineTheSourceAndTheDestination)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupRasterStage0(m);

			// GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR): with an
			// opaque source (alpha 255) the source replaces the destination (gfx-pe.md 4.3). The
			// clear colour is the destination, so a source that is NOT written would leave it.
			SetClearColor(m, Color(64, 32, 16), 0);
			m.BpLoad(PE_CMODE0_ID, 1u /*blend_en*/ | (1u << 3) | (1u << 4) |
				(5u << 5) /*dfactor = one minus source alpha*/ | (4u << 8) /*sfactor = source alpha*/);

			m.BeginFrame();

			DrawPixelRect(m, 100.0f, 100.0f, 200.0f, 200.0f, Color(255, 0, 0));

			uint8_t pixel[4] = { 0 };
			ReadEfbPixel(m, 150, 150, pixel);

			// src factor = dst factor terms: src*255/256 + dst*(255-255)/256 = the source colour
			Assert::IsTrue(pixel[0] >= 250, Widen("the blended red is " +
				std::to_string((int)pixel[0]) + " instead of the source").c_str());
			Assert::AreEqual((int)0, (int)pixel[1], L"the blended green is not the source value");
			Assert::AreEqual((int)0, (int)pixel[2], L"the blended blue is not the source value");

			// A pixel the quad does not cover keeps the destination colour
			ReadEfbPixel(m, 50, 50, pixel);
			Assert::AreEqual((int)64, (int)pixel[0], L"the destination outside the quad changed");
			Assert::AreEqual((int)32, (int)pixel[1], L"the destination outside the quad changed");

			m.EndFrame();
			Restore();
		}

		// ---------------------------------------------------------------------------------------
		// The texture unit and TMEM
		// ---------------------------------------------------------------------------------------

		TEST_METHOD(Soft_A_Rgba8TextureIsSampledOutOfATmemPreload)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupDefaultPixelState(m);
			SetClearColor(m, Color(0, 0, 0));

			// An 8x8 I8 image: the left half has the intensity 0x40 and the right half 0xC0. The
			// main-memory layout is the tiled one of the format: an 8-bit tile is 8 texels wide and
			// 4 tall (32 bytes), so the image is two tiles one after the other and the texels of a
			// tile are stored row by row.
			const uint32_t texAddr = 0x00400000;
			std::vector<uint8_t> raw(64);
			for (int tile = 0; tile < 2; tile++)
			{
				for (int v = 0; v < 4; v++)
				{
					for (int u = 0; u < 8; u++)
					{
						raw[tile * 32 + v * 8 + u] = (u < 4) ? 0x40 : 0xC0;
					}
				}
			}
			WriteMainMemory(texAddr, raw.data(), raw.size());

			// GXInitTexObj: 8x8, I8, wrapped/nearest; the image is software-managed (pre-loaded)
			// and its load destination is TMEM line 16.
			m.BpLoad(TX_SETIMAGE0_I0_ID, (8 - 1) | ((8 - 1) << 10) | ((uint32_t)GFX::TF_I8 << 20));
			m.BpLoad(TX_SETIMAGE1_I0_ID, 16u | (1u << 21));
			m.BpLoad(TX_SETIMAGE3_I0_ID, texAddr >> 5);
			m.BpLoad(TX_SETMODE0_I0_ID, 0);
			m.BpLoad(TX_SETMODE1_I0_ID, 10u << 8);

			// GXLoadTexObj's explicit load (GX_LOAD_BLOCK): the image base, the TMEM offset, the
			// tile count and the load format. `tx_load_format` 2 covers the 8-bit and the 16-bit
			// texels, so the two 32-byte tiles of the image are two count units (gfx-tc.md 4.2).
			m.BpLoad(TX_LOADBLOCK0_ID, texAddr >> 5);
			m.BpLoad(TX_LOADBLOCK1_ID, 16);
			m.BpLoad(TX_LOADBLOCK3_ID, 2u | (2u << 15));

			// One TEV stage that outputs the texel of map 0, sampled with texture coordinate 0
			m.BpLoad(GEN_MODE_ID, 0);
			m.BpLoad(RAS1_TREF0_ID, 0u | (1u << 6));		// ti0 = 0, te0 = 1, tc0 = 0
			m.BpLoad(TEV_COLOR_ENV_0_ID, ColorEnv(ZERO, ZERO, ZERO, TEXC));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, AlphaEnv(AZERO, AZERO, AZERO, 4 /*texa*/));

			m.BeginFrame();

			// A quad over the EFB pixels (200,200)-(300,300) whose texture coordinates run from
			// (0,0) at the top left to (1,1) at the bottom right.
			GFX::Vertex quad[4];
			quad[0] = MakeVertex(ObjectX(200), ObjectY(300), 0.0f, Color(0, 0, 0));
			quad[1] = MakeVertex(ObjectX(300), ObjectY(300), 0.0f, Color(0, 0, 0));
			quad[2] = MakeVertex(ObjectX(300), ObjectY(200), 0.0f, Color(0, 0, 0));
			quad[3] = MakeVertex(ObjectX(200), ObjectY(200), 0.0f, Color(0, 0, 0));

			quad[0].TexCoord[0][0] = 0.0f; quad[0].TexCoord[0][1] = 1.0f;	// bottom left
			quad[1].TexCoord[0][0] = 1.0f; quad[1].TexCoord[0][1] = 1.0f;	// bottom right
			quad[2].TexCoord[0][0] = 1.0f; quad[2].TexCoord[0][1] = 0.0f;	// top right
			quad[3].TexCoord[0][0] = 0.0f; quad[3].TexCoord[0][1] = 0.0f;	// top left

			m.DrawQuad(quad);

			uint8_t pixel[4] = { 0 };

			ReadEfbPixel(m, 210, 250, pixel);
			Assert::AreEqual((int)0x40, (int)pixel[0], L"the left half of the texture was not read");

			ReadEfbPixel(m, 290, 250, pixel);
			Assert::AreEqual((int)0xC0, (int)pixel[0], L"the right half of the texture was not read");

			m.EndFrame();
			Restore();
		}

		TEST_METHOD(Soft_ACachedImageIsFetchedIntoTheTmemCache)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupDefaultPixelState(m);
			SetClearColor(m, Color(0, 0, 0));

			// The same I8 image, but the image is hardware-managed: nothing loads it into TMEM
			// explicitly, so the sampler fetches the line through the tag cache (gfx-tc.md 3.5).
			const uint32_t texAddr = 0x00410000;
			std::vector<uint8_t> raw(64);
			for (int tile = 0; tile < 2; tile++)
			{
				for (int v = 0; v < 4; v++)
				{
					for (int u = 0; u < 8; u++)
					{
						raw[tile * 32 + v * 8 + u] = (u < 4) ? 0x20 : 0xE0;
					}
				}
			}
			WriteMainMemory(texAddr, raw.data(), raw.size());

			m.BpLoad(TX_SETIMAGE0_I0_ID, (8 - 1) | ((8 - 1) << 10) | ((uint32_t)GFX::TF_I8 << 20));
			m.BpLoad(TX_SETIMAGE1_I0_ID, 0);				// image_type = cache
			m.BpLoad(TX_SETIMAGE3_I0_ID, texAddr >> 5);
			m.BpLoad(TX_SETMODE0_I0_ID, 0);
			m.BpLoad(TX_SETMODE1_I0_ID, 10u << 8);

			m.BpLoad(GEN_MODE_ID, 0);
			m.BpLoad(RAS1_TREF0_ID, 0u | (1u << 6));
			m.BpLoad(TEV_COLOR_ENV_0_ID, ColorEnv(ZERO, ZERO, ZERO, TEXC));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, AlphaEnv(AZERO, AZERO, AZERO, 4 /*texa*/));

			m.BeginFrame();

			GFX::Vertex quad[4];
			quad[0] = MakeVertex(ObjectX(200), ObjectY(300), 0.0f, Color(0, 0, 0));
			quad[1] = MakeVertex(ObjectX(300), ObjectY(300), 0.0f, Color(0, 0, 0));
			quad[2] = MakeVertex(ObjectX(300), ObjectY(200), 0.0f, Color(0, 0, 0));
			quad[3] = MakeVertex(ObjectX(200), ObjectY(200), 0.0f, Color(0, 0, 0));

			quad[0].TexCoord[0][0] = 0.0f; quad[0].TexCoord[0][1] = 1.0f;
			quad[1].TexCoord[0][0] = 1.0f; quad[1].TexCoord[0][1] = 1.0f;
			quad[2].TexCoord[0][0] = 1.0f; quad[2].TexCoord[0][1] = 0.0f;
			quad[3].TexCoord[0][0] = 0.0f; quad[3].TexCoord[0][1] = 0.0f;

			m.DrawQuad(quad);

			uint8_t pixel[4] = { 0 };

			ReadEfbPixel(m, 210, 250, pixel);
			Assert::AreEqual((int)0x20, (int)pixel[0], L"the cached image was not fetched");

			ReadEfbPixel(m, 290, 250, pixel);
			Assert::AreEqual((int)0xE0, (int)pixel[0], L"the cached image was not fetched");

			m.EndFrame();
			Restore();
		}

		TEST_METHOD(Soft_AColourIndexImageIsDereferencedThroughTheTlut)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupDefaultPixelState(m);
			SetClearColor(m, Color(0, 0, 0));

			// An 8x4 C8 image: the left half of every row is index 0, the right half index 1. One
			// 8-bit tile is 8 texels wide and 4 tall, which is exactly the image.
			const uint32_t texAddr = 0x00420000;
			const uint32_t tlutAddr = 0x00430000;

			std::vector<uint8_t> raw(32, 0);
			for (int v = 0; v < 4; v++)
			{
				for (int u = 4; u < 8; u++)
					raw[v * 8 + u] = 1;
			}
			WriteMainMemory(texAddr, raw.data(), raw.size());

			// Two RGB565 palette entries: red and blue
			std::vector<uint8_t> tlut(4);
			tlut[0] = 0xF8; tlut[1] = 0x00;		// red
			tlut[2] = 0x00; tlut[3] = 0x1F;		// blue
			WriteMainMemory(tlutAddr, tlut.data(), tlut.size());

			m.BpLoad(TX_SETIMAGE0_I0_ID, (8 - 1) | ((4 - 1) << 10) | ((uint32_t)GFX::TF_C8 << 20));
			m.BpLoad(TX_SETIMAGE1_I0_ID, 0);
			m.BpLoad(TX_SETIMAGE3_I0_ID, texAddr >> 5);
			m.BpLoad(TX_SETMODE0_I0_ID, 0);
			m.BpLoad(TX_SETMODE1_I0_ID, 10u << 8);

			// GXLoadTlut: one block of sixteen entries, bound to map 0 as RGB565
			m.BpLoad(TX_LOADTLUT0_ID, tlutAddr >> 5);
			m.BpLoad(TX_LOADTLUT1_ID, 0u | (1u << 10));
			m.BpLoad(TX_SETTLUT_I0_ID, 0u | ((uint32_t)GFX::TLUT_RGB565 << 10));

			m.BpLoad(GEN_MODE_ID, 0);
			m.BpLoad(RAS1_TREF0_ID, 0u | (1u << 6));
			m.BpLoad(TEV_COLOR_ENV_0_ID, ColorEnv(ZERO, ZERO, ZERO, TEXC));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, AlphaEnv(AZERO, AZERO, AZERO, 4 /*texa*/));

			m.BeginFrame();

			GFX::Vertex quad[4];
			quad[0] = MakeVertex(ObjectX(200), ObjectY(300), 0.0f, Color(0, 0, 0));
			quad[1] = MakeVertex(ObjectX(300), ObjectY(300), 0.0f, Color(0, 0, 0));
			quad[2] = MakeVertex(ObjectX(300), ObjectY(200), 0.0f, Color(0, 0, 0));
			quad[3] = MakeVertex(ObjectX(200), ObjectY(200), 0.0f, Color(0, 0, 0));

			quad[0].TexCoord[0][0] = 0.0f; quad[0].TexCoord[0][1] = 1.0f;
			quad[1].TexCoord[0][0] = 1.0f; quad[1].TexCoord[0][1] = 1.0f;
			quad[2].TexCoord[0][0] = 1.0f; quad[2].TexCoord[0][1] = 0.0f;
			quad[3].TexCoord[0][0] = 0.0f; quad[3].TexCoord[0][1] = 0.0f;

			m.DrawQuad(quad);

			uint8_t pixel[4] = { 0 };

			// The left half of the image holds index 0 (red), the right half index 1 (blue)
			ReadEfbPixel(m, 210, 250, pixel);
			Assert::IsTrue(pixel[0] > 240 && pixel[2] < 15, L"the first palette entry was not used");

			ReadEfbPixel(m, 290, 250, pixel);
			Assert::IsTrue(pixel[2] > 240 && pixel[0] < 15, L"the second palette entry was not used");

			m.EndFrame();
			Restore();
		}

		// ---------------------------------------------------------------------------------------
		// The report: the software pipeline renders a scene
		// ---------------------------------------------------------------------------------------

		TEST_METHOD(Soft_ReportThePipelineRendersAScene)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupDefaultPixelState(m);
			SetupRasterStage0(m);
			SetClearColor(m, Color(0x10, 0x10, 0x20));

			// A textured image in main memory and in TMEM: an 8x8 I8 checkerboard
			const uint32_t texAddr = 0x00440000;
			std::vector<uint8_t> raw(64);
			for (int tile = 0; tile < 2; tile++)
			{
				for (int v = 0; v < 4; v++)
				{
					for (int u = 0; u < 8; u++)
					{
						raw[tile * 32 + v * 8 + u] = (((u / 2) + (v / 2)) & 1) ? 0xF0 : 0x30;
					}
				}
			}
			WriteMainMemory(texAddr, raw.data(), raw.size());

			m.BpLoad(TX_SETIMAGE0_I0_ID, (8 - 1) | ((8 - 1) << 10) | ((uint32_t)GFX::TF_I8 << 20));
			m.BpLoad(TX_SETIMAGE1_I0_ID, 32u | (1u << 21));		// pre-loaded at TMEM line 32
			m.BpLoad(TX_SETIMAGE3_I0_ID, texAddr >> 5);
			m.BpLoad(TX_SETMODE0_I0_ID, 0);						// clamp/clamp, nearest
			m.BpLoad(TX_SETMODE1_I0_ID, 10u << 8);
			m.BpLoad(TX_LOADBLOCK0_ID, texAddr >> 5);
			m.BpLoad(TX_LOADBLOCK1_ID, 32);
			m.BpLoad(TX_LOADBLOCK3_ID, 2u | (2u << 15));

			m.BeginFrame();

			// ---- the background: one full-screen quad with a colour gradient (RAS2) ----

			GFX::Vertex back[4];
			back[0] = MakeVertex(ObjectX(0), ObjectY(480), 0.5f, Color(0x20, 0x20, 0x60));
			back[1] = MakeVertex(ObjectX(640), ObjectY(480), 0.5f, Color(0x60, 0x20, 0x40));
			back[2] = MakeVertex(ObjectX(640), ObjectY(0), 0.5f, Color(0x20, 0x60, 0x50));
			back[3] = MakeVertex(ObjectX(0), ObjectY(0), 0.5f, Color(0x60, 0x60, 0x20));
			m.DrawQuad(back);

			// ---- a triangle with a depth test (RAS0/RAS2/PE) ----

			m.BpLoad(PE_ZMODE_ID, 1u /*enable*/ | (1u << 1) /*less*/ | (1u << 4) /*mask*/);

			GFX::Vertex tri[3];
			tri[0] = MakeVertex(ObjectX(60), ObjectY(420), 0.0f, Color(0x20, 0xC0, 0x40));
			tri[1] = MakeVertex(ObjectX(320), ObjectY(420), 0.0f, Color(0xE0, 0xE0, 0x20));
			tri[2] = MakeVertex(ObjectX(190), ObjectY(200), 0.0f, Color(0x40, 0x80, 0xFF));
			m.DrawPrimitive(GFX::RAS_TRIANGLE, tri, 3);

			// ---- a quad at the front of the triangle, with the depth test ----

			DrawPixelRect(m, 120.0f, 250.0f, 260.0f, 330.0f, Color(0xE0, 0x30, 0x30), -0.5f);

			// ---- a textured quad (RAS1/TX/TEV) ----

			m.BpLoad(GEN_MODE_ID, 0);
			m.BpLoad(RAS1_TREF0_ID, 0u | (1u << 6));			// ti0 = 0, te0 = 1, tc0 = 0
			m.BpLoad(TEV_COLOR_ENV_0_ID, ColorEnv(ZERO, ZERO, ZERO, TEXC));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, AlphaEnv(AZERO, AZERO, AZERO, 4 /*texa*/));

			GFX::Vertex texquad[4];
			texquad[0] = MakeVertex(ObjectX(380), ObjectY(420), -0.9f, Color(0, 0, 0));
			texquad[1] = MakeVertex(ObjectX(600), ObjectY(420), -0.9f, Color(0, 0, 0));
			texquad[2] = MakeVertex(ObjectX(600), ObjectY(200), -0.9f, Color(0, 0, 0));
			texquad[3] = MakeVertex(ObjectX(380), ObjectY(200), -0.9f, Color(0, 0, 0));
			texquad[0].TexCoord[0][0] = 0.0f; texquad[0].TexCoord[0][1] = 1.0f;
			texquad[1].TexCoord[0][0] = 1.0f; texquad[1].TexCoord[0][1] = 1.0f;
			texquad[2].TexCoord[0][0] = 1.0f; texquad[2].TexCoord[0][1] = 0.0f;
			texquad[3].TexCoord[0][0] = 0.0f; texquad[3].TexCoord[0][1] = 0.0f;
			m.DrawQuad(texquad);

			m.EndFrame();

			// ---- the picture is published for review ----

			std::vector<uint8_t> rgb;
			m.ReadColor(0, 0, 640, 480, rgb);

			std::set<uint32_t> distinct;
			for (int y = 0; y < 480; y += 5)
			{
				for (int x = 0; x < 640; x += 5)
				{
					const uint8_t* p = &rgb[((size_t)y * 640 + x) * 3];
					distinct.insert(((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2]);
				}
			}

			Assert::IsTrue(distinct.size() >= 6, Widen("the scene is (nearly) a flat colour - " +
				std::to_string(distinct.size()) + " distinct colours").c_str());

			Assert::IsTrue(m.SaveScreenshot(OutputDir() + "/gfx_soft_scene.png", 0, 0, 640, 480, 2),
				L"the scene screenshot was not saved");

			Report::Image("Software GFX pipeline: a rendered scene", "gfx_soft_scene.png",
				"A scene drawn entirely by the software pipeline: a colour-gradient background, a "
				"depth-tested triangle with a quad in front of it, and a textured quad whose texels "
				"are read out of TMEM. No OpenGL call is involved.");

			Restore();
		}

		TEST_METHOD(Soft_ReportTheXfbTheCopyEngineWrote)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupDefaultPixelState(m);
			SetupRasterStage0(m);
			SetClearColor(m, Color(0, 0, 0));

			m.BeginFrame();

			// A colourful frame: the copy engine is what turns it into the XFB the TV shows
			GFX::Vertex back[4];
			back[0] = MakeVertex(ObjectX(0), ObjectY(480), 0.0f, Color(0x10, 0x20, 0xC0));
			back[1] = MakeVertex(ObjectX(640), ObjectY(480), 0.0f, Color(0xC0, 0x20, 0x10));
			back[2] = MakeVertex(ObjectX(640), ObjectY(0), 0.0f, Color(0x20, 0xC0, 0x30));
			back[3] = MakeVertex(ObjectX(0), ObjectY(0), 0.0f, Color(0xE0, 0xE0, 0xE0));
			m.DrawQuad(back);

			DrawPixelRect(m, 80.0f, 120.0f, 240.0f, 360.0f, Color(0x10, 0x10, 0x10));
			DrawPixelRect(m, 400.0f, 120.0f, 560.0f, 360.0f, Color(0xF0, 0xF0, 0xF0));

			// The display copy of the whole frame (GXCopyDisp)
			const uint32_t xfbAddr = 0x00520000;
			m.BpLoad(PE_COPY_SRC_ADDR_ID, 0u | (0u << 10));
			m.BpLoad(PE_COPY_SRC_SIZE_ID, (640u - 1) | ((480u - 1) << 10));
			m.BpLoad(PE_COPY_DST_BASE0_ID, xfbAddr >> 5);
			m.BpLoad(PE_COPY_DST_STRIDE_ID, 40);
			m.BpLoad(PE_COPY_CMD_ID, 1u << 14);

			m.EndFrame();

			// Decode the XFB (Y0 U0 Y1 V0, video-interface.md 3.1) into an RGB image the report can
			// show, with the studio-range conversion the video interface uses.
			const uint8_t* xfb = TestMainMemory(xfbAddr, 1280 * 480);
			Assert::IsTrue(xfb != nullptr, L"the XFB is not in the emulated main memory");

			std::vector<uint8_t> rgb((size_t)640 * 480 * 3);

			auto clamp8 = [](int v) -> uint8_t { return (uint8_t)((v < 0) ? 0 : ((v > 255) ? 255 : v)); };

			for (int y = 0; y < 480; y++)
			{
				const uint8_t* line = xfb + (size_t)y * 1280;

				for (int x = 0; x < 640; x += 2)
				{
					int y0 = line[x * 2 + 0], u = line[x * 2 + 1];
					int y1 = line[x * 2 + 2], v = line[x * 2 + 3];

					int c[2][3] = {
						{ y0, u, v },
						{ y1, u, v },
					};

					for (int i = 0; i < 2; i++)
					{
						if (x + i >= 640)
							continue;

						int yy = c[i][0] - 16;
						int cb = c[i][1] - 128;
						int cr = c[i][2] - 128;

						uint8_t* p = &rgb[((size_t)y * 640 + x + i) * 3];
						p[0] = clamp8((76283 * yy + 104595 * cr) >> 16);
						p[1] = clamp8((76283 * yy - 53281 * cr - 25624 * cb) >> 16);
						p[2] = clamp8((76283 * yy + 132252 * cb) >> 16);
					}
				}
			}

			// The report images are stored at half size, like the other galleries
			std::vector<uint8_t> half((size_t)320 * 240 * 3);
			for (int oy = 0; oy < 240; oy++)
			{
				for (int ox = 0; ox < 320; ox++)
				{
					for (int c = 0; c < 3; c++)
					{
						int sum = 0;
						for (int dy = 0; dy < 2; dy++)
							for (int dx = 0; dx < 2; dx++)
								sum += rgb[(((size_t)(oy * 2 + dy) * 640) + (ox * 2 + dx)) * 3 + c];

						half[((size_t)oy * 320 + ox) * 3 + c] = (uint8_t)((sum + 2) / 4);
					}
				}
			}

			Assert::IsTrue(Util::SavePng((OutputDir() + "/gfx_soft_xfb.png").c_str(), half.data(), 320, 240),
				L"the XFB picture was not saved");

			Report::Image("Software GFX pipeline: the XFB the copy engine wrote", "gfx_soft_xfb.png",
				"The same frame after GXCopyDisp: the copy engine converted the EFB rectangle into "
				"packed YUV 4:2:2 in main memory and this picture is that XFB decoded back to RGB. "
				"The chroma of every pixel pair is averaged by the 4:2:2 downsampling, which is why "
				"the vertical edges of the two rectangles show a chroma transition.");

			Restore();
		}

		TEST_METHOD(Soft_AnIndirectStageSamplesItsTextureThroughTheBumpOffset)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupDefaultPixelState(m);
			SetClearColor(m, Color(0, 0, 0));

			// The base image (map 0): an 8x4 I8 texture whose left half is 0x40 and right half 0xC0
			const uint32_t baseAddr = 0x00460000;
			std::vector<uint8_t> base(32, 0);
			for (int v = 0; v < 4; v++)
				for (int u = 4; u < 8; u++)
					base[v * 8 + u] = 0xC0;
			for (int v = 0; v < 4; v++)
				for (int u = 0; u < 4; u++)
					base[v * 8 + u] = 0x40;
			WriteMainMemory(baseAddr, base.data(), base.size());

			// The indirect image (map 1): the same layout, every texel 0x80. Its fields are the
			// offset directive the bump unit multiplies by the matrix.
			const uint32_t indAddr = 0x00470000;
			std::vector<uint8_t> ind(32, 0x80);
			WriteMainMemory(indAddr, ind.data(), ind.size());

			m.BpLoad(TX_SETIMAGE0_I0_ID, (8 - 1) | ((4 - 1) << 10) | ((uint32_t)GFX::TF_I8 << 20));
			m.BpLoad(TX_SETIMAGE1_I0_ID, 16u | (1u << 21));
			m.BpLoad(TX_SETIMAGE3_I0_ID, baseAddr >> 5);
			m.BpLoad(TX_SETMODE0_I0_ID, 0);
			m.BpLoad(TX_SETMODE1_I0_ID, 10u << 8);
			m.BpLoad(TX_LOADBLOCK0_ID, baseAddr >> 5);
			m.BpLoad(TX_LOADBLOCK1_ID, 16);
			m.BpLoad(TX_LOADBLOCK3_ID, 1u | (2u << 15));

			m.BpLoad(TX_SETIMAGE0_I1_ID, (8 - 1) | ((4 - 1) << 10) | ((uint32_t)GFX::TF_I8 << 20));
			m.BpLoad(TX_SETIMAGE1_I1_ID, 32u | (1u << 21));
			m.BpLoad(TX_SETIMAGE3_I1_ID, indAddr >> 5);
			m.BpLoad(TX_SETMODE0_I1_ID, 0);
			m.BpLoad(TX_SETMODE1_I1_ID, 10u << 8);
			m.BpLoad(TX_LOADBLOCK0_ID, indAddr >> 5);
			m.BpLoad(TX_LOADBLOCK1_ID, 32);
			m.BpLoad(TX_LOADBLOCK3_ID, 1u | (2u << 15));

			// One TEV stage: the texel of map 0 sampled with texture coordinate 0
			m.BpLoad(GEN_MODE_ID, 0);
			m.BpLoad(RAS1_TREF0_ID, 0u | (1u << 6));
			m.BpLoad(TEV_COLOR_ENV_0_ID, ColorEnv(ZERO, ZERO, ZERO, TEXC));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, AlphaEnv(AZERO, AZERO, AZERO, 4 /*texa*/));

			// A quad over the EFB pixels (200,200)-(300,300) with the texture coordinates running
			// from (0,0) at the top left to (1,1) at the bottom right
			GFX::Vertex quad[4];
			quad[0] = MakeVertex(ObjectX(200), ObjectY(300), 0.0f, Color(0, 0, 0));
			quad[1] = MakeVertex(ObjectX(300), ObjectY(300), 0.0f, Color(0, 0, 0));
			quad[2] = MakeVertex(ObjectX(300), ObjectY(200), 0.0f, Color(0, 0, 0));
			quad[3] = MakeVertex(ObjectX(200), ObjectY(200), 0.0f, Color(0, 0, 0));

			quad[0].TexCoord[0][0] = 0.0f; quad[0].TexCoord[0][1] = 1.0f;
			quad[1].TexCoord[0][0] = 1.0f; quad[1].TexCoord[0][1] = 1.0f;
			quad[2].TexCoord[0][0] = 1.0f; quad[2].TexCoord[0][1] = 0.0f;
			quad[3].TexCoord[0][0] = 0.0f; quad[3].TexCoord[0][1] = 0.0f;

			uint8_t pixel[4] = { 0 };

			// Without an indirect command the stage samples the base texture directly: the pixel a
			// little left of the texture boundary is in the 0x40 half.
			m.BpLoad(BUMP_CMD_ID, 0);

			m.BeginFrame();
			m.DrawQuad(quad);
			ReadEfbPixel(m, 245, 250, pixel);
			Assert::AreEqual((int)0x40, (int)pixel[0], L"the base texture was not sampled directly");

			// The indirect command of stage 0: the texel of map 1 (`bt` = 1), the 8-bit field
			// format, no bias, mode `bp_m_0` (matrix 0, scale 0). Matrix 0 is the identity with a
			// scale of 23, which turns the texel's 0x80 into a one-texel shift.
			m.BpLoad(BUMP_MATRIX_A0_ID, 1u | (3u << 22));			// ma = 1, s0 = 3
			m.BpLoad(BUMP_MATRIX_B0_ID, (1u << 11) | (1u << 22));	// md = 1, s1 = 1
			m.BpLoad(BUMP_MATRIX_C0_ID, (1u << 22));				// s2 = 1  -> scale 23
			m.BpLoad(BUMP_CMD_ID + 0, 1u | (1u << 9));				// bt = 1, fmt = 8 bit, m = matrix 0

			m.BeginFrame();
			m.DrawQuad(quad);
			ReadEfbPixel(m, 245, 250, pixel);
			Assert::AreEqual((int)0xC0, (int)pixel[0], L"the indirect offset was not applied");

			m.EndFrame();
			Restore();
		}

		// ---------------------------------------------------------------------------------------
		// The copy engine: EFB -> XFB
		// ---------------------------------------------------------------------------------------

		TEST_METHOD(Soft_TheDisplayCopyWritesTheYuvXfb)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupDefaultPixelState(m);
			SetupRasterStage0(m);
			SetClearColor(m, Color(0, 0, 0));

			m.BeginFrame();

			// A pure red frame: the copy engine converts it to the studio-range YUV of the XFB.
			// Red separates the two chroma channels (U is well below 128, V well above it), which
			// is what pins the Y0 U0 Y1 V0 byte order of the destination (video-interface.md 3.1).
			DrawPixelRect(m, 0.0f, 0.0f, 640.0f, 480.0f, Color(255, 0, 0));

			// GXCopyDisp: the whole EFB, a 640-pixel line (40 cache lines of stride) at the XFB
			// address, opcode = display copy (gfx-pe.md 5.6, 6.9-6.15).
			const uint32_t xfbAddr = 0x00500000;
			m.BpLoad(PE_COPY_SRC_ADDR_ID, 0u | (0u << 10));
			m.BpLoad(PE_COPY_SRC_SIZE_ID, (640u - 1) | ((480u - 1) << 10));
			m.BpLoad(PE_COPY_DST_BASE0_ID, xfbAddr >> 5);
			m.BpLoad(PE_COPY_DST_STRIDE_ID, 40);
			m.BpLoad(PE_COPY_CMD_ID, 1u << 14);

			// The XFB holds Y0 U0 Y1 V0 for every pixel pair (video-interface.md 3.1)
			uint8_t* xfb = TestMainMemory(xfbAddr, 8);
			Assert::IsTrue(xfb != nullptr, L"the XFB is not in the emulated main memory");

			// Y = 0.257*255 + 16 = 82, Cb (U) = -0.148*255 + 128 = 90, Cr (V) = 0.439*255 + 128 = 240
			Assert::IsTrue(xfb[0] > 78 && xfb[0] < 86, Widen("the luma of the XFB is " +
				std::to_string((int)xfb[0]) + " instead of about 82").c_str());
			Assert::IsTrue(xfb[1] > 86 && xfb[1] < 94, Widen("the Cb of the XFB is " +
				std::to_string((int)xfb[1]) + " instead of about 90").c_str());
			Assert::IsTrue(xfb[3] > 236, Widen("the Cr of the XFB is " +
				std::to_string((int)xfb[3]) + " instead of about 240").c_str());
			Assert::AreEqual((int)xfb[0], (int)xfb[2], L"the two pixels of the pair differ");

			m.EndFrame();
			Restore();
		}

		TEST_METHOD(Soft_TheDisplayCopyOnlyWritesItsOwnRectangle)
		{
			GfxTestMachine& m = M();

			SetupIdentityMatrices(m);
			SetupOrthoProjection(m);
			SetupFullViewport(m);
			SetupDefaultPixelState(m);
			SetupRasterStage0(m);
			SetClearColor(m, Color(0, 0, 0));

			m.BeginFrame();

			DrawPixelRect(m, 0.0f, 0.0f, 640.0f, 480.0f, Color(255, 0, 0));

			const uint32_t xfbAddr = 0x00510000;
			std::vector<uint8_t> pattern(1280 * 4, 0x77);
			WriteMainMemory(xfbAddr, pattern.data(), pattern.size());

			// A display copy of the top two lines only
			m.BpLoad(PE_COPY_SRC_ADDR_ID, 0u | (0u << 10));
			m.BpLoad(PE_COPY_SRC_SIZE_ID, (640u - 1) | ((2u - 1) << 10));
			m.BpLoad(PE_COPY_DST_BASE0_ID, xfbAddr >> 5);
			m.BpLoad(PE_COPY_DST_STRIDE_ID, 40);
			m.BpLoad(PE_COPY_CMD_ID, 1u << 14);

			uint8_t* xfb = TestMainMemory(xfbAddr, 1280 * 4);

			// The lines the copy did not touch keep the pattern
			Assert::AreEqual((int)0x77, (int)xfb[1280 * 2], L"the copy ran past its rectangle");

			m.EndFrame();
			Restore();
		}
	};
}
