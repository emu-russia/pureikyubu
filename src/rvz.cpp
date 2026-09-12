// Read-only RVZ (Dolphin compressed disc image) reader.
//
// See rvz.h for the description of what is implemented. The code follows the reader in the
// Dolphin emulator (Source/Core/DiscIO/WIABlob.cpp): the on-disk structures, the grouping of the
// disc into chunks, the RVZ "packed" junk representation and the lagged Fibonacci generator used
// to recreate the junk are the same, so a file written by Dolphin is read back byte for byte.

#include "pch.h"
#include "rvz.h"

#include <algorithm>
#include <array>

#include "zstd.h"

using namespace Debug;

namespace
{
	// ---------------------------------------------------------------- format constants

	const uint32_t Header1Size = 0x48;
	const uint32_t Header2MinSize = 0xD4;
	const uint32_t DiscHeaderSize = 0x80;
	const uint32_t BlockTotalSize = 0x8000;			// the "Wii block", which RVZ packing aligns to
	const uint32_t RawDataEntrySize = 0x18;
	const uint32_t GroupEntrySize = 0x0C;

	const uint32_t RvzVersion = 0x01000000;
	const uint32_t RvzVersionReadCompatible = 0x00030000;

	// Sanity limits for a corrupted header, so that a bad count cannot ask for a huge allocation.
	// A 2 GB disc with the smallest chunk (32 KB) needs at most 64K groups.
	const uint32_t MaxRawDataEntries = 0x10000;
	const uint32_t MaxGroupEntries = 0x200000;

	enum CompressionType
	{
		CompressionNone = 0,
		CompressionPurge = 1,
		CompressionBzip2 = 2,
		CompressionLzma = 3,
		CompressionLzma2 = 4,
		CompressionZstd = 5,
	};

	// ---------------------------------------------------------------- primitive readers

	// All integers in the container are stored big-endian.
	inline uint32_t ReadBE32(const uint8_t* p)
	{
		uint32_t v;
		memcpy(&v, p, sizeof(v));
		return _BYTESWAP_UINT32(v);
	}

	inline uint64_t ReadBE64(const uint8_t* p)
	{
		uint64_t v;
		memcpy(&v, p, sizeof(v));
		return _BYTESWAP_UINT64(v);
	}

	// ---------------------------------------------------------------- file helpers

	bool SeekFile(FILE* f, uint64_t offset)
	{
#ifdef _WIN32
		return _fseeki64(f, (__int64)offset, SEEK_SET) == 0;
#else
		return fseeko(f, (off_t)offset, SEEK_SET) == 0;
#endif
	}

	uint64_t TellFile(FILE* f)
	{
#ifdef _WIN32
		return (uint64_t)_ftelli64(f);
#else
		return (uint64_t)ftello(f);
#endif
	}

	// ---------------------------------------------------------------- SHA-1

	// A compact SHA-1, only used to verify the two container checksums.
	class Sha1
	{
		uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
		uint8_t block[64] = { 0 };
		size_t blockSize = 0;
		uint64_t totalSize = 0;

		static uint32_t Rol(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

		void Process(const uint8_t* p)
		{
			uint32_t w[80];

			for (int i = 0; i < 16; i++)
				w[i] = ReadBE32(p + i * 4);
			for (int i = 16; i < 80; i++)
				w[i] = Rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

			uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];

			for (int i = 0; i < 80; i++)
			{
				uint32_t f, k;

				if (i < 20)
				{
					f = (b & c) | ((~b) & d);
					k = 0x5A827999;
				}
				else if (i < 40)
				{
					f = b ^ c ^ d;
					k = 0x6ED9EBA1;
				}
				else if (i < 60)
				{
					f = (b & c) | (b & d) | (c & d);
					k = 0x8F1BBCDC;
				}
				else
				{
					f = b ^ c ^ d;
					k = 0xCA62C1D6;
				}

				uint32_t temp = Rol(a, 5) + f + e + k + w[i];
				e = d;
				d = c;
				c = Rol(b, 30);
				b = a;
				a = temp;
			}

			h[0] += a;
			h[1] += b;
			h[2] += c;
			h[3] += d;
			h[4] += e;
		}

	public:
		void Update(const void* data, size_t size)
		{
			const uint8_t* p = (const uint8_t*)data;
			totalSize += size;

			while (size > 0)
			{
				size_t take = min(size, sizeof(block) - blockSize);
				memcpy(block + blockSize, p, take);
				blockSize += take;
				p += take;
				size -= take;

				if (blockSize == sizeof(block))
				{
					Process(block);
					blockSize = 0;
				}
			}
		}

		void Finish(uint8_t digest[20])
		{
			uint64_t bitSize = totalSize * 8;

			const uint8_t padding = 0x80;
			Update(&padding, 1);

			const uint8_t zero = 0;
			while (blockSize != 56)
				Update(&zero, 1);

			uint8_t length[8];
			for (int i = 0; i < 8; i++)
				length[i] = (uint8_t)(bitSize >> (56 - i * 8));
			Update(length, 8);

			for (int i = 0; i < 5; i++)
			{
				digest[i * 4 + 0] = (uint8_t)(h[i] >> 24);
				digest[i * 4 + 1] = (uint8_t)(h[i] >> 16);
				digest[i * 4 + 2] = (uint8_t)(h[i] >> 8);
				digest[i * 4 + 3] = (uint8_t)(h[i]);
			}
		}
	};

	// ---------------------------------------------------------------- lagged Fibonacci generator

	// Recreates the pseudo-random "junk" bytes of a GameCube disc (the same generator the hardware
	// uses for the unused areas). Only the decode direction is needed here.
	class LaggedFibonacci
	{
	public:
		static const size_t K = 521;
		static const size_t J = 32;
		static const size_t SeedSize = 17;			// in 32-bit words

		void SetSeed(const uint8_t* seed)
		{
			position = 0;

			for (size_t i = 0; i < SeedSize; i++)
				buffer[i] = ReadBE32(seed + i * 4);

			Initialize();
		}

		void Forward(size_t count)
		{
			position += count;
			while (position >= K * 4)
			{
				Forward();
				position -= K * 4;
			}
		}

		void GetBytes(size_t count, uint8_t* out)
		{
			while (count > 0)
			{
				size_t length = min(count, K * 4 - position);

				memcpy(out, (const uint8_t*)buffer.data() + position, length);

				position += length;
				count -= length;
				out += length;

				if (position == K * 4)
				{
					Forward();
					position = 0;
				}
			}
		}

	private:
		std::array<uint32_t, K> buffer{};
		size_t position = 0;

		void Forward()
		{
			for (size_t i = 0; i < J; i++)
				buffer[i] ^= buffer[i + K - J];
			for (size_t i = J; i < K; i++)
				buffer[i] ^= buffer[i - J];
		}

		void Initialize()
		{
			for (size_t i = SeedSize; i < K; i++)
				buffer[i] = (buffer[i - 17] << 23) ^ (buffer[i - 16] >> 9) ^ buffer[i - 1];

			// The hardware shifts the generated word by 18 bits instead of 16; the missing two bits
			// are patched in here (the same trick Dolphin uses), so the output code stays simple.
			for (uint32_t& x : buffer)
				x = _BYTESWAP_UINT32((x & 0xFF00FFFF) | ((x >> 2) & 0x00FF0000));

			for (int i = 0; i < 4; i++)
				Forward();
		}
	};

	// ---------------------------------------------------------------- zstd

	bool ZstdDecompress(const uint8_t* in, size_t inSize, size_t expectedSize, std::vector<uint8_t>& out)
	{
		out.clear();
		out.resize(expectedSize);

		ZSTD_DStream* stream = ZSTD_createDStream();
		if (stream == nullptr)
			return false;

		ZSTD_inBuffer input = { in, inSize, 0 };
		ZSTD_outBuffer output = { out.data(), out.size(), 0 };
		uint8_t overflow[8];

		bool ok = true;

		while (input.pos < input.size)
		{
			size_t result;

			if (output.pos < output.size)
			{
				result = ZSTD_decompressStream(stream, &output, &input);
			}
			else
			{
				// The expected data is already there, but zstd may still have the frame checksum to
				// consume. Give it a scratch buffer and make sure it does not produce more data.
				ZSTD_outBuffer extra = { overflow, sizeof(overflow), 0 };
				result = ZSTD_decompressStream(stream, &extra, &input);
				if (!ZSTD_isError(result) && extra.pos != 0)
					ok = false;
			}

			if (ZSTD_isError(result))
			{
				ok = false;
				break;
			}

			if (result == 0)
				break;
		}

		ZSTD_freeDStream(stream);

		return ok && output.pos == expectedSize;
	}

	const char* CompressionName(uint32_t type)
	{
		switch (type)
		{
			case CompressionNone: return "none";
			case CompressionPurge: return "Purge";
			case CompressionBzip2: return "bzip2";
			case CompressionLzma: return "LZMA";
			case CompressionLzma2: return "LZMA2";
			case CompressionZstd: return "Zstandard";
		}
		return "unknown";
	}
}


namespace DVD
{
	RvzImage::RvzImage()
	{
	}

	RvzImage::~RvzImage()
	{
		Unmount();
	}

	bool RvzImage::IsRvzFile(const wchar_t* path)
	{
		if (path == nullptr)
			return false;

		FILE* f = fopen(Util::WstringToString(path).c_str(), "rb");
		if (f == nullptr)
			return false;

		uint8_t magic[4] = { 0 };
		bool result = fread(magic, 1, sizeof(magic), f) == sizeof(magic) &&
			magic[0] == 'R' && magic[1] == 'V' && magic[2] == 'Z' && magic[3] == 1;

		fclose(f);

		return result;
	}

	bool RvzImage::Mount(const wchar_t* path)
	{
		std::lock_guard<std::mutex> lock(mutex);

		Close();

		if (path == nullptr)
			return false;

		if (!Open(path))
		{
			Close();
			return false;
		}

		std::vector<uint8_t> header2;

		if (!ReadHeader(header2) || !ReadTables(header2))
		{
			Close();
			return false;
		}

		wcscpy(fileName, path);
		mounted = true;

		Report(Channel::DVD, "RVZ mounted: %llu bytes on disc, %llu bytes in file, %u byte chunks, %s compression\n",
			(unsigned long long)discSize, (unsigned long long)imageSize, chunkSize, CompressionName(compressionType));

		return true;
	}

	void RvzImage::Unmount()
	{
		std::lock_guard<std::mutex> lock(mutex);
		Close();
	}

	void RvzImage::Close()
	{
		if (file != nullptr)
		{
			fclose(file);
			file = nullptr;
		}

		mounted = false;
		fileName[0] = 0;
		imageSize = 0;
		discSize = 0;
		chunkSize = 0;
		compressionType = CompressionNone;
		memset(discHeader, 0, sizeof(discHeader));

		rawDataEntries.clear();
		groupEntries.clear();
		dataEntries.clear();

		groupCache.clear();
		groupCacheIndex = (uint64_t)-1;
	}

	bool RvzImage::Open(const wchar_t* path)
	{
		file = fopen(Util::WstringToString(path).c_str(), "rb");
		if (file == nullptr)
			return false;

#ifdef _WIN32
		if (_fseeki64(file, 0, SEEK_END) != 0)
			return false;
#else
		if (fseeko(file, 0, SEEK_END) != 0)
			return false;
#endif

		imageSize = TellFile(file);

		if (imageSize < Header1Size + Header2MinSize)
			return false;

		return true;
	}

	bool RvzImage::ReadHeader(std::vector<uint8_t>& header2)
	{
		uint8_t header1[Header1Size];

		if (!ReadFile(0, sizeof(header1), header1))
			return false;

		// The magic is a byte string, not a number.
		if (!(header1[0] == 'R' && header1[1] == 'V' && header1[2] == 'Z' && header1[3] == 1))
		{
			Report(Channel::DVD, "RVZ: bad magic\n");
			return false;
		}

		const uint32_t version = ReadBE32(header1 + 0x04);
		const uint32_t versionCompatible = ReadBE32(header1 + 0x08);
		const uint32_t header2Size = ReadBE32(header1 + 0x0C);
		const uint64_t isoSize = ReadBE64(header1 + 0x24);
		const uint64_t wiaSize = ReadBE64(header1 + 0x2C);

		if (versionCompatible > RvzVersion || version < RvzVersionReadCompatible)
		{
			Report(Channel::DVD, "RVZ: unsupported version %08X (compatible with %08X)\n", version, versionCompatible);
			return false;
		}

		if (wiaSize != imageSize)
		{
			Report(Channel::DVD, "RVZ: file size mismatch (header says %llu, file is %llu)\n",
				(unsigned long long)wiaSize, (unsigned long long)imageSize);
			return false;
		}

		uint8_t digest[20];

		Sha1 sha1;
		sha1.Update(header1, Header1Size - sizeof(digest));
		sha1.Finish(digest);

		if (memcmp(digest, header1 + 0x34, sizeof(digest)) != 0)
		{
			Report(Channel::DVD, "RVZ: header 1 checksum mismatch\n");
			return false;
		}

		if (header2Size < Header2MinSize || (uint64_t)Header1Size + header2Size > imageSize)
		{
			Report(Channel::DVD, "RVZ: bad header 2 size (%u)\n", header2Size);
			return false;
		}

		header2.resize(header2Size);
		if (!ReadFile(Header1Size, header2Size, header2.data()))
			return false;

		Sha1 sha2;
		sha2.Update(header2.data(), header2.size());
		sha2.Finish(digest);

		if (memcmp(digest, header1 + 0x10, sizeof(digest)) != 0)
		{
			Report(Channel::DVD, "RVZ: header 2 checksum mismatch\n");
			return false;
		}

		discSize = isoSize;

		return true;
	}

	bool RvzImage::ReadTables(const std::vector<uint8_t>& header2)
	{
		const uint8_t* h = header2.data();

		const uint32_t discType = ReadBE32(h + 0x00);
		compressionType = ReadBE32(h + 0x04);
		chunkSize = ReadBE32(h + 0x0C);

		memcpy(discHeader, h + 0x10, DiscHeaderSize);

		const uint32_t numberOfPartitionEntries = ReadBE32(h + 0x90);
		const uint32_t numberOfRawDataEntries = ReadBE32(h + 0xB4);
		const uint64_t rawDataEntriesOffset = ReadBE64(h + 0xB8);
		const uint32_t rawDataEntriesSize = ReadBE32(h + 0xC0);
		const uint32_t numberOfGroupEntries = ReadBE32(h + 0xC4);
		const uint64_t groupEntriesOffset = ReadBE64(h + 0xC8);
		const uint32_t groupEntriesSize = ReadBE32(h + 0xD0);
		const uint8_t compressorDataSize = h[0xD4];

		if (compressorDataSize > 7 || header2.size() < Header2MinSize + compressorDataSize)
		{
			Report(Channel::DVD, "RVZ: malformed compressor data\n");
			return false;
		}

		// This is a GameCube emulator: a Wii image would need the Wii partition encryption, and
		// its partitions are stored as encrypted groups.
		if (discType != 1)
		{
			Report(Channel::DVD, "RVZ: only GameCube images are supported (disc type is %u)\n", discType);
			return false;
		}

		if (compressionType != CompressionNone && compressionType != CompressionZstd)
		{
			Report(Channel::DVD, "RVZ: unsupported compression method (%s); only none and Zstandard are supported\n",
				CompressionName(compressionType));
			return false;
		}

		if (chunkSize < BlockTotalSize || (chunkSize & (chunkSize - 1)) != 0)
		{
			Report(Channel::DVD, "RVZ: bad chunk size (%u)\n", chunkSize);
			return false;
		}

		// The disc size is handed to the DVD layer as an int, and a GameCube disc is at most 1.4 GB.
		if (discSize < DiscHeaderSize || discSize > 0x7FFFFFFF)
		{
			Report(Channel::DVD, "RVZ: bad disc size (%llu)\n", (unsigned long long)discSize);
			return false;
		}

		if (numberOfPartitionEntries != 0)
		{
			Report(Channel::DVD, "RVZ: Wii partition tables are not supported\n");
			return false;
		}

		if (numberOfRawDataEntries > MaxRawDataEntries || numberOfGroupEntries > MaxGroupEntries)
		{
			Report(Channel::DVD, "RVZ: table too large\n");
			return false;
		}

		// Raw data entries (the ranges of the virtual disc that the file stores).
		std::vector<uint8_t> rawTable;
		if (!ReadTable(rawDataEntriesOffset, rawDataEntriesSize,
			(uint64_t)numberOfRawDataEntries * RawDataEntrySize, rawTable))
		{
			Report(Channel::DVD, "RVZ: failed to read the raw data table\n");
			return false;
		}

		rawDataEntries.resize(numberOfRawDataEntries);
		for (uint32_t i = 0; i < numberOfRawDataEntries; i++)
		{
			const uint8_t* e = rawTable.data() + i * RawDataEntrySize;
			rawDataEntries[i].dataOffset = ReadBE64(e + 0x00);
			rawDataEntries[i].dataSize = ReadBE64(e + 0x08);
			rawDataEntries[i].groupIndex = ReadBE32(e + 0x10);
			rawDataEntries[i].numberOfGroups = ReadBE32(e + 0x14);
		}

		// Group entries (the compressed chunks).
		std::vector<uint8_t> groupTable;
		if (!ReadTable(groupEntriesOffset, groupEntriesSize,
			(uint64_t)numberOfGroupEntries * GroupEntrySize, groupTable))
		{
			Report(Channel::DVD, "RVZ: failed to read the group table\n");
			return false;
		}

		groupEntries.resize(numberOfGroupEntries);
		for (uint32_t i = 0; i < numberOfGroupEntries; i++)
		{
			const uint8_t* e = groupTable.data() + i * GroupEntrySize;
			groupEntries[i].dataOffset = (uint64_t)ReadBE32(e + 0x00) << 2;
			groupEntries[i].dataSize = ReadBE32(e + 0x04);
			groupEntries[i].rvzPackedSize = ReadBE32(e + 0x08);
		}

		// Index the stored ranges by their end offset, the way Dolphin does.
		for (uint32_t i = 0; i < numberOfRawDataEntries; i++)
		{
			const RawDataEntry& raw = rawDataEntries[i];

			if ((uint64_t)raw.groupIndex + raw.numberOfGroups > numberOfGroupEntries)
			{
				Report(Channel::DVD, "RVZ: raw data entry %u refers to groups past the table\n", i);
				return false;
			}

			if (raw.dataSize != 0)
			{
				DataEntry entry;
				entry.index = i;
				entry.isPartition = false;
				dataEntries.emplace(raw.dataOffset + raw.dataSize, entry);
			}
		}

		// No stored range may overlap another one.
		for (uint32_t i = 0; i < numberOfRawDataEntries; i++)
		{
			const RawDataEntry& raw = rawDataEntries[i];
			if (raw.dataSize == 0)
				continue;

			auto it = dataEntries.upper_bound(raw.dataOffset);
			if (it == dataEntries.end() || it->second.isPartition || it->second.index != i)
			{
				Report(Channel::DVD, "RVZ: overlapping data entries\n");
				return false;
			}
		}

		return true;
	}

	bool RvzImage::ReadTable(uint64_t offset, uint32_t storedSize, uint64_t unpackedSize, std::vector<uint8_t>& out)
	{
		out.clear();

		if (unpackedSize == 0)
			return true;

		if (storedSize == 0 || storedSize > imageSize)
			return false;

		std::vector<uint8_t> stored(storedSize);
		if (!ReadFile(offset, storedSize, stored.data()))
			return false;

		if (compressionType == CompressionNone)
		{
			if (storedSize < unpackedSize)
				return false;

			out.assign(stored.begin(), stored.begin() + (size_t)unpackedSize);
			return true;
		}

		return ZstdDecompress(stored.data(), stored.size(), (size_t)unpackedSize, out);
	}

	bool RvzImage::Read(uint64_t offset, size_t length, uint8_t* buffer)
	{
		std::lock_guard<std::mutex> lock(mutex);

		if (!mounted || file == nullptr)
			return false;

		if (length == 0)
			return true;

		if (length > discSize || offset > discSize - length)
			return false;

		uint64_t position = offset;
		size_t left = length;
		uint8_t* out = buffer;

		// The first bytes of the disc are kept in the header itself, not in the compressed data.
		if (position < DiscHeaderSize)
		{
			size_t take = (size_t)std::min<uint64_t>(DiscHeaderSize - position, left);
			memcpy(out, discHeader + position, take);
			position += take;
			out += take;
			left -= take;
		}

		while (left > 0)
		{
			auto it = dataEntries.upper_bound(position);
			if (it == dataEntries.end())
				return false;

			const DataEntry& entry = it->second;
			if (entry.isPartition || entry.index >= rawDataEntries.size())
				return false;

			const RawDataEntry& raw = rawDataEntries[entry.index];

			const uint64_t previousPosition = position;
			const size_t previousLeft = left;

			if (!ReadFromGroups(&position, &left, &out, raw.dataOffset, raw.dataSize,
				raw.groupIndex, raw.numberOfGroups))
			{
				return false;
			}

			// A malformed image must not turn this loop into an endless one.
			if (position == previousPosition && left == previousLeft)
				return false;
		}

		return true;
	}

	bool RvzImage::ReadFromGroups(uint64_t* offset, size_t* size, uint8_t** buffer,
		uint64_t dataOffset, uint64_t dataSize, uint32_t groupIndex, uint32_t numberOfGroups)
	{
		if (dataOffset + dataSize <= *offset)
			return true;

		if (*offset < dataOffset)
			return false;

		// Dolphin aligns the stored range down to a block boundary and starts the groups there.
		const uint64_t skipped = dataOffset % BlockTotalSize;
		dataOffset -= skipped;
		dataSize += skipped;

		const uint64_t startGroup = (*offset - dataOffset) / chunkSize;

		for (uint64_t i = startGroup; i < numberOfGroups && *size > 0; i++)
		{
			const uint64_t totalGroup = groupIndex + i;
			if (totalGroup >= groupEntries.size())
				return false;

			const GroupEntry& group = groupEntries[(size_t)totalGroup];

			const uint64_t groupOffsetInData = i * chunkSize;
			if (groupOffsetInData >= dataSize)
				return false;

			const uint64_t offsetInGroup = *offset - groupOffsetInData - dataOffset;
			const uint64_t thisChunk = std::min<uint64_t>(chunkSize, dataSize - groupOffsetInData);

			if (offsetInGroup >= thisChunk)
				return false;

			const uint64_t bytesToRead = std::min<uint64_t>(thisChunk - offsetInGroup, *size);

			uint32_t groupDataSize = group.dataSize;
			const bool compressed = (groupDataSize & 0x80000000) != 0;
			groupDataSize &= 0x7FFFFFFF;

			if (groupDataSize == 0)
			{
				// A hole in the image reads back as zeroes.
				memset(*buffer, 0, (size_t)bytesToRead);
			}
			else
			{
				if (!ReadGroup(totalGroup, group.dataOffset, groupDataSize, thisChunk,
					compressed, group.rvzPackedSize, groupOffsetInData))
				{
					return false;
				}

				if (offsetInGroup + bytesToRead > groupCache.size())
					return false;

				memcpy(*buffer, groupCache.data() + offsetInGroup, (size_t)bytesToRead);
			}

			*offset += bytesToRead;
			*size -= (size_t)bytesToRead;
			*buffer += bytesToRead;
		}

		return true;
	}

	bool RvzImage::ReadGroup(uint64_t index, uint64_t fileOffset, uint64_t compressedSize,
		uint64_t unpackedSize, bool compressed, uint32_t rvzPackedSize, uint64_t groupOffsetInData)
	{
		if (groupCacheIndex == index && groupCache.size() == unpackedSize)
			return true;

		groupCache.clear();
		groupCacheIndex = (uint64_t)-1;

		if (compressedSize > imageSize)
			return false;

		std::vector<uint8_t> stored((size_t)compressedSize);
		if (!ReadFile(fileOffset, stored.size(), stored.data()))
			return false;

		// When the group is RVZ-packed the intermediate (packed) form is what the compressor saw,
		// so that is the size zstd has to produce; the packed form then expands to the chunk size.
		const uint64_t intermediateSize = rvzPackedSize ? rvzPackedSize : unpackedSize;

		// The packed form is a chunk plus a few bytes per segment; a wildly larger value in a
		// corrupted header must not turn into a huge allocation.
		if (intermediateSize > (uint64_t)chunkSize * 2 + 0x10000)
			return false;
		std::vector<uint8_t> intermediate;

		if (compressed)
		{
			if (!ZstdDecompress(stored.data(), stored.size(), (size_t)intermediateSize, intermediate))
				return false;
		}
		else
		{
			if (stored.size() != intermediateSize)
				return false;

			intermediate = std::move(stored);
		}

		if (rvzPackedSize != 0)
		{
			if (!UnpackRvz(intermediate.data(), intermediate.size(), unpackedSize, groupOffsetInData, groupCache))
				return false;
		}
		else
		{
			if (intermediate.size() != unpackedSize)
				return false;

			groupCache = std::move(intermediate);
		}

		groupCacheIndex = index;

		return true;
	}

	bool RvzImage::UnpackRvz(const uint8_t* packed, size_t packedSize, uint64_t unpackedSize,
		uint64_t dataOffset, std::vector<uint8_t>& out)
	{
		out.clear();
		out.resize((size_t)unpackedSize);

		LaggedFibonacci lfg;

		size_t position = 0;
		uint64_t written = 0;
		uint64_t currentOffset = dataOffset;

		while (written < unpackedSize)
		{
			if (position + 4 > packedSize)
				return false;

			const uint32_t segment = ReadBE32(packed + position);
			position += 4;

			const uint32_t length = segment & 0x7FFFFFFF;
			if (length > unpackedSize - written)
				return false;

			if (segment & 0x80000000)
			{
				// Junk: the segment is an LFG seed, not data.
				if (position + LaggedFibonacci::SeedSize * 4 > packedSize)
					return false;

				lfg.SetSeed(packed + position);
				position += LaggedFibonacci::SeedSize * 4;

				lfg.Forward((size_t)(currentOffset % BlockTotalSize));
				lfg.GetBytes(length, out.data() + written);
			}
			else
			{
				if (position + length > packedSize)
					return false;

				memcpy(out.data() + written, packed + position, length);
				position += length;
			}

			written += length;
			currentOffset += length;
		}

		return true;
	}

	bool RvzImage::ReadFile(uint64_t offset, size_t length, uint8_t* buffer)
	{
		if (file == nullptr)
			return false;

		if (length > imageSize || offset > imageSize - length)
			return false;

		if (!SeekFile(file, offset))
			return false;

		return fread(buffer, 1, length, file) == length;
	}
}
