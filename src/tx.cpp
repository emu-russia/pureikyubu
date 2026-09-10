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

				for (s = 0; s < (oldw / 4); s++)  // tile hor
					for (t = 0; t < (oldh / 8); t++) // tile ver
						for (u = 0; u < 4; u++)  // texel hor
							for (v = 0; v < 8; v++)  // texel ver
							{
								int ofs = width * (u + 4 * s) + 8 * t + v;
								texbuf[ofs].R =
								texbuf[ofs].G =
								texbuf[ofs].B = *ptr << 4;
								texbuf[ofs].A = *ptr >> 4;
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
									texbuf[ofs].A = a | (a << 3) | ((a << 9) & 3);
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
				Color rgb[4];   // color look-up
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
							if (blk.rgb0 > blk.rgb1)
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
								rgb[3].R = (2 * rgb[1].R + rgb[0].R) / 3;
								rgb[3].G = (2 * rgb[1].G + rgb[0].G) / 3;
								rgb[3].B = (2 * rgb[1].B + rgb[0].B) / 3;
								rgb[3].A = (2 * rgb[1].A + rgb[0].A) / 3;
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

							if (blk.rgb0 > blk.rgb1)
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
								rgb[3].R = (2 * rgb[1].R + rgb[0].R) / 3;
								rgb[3].G = (2 * rgb[1].G + rgb[0].G) / 3;
								rgb[3].B = (2 * rgb[1].B + rgb[0].B) / 3;
								rgb[3].A = (2 * rgb[1].A + rgb[0].A) / 3;
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

							if (blk.rgb0 > blk.rgb1)
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
								rgb[3].R = (2 * rgb[1].R + rgb[0].R) / 3;
								rgb[3].G = (2 * rgb[1].G + rgb[0].G) / 3;
								rgb[3].B = (2 * rgb[1].B + rgb[0].B) / 3;
								rgb[3].A = (2 * rgb[1].A + rgb[0].A) / 3;
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

							if (blk.rgb0 > blk.rgb1)
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
								rgb[3].R = (2 * rgb[1].R + rgb[0].R) / 3;
								rgb[3].G = (2 * rgb[1].G + rgb[0].G) / 3;
								rgb[3].B = (2 * rgb[1].B + rgb[0].B) / 3;
								rgb[3].A = (2 * rgb[1].A + rgb[0].A) / 3;
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

		m->appliedMode0 = mode.bits;
		m->paramsDirty = false;
	}

	void TextureEngine::UpdateAndBindTextures()
	{
		for (int i = 0; i < GFX_MAX_TEXTURES; i++)
		{
			TexMap* m = &texMap[i];

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

			if (m->valid && (m->paramsDirty || m->appliedMode0 != tx.texmode0[i].bits))
				ApplyTextureParams(i);

			glActiveTexture(GL_TEXTURE0 + i);

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

		for (int i = 0; i < GFX_MAX_TEXTURES; i++)
		{
			scale[i][0] = texMap[i].valid ? texMap[i].ds : 1.0f;
			scale[i][1] = texMap[i].valid ? texMap[i].dt : 1.0f;
		}

		glUniform2fv(program.Uniform("texScale[0]"), 8, (float*)scale);
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
		struct Range { size_t lo, hi; int kind; };
		static const Range ranges[] = {
			{ TX_SETMODE0_I0_ID,  TX_SETMODE0_I3_ID,  0 },
			{ TX_SETMODE1_I0_ID,  TX_SETMODE1_I3_ID,  1 },
			{ TX_SETIMAGE0_I0_ID, TX_SETIMAGE0_I3_ID, 2 },
			{ TX_SETIMAGE1_I0_ID, TX_SETIMAGE1_I3_ID, 3 },
			{ TX_SETIMAGE2_I0_ID, TX_SETIMAGE2_I3_ID, 4 },
			{ TX_SETIMAGE3_I0_ID, TX_SETIMAGE3_I3_ID, 5 },
			{ TX_SETTLUT_I0_ID,   TX_SETTLUT_I3_ID,   6 },
			{ TX_SETMODE0_I4_ID,  TX_SETMODE0_I7_ID,  0 },
			{ TX_SETMODE1_I4_ID,  TX_SETMODE1_I7_ID,  1 },
			{ TX_SETIMAGE0_I4_ID, TX_SETIMAGE0_I7_ID, 2 },
			{ TX_SETIMAGE1_I4_ID, TX_SETIMAGE1_I7_ID, 3 },
			{ TX_SETIMAGE2_I4_ID, TX_SETIMAGE2_I7_ID, 4 },
			{ TX_SETIMAGE3_I4_ID, TX_SETIMAGE3_I7_ID, 5 },
			{ TX_SETTLUT_I4_ID,   TX_SETTLUT_I7_ID,   6 },
		};

		for (const Range& r : ranges)
		{
			if (index >= r.lo && index <= r.hi)
			{
				// Both the I0-I3 block (0x80) and the I4-I7 block (0xA0) are laid out identically,
				// so the map id is simply the offset inside the block.
				*id = (int)(index - r.lo);
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
			case TX_LOADTLUT0_ID:
				tx.loadtlut0.bits = value;
				LoadTlut(tx.loadtlut0.base << 5, tx.loadtlut1.tmem << 9, tx.loadtlut1.count);
				return;

			case TX_LOADTLUT1_ID:
				tx.loadtlut1.bits = value;
				LoadTlut(tx.loadtlut0.base << 5, tx.loadtlut1.tmem << 9, tx.loadtlut1.count);
				return;

			default:
				// The sequence of bypassing blocks for register load is as follows: TEV -> Unknown reg load
				gfx->tev->loadTEVReg(index, value);
				return;
		}
	}
}
