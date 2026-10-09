/*

# The DVD banner image

The picture of a disc (`opening.bnr`) is an RGB5A3 texture laid out in 4x4 tiles, the way every GX
texture is, and it is the only thing the game selector shows of a disc besides its title. The module
is the two steps of turning those bytes into pixels of the format the front end's renderer takes:

  * `BannerToRgba` expands the texture and keeps the alpha channel of every texel - the same
    expansion the renderer's own upload used to do inline;
  * `BannerCompositeBackground` paints one colour under those pixels, which is what a user who does
    not want the transparency of the banner (issue #112) gets.

The two are separate because the selector keeps the decoded picture and only re-composites it when
the background setting changes, and because each of them is a pure function of its arguments: the
whole module is a function of the banner bytes, so it can be checked without a window (see
testing/banner_test.cpp).

*/

#pragma once

//! The RGB5A3 texture of a DVD banner, decoded to 4 bytes per texel: R, G, B, then the alpha the
//! format carries (255 where the texel is an opaque RGB555 one).
struct BannerRgba
{
	std::vector<uint8_t> pixels;    //!< DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT texels, 4 bytes each
};

//! Decode the `image` of a DVD banner (DVDBanner2::image, `2 * DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT`
//! bytes) into an RGBA picture. The colours are the ones the texture holds; nothing is blended.
BannerRgba BannerToRgba(const uint8_t* image);

//! Put the colour (R, G, B, A, the way a texel is stored) under the picture. A texel the alpha
//! channel makes see-through comes out as the colour and an opaque one keeps its own; a colour that
//! is itself translucent goes under the texel at the alpha of the texel, so what is left over is the
//! alpha of the result. A fully transparent colour leaves the picture as it is.
//!
//! What comes out is premultiplied: the colour of a texel is how much light it adds to whatever it
//! is drawn over, and the alpha is how much of that surface the texel covers. That is the form a
//! renderer that blends a texture with its destination wants, and it is why an opaque fill makes the
//! whole picture opaque (issue #112) without any renderer setting having to say so.
void BannerCompositeBackground(BannerRgba& image, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
