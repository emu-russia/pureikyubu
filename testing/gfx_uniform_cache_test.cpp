// The uniform upload cache (src/gfxuniformcache.cpp).
//
// The shader pipeline is a pair of static programs, so every draw re-uploads the whole register
// state of the blocks to them (Rasterizer::SetUpPipeline -> TransformUnit::UploadUniforms and
// TextureEnvironmentUnit::UploadUniforms). The cache drops the uploads whose value has not changed,
// and these tests drive that from both sides:
//
//   * the values: an upload that carries what the uniform already holds must not reach the GL
//     context, and one that carries something else must;
//   * the identity of the program: a draw whose register state did not change but whose program was
//     linked again (GEN_MODE.flat_en relinks the TEV program at run time, and a pipeline switch
//     closes the backend and opens it again) must be uploaded again, because the uniforms of a
//     fresh program are the defaults;
//   * the switch of the cache itself (the GFX_UNIFORM_CACHE setting and the `gxuniformcache`
//     command): off, an upload reaches the context even when it carries what the uniform holds.
//
// What the program really holds is read back from the GL context with glGetUniformfv, so these
// tests do not count the calls of the cache: an upload the cache dropped is visible as the value
// another writer left in the uniform. The rendering tests read the EFB, so a value that never
// reached the shaders shows up as a wrong picture.

#include "pch.h"
#include "gfx_test_common.h"

using namespace GfxUnitTest;

namespace pureikyubutest
{
	TEST_CLASS(GfxUniformCacheTest)
	{
		//! The value of a float uniform, as the GL context holds it (the program must be current).
		static GLfloat ReadFloatUniform(GFX::GLProgram& program, const char* name)
		{
			GLfloat v = 0.0f;
			glGetUniformfv(program.prog, program.Uniform(name), &v);
			return v;
		}

		//! The value of a vec4 uniform, as the GL context holds it.
		static void ReadVec4Uniform(GFX::GLProgram& program, const char* name, GLfloat v[4])
		{
			glGetUniformfv(program.prog, program.Uniform(name), v);
		}

		//! Pack a TEV_COLOR_ENV payload (the layout of the register, gfx-tev.md 4.2).
		static uint32_t PackColorEnv(int sela, int selb, int selc, int seld, int bias, int sub, int clamp,
			int shift, int dest)
		{
			return (uint32_t)seld | ((uint32_t)selc << 4) | ((uint32_t)selb << 8) | ((uint32_t)sela << 12) |
				((uint32_t)bias << 16) | ((uint32_t)sub << 18) | ((uint32_t)clamp << 19) |
				((uint32_t)shift << 20) | ((uint32_t)dest << 22);
		}

		//! Pack a TEV_ALPHA_ENV payload.
		static uint32_t PackAlphaEnv(int sela, int selb, int selc, int seld, int bias, int sub, int clamp,
			int shift, int dest)
		{
			return (uint32_t)seld << 4 | ((uint32_t)selc << 7) | ((uint32_t)selb << 10) |
				((uint32_t)sela << 13) | ((uint32_t)bias << 16) | ((uint32_t)sub << 18) |
				((uint32_t)clamp << 19) | ((uint32_t)shift << 20) | ((uint32_t)dest << 22);
		}

		//! A pass-through XF and one TEV stage whose result is twice the colour register 0.
		static void SetupPipeline(GfxTestMachine& m)
		{
			float identity[16] = {
				1,0,0,0,
				0,1,0,0,
				0,0,1,0,
				0,0,0,1,
			};
			m.XfLoadFloats(GFX::XF_MATRIX_MEMORY_ID, identity, 16);

			float proj[6] = { 1,0, 1,0, 1,0 };
			m.XfLoadFloats(GFX::XF_PROJECTION_A_ID, proj, 6);
			m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, 0x3f800000);		// 1.0f
			m.XfLoad(GFX::XF_NUMCOLS_ID, 0);					// the host colours pass through
			m.XfLoad(GFX::XF_NUMTEX_ID, 0);
			m.XfLoad(GFX::XF_MATINDEX_A_ID, 0);

			m.BpLoad(RAS1_TREF0_ID, 0);			// te = 0, cc0 = 0 -> rasterized colour 0
			m.BpLoad(GEN_MODE_ID, 0);			// ntev = 0 -> one stage

			// Stage 0: a = b = c = d = the colour register 0, so the result is (reg0 + reg0), clamped.
			// dest is register 0, and the result of the last stage is what reaches the EFB (the same
			// shape as the pass-through stage of the TEV tests).
			m.BpLoad(TEV_COLOR_ENV_0_ID, PackColorEnv(0, 0, 0, 0, 0, 0, 1, 0, 0));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, PackAlphaEnv(0, 0, 0, 0, 0, 0, 1, 0, 0));

			m.BpLoad(PE_ZMODE_ID, 0);			// no depth test
			m.BpLoad(PE_CMODE0_ID, 0x18);		// no blend, no logic op
		}

		//! The colour register 0 of the TEV, as gfx-tev.md 4.2 lays it out (r and a in the low
		//! register, b and g in the high one).
		static void SetColorRegister0(GfxTestMachine& m, int r, int g, int b)
		{
			m.BpLoad(TEV_REGISTERL_0_ID, (uint32_t)r | (0xffu << 12));
			m.BpLoad(TEV_REGISTERH_0_ID, (uint32_t)b | ((uint32_t)g << 12));
		}

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

	public:

		// =========================================================================================
		// The values
		// =========================================================================================

		TEST_METHOD(UniformCache_AnUnchangedValueIsNotUploaded)
		{
			RequireGL();
			GfxTestMachine& m = Machine();

			GFX::GLProgram* program = m.gfx->tev->GetTevProgram();
			Assert::IsNotNull(program, L"the TEV program was not linked");
			program->Use();

			m.gfx->uniformCache.Invalidate();

			// The first upload of a value reaches the program ...
			m.gfx->uniformCache.Set1f(*program, "tevFogA", 3.5f);
			Assert::AreEqual(3.5f, ReadFloatUniform(*program, "tevFogA"), 0.0f, L"the first upload");

			// ... and the second one carries the same bytes, so it is dropped: the value another
			// writer left in the uniform survives it (the GL context is what the test observes).
			glUniform1f(program->Uniform("tevFogA"), 42.0f);
			m.gfx->uniformCache.Set1f(*program, "tevFogA", 3.5f);
			Assert::AreEqual(42.0f, ReadFloatUniform(*program, "tevFogA"), 0.0f,
				L"the repeated upload reached the program");

			// A value that really changed is uploaded again.
			m.gfx->uniformCache.Set1f(*program, "tevFogA", 3.25f);
			Assert::AreEqual(3.25f, ReadFloatUniform(*program, "tevFogA"), 0.0f, L"the changed value");

			// An array is compared as a whole: one item that changed is an upload, and the whole
			// array is what the uniform then holds.
			GLfloat reg[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
			m.gfx->uniformCache.Set4fv(*program, "tevReg[0]", 1, reg);

			GLfloat got[4] = { 0, 0, 0, 0 };
			ReadVec4Uniform(*program, "tevReg[0]", got);
			for (int i = 0; i < 4; i++)
			{
				Assert::AreEqual(reg[i], got[i], 0.0f, L"the array upload");
			}

			glUniform4f(program->Uniform("tevReg[0]"), 9.0f, 9.0f, 9.0f, 9.0f);
			m.gfx->uniformCache.Set4fv(*program, "tevReg[0]", 1, reg);

			ReadVec4Uniform(*program, "tevReg[0]", got);
			Assert::AreEqual(9.0f, got[0], 0.0f, L"the repeated array upload reached the program");

			reg[2] = 5.0f;
			m.gfx->uniformCache.Set4fv(*program, "tevReg[0]", 1, reg);

			ReadVec4Uniform(*program, "tevReg[0]", got);
			Assert::AreEqual(5.0f, got[2], 0.0f, L"the item that changed");

			// The cache can be dropped as a whole (the pipeline switch does exactly this): the next
			// upload of the same value goes to the context again.
			glUniform1f(program->Uniform("tevFogA"), 7.0f);
			m.gfx->uniformCache.Invalidate();
			m.gfx->uniformCache.Set1f(*program, "tevFogA", 3.25f);

			Assert::AreEqual(3.25f, ReadFloatUniform(*program, "tevFogA"), 0.0f,
				L"the upload after the cache was dropped");
		}

		// The register state of a draw is uploaded through the cache, so a register that changed has
		// to reach the program (and the picture), together with everything that did not.
		TEST_METHOD(UniformCache_ARegisterChangeReachesTheProgramAndThePicture)
		{
			RequireGL();
			GfxTestMachine& m = Machine();

			SetupPipeline(m);
			SetColorRegister0(m, 32, 64, 96);

			m.BeginFrame();
			DrawFullScreenQuad(m, 0x80, 0x40, 0x20, 0xff);

			GFX::GLProgram* program = m.gfx->tev->GetTevProgram();
			Assert::IsNotNull(program, L"the TEV program was not linked");
			program->Use();

			GLfloat reg[4] = { 0, 0, 0, 0 };
			ReadVec4Uniform(*program, "tevReg[0]", reg);
			Assert::AreEqual(32.0f, reg[0], 0.0f, L"tevReg[0].r after the draw");
			Assert::AreEqual(64.0f, reg[1], 0.0f, L"tevReg[0].g after the draw");
			Assert::AreEqual(96.0f, reg[2], 0.0f, L"tevReg[0].b after the draw");

			uint8_t before[3];
			m.ReadColorPixel(320, 240, before);
			Assert::AreEqual<int>(64, before[0], L"the picture of the first draw (2 * 32)");
			Assert::AreEqual<int>(128, before[1], L"the picture of the first draw (2 * 64)");
			Assert::AreEqual<int>(192, before[2], L"the picture of the first draw (2 * 96)");

			// The register moves, so the next draw carries a different value for tevReg[0]: the cache
			// has to notice, or the picture would keep the colour of the previous one.
			SetColorRegister0(m, 40, 70, 100);

			m.BeginFrame();
			DrawFullScreenQuad(m, 0x80, 0x40, 0x20, 0xff);

			ReadVec4Uniform(*program, "tevReg[0]", reg);
			Assert::AreEqual(40.0f, reg[0], 0.0f, L"tevReg[0].r after the register moved");

			uint8_t after[3];
			m.ReadColorPixel(320, 240, after);
			Assert::AreEqual<int>(80, after[0], L"the picture follows the register (2 * 40)");
			Assert::AreEqual<int>(140, after[1], L"the picture follows the register (2 * 70)");
			Assert::AreEqual<int>(200, after[2], L"the picture follows the register (2 * 100)");
		}

		// =========================================================================================
		// The switch of the cache (the GFX_UNIFORM_CACHE setting, the settings window and the
		// `gxuniformcache` command)
		// =========================================================================================

		TEST_METHOD(UniformCache_TheSwitchTurnsTheSkippingOffAndOn)
		{
			RequireGL();
			GfxTestMachine& m = Machine();

			GFX::GLProgram* program = m.gfx->tev->GetTevProgram();
			Assert::IsNotNull(program, L"the TEV program was not linked");
			program->Use();

			// The shipped default is on (GFX_UNIFORM_CACHE = 1).
			Assert::IsTrue(m.gfx->UniformCacheEnabled(), L"the cache is off at start-up");

			m.gfx->uniformCache.Invalidate();
			m.gfx->uniformCache.Set1f(*program, "tevFogA", 2.0f);

			glUniform1f(program->Uniform("tevFogA"), 8.0f);
			m.gfx->uniformCache.Set1f(*program, "tevFogA", 2.0f);
			Assert::AreEqual(8.0f, ReadFloatUniform(*program, "tevFogA"), 0.0f,
				L"the cache is on: the upload is dropped");

			// Off: every upload of the pipeline reaches the context again, and the choice is kept in
			// the configuration for the next start.
			m.gfx->SetUniformCache(false);
			Assert::IsFalse(m.gfx->UniformCacheEnabled(), L"the cache did not switch off");
			Assert::AreEqual(0, GetConfigInt(USER_GFX_UNIFORM_CACHE, USER_HW), L"the choice is not in the config");

			m.gfx->uniformCache.Set1f(*program, "tevFogA", 2.0f);
			Assert::AreEqual(2.0f, ReadFloatUniform(*program, "tevFogA"), 0.0f,
				L"the cache is off: the upload reaches the program");

			// On again: the context was free to move on while the cache was off, so the value it
			// holds is not trusted - the first upload of it goes through, the next one is dropped.
			m.gfx->SetUniformCache(true);
			Assert::IsTrue(m.gfx->UniformCacheEnabled(), L"the cache did not switch on");
			Assert::AreEqual(1, GetConfigInt(USER_GFX_UNIFORM_CACHE, USER_HW), L"the choice is not in the config");

			glUniform1f(program->Uniform("tevFogA"), 8.0f);
			m.gfx->uniformCache.Set1f(*program, "tevFogA", 2.0f);
			Assert::AreEqual(2.0f, ReadFloatUniform(*program, "tevFogA"), 0.0f,
				L"the upload after the cache came back");

			glUniform1f(program->Uniform("tevFogA"), 8.0f);
			m.gfx->uniformCache.Set1f(*program, "tevFogA", 2.0f);
			Assert::AreEqual(8.0f, ReadFloatUniform(*program, "tevFogA"), 0.0f,
				L"the cache skips again");
		}

		// =========================================================================================
		// The program the values belong to
		// =========================================================================================

		// GEN_MODE.flat_en relinks the TEV program at run time, and the fresh program has no uniform
		// set at all. The values of the program before it must not be taken for its values.
		TEST_METHOD(UniformCache_ARelinkedProgramIsUploadedAgain)
		{
			RequireGL();
			GfxTestMachine& m = Machine();

			GFX::GLProgram* first = m.gfx->tev->GetTevProgram();
			Assert::IsNotNull(first, L"the TEV program was not linked");

			// The identity of the storage (the cache keys on it) is read before the object is thrown
			// away: the flip below deletes it.
			uint64_t firstSerial = first->serial;

			first->Use();
			m.gfx->uniformCache.Set1f(*first, "tevFogA", 1.25f);
			Assert::AreEqual(1.25f, ReadFloatUniform(*first, "tevFogA"), 0.0f, L"the value of the first program");

			m.BpLoad(GEN_MODE_ID, 1u << 8);			// flat_en

			GFX::GLProgram* second = m.gfx->tev->GetTevProgram();
			Assert::IsNotNull(second, L"the flat TEV program was not linked");
			Assert::AreNotEqual<uint64_t>(firstSerial, second->serial, L"the program was not linked again");

			second->Use();

			// The value the cache holds for the name is not a value this program has: the upload has
			// to go through (the value another writer left in the new program is what an upload the
			// cache dropped would leave in place).
			glUniform1f(second->Uniform("tevFogA"), 9.0f);
			m.gfx->uniformCache.Set1f(*second, "tevFogA", 1.25f);
			Assert::AreEqual(1.25f, ReadFloatUniform(*second, "tevFogA"), 0.0f,
				L"the value did not reach the relinked program");

			// The machine is shared by the tests; the flat bit is put back (each test opens with a
			// reset, but the state of this one should not travel).
			m.BpLoad(GEN_MODE_ID, 0);
		}

		// The pipeline switch closes the GL backend (and the software pipeline never uploads
		// anything), so the values of the pipeline that is left describe a context that is gone. The
		// same state is programmed and drawn again after the switch: the picture has to come out the
		// same, which can only happen if the fresh program was given the whole state.
		TEST_METHOD(UniformCache_ThePipelineSwitchDoesNotLeaveStaleValuesBehind)
		{
			RequireGL();
			GfxTestMachine& m = Machine();

			SetupPipeline(m);
			SetColorRegister0(m, 32, 64, 96);

			m.BeginFrame();
			DrawFullScreenQuad(m, 0x80, 0x40, 0x20, 0xff);

			uint8_t before[3];
			m.ReadColorPixel(320, 240, before);
			Assert::AreEqual<int>(64, before[0], L"the picture before the switch");

			m.SetPipeline(GFX_PIPELINE_SOFT);
			m.SetPipeline(GFX_PIPELINE_SHADER);

			// Exactly the state that was drawn before the switch: every value of it is what the cache
			// held when the backend went away.
			SetupPipeline(m);
			SetColorRegister0(m, 32, 64, 96);

			m.BeginFrame();
			DrawFullScreenQuad(m, 0x80, 0x40, 0x20, 0xff);

			uint8_t after[3];
			m.ReadColorPixel(320, 240, after);

			Assert::AreEqual<int>(before[0], after[0], L"red after the switch");
			Assert::AreEqual<int>(before[1], after[1], L"green after the switch");
			Assert::AreEqual<int>(before[2], after[2], L"blue after the switch");

			CheckGLError("drawing after the pipeline switch");
		}
	};
}
