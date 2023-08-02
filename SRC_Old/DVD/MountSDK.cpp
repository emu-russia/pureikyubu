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
		wcscpy(directory, DolphinSDKPath);

		// Load dvddata structure.
		// TODO: Generate Json dynamically
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
				maxSize = my_min(requestedSize, (startingOffset + size) - offset);
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
				maxSize = my_min(requestedSize, (startingOffset + size) - offset);

				FILE* f;
				f = fopen( Util::WstringToString(file).c_str(), "rb");
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
		
		// First, try to enter the mapped binary blob, if it doesn't work, try the mapped file.

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
				// None of the options came up - return zeros.

				memset(buffer, 0, length);
				result = false;
			}
		}

		currentSeek += (uint32_t)length;
		return result;
	}

	#pragma region "Data Generators"

	// In addition to the actual files, the DVD image also contains a number of important binary data: DiskID, Apploader image, main program (DOL), BootInfo2 and BootBlock2 structures and FST.
	// Generating almost all blobs is straightforward, with the exception of the FST, which will have to tinker with.

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

		id->magicNumber = _BYTESWAP_UINT32(DVD_DISKID_MAGIC);

		GameName.resize(0x400);
		memset(GameName.data(), 0, GameName.size());
		strcpy((char *)GameName.data(), "GameCube SDK");

		return true;
	}

	bool MountDolphinSdk::GenApploader()
	{
		auto path = fmt::format(L"{:s}{:s}", directory, AppldrPath);
		AppldrData = Util::FileLoad(path);
		
		return true;
	}

	/// <summary>
	/// Unfortunately, all demos in the SDK are in ELF format. Therefore, we will use PONG.DOL as the main program, which is included in each Dolwin release and is a full resident of the project :p
	/// </summary>
	/// <returns></returns>
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

	/// <summary>
	/// Add a string with the name of the entry (directory or file name) to the NameTable.
	/// </summary>
	/// <param name="str"></param>
	void MountDolphinSdk::AddString(std::string str)
	{
		for (auto& c : str)
		{
			NameTableData.push_back(c);
		}

		NameTableData.push_back(0);
	}

	/// <summary>
	/// Process original Json with dvddata directory structure. The Json structure is designed to accommodate the weird FST feature when a directory is in the middle of files.
	/// For a more detailed description of this oddity, see `dolwin-docs\RE\DVD\FSTNotes.md`.
	/// The method is recursive tree descent.
	/// In the process, meta information is added to the original Json structure, which is used by the `WalkAndGenerateFst` method to create the final binary FST blob.
	/// </summary>
	/// <param name="entry"></param>
	void MountDolphinSdk::ParseDvdDataEntryForFst(Json::Value* entry)
	{
		Json::Value* parent = nullptr;

		if (entry->type != Json::ValueType::Object)
		{
			return;
		}

		if (entry->children.size() != 0)
		{
			// Directory

			// Save directory name offset

			size_t nameOffset = NameTableData.size();
			if (entry->name)
			{
				// Root has no name
				AddString(entry->name);
			}
			entry->AddInt("nameOffset", (int)nameOffset);
			entry->AddBool("dir", true);

			// Save current FST index for directory

			entry->AddInt("entryId", entryCounter);
			entryCounter++;

			// Reset totalChildren counter

			entry->AddInt("totalChildren", 0);
		}
		else
		{
			// File.
			// Differs from a directory in that it has no descendants

			assert(entry->name);
			std::string path = entry->name;

			size_t nameOffset = NameTableData.size();
			AddString(path);

			parent = entry->parent;

			// Save file name offset

			entry->AddInt("nameOffset", (int)nameOffset);

			// Generate full path to file

			do
			{
				if (parent->ByName("dir"))
				{
					path = (parent->name ? parent->name + std::string("/") : "/") + path;
				}
				parent = parent->parent;

			} while (parent != nullptr);

			assert(path.size() < DVD_MAXPATH);

			wchar_t filePath[0x1000] = { 0, };

			wcscat(filePath, directory);
			wcscat(filePath, FilesRoot);
			wchar_t* filePathPtr = filePath + wcslen(filePath);

			for (size_t i = 0; i < path.size(); i++)
			{
				*filePathPtr++ = (wchar_t)path[i];
			}
			*filePathPtr++ = 0;

			//Report(Channel::Norm, "Processing file: %s\n", path.c_str());

			// Save file offset

			entry->AddInt("fileOffset", userFilesStart + userFilesOffset);

			// Save file size

			size_t fileSize = Util::FileSize(filePath);
			entry->AddInt("fileSize", (int)fileSize);

			userFilesOffset += RoundUp32((uint32_t)fileSize);

			// Save file path

			entry->AddString("filePath", filePath);

			// Adjust entry counter

			entry->AddInt("entryId", entryCounter);
			entryCounter++;
		}

		// Update parent sibling counters

		parent = entry->parent;
		while (parent)
		{
			Json::Value* totalChildren = parent->ByName("totalChildren");

			if (totalChildren) totalChildren->value.AsInt++;

			parent = parent->parent;		// :p
		}

		// Recursively process descendants

		if (entry->ByName("dir") != nullptr)
		{
			for (auto it = entry->children.begin(); it != entry->children.end(); ++it)
			{
				ParseDvdDataEntryForFst(*it);
			}
		}
	}

	/// <summary>
	/// Based on the Json structure with the data of the dvddata directory tree, in which meta-information is added, the final FST binary blob is built.
	/// </summary>
	/// <param name="entry"></param>
	void MountDolphinSdk::WalkAndGenerateFst(Json::Value* entry)
	{
		DVDFileEntry fstEntry = { 0 };

		if (entry->type != Json::ValueType::Object)
		{
			return;
		}

		Json::Value* isDir = entry->ByName("dir");

		if (isDir)
		{
			// Directory

			fstEntry.isDir = 1;

			Json::Value* nameOffset = entry->ByName("nameOffset");
			assert(nameOffset);

			if (nameOffset)
			{
				fstEntry.nameOffsetHi = (uint8_t)(nameOffset->value.AsInt >> 16);
				fstEntry.nameOffsetLo = _BYTESWAP_UINT16((uint16_t)nameOffset->value.AsInt);
			}

			if (entry->parent)
			{
				Json::Value* parentId = entry->parent->ByName("entryId");
				if (parentId)
				{
					fstEntry.parentOffset = _BYTESWAP_UINT32((uint32_t)parentId->value.AsInt);
				}
			}

			Json::Value* entryId = entry->ByName("entryId");
			Json::Value* totalChildren = entry->ByName("totalChildren");

			if (entryId && totalChildren)
			{
				fstEntry.nextOffset = _BYTESWAP_UINT32((uint32_t)(entryId->value.AsInt + totalChildren->value.AsInt) + 1);
			}

			FstData.insert(FstData.end(), (uint8_t*)&fstEntry, (uint8_t*)&fstEntry + sizeof(fstEntry));

			if (logMount)
			{
				Report(Channel::Norm, "%d: directory: %s. nextOffset: %d\n",
					entryId->value.AsInt, entry->name, _BYTESWAP_UINT32(fstEntry.nextOffset));
			}

			for (auto it = entry->children.begin(); it != entry->children.end(); ++it)
			{
				WalkAndGenerateFst(*it);
			}
		}
		else
		{
			// File

			Json::Value* entryId = entry->ByName("entryId");
			Json::Value* nameOffsetValue = entry->ByName("nameOffset");
			Json::Value* fileOffsetValue = entry->ByName("fileOffset");
			Json::Value* fileSizeValue = entry->ByName("fileSize");
			assert(nameOffsetValue && fileOffsetValue && fileSizeValue);

			fstEntry.isDir = 0;

			uint32_t nameOffset = (uint32_t)(nameOffsetValue->value.AsInt);
			uint32_t fileOffset = (uint32_t)(fileOffsetValue->value.AsInt);
			uint32_t fileSize = (uint32_t)(fileSizeValue->value.AsInt);

			fstEntry.nameOffsetHi = (uint8_t)(nameOffset >> 16);
			fstEntry.nameOffsetLo = _BYTESWAP_UINT16((uint16_t)nameOffset);

			fstEntry.fileOffset = _BYTESWAP_UINT32(fileOffset);
			fstEntry.fileLength = _BYTESWAP_UINT32(fileSize);

			FstData.insert(FstData.end(), (uint8_t*)&fstEntry, (uint8_t*)&fstEntry + sizeof(fstEntry));

			if (logMount)
			{
				Report(Channel::Norm, "%d: file: %s\n", entryId->value.AsInt, entry->name);
			}
		}
	}

	// The basic idea behind generating FST is to walk by DvdDataJson.
	// When traversing a structure a specific meta-information is attached to each node.
	// After generation, this meta-information is collected in a final collection (FST).
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

		if (logMount)
		{
			JDI::Hub.Dump(DvdDataInfo.root.children.back());
		}

		try
		{
			WalkAndGenerateFst(DvdDataInfo.root.children.back());
		}
		catch (...)
		{
			Report(Channel::Norm, "WalkAndGenerateFst failed!\n");
			return false;
		}

		// Add Name Table to the end

		FstData.insert(FstData.end(), NameTableData.begin(), NameTableData.end());

		Util::FileSave(L"Data/DolphinSdkFST.bin", FstData);

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
		if (entry->type != Json::ValueType::Object)
		{
			return;
		}

		Json::Value* isDir = entry->ByName("dir");

		if (!isDir)
		{
			Json::Value* filePath = entry->ByName("filePath");
			Json::Value* fileOffset = entry->ByName("fileOffset");
			assert(filePath && fileOffset);

			MapFile(filePath->value.AsString, (uint32_t)(fileOffset->value.AsInt));
		}
		else
		{
			for (auto it = entry->children.begin(); it != entry->children.end(); ++it)
			{
				WalkAndMapFiles(*it);
			}
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
			*addr = _BYTESWAP_UINT32(*addr);
			addr++;
		}
	}

	#pragma endregion "Data Generators"

}
