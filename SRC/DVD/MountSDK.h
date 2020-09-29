
#pragma once

namespace DVD
{
	class MountDolphinSdk
	{
		bool mounted = false;
		uint32_t currentSeek = 0;
		wchar_t directory[0x1000] = { 0 };

		Json DvdDataInfo;
		const wchar_t* DvdDataJson = L"./Data/Json/DolphinSdkDvdData.json";

		const wchar_t* AppldrPath = L"/HW2/boot/apploader.img";
		const wchar_t* Bi2Path = L"/X86/bin/bi2.bin";
		const wchar_t* FilesRoot = L"/dvddata";
		const wchar_t* DolPath = L"pong.dol";			// SDK contains demos only in ELF format :/

		std::vector<uint8_t> DiskId;
		std::vector<uint8_t> GameName;
		std::vector<uint8_t> AppldrData;
		std::vector<uint8_t> Dol;
		std::vector<uint8_t> Bb2Data;
		std::vector<uint8_t> Bi2Data;
		std::vector<uint8_t> FstData;
		std::vector<uint8_t> NameTableData;

		uint32_t userFilesStart = 16 * 1024 * 1024;
		uint32_t userFilesOffset = 0;
		int entryCounter = 0;

		bool GenDiskId();
		bool GenApploader();
		bool GenDol();
		bool GenBi2();
		bool GenBb2();

		void AddString(std::string str);
		void ParseDvdDataEntryForFst(Json::Value* entry);
		void WalkAndGenerateFst(Json::Value* entry);
		bool GenFst();

		std::list<std::tuple<std::vector<uint8_t> &, uint32_t, size_t>> mapping;		// A collection for mapping generated DVD structures (eg FST) binary blobs to the virtual DVD address space.
		std::list<std::tuple<wchar_t*, uint32_t, size_t>> fileMapping;		// Collection of mapping real files from dvddata to virtual DVD address space. The files themselves are not loaded into memory.
		bool GenMap();
		void WalkAndMapFiles(Json::Value* entry);
		bool GenFileMap();
		void MapVector(std::vector<uint8_t>& v, uint32_t offset);
		void MapFile(wchar_t*path, uint32_t offset);
		uint8_t* TranslateMemory(uint32_t offset, size_t requestedSize, size_t& maxSize);
		FILE* TranslateFile(uint32_t offset, size_t requestedSize, size_t& maxSize);

		uint32_t RoundUp32(uint32_t offset)
		{
			return (offset + 31) & ~0x1f;
		}

		uint32_t RoundUpSector(uint32_t offset)
		{
			return (offset + (DVD_SECTOR_SIZE - 1)) & ~(DVD_SECTOR_SIZE - 1);
		}

		void SwapArea(void* _addr, int sizeInBytes);

	public:
		MountDolphinSdk(const wchar_t* DolphinSDKPath);
		~MountDolphinSdk();

		bool Mounted() { return mounted; }

		void Seek(int position);
		bool Read(void* buffer, size_t length);

		int GetSeek() { return currentSeek; }
		wchar_t* GetDirectory() { return directory; }
	};
}
