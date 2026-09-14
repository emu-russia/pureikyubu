// A minimal PNG writer for the harness (the frame dumps the documentation and the test reports
// use). It writes uncompressed deflate blocks, so no compression library is needed and the file
// is still a valid PNG every viewer reads.

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace GbaTest
{
	/// <summary>CRC32 (the PNG chunk checksum).</summary>
	inline uint32_t Crc32(const uint8_t* data, size_t size)
	{
		static uint32_t table[256];
		static bool ready = false;
		if (!ready)
		{
			for (uint32_t n = 0; n < 256; n++)
			{
				uint32_t c = n;
				for (int k = 0; k < 8; k++)
					c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
				table[n] = c;
			}
			ready = true;
		}

		uint32_t c = 0xFFFFFFFFu;
		for (size_t i = 0; i < size; i++)
			c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
		return c ^ 0xFFFFFFFFu;
	}

	inline void PushBe32(std::vector<uint8_t>& out, uint32_t value)
	{
		out.push_back((uint8_t)(value >> 24));
		out.push_back((uint8_t)(value >> 16));
		out.push_back((uint8_t)(value >> 8));
		out.push_back((uint8_t)value);
	}

	inline void PushChunk(std::vector<uint8_t>& out, const char type[5], const std::vector<uint8_t>& data)
	{
		PushBe32(out, (uint32_t)data.size());
		size_t start = out.size();
		out.insert(out.end(), type, type + 4);
		out.insert(out.end(), data.begin(), data.end());
		uint32_t crc = Crc32(out.data() + start, out.size() - start);
		PushBe32(out, crc);
	}

	/// <summary>
	/// Write an XRGB8888 image (0xAARRGGBB, row 0 on top) as a PNG. `scale` repeats every pixel.
	/// </summary>
	inline bool WritePng(const std::string& path, const uint32_t* pixels, int width, int height, int scale = 1)
	{
		if (pixels == nullptr || width <= 0 || height <= 0 || scale < 1)
			return false;

		const int outWidth = width * scale;
		const int outHeight = height * scale;

		// Raw scanlines: filter byte 0 + RGB triples.
		std::vector<uint8_t> raw;
		raw.reserve((size_t)outHeight * (1 + (size_t)outWidth * 3));

		for (int y = 0; y < outHeight; y++)
		{
			raw.push_back(0);
			const uint32_t* row = pixels + (size_t)(y / scale) * width;
			for (int x = 0; x < outWidth; x++)
			{
				uint32_t p = row[x / scale];
				raw.push_back((uint8_t)(p >> 16));
				raw.push_back((uint8_t)(p >> 8));
				raw.push_back((uint8_t)p);
			}
		}

		std::vector<uint8_t> out;
		const uint8_t signature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
		out.insert(out.end(), signature, signature + 8);

		std::vector<uint8_t> ihdr;
		PushBe32(ihdr, (uint32_t)outWidth);
		PushBe32(ihdr, (uint32_t)outHeight);
		ihdr.push_back(8);		// bit depth
		ihdr.push_back(2);		// colour type: truecolour
		ihdr.push_back(0);		// compression
		ihdr.push_back(0);		// filter
		ihdr.push_back(0);		// interlace
		PushChunk(out, "IHDR", ihdr);

		// zlib stream with stored deflate blocks.
		std::vector<uint8_t> z;
		z.push_back(0x78);
		z.push_back(0x01);

		size_t offset = 0;
		while (offset < raw.size())
		{
			size_t block = raw.size() - offset;
			if (block > 65535) block = 65535;
			bool last = (offset + block) >= raw.size();
			z.push_back(last ? 1 : 0);
			z.push_back((uint8_t)block);
			z.push_back((uint8_t)(block >> 8));
			z.push_back((uint8_t)(~block & 0xFF));
			z.push_back((uint8_t)((~block >> 8) & 0xFF));
			z.insert(z.end(), raw.begin() + offset, raw.begin() + offset + block);
			offset += block;
		}

		// Adler32 of the raw data.
		uint32_t a = 1, b = 0;
		for (size_t i = 0; i < raw.size(); i++)
		{
			a = (a + raw[i]) % 65521;
			b = (b + a) % 65521;
		}
		PushBe32(z, (b << 16) | a);

		PushChunk(out, "IDAT", z);
		PushChunk(out, "IEND", std::vector<uint8_t>());

		FILE* f = fopen(path.c_str(), "wb");
		if (f == nullptr)
			return false;
		size_t written = fwrite(out.data(), 1, out.size(), f);
		fclose(f);
		return written == out.size();
	}
}
