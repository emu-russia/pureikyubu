/*

# RVZ disc images

Read-only support for the `.rvz` compressed disc image format used by Dolphin.

RVZ is the "revised" variant of Dolphin's WIA container (Wii ISO Archive). A file holds a
header, a table of *data entries* (the ranges of the virtual disc that are stored) and a
table of *groups* (fixed-size chunks of that data, each compressed on its own). Everything
that follows the byte-string magic is stored big-endian.

Only what is needed to mount a ready GameCube image is implemented:

- the container header with its SHA-1 checksums, and the data/group tables;
- the `None` and `Zstandard` compression methods (Zstandard is what Dolphin writes by
  default); the other methods (bzip2/LZMA/LZMA2) are reported as unsupported;
- the RVZ "packed" representation, in which runs of the pseudo-random disc junk are stored
  as an LFG seed and regenerated while reading.

Compression (writing such images) is deliberately not implemented, and neither are Wii
images, which need the Wii partition encryption.

The format itself is described in the Dolphin source tree (`docs/WiaAndRvz.md`).

*/

#pragma once

#include <cstdint>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace DVD
{
	class RvzImage
	{
	public:
		RvzImage();
		~RvzImage();

		RvzImage(const RvzImage&) = delete;
		RvzImage& operator=(const RvzImage&) = delete;

		// Mount an .rvz image. On failure the reader is left unmounted.
		bool Mount(const wchar_t* file);
		void Unmount();
		bool IsMounted() const { return mounted; }

		// Size of the uncompressed ("virtual disc") image in bytes.
		uint64_t GetDiscSize() const { return discSize; }

		// Size of the .rvz file itself.
		uint64_t GetImageSize() const { return imageSize; }

		// The compression method of the container (informative).
		int GetCompressionType() const { return (int)compressionType; }

		// Cheap check that does not parse the file: does it begin with the RVZ magic?
		static bool IsRvzFile(const wchar_t* file);

		// Read a contiguous range of the virtual disc. The whole range must be present.
		bool Read(uint64_t offset, size_t length, uint8_t* buffer);

	private:
		// A range of the virtual disc that is stored in the file.
		struct RawDataEntry
		{
			uint64_t dataOffset = 0;		// offset on the virtual disc
			uint64_t dataSize = 0;			// size on the virtual disc
			uint32_t groupIndex = 0;		// first entry in the group table
			uint32_t numberOfGroups = 0;
		};

		// One compressed chunk of the virtual disc.
		struct GroupEntry
		{
			uint64_t dataOffset = 0;		// offset in the .rvz file (the stored value is >> 2)
			uint32_t dataSize = 0;			// compressed size; bit 31 set when actually compressed
			uint32_t rvzPackedSize = 0;		// size before compression when the group is RVZ-packed
		};

		struct DataEntry
		{
			uint32_t index = 0;
			bool isPartition = false;
		};

		FILE* file = nullptr;
		bool mounted = false;
		wchar_t fileName[0x1000] = { 0 };

		// The DVD data and audio threads both read the mounted image, so the reader serializes
		// access to its file handle and group cache.
		std::mutex mutex;

		uint64_t imageSize = 0;
		uint64_t discSize = 0;
		uint32_t chunkSize = 0;
		uint32_t compressionType = 0;
		uint8_t discHeader[0x80] = { 0 };

		std::vector<RawDataEntry> rawDataEntries;
		std::vector<GroupEntry> groupEntries;
		std::map<uint64_t, DataEntry> dataEntries;	// end offset on the disc -> entry

		// The most recently decompressed group, so that sequential reads do not decompress
		// the same chunk twice.
		std::vector<uint8_t> groupCache;
		uint64_t groupCacheIndex = (uint64_t)-1;

		bool Open(const wchar_t* file);
		void Close();
		bool ReadHeader(std::vector<uint8_t>& header2);
		bool ReadTables(const std::vector<uint8_t>& header2);
		bool ReadTable(uint64_t offset, uint32_t storedSize, uint64_t unpackedSize, std::vector<uint8_t>& out);

		bool ReadFromGroups(uint64_t* offset, size_t* size, uint8_t** buffer,
			uint64_t dataOffset, uint64_t dataSize, uint32_t groupIndex, uint32_t numberOfGroups);
		bool ReadGroup(uint64_t index, uint64_t fileOffset, uint64_t compressedSize,
			uint64_t unpackedSize, bool compressed, uint32_t rvzPackedSize, uint64_t groupOffsetInData);
		bool UnpackRvz(const uint8_t* packed, size_t packedSize, uint64_t unpackedSize,
			uint64_t dataOffset, std::vector<uint8_t>& out);
		bool ReadFile(uint64_t offset, size_t length, uint8_t* buffer);
	};
}
