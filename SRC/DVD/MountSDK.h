
#pragma once

#include <string>
#include <vector>
#include <list>
#include <tchar.h>

namespace DVD
{
	class MountDolphinSdk
	{
		bool mounted = false;
		uint32_t currentSeek = 0;
		TCHAR directory[0x1000] = { 0 };

		const TCHAR* AppldrPath = _T("/HW2/boot/apploader.img");
		const TCHAR* Bi2Path = _T("/X86/bin/bi2.bin");
		const TCHAR* FilesRoot = _T("/dvddata");
		const TCHAR* DolPath = _T("/pong.dol");			// SDK contains demos only in ELF format :/

		std::vector<uint8_t> DiskId;
		std::vector<uint8_t> AppldrData;
		std::vector<uint8_t> Dol;
		std::vector<uint8_t> Bb2Data;
		std::vector<uint8_t> Bi2Data;
		std::vector<uint8_t> FstData;
		std::vector<uint8_t> UserFilesData;

		bool GenDiskId();
		bool GenApploader();
		bool GenDol();
		bool GenBi2();
		bool GenDvdData();
		bool GenBb2();

		std::list<std::tuple<uint8_t*, uint32_t, size_t>> mapping;
		bool GenMap();
		void MapVector(std::vector<uint8_t>& v, uint32_t offset);
		uint8_t* Translate(uint32_t offset, size_t requestedSize, size_t& maxSize);

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