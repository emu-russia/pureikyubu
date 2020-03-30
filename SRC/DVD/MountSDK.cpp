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

		// Generate mapping

		if (!GenMap())
		{
			DBReport("Failed to GenMap\n");
			return;
		}
		if (!GenFileMap())
		{
			DBReport("Failed to GenFileMap\n");
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

	void MountDolphinSdk::MapFile(TCHAR* path, uint32_t offset)
	{
		size_t size = UI::FileSize(path);
		std::tuple<TCHAR*, uint32_t, size_t> entry(path, offset, size);
		fileMapping.push_back(entry);
	}

	// Check memory mapping
	uint8_t* MountDolphinSdk::TranslateMemory(uint32_t offset, size_t requestedSize, size_t& maxSize)
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

	// Check file mapping
	FILE * MountDolphinSdk::TranslateFile(uint32_t offset, size_t requestedSize, size_t& maxSize)
	{
		for (auto it = fileMapping.begin(); it != fileMapping.end(); ++it)
		{
			TCHAR* file = std::get<0>(*it);
			uint32_t startingOffset = std::get<1>(*it);
			size_t size = std::get<2>(*it);

			if (startingOffset <= offset && offset < (startingOffset + size))
			{
				maxSize = min(requestedSize, (startingOffset + size) - offset);

				FILE* f;
				_tfopen_s(&f, file, _T("rb"));
				assert(f);

				fseek(f, offset - startingOffset, SEEK_SET);
				return f;
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
		
		uint8_t* ptr = TranslateMemory(currentSeek, length, maxLength);
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
			FILE* f = TranslateFile(currentSeek, length, maxLength);
			if (f != nullptr)
			{
				fread(buffer, 1, maxLength, f);
				if (maxLength < length)
				{
					memset((uint8_t*)buffer + maxLength, 0, length - maxLength);
				}
				fclose(f);
			}
			else
			{
				memset(buffer, 0, length);
			}
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

	void MountDolphinSdk::AddString(std::string str)
	{
		for (int i = 0; i < str.size(); i++)
		{
			NameTableData.push_back(str[i]);
		}
		NameTableData.push_back(0);
	}

	void MountDolphinSdk::ParseDvdDataEntryForFst(Json::Value* entry)
	{
		if (entry->type == Json::ValueType::Object)
		{
			// Directory

			// Save directory name offset
			size_t nameOffset = NameTableData.size();
			if (entry->name)
			{
				AddString(entry->name);
			}
			entry->AddInt("nameOffset", (int)nameOffset);

			// Save parent and next directory FST index

		}
		else if (entry->type == Json::ValueType::String)
		{
			bool skipMeta = false;

			if (entry->parent->name && entry->parent->type == Json::ValueType::Array)
			{
				if (!_stricmp(entry->parent->name, "filePaths"))
				{
					skipMeta = true;
				}
			}

			// File

			if (!skipMeta)
			{
				std::string path = Debug::Hub.TcharToString(entry->value.AsString);

				size_t nameOffset = NameTableData.size();
				AddString(path);

				Json::Value* parent = entry->parent;

				// Save file name offset
				Json::Value* nameOffsets = entry->parent->parent->ByName("nameOffsets");
				if (nameOffsets == nullptr)
				{
					nameOffsets = entry->parent->parent->AddArray("nameOffsets");
				}
				assert(nameOffsets);

				nameOffsets->AddInt(nullptr, (int)nameOffset);

				do
				{
					if (parent)
					{
						if (parent->type == Json::ValueType::Object)
						{
							path = (parent->name ? parent->name + std::string("/") : "/") + path;
						}

						parent = parent->parent;
					}

				} while (parent != nullptr);

				assert(path.size() < DVD_MAXPATH);

				// Save file offset and size

				//DBReport("Processing file: %s\n", path.c_str());

				TCHAR filePath[0x1000] = { 0, };

				_tcscat_s(filePath, _countof(filePath) - 1, directory);
				_tcscat_s(filePath, _countof(filePath) - 1, _T("/dvddata"));
				TCHAR* filePathPtr = filePath + _tcslen(filePath);

				for (int i = 0; i < path.size(); i++)
				{
					*filePathPtr++ = (TCHAR)path[i];
				}
				*filePathPtr++ = 0;

				Json::Value* fileOffsets = entry->parent->parent->ByName("fileOffsets");
				if (fileOffsets == nullptr)
				{
					fileOffsets = entry->parent->parent->AddArray("fileOffsets");
				}
				assert(fileOffsets);

				fileOffsets->AddInt(nullptr, userFilesStart + userFilesOffset);

				size_t fileSize = UI::FileSize(filePath);

				Json::Value* fileSizes = entry->parent->parent->ByName("fileSizes");
				if (fileSizes == nullptr)
				{
					fileSizes = entry->parent->parent->AddArray("fileSizes");
				}
				assert(fileSizes);

				fileSizes->AddInt(nullptr, (int)fileSize);

				userFilesOffset += RoundUp32((uint32_t)fileSize);

				Json::Value* filePaths = entry->parent->parent->ByName("filePaths");
				if (filePaths == nullptr)
				{
					filePaths = entry->parent->parent->AddArray("filePaths");
				}
				assert(filePaths);

				filePaths->AddString(nullptr, filePath);
			}
		}

		for (auto it = entry->children.begin(); it != entry->children.end(); ++it)
		{
			ParseDvdDataEntryForFst(*it);
		}
	}

	void MountDolphinSdk::WalkAndGenerateFst(Json::Value* entry)
	{
		DVDFileEntry fstEntry = { 0 };

		if (entry->type == Json::ValueType::Object)
		{
			// Directory

			fstEntry.isDir = 1;

			Json::Value* nameOffset = entry->ByName("nameOffset");
			assert(nameOffset);

			if (nameOffset)
			{
				fstEntry.nameOffsetHi = (uint8_t)(nameOffset->value.AsInt >> 16);
				fstEntry.nameOffsetLo = _byteswap_ushort((uint16_t)nameOffset->value.AsInt);
			}
		}
		else if (entry->type == Json::ValueType::Array)
		{
			// Files

			if (!_stricmp(entry->name, "files"))
			{
				Json::Value* nameOffsets = entry->parent->ByName("nameOffsets");
				Json::Value* fileOffsets = entry->parent->ByName("fileOffsets");
				Json::Value* fileSizes = entry->parent->ByName("fileSizes");
				assert(nameOffsets && fileOffsets && fileSizes);

				if (nameOffsets && fileOffsets && fileSizes)
				{
					auto nameOffsetsIt = nameOffsets->children.begin();
					auto fileOffsetsIt = fileOffsets->children.begin();
					auto fileSizesIt = fileSizes->children.begin();

					while (nameOffsetsIt != nameOffsets->children.end())
					{
						fstEntry.isDir = 0;

						uint32_t nameOffset = (uint32_t)(*nameOffsetsIt)->value.AsInt;
						uint32_t fileOffset = (uint32_t)(*fileOffsetsIt)->value.AsInt;
						uint32_t fileSize = (uint32_t)(*fileSizesIt)->value.AsInt;

						fstEntry.nameOffsetHi = (uint8_t)(nameOffset >> 16);
						fstEntry.nameOffsetLo = _byteswap_ushort((uint16_t)nameOffset);

						fstEntry.fileOffset = _byteswap_ulong(fileOffset);
						fstEntry.fileLength = _byteswap_ulong(fileSize);

						nameOffsetsIt++;
						fileOffsetsIt++;
						fileSizesIt++;
					}
				}
			}

			return;
		}

		FstData.insert(FstData.end(), (uint8_t *)&fstEntry, (uint8_t*)&fstEntry + sizeof(fstEntry));

		for (auto it = entry->children.begin(); it != entry->children.end(); ++it)
		{
			WalkAndGenerateFst(*it);
		}
	}

	// The basic idea behind generating FST is to walk by DvdDataJson.
	// When traversing a structure a specific userData is attached to each node.
	// After generation, this userData is collected in a common collection (FST).
	bool MountDolphinSdk::GenFst()
	{
		try
		{
			ParseDvdDataEntryForFst(DvdDataInfo.root.children.back());
		}
		catch (...)
		{
			DBReport("ParseDvdDataEntryForFst failed!\n");
			return false;
		}

		//Debug::Hub.Dump(DvdDataInfo.root.children.back());

		try
		{
			WalkAndGenerateFst(DvdDataInfo.root.children.back());
		}
		catch (...)
		{
			DBReport("WalkAndGenerateFst failed!\n");
			return false;
		}

		FstData.insert(FstData.end(), NameTableData.begin(), NameTableData.end());

		//UI::FileSave(_T("Data\\FST.bin"), FstData.data(), FstData.size());

		return true;
	}

	bool MountDolphinSdk::GenBb2()
	{
		DVDBB2 bb2 = { 0 };

		bb2.bootFilePosition = RoundUpSector(DVD_APPLDR_OFFSET + (uint32_t)AppldrData.size());
		bb2.FSTLength = (uint32_t)FstData.size();
		bb2.FSTMaxLength = bb2.FSTLength;
		bb2.FSTPosition = RoundUpSector(bb2.bootFilePosition + (uint32_t)Dol.size() + DVD_SECTOR_SIZE);
		bb2.userPosition = 0;
		bb2.userLength = 0;

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

		SwapArea(bb2, sizeof(DVDBB2));

		return true;
	}

	void MountDolphinSdk::WalkAndMapFiles(Json::Value* entry)
	{
		if (entry->type == Json::ValueType::Array)
		{
			// Files

			if (!_stricmp(entry->name, "files"))
			{
				Json::Value* filePaths = entry->parent->ByName("filePaths");
				Json::Value* fileOffsets = entry->parent->ByName("fileOffsets");
				assert(filePaths && fileOffsets);

				if (filePaths && fileOffsets)
				{
					auto filePathsIt = filePaths->children.begin();
					auto fileOffsetsIt = fileOffsets->children.begin();

					while (filePathsIt != filePaths->children.end())
					{
						MapFile((*filePathsIt)->value.AsString, (*fileOffsetsIt)->value.AsInt);

						filePathsIt++;
						fileOffsetsIt++;
					}
				}
			}

			return;
		}

		for (auto it = entry->children.begin(); it != entry->children.end(); ++it)
		{
			WalkAndGenerateFst(*it);
		}
	}

	bool MountDolphinSdk::GenFileMap()
	{
		userFilesOffset = 0;

		try
		{
			WalkAndMapFiles(DvdDataInfo.root.children.back());
		}
		catch (...)
		{
			DBReport("WalkAndMapFiles failed!\n");
			return false;
		}

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
