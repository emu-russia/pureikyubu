// DVD API for emulator
#include "pch.h"
#include "rvz.h"

DVDControl dvd;

using namespace Debug;

namespace DVD
{
	// Mount current dvd 
	bool MountFile(const wchar_t* file)
	{
		// try to open file
		if (!Util::FileExists(file))
		{
			return false;
		}

		Unmount();

		// select current DVD
		bool res = GCMMountFile(file);
		if (!res)
			return res;

		// init filesystem
		res = dvd_fs_init();
		if (!res)
		{
			GCMMountFile(nullptr);
			return res;
		}

		Seek(0);

		return true;
	}

	bool MountFile(const std::string& file)
	{
		wchar_t path[0x1000] = { 0, };
		wchar_t* tcharPtr = path;
		char* ansiPtr = (char *)file.c_str();
		while (*ansiPtr)
		{
			*tcharPtr++ = *ansiPtr++;
		}
		*tcharPtr++ = 0;
		return MountFile(path);
	}

	bool MountFile(const std::wstring& file)
	{
		wchar_t path[0x1000] = { 0, };
		wchar_t* tcharPtr = path;
		wchar_t* widePtr = (wchar_t*)file.c_str();
		while (*widePtr)
		{
			*tcharPtr++ = *widePtr++;
		}
		*tcharPtr++ = 0;
		return MountFile(path);
	}

	bool MountSdk(const wchar_t* path)
	{
		Unmount();

		dvd.mountedSdk = new MountDolphinSdk(path);

		if (!dvd.mountedSdk->Mounted())
		{
			delete dvd.mountedSdk;
			dvd.mountedSdk = nullptr;
			return false;
		}

		// init filesystem
		if (!dvd_fs_init())
		{
			delete dvd.mountedSdk;
			dvd.mountedSdk = nullptr;
			return false;
		}

		dvd.mountedSdk->Seek(0);

		return true;
	}

	bool MountSdk(std::string path)
	{
		wchar_t tcharStr[0x1000] = { 0, };
		wchar_t* tcharPtr = tcharStr;
		char* ansiPtr = (char*)path.c_str();
		while (*ansiPtr)
		{
			*tcharPtr++ = *ansiPtr++;
		}
		*tcharPtr++ = 0;
		return MountSdk(tcharStr);
	}

	// Unmount
	void Unmount()
	{
		GCMMountFile(nullptr);

		if (dvd.mountedSdk)
		{
			delete dvd.mountedSdk;
			dvd.mountedSdk = nullptr;
		}

		dvd_fs_shutdown();
	}

	bool IsMounted()
	{
		return (dvd.mountedImage || dvd.mountedSdk != nullptr);
	}

	// dvd operations on current mounted dvd

	void Seek(int position)
	{
		if (dvd.mountedImage)
		{
			GCMSeek(position);
		}
		else if (dvd.mountedSdk)
		{
			dvd.mountedSdk->Seek(position);
		}
	}

	int GetSeek()
	{
		if (dvd.mountedImage)
		{
			return dvd.seekval;
		}
		else if (dvd.mountedSdk)
		{
			return dvd.mountedSdk->GetSeek();
		}
		else return 0;
	}

	bool Read(void *buffer, size_t length)
	{
		if (length == 0)
			return true;

		if (dvd.mountedImage)
		{
			return GCMRead((uint8_t*)buffer, length);
		}
		else if (dvd.mountedSdk)
		{
			return dvd.mountedSdk->Read((uint8_t*)buffer, length);
		}
		else
		{
			memset(buffer, 0, length);        // fill by zeroes
		}

		return true;
	}

	long OpenFile(std::string& dvdfile)
	{
		if (dvd.mountedImage || dvd.mountedSdk)
		{
			// call DVD filesystem open
			return dvd_open(dvdfile.data());
		}

		// Not mounted
		return 0;
	}

	// Call somewhere

	void InitSubsystem()
	{
		JDI::Hub.AddNode(L"DDU_JDI_JSON", JdiSpecs::DduJdi, DvdCommandsReflector);

		DDU = new DduCore;
	}

	void ShutdownSubsystem()
	{
		Unmount();
		JDI::Hub.RemoveNode(L"DDU_JDI_JSON");
		delete DDU;
	}

}


/*
Code for mounting the Dolphin SDK folder as a virtual disk.

All the necessary data (BI2, Appldr, some DOL executable, we take from the SDK). If they are not there, then the disk is simply not mounted.
*/

namespace DVD
{
	const char* dvdDataInfoText = R"json(

{
	"audio":
	{
		"tracks":
		{
			"dtk_car.adp": {},
			"dtk_gs.adp": {},
			"dtk_spl.adp": {}
		}
	},
	
	"axdemo":
	{
		"axart":
		{
			"axartdemo.spd": {},
			"axartdemo.spt": {}
		},
		"midi":
		{
			"2nd_time.mid": {},
			"annie.mid": {},
			"autumnal.mid": {},
			"candy.mid": {},
			"j_cycle.mid": {},
			"worm.mid": {}
		},
		"stream":
		{
			"left.adpcm": {},
			"left.pcm16": {},
			"left.pcm8": {},
			"right.adpcm": {},
			"right.pcm16": {},
			"right.pcm8": {}
		},
		"synth":
		{
			"gm16adpcm.pcm": {},
			"gm16adpcm.wt": {},
			"gm16pcm.pcm": {},
			"gm16pcm.wt": {}
		},
		"tests":
		{
			"heli_60.pcm8": {},
			"sine_64.pcm16": {},
			"siren.adpcm": {}
		}
	},

	"carddemo":
	{
		"banner.tpl": {},
		"gamecube.tpl": {},
		"heiho.tpl": {},
		"heihoC8.tpl": {},
		"icon.tpl": {},
		"luigi.tpl": {},
		"marioCI.tpl": {},
		"yoshi.tpl": {}
	},

	"constitu":
	{
		"article1":
		{
			"section1": {},
			"section2": {},
			"section3": {},
			"section4": {},
			"section5": {},
			"section6": {},
			"section7": {},
			"section8": {},
			"section9": {},
			"sectionA": {}
		},
		"article2":
		{
			"section1": {},
			"section2": {},
			"section3": {},
			"section4": {}
		},
		"article3":
		{
			"section1": {},
			"section2": {},
			"section3": {}
		}
	},

	"gddemo":
	{
		"gdInit.gdl": {},
		"gdLight.gdl": {},
		"gdMatrix.gdl": {},
		"gdTev.gdl": {},
		"gdTextr.gdl": {}
	},

	"gxTests":
	{
		"dnt-02.tpl": {},
		"dnt-03.tpl": {},
		"frb-00.tpl": {},
		"g2d-00.tpl": {},
		"geo-00.tpl": {},
		"lit-06.tpl": {},
		"lit-10.tpl": {},
		"pseudo.tpl": {},
		"tev-00.tpl": {},
		"tev-01.tpl": {},
		"tev-02.tpl": {},
		"tev-03.tpl": {},
		"tev-04.tpl": {},
		"tex-01.tpl": {},

		"tex-02":
		{
			"ci8_1.tpl": {},
			"ci8_2.tpl": {},
			"cmp_1.tpl": {},
			"cmp_1mm.tpl": {},
			"cmp_even.tpl": {},
			"i4_1.tpl": {},
			"i4_1mm.tpl": {},
			"i4_odd.tpl": {},
			"i8_1.tpl": {},
			"i8_1mm.tpl": {},
			"i8_odd.tpl": {},
			"ia4_1.tpl": {},
			"ia4_1mm.tpl": {},
			"ia4_odd.tpl": {},
			"ia8_1.tpl": {},
			"ia8_1mm.tpl": {},
			"ia8_odd.tpl": {},
			"rgb565mm.tpl": {},
			"rgb565od.tpl": {},
			"rgb565_1.tpl": {},
			"rgb5a3mm.tpl": {},
			"rgb5a3od.tpl": {},
			"rgb5a3_1.tpl": {},
			"rgba8mm.tpl": {},
			"rgba8od.tpl": {},
			"rgba8_1.tpl": {}
		},

		"tex-03.tpl": {},
		"tex-05.tpl": {},
		"tex-06.tpl": {},
		"tex-07.tpl": {},
		"tex-08.tpl": {},
		"tf-02.tpl": {},
		"tg-01.tpl": {},
		"tg-02.tpl": {},
		"tg-cube.tpl": {},
		"tg-cube1.tpl": {},
		"tg-dual.tpl": {},
		"tg-pc.tpl": {}
	},

	"gxTextrs.tpl": {},	

	"opening.bnr": {},	

	"pictures":
	{
		"clrbars7.BMP": {},
		"dump.bmp": {},
		"gray256b.BMP": {},
		"gray3.BMP": {},
		"marina2.BMP": {},
		"rainbow.BMP": {},
		"venice3.BMP": {}
	},

	"spdemo":
	{
		"spdemo.spd": {},
		"spdemo.spt": {}
	},

	"texts":
	{
		"test1.txt": {},
		"test2.txt": {}
	}

}

)json";

	MountDolphinSdk::MountDolphinSdk(const wchar_t* DolphinSDKPath)
	{
		wcscpy(directory, DolphinSDKPath);

		try
		{
			DvdDataInfo.Deserialize((void *)dvdDataInfoText, strlen(dvdDataInfoText));
		}
		catch (...)
		{
			Report(Channel::Norm, "Failed to Deserialize DolphinSDK dvddata json\n");
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
			uint8_t* ptr = std::get<0>(*it).data();
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
	FILE* MountDolphinSdk::TranslateFile(uint32_t offset, size_t requestedSize, size_t& maxSize)
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
				f = fopen(Util::WstringToString(file).c_str(), "rb");
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
				memset((uint8_t*)buffer + maxLength, 0, length - maxLength);
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
		strcpy((char*)GameName.data(), "GameCube SDK");

		return true;
	}

	bool MountDolphinSdk::GenApploader()
	{
		AppldrData = Util::FileLoad(std::wstring(directory) + std::wstring(AppldrPath));

		return true;
	}

	/// <summary>
	/// Unfortunately, all demos in the SDK are in ELF format. Therefore, we will use PONG.DOL as the main program, which is included in each release and is a full resident of the project :p
	/// </summary>
	/// <returns></returns>
	bool MountDolphinSdk::GenDol()
	{
		Dol = Util::FileLoad(DolPath);

		return true;
	}

	bool MountDolphinSdk::GenBi2()
	{
		Bi2Data = Util::FileLoad(std::wstring(directory) + std::wstring(Bi2Path));

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

		DVDBB2* bb2 = (DVDBB2*)Bb2Data.data();

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


// Disk region detection

namespace DVD
{
	// Get region by DiskID (not a very reliable method)
	Region RegionById(const char* DiskId)
	{
		switch (DiskId[3])
		{
			case 'P':
			case 'Y':
				return Region::EUR;
			case 'D':
				return Region::NOE;
			case 'F':
				return Region::FRA;
			case 'S':
				return Region::ESP;
			case 'I':
				return Region::ITA;
			case 'X':
			case 'K':
				return Region::FAH;
			case 'H':
				return Region::HOL;
			case 'U':
				return Region::AUS;

			case 'J':
				return Region::JPN;

			case 'E':
				return Region::USA;
			case 'W':
				return Region::KOR;
		}

		return Region::Unknown;
	}

	// Check that the region is NTSC-like
	bool IsNtsc(Region region)
	{
		switch (region)
		{
			case Region::JPN:
			case Region::USA:
			case Region::KOR:
				return true;
		}
		return false;
	}

}


// DVD filesystem access.

// local data
static DVDBB2           bb2;
static DVDFileEntry* FstStart;            // Loaded FST (byte-swapped as little-endian)
static uint32_t         fstSize;        // Size of loaded FST in bytes (not greater DVD_FST_MAX_SIZE)
static char* FstStringStart;          // Strings(name) table

#define FSTOFS(lo, hi) (((uint32_t)hi << 16) | lo)

// Swap longs (no need in assembly, used by tools)
static void SwapArea(uint32_t* addr, int count)
{
	uint32_t* until = addr + count / sizeof(uint32_t);

	while (addr != until)
	{
		*addr = _BYTESWAP_UINT32(*addr);
		addr++;
	}
}

// swap bytes in FST (little-endian)
// return beginning of strings table (or NULL, if bad FST)
static char *fst_prepare(DVDFileEntry *root)
{
	char* nameTablePtr = nullptr;

	root->nameOffsetLo = _BYTESWAP_UINT16(root->nameOffsetLo);
	root->fileOffset   = _BYTESWAP_UINT32(root->fileOffset);
	root->fileLength   = _BYTESWAP_UINT32(root->fileLength);

	// Check root: must have no parent, has zero nameOfset and non-zero nextOffset.
	if (! ( root->isDir && 
		root->parentOffset == 0 && 
		FSTOFS(root->nameOffsetLo, root->nameOffsetHi) == 0 && 
		root->nextOffset != 0 ) )
	{
		return nullptr;
	}

	nameTablePtr = (char*)&root[root->nextOffset];

	// Starting from next after root
	for (uint32_t i = 1; i < root->nextOffset; i++)
	{
		DVDFileEntry* entry = &root[i];

		entry->nameOffsetLo = _BYTESWAP_UINT16(entry->nameOffsetLo);
		entry->fileOffset = _BYTESWAP_UINT32(entry->fileOffset);
		entry->fileLength = _BYTESWAP_UINT32(entry->fileLength);
	}

	return nameTablePtr;
}

// initialize filesystem
bool dvd_fs_init()
{
	// Check DiskID

	char diskId[5] = { 0 };

	DVD::Seek(DVD_ID_OFFSET);
	DVD::Read(diskId, 4);

	for (int i = 0; i < 4; i++)
	{
		if (!isalnum(diskId[i]))
		{
			return false;
		}
	}

	// Check Apploader

	char apploaderBytes[5] = { 0 };

	DVD::Seek(DVD_APPLDR_OFFSET);
	DVD::Read(apploaderBytes, 4);

	for (int i = 0; i < 4; i++)
	{
		if (!isdigit(apploaderBytes[i]))
		{
			return false;
		}
	}

	// load tables
	DVD::Seek(DVD_BB2_OFFSET);
	DVD::Read(&bb2, sizeof(DVDBB2));
	SwapArea((uint32_t *)&bb2, sizeof(DVDBB2));

	// delete previous FST
	if(FstStart)
	{
		free(FstStart);
		FstStart = NULL;
		fstSize = 0;
	}

	// create new FST
	fstSize = bb2.FSTLength;
	if(fstSize > DVD_FST_MAX_SIZE)
	{
		return false;
	}
	FstStart = (DVDFileEntry *)malloc(fstSize);
	if(FstStart == NULL)
	{
		return false;
	}
	DVD::Seek(bb2.FSTPosition);
	DVD::Read(FstStart, fstSize);
		
	// swap bytes in FST and find offset of string table
	FstStringStart = fst_prepare(FstStart);
	if(!FstStringStart)
	{
		free(FstStart);
		FstStart = NULL;
		return false;
	}

	// FST loaded ok
	return true;
}

void dvd_fs_shutdown()
{
	if (FstStart)
	{
		free(FstStart);
		FstStart = NULL;
		fstSize = 0;
	}
}

// Based on reversing of original method.
// <0: Bad path
static int DVDConvertPathToEntrynum(const char* _path)
{
	char* path = (char *)_path;

	// currentDirectory assigned by DVDChangeDir
	int entry = 0;         // running entry

	// Loop1
	while (true)
	{
		if (path[0] == 0)
			return entry;

		// Current/parent directory walk

		if (path[0] == '/')
		{
			entry = 0;      // root
			path++;
			continue;   // Loop1
		}

		if (path[0] == '.')
		{
			if (path[1] == '.')
			{
				if (path[2] == '/')
				{
					entry = FstStart[entry].parentOffset;
					path += 3;
					continue;   // Loop1
				}
				if (path[2] == 0)
				{
					return FstStart[entry].parentOffset;
				}
			}
			else
			{
				if (path[1] == '/')
				{
					path += 2;
					continue;   // Loop1
				}
				if (path[1] == 0)
				{
					return entry;
				}
			}
		}

		// Get a pointer to the end of a file or directory name (the end is 0 or /)
		char* endPathPtr;

		if (true)
		{
			endPathPtr = path;
			while (!(endPathPtr[0] == 0 || endPathPtr[0] == '/'))
			{
				endPathPtr++;
			}
		}

		// if-else Block 2

		bool afterNameCharNZ = endPathPtr[0] != 0;      // after-name character != 0
		int prevEntry = entry;          // Save previous entry
		size_t nameSize = endPathPtr - path;        // path element nameSize
		entry++;              // Increment entry

		// Loop2
		while (true)
		{
			if ((int)FstStart[prevEntry].nextOffset <= entry)   // Walk forward only
				return -1;      // Bad FST

			// Loop2 - Group 1  -- Compare names
			if (FstStart[entry].isDir || afterNameCharNZ == false /* after-name is 0 */)
			{
				char* r21 = path;      // r21 -- current pathPtr to inner loop
				int nameOffset = (FstStart[entry].nameOffsetHi << 16) | FstStart[entry].nameOffsetLo;
				char* r20 = &FstStringStart[nameOffset & 0xFFFFFF];     // r20 -- ptr to current entry name

				bool same;
				while (true)
				{
					if (*r20 == 0)
					{
						same = (*r21 == '/' || *r21 == 0);
						break;
					}

					if (_tolower(*r20++) != _tolower(*r21++))
					{
						same = false;
						break;
					}
				}

				if (same)
				{
					if (afterNameCharNZ == false)
						return entry;
					path += nameSize + 1;
					break;      // break Loop2
				}
			}

			// Walk next directory/file at same level
			entry = FstStart[entry].isDir ? FstStart[entry].nextOffset : (entry + 1);

		}   // Loop2

	}   // Loop1
}

// convert DVD file name into file position on the disk
// 0, if file not found
int dvd_open(const char *path)
{
	int entry = DVDConvertPathToEntrynum(path);
	if (entry < 0)
	{
		return 0;
	}

	return (int)FstStart[entry].fileOffset;
}


// simple GCM image reading. Compressed RVZ containers are detected by their magic and read
// through the RvzImage reader (see rvz.cpp).

static DVD::RvzImage rvzImage;

bool GCMMountFile(const wchar_t*file)
{
	dvd.gcm_filename[0] = 0;
	dvd.mountedImage = false;
	dvd.mountedRvz = false;
	dvd.gcm_size = 0;

	rvzImage.Unmount();

	if (file == nullptr)
	{
		return true;
	}

	// Is this a compressed RVZ image? The container is recognized by its magic, not by the
	// extension, so that a renamed image still mounts.
	if (DVD::RvzImage::IsRvzFile(file))
	{
		if (!rvzImage.Mount(file))
		{
			return false;
		}

		// For RVZ, gcm_size is the size of the uncompressed disc.
		dvd.gcm_size = (int)rvzImage.GetDiscSize();
		dvd.seekval = 0;

		wcscpy(dvd.gcm_filename, file);
		dvd.mountedImage = true;
		dvd.mountedRvz = true;

		return true;
	}

	// open GCM file
	FILE* gcm_file = fopen(Util::WstringToString(file).c_str(), "rb");
	if(!gcm_file) return false;

	// get file size
	fseek(gcm_file, 0, SEEK_END);
	dvd.gcm_size = ftell(gcm_file);
	fseek(gcm_file, 0, SEEK_SET);

	fclose(gcm_file);

	// protect from damaged GCMs
	if(dvd.gcm_size < DVD_APPLDR_OFFSET)
	{
		return false;
	}

	// reset position
	dvd.seekval = 0;

	wcscpy(dvd.gcm_filename, file);
	dvd.mountedImage = true;

	return true;
}

void GCMSeek(int position)
{
	dvd.seekval = position;
}

bool GCMRead(uint8_t*buf, size_t length)
{
	if (dvd.gcm_filename[0] == 0)
	{
		memset(buf, 0, length);        // fill by zeroes
		return true;
	}

	// out of DVD
	if(dvd.seekval >= DVD_SIZE)
	{
		memset(buf, 0, length);     // fill by zeroes
		dvd.seekval += (int)length;
		return false;
	}

	// Images can be less than 1.4 GB,
	// so just return zeroes, when seek is out of the image
	if(dvd.seekval >= dvd.gcm_size)
	{
		memset(buf, 0, length);     // fill by zeroes
		dvd.seekval += (int)length;
		return true;
	}

	// wrap, if seek is near to out of DVD
	if( (dvd.seekval + length) >= DVD_SIZE)
	{
		length = DVD_SIZE - dvd.seekval;
	}

	// wrap, if seek is near to out of the image
	if( (dvd.seekval + length) >= dvd.gcm_size)
	{
		length = dvd.gcm_size - dvd.seekval;
	}

	// read data
	if(length)
	{
		if (dvd.mountedRvz)
		{
			if (!rvzImage.Read((uint64_t)dvd.seekval, length, buf))
			{
				return false;
			}

			dvd.seekval += (int)length;
			return true;
		}

		FILE* gcm_file = fopen ( Util::WstringToString(dvd.gcm_filename).c_str(), "rb");

		if (!gcm_file)
		{
			memset(buf, 0, length);        // fill by zeroes
			return false;
		}

		fseek(gcm_file, dvd.seekval, SEEK_SET);
		// https://stackoverflow.com/questions/295994/what-is-the-rationale-for-fread-fwrite-taking-size-and-count-as-arguments
		size_t bytesRead = fread(buf, 1, length, gcm_file);
		fclose(gcm_file);
		dvd.seekval += (int)length;
		return (bytesRead == length);
	}

	return true;
}



int32_t YnLeft[2], YnRight[2];

typedef int Nibble;

void DvdAudioInitDecoder()
{
	YnLeft[0] = YnLeft[1] = 0;
	YnRight[0] = YnRight[1] = 0;
}

int32_t MulSomething(Nibble arg_0, int32_t yn1, int32_t yn2)
{
	int16_t a1 = 0;
	int16_t a2 = 0;

	switch (arg_0)
	{
		case 0:
			a1 = 0;
			a2 = 0;
			break;
		case 1:
			a1 = 60;
			a2 = 0;
			break;
		case 2:
			a1 = 115;
			a2 = -52;
			break;
		case 3:
			a1 = 98;
			a2 = -55;
			break;
	}

	int32_t ps1 = (int32_t)a1 * yn1;
	int32_t ps2 = (int32_t)a2 * yn2;

	int32_t var_C = (ps1 + ps2 + 32) >> 6;
	return my_max(-0x200000, my_min(var_C, 0x1FFFFF) );
}

int16_t Shifts1(Nibble arg_0, Nibble arg_4)
{
	int16_t var_4 = (int16_t)arg_0 << 12;
	return var_4 >> arg_4;
}

int32_t Shifts2(int16_t arg_0, int32_t arg_4)
{
	return ((int32_t)arg_0 << 6) + arg_4;
}

// Clamp
uint16_t Clamp(int32_t arg_0)
{
	int32_t var_8 = arg_0 >> 6;
	return (uint16_t)my_max(-0x8000, my_min(var_8, 0x7FFF));
}

uint16_t DecodeSample(Nibble arg_0, uint8_t arg_4, int Yn[2])
{
	uint16_t res;

	Nibble var_4 = arg_4 >> 4;
	Nibble var_8 = arg_4 & 0xF;

	int32_t var_18 = MulSomething(var_4, Yn[0], Yn[1]);
	int16_t var_C = Shifts1(arg_0, var_8);
	int32_t var_14 = Shifts2(var_C, var_18);
	res = Clamp(var_14);

	Yn[1] = Yn[0];
	Yn[0] = var_14;

	return res;
}

void DvdAudioDecode(uint8_t adpcmBuffer[32], uint16_t pcmBuffer[2 * 28])
{
	uint8_t* adpcmData = &adpcmBuffer[4];

	if (!(adpcmBuffer[0] == adpcmBuffer[2] && adpcmBuffer[1] == adpcmBuffer[3]))
	{
		memset(pcmBuffer, 0, 2 * 28 * sizeof(uint16_t));
		return;
	}

	for (int i = 0; i < 28; i++)
	{
		pcmBuffer[2 * i] = DecodeSample(adpcmData[i] & 0xF, adpcmBuffer[0], YnLeft);
		pcmBuffer[2 * i + 1] = DecodeSample(adpcmData[i] >> 4, adpcmBuffer[1], YnRight);
	}
}


namespace DVD
{
	DduCore * DDU;

	DduCore::DduCore()
	{
		dduThread = EMUCreateThread(DduThreadProc, true, this, "DvdData");
		dvdAudioThread = EMUCreateThread(DvdAudioThreadProc, true, this, "DvdAudio");

		dataCache = new uint8_t[dataCacheSize];
		memset(dataCache, 0, dataCacheSize);
		streamingCache = new uint8_t[streamCacheSize];
		memset(streamingCache, 0, streamCacheSize);

		Reset();

		if (adpcmStreamDump)
		{
			adpcmStreamFile = fopen("Data\\DvdAdpcm.bin", "wb");
		}
		if (decodedStreamDump)
		{
			decodedStreamFile = fopen("Data\\DvdDecodedPcm.bin", "wb");
		}
	}

	DduCore::~DduCore()
	{
		if (adpcmStreamFile)
		{
			fclose(adpcmStreamFile);
		}
		if (decodedStreamFile)
		{
			fclose(decodedStreamFile);
		}

		TransferComplete();
		EMUJoinThread(dduThread);
		EMUJoinThread(dvdAudioThread);
		delete[] dataCache;
		delete[] streamingCache;
	}

	void DduCore::ExecuteCommand()
	{
		// Execute command

		errorState = false;
		errorCode = 0;

		if (logCommands)
		{
			Report(Channel::DVD, "Command: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X\n",
				commandBuffer[0], commandBuffer[1], commandBuffer[2], commandBuffer[3],
				commandBuffer[4], commandBuffer[5], commandBuffer[6], commandBuffer[7],
				commandBuffer[8], commandBuffer[9], commandBuffer[10], commandBuffer[11]);
		}

		switch (commandBuffer[0])
		{
			// Inquiry (DVDLowInquiry), read manufacture info	(DMA)
			case 0x12:
				state = DduThreadState::ReadBogusData;
				if (log)
				{
					Report(Channel::DVD, "DVD Inquiry.\n");
				}
				break;

			// read sector / disk id
			case 0xA8:
				state = DduThreadState::ReadDvdData;
				{
					uint32_t seekTemp = (commandBuffer[4] << 24) |
						(commandBuffer[5] << 16) |
						(commandBuffer[6] << 8) |
						(commandBuffer[7]);
					seekVal = seekTemp << 2;

					// Use transaction size as hint for pre-caching
					if (commandBuffer[3] == 0x40)
					{
						transactionSize = sizeof(DiskID);
					}
					else
					{
						transactionSize = (commandBuffer[8] << 24) |
							(commandBuffer[9] << 16) |
							(commandBuffer[10] << 8) |
							(commandBuffer[11]);
					}
				}
				// Invalidate reading cache
				dataCachePtr = dataCacheSize;

				if (log)
				{
					Report(Channel::DVD, "DVD Read: 0x%08X, %i bytes\n", seekVal, transactionSize);
				}
				break;

			// seek (DVDLowSeek)	(IMM)
			case 0xAB:
				state = DduThreadState::ReadBogusData;

				{
					uint32_t seekTemp = (commandBuffer[4] << 24) |
						(commandBuffer[5] << 16) |
						(commandBuffer[6] << 8) |
						(commandBuffer[7]);

					if (log)
					{
						Report(Channel::DVD, "Seek: 0x%08X (ignored)\n", seekTemp << 2);
					}
				}
				break;

			// Request Error (DVDLowRequestError)	(IMM)
			case 0xE0:
				state = DduThreadState::ReadBogusData;
				if (log)
				{
					Report(Channel::DVD, "Request Error\n");
				}
				break;

			// set stream (DVDLowAudioStream) (IMM)
			case 0xE1:
				state = DduThreadState::ReadBogusData;

				{
					uint32_t seekTemp = (commandBuffer[4] << 24) |
						(commandBuffer[5] << 16) |
						(commandBuffer[6] << 8) |
						(commandBuffer[7]);

					// The SDK first sets the address of the stream, and then for some reason resets it to 0.
					// We make the assumption that the value 0 should be ignored.
					if (seekTemp != 0)
					{
						streamSeekVal = seekTemp << 2;
						streamCount = (commandBuffer[8] << 24) |
							(commandBuffer[9] << 16) |
							(commandBuffer[10] << 8) |
							(commandBuffer[11]);

						SetDvdAudioSampleRate(sampleRate);
						DvdAudioInitDecoder();
						streamEnabledByDduCommand = true;

						// Invalidate streaming cache
						streamingCachePtr = streamCacheSize;

						// Invalidate PCM buffer
						pcmPlaybackCounter = sizeof(pcmPlaybackBuffer);

						if (log)
						{
							Report(Channel::DVD, "DVD Streaming setup: stream start 0x%08X, counter: %i\n",
								streamSeekVal, streamCount);
						}
					}
					else
					{
						DvdAudioInitDecoder();

						if (log)
						{
							Report(Channel::DVD, "DVD Bogus Streaming setup (ignored)\n");
						}
					}

				}
				break;

			// get stream status (DVDLowRequestAudioStatus) (IMM)
			case 0xE2:
				switch (commandBuffer[1])
				{
					case 0:             // Get stream enable
						state = DduThreadState::GetStreamEnable;
						immediateBuffer[0] = 0;
						immediateBuffer[1] = 0;
						immediateBuffer[2] = 0;
						immediateBuffer[3] = streamEnabledByDduCommand ? 1 : 0;
						immediateBufferPtr = 0;
						break;
					case 1:             // Get stream address
						state = DduThreadState::GetStreamOffset;
						{
							uint32_t seekTemp = streamSeekVal >> 2;
							immediateBuffer[0] = (seekTemp >> 24) & 0xff;
							immediateBuffer[1] = (seekTemp >> 16) & 0xff;
							immediateBuffer[2] = (seekTemp >> 8) & 0xff;
							immediateBuffer[3] = (seekTemp) & 0xff;
						}
						immediateBufferPtr = 0;
						break;
					default:
						state = DduThreadState::GetStreamBogus;
						Report(Channel::DVD, "Unknown GetStreamStatus: %i\n", commandBuffer[1]);
						break;
				}
				break;

			// stop motor (DVDLowStopMotor)		(IMM)
			case 0xE3:
				state = DduThreadState::ReadBogusData;
				if (log)
				{
					Report(Channel::DVD, "Stop motor.\n");
				}
				break;

			// Set Audio Buffer (DVDLowAudioBufferConfig)  (IMM)
			// It looks like it's setting up a FIFO buffer that stores decoded PCM samples of the next 32 Bytes ADPCM chunk.
			case 0xE4:
				state = DduThreadState::ReadBogusData;

				if (log)
				{
					Report(Channel::DVD, "SetAudioBuffer: Trig: %i, Enable: %i, Size: %i\n", 
						commandBuffer[0] & 3, commandBuffer[2] & 0x80 ? 1 : 0, commandBuffer[3] & 0xf);
				}
				break;

			default:
				state = DduThreadState::ReadBogusData;

				Report(Channel::DVD, "Unknown DDU command: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X\n",
					commandBuffer[0], commandBuffer[1], commandBuffer[2], commandBuffer[3],
					commandBuffer[4], commandBuffer[5], commandBuffer[6], commandBuffer[7],
					commandBuffer[8], commandBuffer[9], commandBuffer[10], commandBuffer[11]);
				break;
		}

		commandPtr = 0;
	}

	void DduCore::DduThreadProc(void* Parameter)
	{
		DduCore* core = (DduCore*)Parameter;

		// Wait Gekko ticks
		if (!core->transferRateNoLimit)
		{
			int64_t ticks = Core->GetTicks();
			if (ticks >= core->savedGekkoTicks)
			{
				core->savedGekkoTicks = ticks + core->dduTicksPerByte;
			}
			else
			{
				return;
			}
		}

		// Until break or transfer completed
		while (core->ddBusBusy)
		{
			if (core->busDir == DduBusDirection::HostToDdu)
			{
				switch (core->state)
				{
					case DduThreadState::WriteCommand:
						if (core->commandPtr < sizeof(core->commandBuffer))
						{
							core->commandBuffer[core->commandPtr] = core->hostToDduCallback(core->transferContext);
							core->stats.bytesWrite++;
							core->commandPtr++;
						}

						if (core->commandPtr >= sizeof(core->commandBuffer))
						{
							core->ExecuteCommand();
						}
						break;

					// Hidden debug commands are not supported yet

					default:
						core->DeviceError(0);
						break;
				}
			}
			else
			{
				switch (core->state)
				{
					case DduThreadState::ReadDvdData:
						// Read-ahead new DVD data
						if (core->dataCachePtr >= dataCacheSize)
						{
							Seek(core->seekVal);
							size_t bytes = my_min(dataCacheSize, core->transactionSize);
							bool readResult = Read(core->dataCache, bytes);
							core->seekVal += (uint32_t)bytes;
							core->transactionSize -= bytes;

							if (core->seekVal >= DVD_SIZE || !readResult)
							{
								core->DeviceError(0);
							}

							core->dataCachePtr = 0;
						}

						core->dduToHostCallback(core->dataCache[core->dataCachePtr], core->transferContext);
						core->stats.bytesRead++;
						core->dataCachePtr++;
						break;

					case DduThreadState::ReadBogusData:
						core->dduToHostCallback(0, core->transferContext);
						core->stats.bytesRead++;
						break;

					case DduThreadState::GetStreamEnable:
					case DduThreadState::GetStreamOffset:
					case DduThreadState::GetStreamBogus:
						if (core->immediateBufferPtr < sizeof(core->immediateBuffer))
						{
							core->dduToHostCallback(core->immediateBuffer[core->immediateBufferPtr], core->transferContext);
							core->stats.bytesRead++;
							core->immediateBufferPtr++;
						}
						else
						{
							core->DeviceError(0);
						}
						break;

					case DduThreadState::Idle:
						break;

					default:
						core->DeviceError(0);
						break;
				}
			}
		}

		// Sleep until next transfer
		core->dduThread->Suspend();
	}

	// Enabling AISCLK forces the DDU to issue samples out even if there are none (zeros goes to output).
	void DduCore::EnableAudioStreamClock(bool enable)
	{
		if (enable)
		{
			dvdAudioThread->Resume();
		}
		else
		{
			dvdAudioThread->Suspend();
		}
	}

	void DduCore::DvdAudioThreadProc(void* Parameter)
	{
		uint16_t sample[2] = { 0, 0 };
		DduCore* core = (DduCore*)Parameter;

		while (true)
		{
			// If AISCLK is enabled but streaming is not enabled by the DDU command, DVD Audio will output only zeros.

			// If its time to send sample
			int64_t ticks = Core->GetTicks();
			if (ticks < core->nextGekkoTicksToSample)
			{
				continue;
			}
			core->nextGekkoTicksToSample = ticks + core->TicksPerSample();

			// Invalidate cache
			if (core->streamEnabledByDduCommand)
			{
				if (core->streamingCachePtr >= streamCacheSize)
				{
					core->streamingCachePtr = 0;
					Seek(core->streamSeekVal);
					bool readResult = Read(core->streamingCache, streamCacheSize);

					if (core->log)
					{
						//DBReport2(DbgChannel::DVD, "Streaming Seek: 0x%08X, Byte[0]: 0x%02X\n", core->streamSeekVal, core->streamingCache[0]);
					}

					//if (!readResult)
					//{
					//	core->DeviceError(0);
					//}

					if (core->adpcmStreamDump && core->adpcmStreamFile)
					{
						fwrite(core->streamingCache, 1, streamCacheSize, core->adpcmStreamFile);
					}
				}
			}

			// From changing the playback frequency, the size of the ADPCM data does not change. The frequency of samples output to the outside changes.

			if (core->streamEnabledByDduCommand)
			{
				if (core->pcmPlaybackCounter >= sizeof(core->pcmPlaybackBuffer))
				{
					// Decode next ADPCM chunk
					DvdAudioDecode(&core->streamingCache[core->streamingCachePtr], core->pcmPlaybackBuffer);

					if (core->decodedStreamDump && core->decodedStreamFile)
					{
						fwrite(core->pcmPlaybackBuffer, 1, sizeof(core->pcmPlaybackBuffer), core->decodedStreamFile);
					}

					core->streamingCachePtr += 32;
					core->streamSeekVal += 32;
					core->streamCount -= 32;
					core->pcmPlaybackCounter = 0;
				}

				uint8_t* rawPtr = (uint8_t *)core->pcmPlaybackBuffer + core->pcmPlaybackCounter;
				sample[0] = *(uint16_t *)rawPtr;
				sample[1] = *(uint16_t *)(rawPtr + 2);
				core->pcmPlaybackCounter += 4;
			}
			else
			{
				sample[0] = 0;
				sample[1] = 0;
			}

			// Send sample

			if (core->streamCallback)
			{
				core->streamCallback(sample[0], sample[1], core->streamContext);
			}

			core->stats.sampleCounter++;

			if (core->streamEnabledByDduCommand)
			{
				if (core->streamCount <= 0)
				{
					core->streamEnabledByDduCommand = false;

					if (core->log)
					{
						Report(Channel::DVD, "DVD streaming stopped by counter value reach zero\n");
					}
				}
			}
		}
	}

	// Calculates how many Gekko ticks takes 1 sample, at the selected sample rate.
	int64_t DduCore::TicksPerSample()
	{
		if (sampleRate == DvdAudioSampleRate::Rate_32000)
		{
			// 32 kHz means 32,000 LR samples per second come from DVD Audio.
			// To find out how many ticks a sample takes, you need to divide the number of Gekko ticks per second by 32000.
			return gekkoOneSecond / 32000;
		}
		else
		{
			return gekkoOneSecond / 48000;
		}
	}

	void DduCore::SetDvdAudioSampleRate(DvdAudioSampleRate rate)
	{
		sampleRate = rate;
		nextGekkoTicksToSample = Core->GetTicks() + TicksPerSample();
	}

	// Reset internal state. If you forget something, then it will come out later..
	void DduCore::Reset()
	{
		dduThread->Suspend();
		dvdAudioThread->Suspend();
		ddBusBusy = false;
		errorState = false;
		commandPtr = 0;
		dataCachePtr = dataCacheSize;
		streamingCachePtr = streamCacheSize;
		state = DduThreadState::WriteCommand;
		ResetStats();
		gekkoOneSecond = Core->OneSecond();
		dduTicksPerByte = gekkoOneSecond / transferRate;
		SetDvdAudioSampleRate(DvdAudioSampleRate::Rate_48000);
		streamEnabledByDduCommand = false;
	}

	void DduCore::Break()
	{
		// Abort data transfer
		Report(Channel::DVD, "DDU Break\n");
		ddBusBusy = false;
	}

	void DduCore::OpenCover()
	{
		if (coverStatus == CoverStatus::Open)
			return;

		coverStatus = CoverStatus::Open;
		Report(Channel::DVD, "Cover opened\n");

		// Notify host hardware
		if (openCoverCallback)
		{
			openCoverCallback(openCoverContext);
		}
	}

	void DduCore::CloseCover()
	{
		if (coverStatus == CoverStatus::Close)
			return;

		coverStatus = CoverStatus::Close;
		Report(Channel::DVD, "Cover closed\n");

		// Notify host hardware
		if (closeCoverCallback)
		{
			closeCoverCallback(closeCoverContext);
		}
	}

	void DduCore::DeviceError(uint32_t reason)
	{
		Report(Channel::DVD, "DDU DeviceError: %08X\n", reason);
		// Will deassert DIERR after the next command is received from the host
		errorState = true;
		errorCode = reason;
		ddBusBusy = false;
		if (errorCallback)
		{
			errorCallback(errorContext);
		}
	}

	void DduCore::StartTransfer(DduBusDirection direction)
	{
		if (logTransfers)
		{
			Report(Channel::DVD, "StartTransfer: %s\n", direction == DduBusDirection::DduToHost ? "Ddu->Host" : "Host->Ddu");
		}

		ddBusBusy = true;
		busDir = direction;

		savedGekkoTicks = Core->GetTicks() + dduTicksPerByte;

		dduThread->Resume();
	}

	void DduCore::TransferComplete()
	{
		if (logTransfers)
		{
			Report(Channel::DVD, "TransferComplete\n");
		}

		ddBusBusy = false;

		switch (state)
		{
			case DduThreadState::WriteCommand:
				// Invalidate reading cache
				dataCachePtr = dataCacheSize;
				break;

			case DduThreadState::Idle:
			case DduThreadState::ReadDvdData:
			case DduThreadState::ReadBogusData:
			case DduThreadState::GetStreamEnable:
			case DduThreadState::GetStreamOffset:
			case DduThreadState::GetStreamBogus:
				state = DduThreadState::WriteCommand;
				break;
		}

		if (busDir == DduBusDirection::DduToHost)
		{
			stats.dduToHostTransferCount++;
		}
		else
		{
			stats.hostToDduTransferCount++;
		}
	}

}
