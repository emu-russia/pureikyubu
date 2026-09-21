// Pixel Engine (PE) tests.
//
// The PE owns the BP registers 0x40-0x59: the Z mode, the colour/blend mode, the field mask, the
// copy engine registers and the EFB bounds (see gfx-pe.md 6).
//
// What the OpenGL backend can honour of them is checked by rendering: the Z mode, the blend mode,
// the write masks and the copy engine's clear. The registers that only describe hardware details
// (the pixel type, the Z compression format, the copy bounds) are checked by decoding them.

#include "pch.h"
#include "gfx_test_common.h"

using namespace GfxUnitTest;

namespace pureikyubutest
{
	TEST_CLASS(GfxPeTest)
	{
		static GfxTestMachine& M()
		{
			GfxTestMachine& m = Machine();
			m.Reset();
			return m;
		}

		//! A pass-through XF and TEV so that a draw lands in the EFB as the vertex colour.
		static void SetupPassThrough(GfxTestMachine& m)
		{
			float identity[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
			m.XfLoadFloats(GFX::XF_MATRIX_MEMORY_ID, identity, 16);
			float proj[6] = { 1,0, 1,0, 1,0 };
			m.XfLoadFloats(GFX::XF_PROJECTION_A_ID, proj, 6);
			m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, 0x3f800000);
			m.XfLoad(GFX::XF_NUMCOLS_ID, 0);

			m.BpLoad(RAS1_TREF0_ID, 0);

			// One stage that outputs the rasterized colour and its alpha
			m.BpLoad(GEN_MODE_ID, 0);
			m.BpLoad(TEV_COLOR_ENV_0_ID, (15u << 12) | (15u << 8) | (15u << 4) | 10u | (1u << 19));
			m.BpLoad(TEV_ALPHA_ENV_0_ID, (5u << 13) | (5u << 10) | (5u << 7) | (5u << 4) | (1u << 19));
		}

		static void DrawQuad(GfxTestMachine& m, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xff)
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
		// The register file
		// =========================================================================================

		// gfx-pe.md 6: every PE register must be decoded by the PE; a register swallowed by the
		// bypass chain (which walks PE -> BUMP -> TX -> TEV) would silently do nothing.
		TEST_METHOD(Pe_EveryRegisterIsDecodedByThePixelEngine)
		{
			GfxTestMachine& m = M();

			for (unsigned index = PE_ZMODE_ID; index <= PE_QUAD_OFFSET_ID; index++)
			{
				// BIT(31) and BIT(30) are reserved "rid" bits, so the payload is 24 bits wide
				uint32_t value = 0x00AA0000u | (index * 7u);

				ClearTestLog();
				EnableTestLog(true);
				m.BpLoad(index, value);
				std::string log = TestLogText();
				EnableTestLog(false);

				wchar_t msg[128];
				swprintf_s(msg, L"PE register 0x%02X", index);
				Assert::IsTrue(log.find("Unknown") == std::string::npos,
					Widen(std::string(Utf8Of(msg)) + ": " + log).c_str());
			}
		}

		static std::string Utf8Of(const wchar_t* text)
		{
			return Util::WstringToString(text);
		}

		TEST_METHOD(Pe_ZModeIsDecodedFieldByField)
		{
			GfxTestMachine& m = M();

			for (unsigned func = 0; func < 8; func++)
			{
				// enable = 1, func, mask = 1
				m.BpLoad(PE_ZMODE_ID, 1u | (func << 1) | (1u << 4));

				const GFX::PE_ZMODE& zmode = m.gfx->pe->State().zmode;
				Assert::AreEqual<unsigned>(1, zmode.enable, L"enable");
				Assert::AreEqual<unsigned>(func, zmode.func, L"func");
				Assert::AreEqual<unsigned>(1, zmode.mask, L"mask");
			}
		}

		TEST_METHOD(Pe_ColorModeIsDecodedFieldByField)
		{
			GfxTestMachine& m = M();

			// blend_en = 1, logop_en = 1, dither_en = 1, col_mask = 1, alpha_mask = 1,
			// dfactor = 3, sfactor = 5, blendop = 1, logop = 9
			uint32_t value = 1u | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4) |
				(3u << 5) | (5u << 8) | (1u << 11) | (9u << 12);
			m.BpLoad(PE_CMODE0_ID, value);

			const GFX::PE_CMODE0& cmode0 = m.gfx->pe->State().cmode0;
			Assert::AreEqual<unsigned>(1, cmode0.blend_en, L"blend_en");
			Assert::AreEqual<unsigned>(1, cmode0.logop_en, L"logop_en");
			Assert::AreEqual<unsigned>(1, cmode0.dither_en, L"dither_en");
			Assert::AreEqual<unsigned>(1, cmode0.col_mask, L"col_mask");
			Assert::AreEqual<unsigned>(1, cmode0.alpha_mask, L"alpha_mask");
			Assert::AreEqual<unsigned>(3, cmode0.dfactor, L"dfactor");
			Assert::AreEqual<unsigned>(5, cmode0.sfactor, L"sfactor");
			Assert::AreEqual<unsigned>(1, cmode0.blendop, L"blendop");
			Assert::AreEqual<unsigned>(9, cmode0.logop, L"logop");
		}

		TEST_METHOD(Pe_ControlAndCopyRegistersAreDecoded)
		{
			GfxTestMachine& m = M();

			m.BpLoad(PE_CONTROL_ID, (2u << 0) | (4u << 3) | (1u << 6));
			m.BpLoad(PE_FIELD_MASK_ID, 0);
			m.BpLoad(PE_REFRESH_ID, 0x123);
			m.BpLoad(PE_COPY_SRC_ADDR_ID, (10u << 0) | (20u << 10));
			m.BpLoad(PE_COPY_SRC_SIZE_ID, (30u << 0) | (40u << 10));
			m.BpLoad(PE_COPY_DST_BASE0_ID, 0x12345);
			m.BpLoad(PE_COPY_DST_BASE1_ID, 0x23456);
			m.BpLoad(PE_COPY_DST_STRIDE_ID, 0x40);
			m.BpLoad(PE_COPY_SCALE_ID, 0x100);
			m.BpLoad(PE_COPY_VFILTER0_ID, 0x30u | (0x20u << 6) | (0x10u << 12) | (0x08u << 18));
			m.BpLoad(PE_COPY_VFILTER1_ID, 0x20u | (0x10u << 6) | (0x08u << 12));
			m.BpLoad(PE_XBOUND_ID, (100u << 0) | (200u << 10));
			m.BpLoad(PE_YBOUND_ID, (50u << 0) | (150u << 10));
			m.BpLoad(PE_PERFMODE_ID, 0x155);
			m.BpLoad(PE_CHICKEN_ID, 0xf);
			m.BpLoad(PE_QUAD_OFFSET_ID, 0x12345);
			m.BpLoad(PE_CMODE1_ID, 0x80 | (1u << 8) | (3u << 9));

			const GFX::PEState& pe = m.gfx->pe->State();
			Assert::AreEqual<unsigned>(2, pe.control.pixtype, L"pixtype");
			Assert::AreEqual<unsigned>(4, pe.control.zcmode, L"zcmode");
			Assert::AreEqual<unsigned>(1, pe.control.ztop, L"ztop");
			Assert::AreEqual<unsigned>(0, pe.field_mask.bits, L"field mask");
			Assert::AreEqual<unsigned>(0x123, pe.refresh.interval, L"refresh interval");
			Assert::AreEqual<unsigned>(10, pe.copy_src_addr.x, L"copy src x");
			Assert::AreEqual<unsigned>(20, pe.copy_src_addr.y, L"copy src y");
			Assert::AreEqual<unsigned>(30, pe.copy_src_size.x, L"copy size x");
			Assert::AreEqual<unsigned>(40, pe.copy_src_size.y, L"copy size y");
			Assert::AreEqual<unsigned>(0x12345, pe.copy_dst_base[0].base, L"dst base 0");
			Assert::AreEqual<unsigned>(0x23456, pe.copy_dst_base[1].base, L"dst base 1");
			Assert::AreEqual<unsigned>(0x40, pe.copy_dst_stride.stride, L"dst stride");
			Assert::AreEqual<unsigned>(0x100, pe.copy_scale.scale, L"copy scale");
			Assert::AreEqual<unsigned>(0x30, pe.vfilter_0.coeff0, L"vfilter coeff0");
			Assert::AreEqual<unsigned>(0x20, pe.vfilter_0.coeff1, L"vfilter coeff1");
			Assert::AreEqual<unsigned>(0x20, pe.vfilter_1.coeff4, L"vfilter coeff4");
			Assert::AreEqual<unsigned>(0x10, pe.vfilter_1.coeff5, L"vfilter coeff5");
			Assert::AreEqual<unsigned>(100, pe.xbound.left, L"xbound left");
			Assert::AreEqual<unsigned>(200, pe.xbound.right, L"xbound right");
			Assert::AreEqual<unsigned>(50, pe.ybound.top, L"ybound top");
			Assert::AreEqual<unsigned>(150, pe.ybound.bottom, L"ybound bottom");
			Assert::AreEqual<unsigned>(0x155, pe.perfmode.bits, L"perfmode");
			Assert::AreEqual<unsigned>(0xf, pe.chicken.bits, L"chicken");
			Assert::AreEqual<unsigned>(0x12345, pe.quad_offset.bits, L"quad offset");
			Assert::AreEqual<unsigned>(0x80, pe.cmode1.const_alpha, L"const alpha");
			Assert::AreEqual<unsigned>(1, pe.cmode1.const_alpha_en, L"const alpha enable");
			Assert::AreEqual<unsigned>(3, pe.cmode1.yuv, L"yuv");
		}

		// =========================================================================================
		// The Z path
		// =========================================================================================

		// PE_ZMODE.func: 0 never, 1 less, 2 equal, 3 lequal, 4 greater, 5 nequal, 6 gequal, 7 always
		TEST_METHOD(Pe_ZTestUsesTheConfiguredCompareFunction)
		{
			RequireGL();

			struct Case
			{
				int func;
				float z;
				bool drawn;
				const wchar_t* name;
			};

			// The quad is drawn at a window depth of 0.5; the clear depth is 1.0. The z below is the GX
			// clip z that produces it: the GX clip range is (-w, 0] and the shader maps it onto GL's,
			// so the window depth is z + 1 for an orthographic pass-through.
			const Case cases[] = {
				{ 0, -0.5f, false, L"never" },
				{ 1, -0.5f, true,  L"less (0.5 < 1.0)" },
				{ 3, -0.5f, true,  L"lequal" },
				{ 4, -0.5f, false, L"greater (0.5 > 1.0 is false)" },
				{ 7, -0.5f, true,  L"always" },
			};

			for (const Case& c : cases)
			{
				GfxTestMachine& m = M();
				SetupPassThrough(m);

				// The vertex z is the GX clip space z; with the default depth range [0,1] the shader
				// maps a GX clip z of -0.25 to a window depth of 0.75 (the GX far plane, z = 0, is the
				// window depth 1).
				m.BpLoad(PE_COPY_CLEAR_Z_ID, 0xFFFFFF);	// clear the depth to the far value
				m.BpLoad(PE_ZMODE_ID, 1u | ((unsigned)c.func << 1) | (1u << 4));
				m.BpLoad(PE_CMODE0_ID, 0x18);

				m.BeginFrame();

				GFX::Vertex quad[4] = {
					GfxTestMachine::MakeVertex(-1, -1, c.z, 0xff, 0xff, 0xff, 0xff),
					GfxTestMachine::MakeVertex(1, -1, c.z, 0xff, 0xff, 0xff, 0xff),
					GfxTestMachine::MakeVertex(1, 1, c.z, 0xff, 0xff, 0xff, 0xff),
					GfxTestMachine::MakeVertex(-1, 1, c.z, 0xff, 0xff, 0xff, 0xff),
				};
				m.DrawQuad(quad);

				uint8_t rgb[3];
				m.ReadColorPixel(320, 240, rgb);

				bool drawn = (rgb[0] != 0);
				Assert::AreEqual(c.drawn, drawn, c.name);
			}
		}

		// The Z write mask decides whether the depth of a drawn pixel reaches the depth buffer.
		TEST_METHOD(Pe_ZWriteMaskControlsTheDepthUpdate)
		{
			RequireGL();

			for (int mask = 0; mask < 2; mask++)
			{
				GfxTestMachine& m = M();
				SetupPassThrough(m);

				// Depth test always, with the write mask bit under test
				m.BpLoad(PE_COPY_CLEAR_Z_ID, 0xFFFFFF);
				m.BpLoad(PE_ZMODE_ID, 1u | (7u << 1) | ((unsigned)mask << 4));
				m.BpLoad(PE_CMODE0_ID, 0x18);

				m.BeginFrame();

				float cleared = m.ReadDepthPixel(320, 240);
				Assert::AreEqual(1.0f, cleared, 0.01f, L"the frame clear must fill the depth with the clear Z");

				GLboolean writeMask = 1;
				glGetBooleanv(GL_DEPTH_WRITEMASK, &writeMask);
				Assert::AreEqual<int>(mask ? 1 : 0, writeMask ? 1 : 0, L"GL_DEPTH_WRITEMASK");

				GFX::Vertex quad[4] = {
					GfxTestMachine::MakeVertex(-1, -1, -0.25f, 0xff, 0xff, 0xff, 0xff),
					GfxTestMachine::MakeVertex(1, -1, -0.25f, 0xff, 0xff, 0xff, 0xff),
					GfxTestMachine::MakeVertex(1, 1, -0.25f, 0xff, 0xff, 0xff, 0xff),
					GfxTestMachine::MakeVertex(-1, 1, -0.25f, 0xff, 0xff, 0xff, 0xff),
				};
				m.DrawQuad(quad);

				float depth = m.ReadDepthPixel(320, 240);

				if (mask)
				{
					// The quad's window depth is 0.75; the buffer must have been updated
					Assert::IsTrue(fabsf(depth - 0.75f) < 0.01f, Widen("the depth was not written: " +
						std::to_string(depth)).c_str());
				}
				else
				{
					// The clear depth survives
					Assert::AreEqual(1.0f, depth, 0.01f, L"the depth must not have been written");
				}
			}
		}

		// GEN_MODE.zfreeze freezes the Z buffer: the depth of the first primitive is reused and no
		// later primitive writes it (gfx-su.md 3.6, gfx-pe.md 4.2).
		TEST_METHOD(Pe_ZFreezeDisablesTheDepthWrites)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);

			m.BpLoad(PE_COPY_CLEAR_Z_ID, 0xFFFFFF);
			m.BpLoad(PE_ZMODE_ID, 1u | (7u << 1) | (1u << 4));		// always, write enabled
			m.BpLoad(PE_CMODE0_ID, 0x18);
			m.BpLoad(GEN_MODE_ID, 1u << 19);						// zfreeze

			m.BeginFrame();

			GLboolean writeMask = 1;
			glGetBooleanv(GL_DEPTH_WRITEMASK, &writeMask);
			Assert::AreEqual<int>(0, writeMask ? 1 : 0, L"zfreeze must clear GL_DEPTH_WRITEMASK");

			float cleared = m.ReadDepthPixel(320, 240);
			Assert::AreEqual(1.0f, cleared, 0.01f, L"the frame clear must fill the depth");

			GFX::Vertex quad[4] = {
				GfxTestMachine::MakeVertex(-1, -1, 0.5f, 0xff, 0xff, 0xff, 0xff),
				GfxTestMachine::MakeVertex(1, -1, 0.5f, 0xff, 0xff, 0xff, 0xff),
				GfxTestMachine::MakeVertex(1, 1, 0.5f, 0xff, 0xff, 0xff, 0xff),
				GfxTestMachine::MakeVertex(-1, 1, 0.5f, 0xff, 0xff, 0xff, 0xff),
			};
			m.DrawQuad(quad);

			float depth = m.ReadDepthPixel(320, 240);
			Assert::AreEqual(1.0f, depth, 0.01f, L"zfreeze must keep the depth buffer frozen");
		}

		// =========================================================================================
		// The colour path
		// =========================================================================================

		TEST_METHOD(Pe_BlendAddsSourceAndDestination)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);

			m.BpLoad(PE_ZMODE_ID, 0);
			// blend on, sfactor = one (1), dfactor = one (1) -> src + dst
			m.BpLoad(PE_CMODE0_ID, 0x18 | 1u | (1u << 5) | (1u << 8));

			m.BeginFrame();

			// First draw fills the EFB with 0x40, the second one adds 0x30 to it
			DrawQuad(m, 0x40, 0x40, 0x40);
			DrawQuad(m, 0x30, 0x30, 0x30);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);
			Assert::AreEqual<int>(0x70, rgb[0], L"src + dst");
		}

		TEST_METHOD(Pe_BlendSubtractTakesTheDestinationMinusTheSource)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);

			m.BpLoad(PE_ZMODE_ID, 0);

			// The destination has to be filled with blending disabled: a subtract blend against the
			// cleared (black) buffer would clamp to zero.
			m.BpLoad(PE_CMODE0_ID, 0x18);

			m.BeginFrame();

			DrawQuad(m, 0x80, 0x80, 0x80);

			// blend on, blendop = subtract, both factors one -> dst - src
			m.BpLoad(PE_CMODE0_ID, 0x18 | 1u | (1u << 5) | (1u << 8) | (1u << 11));
			DrawQuad(m, 0x30, 0x30, 0x30);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);
			Assert::AreEqual<int>(0x50, rgb[0], L"dst - src");
		}

		// The mask bits are "update enabled" flags: GXSetColorUpdate/GXSetAlphaUpdate write their
		// argument straight into them.
		TEST_METHOD(Pe_ColorWriteMaskGatesTheColourWrite)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);

			m.BpLoad(PE_ZMODE_ID, 0);

			// The frame is cleared first: the clear is a copy operation and ignores the write mask
			// (that is checked on its own in Pe_TheFrameClearIgnoresTheColorWriteMask).
			m.BeginFrame();

			m.BpLoad(PE_CMODE0_ID, 0);					// no colour update
			DrawQuad(m, 0xff, 0xff, 0xff);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);
			Assert::AreEqual<int>(0, rgb[0], L"the colour write must stay disabled");

			m.BpLoad(PE_CMODE0_ID, 1u << 3);		// colour update enabled
			DrawQuad(m, 0xff, 0xff, 0xff);

			m.ReadColorPixel(320, 240, rgb);
			Assert::AreEqual<int>(0xff, rgb[0], L"the colour write must be enabled again");
		}

		// The frame clear is the copy engine's clear, so the write mask left behind by the previous
		// scene must not stop it (a hardware clear writes the EFB regardless of the colour mask).
		TEST_METHOD(Pe_TheFrameClearIgnoresTheColorWriteMask)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);

			m.BpLoad(PE_ZMODE_ID, 0);
			m.BpLoad(PE_COPY_CLEAR_AR_ID, 0x3f);		// red = 0x3f
			m.BpLoad(PE_COPY_CLEAR_GB_ID, 0x001f00);	// green = 0x1f, blue = 0

			// The colour update is left off, as a previous scene would leave it
			m.BpLoad(PE_CMODE0_ID, 0);

			m.BeginFrame();
			DrawQuad(m, 0xff, 0xff, 0xff);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);
			Assert::AreEqual<int>(0x3f, rgb[0], L"the frame clear must run despite the write mask");
			Assert::AreEqual<int>(0x1f, rgb[1], L"...");
		}

		TEST_METHOD(Pe_LogicOpReplacesTheBlend)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);

			m.BpLoad(PE_ZMODE_ID, 0);
			// logic op on, op = 0 (clear) -> every drawn pixel becomes black
			m.BpLoad(PE_CMODE0_ID, 0x18 | (1u << 1) | (0u << 12));

			m.BeginFrame();
			DrawQuad(m, 0xff, 0xff, 0xff);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);
			Assert::AreEqual<int>(0, rgb[0], L"the logic op must have cleared the pixel");
		}

		// =========================================================================================
		// The copy engine
		// =========================================================================================

		// PE_COPY_CMD with the clear bit set asks the copy engine to turn the quads it reads into
		// the clear colour while it reads them out (gfx-pe.md 5.1). A *texture* copy reads the
		// rectangle for the texture and clears the EFB with it, so the copies that follow it read
		// the cleared region - that is how a title that renders through the EFB gets each pass
		// isolated (the render-to-texture titles relied on it).
		//
		// The clear of a *display* copy is the one that has to wait: the backend shows the EFB in
		// place of the XFB the hardware would have written before the clear, so running it here
		// would erase the frame that is still to be shown (that is what turned the bootrom screen
		// black). The frame begin performs it instead, with the values this copy was programmed
		// with: the live registers cannot be used there, because by then the game may have
		// programmed them for its next copy, and Metroid Prime left the clear Z at 0 that way,
		// which made its LEQUAL depth test reject every draw (issue #349).
		TEST_METHOD(Pe_ACopyClearRunsWithTheCopyAndKeepsTheValuesItWasProgrammedWith)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);

			m.BpLoad(PE_ZMODE_ID, 0);
			m.BpLoad(PE_CMODE0_ID, 0x18);

			m.BeginFrame();
			DrawQuad(m, 0x80, 0x80, 0x80);

			// The clear colour of this copy, and the depth it must leave behind. The clear only
			// covers the rectangle the copy reads (gfx-pe.md 5.1), so the rectangle has to be
			// programmed for the whole frame to be cleared.
			m.BpLoad(PE_COPY_SRC_ADDR_ID, 0);
			m.BpLoad(PE_COPY_SRC_SIZE_ID, (m.gfx->RenderWidth() - 1) | ((m.gfx->RenderHeight() - 1) << 10));
			m.BpLoad(PE_COPY_CLEAR_AR_ID, 0xff);			// red = 0xff, alpha = 0
			m.BpLoad(PE_COPY_CLEAR_GB_ID, 0);				// blue = 0, green = 0
			m.BpLoad(PE_COPY_CLEAR_Z_ID, 0x800000);
			m.BpLoad(PE_COPY_CMD_ID, 1u << 11);				// clear

			// The clear ran with the copy: what the copy read out was the frame, and the EFB now
			// holds the clear colour.
			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);
			Assert::AreEqual<int>(0xff, rgb[0], L"the clear of a texture copy runs with the copy");

			// A display copy hands the rectangle over to the XFB the display shows and clears the
			// EFB rectangle it read in the same command (gfx-pe.md 5.1, 5.6). The clear uses the
			// values that copy was programmed with, not the live registers, which may already
			// belong to the next copy.
			m.BpLoad(PE_COPY_CLEAR_AR_ID, 0xff);
			m.BpLoad(PE_COPY_CLEAR_GB_ID, 0);
			m.BpLoad(PE_COPY_CLEAR_Z_ID, 0x800000);
			m.BpLoad(PE_COPY_CMD_ID, (1u << 11) | (GFX::PE_COPY_CMD_DISPLAY << 14));	// clear + display copy

			// Reprogram the clear registers, as a game does for its next copy.
			m.BpLoad(PE_COPY_CLEAR_AR_ID, 0);
			m.BpLoad(PE_COPY_CLEAR_GB_ID, 0);
			m.BpLoad(PE_COPY_CLEAR_Z_ID, 0);

			m.ReadColorPixel(320, 240, rgb);
			Assert::AreEqual<int>(0xff, rgb[0], L"the clear ran with the copy, with the colour it was programmed with");
			Assert::AreEqual<int>(0, rgb[1], L"...");
			Assert::AreEqual(0.5f, m.ReadDepthPixel(320, 240), 0.01f,
				L"the clear Z is the Z of that copy, not the one programmed later");
		}

		// PE_COPY_CMD.opcode = texture turns the EFB rectangle into a tiled texture in main memory
		// (gfx-pe.md 5.7): the destination holds the tiles of the format, the tile rows are a
		// `stride` apart, and the rectangle is taken in screen coordinates.
		TEST_METHOD(Pe_TextureCopyWritesTheRectangleAsATiledTexture)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);

			m.BpLoad(PE_ZMODE_ID, 0);
			m.BpLoad(PE_CMODE0_ID, 0x18);

			m.BeginFrame();

			// The upper half of the screen is red; the lower half keeps the colour the frame was
			// cleared with. A copy of row 0 therefore has to hold red and a copy of row 240 none.
			GFX::Vertex quad[4] = {
				GfxTestMachine::MakeVertex(-1, 0, 0, 0xff, 0, 0, 0xff),
				GfxTestMachine::MakeVertex(1, 0, 0, 0xff, 0, 0, 0xff),
				GfxTestMachine::MakeVertex(1, 1, 0, 0xff, 0, 0, 0xff),
				GfxTestMachine::MakeVertex(-1, 1, 0, 0xff, 0, 0, 0xff),
			};
			m.DrawQuad(quad);

			const uint32_t red = 0x2000, dark = 0x3000;

			// A 4x4 texel rectangle is one tile, and the tiles of the next row start four cache
			// lines further on.
			m.BpLoad(PE_COPY_DST_STRIDE_ID, 4);
			m.BpLoad(PE_COPY_SRC_SIZE_ID, 3 | (3u << 10));

			m.BpLoad(PE_COPY_SRC_ADDR_ID, 0);
			m.BpLoad(PE_COPY_DST_BASE0_ID, red >> 5);
			m.BpLoad(PE_COPY_CMD_ID, (uint32_t)GFX::TF_RGB565 << 4);

			m.BpLoad(PE_COPY_SRC_ADDR_ID, 240u << 10);
			m.BpLoad(PE_COPY_DST_BASE0_ID, dark >> 5);
			m.BpLoad(PE_COPY_CMD_ID, (uint32_t)GFX::TF_RGB565 << 4);

			const uint8_t* r = TestMainMemory(red, 32);
			const uint8_t* d = TestMainMemory(dark, 32);
			Assert::IsNotNull(r, L"the destination of the copy");
			Assert::IsNotNull(d, L"the destination of the copy");

			for (int texel = 0; texel < 16; texel++)
			{
				Assert::AreEqual<int>(0xf8, r[texel * 2], L"R5G6B5 is stored big endian: the red channel");
				Assert::AreEqual<int>(0x00, r[texel * 2 + 1], L"... and green with blue");
			}

			Assert::AreEqual<int>(0, d[0], L"the rectangle starts at the screen row it names");

			// The tile rows are a stride apart, so the cache lines that follow the tile are left
			// as they were.
			for (size_t i = 32; i < 32 * 4; i++)
			{
				Assert::AreEqual<int>(0, r[i], L"the row stride leaves the next cache lines alone");
			}
		}

		// PE_COPY_CMD.opcode decides what the copy engine does with the EFB rectangle (gfx-pe.md 5.6,
		// 5.7). A full-frame *display* copy writes the XFB that the video interface scans out, so it is
		// a point at which the frame becomes visible and the emulator has to swap the EFB it displays.
		// A *texture* copy is an intermediate render target, and a partial display copy is one pass of
		// a picture the title finishes with PE_FINISH (the bootrom writes the logo that way), so
		// neither of them presents: swapping there flickered the picture.
		//
		// This is the frame boundary the SDK THP movie player relies on: it draws a frame, copies it
		// to the XFB and waits for the retrace without ever calling GXDrawDone (PE_FINISH). Without
		// the swap on the full-frame display copy the Metroid Prime FMV stayed black (issue #349).
		TEST_METHOD(Pe_OnlyAFullFrameDisplayCopyPresentsTheFrame)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);

			m.BpLoad(PE_ZMODE_ID, 0);
			m.BpLoad(PE_CMODE0_ID, 0x18);

			m.BeginFrame();
			DrawQuad(m, 0xff, 0xff, 0xff);

			uint8_t rgb[3];
			m.ReadColorPixel(320, 240, rgb);
			Assert::AreEqual<int>(0xff, rgb[0], L"the quad must be in the EFB before the copy");

			size_t frames = m.gfx->pe->Frames();

			// The copy rectangle covers the whole render target, like the movie players' copy does.
			m.BpLoad(PE_COPY_SRC_ADDR_ID, 0);
			m.BpLoad(PE_COPY_SRC_SIZE_ID, (m.gfx->RenderWidth() - 1) | ((m.gfx->RenderHeight() - 1) << 10));

			GFX::PE_COPY_CMD copy{};

			// A texture copy leaves the frame on the EFB: no swap.
			copy.opcode = GFX::PE_COPY_CMD_TEXTURE;
			m.BpLoad(PE_COPY_CMD_ID, copy.bits);
			Assert::AreEqual<size_t>(frames, m.gfx->pe->Frames(), L"a texture copy must not present");

			// A partial display copy is one pass of a frame the title finishes with PE_FINISH.
			m.BpLoad(PE_COPY_SRC_SIZE_ID, (m.gfx->RenderWidth() / 2 - 1) | ((m.gfx->RenderHeight() / 2 - 1) << 10));
			copy.opcode = GFX::PE_COPY_CMD_DISPLAY;
			m.BpLoad(PE_COPY_CMD_ID, copy.bits);
			Assert::AreEqual<size_t>(frames, m.gfx->pe->Frames(), L"a partial display copy must not present");

			// Drawing into the frame arms the next present again, and the full-frame display copy
			// presents it: this is how the titles that never call GXDrawDone show their picture.
			DrawQuad(m, 0x40, 0x40, 0x40);
			m.BpLoad(PE_COPY_SRC_SIZE_ID, (m.gfx->RenderWidth() - 1) | ((m.gfx->RenderHeight() - 1) << 10));
			m.BpLoad(PE_COPY_CMD_ID, copy.bits);
			Assert::AreEqual<size_t>(frames + 1, m.gfx->pe->Frames(), L"a full-frame display copy must present");

			// A second copy of a frame that has not been drawn into again must not present: that is
			// what wiped the picture between frames before.
			m.gfx->GPFrameBegin();
			m.BpLoad(PE_COPY_CMD_ID, copy.bits);
			Assert::AreEqual<size_t>(frames + 1, m.gfx->pe->Frames(),
				L"a copy of a frame with no drawing must not present");
		}
	};
}
