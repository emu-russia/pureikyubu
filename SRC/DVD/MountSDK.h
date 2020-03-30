
#pragma once

#include <string>
#include <vector>
#include <list>
#include <tchar.h>

#include "../Common/Json.h"

namespace DVD
{
	class MountDolphinSdk
	{
		bool mounted = false;
		uint32_t currentSeek = 0;
		TCHAR directory[0x1000] = { 0 };

		Json DvdDataInfo;
		const TCHAR* DvdDataJson = _T("Data\\DolphinSdkDvdData.json");

		const TCHAR* AppldrPath = _T("/HW2/boot/apploader.img");
		const TCHAR* Bi2Path = _T("/X86/bin/bi2.bin");
		const TCHAR* FilesRoot = _T("/dvddata");
		const TCHAR* DolPath = _T("pong.dol");			// SDK contains demos only in ELF format :/

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

		bool GenDiskId();
		bool GenApploader();
		bool GenDol();
		bool GenBi2();
		bool GenBb2();

		void AddString(std::string str);
		void ParseDvdDataEntryForFst(Json::Value* entry);
		void WalkAndGenerateFst(Json::Value* entry);
		bool GenFst();

		std::list<std::tuple<uint8_t*, uint32_t, size_t>> mapping;
		std::list<std::tuple<TCHAR *, uint32_t, size_t>> fileMapping;
		bool GenMap();
		void WalkAndMapFiles(Json::Value* entry);
		bool GenFileMap();
		void MapVector(std::vector<uint8_t>& v, uint32_t offset);
		void MapFile(TCHAR *path, uint32_t offset);
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
		MountDolphinSdk(const TCHAR* DolphinSDKPath);
		~MountDolphinSdk();

		bool Mounted() { return mounted; }

		void Seek(int position);
		void Read(void* buffer, size_t length);

		int GetSeek() { return currentSeek; }
		TCHAR* GetDirectory() { return directory; }
	};
}
