// Rendered-image galleries for the texture engine (TX) and the palette (TLUT).
//
// Every test in this file walks a table of texture configurations, renders one full-EFB picture per
// configuration and publishes it into the HTML report. The pictures all use the same three test
// patterns (a swatch, a detail field and an index ramp), so what changes from image to image is
// exactly the configuration under test: the texel format, the palette format, the sampler filters,
// the wrap modes or the CMPR block encoding.
//
// The texel formats are encoded here the way src/tx.cpp decodes them (the tile geometry of a format
// decides the byte order), which is the same thing gfx_texture_test.cpp does for single texels - only
// here a whole image is built, so the picture shows the format instead of one sampled value.
//
// See specs: gfx-tc.md 5.1-5.4 (formats, tile geometry) and 4.2 (the TX registers).

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

		//! Where the gallery textures and palettes live in the emulated main memory.
		const uint32_t TexAddr = 0x00080000;
		const uint32_t TlutAddr = 0x00200000;

		// ---------------------------------------------------------------------------------------
		// The test patterns. All of them are written in texture space with v = 0 at the *bottom*:
		// the picture of a full-screen quad maps texture row 0 to the bottom of the EFB, so what the
		// description of an image says matches what the image shows.
		// ---------------------------------------------------------------------------------------

		using TexelFunc = Rgba(*)(int x, int y, int w, int h);

		//! The standard swatch: colour bars, a one-texel checkerboard, a two-axis gradient and a
		//! sixteen step wedge.
		Rgba SwatchTexel(int x, int y, int w, int h)
		{
			int halfW = w / 2, halfH = h / 2;
			bool top = y >= halfH;
			bool right = x >= halfW;

			if (!top && !right)
			{
				int bar = (x * 8) / halfW;
				return GridColor(bar);
			}

			if (!top && right)
			{
				// One-texel checkerboard: the sampler is nearest, so it shows the texel grid
				bool white = ((x + y) & 1) != 0;
				int value = white ? 0xE0 : 0x20;
				return MakeRgba(value, value, value);
			}

			if (top && !right)
			{
				// Two-axis gradient, with a vertical alpha ramp (visible when the texel is blended)
				int r = (x * 255) / (halfW - 1);
				int g = ((y - halfH) * 255) / (halfH - 1);
				int b = 255 - r / 2 - g / 2;
				int a = 32 + g * 220 / 255;
				return MakeRgba(r, g, b < 0 ? 0 : b, a);
			}

			// Sixteen step wedge: the quantisation of the format shows as the banding
			int step = ((x - halfW) * 16) / halfW;
			int value = step * 255 / 15;
			return MakeRgba(value, 255 - value, (value * 3) & 0xFF);
		}

		//! A high frequency field (a two-texel checkerboard over a gradient) for the filter gallery:
		//! minification samples it at a much lower rate, so the filters differ visibly.
		Rgba DetailTexel(int x, int y, int w, int h)
		{
			bool white = (((x >> 1) + (y >> 1)) & 1) != 0;
			int r = (x * 255) / (w - 1);
			int g = (y * 255) / (h - 1);

			return MakeRgba(white ? r : 0x10, white ? g : 0x10, white ? (255 - r) : 0x90);
		}

		//! A coarse pattern the wrap galleries sample outside [0, 1]: four differently coloured
		//! quadrants over a two-axis brightness ramp with a bright border, so a seam, a mirror or a
		//! stretched edge is easy to spot - and the clamped picture, whose margin is the stretched
		//! border alone, is still a picture rather than a flat colour.
		Rgba FrameTexel(int x, int y, int w, int h)
		{
			int halfW = w / 2, halfH = h / 2;

			if (x < 2 || y < 2 || x >= w - 2 || y >= h - 2)
				return MakeRgba(0xFF, 0xFF, 0xFF);			// the frame

			int quadrant = ((y >= halfH) ? 2 : 0) + ((x >= halfW) ? 1 : 0);
			static const Rgba colors[4] = {
				{ 0xC0, 0x20, 0x20, 0xFF },
				{ 0x20, 0xC0, 0x20, 0xFF },
				{ 0x20, 0x20, 0xC0, 0xFF },
				{ 0xC0, 0xC0, 0x20, 0xFF },
			};

			// The quadrant colour, shaded by a two-axis ramp (96..224) inside the quadrant
			Rgba c = colors[quadrant];
			int ramp = 96 + ((x * 128) / (w - 1) + (y * 128) / (h - 1)) / 2;

			return MakeRgba(c.R * ramp / 255, c.G * ramp / 255, c.B * ramp / 255);
		}

		//! The palette index of the paletted galleries: an entry ramp, entry bars, a checkerboard of
		//! the first and the last entry, and a sixteen step wedge.
		int SwatchIndex(int x, int y, int w, int h, int entries)
		{
			int halfW = w / 2, halfH = h / 2;
			bool top = y >= halfH;
			bool right = x >= halfW;

			if (!top && !right)
			{
				int bar = (x * 8) / halfW;
				return bar * (entries - 1) / 7;
			}

			if (!top && right)
				return (((x + y) & 1) != 0) ? (entries - 1) : 0;

			if (top && !right)
				return (x * (entries - 1)) / (halfW - 1);

			int step = ((x - halfW) * 16) / halfW;
			return step * (entries - 1) / 15;
		}

		// ---------------------------------------------------------------------------------------
		// The encoders (one per format, mirroring the tile walk of tx.cpp DecodeTexture)
		// ---------------------------------------------------------------------------------------

		int Luma(const Rgba& c)
		{
			return ((int)c.R * 77 + (int)c.G * 151 + (int)c.B * 28) >> 8;
		}

		uint16_t Encode565(const Rgba& c)
		{
			return (uint16_t)(((c.R >> 3) << 11) | ((c.G >> 2) << 5) | (c.B >> 3));
		}

		//! RGB5A3: the top bit set selects the opaque RGB555 form, clear the RGBA4444 form.
		uint16_t EncodeRgb5a3(const Rgba& c, bool forceRgba4444 = false)
		{
			if (!forceRgba4444 && c.A >= 0xE0)
				return (uint16_t)(0x8000 | ((c.R >> 3) << 10) | ((c.G >> 3) << 5) | (c.B >> 3));

			return (uint16_t)(((c.A >> 5) << 12) | ((c.R >> 4) << 8) | ((c.G >> 4) << 4) | (c.B >> 4));
		}

		//! TLUT IA8: the decoder takes the alpha from the high byte and the intensity from the low one
		//! (tx.cpp GetTlutCol reads the palette word little-endian out of the memory image).
		uint16_t EncodeIa8(const Rgba& c)
		{
			return (uint16_t)(((uint32_t)c.A << 8) | (uint32_t)Luma(c));
		}

		void PushBe16(std::vector<uint8_t>& raw, uint16_t value)
		{
			raw.push_back((uint8_t)(value >> 8));
			raw.push_back((uint8_t)value);
		}

		//! One CMPR sub-block (4x4 texels, eight bytes). The decoder derives the two interpolated
		//! colours from the packed endpoints, so the encoder does the same and then picks, per texel,
		//! the palette entry that is closest to the wanted colour.
		void EncodeCmprBlock(const Rgba texels[4][4], bool threeColourMode, std::vector<uint8_t>& raw,
			bool fixedEndpoints = false, uint16_t fixed0 = 0, uint16_t fixed1 = 0)
		{
			Rgba e0 = texels[0][0], e1 = texels[3][3];

			if (fixedEndpoints)
			{
				e0 = MakeRgba(((fixed0 >> 11) & 0x1F) * 255 / 31, ((fixed0 >> 5) & 0x3F) * 255 / 63, (fixed0 & 0x1F) * 255 / 31);
				e1 = MakeRgba(((fixed1 >> 11) & 0x1F) * 255 / 31, ((fixed1 >> 5) & 0x3F) * 255 / 63, (fixed1 & 0x1F) * 255 / 31);
			}
			else
			{
				// The brightest and the darkest texel of the block are the endpoints
				int bright = -1, dark = 999;
				for (int v = 0; v < 4; v++)
					for (int u = 0; u < 4; u++)
					{
						int l = Luma(texels[v][u]);
						if (l > bright) { bright = l; e0 = texels[v][u]; }
						if (l < dark) { dark = l; e1 = texels[v][u]; }
					}
			}

			uint16_t p0 = Encode565(e0);
			uint16_t p1 = Encode565(e1);

			// The decoder selects the mode by comparing the packed words: col0 > col1 is the
			// four-colour mode, col0 < col1 the three-colour one
			if (threeColourMode)
			{
				if (p0 > p1) { uint16_t t = p0; p0 = p1; p1 = t; }
				if (p0 == p1) p1 = (uint16_t)(p0 > 0 ? p0 - 1 : p0 + 1);
			}
			else
			{
				if (p0 < p1) { uint16_t t = p0; p0 = p1; p1 = t; }
				if (p0 == p1) p0 = (uint16_t)(p1 < 0xFFFF ? p1 + 1 : p1 - 1);
			}

			// The palette the decoder will build for these endpoints (RGB only: the alpha of the two
			// interpolated entries is not part of the format)
			Rgba c0 = MakeRgba(((p0 >> 11) & 0x1F) * 255 / 31, ((p0 >> 5) & 0x3F) * 255 / 63, (p0 & 0x1F) * 255 / 31);
			Rgba c1 = MakeRgba(((p1 >> 11) & 0x1F) * 255 / 31, ((p1 >> 5) & 0x3F) * 255 / 63, (p1 & 0x1F) * 255 / 31);

			Rgba palette[4] = { c0, c1, {}, {} };

			if (threeColourMode)
			{
				palette[2] = MakeRgba((c0.R + c1.R) / 2, (c0.G + c1.G) / 2, (c0.B + c1.B) / 2);
				palette[3] = MakeRgba(0, 0, 0, 0);		// the fourth entry is the transparent texel
			}
			else
			{
				palette[2] = MakeRgba((2 * c0.R + c1.R) / 3, (2 * c0.G + c1.G) / 3, (2 * c0.B + c1.B) / 3);
				palette[3] = MakeRgba((2 * c1.R + c0.R) / 3, (2 * c1.G + c0.G) / 3, (2 * c1.B + c0.B) / 3);
			}

			PushBe16(raw, p0);
			PushBe16(raw, p1);

			for (int v = 0; v < 4; v++)
			{
				uint8_t row = 0;

				for (int u = 0; u < 4; u++)
				{
					int best = 0, bestDist = 1 << 30;

					for (int i = 0; i < 4; i++)
					{
						int dr = (int)texels[v][u].R - (int)palette[i].R;
						int dg = (int)texels[v][u].G - (int)palette[i].G;
						int db = (int)texels[v][u].B - (int)palette[i].B;
						int dist = dr * dr + dg * dg + db * db;

						if (dist < bestDist) { bestDist = dist; best = i; }
					}

					row = (uint8_t)(row | (best << (6 - u * 2)));
				}

				raw.push_back(row);
			}
		}

		std::vector<uint8_t> EncodeCmpr(int w, int h, TexelFunc texel, bool threeColourMode,
			bool fixedEndpoints = false)
		{
			std::vector<uint8_t> raw;

			// Fixed endpoints of the sub-block gallery: red/blue, green/yellow, white/black, cyan/magenta
			static const uint16_t fixed[4][2] = {
				{ 0xF800, 0x001F },
				{ 0x07E0, 0xFFE0 },
				{ 0xFFFF, 0x0000 },
				{ 0x07FF, 0xF81F },
			};

			for (int t = 0; t < h; t += 8)
			{
				for (int s = 0; s < w; s += 8)
				{
					// The four sub-blocks of a tile, in the order the decoder walks them
					static const int origin[4][2] = { { 0, 0 }, { 4, 0 }, { 0, 4 }, { 4, 4 } };

					for (int b = 0; b < 4; b++)
					{
						Rgba block[4][4];

						for (int v = 0; v < 4; v++)
							for (int u = 0; u < 4; u++)
								block[v][u] = texel(s + origin[b][0] + u, t + origin[b][1] + v, w, h);

						// The sub-block gallery uses one endpoint pair per sub-block of the texture
						int which = ((t / 4) * (w / 4) + (s / 4)) & 3;

						EncodeCmprBlock(block, threeColourMode, raw, fixedEndpoints,
							fixedEndpoints ? fixed[which][0] : 0,
							fixedEndpoints ? fixed[which][1] : 0);
					}
				}
			}

			return raw;
		}

		//! The raw bytes of a texture of `format`, encoded from `texel`.
		std::vector<uint8_t> EncodeTexture(GFX::TexFormat format, int w, int h, TexelFunc texel)
		{
			std::vector<uint8_t> raw;
			raw.reserve((size_t)w * h * 4);

			switch (format)
			{
				case GFX::TF_I4:
					for (int t = 0; t < h; t += 8)
						for (int s = 0; s < w; s += 8)
							for (int v = 0; v < 8; v++)
								for (int u = 0; u < 8; u += 2)
								{
									uint8_t hi = (uint8_t)(Luma(texel(s + u, t + v, w, h)) >> 4);
									uint8_t lo = (uint8_t)(Luma(texel(s + u + 1, t + v, w, h)) >> 4);
									raw.push_back((uint8_t)((hi << 4) | lo));
								}
					break;

				case GFX::TF_I8:
					for (int t = 0; t < h; t += 4)
						for (int s = 0; s < w; s += 8)
							for (int v = 0; v < 4; v++)
								for (int u = 0; u < 8; u++)
									raw.push_back((uint8_t)Luma(texel(s + u, t + v, w, h)));
					break;

				case GFX::TF_IA4:
					// Tile rows first, like the decoder: the tiles of a texture are stored row by
					// row, so the encoder must not mirror a column-first walk here (doing that hid
					// the transposed decode the gallery was supposed to show).
					for (int t = 0; t < h; t += 4)
						for (int s = 0; s < w; s += 8)
							for (int v = 0; v < 4; v++)
								for (int u = 0; u < 8; u++)
								{
									Rgba c = texel(s + u, t + v, w, h);
									raw.push_back((uint8_t)((Luma(c) >> 4) | ((c.A >> 4) << 4)));
								}
					break;

				case GFX::TF_IA8:
					for (int t = 0; t < h; t += 4)
						for (int s = 0; s < w; s += 4)
							for (int v = 0; v < 4; v++)
								for (int u = 0; u < 4; u++)
								{
									Rgba c = texel(s + u, t + v, w, h);
									raw.push_back(c.A);				// the alpha is the high byte
									raw.push_back((uint8_t)Luma(c));
								}
					break;

				case GFX::TF_RGB565:
					for (int t = 0; t < h; t += 4)
						for (int s = 0; s < w; s += 4)
							for (int v = 0; v < 4; v++)
								for (int u = 0; u < 4; u++)
									PushBe16(raw, Encode565(texel(s + u, t + v, w, h)));
					break;

				case GFX::TF_RGB5A3:
					for (int t = 0; t < h; t += 4)
						for (int s = 0; s < w; s += 4)
							for (int v = 0; v < 4; v++)
								for (int u = 0; u < 4; u++)
									PushBe16(raw, EncodeRgb5a3(texel(s + u, t + v, w, h)));
					break;

				case GFX::TF_RGBA8:
					// Per tile: the alpha/red pairs first, then the green/blue pairs
					for (int t = 0; t < h; t += 4)
						for (int s = 0; s < w; s += 4)
						{
							for (int v = 0; v < 4; v++)
								for (int u = 0; u < 4; u++)
								{
									Rgba c = texel(s + u, t + v, w, h);
									raw.push_back(c.A);
									raw.push_back(c.R);
								}

							for (int v = 0; v < 4; v++)
								for (int u = 0; u < 4; u++)
								{
									Rgba c = texel(s + u, t + v, w, h);
									raw.push_back(c.G);
									raw.push_back(c.B);
								}
						}
					break;

				case GFX::TF_CMPR:
					raw = EncodeCmpr(w, h, texel, false);
					break;

				default:
					Assert::Fail(L"EncodeTexture: this format has its own encoder");
					break;
			}

			return raw;
		}

		//! The raw bytes of a paletted texture: the index pattern, encoded in the format's index size.
		std::vector<uint8_t> EncodePaletted(GFX::TexFormat format, int w, int h, int entries)
		{
			std::vector<uint8_t> raw;

			switch (format)
			{
				case GFX::TF_C4:
					for (int t = 0; t < h; t += 8)
						for (int s = 0; s < w; s += 8)
							for (int v = 0; v < 8; v++)
								for (int u = 0; u < 8; u += 2)
								{
									uint8_t hi = (uint8_t)SwatchIndex(s + u, t + v, w, h, entries);
									uint8_t lo = (uint8_t)SwatchIndex(s + u + 1, t + v, w, h, entries);
									raw.push_back((uint8_t)(((hi & 0xF) << 4) | (lo & 0xF)));
								}
					break;

				case GFX::TF_C8:
					for (int t = 0; t < h; t += 4)
						for (int s = 0; s < w; s += 8)
							for (int v = 0; v < 4; v++)
								for (int u = 0; u < 8; u++)
									raw.push_back((uint8_t)SwatchIndex(s + u, t + v, w, h, entries));
					break;

				case GFX::TF_C14:
					for (int t = 0; t < h; t += 4)
						for (int s = 0; s < w; s += 4)
							for (int v = 0; v < 4; v++)
								for (int u = 0; u < 4; u++)
									PushBe16(raw, (uint16_t)SwatchIndex(s + u, t + v, w, h, entries));
					break;

				default:
					Assert::Fail(L"EncodePaletted: not a paletted format");
					break;
			}

			return raw;
		}

		//! A palette entry of the ramp the paletted galleries use: the hue walks with the index, and so
		//! does the alpha - the palette formats that can store one (IA8, RGB5A3) therefore show an
		//! alpha ramp when the texel is blended.
		Rgba PaletteEntry(int index, int entries)
		{
			int t = (index * 255) / (entries - 1);
			int r = t;
			int g = (t * 2) & 0xFF;
			int b = 255 - t;
			int a = 64 + (index * 191) / (entries - 1);
			return MakeRgba(r, g, b, a);
		}

		//! Build the TLUT source for the given palette format.
		std::vector<uint16_t> BuildPalette(GFX::TlutFormat format, int entries)
		{
			std::vector<uint16_t> palette(entries);

			for (int i = 0; i < entries; i++)
			{
				Rgba c = PaletteEntry(i, entries);

				switch (format)
				{
					case GFX::TLUT_IA8: palette[i] = EncodeIa8(c); break;
					case GFX::TLUT_RGB565: palette[i] = Encode565(c); break;
					default:
						// Every other entry uses the RGBA4444 form, so both halves of the format show
						palette[i] = EncodeRgb5a3(c, (i & 1) != 0);
						break;
				}
			}

			return palette;
		}

		// ---------------------------------------------------------------------------------------
		// Gallery helpers
		// ---------------------------------------------------------------------------------------

		//! Draw the texture of map 0 over the whole EFB, with the given sampler mode.
		void DrawFullTexture(GfxTestMachine& m, float s0 = 0.0f, float t0 = 0.0f, float s1 = 1.0f, float t1 = 1.0f)
		{
			DrawClipQuad(m, -1, -1, 1, 1, MakeRgba(0xFF, 0xFF, 0xFF), MakeRgba(0xFF, 0xFF, 0xFF),
				MakeRgba(0xFF, 0xFF, 0xFF), MakeRgba(0xFF, 0xFF, 0xFF), s0, t0, s1, t1);
		}

		//! The TX_SETMODE0 word: wrap modes, the two filters, the LOD bias and the anisotropy.
		uint32_t TexMode0(int wrapS, int wrapT, int magFilter, int minFilter, int lodBias = 0)
		{
			return (uint32_t)(wrapS & 3) | ((uint32_t)(wrapT & 3) << 2) |
				((uint32_t)(magFilter & 1) << 4) | ((uint32_t)(minFilter & 7) << 5) |
				((uint32_t)(lodBias & 0xFF) << 8);
		}

		//! The bright four-corner background the alpha gallery fades into, drawn with the raster stage
		//! (the TEV stage is global state, so the background needs the raster program; the texture stage
		//! is put back afterwards).
		void DrawBrightBackgroundThenTextureStage(GfxTestMachine& m)
		{
			SetupRasterStage0(m);
			m.BpLoad(PE_CMODE0_ID, (1u << 3) | (1u << 4));
			DrawScreenQuad(m, MakeRgba(0x90, 0x30, 0x30), MakeRgba(0x30, 0x90, 0x40),
				MakeRgba(0x30, 0x40, 0xA0), MakeRgba(0xA0, 0xA0, 0x30));
			SetupTextureStage0(m);
		}
		//! The format gallery layout: the left half of the EFB shows the texture blended over a bright
		//! background (so what the format puts in the alpha channel shows), the right half shows it
		//! opaque on the black clear (so the colour precision shows).
		void DrawFormatPanels(GfxTestMachine& m, int map = 0)
		{
			const float gap = 0.02f;

			SetupRasterStage0(m);
			m.BpLoad(PE_CMODE0_ID, (1u << 3) | (1u << 4));
			DrawClipQuad(m, -1, -1, -gap, 1, MakeRgba(0x90, 0x30, 0x30), MakeRgba(0x30, 0x90, 0x40),
				MakeRgba(0x30, 0x40, 0xA0), MakeRgba(0xA0, 0xA0, 0x30));

			SetupTextureStage0(m, map);
			m.BpLoad(PE_CMODE0_ID, (1u << 3) | (1u << 4) | 1u | (5u << 5) | (4u << 8));
			DrawClipQuad(m, -1, -1, -gap, 1, MakeRgba(0xFF, 0xFF, 0xFF), MakeRgba(0xFF, 0xFF, 0xFF),
				MakeRgba(0xFF, 0xFF, 0xFF), MakeRgba(0xFF, 0xFF, 0xFF));

			m.BpLoad(PE_CMODE0_ID, (1u << 3) | (1u << 4));
			DrawClipQuad(m, gap, -1, 1, 1, MakeRgba(0xFF, 0xFF, 0xFF), MakeRgba(0xFF, 0xFF, 0xFF),
				MakeRgba(0xFF, 0xFF, 0xFF), MakeRgba(0xFF, 0xFF, 0xFF));
		}
	}

	TEST_CLASS(GfxReportTextureTests)
	{
	public:

		// =========================================================================================
		// The texel formats
		// =========================================================================================

		// Every format the texture unit decodes gets its own picture of the same test swatch, so the
		// formats can be compared at a glance: the intensity formats lose the hue, the 16-bit and
		// 4-bit formats quantise visibly, the paletted ones show their palette density and CMPR shows
		// its 4x4 sub-blocks. gfx-tc.md 5.1-5.4.
		TEST_METHOD(Report_TextureFormats)
		{
			GfxTestMachine& m = M();

			Report::Section("Texture formats",
				"One picture per texel format, all of them the same test swatch (colour bars, a one-texel\n"
				"checkerboard, a two-axis gradient and a sixteen step wedge). The left half of every picture is the\n"
				"texture blended over a bright background, the right half the same texture drawn opaque on black:\n"
				"the left half therefore shows what the format puts in the alpha channel and the right half its\n"
				"colour precision.");

			const int size = 32;

			struct Case
			{
				const char* file;
				const char* title;
				GFX::TexFormat format;
				const char* note;
			};

			const Case cases[] = {
				{ "tex_fmt_i4.png", "I4 - four bit intensity", GFX::TF_I4,
					"Sixteen grey levels shared by all four components, so the alpha of a texel is its intensity." },
				{ "tex_fmt_i8.png", "I8 - eight bit intensity", GFX::TF_I8,
					"Smooth grey ramp; the colour bars collapse into brightness steps." },
				{ "tex_fmt_ia4.png", "IA4 - intensity and alpha", GFX::TF_IA4,
					"Four bits of intensity and four of alpha per texel: the left half fades by the alpha nibble,\nnot by the brightness." },
				{ "tex_fmt_ia8.png", "IA8 - intensity and alpha", GFX::TF_IA8,
					"Eight bits each; the alpha is the high byte of the texel." },
				{ "tex_fmt_rgb565.png", "RGB565", GFX::TF_RGB565,
					"Five and six bit components and no alpha at all, so both halves look the same." },
				{ "tex_fmt_rgb5a3.png", "RGB5A3", GFX::TF_RGB5A3,
					"The top bit of a texel selects RGB555 with an opaque alpha or RGBA4444, so the left half fades\nwherever a texel took the second form." },
				{ "tex_fmt_rgba8.png", "RGBA8", GFX::TF_RGBA8,
					"Full eight bit precision from the two tiles of the format (alpha/red, green/blue)." },
				{ "tex_fmt_c4.png", "C4 with a 16 entry palette", GFX::TF_C4,
					"Two indices per byte, so the picture can only use sixteen colours." },
				{ "tex_fmt_c8.png", "C8 with a 256 entry palette", GFX::TF_C8,
					"One index per byte; the palette ramp is smooth." },
				{ "tex_fmt_c14.png", "C14 with a 1024 entry palette", GFX::TF_C14,
					"A fourteen bit index, so the palette is finer than the picture can show." },
				{ "tex_fmt_cmpr.png", "CMPR", GFX::TF_CMPR,
					"Four 4x4 sub-blocks per tile, each with two RGB565 endpoints and sixteen 2-bit indices." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupTextureStage0(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				if (c.format == GFX::TF_C4 || c.format == GFX::TF_C8 || c.format == GFX::TF_C14)
				{
					int entries = (c.format == GFX::TF_C4) ? 16 : ((c.format == GFX::TF_C8) ? 256 : 1024);
					std::vector<uint8_t> raw = EncodePaletted(c.format, size, size, entries);
					std::vector<uint16_t> palette = BuildPalette(GFX::TLUT_RGB5A3, entries);

					SetupTlut(m, 0, TlutAddr, GFX::TLUT_RGB5A3, palette.data(), palette.size());
					SetupTexture(m, 0, TexAddr, size, size, c.format, raw.data(), raw.size());
				}
				else
				{
					std::vector<uint8_t> raw = EncodeTexture(c.format, size, size, SwatchTexel);
					SetupTexture(m, 0, TexAddr, size, size, c.format, raw.data(), raw.size());
				}

				m.BeginFrame();
				DrawFormatPanels(m);
				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// The palette (TLUT) formats
		// =========================================================================================

		// The same index ramp through the three palette formats: an IA8 palette is grey and carries the
		// alpha in its low byte, RGB565 has no alpha at all and RGB5A3 switches between the opaque and
		// the RGBA4444 form per entry. gfx-tc.md 5.4.
		TEST_METHOD(Report_TlutPalettes)
		{
			GfxTestMachine& m = M();

			Report::Section("Palette (TLUT) formats",
				"The same index picture (an entry ramp, eight entry bars, a checkerboard of the first and the last\n"
				"entry and a sixteen step wedge) looked up through the three palette formats and the three index\n"
				"sizes. The palette is what decides the colours: the index ramp is identical in every image.");

			struct Case
			{
				const char* file;
				const char* title;
				GFX::TexFormat format;
				GFX::TlutFormat palette;
				int entries;
				const char* note;
			};

			const Case cases[] = {
				{ "tex_tlut_c4_ia8.png", "C4 indices, IA8 palette", GFX::TF_C4, GFX::TLUT_IA8, 16,
					"Sixteen grey entries: the index ladder is coarse and monochrome." },
				{ "tex_tlut_c4_rgb565.png", "C4 indices, RGB565 palette", GFX::TF_C4, GFX::TLUT_RGB565, 16,
					"The same sixteen entries as colours, which is what a 4-bit index can address." },
				{ "tex_tlut_c4_rgb5a3.png", "C4 indices, RGB5A3 palette", GFX::TF_C4, GFX::TLUT_RGB5A3, 16,
					"Odd entries are stored in the RGBA4444 form, so half the bars are quantised harder." },
				{ "tex_tlut_c8_ia8.png", "C8 indices, IA8 palette", GFX::TF_C8, GFX::TLUT_IA8, 256,
					"256 grey entries with an alpha byte each." },
				{ "tex_tlut_c8_rgb565.png", "C8 indices, RGB565 palette", GFX::TF_C8, GFX::TLUT_RGB565, 256,
					"The full colour ramp a byte-sized index can address." },
				{ "tex_tlut_c8_rgb5a3.png", "C8 indices, RGB5A3 palette", GFX::TF_C8, GFX::TLUT_RGB5A3, 256,
					"Every other entry also carries an alpha, which this picture shows as a slightly coarser ramp." },
				{ "tex_tlut_c14_ia8.png", "C14 indices, IA8 palette", GFX::TF_C14, GFX::TLUT_IA8, 1024,
					"A 1024 entry grey palette, the finest index ramp of the three sizes." },
				{ "tex_tlut_c14_rgb565.png", "C14 indices, RGB565 palette", GFX::TF_C14, GFX::TLUT_RGB565, 1024,
					"1024 colour entries: the wedges are almost continuous." },
				{ "tex_tlut_c14_rgb5a3.png", "C14 indices, RGB5A3 palette", GFX::TF_C14, GFX::TLUT_RGB5A3, 1024,
					"The 14-bit index reaches entries far past the 256 the C8 format can address." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetupTextureStage0(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				const int size = 32;
				std::vector<uint8_t> raw = EncodePaletted(c.format, size, size, c.entries);
				std::vector<uint16_t> palette = BuildPalette(c.palette, c.entries);

				SetupTlut(m, 0, TlutAddr, c.palette, palette.data(), palette.size());
				SetupTexture(m, 0, TexAddr, size, size, c.format, raw.data(), raw.size());

				m.BeginFrame();
				DrawFullTexture(m);
				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// Filters and mip levels
		// =========================================================================================

		// The same detailed texture is drawn four times, from a five-fold magnification down to a
		// minification of about 1.7 samples per texel, once per sampler setting. Near the right edge
		// the sampler is minifying, which is where the mip chain changes the picture.
		TEST_METHOD(Report_TextureFilters)
		{
			GfxTestMachine& m = M();

			Report::Section("Texture filters and mip levels",
				"A high frequency test texture (a two-texel checkerboard over a gradient) drawn at four sizes.\n"
				"The left copy is magnified five times, the right one is minified, so the same image shows the\n"
				"magnification filter, the minification filter and - from the third copy on - the mip chain.");

			struct Case
			{
				const char* file;
				const char* title;
				int magFilter;
				int minFilter;
				const char* note;
			};

			// The min filter values are the Flipper ones: 0 nearest, 1 linear, 2/3 nearest mipmap,
			// 4/5 linear mipmap (tx.cpp ApplyTextureParams).
			const Case cases[] = {
				{ "tex_filter_nearest.png", "mag and min nearest", 0, 0,
					"Every copy shows the texel grid; the minified copies alias badly." },
				{ "tex_filter_linear.png", "mag and min linear", 1, 1,
					"The magnified copies interpolate between texels, the minified ones average the level 0 image." },
				{ "tex_filter_mip_nearest.png", "min nearest mipmap", 1, 2,
					"Minification picks a mip level per pixel, so the small copies lose the checkerboard." },
				{ "tex_filter_mip_linear.png", "min linear mipmap", 1, 5,
					"Trilinear filtering: the small copies are smooth and keep the gradient." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetupTextureStage0(m);
				SetClearColor(m, MakeRgba(8, 8, 16));

				const int size = 64;
				std::vector<uint8_t> raw = EncodeTexture(GFX::TF_RGBA8, size, size, DetailTexel);
				SetupTexture(m, 0, TexAddr, size, size, GFX::TF_RGBA8, raw.data(), raw.size(),
					TexMode0(0, 0, c.magFilter, c.minFilter));

				m.BeginFrame();

				// Four copies of the texture, left to right: magnified, roughly 1:1 and minified
				const int sizes[4] = { 320, 160, 80, 40 };
				int x = 8;

				for (int i = 0; i < 4; i++)
				{
					DrawPixelRect(m, (float)x, (float)(240 - sizes[i] / 2), (float)(x + sizes[i]),
						(float)(240 + sizes[i] / 2), MakeRgba(0xFF, 0xFF, 0xFF));
					x += sizes[i] + 8;
				}

				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// The mip selection of the sampler
		// =========================================================================================

		// TX_SETMODE0.lodbias (a bias in 1/32 of a level) and TX_SETMODE1.minlod / maxlod (limits in
		// 1/16 of a level) are what the texture unit adds to the computed level of detail and clamps it
		// with (gfx-tc.md 3.3, 4.3, 4.4). The detail texture of the filter gallery is drawn four times
		// in every picture, from five-fold magnification down to minification; the mip chain is on in
		// all of them, so the sampler shows the level the registers selected.
		TEST_METHOD(Report_TexLodSelection)
		{
			GfxTestMachine& m = M();

			Report::Section("The mip selection of the sampler",
				"The same four copies of the detail texture as in the filter gallery, drawn once per LOD setting. The\n"
				"sampler is nearest with an integer mip level, so the level is visible as the size of the blocks the\n"
				"picture breaks into: the bias moves the level up, maxlod clamps it back to a level of its own, and\n"
				"minlod = 4 pins even the magnified copies four levels down, which turns them into a flat colour.");

			struct Case
			{
				const char* file;
				const char* title;
				int lodBias;			// 1/32 of a level
				int minLod;				// 1/16 of a level
				int maxLod;				// 1/16 of a level
				const char* note;
			};

			const Case cases[] = {
				{ "tex_lod_chain.png", "no bias, the whole mip chain", 0, 0, 160,
					"The sampler picks the level the derivatives ask for; the magnified copies are at level 0." },
				{ "tex_lod_bias_up.png", "bias +4 levels", 127, 0, 160,
					"The whole picture is four levels coarser: the magnified copies lose their checkerboard and the\n"
					"small ones are a flat colour." },
				{ "tex_lod_max1.png", "bias +4 but maxlod 1 level", 127, 0, 16,
					"The maximum level is one, so the clamp undoes the bias: every copy is sampled one level down\n"
					"instead of four, and the picture breaks into blocks of two texels rather than into flat colour." },
				{ "tex_lod_min4.png", "minlod 4", 0, 64, 160,
					"The minimum level is four: even the magnified copies are sampled four levels down, which is why\n"
					"the picture is flat." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetupTextureStage0(m);
				SetClearColor(m, MakeRgba(8, 8, 16));

				const int size = 64;
				std::vector<uint8_t> raw = EncodeTexture(GFX::TF_RGBA8, size, size, DetailTexel);

				// The sampler is nearest in both filters, so the level the registers select is visible
				// as the size of the blocks the picture breaks into (a level of zero shows the texel
				// grid, a coarse level shows the averaged colour)
				uint32_t mode0 = TexMode0(0, 0, 0, 2, c.lodBias);
				uint32_t mode1 = (uint32_t)(c.minLod & 0xFF) | ((uint32_t)(c.maxLod & 0xFF) << 8);

				SetupTexture(m, 0, TexAddr, size, size, GFX::TF_RGBA8, raw.data(), raw.size(), mode0, mode1);

				m.BeginFrame();

				const int sizes[4] = { 320, 160, 80, 40 };
				int x = 8;

				for (int i = 0; i < 4; i++)
				{
					DrawPixelRect(m, (float)x, (float)(240 - sizes[i] / 2), (float)(x + sizes[i]),
						(float)(240 + sizes[i] / 2), MakeRgba(0xFF, 0xFF, 0xFF));
					x += sizes[i] + 8;
				}

				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// The texture coordinate scale of the setup unit
		// =========================================================================================

		// SU_SSIZE/SU_TSIZE hold the size of the texture bound to a coordinate pair minus one: the SU
		// multiplies the coordinate by it before the texture unit sees it, so a smaller value samples a
		// smaller part of the texture and magnifies the picture (gfx-su.md 4.5/4.6,
		// GX_SetTexCoordScaleManually). The swatch texture is drawn over the whole EFB with three
		// scales: the automatic one (no register written), a scale twice as small and one twice as
		// large, which stretches the coordinate past the texture and shows the wrap mode at the edges.
		TEST_METHOD(Report_TexCoordScale)
		{
			GfxTestMachine& m = M();

			Report::Section("The texture coordinate scale (SU_SSIZE/SU_TSIZE)",
				"The swatch texture drawn over the whole EFB with a different coordinate scale each time. The\n"
				"register holds the size of the texture minus one, so the automatic setting is 32 for this 32 x 32\n"
				"texture; a scale of 16 magnifies the left half of it over the whole screen and a scale of 64 zooms\n"
				"out, which leaves the coordinate outside the texture for three quarters of the screen (the wrap\n"
				"mode clamps, so the border texels are stretched into the margin).");

			struct Case
			{
				const char* file;
				const char* title;
				int scale;				// The value written to SU_SSIZE0/SU_TSIZE0 (size in texels)
				const char* note;
			};

			const Case cases[] = {
				{ "tex_coordscale_auto.png", "the automatic scale", 0,
					"Nothing is programmed: the coordinate covers the whole 32 x 32 texture exactly once." },
				{ "tex_coordscale_half.png", "a scale of 16 texels", 16,
					"Half the scale: the coordinate reaches only the left half of the texture, so the left half of\n"
					"the swatch fills the screen." },
				{ "tex_coordscale_double.png", "a scale of 64 texels", 64,
					"Twice the scale: the coordinate runs past the right edge of the texture a quarter of the way\n"
					"across the screen, and the clamped border texels fill the right half." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetupTextureStage0(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				const int size = 32;
				std::vector<uint8_t> raw = EncodeTexture(GFX::TF_RGBA8, size, size, SwatchTexel);

				SetupTexture(m, 0, TexAddr, size, size, GFX::TF_RGBA8, raw.data(), raw.size(),
					TexMode0(0, 0, 0, 0));

				if (c.scale != 0)
				{
					m.BpLoad(SU_SSIZE0_ID, (uint32_t)(c.scale - 1));
					m.BpLoad(SU_TSIZE0_ID, (uint32_t)(c.scale - 1));
				}

				m.BeginFrame();
				DrawFullTexture(m);

				Publish(m, c.title, c.file, c.note);
			}
		}

		// Texture coordinates that run outside [0, 1] are where the wrap mode is defined. The texture
		// is a four-quadrant frame with a white border, so a seam, a mirror or a stretched edge is
		// immediately visible. gfx-tc.md 4.3 (TX_SETMODE0.wrap_s / wrap_t).
		TEST_METHOD(Report_TextureWrapModes)
		{
			GfxTestMachine& m = M();

			Report::Section("Wrap modes",
				"The sampler is handed coordinates from -0.25 to 1.25 (a quarter of a tile outside the texture\n"
				"on every side), so the picture always shows three tiles' worth of coordinate range. The texture is\n"
				"a four-quadrant frame - a shaded colour per quadrant inside a white border - so a repeat shows whole\n"
				"tiles, a clamp stretches the border texels (the margin of the first picture is the white frame alone)\n"
				"and a mirror flips every other tile.");

			struct Case
			{
				const char* file;
				const char* title;
				int wrapS;
				int wrapT;
				float s0, t0, s1, t1;
				const char* note;
			};

			const Case cases[] = {
				{ "tex_wrap_clamp.png", "clamp / clamp", 0, 0, -0.25f, -0.25f, 1.25f, 1.25f,
					"Outside [0, 1] the border texels are stretched: the white frame fills the picture's edge." },
				{ "tex_wrap_repeat.png", "repeat / repeat", 1, 1, -0.25f, -0.25f, 1.25f, 1.25f,
					"The coordinate wraps, so a quarter of the neighbouring tile shows on every side - the four\nquadrants reappear at a smaller size." },
				{ "tex_wrap_mirror.png", "mirror / mirror", 2, 2, -0.25f, -0.25f, 1.25f, 1.25f,
					"Every other tile is mirrored, which puts the red quadrant in two opposite corners." },
				{ "tex_wrap_repeat_s.png", "repeat in S, clamp in T", 1, 0, -1.0f, 0.0f, 2.0f, 1.0f,
					"S runs from -1 to 2: three full tiles horizontally, while T stays inside the texture." },
				{ "tex_wrap_mirror_t.png", "clamp in S, mirror in T", 0, 2, 0.0f, -1.0f, 1.0f, 2.0f,
					"The same three tiles vertically, mirrored; the horizontal direction is clamped." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetupTextureStage0(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				const int size = 32;
				std::vector<uint8_t> raw = EncodeTexture(GFX::TF_RGB565, size, size, FrameTexel);
				SetupTexture(m, 0, TexAddr, size, size, GFX::TF_RGB565, raw.data(), raw.size(),
					TexMode0(c.wrapS, c.wrapT, 0, 0));

				m.BeginFrame();
				DrawFullTexture(m, c.s0, c.t0, c.s1, c.t1);
				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// Texture sizes
		// =========================================================================================

		// The same swatch at four resolutions: the texture size decides how much of the pattern a texel
		// carries, and the sampler magnifies the small ones into large blocks.
		TEST_METHOD(Report_TextureSizes)
		{
			GfxTestMachine& m = M();

			Report::Section("Texture sizes",
				"The same swatch encoded at four texture sizes and magnified over the whole EFB with the nearest\n"
				"filter. The larger the texture, the finer the pattern; the smallest one is a 4x4 texel texture\n"
				"whose every texel covers a quarter of the screen.");

			const int sizes[4] = { 4, 8, 16, 64 };
			const char* files[4] = { "tex_size_4.png", "tex_size_8.png", "tex_size_16.png", "tex_size_64.png" };

			for (int i = 0; i < 4; i++)
			{
				int size = sizes[i];

				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetupTextureStage0(m);
				SetClearColor(m, MakeRgba(0, 0, 0));

				std::vector<uint8_t> raw = EncodeTexture(GFX::TF_RGB565, size, size, SwatchTexel);
				SetupTexture(m, 0, TexAddr, size, size, GFX::TF_RGB565, raw.data(), raw.size());

				m.BeginFrame();
				DrawFullTexture(m);

				Publish(m, "Swatch in a " + std::to_string(size) + "x" + std::to_string(size) + " texture", files[i],
					"The texel grid of a " + std::to_string(size) + "x" + std::to_string(size) +
					" texture magnified to 640x480.");
			}
		}

		// =========================================================================================
		// CMPR
		// =========================================================================================

		// CMPR stores each 4x4 sub-block as two RGB565 endpoints plus sixteen 2-bit indices. Depending
		// on which endpoint is the larger the decoder builds the four-colour palette (1/3 and 2/3
		// points) or the three-colour one (1/2 and 2/3 points plus a transparent index), so the two
		// modes reproduce the same block differently. gfx-tc.md 5.3.
		TEST_METHOD(Report_TextureCmprModes)
		{
			GfxTestMachine& m = M();

			Report::Section("CMPR block encoding",
				"Every picture is one CMPR texture over the whole EFB, blended over the gallery background so that\n"
				"the transparent texel of the three-colour mode is visible as the background showing through. A tile\n"
				"is four 4x4 sub-blocks; the encoder picks the two endpoints of a sub-block and the closest of the\n"
				"four palette entries per texel, so the pictures show what the format can look like.");

			const int size = 32;

			// The same swatch through both endpoint orderings
			struct Case
			{
				const char* file;
				const char* title;
				bool threeColour;
				bool fixedEndpoints;
				const char* note;
			};

			const Case cases[] = {
				{ "tex_cmpr_opaque.png", "four-colour mode (col0 > col1)", false, false,
					"The palette is col0, col1 and the 1/3 and 2/3 points, so every sub-block keeps four levels\nof its own brightest-to-darkest range." },
				{ "tex_cmpr_three_colour.png", "three-colour mode (col0 < col1)", true, false,
					"The mid entry is the 1/2 point instead of the 1/3 point, and the fourth entry is the\n"
					"transparent texel: those texels are black and let the background through, so the picture is\n"
					"full of holes where the swatch is dark." },
				{ "tex_cmpr_endpoints.png", "fixed endpoints per sub-block", false, true,
					"Every sub-block of the texture was encoded with its own endpoint pair (red/blue, green/yellow,\nwhite/black, cyan/magenta) and indices stepping through the palette, which makes the four entries\nof a block visible as four bands." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupTextureStage0(m);
				SetClearColor(m, MakeRgba(0x18, 0x10, 0x20));

				std::vector<uint8_t> raw = EncodeCmpr(size, size, SwatchTexel, c.threeColour, c.fixedEndpoints);
				SetupTexture(m, 0, TexAddr, size, size, GFX::TF_CMPR, raw.data(), raw.size());

				m.BeginFrame();
				m.BpLoad(PE_ZMODE_ID, 0);

				// The background first (with the raster stage, blend off), then the texture blended on top
				DrawBackgroundThenTextureStage(m);

				m.BpLoad(PE_CMODE0_ID, (1u << 3) | (1u << 4) | 1u | (5u << 5) | (4u << 8));
				DrawFullTexture(m);

				Publish(m, c.title, c.file, c.note);
			}

			// ... and the tile geometry: the sub-blocks of a tile are ordered TL, TR, BL, BR, so a
			// texture whose 8x8 tiles are coloured differently shows where the tile boundaries are.
			SetupPassThroughXF(m);
			SetupTextureStage0(m);
			SetClearColor(m, MakeRgba(0, 0, 0));

			auto tiledTexel = [](int x, int y, int w, int h) -> Rgba {
				int tile = ((y / 8) * (w / 8) + (x / 8)) & 3;
				static const Rgba colors[4] = {
					{ 0xD0, 0x40, 0x30, 0xFF },
					{ 0x30, 0xD0, 0x50, 0xFF },
					{ 0x40, 0x50, 0xE0, 0xFF },
					{ 0xE0, 0xC0, 0x30, 0xFF },
				};
				int shading = ((x % 8) + (y % 8)) * 16;
				Rgba c = colors[tile];
				c.R = (uint8_t)((c.R + shading) / 2);
				c.G = (uint8_t)((c.G + shading) / 2);
				c.B = (uint8_t)((c.B + shading) / 2);
				return c;
			};

			{
				std::vector<uint8_t> raw = EncodeCmpr(16, 16, tiledTexel, false);
				SetupTexture(m, 0, TexAddr, 16, 16, GFX::TF_CMPR, raw.data(), raw.size());

				m.BeginFrame();
				DrawFullTexture(m);
				Publish(m, "The 8x8 tile grid of CMPR", "tex_cmpr_tiles.png",
					"A 16x16 CMPR texture: each 8x8 tile is four sub-blocks, and every sub-block of a tile was\n"
					"given a different base colour. The sub-block grid inside a tile is the 4x4 texel unit the\n"
					"format compresses, so the picture shows both levels of the block structure.");
			}
		}

		// =========================================================================================
		// Alpha formats
		// =========================================================================================

		// The alpha of a texel is invisible in an RGB readback, so the whole swatch is drawn once per
		// format with the source-alpha blend enabled over a coloured background: the picture then shows
		// the decoded alpha channel directly. gfx-tc.md 5.1 (the alpha of I4/I8/IA4/IA8 and the A-bit of
		// RGB5A3) and gfx-pe.md 6.2 (the blend factors).
		TEST_METHOD(Report_TextureAlphaBlend)
		{
			GfxTestMachine& m = M();

			Report::Section("The alpha channel of the formats",
				"The same swatch drawn over a four-corner colour background with the source-alpha blend enabled,\n"
				"once per format that carries an alpha. Where the texel alpha is low the background shows through,\n"
				"so these pictures are a direct view of what each format puts in the alpha channel: the intensity\n"
				"itself for I4 and I8, the alpha nibble or byte for IA4 and IA8, a three bit alpha for the RGB5A3\n"
				"texels whose A-bit is clear, and the RGBA8 alpha. RGB565 and CMPR have no alpha (except the CMPR\n"
				"transparent entry of the three-colour mode), so blending them would change nothing and their\n"
				"pictures are in the format gallery instead.");

			struct Case
			{
				const char* file;
				const char* title;
				GFX::TexFormat format;
				const char* note;
			};

			const Case cases[] = {
				{ "tex_alpha_i4.png", "I4: the alpha is the intensity", GFX::TF_I4,
					"An intensity texture is opaque where it is bright, so the dark half of the swatch fades out." },
				{ "tex_alpha_i8.png", "I8: the alpha is the intensity", GFX::TF_I8,
					"The same for eight bit intensity: the fade follows the brightness much more closely." },
				{ "tex_alpha_ia4.png", "IA4: a four bit alpha", GFX::TF_IA4,
					"The alpha nibble is independent of the intensity, so the fade is a regular ladder of about\nsixteen steps." },
				{ "tex_alpha_ia8.png", "IA8: an eight bit alpha", GFX::TF_IA8,
					"A smooth fade that follows the alpha byte, not the intensity." },
				{ "tex_alpha_rgb5a3.png", "RGB5A3: a three bit alpha", GFX::TF_RGB5A3,
					"The texels whose top bit is clear carry a three bit alpha (the coarse bands), the others are\nopaque - which is why half of this picture hides the background completely." },
				{ "tex_alpha_rgba8.png", "RGBA8: a full alpha", GFX::TF_RGBA8,
					"Eight bits of alpha from the alpha/red tile pair." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupTextureStage0(m);

				// sfactor = source alpha, dfactor = one minus source alpha (gfx-pe.md 6.2)
				const uint32_t blendMode = (1u << 3) | (1u << 4) | 1u | (5u << 5) | (4u << 8);

				m.BpLoad(PE_ZMODE_ID, 0);
				SetClearColor(m, MakeRgba(0, 0, 0));

				const int size = 32;
				std::vector<uint8_t> raw = EncodeTexture(c.format, size, size, SwatchTexel);
				SetupTexture(m, 0, TexAddr, size, size, c.format, raw.data(), raw.size());

				m.BeginFrame();

				// The background is drawn with the raster stage and the blend off, the texture on top of
				// it with the source alpha blend on
				DrawBrightBackgroundThenTextureStage(m);

				m.BpLoad(PE_CMODE0_ID, blendMode);
				DrawFullTexture(m);

				Publish(m, c.title, c.file, c.note);
			}
		}
	};
}
