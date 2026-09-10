// Flipper Texture Engine + TMEM
#include "pch.h"

using namespace Debug;

namespace GFX
{
	static int NextPowerOfTwo(int v)
	{
		int p = 1;
		while (p < v)
			p <<= 1;
		return p;
	}

	void TextureEngine::TexInit()
	{
		memset(texMap, 0, sizeof(texMap));

		for (int i = 0; i < GFX_MAX_TEXTURES; i++)
		{
			glGenTextures(1, &texMap[i].glTexture);
			texMap[i].dirty = false;
			texMap[i].paramsDirty = true;
		}

		active = true;
	}

	void TextureEngine::TexFree()
	{
		for (int i = 0; i < GFX_MAX_TEXTURES; i++)
		{
			if (texMap[i].glTexture)
			{
				glDeleteTextures(1, &texMap[i].glTexture);
				texMap[i].glTexture = 0;
			}
		}
	}

	void TextureEngine::GetTlutCol(Color* c, unsigned id, unsigned entry)
	{
		uint16_t* tptr = (uint16_t*)(&tlut[(tx.settlut[id].tmem << 9) + 2 * entry]);
		int fmt = tx.settlut[id].fmt;

		switch (fmt)
		{
			case TLUT_IA8:
			{
				c->A = *tptr & 0xff;
				c->R = c->G = c->B = *tptr >> 8;
			}
			break;

			case TLUT_RGB565:
			{
				uint16_t p = _BYTESWAP_UINT16(*tptr);

				uint8_t r = p >> 11;
				uint8_t g = (p >> 5) & 0x3f;
				uint8_t b = p & 0x1f;

				c->R = (r << 3) | (r >> 2);
				c->G = (g << 2) | (g >> 4);
				c->B = (b << 3) | (b >> 2);
				c->A = 255;
			}
			break;

			case TLUT_RGB5A3:
			{
				uint16_t p = _BYTESWAP_UINT16(*tptr);
				if (p >> 15)
				{
					p &= ~0x8000;   // clear A-bit

					uint8_t r = p >> 10;
					uint8_t g = (p >> 5) & 0x1f;
					uint8_t b = p & 0x1f;

					c->R = (r << 3) | (r >> 2);
					c->G = (g << 3) | (g >> 2);
					c->B = (b << 3) | (b >> 2);
					c->A = 255;
				}
				else
				{
					uint8_t r = (p >> 8) & 0xf;
					uint8_t g = (p >> 4) & 0xf;
					uint8_t b = p & 0xf;
					uint8_t a = p >> 12;

					c->R = (r << 4) | r;
					c->G = (g << 4) | g;
					c->B = (b << 4) | b;
					c->A = a | (a << 3) | ((a << 9) & 3);
				}
			}
			break;

			default:
			{
				Halt("GX: Unknown TLUT format: %i\n", fmt);
				break;
			}
		}
	}

	void TextureEngine::LoadTlut(uint32_t addr, uint32_t tmem, uint32_t cnt)
	{
		assert(tmem < sizeof(tlut));
		uint8_t* ptr = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForTX(addr);
		memcpy(&tlut[tmem], ptr, cnt * 16 * 2);

		InvalidatePalettedTextures();
	}

	void TextureEngine::InvalidatePalettedTextures()
	{
		// Paletted formats are expanded through the TLUT, so a palette load invalidates them
		for (int i = 0; i < GFX_MAX_TEXTURES; i++)
		{
			int fmt = tx.teximg0[i].fmt;
			if (fmt == TF_C4 || fmt == TF_C8 || fmt == TF_C14)
				texMap[i].dirty = true;
		}
	}

	// Decode one texture map from main memory into rgbabuf. Returns false if the map is not usable.
	bool TextureEngine::DecodeTexture(int id)
	{
		TexMap* m = &texMap[id];

		int fmt = tx.teximg0[id].fmt;
		int oldw = tx.teximg0[id].width + 1;
		int oldh = tx.teximg0[id].height + 1;
		uint32_t addr = tx.teximg3[id].base << 5;

		if (oldw == 0 || oldh == 0)
			return false;

		int width = NextPowerOfTwo(oldw);
		int height = NextPowerOfTwo(oldh);

		if ((size_t)width * height > _countof(rgbabuf))
			return false;

		uint8_t* rawData = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForTX(addr);
		if (rawData == nullptr)
			return false;

		Color* texbuf = rgbabuf;

		m->ds = (float)oldw / (float)width;
		m->dt = (float)oldh / (float)height;
		m->width = oldw;
		m->height = oldh;
		m->dw = width;
		m->dh = height;

		// convert texture
		switch (fmt)
		{
			// "intensity 4". 2 texels per byte, grayscale 0..15
			case TF_I4:
			{
				int s, t, u, v;
				uint8_t* ptr = rawData;

				for (t = 0; t < oldh; t += 8)
					for (s = 0; s < oldw; s += 8)
						for (v = 0; v < 8; v++)
							for (u = 0; u < 8; u += 2)
							{
								unsigned ofs = width * (t + v) + s + u;
								texbuf[ofs].R =
								texbuf[ofs].G =
								texbuf[ofs].B =
								texbuf[ofs].A = (*ptr >> 4) | (*ptr & 0xf0);
								ofs++;
								texbuf[ofs].R =
								texbuf[ofs].G =
								texbuf[ofs].B =
								texbuf[ofs].A = (*ptr << 4) | (*ptr & 0x0f);
								ptr++;
							}
				break;
			}

			// intensity 8-bit. R=G=B=A=I8.
			case TF_I8:
			{
				int s, t, u, v;
				uint8_t* ptr = rawData;

				for (t = 0; t < oldh; t += 4)
					for (s = 0; s < oldw; s += 8)
						for (v = 0; v < 4; v++)
							for (u = 0; u < 8; u++)
							{
								int ofs = width * (t + v) + s + u;
								texbuf[ofs].R =
								texbuf[ofs].G =
								texbuf[ofs].B =
								texbuf[ofs].A = *ptr++;
							}
				break;
			}

			// intesity / alpha 4. one texel per byte
			case TF_IA4:
			{
				int s, t, u, v;
				uint8_t* ptr = rawData;

				// An IA4 tile is 8 texels wide and 4 tall, like every other 8-bit format (the tile
				// shapes follow the tile geometry of the format, see gfx-tc.md 5.3). The intensity is
				// the LOW nibble of the byte and the alpha the HIGH one,
				// and each nibble is repeated to fill 8 bits.
				for (s = 0; s < (oldw / 8); s++)  // tile hor
					for (t = 0; t < (oldh / 4); t++) // tile ver
						for (v = 0; v < 4; v++)  // texel ver
							for (u = 0; u < 8; u++)  // texel hor
							{
								int ofs = width * (4 * t + v) + 8 * s + u;

								uint8_t i = *ptr & 0xf;
								uint8_t a = (*ptr >> 4) & 0xf;

								texbuf[ofs].R =
								texbuf[ofs].G =
								texbuf[ofs].B = (uint8_t)((i << 4) | i);
								texbuf[ofs].A = (uint8_t)((a << 4) | a);
								texbuf[ofs].RGBA = _BYTESWAP_UINT32(texbuf[ofs].RGBA);
								ptr++;
							}
				break;
			}

			case TF_IA8:
			{
				int s, t, u, v;
				uint8_t* ptr = rawData;

				for (t = 0; t < oldh; t += 4)
					for (s = 0; s < oldw; s += 4)
						for (v = 0; v < 4; v++)
							for (u = 0; u < 4; u++)
							{
								unsigned ofs = width * (t + v) + s + u;
								texbuf[ofs].A = *ptr++;
								texbuf[ofs].R =
								texbuf[ofs].G =
								texbuf[ofs].B = *ptr++;
								texbuf[ofs].RGBA = _BYTESWAP_UINT32(texbuf[ofs].RGBA);
							}
				break;
			}

			case TF_RGB565:
			{
				int s, t, u, v;
				uint16_t* ptr = (uint16_t*)rawData;

				for (t = 0; t < oldh; t += 4)
					for (s = 0; s < oldw; s += 4)
						for (v = 0; v < 4; v++)
							for (u = 0; u < 4; u++)
							{
								unsigned ofs = width * (t + v) + s + u;
								uint16_t p = _BYTESWAP_UINT16(*ptr++);

								uint8_t r = p >> 11;
								uint8_t g = (p >> 5) & 0x3f;
								uint8_t b = p & 0x1f;

								texbuf[ofs].R = (r << 3) | (r >> 2);
								texbuf[ofs].G = (g << 2) | (g >> 4);
								texbuf[ofs].B = (b << 3) | (b >> 2);
								texbuf[ofs].A = 255;
								texbuf[ofs].RGBA = _BYTESWAP_UINT32(texbuf[ofs].RGBA);
							}
				break;
			}

			case TF_RGB5A3:
			{
				int s, t, u, v;
				uint16_t* ptr = (uint16_t*)rawData;

				for (t = 0; t < oldh; t += 4)
					for (s = 0; s < oldw; s += 4)
						for (v = 0; v < 4; v++)
							for (u = 0; u < 4; u++)
							{
								unsigned ofs = width * (t + v) + s + u;
								uint16_t p = _BYTESWAP_UINT16(*ptr++);
								if (p >> 15)
								{
									p &= ~0x8000;   // clear A-bit

									uint8_t r = p >> 10;
									uint8_t g = (p >> 5) & 0x1f;
									uint8_t b = p & 0x1f;

									texbuf[ofs].R = (r << 3) | (r >> 2);
									texbuf[ofs].G = (g << 3) | (g >> 2);
									texbuf[ofs].B = (b << 3) | (b >> 2);
									texbuf[ofs].A = 255;
								}
								else
								{
									uint8_t r = (p >> 8) & 0xf;
									uint8_t g = (p >> 4) & 0xf;
									uint8_t b = p & 0xf;
									uint8_t a = p >> 12;

									texbuf[ofs].R = (r << 4) | r;
									texbuf[ofs].G = (g << 4) | g;
									texbuf[ofs].B = (b << 4) | b;
									// The three alpha bits repeat to fill 8 bits ({a, a, a[2:1]}),
									// i.e. the 3-bit alpha repeated to fill 8 bits
									texbuf[ofs].A = (uint8_t)((a << 5) | (a << 2) | ((a >> 1) & 3));
								}
								texbuf[ofs].RGBA = _BYTESWAP_UINT32(texbuf[ofs].RGBA);
							}
				break;
			}

			case TF_RGBA8:
			{
				int s, t, u, v;
				uint8_t* ptr = rawData;

				for (t = 0; t < oldh; t += 4)
					for (s = 0; s < oldw; s += 4)
					{
						for (v = 0; v < 4; v++)
							for (u = 0; u < 4; u++)
							{
								unsigned ofs = width * (t + v) + s + u;
								texbuf[ofs].A = *ptr++;
								texbuf[ofs].R = *ptr++;
							}

						for (v = 0; v < 4; v++)
							for (u = 0; u < 4; u++)
							{
								unsigned ofs = width * (t + v) + s + u;
								texbuf[ofs].G = *ptr++;
								texbuf[ofs].B = *ptr++;
								texbuf[ofs].RGBA = _BYTESWAP_UINT32(texbuf[ofs].RGBA);
							}
					}
				break;
			}

			case TF_C4:
			{
				int s, t, u, v;
				uint8_t* ptr = rawData;
				Color rgba;

				for (t = 0; t < oldh; t += 8)
					for (s = 0; s < oldw; s += 8)
						for (v = 0; v < 8; v++)
							for (u = 0; u < 8; u += 2)
							{
								unsigned ofs = width * (t + v) + s + u;
								uint8_t e = *ptr++;
								GetTlutCol(&rgba, id, e >> 4);
								texbuf[ofs].RGBA = _BYTESWAP_UINT32(rgba.RGBA);
								ofs++;
								GetTlutCol(&rgba, id, e & 0xf);
								texbuf[ofs].RGBA = _BYTESWAP_UINT32(rgba.RGBA);
							}
				break;
			}

			case TF_C8:
			{
				int s, t, u, v;
				uint8_t* ptr = rawData;
				Color rgba;

				for (t = 0; t < oldh; t += 4)
					for (s = 0; s < oldw; s += 8)
						for (v = 0; v < 4; v++)
							for (u = 0; u < 8; u++)
							{
								int ofs = width * (t + v) + s + u;
								uint8_t idx = *ptr++;
								GetTlutCol(&rgba, id, idx);
								texbuf[ofs].RGBA = _BYTESWAP_UINT32(rgba.RGBA);
							}
				break;
			}

			case TF_C14:
			{
				int s, t, u, v;
				uint16_t* ptr = (uint16_t*)rawData;
				Color rgba;

				for (t = 0; t < oldh; t += 4)
					for (s = 0; s < oldw; s += 4)
						for (v = 0; v < 4; v++)
							for (u = 0; u < 4; u++)
							{
								unsigned ofs = width * (t + v) + s + u;
								uint16_t idx = _BYTESWAP_UINT16(*ptr++) & 0x3ff;
								GetTlutCol(&rgba, id, idx);
								texbuf[ofs].RGBA = _BYTESWAP_UINT32(rgba.RGBA);
							}
				break;
			};

			case TF_CMPR:
			{
				int s, t, u, v;
				uint8_t* ptr = rawData;
				Color rgb[4] = {};   // color look-up (only the entries a texel really uses are filled)
				uint8_t r, g, b;
				uint8_t tnum;
				uint16_t p;
				S3TC_BLK blk;

				// 4 blocks by 4x4 texels in a row
				// zigzag order
				for (t = 0; t < oldh; t += 8)
					for (s = 0; s < oldw; s += 8)
					{
						tnum = 0;
						memcpy(&blk, ptr, sizeof(S3TC_BLK));
						ptr += sizeof(S3TC_BLK);
						for (v = 0; v < 4; v++)
						{
							p = _BYTESWAP_UINT16(blk.rgb0);
							r = p >> 11;
							g = (p >> 5) & 0x3f;
							b = p & 0x1f;

							rgb[0].R = (r << 3) | (r >> 2);
							rgb[0].G = (g << 2) | (g >> 4);
							rgb[0].B = (b << 3) | (b >> 2);

							p = _BYTESWAP_UINT16(blk.rgb1);
							r = p >> 11;
							g = (p >> 5) & 0x3f;
							b = p & 0x1f;

							rgb[1].R = (r << 3) | (r >> 2);
							rgb[1].G = (g << 2) | (g >> 4);
							rgb[1].B = (b << 3) | (b >> 2);

							// interpolate two other
							// A CMPR endpoint carries no alpha of its own: both endpoints are opaque,
							// and only the fourth colour of the 3-colour mode is not.
							rgb[0].A = 255;
							rgb[1].A = 255;

							// The mode is selected by comparing the two *packed* RGB565 endpoints
							// (col0 > col1 in the packed format), so the big-endian words have to
							// be swapped before the comparison; comparing the raw little-endian reads
							// picks the other mode for a few endpoint pairs.
							if (_BYTESWAP_UINT16(blk.rgb0) > _BYTESWAP_UINT16(blk.rgb1))
							{
								rgb[2].R = (2 * rgb[0].R + rgb[1].R) / 3;
								rgb[2].G = (2 * rgb[0].G + rgb[1].G) / 3;
								rgb[2].B = (2 * rgb[0].B + rgb[1].B) / 3;
								rgb[2].A = 255;
								rgb[3].R = (2 * rgb[1].R + rgb[0].R) / 3;
								rgb[3].G = (2 * rgb[1].G + rgb[0].G) / 3;
								rgb[3].B = (2 * rgb[1].B + rgb[0].B) / 3;
								rgb[3].A = 255;
							}
							else
							{
								rgb[2].R = (rgb[0].R + rgb[1].R) / 2;
								rgb[2].G = (rgb[0].G + rgb[1].G) / 2;
								rgb[2].B = (rgb[0].B + rgb[1].B) / 2;
								rgb[2].A = 255;
								// The fourth colour of the 3-colour mode is the transparent texel
								rgb[3].R = 0;
								rgb[3].G = 0;
								rgb[3].B = 0;
								rgb[3].A = 0;
							}

							uint8_t texel = blk.row[tnum++];
							int shft;
							for (u = 0, shft = 6; u < 4; u++, shft -= 2)
							{
								unsigned ofs = width * (t + v) + s + u;
								uint8_t pi = (texel >> shft) & 3;
								texbuf[ofs].RGBA = _BYTESWAP_UINT32(rgb[pi].RGBA);
							}
						}

						tnum = 0;
						memcpy(&blk, ptr, sizeof(S3TC_BLK));
						ptr += sizeof(S3TC_BLK);
						for (v = 0; v < 4; v++)
						{
							p = _BYTESWAP_UINT16(blk.rgb0);
							r = p >> 11;
							g = (p >> 5) & 0x3f;
							b = p & 0x1f;

							rgb[0].R = (r << 3) | (r >> 2);
							rgb[0].G = (g << 2) | (g >> 4);
							rgb[0].B = (b << 3) | (b >> 2);

							p = _BYTESWAP_UINT16(blk.rgb1);
							r = p >> 11;
							g = (p >> 5) & 0x3f;
							b = p & 0x1f;

							rgb[1].R = (r << 3) | (r >> 2);
							rgb[1].G = (g << 2) | (g >> 4);
							rgb[1].B = (b << 3) | (b >> 2);

							// A CMPR endpoint carries no alpha of its own: both endpoints are opaque,
							// and only the fourth colour of the 3-colour mode is not.
							rgb[0].A = 255;
							rgb[1].A = 255;

							// The mode is selected by comparing the two *packed* RGB565 endpoints
							// (col0 > col1 in the packed format), so the big-endian words have to
							// be swapped before the comparison; comparing the raw little-endian reads
							// picks the other mode for a few endpoint pairs.
							if (_BYTESWAP_UINT16(blk.rgb0) > _BYTESWAP_UINT16(blk.rgb1))
							{
								rgb[2].R = (2 * rgb[0].R + rgb[1].R) / 3;
								rgb[2].G = (2 * rgb[0].G + rgb[1].G) / 3;
								rgb[2].B = (2 * rgb[0].B + rgb[1].B) / 3;
								rgb[2].A = 255;
								rgb[3].R = (2 * rgb[1].R + rgb[0].R) / 3;
								rgb[3].G = (2 * rgb[1].G + rgb[0].G) / 3;
								rgb[3].B = (2 * rgb[1].B + rgb[0].B) / 3;
								rgb[3].A = 255;
							}
							else
							{
								rgb[2].R = (rgb[0].R + rgb[1].R) / 2;
								rgb[2].G = (rgb[0].G + rgb[1].G) / 2;
								rgb[2].B = (rgb[0].B + rgb[1].B) / 2;
								rgb[2].A = 255;
								// The fourth colour of the 3-colour mode is the transparent texel
								rgb[3].R = 0;
								rgb[3].G = 0;
								rgb[3].B = 0;
								rgb[3].A = 0;
							}

							uint8_t texel = blk.row[tnum++];
							int shft;
							for (u = 4, shft = 6; u < 8; u++, shft -= 2)
							{
								unsigned ofs = width * (t + v) + s + u;
								uint8_t pi = (texel >> shft) & 3;
								texbuf[ofs].RGBA = _BYTESWAP_UINT32(rgb[pi].RGBA);
							}
						}

						tnum = 0;
						memcpy(&blk, ptr, sizeof(S3TC_BLK));
						ptr += sizeof(S3TC_BLK);
						for (v = 4; v < 8; v++)
						{
							p = _BYTESWAP_UINT16(blk.rgb0);
							r = p >> 11;
							g = (p >> 5) & 0x3f;
							b = p & 0x1f;

							rgb[0].R = (r << 3) | (r >> 2);
							rgb[0].G = (g << 2) | (g >> 4);
							rgb[0].B = (b << 3) | (b >> 2);

							p = _BYTESWAP_UINT16(blk.rgb1);
							r = p >> 11;
							g = (p >> 5) & 0x3f;
							b = p & 0x1f;

							rgb[1].R = (r << 3) | (r >> 2);
							rgb[1].G = (g << 2) | (g >> 4);
							rgb[1].B = (b << 3) | (b >> 2);

							// A CMPR endpoint carries no alpha of its own: both endpoints are opaque,
							// and only the fourth colour of the 3-colour mode is not.
							rgb[0].A = 255;
							rgb[1].A = 255;

							// The mode is selected by comparing the two *packed* RGB565 endpoints
							// (col0 > col1 in the packed format), so the big-endian words have to
							// be swapped before the comparison; comparing the raw little-endian reads
							// picks the other mode for a few endpoint pairs.
							if (_BYTESWAP_UINT16(blk.rgb0) > _BYTESWAP_UINT16(blk.rgb1))
							{
								rgb[2].R = (2 * rgb[0].R + rgb[1].R) / 3;
								rgb[2].G = (2 * rgb[0].G + rgb[1].G) / 3;
								rgb[2].B = (2 * rgb[0].B + rgb[1].B) / 3;
								rgb[2].A = 255;
								rgb[3].R = (2 * rgb[1].R + rgb[0].R) / 3;
								rgb[3].G = (2 * rgb[1].G + rgb[0].G) / 3;
								rgb[3].B = (2 * rgb[1].B + rgb[0].B) / 3;
								rgb[3].A = 255;
							}
							else
							{
								rgb[2].R = (rgb[0].R + rgb[1].R) / 2;
								rgb[2].G = (rgb[0].G + rgb[1].G) / 2;
								rgb[2].B = (rgb[0].B + rgb[1].B) / 2;
								rgb[2].A = 255;
								// The fourth colour of the 3-colour mode is the transparent texel
								rgb[3].R = 0;
								rgb[3].G = 0;
								rgb[3].B = 0;
								rgb[3].A = 0;
							}

							uint8_t texel = blk.row[tnum++];
							int shft;
							for (u = 0, shft = 6; u < 4; u++, shft -= 2)
							{
								unsigned ofs = width * (t + v) + s + u;
								uint8_t pi = (texel >> shft) & 3;
								texbuf[ofs].RGBA = _BYTESWAP_UINT32(rgb[pi].RGBA);
							}
						}

						tnum = 0;
						memcpy(&blk, ptr, sizeof(S3TC_BLK));
						ptr += sizeof(S3TC_BLK);
						for (v = 4; v < 8; v++)
						{
							p = _BYTESWAP_UINT16(blk.rgb0);
							r = p >> 11;
							g = (p >> 5) & 0x3f;
							b = p & 0x1f;

							rgb[0].R = (r << 3) | (r >> 2);
							rgb[0].G = (g << 2) | (g >> 4);
							rgb[0].B = (b << 3) | (b >> 2);
							rgb[0].A = 255;

							p = _BYTESWAP_UINT16(blk.rgb1);
							r = p >> 11;
							g = (p >> 5) & 0x3f;
							b = p & 0x1f;

							rgb[1].R = (r << 3) | (r >> 2);
							rgb[1].G = (g << 2) | (g >> 4);
							rgb[1].B = (b << 3) | (b >> 2);
							rgb[1].A = 255;

							// A CMPR endpoint carries no alpha of its own: both endpoints are opaque,
							// and only the fourth colour of the 3-colour mode is not.
							rgb[0].A = 255;
							rgb[1].A = 255;

							// The mode is selected by comparing the two *packed* RGB565 endpoints
							// (col0 > col1 in the packed format), so the big-endian words have to
							// be swapped before the comparison; comparing the raw little-endian reads
							// picks the other mode for a few endpoint pairs.
							if (_BYTESWAP_UINT16(blk.rgb0) > _BYTESWAP_UINT16(blk.rgb1))
							{
								rgb[2].R = (2 * rgb[0].R + rgb[1].R) / 3;
								rgb[2].G = (2 * rgb[0].G + rgb[1].G) / 3;
								rgb[2].B = (2 * rgb[0].B + rgb[1].B) / 3;
								rgb[2].A = 255;
								rgb[3].R = (2 * rgb[1].R + rgb[0].R) / 3;
								rgb[3].G = (2 * rgb[1].G + rgb[0].G) / 3;
								rgb[3].B = (2 * rgb[1].B + rgb[0].B) / 3;
								rgb[3].A = 255;
							}
							else
							{
								rgb[2].R = (rgb[0].R + rgb[1].R) / 2;
								rgb[2].G = (rgb[0].G + rgb[1].G) / 2;
								rgb[2].B = (rgb[0].B + rgb[1].B) / 2;
								rgb[2].A = 255;
								// The fourth colour of the 3-colour mode is the transparent texel
								rgb[3].R = 0;
								rgb[3].G = 0;
								rgb[3].B = 0;
								rgb[3].A = 0;
							}

							uint8_t texel = blk.row[tnum++];
							int shft;
							for (u = 4, shft = 6; u < 8; u++, shft -= 2)
							{
								unsigned ofs = width * (t + v) + s + u;
								uint8_t pi = (texel >> shft) & 3;
								texbuf[ofs].RGBA = _BYTESWAP_UINT32(rgb[pi].RGBA);
							}
						}
					}
				break;
			};

			default:
				Report(Channel::GP, "Unknown texture format: %i (map %i)\n", fmt, id);
				return false;
		}

		// Remember what the GL image was decoded from
		m->keyAddr = addr;
		m->keyFmt = fmt;
		m->keyWidth = m->width;
		m->keyHeight = m->height;
		m->keyTlut = tx.settlut[id].tmem;
		m->valid = true;

		return true;
	}

	void TextureEngine::UploadTexture(int id)
	{
		TexMap* m = &texMap[id];

		glBindTexture(GL_TEXTURE_2D, m->glTexture);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m->dw, m->dh, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbabuf);

		m->paramsDirty = true;
	}

	void TextureEngine::ApplyTextureParams(int id)
	{
		TexMap* m = &texMap[id];
		TexMode0& mode = tx.texmode0[id];

		glBindTexture(GL_TEXTURE_2D, m->glTexture);

		static const GLint wrap[4] = { GL_CLAMP_TO_EDGE, GL_REPEAT, GL_MIRRORED_REPEAT, GL_REPEAT };
		static const GLint magfilt[2] = { GL_NEAREST, GL_LINEAR };
		static const GLint minfilt[8] = {
			GL_NEAREST,
			GL_LINEAR,
			GL_NEAREST_MIPMAP_NEAREST,
			GL_NEAREST_MIPMAP_LINEAR,
			GL_LINEAR_MIPMAP_NEAREST,
			GL_LINEAR_MIPMAP_LINEAR,
			GL_LINEAR,
			GL_LINEAR
		};

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap[mode.wrap_s & 3]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap[mode.wrap_t & 3]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magfilt[mode.mag_filter & 1]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minfilt[mode.min_filter & 7]);

		// The Flipper can sample mip levels; the emulator only has level 0, so the chain is built here
		if ((mode.min_filter & 7) >= 2)
			glGenerateMipmap(GL_TEXTURE_2D);

		//
		// The mip selection of the sampler (gfx-tc.md 3.3, 4.3, 4.4).
		//
		// TX_SETMODE0.lodbias is an 8-bit signed s2.5 value added to the computed LOD before it is
		// clamped (GX_InitTexObjLOD stores 32*lodbias there, so the value is `bias * 32`), and
		// TX_SETMODE1.minlod/maxlod are unsigned 4.4 values the biased LOD is clamped between
		// (GX_InitTexObjLOD stores 16*lod there). GL applies GL_TEXTURE_LOD_BIAS to the computed level
		// of detail and then clamps it between GL_TEXTURE_MIN_LOD and GL_TEXTURE_MAX_LOD, i.e. the
		// same order, so the three registers map onto the three GL parameters directly.
		//

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, (float)(int8_t)(mode.lodbias & 0xFF) / 32.0f);

		TexMode1& mode1 = tx.texmode1[id];

		float minlod = (float)mode1.minlod / 16.0f;
		float maxlod = (float)mode1.maxlod / 16.0f;

		// The hardware requires minlod <= maxlod; GL rejects the other order outright.
		if (minlod > maxlod)
			minlod = maxlod;

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, minlod);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, maxlod);

		m->appliedMode0 = mode.bits;
		m->appliedMode1 = mode1.bits;
		m->paramsDirty = false;
	}

	void TextureEngine::UpdateAndBindTextures()
	{
		for (int i = 0; i < GFX_MAX_TEXTURES; i++)
		{
			TexMap* m = &texMap[i];

			// Every map lives on its own texture unit, and the decode, the upload and the sampler
			// parameters all act on the texture of the *active* unit (glTexImage2D and glTexParameteri
			// address what is bound there). The unit therefore has to be selected before the map is
			// touched: with the switch at the end of the loop the work of map i landed on the unit of
			// map i-1, and the unit of the last map kept the binding of the one after it, which made
			// several programmed maps sample the same texture.
			glActiveTexture(GL_TEXTURE0 + i);

			if (m->dirty)
			{
				m->dirty = false;

				if (DecodeTexture(i))
				{
					UploadTexture(i);
				}
				else
				{
					m->valid = false;
				}
			}

			if (m->valid && (m->paramsDirty || m->appliedMode0 != tx.texmode0[i].bits ||
				m->appliedMode1 != tx.texmode1[i].bits))
				ApplyTextureParams(i);

			if (m->valid)
				glBindTexture(GL_TEXTURE_2D, m->glTexture);
			else
				glBindTexture(GL_TEXTURE_2D, 0);
		}

		glActiveTexture(GL_TEXTURE0);
	}

	void TextureEngine::UploadTexScales(GLProgram& program)
	{
		float scale[8][2];
		float size[8][2];
		float coordScale[8][2];

		for (int i = 0; i < GFX_MAX_TEXTURES; i++)
		{
			scale[i][0] = texMap[i].valid ? texMap[i].ds : 1.0f;
			scale[i][1] = texMap[i].valid ? texMap[i].dt : 1.0f;

			// The real size in texels: the indirect texturing works in texel units
			size[i][0] = texMap[i].valid ? (float)texMap[i].width : 1.0f;
			size[i][1] = texMap[i].valid ? (float)texMap[i].height : 1.0f;
		}

		// The coordinate scale of the setup unit (SU_SSIZE/SU_TSIZE), one pair per texture coordinate.
		// It is uploaded alongside the padding correction of the sampler (`scale` above) instead of
		// replacing it: the fragment program composes the two (see CoordScale in tev.cpp).
		for (int i = 0; i < 8; i++)
		{
			gfx->su->CoordScale(i, &coordScale[i][0], &coordScale[i][1]);
		}

		glUniform2fv(program.Uniform("texScale[0]"), 8, (float*)scale);
		glUniform2fv(program.Uniform("texSize[0]"), 8, (float*)size);
		glUniform2fv(program.Uniform("suScale[0]"), 8, (float*)coordScale);
	}

	TextureEngine::TextureEngine(HWConfig* config, GFXCore* parent_gfx)
	{
		gfx = parent_gfx;
		memset(rgbabuf, 0, sizeof(rgbabuf));
		memset(tlut, 0, sizeof(tlut));
		// GL objects are created in TexInit(), which is called once the backend has a context
	}

	TextureEngine::~TextureEngine()
	{
		// The GL context is already gone by now; TexFree() is called from GFXCore::GL_CloseSubsystem
		active = false;
	}

	// Map a texture register index to a texture map id. Returns -1 for non-texture registers.
	static bool DecodeTexIndex(size_t index, int* id, int* kind)
	{
		// kind: 0 = SETMODE0, 1 = SETMODE1, 2 = SETIMAGE0, 3 = SETIMAGE1, 4 = SETIMAGE2, 5 = SETIMAGE3, 6 = SETTLUT
		// base: the first texture map of the block (the I4-I7 block programs the maps 4-7)
		struct Range { size_t lo, hi; int kind; int base; };
		static const Range ranges[] = {
			{ TX_SETMODE0_I0_ID,  TX_SETMODE0_I3_ID,  0, 0 },
			{ TX_SETMODE1_I0_ID,  TX_SETMODE1_I3_ID,  1, 0 },
			{ TX_SETIMAGE0_I0_ID, TX_SETIMAGE0_I3_ID, 2, 0 },
			{ TX_SETIMAGE1_I0_ID, TX_SETIMAGE1_I3_ID, 3, 0 },
			{ TX_SETIMAGE2_I0_ID, TX_SETIMAGE2_I3_ID, 4, 0 },
			{ TX_SETIMAGE3_I0_ID, TX_SETIMAGE3_I3_ID, 5, 0 },
			{ TX_SETTLUT_I0_ID,   TX_SETTLUT_I3_ID,   6, 0 },
			{ TX_SETMODE0_I4_ID,  TX_SETMODE0_I7_ID,  0, 4 },
			{ TX_SETMODE1_I4_ID,  TX_SETMODE1_I7_ID,  1, 4 },
			{ TX_SETIMAGE0_I4_ID, TX_SETIMAGE0_I7_ID, 2, 4 },
			{ TX_SETIMAGE1_I4_ID, TX_SETIMAGE1_I7_ID, 3, 4 },
			{ TX_SETIMAGE2_I4_ID, TX_SETIMAGE2_I7_ID, 4, 4 },
			{ TX_SETIMAGE3_I4_ID, TX_SETIMAGE3_I7_ID, 5, 4 },
			{ TX_SETTLUT_I4_ID,   TX_SETTLUT_I7_ID,   6, 4 },
		};

		for (const Range& r : ranges)
		{
			if (index >= r.lo && index <= r.hi)
			{
				// Both blocks are laid out the same way - four maps of seven registers each - but the
				// I4-I7 block at 0xA0 programs the maps 4-7, not 0-3.
				*id = r.base + (int)(index - r.lo);
				*kind = r.kind;
				return true;
			}
		}

		return false;
	}

	void TextureEngine::loadTXReg(size_t index, uint32_t value)
	{
		int id = 0, kind = 0;

		if (DecodeTexIndex(index, &id, &kind))
		{
			TexMap* m = &texMap[id];

			switch (kind)
			{
				case 0: tx.texmode0[id].bits = value; m->paramsDirty = true; break;
				case 1: tx.texmode1[id].bits = value; break;
				case 2:
					tx.teximg0[id].bits = value;
					// The format or the size changed: the image has to be decoded again
					if (m->keyFmt != (int)tx.teximg0[id].fmt ||
						m->keyWidth != (int)(tx.teximg0[id].width + 1) ||
						m->keyHeight != (int)(tx.teximg0[id].height + 1))
						m->dirty = true;
					break;
				case 3: tx.teximg1[id].bits = value; break;
				case 4: tx.teximg2[id].bits = value; break;
				case 5:
					tx.teximg3[id].bits = value;
					// The texture base may point at new data even if the address is unchanged
					m->dirty = true;
					break;
				case 6:
					tx.settlut[id].bits = value;
					m->dirty = true;
					break;
			}

			return;
		}

		switch (index)
		{
			//
			// The global texture registers (gfx-tc.md 4.2). The preload commands (TX_LOADBLOCK and
			// TX_LOADTLUT) fill TMEM on the hardware; this emulator decodes a texture from main memory
			// whenever the draw path needs it, so the block loads have nothing to do - but they are
			// decoded here so that the register file is complete and the debugger can show them.
			//

			case TX_LOADBLOCK0_ID:
			case TX_LOADBLOCK1_ID:
			case TX_LOADBLOCK2_ID:
			case TX_LOADBLOCK3_ID:
				tx.loadblock[index - TX_LOADBLOCK0_ID] = value;
				return;

			case TX_INVTAGS_ID:
				tx.invtags = value;
				return;

			case TX_PERFMODE_ID:
				tx.perfmode = value;
				return;

			case TX_MISC_ID:
				tx.misc = value;
				return;

			case TX_REFRESH_ID:
				tx.refresh = value;
				return;

			case TX_LOADTLUT0_ID:
				tx.loadtlut0.bits = value;
				LoadTlut(tx.loadtlut0.base << 5, tx.loadtlut1.tmem << 9, tx.loadtlut1.count);
				return;

			case TX_LOADTLUT1_ID:
				tx.loadtlut1.bits = value;
				LoadTlut(tx.loadtlut0.base << 5, tx.loadtlut1.tmem << 9, tx.loadtlut1.count);
				return;

			default:
				// The sequence of bypassing blocks for register load is follows: TEV -> Unknown reg load
				gfx->tev->loadTEVReg(index, value);
				return;
		}
	}

	// Decode a texture map and hand the real (unpadded) image back as RGB. The debugger uses it for
	// the `gxtexdump` command; the decoded buffer is the same one the draw path uploads.
	bool TextureEngine::DumpTexture(int id, std::vector<uint8_t>& rgb, int* width, int* height)
	{
		id &= (GFX_MAX_TEXTURES - 1);

		if (!DecodeTexture(id))
		{
			return false;
		}

		const TexMap* m = &texMap[id];

		*width = m->width;
		*height = m->height;

		rgb.resize((size_t)m->width * m->height * 3);

		for (int y = 0; y < m->height; y++)
		{
			for (int x = 0; x < m->width; x++)
			{
				const Color& c = rgbabuf[(size_t)y * m->dw + x];
				uint8_t* p = &rgb[((size_t)y * m->width + x) * 3];
				p[0] = c.R;
				p[1] = c.G;
				p[2] = c.B;
			}
		}

		return true;
	}

	void TextureEngine::Reset()
	{		tx = TXState{};

		for (int i = 0; i < GFX_MAX_TEXTURES; i++)
		{
			TexMap* m = &texMap[i];

			m->valid = false;
			m->dirty = false;
			m->paramsDirty = false;
			m->width = m->height = 0;
			m->dw = m->dh = 0;
			m->ds = m->dt = 1.0f;
			m->keyAddr = 0;
			m->keyFmt = -1;
			m->keyWidth = m->keyHeight = 0;
			m->keyTlut = 0xFFFFFFFF;
			m->appliedMode0 = 0xFFFFFFFF;
			m->appliedMode1 = 0xFFFFFFFF;
		}
	}
}
