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

	// FNV-1a over the raw texture bytes. GXLoadTexObj programs the map of a draw on every draw, so
	// the draw path asks for a decode all the time; the hash is what lets it tell "the same image
	// again" from "a title edited the texels in place" without a byte-by-byte comparison of a
	// working copy it would have to keep.
	static uint64_t HashTextureData(const uint8_t* data, size_t size)
	{
		uint64_t hash = 14695981039346656037ull;

		for (size_t i = 0; i < size; i++)
		{
			hash ^= data[i];
			hash *= 1099511628211ull;
		}

		return hash;
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

		// The decoded image of a paletted texture is a function of the palette bytes as well:
		// remember that the palettes changed so that DecodeTexture converts those maps again even
		// when the texture bytes themselves did not move.
		tlutGeneration++;

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

	// The number of bytes the decoder reads for the image of a map. The tile geometry packs the
	// texels of a format into 32-byte units, but the total is the nominal size of the image.
	size_t TextureEngine::TextureDataSize(int id)
	{
		size_t texels = (size_t)(tx.teximg0[id].width + 1) * (tx.teximg0[id].height + 1);

		switch (tx.teximg0[id].fmt)
		{
			case TF_I4:
			case TF_C4:
			case TF_CMPR:
				return texels / 2;		// 4 bits per texel

			case TF_I8:
			case TF_IA4:
			case TF_C8:
				return texels;			// 8 bits per texel

			case TF_IA8:
			case TF_RGB565:
			case TF_RGB5A3:
			case TF_C14:
				return texels * 2;		// 16 bits per texel

			case TF_RGBA8:
				return texels * 4;		// 32 bits per texel

			default:
				return 0;
		}
	}

	// Decode a texture map on demand. The conversion is skipped while the GL image already holds
	// exactly the image the map describes: the address, the format, the size, the palette binding
	// and the texture bytes (plus the palette bytes, through the TLUT generation) all have to
	// match. This is what keeps a title that programs the same map on every draw from redoing the
	// conversion and the upload.
	TextureEngine::DecodeResult TextureEngine::DecodeTexture(int id)
	{
		TexMap* m = &texMap[id];

		int fmt = tx.teximg0[id].fmt;
		int oldw = tx.teximg0[id].width + 1;
		int oldh = tx.teximg0[id].height + 1;
		uint32_t addr = tx.teximg3[id].base << 5;
		size_t size = TextureDataSize(id);

		if (oldw == 0 || oldh == 0 || size == 0)
			return DecodeResult::Failed;

		// Do not hash an image that could not be decoded in the first place
		if ((size_t)NextPowerOfTwo(oldw) * NextPowerOfTwo(oldh) > _countof(rgbabuf))
			return DecodeResult::Failed;

		const uint8_t* rawData = (const uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForTX(addr);
		if (rawData == nullptr)
			return DecodeResult::Failed;

		uint64_t hash = HashTextureData(rawData, size);

		if (m->valid &&
			m->keyAddr == addr && m->keyFmt == fmt &&
			m->keyWidth == oldw && m->keyHeight == oldh &&
			m->keyTlut == tx.settlut[id].tmem && m->keyTlutGen == tlutGeneration &&
			m->keyHash == hash)
		{
			return DecodeResult::Unchanged;
		}

		if (!ConvertTexture(id))
			return DecodeResult::Failed;

		// Remember what the GL image now holds
		m->keyAddr = addr;
		m->keyFmt = fmt;
		m->keyWidth = oldw;
		m->keyHeight = oldh;
		m->keyTlut = tx.settlut[id].tmem;
		m->keyTlutGen = tlutGeneration;
		m->keyHash = hash;
		m->valid = true;

		return DecodeResult::Decoded;
	}

	// Convert one texture map from main memory into rgbabuf. Returns false if the map is not usable.
	bool TextureEngine::ConvertTexture(int id)
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
				//
				// The tiles of a texture are stored row by row: every tile of the first tile row
				// comes first, then the tiles of the second one, and so on. The tile rows therefore
				// have to be the outer loop; with the tile columns outer the decoder reads the tiles
				// of a column and an image wider than one tile comes out transposed.
				for (t = 0; t < (oldh / 4); t++) // tile ver
					for (s = 0; s < (oldw / 8); s++)  // tile hor
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
								// C14X2 carries a 14-bit index into a palette of up to 16384 entries
								uint16_t idx = _BYTESWAP_UINT16(*ptr++) & 0x3fff;
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
								// The fourth colour of the 3-colour mode is the transparent texel: the
								// hardware stores the average of the endpoints there and only clears
								// its alpha, so the RGB still takes part in a blend that uses it.
								rgb[3].R = (rgb[0].R + rgb[1].R) / 2;
								rgb[3].G = (rgb[0].G + rgb[1].G) / 2;
								rgb[3].B = (rgb[0].B + rgb[1].B) / 2;
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
								// The fourth colour of the 3-colour mode is the transparent texel: the
								// hardware stores the average of the endpoints there and only clears
								// its alpha, so the RGB still takes part in a blend that uses it.
								rgb[3].R = (rgb[0].R + rgb[1].R) / 2;
								rgb[3].G = (rgb[0].G + rgb[1].G) / 2;
								rgb[3].B = (rgb[0].B + rgb[1].B) / 2;
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
								// The fourth colour of the 3-colour mode is the transparent texel: the
								// hardware stores the average of the endpoints there and only clears
								// its alpha, so the RGB still takes part in a blend that uses it.
								rgb[3].R = (rgb[0].R + rgb[1].R) / 2;
								rgb[3].G = (rgb[0].G + rgb[1].G) / 2;
								rgb[3].B = (rgb[0].B + rgb[1].B) / 2;
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
								// The fourth colour of the 3-colour mode is the transparent texel: the
								// hardware stores the average of the endpoints there and only clears
								// its alpha, so the RGB still takes part in a blend that uses it.
								rgb[3].R = (rgb[0].R + rgb[1].R) / 2;
								rgb[3].G = (rgb[0].G + rgb[1].G) / 2;
								rgb[3].B = (rgb[0].B + rgb[1].B) / 2;
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

		return true;
	}

	void TextureEngine::UploadTexture(int id)
	{
		TexMap* m = &texMap[id];

		glBindTexture(GL_TEXTURE_2D, m->glTexture);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

		// A texture object that already has the same size only needs its texels replaced;
		// glTexImage2D would throw the storage away and allocate it again.
		if (m->glWidth == m->dw && m->glHeight == m->dh)
		{
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m->dw, m->dh, GL_RGBA, GL_UNSIGNED_BYTE, rgbabuf);
		}
		else
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m->dw, m->dh, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbabuf);
			m->glWidth = m->dw;
			m->glHeight = m->dh;
		}

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

				switch (DecodeTexture(i))
				{
					case DecodeResult::Decoded:
						UploadTexture(i);
						break;

					case DecodeResult::Unchanged:
						// The GL texture already holds this image
						break;

					default:
						m->valid = false;
						break;
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

		// The software pipeline owns a real TMEM; the array is allocated up front so that the
		// debugger and the tests can look into it at any time.
		SoftTmemInit();
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

	void TextureEngine::loadTXReg(size_t index, uint32_t value, uint32_t mask)
	{
		int id = 0, kind = 0;

		if (DecodeTexIndex(index, &id, &kind))
		{
			TexMap* m = &texMap[id];

			// The map registers honour a BP write mask like every other register of the file (see
			// MergeBpWriteMask).
			switch (kind)
			{
				case 0:
				{
					// GXLoadTexObj programs the mode on every draw, and the sampler parameters
					// (which may rebuild the mip chain) only change with the value.
					uint32_t merged = MergeBpWriteMask(tx.texmode0[id].bits, value, mask);

					if (tx.texmode0[id].bits != merged)
					{
						tx.texmode0[id].bits = merged;
						m->paramsDirty = true;
					}
				}
				break;
				case 1: tx.texmode1[id].bits = MergeBpWriteMask(tx.texmode1[id].bits, value, mask); break;
				case 2:
					tx.teximg0[id].bits = MergeBpWriteMask(tx.teximg0[id].bits, value, mask);
					// The format or the size changed: the image has to be decoded again
					if (m->keyFmt != (int)tx.teximg0[id].fmt ||
						m->keyWidth != (int)(tx.teximg0[id].width + 1) ||
						m->keyHeight != (int)(tx.teximg0[id].height + 1))
						m->dirty = true;
					break;
				case 3: tx.teximg1[id].bits = MergeBpWriteMask(tx.teximg1[id].bits, value, mask); break;
				case 4: tx.teximg2[id].bits = MergeBpWriteMask(tx.teximg2[id].bits, value, mask); break;
				case 5:
					tx.teximg3[id].bits = MergeBpWriteMask(tx.teximg3[id].bits, value, mask);
					// The texture base may point at new data even if the address is unchanged, and
					// a title may edit the texels in place, so the map has to be looked at again.
					// DecodeTexture compares the description and the texture bytes and keeps the
					// decoded image when neither changed.
					m->dirty = true;
					break;
				case 6:
					tx.settlut[id].bits = MergeBpWriteMask(tx.settlut[id].bits, value, mask);
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
			{
				uint32_t& reg = tx.loadblock[index - TX_LOADBLOCK0_ID];
				reg = MergeBpWriteMask(reg, value, mask);

				// The software pipeline owns a real TMEM: the explicit load streamed by these
				// registers is what fills it (gfx-tc.md 3.6). `TX_LOADBLOCK3` carries the count
				// and the format and initiates the load; the shader pipeline decodes textures from
				// main memory on demand and therefore ignores it.
				if (gfx != nullptr && gfx->SoftPipeline() && index == TX_LOADBLOCK3_ID)
				{
					uint32_t base = (tx.loadblock[0] & 0x1FFFFF) << 5;
					uint32_t off0 = tx.loadblock[1] & 0x7FFF;
					uint32_t off1 = tx.loadblock[2] & 0x7FFF;
					uint32_t count = tx.loadblock[3] & 0x7FFF;
					unsigned format = (tx.loadblock[3] >> 15) & 3;

					SoftLoadBlock(base, off0, off1, count, format);
				}
				return;
			}

			case TX_INVTAGS_ID:
				tx.invtags = MergeBpWriteMask(tx.invtags, value, mask);
				// The hardware's "the texture bytes changed" command (GXInvalidateTexAll). The
				// backend decodes from main memory on demand and otherwise keeps the decoded image,
				// so every map has to be decoded again.
				for (int i = 0; i < GFX_MAX_TEXTURES; i++)
					texMap[i].dirty = true;
				return;

			case TX_PERFMODE_ID:
				tx.perfmode = MergeBpWriteMask(tx.perfmode, value, mask);
				return;

			case TX_MISC_ID:
				tx.misc = MergeBpWriteMask(tx.misc, value, mask);
				return;

			case TX_REFRESH_ID:
				tx.refresh = MergeBpWriteMask(tx.refresh, value, mask);
				return;

			case TX_LOADTLUT0_ID:
				tx.loadtlut0.bits = MergeBpWriteMask(tx.loadtlut0.bits, value, mask);
				LoadTlut(tx.loadtlut0.base << 5, tx.loadtlut1.tmem << 9, tx.loadtlut1.count);
				if (gfx != nullptr && gfx->SoftPipeline())
					SoftLoadTlut(tx.loadtlut0.base << 5, tx.loadtlut1.tmem, tx.loadtlut1.count);
				return;

			case TX_LOADTLUT1_ID:
				tx.loadtlut1.bits = MergeBpWriteMask(tx.loadtlut1.bits, value, mask);
				LoadTlut(tx.loadtlut0.base << 5, tx.loadtlut1.tmem << 9, tx.loadtlut1.count);
				if (gfx != nullptr && gfx->SoftPipeline())
					SoftLoadTlut(tx.loadtlut0.base << 5, tx.loadtlut1.tmem, tx.loadtlut1.count);
				return;

			default:
				// The sequence of bypassing blocks for register load is follows: TEV -> Unknown reg load
				gfx->tev->loadTEVReg(index, value, mask);
				return;
		}
	}

	// Decode a texture map and hand the real (unpadded) image back as RGB. The debugger uses it for
	// the `gxtexdump` command; the decoded buffer is the same one the draw path uploads.
	bool TextureEngine::DumpTexture(int id, std::vector<uint8_t>& rgb, int* width, int* height)
	{
		id &= (GFX_MAX_TEXTURES - 1);

		// The dump wants the pixels, not the cache: rgbabuf is shared by all maps and may hold the
		// image of another one, so the map has to be converted into it unconditionally.
		if (!ConvertTexture(id))
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
				// The decoder serialises every texel into the R,G,B,A byte order the GL upload
				// wants, which is the reverse of the Color field order (Color is A,B,G,R). Read
				// the bytes, not the fields: after the serialisation the `R` field holds the
				// alpha, the `G` field the blue one and so on, which is why reading the fields
				// dumped every non-grey format with the channels rotated.
				const uint8_t* c = (const uint8_t*)&rgbabuf[(size_t)y * m->dw + x];
				uint8_t* p = &rgb[((size_t)y * m->width + x) * 3];
				p[0] = c[0];
				p[1] = c[1];
				p[2] = c[2];
			}
		}

		return true;
	}

	void TextureEngine::Reset()
	{		tx = TXState{};
		tlutGeneration = 0;

		SoftTmemInit();

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
			m->keyTlutGen = 0;
			m->keyHash = 0;
			m->appliedMode0 = 0xFFFFFFFF;
			m->appliedMode1 = 0xFFFFFFFF;
		}
	}

	// -------------------------------------------------------------------------------------------
	// The software texture unit (GFX_PIPELINE = soft, issue #384)
	//
	// TMEM is modelled for real: the 32 embedded banks of 16K x 16-bit words of gfx-tc.md 3.1, in
	// the two 512 KB halves that the 15-bit `tmem_offset` fields address. A 32-byte cache line is
	// written through the sixteen banks of one half (2.4), the explicit load commands stream
	// main-memory tiles into it (3.6) and a hardware-managed ("cached") image is fetched through
	// the tag cache described in 3.5.
	//
	// The sampler then follows gfx-tc.md 3.3/3.4 (the level of detail from the texel/pixel ratio,
	// the bias and the LOD limits, the clamp/repeat/mirror coordinate operations) and the filter
	// datapath of gfx-tf.md 3 (the format expansion of 5.2, the bilinear S/T lerps of 3.4 with
	// 6-bit fractions, the trilinear blend of 3.5 with a 5-bit fraction and the TLUT dereference
	// of the colour-index formats in 3.7).
	// -------------------------------------------------------------------------------------------

	//! Words per TMEM bank (16K words, gfx-tc.md 3.1)
	static const uint32_t SoftTmemWords = 16384;
	//! Lines of the two halves: a half is 512 KB = 16384 lines of 32 bytes.
	static const uint32_t SoftTmemHalfLines = 16384;
	//! Where the software model keeps the tag cache of the hardware-managed images: 128 KB at the
	//! top of the low half.
	static const uint32_t SoftCacheBase = SoftTmemHalfLines - 4096;
	static const uint32_t SoftCacheLines = 4096;

	void TextureEngine::SoftTmemInit()
	{
		if (tmem.size() != (size_t)SoftTmemBankCount * SoftTmemWords)
			tmem.assign((size_t)SoftTmemBankCount * SoftTmemWords, 0);
		else
			memset(tmem.data(), 0, tmem.size() * sizeof(uint16_t));

		for (int i = 0; i < GFX_MAX_TEXTURES; i++)
		{
			for (size_t s = 0; s < _countof(softCacheTags[i]); s++)
			{
				softCacheTags[i][s].line = 0xFFFFFFFF;
				softCacheTags[i][s].valid = false;
			}
		}
	}

	uint16_t& TextureEngine::SoftTmemWord(uint32_t line, int bank)
	{
		// A line address beyond the low half addresses the upper half of the array (gfx-tc.md 3.1)
		if (line >= SoftTmemHalfLines)
		{
			line -= SoftTmemHalfLines;
			bank += 16;
		}

		bank &= 31;
		return tmem[(size_t)bank * SoftTmemWords + (line & (SoftTmemWords - 1))];
	}

	void TextureEngine::SoftWriteLine(uint32_t line, const uint8_t* data)
	{
		for (int k = 0; k < 16; k++)
			SoftTmemWord(line, k) = (uint16_t)(((uint16_t)data[k * 2] << 8) | data[k * 2 + 1]);
	}

	void TextureEngine::SoftLoadBlock(uint32_t base, uint32_t off0, uint32_t off1, uint32_t count,
		unsigned format)
	{
		if (tmem.empty())
			SoftTmemInit();

		if (count == 0)
			return;

		if (count > SoftTmemHalfLines)
			count = SoftTmemHalfLines;

		const uint8_t* src = (const uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForPI(base);
		if (src == nullptr)
			return;

		// gfx-tc.md 4.2: a 32-bit texel needs a pair of cache lines (the AR and the GB half), which
		// the two TMEM offsets name; every other format streams one line per count.
		if (format == 3)
		{
			for (uint32_t i = 0; i < count; i++)
			{
				SoftWriteLine((off0 + i) & 0x7FFF, src + (size_t)i * 64);
				SoftWriteLine((off1 + i) & 0x7FFF, src + (size_t)i * 64 + 32);
			}
		}
		else
		{
			for (uint32_t i = 0; i < count; i++)
			{
				SoftWriteLine((off0 + i) & 0x7FFF, src + (size_t)i * 32);
			}
		}
	}

	void TextureEngine::SoftLoadTlut(uint32_t base, uint32_t tmemOffset, uint32_t count)
	{
		if (tmem.empty())
			SoftTmemInit();

		if (count == 0)
			return;

		if (count > 1024)
			count = 1024;

		const uint8_t* src = (const uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForPI(base);
		if (src == nullptr)
			return;

		// TLUTs live in the upper half, addressed in 512-byte units (gfx-tc.md 3.1, 4.2). Each
		// main-memory fetch carries sixteen 16-bit entries; the write path replicates every entry
		// across the sixteen banks of the half so that sixteen indices can be dereferenced in one
		// cycle (3.7).
		uint32_t wordBase = (tmemOffset & 0x3FF) * 256;

		for (uint32_t b = 0; b < count; b++)
		{
			const uint8_t* p = src + (size_t)b * 32;

			for (int k = 0; k < 16; k++)
			{
				uint16_t entry = (uint16_t)(((uint16_t)p[k * 2] << 8) | p[k * 2 + 1]);
				uint32_t word = (wordBase + b * 16 + k) & (SoftTmemWords - 1);

				for (int bank = 0; bank < 16; bank++)
					SoftTmemWord(SoftTmemHalfLines + word, bank) = entry;
			}
		}
	}

	//! The width and the height of a 32-byte texture tile, per texel size (gfx-tc.md 5.3).
	static void SoftTileShape(int fmt, int* tileW, int* tileH)
	{
		switch (fmt)
		{
			case TF_I4:
			case TF_C4:
			case TF_CMPR:
				*tileW = 8; *tileH = 8;
				break;

			case TF_I8:
			case TF_IA4:
			case TF_C8:
				*tileW = 8; *tileH = 4;
				break;

			default:
				*tileW = 4; *tileH = 4;
				break;
		}
	}

	//! The number of 32-byte lines one mip level of an image occupies (a 32-bit texel needs two
	//! lines per tile: the AR and the GB half).
	static uint32_t SoftLevelLines(int fmt, int width, int height)
	{
		if (width < 1) width = 1;
		if (height < 1) height = 1;

		int tileW, tileH;
		SoftTileShape(fmt, &tileW, &tileH);

		uint32_t tilesX = ((uint32_t)width + tileW - 1) / tileW;
		uint32_t tilesY = ((uint32_t)height + tileH - 1) / tileH;

		return tilesX * tilesY;
	}

	bool TextureEngine::SoftTileLine(int map, int level, uint32_t tile, bool gb, uint32_t* line)
	{
		int fmt = tx.teximg0[map].fmt;
		int width = tx.teximg0[map].width + 1;
		int height = tx.teximg0[map].height + 1;

		if (width <= 0 || height <= 0)
			return false;

		if (level < 0)
			level = 0;

		if (tx.teximg1[map].image_type != 0)
		{
			// A software-managed ("pre-loaded") image: the tiles the load commands streamed are in
			// TMEM in the main-memory tile order (gfx-tc.md 5.3), so the line of a tile is the load
			// destination plus the number of tiles of the finer levels. The two offsets hold the
			// even and odd mip levels, or the AR and the GB half of a 32-bit image.
			uint32_t base;

			if (fmt == TF_RGBA8)
			{
				base = (gb ? (uint32_t)tx.teximg2[map].tmem_offset : (uint32_t)tx.teximg1[map].tmem_offset) & 0x7FFF;
			}
			else
			{
				base = ((level & 1) ? (uint32_t)tx.teximg2[map].tmem_offset : (uint32_t)tx.teximg1[map].tmem_offset) & 0x7FFF;

				for (int j = (level & 1); j < level; j += 2)
				{
					int lw = (width >> j) > 0 ? (width >> j) : 1;
					int lh = (height >> j) > 0 ? (height >> j) : 1;
					base += SoftLevelLines(fmt, lw, lh);
				}
			}

			*line = (base + tile) & 0x7FFF;
			return true;
		}

		// A hardware-managed ("cached") image: the tile is fetched from main memory through the tag
		// cache (gfx-tc.md 3.5). The software model keeps one shared cache of 128 KB in TMEM; the
		// tag of a slot is the main-memory line it holds, so two images that happen to share a slot
		// never read each other's data.
		uint32_t memBase = (uint32_t)tx.teximg3[map].base << 5;
		uint32_t levelOffset = 0;

		for (int j = 0; j < level; j++)
		{
			int lw = (width >> j) > 0 ? (width >> j) : 1;
			int lh = (height >> j) > 0 ? (height >> j) : 1;
			levelOffset += SoftLevelLines(fmt, lw, lh) * 32;
		}

		uint32_t byteAddr = memBase + levelOffset + tile * 32 * (fmt == TF_RGBA8 ? 2 : 1) + (gb ? 32 : 0);
		uint32_t memLine = byteAddr >> 5;

		uint32_t slot = (memLine ^ (uint32_t)(level * 0x9E3779B1u)) % SoftCacheLines;
		SoftCacheTag* tag = &softCacheTags[map][slot & (_countof(softCacheTags[map]) - 1)];

		if (!tag->valid || tag->line != memLine)
		{
			const uint8_t* src = (const uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForPI(byteAddr);
			if (src == nullptr)
				return false;

			SoftWriteLine(SoftCacheBase + slot, src);

			tag->valid = true;
			tag->line = memLine;
		}

		*line = SoftCacheBase + slot;
		return true;
	}

	//! One byte of a 32-byte TMEM line (the bytes of a line are the sixteen bank words, big-endian).
	uint8_t TextureEngine::SoftLineByte(uint32_t line, int index)
	{
		uint16_t word = SoftTmemWord(line, index >> 1);
		return (index & 1) ? (uint8_t)(word & 0xFF) : (uint8_t)(word >> 8);
	}

	//! Expand the two 5/6/5 channels of a packed RGB565 word (gfx-tf.md 5.2).
	static void SoftRgb565(uint16_t p, float* r, float* g, float* b)
	{
		int r5 = (p >> 11) & 0x1F;
		int g6 = (p >> 5) & 0x3F;
		int b5 = p & 0x1F;

		*r = (float)((r5 << 3) | (r5 >> 2));
		*g = (float)((g6 << 2) | (g6 >> 4));
		*b = (float)((b5 << 3) | (b5 >> 2));
	}

	//! Decode a colour-index texel against the palette (gfx-tf.md 3.7, gfx-tc.md 5.4).
	bool TextureEngine::SoftTlutEntry(int map, uint32_t index, float rgba[4])
	{
		if (tmem.empty())
			SoftTmemInit();

		if (index > 16383u)
			return false;

		uint32_t base = (uint32_t)tx.settlut[map].tmem & 0x3FF;
		uint32_t word = (base * 256 + index) & (SoftTmemWords - 1);

		uint16_t entry = SoftTmemWord(SoftTmemHalfLines + word, 0);
		unsigned tlutFmt = tx.settlut[map].fmt;

		if (tlutFmt == TLUT_RGB565)
		{
			SoftRgb565(entry, &rgba[0], &rgba[1], &rgba[2]);
			rgba[3] = 255.0f;
			return true;
		}

		if (tlutFmt == TLUT_RGB5A3)
		{
			if (entry & 0x8000)
			{
				SoftRgb565(entry, &rgba[0], &rgba[1], &rgba[2]);
				rgba[3] = 255.0f;
			}
			else
			{
				int r4 = (entry >> 8) & 0xF;
				int g4 = (entry >> 4) & 0xF;
				int b4 = entry & 0xF;
				int a3 = (entry >> 12) & 0x7;

				rgba[0] = (float)((r4 << 4) | r4);
				rgba[1] = (float)((g4 << 4) | g4);
				rgba[2] = (float)((b4 << 4) | b4);
				rgba[3] = (float)((a3 << 5) | (a3 << 2) | (a3 >> 1));
			}
			return true;
		}

		// The default entry format is IA8: intensity in the low byte, alpha in the high one
		rgba[3] = (float)(entry >> 8);
		rgba[0] = rgba[1] = rgba[2] = (float)(entry & 0xFF);
		return true;
	}

	//! The colour of one texel of an image, read out of TMEM and expanded per gfx-tf.md 5.2.
	bool TextureEngine::SoftFetchTexel(int map, int level, int u, int v, float rgba[4])
	{
		int fmt = tx.teximg0[map].fmt;
		int width = tx.teximg0[map].width + 1;
		int height = tx.teximg0[map].height + 1;

		int lw = width >> level;
		int lh = height >> level;
		if (lw < 1) lw = 1;
		if (lh < 1) lh = 1;

		if (u < 0) u = 0;
		if (v < 0) v = 0;
		if (u >= lw) u = lw - 1;
		if (v >= lh) v = lh - 1;

		int tileW, tileH;
		SoftTileShape(fmt, &tileW, &tileH);

		uint32_t tilesX = ((uint32_t)lw + tileW - 1) / tileW;
		uint32_t tile = ((uint32_t)v / tileH) * tilesX + ((uint32_t)u / tileW);

		int tx0 = u % tileW;
		int ty0 = v % tileH;

		uint32_t lineAR = 0, lineGB = 0;
		if (!SoftTileLine(map, level, tile, false, &lineAR))
			return false;

		if (fmt == TF_RGBA8 && !SoftTileLine(map, level, tile, true, &lineGB))
			return false;

		rgba[0] = rgba[1] = rgba[2] = rgba[3] = 255.0f;

		switch (fmt)
		{
			case TF_I4:
			{
				uint8_t b = SoftLineByte(lineAR, ty0 * 4 + tx0 / 2);
				int v4 = (tx0 & 1) ? (b & 0xF) : (b >> 4);
				rgba[0] = rgba[1] = rgba[2] = rgba[3] = (float)(v4 * 17);
				return true;
			}

			case TF_I8:
			{
				uint8_t b = SoftLineByte(lineAR, ty0 * 8 + tx0);
				rgba[0] = rgba[1] = rgba[2] = rgba[3] = (float)b;
				return true;
			}

			case TF_IA4:
			{
				uint8_t b = SoftLineByte(lineAR, ty0 * 8 + tx0);
				float i = (float)((b & 0xF) * 17);
				float a = (float)((b >> 4) * 17);
				rgba[0] = rgba[1] = rgba[2] = i;
				rgba[3] = a;
				return true;
			}

			case TF_IA8:
			{
				int idx = (ty0 * 4 + tx0) * 2;
				rgba[3] = (float)SoftLineByte(lineAR, idx + 0);
				rgba[0] = rgba[1] = rgba[2] = (float)SoftLineByte(lineAR, idx + 1);
				return true;
			}

			case TF_RGB565:
			{
				int idx = (ty0 * 4 + tx0) * 2;
				uint16_t p = (uint16_t)(((uint16_t)SoftLineByte(lineAR, idx) << 8) | SoftLineByte(lineAR, idx + 1));
				SoftRgb565(p, &rgba[0], &rgba[1], &rgba[2]);
				rgba[3] = 255.0f;
				return true;
			}

			case TF_RGB5A3:
			{
				int idx = (ty0 * 4 + tx0) * 2;
				uint16_t p = (uint16_t)(((uint16_t)SoftLineByte(lineAR, idx) << 8) | SoftLineByte(lineAR, idx + 1));

				if (p & 0x8000)
				{
					SoftRgb565(p, &rgba[0], &rgba[1], &rgba[2]);
					rgba[3] = 255.0f;
				}
				else
				{
					int r4 = (p >> 8) & 0xF;
					int g4 = (p >> 4) & 0xF;
					int b4 = p & 0xF;
					int a3 = (p >> 12) & 0x7;

					rgba[0] = (float)((r4 << 4) | r4);
					rgba[1] = (float)((g4 << 4) | g4);
					rgba[2] = (float)((b4 << 4) | b4);
					rgba[3] = (float)((a3 << 5) | (a3 << 2) | (a3 >> 1));
				}
				return true;
			}

			case TF_RGBA8:
			{
				int idx = (ty0 * 4 + tx0) * 2;

				// The AR word: alpha in the high byte, red in the low one; the GB word: green and
				// blue (gfx-tf.md 5.2)
				rgba[3] = (float)SoftLineByte(lineAR, idx + 0);
				rgba[0] = (float)SoftLineByte(lineAR, idx + 1);
				rgba[1] = (float)SoftLineByte(lineGB, idx + 0);
				rgba[2] = (float)SoftLineByte(lineGB, idx + 1);
				return true;
			}

			case TF_C4:
			case TF_C8:
			case TF_C14:
			{
				uint32_t index;

				if (fmt == TF_C4)
				{
					uint8_t b = SoftLineByte(lineAR, ty0 * 4 + tx0 / 2);
					index = (tx0 & 1) ? (uint32_t)(b & 0xF) : (uint32_t)(b >> 4);
				}
				else if (fmt == TF_C8)
				{
					index = (uint32_t)SoftLineByte(lineAR, ty0 * 8 + tx0);
				}
				else
				{
					int idx = (ty0 * 4 + tx0) * 2;
					index = (uint32_t)(((uint16_t)SoftLineByte(lineAR, idx) << 8) | SoftLineByte(lineAR, idx + 1)) & 0x3FFFu;
				}

				return SoftTlutEntry(map, index, rgba);
			}

			case TF_CMPR:
			{
				// The 8x8 tile holds four 4x4 sub-blocks in the order TL, TR, BL, BR (gfx-tc.md 5.5)
				int sub = (ty0 >> 2) * 2 + (tx0 >> 2);
				int base = sub * 8;

				uint16_t c0 = (uint16_t)(((uint16_t)SoftLineByte(lineAR, base + 0) << 8) | SoftLineByte(lineAR, base + 1));
				uint16_t c1 = (uint16_t)(((uint16_t)SoftLineByte(lineAR, base + 2) << 8) | SoftLineByte(lineAR, base + 3));

				float col[4][4];
				SoftRgb565(c0, &col[0][0], &col[0][1], &col[0][2]);
				SoftRgb565(c1, &col[1][0], &col[1][1], &col[1][2]);
				col[0][3] = col[1][3] = 255.0f;

				if (c0 > c1)
				{
					for (int i = 0; i < 3; i++)
					{
						col[2][i] = (2.0f * col[0][i] + col[1][i]) / 3.0f;
						col[3][i] = (2.0f * col[1][i] + col[0][i]) / 3.0f;
					}
					col[2][3] = col[3][3] = 255.0f;
				}
				else
				{
					for (int i = 0; i < 3; i++)
					{
						col[2][i] = (col[0][i] + col[1][i]) * 0.5f;
						col[3][i] = (col[0][i] + col[1][i]) * 0.5f;
					}
					col[2][3] = 255.0f;
					col[3][3] = 0.0f;
				}

				uint8_t row = SoftLineByte(lineAR, base + 4 + (ty0 & 3));
				int pi = (row >> (6 - 2 * (tx0 & 3))) & 3;

				for (int i = 0; i < 4; i++)
					rgba[i] = col[pi][i];

				return true;
			}

			default:
				return false;
		}
	}

	//! The coordinate operations of gfx-tc.md 3.4: clamp to the edge, repeat (modulo the size) or
	//! mirror (repeat with the coordinate one's-complemented on alternate wraps).
	static int SoftWrapCoord(int c, int size, unsigned mode)
	{
		if (size <= 0)
			return 0;

		switch (mode & 3)
		{
			case TX_WRAP_REPEAT:
			{
				int m = c % size;
				return (m < 0) ? (m + size) : m;
			}

			case TX_WRAP_MIRROR:
			{
				int period = size * 2;
				int m = c % period;
				if (m < 0) m += period;
				return (m < size) ? m : (period - 1 - m);
			}

			default:
				return (c < 0) ? 0 : ((c >= size) ? (size - 1) : c);
		}
	}

	//! Sample one texture map at a coordinate that is already in texels.
	bool TextureEngine::SoftSampleTexel(int map, float us, float vt, const float* deriv,
		float rgba[4])
	{
		map &= 7;

		if (tmem.empty())
			SoftTmemInit();

		int width = tx.teximg0[map].width + 1;
		int height = tx.teximg0[map].height + 1;

		if (width <= 0 || height <= 0)
			return false;

		// ---- level of detail (gfx-tc.md 3.3) ----
		//
		// The texel-to-pixel ratio across the quad: the largest of the screen-space derivatives of
		// the coordinate, converted to a level by the log2. The hardware computes it once per quad;
		// the software model computes it for the sample it is shading.
		float dsdx = fabsf(deriv[0]);
		float dtdx = fabsf(deriv[1]);
		float dsdy = fabsf(deriv[2]);
		float dtdy = fabsf(deriv[3]);

		float rho = dsdx;
		if (dtdx > rho) rho = dtdx;
		if (dsdy > rho) rho = dsdy;
		if (dtdy > rho) rho = dtdy;

		float lod = (rho > 0.0f) ? log2f(rho) : 0.0f;

		// The bias is an s2.5 value (GX_InitTexObjLOD stores 32*lodbias)
		lod += (float)(int8_t)(tx.texmode0[map].lodbias & 0xFF) / 32.0f;

		float minlod = (float)tx.texmode1[map].minlod / 16.0f;
		float maxlod = (float)tx.texmode1[map].maxlod / 16.0f;
		if (minlod > maxlod)
		{
			float sw = minlod; minlod = maxlod; maxlod = sw;
		}

		if (lod < minlod) lod = minlod;
		if (lod > maxlod) lod = maxlod;

		// ---- the filter pair (gfx-tc.md 4.3, gfx-tf.md 3.1) ----

		bool magnify = (lod <= 0.0f);
		bool linear;
		bool mipmap;
		bool trilerp;

		if (magnify)
		{
			linear = (tx.texmode0[map].mag_filter != 0);
			mipmap = false;
			trilerp = false;
		}
		else
		{
			switch (tx.texmode0[map].min_filter & 7)
			{
				case 0: linear = false; mipmap = false; trilerp = false; break;	// nearest
				case 1: linear = false; mipmap = true;  trilerp = false; break;	// nearest mip nearest
				case 2: linear = false; mipmap = true;  trilerp = true;  break;	// nearest mip linear
				case 5: linear = true;  mipmap = true;  trilerp = false; break;	// linear mip nearest
				case 6: linear = true;  mipmap = true;  trilerp = true;  break;	// linear mip linear
				default: linear = true; mipmap = false; trilerp = false; break;	// linear (planar)
			}
		}

		// The number of levels the image has (down to a 1x1 level)
		int levels = 1;
		{
			int m = (width > height) ? width : height;
			while (m > 1) { m >>= 1; levels++; }
		}

		int level0 = 0;
		float lfrac = 0.0f;

		if (trilerp)
		{
			level0 = (int)floorf(lod);
			lfrac = lod - (float)level0;
		}
		else if (mipmap)
		{
			level0 = (int)floorf(lod + 0.5f);
		}

		if (level0 < 0) { level0 = 0; lfrac = 0.0f; }
		if (level0 > levels - 1) { level0 = levels - 1; lfrac = 0.0f; }

		int level1 = level0 + 1;
		if (level1 > levels - 1)
		{
			level1 = level0;
			lfrac = 0.0f;
		}

		// ---- the bilinear unit (gfx-tf.md 3.4) ----
		//
		// Two S lerps and one T lerp, each an unsigned multiply-add with a 6-bit fraction; the
		// result of an S lerp is cut back to 8 bits before the T lerp.

		auto lerp8 = [](int a, int b, int frac) -> int
		{
			return a + (int)(((b - a) * frac) / 64);
		};

		auto bilinear = [&](int level, float* out) -> bool
		{
			int lw = width >> level;
			int lh = height >> level;
			if (lw < 1) lw = 1;
			if (lh < 1) lh = 1;

			float u = us / (float)(1 << level);
			float v = vt / (float)(1 << level);

			// "The 1/2-texel offset needed to align linear filtering is subtracted" (gfx-tc.md 3.4)
			if (linear)
			{
				u -= 0.5f;
				v -= 0.5f;
			}

			int u0 = (int)floorf(u);
			int v0 = (int)floorf(v);
			float fu = u - (float)u0;
			float fv = v - (float)v0;

			int fracS = linear ? (int)(fu * 64.0f) : 0;
			int fracT = linear ? (int)(fv * 64.0f) : 0;

			float texel[4][4];
			int uc[2] = { u0, u0 + 1 };
			int vc[2] = { v0, v0 + 1 };

			for (int j = 0; j < 2; j++)
			{
				int vv = SoftWrapCoord(vc[j], lh, tx.texmode0[map].wrap_t);

				for (int i = 0; i < 2; i++)
				{
					int uu = SoftWrapCoord(uc[i], lw, tx.texmode0[map].wrap_s);

					if (!SoftFetchTexel(map, level, uu, vv, texel[j * 2 + i]))
						return false;
				}
			}

			for (int c = 0; c < 4; c++)
			{
				int top = lerp8((int)(texel[0][c] + 0.5f), (int)(texel[1][c] + 0.5f), fracS);
				int bottom = lerp8((int)(texel[2][c] + 0.5f), (int)(texel[3][c] + 0.5f), fracS);
				int value = lerp8(top, bottom, fracT);

				out[c] = (float)((value < 0) ? 0 : ((value > 255) ? 255 : value));
			}

			return true;
		};

		float lo[4], hi[4];

		if (!bilinear(level0, lo))
			return false;

		if (level1 != level0)
		{
			if (!bilinear(level1, hi))
			{
				for (int i = 0; i < 4; i++)
					rgba[i] = lo[i];
				return true;
			}

			// The trilinear blend uses the 5-bit LOD fraction (gfx-tf.md 3.5)
			int lf = (int)(lfrac * 32.0f + 0.5f);
			if (lf < 0) lf = 0;
			if (lf > 32) lf = 32;

			for (int i = 0; i < 4; i++)
			{
				float value = (hi[i] * (float)lf + lo[i] * (float)(32 - lf)) / 32.0f;
				rgba[i] = (value < 0.0f) ? 0.0f : ((value > 255.0f) ? 255.0f : value);
			}

			return true;
		}

		for (int i = 0; i < 4; i++)
			rgba[i] = lo[i];

		return true;
	}

	bool TextureEngine::SoftSample(int map, int coordIndex, float s, float t, const float* deriv,
		float rgba[4])
	{
		map &= 7;

		int width = tx.teximg0[map].width + 1;
		int height = tx.teximg0[map].height + 1;

		if (width <= 0 || height <= 0)
			return false;

		// The coordinate scale of the setup unit: the SU_SSIZE/SU_TSIZE pair holds the size of the
		// texture minus one when the API programmed it, and the real size is the automatic scale
		// otherwise (see SetupUnit::CoordScale).
		float ss = 0.0f, ts = 0.0f;
		if (gfx != nullptr)
			gfx->su->CoordScale(coordIndex, &ss, &ts);

		if (ss <= 0.0f) ss = (float)width;
		if (ts <= 0.0f) ts = (float)height;

		float d[4] = { deriv[0] * ss, deriv[1] * ts, deriv[2] * ss, deriv[3] * ts };

		return SoftSampleTexel(map, s * ss, t * ts, d, rgba);
	}
}
