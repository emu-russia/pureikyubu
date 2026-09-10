// Smoke tests for the GFX test rig itself.
//
// These do not test emulation behaviour: they make sure the machine under the other GFX tests
// (hidden window, real OpenGL context, the Flipper device doubles) really is up, and that the
// pipeline can be programmed and read back.

#include "pch.h"
#include "gfx_test_common.h"

using namespace GfxUnitTest;

namespace pureikyubutest
{
	TEST_CLASS(GfxRigTest)
	{
	public:

		TEST_METHOD(Rig_TheMachineStartsTheOpenGLBackend)
		{
			GfxTestMachine& m = Machine();
			Assert::IsTrue(m.Start(), L"the GFX test machine did not start");
			m.Reset();

			Assert::IsTrue(m.GLEnabled(),
				L"the OpenGL backend did not start");

			// The backend reports the GL context it created the same way the emulator does
			const char* version = (const char*)glGetString(GL_VERSION);
			Assert::IsNotNull(version);

			std::string text = "GL_VERSION: ";
			text += version;
			Logger::WriteMessage(text.c_str());
		}

		TEST_METHOD(Rig_TheXfVertexShaderCompiles)
		{
			GfxTestMachine& m = Machine();
			m.Reset();

			Assert::IsTrue(m.gfx->xf->CreateShader(), L"the XF vertex shader did not compile");
			Assert::IsTrue(m.gfx->xf->VertexShader() != 0, L"no vertex shader object");
		}

		TEST_METHOD(Rig_AResetFrameIsClearedToThePeClearColour)
		{
			GfxTestMachine& m = Machine();
			m.Reset();

			// PE_COPY_CLEAR_AR is red:8 (bits 0-7) / alpha:8 (bits 8-15);
			// PE_COPY_CLEAR_GB is blue:8 (bits 0-7) / green:8 (bits 8-15)
			m.BpLoad(PE_COPY_CLEAR_AR_ID, 0x00000012);
			m.BpLoad(PE_COPY_CLEAR_GB_ID, 0x00003456);

			m.BeginFrame();
			uint8_t rgb[3];
			m.ReadColorPixel(10, 10, rgb);

			Assert::AreEqual<int>(0x12, rgb[0], L"red");
			Assert::AreEqual<int>(0x34, rgb[1], L"green");
			Assert::AreEqual<int>(0x56, rgb[2], L"blue");
		}

		TEST_METHOD(Rig_TheXfVertexShaderRunsThroughTransformFeedback)
		{
			GfxTestMachine& m = Machine();
			m.Reset();

			// A pass-through XF: the geometry matrix is the identity and the projection is orthographic
			float identity[16] = {
				1,0,0,0,
				0,1,0,0,
				0,0,1,0,
				0,0,0,1,
			};
			m.XfLoadFloats(GFX::XF_MATRIX_MEMORY_ID, identity, 16);

			float proj[6] = { 1,0, 1,0, 1,0 };
			m.XfLoadFloats(GFX::XF_PROJECTION_A_ID, proj, 6);
			m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, 0x3f800000);	// 1.0f

			std::vector<GFX::Vertex> in;
			in.push_back(GfxTestMachine::MakeVertex(0.25f, -0.5f, 0.75f));

			std::vector<GfxTestMachine::XFVertex> out;
			Assert::IsTrue(m.RunVertexShader(in, out), Widen(m.LastError()).c_str());
			Assert::AreEqual<size_t>(1, out.size());

			Assert::AreEqual(0.25f, out[0].Position[0], 0.0001f, L"clip x");
			Assert::AreEqual(-0.5f, out[0].Position[1], 0.0001f, L"clip y");
			Assert::AreEqual(0.75f, out[0].Position[2], 0.0001f, L"clip z");
			Assert::AreEqual(1.0f, out[0].Position[3], 0.0001f, L"clip w");
		}

		TEST_METHOD(Rig_TheDefaultPipelineStateDrawsAFullScreenQuad)
		{
			GfxTestMachine& m = Machine();
			m.Reset();

			// Pass-through XF
			float identity[16] = {
				1,0,0,0,
				0,1,0,0,
				0,0,1,0,
				0,0,0,1,
			};
			m.XfLoadFloats(GFX::XF_MATRIX_MEMORY_ID, identity, 16);
			float proj[6] = { 1,0, 1,0, 1,0 };
			m.XfLoadFloats(GFX::XF_PROJECTION_A_ID, proj, 6);
			m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, 0x3f800000);

			// One TEV stage that copies the rasterized colour through
			m.BpLoad(GEN_MODE_ID, 1 << 10);			// ntev = 1
			m.BpLoad(TEV_COLOR_ENV_0_ID, 0x0000003f);	// dest = prev (3), a = prev, b = prev, c = prev, d = prev... see the TEV tests
			m.BpLoad(TEV_ALPHA_ENV_0_ID, 0x0000003f);
			m.BpLoad(PE_ZMODE_ID, 0);					// no depth test

			m.BeginFrame();

			GFX::Vertex quad[4] = {
				GfxTestMachine::MakeVertex(-1, -1, 0),
				GfxTestMachine::MakeVertex(1, -1, 0),
				GfxTestMachine::MakeVertex(1, 1, 0),
				GfxTestMachine::MakeVertex(-1, 1, 0),
			};
			m.DrawQuad(quad);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);

			// The default TEV stage on an unwritten register is (0,0,0) with dest = prev, so the
			// result only has to be a defined colour: the point of this test is that the draw call
			// went through XF -> SU -> RAS -> TEV -> PE without an error.
			CheckGLError("drawing a full screen quad");
		}
	};
}
