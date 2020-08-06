/*
Code for mounting the Dolphin SDK folder as a virtual disk.

All the necessary data (BI2, Appldr, some DOL executable, we take from the SDK). If they are not there, then the disk is simply not mounted.
*/

#include "pch.h"

using namespace Debug;

namespace DVD
{
	MountDolphinSdk::MountDolphinSdk(const wchar_t * DolphinSDKPath)
	{
		wcscpy_s(directory, _countof(directory) - 1, DolphinSDKPath);

		// Load dvddata structure.
		auto dvdDataInfoText = Util::FileLoad(DvdDataJson);
		if (dvdDataInfoText.empty())
		{
			Report(Channel::Norm, "Failed to load DolphinSDK dvddata json: %s\n", Util::WstringToString(DvdDataJson).c_str());
			return;
		}

		try
		{
			DvdDataInfo.Deserialize(dvdDataInfoText.data(), dvdDataInfoText.size());
		}
		catch (...)
		{
			Report(Channel::Norm, "Failed to Deserialize DolphinSDK dvddata json: %s\n", Util::WstringToString(DvdDataJson).c_str());
			return;
		}

		// Generate data blobs
		if (!GenDiskId())
		{
			Report(Channel::Norm, "Failed to GenDiskId\n");
			return;
		}
		if (!GenApploader())
		{
			Report(Channel::Norm, "Failed to GenApploader\n");
			return;
		}
		if (!GenBi2())
		{
			Report(Channel::Norm, "Failed to GenBi2\n");
			return;
		}
		if (!GenFst())
		{
			Report(Channel::Norm, "Failed to GenFst\n");
			return;
		}
		if (!GenDol())
		{
			Report(Channel::Norm, "Failed to GenDol\n");
			return;
		}
		if (!GenBb2())
		{
			Report(Channel::Norm, "Failed to GenBb2\n");
			return;
		}

		// Generate mapping

		if (!GenMap())
		{
			Report(Channel::Norm, "Failed to GenMap\n");
			return;
		}
		if (!GenFileMap())
		{
			Report(Channel::Norm, "Failed to GenFileMap\n");
			return;
		}

		Report(Channel::DVD, "DolphinSDK mounted!\n");
		mounted = true;
	}

	MountDolphinSdk::~MountDolphinSdk()
	{
	}

	void MountDolphinSdk::MapVector(std::vector<uint8_t>& v, uint32_t offset)
	{
		std::tuple<std::vector<uint8_t>&, uint32_t, size_t> entry(v, offset, v.size());
		mapping.push_back(entry);
	}

	void MountDolphinSdk::MapFile(wchar_t* path, uint32_t offset)
	{
		size_t size = Util::FileSize(path);
		std::tuple<wchar_t*, uint32_t, size_t> entry(path, offset, size);
		fileMapping.push_back(entry);
	}

	// Check memory mapping
	uint8_t* MountDolphinSdk::TranslateMemory(uint32_t offset, size_t requestedSize, size_t& maxSize)
	{
		for (auto it = mapping.begin(); it != mapping.end(); ++it)
		{
			uint8_t * ptr = std::get<0>(*it).data();
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
			wchar_t* file = std::get<0>(*it);
			uint32_t startingOffset = std::get<1>(*it);
			size_t size = std::get<2>(*it);

			if (startingOffset <= offset && offset < (startingOffset + size))
			{
				maxSize = min(requestedSize, (startingOffset + size) - offset);

				FILE* f;
				_wfopen_s(&f, file, L"rb");
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

	bool MountDolphinSdk::Read(void* buffer, size_t length)
	{
		bool result = true;

		assert(buffer);

		if (!mounted)
		{
			memset(buffer, 0, length);
			return true;
		}

		if (currentSeek >= DVD_SIZE)
		{
			memset(buffer, 0, length);
			return false;
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
				result = false;
			}
		}

		currentSeek += (uint32_t)length;
		return result;
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
		strcpy_s((char *)GameName.data(), 0x100, "GameCube SDK");

		return true;
	}

	bool MountDolphinSdk::GenApploader()
	{
		auto path = fmt::format(L"{:s}{:s}", directory, AppldrPath);
		AppldrData = Util::FileLoad(path);
		
		return true;
	}

	bool MountDolphinSdk::GenDol()
	{
		Dol = Util::FileLoad(DolPath);

		return true;
	}

	bool MountDolphinSdk::GenBi2()
	{
		auto path = fmt::format(L"{:s}{:s}", directory, Bi2Path);
		Bi2Data = Util::FileLoad(path);

		return true;
	}

	void MountDolphinSdk::AddString(std::string str)
	{
		for (auto& c : str)
		{
			NameTableData.push_back(c);
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

			// Save current FST index for directory

			entry->AddInt("entryId", entryCounter);
			entryCounter++;

			// Reset totalChildren counter
			entry->AddInt("totalChildren", 0);

			// Update parent totalChildren counters
			Json::Value * parent = entry->parent;
			while (parent)
			{
				Json::Value* totalChildren = parent->ByName("totalChildren");

				if (totalChildren) totalChildren->value.AsInt++;

				parent = parent->parent;		// :p
			}
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
				std::string path = Util::WstringToString(entry->value.AsString);

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

				wchar_t filePath[0x1000] = { 0, };

				wcscat_s(filePath, _countof(filePath) - 1, directory);
				wcscat_s(filePath, _countof(filePath) - 1, L"/dvddata");
				wchar_t* filePathPtr = filePath + wcslen(filePath);

				for (size_t i = 0; i < path.size(); i++)
				{
					*filePathPtr++ = (wchar_t)path[i];
				}
				*filePathPtr++ = 0;

				Json::Value* fileOffsets = entry->parent->parent->ByName("fileOffsets");
				if (fileOffsets == nullptr)
				{
					fileOffsets = entry->parent->parent->AddArray("fileOffsets");
				}
				assert(fileOffsets);

				fileOffsets->AddInt(nullptr, userFilesStart + userFilesOffset);

				size_t fileSize = Util::FileSize(filePath);

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
				
				// Adjust counters
				entryCounter++;

				// Update parent sibling counters
				parent = entry->parent;
				while (parent)
				{
					Json::Value* totalChildren = parent->ByName("totalChildren");

					if (totalChildren) totalChildren->value.AsInt++;

					parent = parent->parent;		// :p
				}
			}
		}

		Json::Value* lastObject = nullptr;

		for (auto it = entry->children.begin(); it != entry->children.end(); ++it)
		{
			Json::Value * child = *it;
			ParseDvdDataEntryForFst(child);
			if (child->type == Json::ValueType::Object)
			{
				lastObject = child;
			}
		}

		if (lastObject)
		{
			lastObject->AddBool("last", true);
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

			if (entry->parent)
			{
				Json::Value* parentId = entry->parent->ByName("entryId");
				if (parentId)
					fstEntry.parentOffset = _byteswap_ulong((uint32_t)parentId->value.AsInt);
			}

			Json::Value* entryId = entry->ByName("entryId");
			Json::Value* totalChildren = entry->ByName("totalChildren");

			if (entryId && totalChildren)
			{
				Json::Value* last = entry->ByName("last");

				fstEntry.nextOffset = _byteswap_ulong((uint32_t)(entryId->value.AsInt + totalChildren->value.AsInt) + 1);
			}

			FstData.insert(FstData.end(), (uint8_t*)&fstEntry, (uint8_t*)&fstEntry + sizeof(fstEntry));
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

						FstData.insert(FstData.end(), (uint8_t*)&fstEntry, (uint8_t*)&fstEntry + sizeof(fstEntry));

						nameOffsetsIt++;
						fileOffsetsIt++;
						fileSizesIt++;
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
			Report(Channel::Norm, "ParseDvdDataEntryForFst failed!\n");
			return false;
		}

		JDI::Hub.Dump(DvdDataInfo.root.children.back());

		try
		{
			WalkAndGenerateFst(DvdDataInfo.root.children.back());
		}
		catch (...)
		{
			Report(Channel::Norm, "WalkAndGenerateFst failed!\n");
			return false;
		}

		FstData.insert(FstData.end(), NameTableData.begin(), NameTableData.end());

		Util::FileSave(L"Data\\DolphinSdkFST.bin", FstData);

		return true;
	}

	bool MountDolphinSdk::GenBb2()
	{
		DVDBB2 bb2 = { 0 };

		bb2.bootFilePosition = RoundUpSector(DVD_APPLDR_OFFSET + (uint32_t)AppldrData.size());
		bb2.FSTLength = (uint32_t)FstData.size();
		bb2.FSTMaxLength = bb2.FSTLength;
		bb2.FSTPosition = RoundUpSector(bb2.bootFilePosition + (uint32_t)Dol.size() + DVD_SECTOR_SIZE);
		bb2.userPosition = 0x80030000;		// Ignored
		bb2.userLength = RoundUpSector(bb2.FSTLength);

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
						MapFile((*filePathsIt)->value.AsString, (uint32_t)(*fileOffsetsIt)->value.AsInt);

						filePathsIt++;
						fileOffsetsIt++;
					}
				}
			}

			return;
		}

		for (auto it = entry->children.begin(); it != entry->children.end(); ++it)
		{
			WalkAndMapFiles(*it);
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
			Report(Channel::Norm, "WalkAndMapFiles failed!\n");
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
