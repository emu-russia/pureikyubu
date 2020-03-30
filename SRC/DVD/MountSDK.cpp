/*
Code for mounting the Dolphin SDK folder as a virtual disk.

All the necessary data (BI2, Appldr, some DOL executable, we take from the SDK). If they are not there, then the disk is simply not mounted.
*/

#include "pch.h"
#include "../UI/UserFile.h"

namespace DVD
{
	MountDolphinSdk::MountDolphinSdk(const TCHAR * DolphinSDKPath)
	{
		_tcscpy_s(directory, _countof(directory) - 1, DolphinSDKPath);

		// Load dvddata structure

		size_t dvdDataInfoTextSize = 0;
		void* dvdDataInfoText = UI::FileLoad(DvdDataJson, &dvdDataInfoTextSize);
		if (!dvdDataInfoText)
		{
			DBReport("Failed to load DolphinSDK dvddata json: %s\n", Debug::Hub.TcharToString((TCHAR *)DvdDataJson).c_str());
			return;
		}

		try
		{
			DvdDataInfo.Deserialize(dvdDataInfoText, dvdDataInfoTextSize);
		}
		catch (...)
		{
			DBReport("Failed to Deserialize DolphinSDK dvddata json: %s\n", Debug::Hub.TcharToString((TCHAR*)DvdDataJson).c_str());
			free(dvdDataInfoText);
			return;
		}

		free(dvdDataInfoText);

		// Generate data blobs

		if (!GenDiskId())
		{
			DBReport("Failed to GenDiskId\n");
			return;
		}
		if (!GenApploader())
		{
			DBReport("Failed to GenApploader\n");
			return;
		}
		if (!GenBi2())
		{
			DBReport("Failed to GenBi2\n");
			return;
		}
		if (!GenFst())
		{
			DBReport("Failed to GenFst\n");
			return;
		}
		if (!GenDvdData())
		{
			DBReport("Failed to GenDvdData\n");
			return;
		}
		if (!GenDol())
		{
			DBReport("Failed to GenDol\n");
			return;
		}
		if (!GenBb2())
		{
			DBReport("Failed to GenBb2\n");
			return;
		}

		// Generate map

		if (!GenMap())
		{
			DBReport("Failed to GenMap\n");
			return;
		}

		DBReport2(DbgChannel::DVD, "DolphinSDK mounted!\n");
		mounted = true;
	}

	MountDolphinSdk::~MountDolphinSdk()
	{
	}

	void MountDolphinSdk::MapVector(std::vector<uint8_t>& v, uint32_t offset)
	{
		std::tuple<uint8_t*, uint32_t, size_t> entry(v.data(), offset, v.size());
		mapping.push_back(entry);
	}

	uint8_t* MountDolphinSdk::Translate(uint32_t offset, size_t requestedSize, size_t& maxSize)
	{
		for (auto it = mapping.begin(); it != mapping.end(); ++it)
		{
			uint8_t * ptr = std::get<0>(*it);
			uint32_t startingOffset = std::get<1>(*it);
			size_t size = std::get<2>(*it);

			if (startingOffset <= offset && offset < (startingOffset + size))
			{
				maxSize = min(requestedSize, (startingOffset + size) - offset);
				return ptr + (offset - startingOffset);
			}
		}
		return nullptr;
	}

	void MountDolphinSdk::Seek(int position)
	{
		if (!mounted)
			return;

		assert(position >= 0 && position < DVD_SIZE);

		currentSeek = (uint32_t)position;
	}

	void MountDolphinSdk::Read(void* buffer, size_t length)
	{
		assert(buffer);

		if (!mounted)
		{
			memset(buffer, 0, length);
			return;
		}

		if (currentSeek >= DVD_SIZE)
		{
			memset(buffer, 0, length);
			return;
		}

		size_t maxLength = 0;
		
		uint8_t* ptr = Translate(currentSeek, length, maxLength);
		if (ptr != nullptr)
		{
			memcpy(buffer, ptr, maxLength);
			if (maxLength < length)
			{
				memset((uint8_t *)buffer + maxLength, 0, length - maxLength);
			}
		}
		else
		{
			memset(buffer, 0, length);
		}

		currentSeek += (uint32_t)length;
	}

	#pragma region "Data Generators"

	bool MountDolphinSdk::GenDiskId()
	{
		DiskId.resize(sizeof(DiskID));

		DiskID* id = (DiskID*)DiskId.data();

		id->gameName[0] = 'S';
		id->gameName[1] = 'D';
		id->gameName[2] = 'K';
		id->gameName[3] = 'E';
		
		id->company[0] = '0';
		id->company[1] = '1';

		id->magicNumber = _byteswap_ulong(DVD_DISKID_MAGIC);

		GameName.resize(0x400);
		memset(GameName.data(), 0, GameName.size());
		strcpy_s((char *)GameName.data(), 0x100, "Dolphin SDK");

		return true;
	}

	bool MountDolphinSdk::GenApploader()
	{
		TCHAR path[0x1000] = { 0, };

		_stprintf_s(path, _countof(path) - 1, _T("%s%s"), directory, AppldrPath);

		size_t size = 0;
		void* ptr = UI::FileLoad(path, &size);
		assert(ptr);

		AppldrData.resize(size);
		memcpy(AppldrData.data(), ptr, size);

		free(ptr);

		return true;
	}

	bool MountDolphinSdk::GenDol()
	{
		size_t size = 0;
		void* ptr = UI::FileLoad(DolPath, &size);
		assert(ptr);

		Dol.resize(size);
		memcpy(Dol.data(), ptr, size);

		free(ptr);

		return true;
	}

	bool MountDolphinSdk::GenBi2()
	{
		TCHAR path[0x1000] = { 0, };

		_stprintf_s(path, _countof(path) - 1, _T("%s%s"), directory, Bi2Path);

		size_t size = 0;
		void* ptr = UI::FileLoad(path, &size);
		assert(ptr);

		Bi2Data.resize(size);
		memcpy(Bi2Data.data(), ptr, size);

		free(ptr);

		return true;
	}

	bool MountDolphinSdk::GenFst()
	{
		return true;
	}

	bool MountDolphinSdk::GenDvdData()
	{
		return true;
	}

	bool MountDolphinSdk::GenBb2()
	{
		DVDBB2 bb2 = { 0 };

		bb2.bootFilePosition = RoundUpSector(DVD_APPLDR_OFFSET + (uint32_t)AppldrData.size());
		bb2.FSTLength = (uint32_t)FstData.size();
		bb2.FSTMaxLength = bb2.FSTLength;
		bb2.FSTPosition = RoundUpSector(bb2.bootFilePosition + (uint32_t)Dol.size() + DVD_SECTOR_SIZE);
		bb2.userPosition = RoundUpSector(bb2.FSTPosition + (uint32_t)FstData.size() + DVD_SECTOR_SIZE);
		bb2.userLength = (uint32_t)UserFilesData.size();

		Bb2Data.resize(sizeof(DVDBB2));
		memcpy(Bb2Data.data(), &bb2, sizeof(bb2));

		return true;
	}

	bool MountDolphinSdk::GenMap()
	{
		MapVector(DiskId, DVD_ID_OFFSET);
		MapVector(GameName, sizeof(DiskID));

		MapVector(Bb2Data, DVD_BB2_OFFSET);
		MapVector(Bi2Data, DVD_BI2_OFFSET);
		MapVector(AppldrData, DVD_APPLDR_OFFSET);

		DVDBB2* bb2 = (DVDBB2 *)Bb2Data.data();

		MapVector(Dol, bb2->bootFilePosition);
		MapVector(FstData, bb2->FSTPosition);
		MapVector(UserFilesData, bb2->userPosition);

		SwapArea(bb2, sizeof(DVDBB2));

		return true;
	}

	void MountDolphinSdk::SwapArea(void* _addr, int sizeInBytes)
	{
		uint32_t* addr = (uint32_t*)_addr;
		uint32_t* until = addr + sizeInBytes / sizeof(uint32_t);

		while (addr != until)
		{
			*addr = _byteswap_ulong(*addr);
			addr++;
		}
	}

	#pragma endregion "Data Generators"

}
