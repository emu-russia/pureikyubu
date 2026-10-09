// The DVD banner image. The module description is in banner.h.

#include "pch.h"
#include "banner.h"

BannerRgba BannerToRgba(const uint8_t* image)
{
	const int tiles = (DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT) / 16;

	BannerRgba decoded;
	decoded.pixels.resize(DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT * 4);

	const uint16_t* tile = (const uint16_t*)image;
	int row = 0, col = 0;

	for (int i = 0; i < tiles; i++, tile += 16)
	{
		for (int j = 0; j < 4; j++)
		{
			for (int k = 0; k < 4; k++)
			{
				uint16_t p = tile[j * 4 + k];
				p = (uint16_t)((p << 8) | (p >> 8));    // the banner is always big-endian

				uint8_t r, g, b, a;

				if (p & 0x8000)                 // RGB555: opaque
				{
					r = (uint8_t)(((p >> 10) & 0x1f) * 255 / 31);
					g = (uint8_t)(((p >> 5) & 0x1f) * 255 / 31);
					b = (uint8_t)((p & 0x1f) * 255 / 31);
					a = 255;
				}
				else                            // RGB4A3: 4 bits of a colour, 3 bits of alpha
				{
					r = (uint8_t)(((p >> 8) & 0x0f) * 17);
					g = (uint8_t)(((p >> 4) & 0x0f) * 17);
					b = (uint8_t)((p & 0x0f) * 17);
					a = (uint8_t)(((p >> 12) & 0x07) * 255 / 7);
				}

				uint8_t* texel = &decoded.pixels[((size_t)(row + j) * DVD_BANNER_WIDTH + (col + k)) * 4];

				texel[0] = r;
				texel[1] = g;
				texel[2] = b;
				texel[3] = a;
			}
		}

		col += 4;
		if (col == DVD_BANNER_WIDTH)
		{
			col = 0;
			row += 4;
		}
	}

	return decoded;
}

void BannerCompositeBackground(BannerRgba& image, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	if (a == 0)
	{
		return;         // nothing to paint over with
	}

	// The two layers are put together the way their alpha says: the texel is over, the colour is
	// under, and what comes out keeps the alpha that neither of them filled. With an opaque colour
	// that alpha is 255 for every texel, which is the whole point of the setting (issue #112).
	//
	// What comes out is stored premultiplied: the colour of the texel at the part of it that is
	// there, plus the colour of the fill at the part of the fill that got through - which is what a
	// renderer that blends the picture with the row behind it wants (see banner.h). The alpha of
	// the result is the two parts together, every weight is in 0..255, and the arithmetic stays in
	// 32 bits.
	for (size_t i = 0; i < image.pixels.size(); i += 4)
	{
		uint8_t* texel = &image.pixels[i];

		const uint32_t over = texel[3];                     // how much of the texel is there
		const uint32_t under = (uint32_t)a * (255 - over) / 255;    // ... and of the fill behind it

		if (over == 255)
		{
			continue;   // an opaque texel keeps its colour
		}

		texel[0] = (uint8_t)((texel[0] * over + r * under) / 255);
		texel[1] = (uint8_t)((texel[1] * over + g * under) / 255);
		texel[2] = (uint8_t)((texel[2] * over + b * under) / 255);
		texel[3] = (uint8_t)(over + under);
	}
}
