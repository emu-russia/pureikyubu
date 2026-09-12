// Texture engine tests: the texture formats and the palette (TLUT) lookup.
//
// Every test builds a texture in the emulated main memory, programs the TX registers the way a GX
// title does, draws a quad that samples a known texel through the real pipeline and compares the
// pixel that comes out of the TEV with the value the hardware format definition produces.
//
// The expected values come from the Flipper format definitions (gfx-tc.md 5.1-5.4): an n-bit
// component is expanded to 8 bits by repeating its high bits, the 16- and 32-bit texels are stored
// big-endian, and a colour-index texture replaces the texel by a palette entry.
//
// The tile geometry of a format decides where a texel lives inside the 32-byte tile the decoder
// reads: 4-bit formats are 8x8 texels, 8-bit formats 4x8, 16-bit ones 4x4, and a 32-bit texel is
// split over two tiles (the "AR" tile and the "GB" tile). These tests only sample texel (0, 0),
// which is the first texel of the first tile in every one of those layouts.

#include "pch.h"
#include "gfx_test_common.h"

using namespace GfxUnitTest;

namespace pureikyubutest
{
	TEST_CLASS(GfxTextureTest)
	{
		static const uint32_t TextureAddr = 0x00100000;
		static const uint32_t TlutAddr = 0x00120000;

		static GfxTestMachine& M()
		{
			GfxTestMachine& m = Machine();
			m.Reset();
			return m;
		}

		static uint32_t ColorEnv(int seld, int clamp)
		{
			return (uint32_t)seld | (15u << 4) | (15u << 8) | (15u << 12) | ((uint32_t)clamp << 19);
		}

		static uint32_t AlphaEnv(int seld, int clamp)
		{
			return (uint32_t)seld << 4 | (7u << 7) | (7u << 10) | (7u << 13) | ((uint32_t)clamp << 19);
		}

		static void BE16(uint8_t* p, uint16_t value)
		{
			p[0] = (uint8_t)(value >> 8);
			p[1] = (uint8_t)value;
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
			m.XfLoad(GFX::XF_NUMTEX_ID, 0);

			m.BpLoad(PE_ZMODE_ID, 0);
			m.BpLoad(PE_CMODE0_ID, 0x18);
		}

		//! One TEV stage that outputs the sampled texel colour and alpha.
		static void SetupStage0(GfxTestMachine& m)
		{
			m.BpLoad(GEN_MODE_ID, 0);
			m.BpLoad(RAS1_TREF0_ID, 1u << 6);					// te0 = 1, ti0 = 0, tc0 = 0
			m.BpLoad(TEV_COLOR_ENV_0_ID, ColorEnv(8, 1));		// d = texel colour
			m.BpLoad(TEV_ALPHA_ENV_0_ID, AlphaEnv(4, 1));		// alpha = texel alpha
		}

		//! Program texture map 0: an image of `width` x `height` texels in the given format.
		static void SetupTexture(GfxTestMachine& m, int width, int height, GFX::TexFormat format,
			const uint8_t* raw, size_t rawSize)
		{
			WriteMainMemory(TextureAddr, raw, rawSize);

			m.BpLoad(TX_SETIMAGE0_I0_ID,
				(uint32_t)(width - 1) | ((uint32_t)(height - 1) << 10) | ((uint32_t)format << 20));
			m.BpLoad(TX_SETIMAGE3_I0_ID, TextureAddr >> 5);
			m.BpLoad(TX_SETMODE0_I0_ID, 0);						// nearest, clamp
			m.BpLoad(TX_SETMODE1_I0_ID, 0);
		}

		//! Draw a full-screen quad whose texture coordinate is the centre of texel (x, y).
		static void DrawTexel(GfxTestMachine& m, int texelsX, int texelsY, int x, int y, uint8_t rgb[3])
		{
			float u = ((float)x + 0.5f) / (float)texelsX;
			float v = ((float)y + 0.5f) / (float)texelsY;

			GFX::Vertex quad[4];
			for (int i = 0; i < 4; i++)
			{
				quad[i] = GfxTestMachine::MakeVertex(0, 0, 0, 0xff, 0xff, 0xff, 0xff);
				quad[i].TexCoord[0][0] = u;
				quad[i].TexCoord[0][1] = v;
			}

			quad[0].Position[0] = -1; quad[0].Position[1] = -1;
			quad[1].Position[0] = 1; quad[1].Position[1] = -1;
			quad[2].Position[0] = 1; quad[2].Position[1] = 1;
			quad[3].Position[0] = -1; quad[3].Position[1] = 1;

			m.BeginFrame();
			m.DrawQuad(quad);
			m.ReadColorPixel(320, 240, rgb);
		}

	public:

		// =========================================================================================
		// The component expansions
		// =========================================================================================

		// I8: the byte is the intensity of all four components.
		TEST_METHOD(Tex_I8SpreadsTheIntensityOverAllComponents)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			uint8_t raw[32] = { 0 };
			raw[0] = 0x80;

			SetupTexture(m, 4, 4, GFX::TF_I8, raw, sizeof(raw));

			uint8_t rgb[3];
			DrawTexel(m, 4, 4, 0, 0, rgb);

			Assert::AreEqual<int>(0x80, rgb[0], L"red");
			Assert::AreEqual<int>(0x80, rgb[1], L"green");
			Assert::AreEqual<int>(0x80, rgb[2], L"blue");
		}

		// I4: a 4-bit intensity expanded by repeating the nibble (0xA -> 0xAA).
		TEST_METHOD(Tex_I4ReplicatesTheNibble)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			uint8_t raw[32] = { 0 };
			raw[0] = 0xA0;					// the first texel is the high nibble

			SetupTexture(m, 8, 8, GFX::TF_I4, raw, sizeof(raw));

			uint8_t rgb[3];
			DrawTexel(m, 8, 8, 0, 0, rgb);

			Assert::AreEqual<int>(0xAA, rgb[0], L"red (0xA replicated)");
			Assert::AreEqual<int>(0xAA, rgb[1], L"green");
			Assert::AreEqual<int>(0xAA, rgb[2], L"blue");
		}

		// IA4: 4 bits of intensity in the high nibble and 4 bits of alpha in the low one.
		TEST_METHOD(Tex_IA4SplitsIntensityAndAlpha)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			uint8_t raw[32] = { 0 };
			raw[0] = 0xC5;					// A = 0xC, I = 0x5

			SetupTexture(m, 8, 4, GFX::TF_IA4, raw, sizeof(raw));

			uint8_t rgb[3];
			DrawTexel(m, 8, 4, 0, 0, rgb);

			Assert::AreEqual<int>(0x55, rgb[0], L"red (the low nibble 0x5 repeated)");
			Assert::AreEqual<int>(0x55, rgb[1], L"green");
			Assert::AreEqual<int>(0x55, rgb[2], L"blue");
		}

		// ... and the alpha comes from the high nibble: the alpha function keeps a pixel whose alpha
		// is 0xCC (0xC repeated) and drops one whose alpha is 0x55.
		TEST_METHOD(Tex_IA4AlphaComesFromTheHighNibble)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			// The alpha function: alpha > 0x80
			m.BpLoad(TEV_ALPHAFUNC_ID, 0x80 | (4u << 16) | (7u << 19));

			uint8_t highAlpha[32] = { 0 };
			highAlpha[0] = 0xC5;			// alpha = 0xCC

			SetupTexture(m, 8, 4, GFX::TF_IA4, highAlpha, sizeof(highAlpha));

			uint8_t rgb[3];
			DrawTexel(m, 8, 4, 0, 0, rgb);
			Assert::AreEqual<int>(0x55, rgb[0], L"alpha 0xCC passes the alpha test");

			uint8_t lowAlpha[32] = { 0 };
			lowAlpha[0] = 0x5C;				// alpha = 0x55

			SetupTexture(m, 8, 4, GFX::TF_IA4, lowAlpha, sizeof(lowAlpha));

			DrawTexel(m, 8, 4, 0, 0, rgb);
			Assert::AreEqual<int>(0x00, rgb[0], L"alpha 0x55 must be discarded");
		}

		// The 8x4 tiles of an IA4 image are stored row by row: every tile of the first tile row
		// comes first, then the tiles of the second one. With the tile columns as the outer loop
		// the decoder reads a column of tiles and any image wider than one tile comes out
		// transposed. Tag the first texel of each of the four tiles of a 16x8 image (the low
		// nibble is the intensity, the high one keeps the texel opaque) and read them back.
		TEST_METHOD(Tex_IA4ReadsTheTileGridInRowOrder)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			uint8_t raw[128] = { 0 };
			raw[0 * 32] = 0xF1;				// tile (0, 0)
			raw[1 * 32] = 0xF2;				// tile (1, 0), the other tile of the first tile row
			raw[2 * 32] = 0xF3;				// tile (0, 1), the first tile of the second tile row
			raw[3 * 32] = 0xF4;				// tile (1, 1)

			SetupTexture(m, 16, 8, GFX::TF_IA4, raw, sizeof(raw));

			uint8_t rgb[3];

			DrawTexel(m, 16, 8, 0, 0, rgb);
			Assert::AreEqual<int>(0x11, rgb[0], L"tile (0, 0)");

			DrawTexel(m, 16, 8, 8, 0, rgb);
			Assert::AreEqual<int>(0x22, rgb[0], L"tile (1, 0) follows it in memory");

			DrawTexel(m, 16, 8, 0, 4, rgb);
			Assert::AreEqual<int>(0x33, rgb[0], L"tile (0, 1) starts the second tile row");

			DrawTexel(m, 16, 8, 8, 4, rgb);
			Assert::AreEqual<int>(0x44, rgb[0], L"tile (1, 1)");
		}

		// The texels of the 8x8 tiles of a 4-bit image are laid out the same way. I4 has two
		// texels per byte (the high nibble first), so the tag goes into the high nibble.
		TEST_METHOD(Tex_I4ReadsTheTileGridInRowOrder)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			uint8_t raw[128] = { 0 };
			raw[0 * 32] = 0x10;				// tile (0, 0)
			raw[1 * 32] = 0x20;				// tile (1, 0)
			raw[2 * 32] = 0x30;				// tile (0, 1)
			raw[3 * 32] = 0x40;				// tile (1, 1)

			SetupTexture(m, 16, 16, GFX::TF_I4, raw, sizeof(raw));

			uint8_t rgb[3];

			DrawTexel(m, 16, 16, 0, 0, rgb);
			Assert::AreEqual<int>(0x11, rgb[0], L"tile (0, 0)");

			DrawTexel(m, 16, 16, 8, 0, rgb);
			Assert::AreEqual<int>(0x22, rgb[0], L"tile (1, 0)");

			DrawTexel(m, 16, 16, 0, 8, rgb);
			Assert::AreEqual<int>(0x33, rgb[0], L"tile (0, 1)");

			DrawTexel(m, 16, 16, 8, 8, rgb);
			Assert::AreEqual<int>(0x44, rgb[0], L"tile (1, 1)");
		}

		// IA8: 8 bits of intensity and 8 bits of alpha, intensity in the high byte.
		TEST_METHOD(Tex_IA8SplitsIntensityAndAlpha)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			uint8_t raw[32] = { 0 };
			// For IA8 the alpha is the high byte of the big-endian texel and the intensity the
			// low one (gfx-tc.md 5.1).
			BE16(&raw[0], 0xC040);			// A = 0xC0, I = 0x40

			SetupTexture(m, 4, 4, GFX::TF_IA8, raw, sizeof(raw));

			uint8_t rgb[3];
			DrawTexel(m, 4, 4, 0, 0, rgb);

			Assert::AreEqual<int>(0x40, rgb[0], L"red = intensity");
			Assert::AreEqual<int>(0x40, rgb[1], L"green = intensity");
			Assert::AreEqual<int>(0x40, rgb[2], L"blue = intensity");
		}

		// RGB565: 5 bits of red, 6 of green and 5 of blue, no alpha (opaque).
		TEST_METHOD(Tex_RGB565ExpandsFiveAndSixBitComponents)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			uint8_t raw[32] = { 0 };
			BE16(&raw[0], 0xF81F);			// R = 0x1F, G = 0, B = 0x1F

			SetupTexture(m, 4, 4, GFX::TF_RGB565, raw, sizeof(raw));

			uint8_t rgb[3];
			DrawTexel(m, 4, 4, 0, 0, rgb);

			Assert::AreEqual<int>(0xFF, rgb[0], L"red (0x1F replicated)");
			Assert::AreEqual<int>(0x00, rgb[1], L"green");
			Assert::AreEqual<int>(0xFF, rgb[2], L"blue");
		}

		// The 6-bit green of RGB565 replicates its top two bits: 0x21 -> 0x84.
		TEST_METHOD(Tex_RGB565GreenUsesTheSixBitReplication)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			uint8_t raw[32] = { 0 };
			BE16(&raw[0], 0x0420);			// G = 0x21

			SetupTexture(m, 4, 4, GFX::TF_RGB565, raw, sizeof(raw));

			uint8_t rgb[3];
			DrawTexel(m, 4, 4, 0, 0, rgb);

			Assert::AreEqual<int>(0x86, rgb[1], L"green = (0x21 << 2) | (0x21 >> 4)");
		}

		// RGB5A3 with the top bit set is RGB555 with an opaque alpha.
		TEST_METHOD(Tex_RGB5A3WithTheTopBitIsOpaqueRGB555)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			uint8_t raw[32] = { 0 };
			BE16(&raw[0], 0x8010);			// A-bit set, R = 0, G = 0x10 >> 5... R = 0, G = 0, B = 0x10

			SetupTexture(m, 4, 4, GFX::TF_RGB5A3, raw, sizeof(raw));

			uint8_t rgb[3];
			DrawTexel(m, 4, 4, 0, 0, rgb);

			Assert::AreEqual<int>(0x00, rgb[0], L"red");
			Assert::AreEqual<int>(0x00, rgb[1], L"green");
			Assert::AreEqual<int>(0x84, rgb[2], L"blue = 0x10 replicated");
		}

		// RGB5A3 with the top bit clear is RGBA4444: every component is 4 bits, and the alpha is
		// expanded the same way as the colour components (0xF -> 0xFF).
		TEST_METHOD(Tex_RGB5A3WithTheTopBitClearIsRGBA4444)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			// The alpha function: alpha > 0x80, so only the 0xF (0xFF) alpha passes
			m.BpLoad(TEV_ALPHAFUNC_ID, 0x80 | (4u << 16) | (7u << 19));

			uint8_t raw[32] = { 0 };
			BE16(&raw[0], 0x7F8F);			// A = 0x7 (the 3-bit maximum), R = 0xF, G = 0x8, B = 0xF

			SetupTexture(m, 4, 4, GFX::TF_RGB5A3, raw, sizeof(raw));

			uint8_t rgb[3];
			DrawTexel(m, 4, 4, 0, 0, rgb);

			Assert::AreEqual<int>(0xFF, rgb[0], L"red = 0xF replicated");
			Assert::AreEqual<int>(0x88, rgb[1], L"green = 0x8 replicated");
			Assert::AreEqual<int>(0xFF, rgb[2], L"blue = 0xF replicated");

			// The 4-bit alpha repeats its nibble, so 0x1 is 0x11 and the pixel is dropped
			uint8_t lowAlpha[32] = { 0 };
			BE16(&lowAlpha[0], 0x1F8F);		// A = 0x1 (3 bits)

			SetupTexture(m, 4, 4, GFX::TF_RGB5A3, lowAlpha, sizeof(lowAlpha));

			DrawTexel(m, 4, 4, 0, 0, rgb);
			Assert::AreEqual<int>(0x00, rgb[0], L"alpha 0x24 (the 3-bit 0x1 repeated) must be discarded");
		}

		// RGBA8: the red/alpha tile comes first in memory, the green/blue one after it.
		TEST_METHOD(Tex_RGBA8ReadsBothTiles)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			uint8_t raw[64] = { 0 };

			// The first tile holds alpha and red of every texel, the second one green and blue;
			// the byte order inside a tile is the one the decoder uses (alpha first).
			raw[0] = 0x22;					// alpha
			raw[1] = 0x11;					// red

			raw[32] = 0x33;					// green
			raw[33] = 0x44;					// blue

			SetupTexture(m, 4, 4, GFX::TF_RGBA8, raw, sizeof(raw));

			uint8_t rgb[3];
			DrawTexel(m, 4, 4, 0, 0, rgb);

			Assert::AreEqual<int>(0x11, rgb[0], L"red");
			Assert::AreEqual<int>(0x33, rgb[1], L"green");
			Assert::AreEqual<int>(0x44, rgb[2], L"blue");
		}

		// =========================================================================================
		// CMPR: a tile is four 4x4 sub-blocks of {col0, col1, sixteen 2-bit indices} (TL, TR, BL, BR).
		// With every index zero the whole tile decodes to the first endpoint, and the endpoint pair
		// here (red over blue) selects the opaque four-colour mode.
		TEST_METHOD(Tex_CmprDecodesTheFirstEndpoint)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			// The alpha function: only an opaque texel passes, so a wrong mode shows up as well
			m.BpLoad(TEV_ALPHAFUNC_ID, 0x80 | (4u << 16) | (7u << 19));

			uint8_t raw[32] = { 0 };
			BE16(&raw[0], 0xF800);			// col0 = red
			BE16(&raw[2], 0x001F);			// col1 = blue (0xF800 > 0x001F, so the opaque mode)

			SetupTexture(m, 8, 8, GFX::TF_CMPR, raw, sizeof(raw));

			uint8_t rgb[3];
			DrawTexel(m, 8, 8, 0, 0, rgb);

			Assert::AreEqual<int>(0xFF, rgb[0], L"red (the first endpoint)");
			Assert::AreEqual<int>(0x00, rgb[1], L"green");
			Assert::AreEqual<int>(0x00, rgb[2], L"blue");
		}

		// The other CMPR mode: with col0 <= col1 the two endpoints and their average are opaque, and
		// the fourth colour is the transparent texel. The indices of the first row live in the byte
		// that follows the endpoints, packed from the high bits down.
		TEST_METHOD(Tex_CmprThreeColourModeEndsWithATransparentTexel)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			uint8_t raw[32] = { 0 };
			BE16(&raw[0], 0x001F);			// col0 = blue
			BE16(&raw[2], 0xF800);			// col1 = red (0x001F < 0xF800, so the 3-colour mode)
			raw[4] = (0u << 6) | (1u << 4) | (2u << 2) | 3u;	// col0, col1, average, transparent

			SetupTexture(m, 8, 8, GFX::TF_CMPR, raw, sizeof(raw));

			uint8_t rgb[3];

			DrawTexel(m, 8, 8, 0, 0, rgb);
			Assert::AreEqual<int>(0x00, rgb[0], L"texel 0 is the first endpoint");
			Assert::AreEqual<int>(0xFF, rgb[2], L"...");

			DrawTexel(m, 8, 8, 1, 0, rgb);
			Assert::AreEqual<int>(0xFF, rgb[0], L"texel 1 is the second endpoint");
			Assert::AreEqual<int>(0x00, rgb[2], L"...");

			DrawTexel(m, 8, 8, 2, 0, rgb);
			Assert::AreEqual<int>(0x7F, rgb[0], L"texel 2 is the average of the endpoints");
			Assert::AreEqual<int>(0x7F, rgb[2], L"...");

			// The transparent texel: only an opaque texel passes the alpha function, so a transparent
			// one leaves the pixel at the clear colour (the default, always-passing alpha function is
			// replaced here on purpose).
			m.BpLoad(PE_COPY_CLEAR_AR_ID, 0x00000000);		// red = 0, alpha = 0
			m.BpLoad(PE_COPY_CLEAR_GB_ID, 0x0000FF00);		// green = 0xff, blue = 0
			m.BpLoad(TEV_ALPHAFUNC_ID, 0x01u | (4u << 16) | (7u << 19));	// alpha > 1

			DrawTexel(m, 8, 8, 3, 0, rgb);
			Assert::AreEqual<int>(0x00, rgb[0], L"the transparent texel must not be drawn at all");
			Assert::AreEqual<int>(0xFF, rgb[1], L"the pixel keeps the clear colour");
		}

		// Program one texture map: `width` x `height` texels of `format` at `addr`.
		static void SetupMap(GfxTestMachine& m, int map, uint32_t addr, int width, int height,
			GFX::TexFormat format, const uint8_t* raw, size_t rawSize)
		{
			// The I0-I3 and the I4-I7 blocks are laid out the same way: seven registers per map.
			static const uint32_t img0[] = { TX_SETIMAGE0_I0_ID, TX_SETIMAGE0_I1_ID, TX_SETIMAGE0_I2_ID,
				TX_SETIMAGE0_I3_ID, TX_SETIMAGE0_I4_ID, TX_SETIMAGE0_I5_ID, TX_SETIMAGE0_I6_ID,
				TX_SETIMAGE0_I7_ID };
			static const uint32_t img3[] = { TX_SETIMAGE3_I0_ID, TX_SETIMAGE3_I1_ID, TX_SETIMAGE3_I2_ID,
				TX_SETIMAGE3_I3_ID, TX_SETIMAGE3_I4_ID, TX_SETIMAGE3_I5_ID, TX_SETIMAGE3_I6_ID,
				TX_SETIMAGE3_I7_ID };
			static const uint32_t mode0[] = { TX_SETMODE0_I0_ID, TX_SETMODE0_I1_ID, TX_SETMODE0_I2_ID,
				TX_SETMODE0_I3_ID, TX_SETMODE0_I4_ID, TX_SETMODE0_I5_ID, TX_SETMODE0_I6_ID,
				TX_SETMODE0_I7_ID };
			static const uint32_t mode1[] = { TX_SETMODE1_I0_ID, TX_SETMODE1_I1_ID, TX_SETMODE1_I2_ID,
				TX_SETMODE1_I3_ID, TX_SETMODE1_I4_ID, TX_SETMODE1_I5_ID, TX_SETMODE1_I6_ID,
				TX_SETMODE1_I7_ID };

			WriteMainMemory(addr, raw, rawSize);

			m.BpLoad(mode0[map], 0);							// nearest, clamp
			m.BpLoad(mode1[map], 0);
			m.BpLoad(img0[map],
				(uint32_t)(width - 1) | ((uint32_t)(height - 1) << 10) | ((uint32_t)format << 20));
			m.BpLoad(img3[map], addr >> 5);
		}

		// The map a stage samples is the RAS1_TREF `ti` field of the stage: two maps programmed with
		// different textures must produce different texels when only that field changes.
		TEST_METHOD(Tex_AStageSamplesTheMapItsTrefSelects)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			// Two 4x4 RGBA8 textures: map 0 is red, map 1 is blue. The first 32-byte tile holds
			// alpha and red of every texel, the second one green and blue (gfx-tc.md 5.1).
			uint8_t red[64] = { 0 }, blue[64] = { 0 };
			for (int i = 0; i < 16; i++)
			{
				red[2 * i] = 0xFF; red[2 * i + 1] = 0xFF;		// alpha, red
				red[32 + 2 * i] = 0x00; red[32 + 2 * i + 1] = 0x00;	// green, blue
				blue[2 * i] = 0xFF; blue[2 * i + 1] = 0x00;		// alpha, red
				blue[32 + 2 * i] = 0x00; blue[32 + 2 * i + 1] = 0xFF;	// green, blue
			}

			SetupMap(m, 0, TextureAddr, 4, 4, GFX::TF_RGBA8, red, sizeof(red));
			SetupMap(m, 1, TextureAddr + 0x10000, 4, 4, GFX::TF_RGBA8, blue, sizeof(blue));

			uint8_t rgb[3];

			m.BpLoad(RAS1_TREF0_ID, (1u << 6) | 0);				// te0 = 1, ti0 = 0, tc0 = 0
			DrawTexel(m, 4, 4, 0, 0, rgb);
			Assert::AreEqual<int>(0xFF, rgb[0], L"map 0 is the red texture");
			Assert::AreEqual<int>(0x00, rgb[2], L"...");

			m.BpLoad(RAS1_TREF0_ID, (1u << 6) | 1);				// te0 = 1, ti0 = 1, tc0 = 0
			DrawTexel(m, 4, 4, 0, 0, rgb);
			Assert::AreEqual<int>(0x00, rgb[0], L"map 1 is the blue texture");
			Assert::AreEqual<int>(0xFF, rgb[2], L"...");

			// ... and a map of the second block (I4-I7) has to work the same way.
			SetupMap(m, 5, TextureAddr + 0x20000, 4, 4, GFX::TF_RGBA8, red, sizeof(red));

			m.BpLoad(RAS1_TREF0_ID, (1u << 6) | 5);				// te0 = 1, ti0 = 5, tc0 = 0
			DrawTexel(m, 4, 4, 0, 0, rgb);
			Assert::AreEqual<int>(0xFF, rgb[0], L"map 5 is the red texture");
			Assert::AreEqual<int>(0x00, rgb[2], L"...");
		}
		// The palette (TLUT) path
		// =========================================================================================

		//! Load `entries` RGB565 palette entries at the start of the TLUT and bind it to map 0.
		static void SetupRgb565Tlut(GfxTestMachine& m, const uint16_t* entries, size_t count)
		{
			std::vector<uint8_t> raw(count * 2);
			for (size_t i = 0; i < count; i++)
			{
				BE16(&raw[i * 2], entries[i]);
			}

			WriteMainMemory(TlutAddr, raw.data(), raw.size());

			// TX_LOADTLUT0: the palette base address in bits 25:5
			m.BpLoad(TX_LOADTLUT0_ID, TlutAddr >> 5);

			// TX_LOADTLUT1: the TMEM offset (bits 9:0) and the entry count (bits 20:10)
			m.BpLoad(TX_LOADTLUT1_ID, 0u | ((uint32_t)(count - 1) << 10));

			// TX_SETTLUT: the palette is at TMEM offset 0 and holds RGB565 entries (format 1)
			m.BpLoad(TX_SETTLUT_I0_ID, 0u | ((uint32_t)GFX::TLUT_RGB565 << 10));
		}

		// A C8 texture takes its colour from the palette: index 1 must give the second entry.
		TEST_METHOD(Tex_C8UsesTheLoadedPalette)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			const uint16_t palette[4] = { 0x001F, 0xF800, 0x07E0, 0xFFFF };	// blue, red, green, white
			SetupRgb565Tlut(m, palette, 4);

			uint8_t raw[32] = { 0 };
			raw[0] = 1;						// texel (0, 0) uses palette entry 1 (red)

			SetupTexture(m, 4, 4, GFX::TF_C8, raw, sizeof(raw));

			uint8_t rgb[3];
			DrawTexel(m, 4, 4, 0, 0, rgb);

			Assert::AreEqual<int>(0xFF, rgb[0], L"red from the palette");
			Assert::AreEqual<int>(0x00, rgb[1], L"green");
			Assert::AreEqual<int>(0x00, rgb[2], L"blue");
		}

		// The palette entries must not be confused with each other: another index gives another
		// colour.
		TEST_METHOD(Tex_C8SelectsThePaletteEntryByIndex)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			const uint16_t palette[4] = { 0x001F, 0xF800, 0x07E0, 0xFFFF };
			SetupRgb565Tlut(m, palette, 4);

			uint8_t raw[32] = { 0 };
			raw[3] = 3;						// texel (3, 0) uses palette entry 3 (white)

			SetupTexture(m, 4, 4, GFX::TF_C8, raw, sizeof(raw));

			uint8_t rgb[3];
			DrawTexel(m, 4, 4, 3, 0, rgb);

			Assert::AreEqual<int>(0xFF, rgb[0], L"red from the fourth palette entry");
			Assert::AreEqual<int>(0xFF, rgb[1], L"green");
			Assert::AreEqual<int>(0xFF, rgb[2], L"blue");
		}

		// A C4 texture packs two indices per byte; the first texel is the high nibble.
		TEST_METHOD(Tex_C4UsesTheHighNibbleForTheFirstTexel)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			const uint16_t palette[4] = { 0x001F, 0xF800, 0x07E0, 0xFFFF };
			SetupRgb565Tlut(m, palette, 4);

			uint8_t raw[32] = { 0 };
			raw[0] = 0x20;					// texel 0 = index 2 (green), texel 1 = index 0

			SetupTexture(m, 8, 8, GFX::TF_C4, raw, sizeof(raw));

			uint8_t rgb[3];
			DrawTexel(m, 8, 8, 0, 0, rgb);

			Assert::AreEqual<int>(0x00, rgb[0], L"red");
			Assert::AreEqual<int>(0xFF, rgb[1], L"green from palette entry 2");
			Assert::AreEqual<int>(0x00, rgb[2], L"blue");
		}

		// =========================================================================================
		// Decoding on demand: what invalidates a decoded image, and what the debugger gets back
		// =========================================================================================

		// The debugger's texture dump (the `gxtexdump` command) must hand the colour channels back
		// in R,G,B order. The decoder serialises every texel into the byte order the GL upload
		// wants (R,G,B,A), which is the reverse of the Color field order, so a dump that read the
		// union fields used to rotate the channels of every non-grey format.
		TEST_METHOD(Tex_DumpReturnsTheColourChannelsInOrder)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			uint8_t raw[64] = { 0 };
			raw[0] = 0xAA;					// alpha
			raw[1] = 0x11;					// red
			raw[32] = 0x22;					// green
			raw[33] = 0x33;					// blue

			SetupTexture(m, 4, 4, GFX::TF_RGBA8, raw, sizeof(raw));

			std::vector<uint8_t> rgb;
			int width = 0, height = 0;
			Assert::IsTrue(m.gfx->tx->DumpTexture(0, rgb, &width, &height));

			Assert::AreEqual<int>(4, width, L"width");
			Assert::AreEqual<int>(4, height, L"height");
			Assert::AreEqual<int>(0x11, rgb[0], L"red");
			Assert::AreEqual<int>(0x22, rgb[1], L"green");
			Assert::AreEqual<int>(0x33, rgb[2], L"blue");
		}

		// GXLoadTexObj programs the whole map on every draw, so the draw path asks for a decode all
		// the time; the decoder keeps the image it already has when the description *and* the
		// texture bytes are the same, but a title that edits the texels in place (and programs the
		// same map again) must still get the new bytes. Rewriting the same registers with other
		// data at the same address is exactly that case.
		TEST_METHOD(Tex_AnInPlaceEditOfTheTexelsIsPickedUp)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			uint8_t raw[32] = { 0 };
			raw[0] = 0x20;

			SetupTexture(m, 4, 4, GFX::TF_I8, raw, sizeof(raw));

			uint8_t rgb[3];
			DrawTexel(m, 4, 4, 0, 0, rgb);
			Assert::AreEqual<int>(0x20, rgb[0], L"the first image");

			// The same map (same address, format and size) with other texels behind it
			raw[0] = 0x80;
			SetupTexture(m, 4, 4, GFX::TF_I8, raw, sizeof(raw));

			DrawTexel(m, 4, 4, 0, 0, rgb);
			Assert::AreEqual<int>(0x80, rgb[0], L"the edited texel");
		}

		// The same holds for a palette: rebinding the same TLUT after its entries changed has to
		// re-expand the indices, even though the texture bytes did not move.
		TEST_METHOD(Tex_AReloadedPaletteIsPickedUp)
		{
			RequireGL();
			GfxTestMachine& m = M();
			SetupPassThrough(m);
			SetupStage0(m);

			const uint16_t red[4] = { 0xF800, 0xF800, 0xF800, 0xF800 };
			SetupRgb565Tlut(m, red, 4);

			uint8_t raw[32] = { 0 };
			raw[0] = 0;						// texel (0, 0) uses the first entry

			SetupTexture(m, 4, 4, GFX::TF_C8, raw, sizeof(raw));

			uint8_t rgb[3];
			DrawTexel(m, 4, 4, 0, 0, rgb);
			Assert::AreEqual<int>(0xFF, rgb[0], L"red from the first palette");
			Assert::AreEqual<int>(0x00, rgb[2], L"...");

			// Load another palette into the same TMEM slot and bind it again
			const uint16_t blue[4] = { 0x001F, 0x001F, 0x001F, 0x001F };
			SetupRgb565Tlut(m, blue, 4);

			DrawTexel(m, 4, 4, 0, 0, rgb);
			Assert::AreEqual<int>(0x00, rgb[0], L"the texture bytes did not change");
			Assert::AreEqual<int>(0xFF, rgb[2], L"the new palette entries appear");
		}
	};
}
