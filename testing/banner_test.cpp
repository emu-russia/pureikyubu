// DVD banner image unit tests (issue #112).
//
// The subject is the whole of src/banner.cpp: the decode of the RGB5A3 texture a disc carries, and
// the compositing of a background colour under the texels its alpha channel makes see-through. Both
// are pure functions of the banner bytes - no window, no renderer, no console - so the tests build
// banners of their own, with the texel values chosen to pin down what the format means.
//
// What the tests pin down:
//
//   * the two formats a texel of a banner can be in: an opaque RGB555 one (5 bits per channel) and
//     an RGB4A3 one (4 bits per channel and 3 bits of alpha);
//   * which bit of the word picks the format, and that the word is read big-endian;
//   * the 4x4 tiling of the picture (a texel of the second tile of a row is not the texel next to
//     the first tile's);
//   * a background that is composited under a translucent texel, at the alpha of the texel and not
//     at the alpha of the colour;
//   * a background that is fully transparent, which is not a background at all;
//   * a fill that is applied twice, which is the same fill (the selector applies the setting to the
//     same decoded picture every time it changes).

#include "pch.h"

namespace BannerUnitTest
{
	// -------------------------------------------------------------------------------------------
	// Banners of the test's own
	// -------------------------------------------------------------------------------------------

	/// <summary>The number of 4x4 tiles a banner is made of.</summary>
	constexpr int BannerTiles = (DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT) / 16;

	/// <summary>Pack a texel word the way the big-endian image carries it: the high byte of the
	/// word is the byte at the lower address, whatever the byte order of the host is.</summary>
	static void PutWord(std::vector<uint8_t>& image, int tile, int j, int k, uint16_t word)
	{
		const size_t at = (size_t)(tile * 16 + j * 4 + k) * 2;

		image[at + 0] = (uint8_t)(word >> 8);
		image[at + 1] = (uint8_t)(word & 0xff);
	}

	/// <summary>An RGB555 texel of the word: 5 bits of every channel, opaque.</summary>
	static uint16_t Rgb555(uint16_t r5, uint16_t g5, uint16_t b5)
	{
		return (uint16_t)(0x8000 | (r5 << 10) | (g5 << 5) | b5);
	}

	/// <summary>An RGB4A3 texel of the word: 4 bits of every channel, 3 bits of alpha.</summary>
	static uint16_t Rgb4a3(uint16_t r4, uint16_t g4, uint16_t b4, uint16_t a3)
	{
		return (uint16_t)((a3 << 12) | (r4 << 8) | (g4 << 4) | b4);
	}

	/// <summary>A banner of one word spread over the whole picture.</summary>
	static std::vector<uint8_t> UniformBanner(uint16_t word)
	{
		std::vector<uint8_t> image(2 * DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT, 0);

		for (int tile = 0; tile < BannerTiles; tile++)
		{
			for (int j = 0; j < 4; j++)
			{
				for (int k = 0; k < 4; k++)
				{
					PutWord(image, tile, j, k, word);
				}
			}
		}

		return image;
	}

	/// <summary>The texel of a decoded picture, as the four bytes it is stored in. A texel of a
	/// picture a background was put under is premultiplied (see BannerCompositeBackground).</summary>
	static const uint8_t* Texel(const BannerRgba& image, int x, int y)
	{
		return &image.pixels[((size_t)y * DVD_BANNER_WIDTH + x) * 4];
	}

	TEST_CLASS(BannerTest)
	{
	public:

		// ---------------------------------------------------------------------------------------
		// The decode
		// ---------------------------------------------------------------------------------------

		TEST_METHOD(Decode_Rgb555ExpandsTheFiveBitChannels)
		{
			BannerRgba image = BannerToRgba(UniformBanner(Rgb555(0, 0, 0)).data());

			const uint8_t* black = Texel(image, 0, 0);
			Assert::AreEqual((int)0, (int)black[0]);
			Assert::AreEqual((int)0, (int)black[1]);
			Assert::AreEqual((int)0, (int)black[2]);
			Assert::AreEqual((int)255, (int)black[3], L"an RGB555 texel is opaque");

			// The maximum of a 5 bit channel, and the middle one, which is the rounding the format's
			// 31 steps become in 8 bits (16 * 255 / 31 = 131).
			image = BannerToRgba(UniformBanner(Rgb555(31, 16, 0)).data());

			const uint8_t* texel = Texel(image, 5, 7);
			Assert::AreEqual((int)255, (int)texel[0]);
			Assert::AreEqual((int)131, (int)texel[1]);
			Assert::AreEqual((int)0, (int)texel[2]);
		}

		TEST_METHOD(Decode_Rgb4a3ExpandsTheFourBitChannels)
		{
			// The three alpha codes that matter: nothing, the middle one, and everything.
			BannerRgba image = BannerToRgba(UniformBanner(Rgb4a3(15, 15, 15, 7)).data());
			const uint8_t* opaque = Texel(image, 0, 0);
			Assert::AreEqual((int)255, (int)opaque[0]);
			Assert::AreEqual((int)255, (int)opaque[1]);
			Assert::AreEqual((int)255, (int)opaque[2]);
			Assert::AreEqual((int)255, (int)opaque[3]);

			image = BannerToRgba(UniformBanner(Rgb4a3(0, 0, 0, 0)).data());
			const uint8_t* clear = Texel(image, 0, 0);
			Assert::AreEqual((int)0, (int)clear[0]);
			Assert::AreEqual((int)0, (int)clear[3], L"the alpha code 0 is a see-through texel");

			// 4 bits of a channel are the 4 bits doubled (0xf -> 255, 0x8 -> 136); 3 bits of alpha
			// are the 8 bit range in 7 steps (4 * 255 / 7 = 145).
			image = BannerToRgba(UniformBanner(Rgb4a3(8, 0, 1, 4)).data());
			const uint8_t* texel = Texel(image, 11, 3);
			Assert::AreEqual((int)136, (int)texel[0]);
			Assert::AreEqual((int)0, (int)texel[1]);
			Assert::AreEqual((int)17, (int)texel[2]);
			Assert::AreEqual((int)145, (int)texel[3]);
		}

		TEST_METHOD(Decode_TheTilesAreLaidOutFourByFour)
		{
			// The first word of the picture is the top left texel of the first tile, and the second
			// tile of the same row of tiles starts four texels to the right of it.
			std::vector<uint8_t> image(2 * DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT, 0);

			PutWord(image, 0, 0, 0, Rgb555(31, 0, 0));      // (0, 0)
			PutWord(image, 0, 1, 2, Rgb555(0, 31, 0));      // (2, 1)
			PutWord(image, 1, 3, 3, Rgb555(0, 0, 31));      // (7, 3), the last texel of the second tile

			BannerRgba decoded = BannerToRgba(image.data());

			Assert::AreEqual((int)255, (int)Texel(decoded, 0, 0)[0], L"the top left texel is the first word of the first tile");
			Assert::AreEqual((int)255, (int)Texel(decoded, 2, 1)[1], L"the word at j=1, k=2 is the texel at (2, 1)");
			Assert::AreEqual((int)255, (int)Texel(decoded, 7, 3)[2], L"the tiles of a row run left to right");
			Assert::AreEqual((int)0, (int)Texel(decoded, 4, 0)[0], L"the second tile does not overlap the first");
		}

		TEST_METHOD(Decode_TheWordsOfAImageAreBigEndian)
		{
			// The bytes of the image are the texture as the console stores it, so the word of the first
			// texel has its high half first - not the order the host reads a `uint16_t` in.
			std::vector<uint8_t> image(2 * DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT, 0);

			PutWord(image, 0, 0, 0, Rgb555(31, 0, 0));

			Assert::AreEqual((int)0xfc, (int)image[0], L"the high half of the word comes first");
			Assert::AreEqual((int)0x00, (int)image[1], L"and the low half second");

			BannerRgba decoded = BannerToRgba(image.data());

			Assert::AreEqual((int)255, (int)Texel(decoded, 0, 0)[0], L"an RGB555 word is red, not a format the alpha picks");
		}

		TEST_METHOD(Decode_TheWordIsReadBigEndian)
		{
			// A word whose two halves differ: were the halves swapped, the texel would come out the
			// second colour instead of the first one.
			BannerRgba image = BannerToRgba(UniformBanner(Rgb555(31, 0, 0)).data());

			Assert::AreEqual((int)255, (int)Texel(image, 0, 0)[0]);
			Assert::AreEqual((int)0, (int)Texel(image, 0, 0)[1]);

			image = BannerToRgba(UniformBanner(Rgb555(0, 31, 0)).data());

			Assert::AreEqual((int)0, (int)Texel(image, 0, 0)[0]);
			Assert::AreEqual((int)255, (int)Texel(image, 0, 0)[1]);
		}

		TEST_METHOD(Decode_PreserveKeepsTheAlphaOfEveryTexel)
		{
			// The decode never touches the alpha: that is what a caller that wants the transparency
			// of the banner keeps (BannerCompositeBackground is the only thing that changes it).
			std::vector<uint8_t> image(2 * DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT, 0);

			PutWord(image, 0, 0, 0, Rgb4a3(15, 0, 0, 0));   // see-through
			PutWord(image, 0, 0, 1, Rgb4a3(15, 0, 0, 3));   // half of the alpha range

			BannerRgba decoded = BannerToRgba(image.data());

			Assert::AreEqual((int)0, (int)Texel(decoded, 0, 0)[3]);
			Assert::AreEqual((int)(3 * 255 / 7), (int)Texel(decoded, 1, 0)[3]);
		}

		// ---------------------------------------------------------------------------------------
		// The background
		// ---------------------------------------------------------------------------------------

		TEST_METHOD(Composite_OpaqueFillMakesEveryTexelOpaque)
		{
			BannerRgba image = BannerToRgba(UniformBanner(Rgb4a3(15, 15, 15, 0)).data());

			BannerCompositeBackground(image, 0x00, 0x00, 0xff, 255);    // opaque blue

			// A texel the alpha made fully see-through comes out as the colour itself.
			const uint8_t* texel = Texel(image, 20, 10);
			Assert::AreEqual((int)0x00, (int)texel[0]);
			Assert::AreEqual((int)0x00, (int)texel[1]);
			Assert::AreEqual((int)0xff, (int)texel[2]);
			Assert::AreEqual((int)255, (int)texel[3], L"the fill leaves nothing see-through");
		}

		TEST_METHOD(Composite_TheFillGoesUnderTheTexelAtItsAlpha)
		{
			// A white texel at the middle alpha over an opaque black fill is the part of it the alpha
			// does not fill; an opaque one is itself whatever the fill is.
			BannerRgba image = BannerToRgba(UniformBanner(Rgb4a3(15, 15, 15, 4)).data());

			BannerCompositeBackground(image, 0x00, 0x00, 0x00, 255);

			// 3 bits of alpha are the 8 bit range in 7 steps: 4 is 145, and 255 - 145 = 110.
			const uint8_t* texel = Texel(image, 0, 0);
			Assert::AreEqual((int)((255 * 145 + 0 * 110) / 255), (int)texel[0]);
			Assert::AreEqual((int)255, (int)texel[3], L"an opaque fill leaves nothing see-through");

			// An opaque texel has no room for the fill at all, whatever colour it is.
			BannerRgba white = BannerToRgba(UniformBanner(Rgb4a3(15, 15, 15, 7)).data());

			BannerCompositeBackground(white, 0x00, 0x00, 0xff, 255);

			Assert::AreEqual((int)255, (int)Texel(white, 0, 0)[2], L"an opaque white texel keeps its white");

			// And an opaque texel keeps its own colour, whatever the fill is.
			BannerRgba opaque = BannerToRgba(UniformBanner(Rgb555(31, 31, 31)).data());

			BannerCompositeBackground(opaque, 0xff, 0x00, 0x00, 255);

			Assert::AreEqual((int)255, (int)Texel(opaque, 30, 20)[0]);
			Assert::AreEqual((int)255, (int)Texel(opaque, 30, 20)[1]);
			Assert::AreEqual((int)255, (int)Texel(opaque, 30, 20)[2]);
		}

		TEST_METHOD(Composite_ATransparentFillIsNoFill)
		{
			BannerRgba image = BannerToRgba(UniformBanner(Rgb4a3(15, 0, 0, 0)).data());

			BannerCompositeBackground(image, 0x11, 0x22, 0x33, 0);

			const uint8_t* texel = Texel(image, 0, 0);
			Assert::AreEqual((int)255, (int)texel[0], L"the colour is not painted at all");
			Assert::AreEqual((int)0, (int)texel[3], L"and the texel is as see-through as it was");
		}

		TEST_METHOD(Composite_AFillAppliedTwiceIsTheSameFill)
		{
			// What the selector does: the setting is applied to the decoded picture again and again,
			// so a fill that is applied twice has to be the fill that was applied once.
			BannerRgba once = BannerToRgba(UniformBanner(Rgb4a3(15, 0, 15, 4)).data());
			BannerRgba twice = once;

			BannerCompositeBackground(once, 0x10, 0x20, 0x30, 255);
			BannerCompositeBackground(twice, 0x10, 0x20, 0x30, 255);
			BannerCompositeBackground(twice, 0x10, 0x20, 0x30, 255);

			Assert::IsTrue(once.pixels == twice.pixels, L"the second fill changed the picture");
		}

		TEST_METHOD(Composite_ATranslucentFillIsBlendedToo)
		{
			// The colour itself may be translucent. A texel the banner made fully see-through has nothing
			// of its own to give, so the fill is the whole of it: the alpha is the alpha of the colour and
			// the colour is that alpha of the colour, the way a premultiplied one is stored.
			BannerRgba image = BannerToRgba(UniformBanner(Rgb4a3(0, 0, 0, 0)).data());

			BannerCompositeBackground(image, 0xff, 0xff, 0xff, 128);

			const uint8_t* texel = Texel(image, 0, 0);
			Assert::AreEqual((int)128, (int)texel[0], L"the colour of the fill is premultiplied by its alpha");
			Assert::AreEqual((int)128, (int)texel[3]);

			// A texel that is not fully see-through gives its own part of the result: the alpha of the two
			// is what the texel holds plus what the fill adds to the room the texel left.
			BannerRgba grey = BannerToRgba(UniformBanner(Rgb4a3(15, 15, 15, 4)).data());

			BannerCompositeBackground(grey, 0xff, 0xff, 0xff, 128);

			Assert::AreEqual((int)(145 + 128 * 110 / 255), (int)Texel(grey, 0, 0)[3],
				L"145 + 128 * (255 - 145) / 255 is the alpha of the two");
			Assert::AreEqual((int)(255 * 145 / 255 + 128 * 110 / 255), (int)Texel(grey, 0, 0)[0],
				L"what the white texel holds and what the fill got through are added, premultiplied");
		}
	};
}
