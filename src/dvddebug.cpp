// DDU Debug Interface

#include "pch.h"

using namespace Debug;

namespace DVD
{
	const char* DduJdi = R"json(

{
	"info":
	{
		"description": "GAMECUBE Disk Drive Unit Jey-Dai specs.",
		"helpGroup": "DVD Debug Commands"
	},

	"can": {
		"DvdInfo": {
			"help": "Get DDU Status Information",
			"hints": "[silent]",
			"output": "Array [String File/Dir or \"\" if not mounted, Int Seek, Bool LidStatus (true: Open, false: Close)]"
		},

		"MountIso": {
			"help": "Mount GC DVD image (GCM)",
			"hints": "<file>",
			"args": 1,
			"usage": [
				"Syntax: MountIso <file>\n",
				"Examples of use: MountIso C:\\Isos\\mygame.iso\n"
			],
			"output": "Bool Success/Fail"
		},

		"OpenLid": {
			"help": "Simulate opening of the drive cover"
		},

		"CloseLid": {
			"help": "Simulate closing of the drive cover"
		},

		"DvdStats": {
			"help": "Show some stats"
		},

		"DvdResetStats": {
			"help": "Reset stats"
		},

		"MountSDK": {
			"help": "Mount Dolphin SDK folder as virtual disk",
			"hints": "<dir>",
			"args": 1,
			"usage": [
				"Syntax: MountSDK <dir>\n",
				"Examples of use: MountSDK C:\\DolphinSDK\n"
			],
			"output": "Bool Success/Fail"
		},

		"UnmountDvd": {
			"help": "Unmount DVD (extract virtual disk)"
		},

		"DvdSeek": {
			"help": "Seek at DVD offset",
			"hints": "<pos>",
			"args": 1,
			"usage": [
				"Syntax: DvdSeek <offset>\n",
				"Offset (in bytes) must not be greater current DVD size.\n",
				"Examples of use: DvdSeek 0\n",
				"                 DvdSeek 0x2440\n"
			]
		},

		"DvdRead": {
			"help": "Read DVD data",
			"hints": "<len> [silent]",
			"args": 1,
			"usage": [
				"Syntax: DvdRead <length> [silent]\n",
				"First 32 Bytes will be dumped on screen. Length must not greater 1 MByte.\n",
				"If silent == 1, then the command works in silent mode (nothing is output to the log)\n",
				"Examples of use: DvdRead 32\n",
				"                 DvdRead 0x1000 1\n"
			],
			"output": "Array [] of bytes"
		},

		"DvdOpenFile": {
			"help": "Open file on DVD filesystem",
			"hints": "<file>",
			"args": 1,
			"usage": [
				"Syntax: DvdOpenFile <file>\n",
				"path must be absolute, including root prefix '/'\n",
				"Examples of use: DvdOpenFile \"/opening.bnr\"\n",
				"                 DvdOpenFile \"/gxTests/tex-02/ia8_odd.tpl\"\n"
			],
			"output": "File offset in bytes. 0 if file not found."
		},

		"DumpBb2": {
			"help": "Dump mounted DVDBB2 struct",
			"output": "Array [] of bytes"
		},

		"DumpFst": {
			"help": "Dump mounted DVD filesystem",
			"hints": "[0|1]",
			"output": "Array [] of bytes"
		},

		"MnDisa": {
			"help": "Disassemble DVD Firmware",
			"hints": "<file>",
			"args": 1,
			"usage": [
				"Syntax: MnDisa <file>",
				"Disassembles specified firmware dump in disa.txt",
				"Example: MnDisa gc-dvd-20010608.bin"
			]
		},

		"DvdRegionById": {
			"internal": true,
			"args": 1,
			"output": "Array [String regionName=Unknown|EUR|NOE|FRA|ESP|ITA|FAH|HOL|AUS|JPN|USA|KOR]"
		}

	}
}


)json";

	static void SwapArea(void* _addr, int count)
	{
		uint32_t* addr = (uint32_t *)_addr;
		uint32_t* until = addr + count / sizeof(uint32_t);

		while (addr != until)
		{
			*addr = _BYTESWAP_UINT32(*addr);
			addr++;
		}
	}

	// Get DDU Status Information
	static Json::Value* DvdInfo(std::vector<std::string>& args)
	{
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		bool silent = args.size() > 1;

		if (dvd.mountedImage)
		{
			if (!silent)
			{
				Report(Channel::Norm, "Mounted as disk image: %s\n", Util::WstringToString(dvd.gcm_filename).c_str());
				Report(Channel::Norm, "GCM Size: 0x%08X bytes\n", dvd.gcm_size);
				Report(Channel::Norm, "Current seek position: 0x%08X\n", GetSeek());
			}

			output->AddString(nullptr, dvd.gcm_filename);
			output->AddInt(nullptr, GetSeek());
		}
		else if (dvd.mountedSdk != nullptr)
		{
			if (!silent)
			{
				Report(Channel::Norm, "Mounted as SDK directory: %s\n", Util::WstringToString(dvd.mountedSdk->GetDirectory()).c_str());
				Report(Channel::Norm, "Current seek position: 0x%08X\n", GetSeek());
			}

			output->AddString(nullptr, dvd.mountedSdk->GetDirectory());
			output->AddInt(nullptr, GetSeek());
		}
		else
		{
			if (!silent)
			{
				Report(Channel::Norm, "Disk Unmounted\n");
			}

			output->AddString(nullptr, L"");
			output->AddInt(nullptr, 0);
		}

		if (!silent)
		{
			Report(Channel::Norm, "Lid status: %s\n", (DDU->GetCoverStatus() == CoverStatus::Open) ? "Open" : "Closed");
		}

		output->AddBool(nullptr, DDU->GetCoverStatus() == CoverStatus::Open ? true : false);

		// Return info

		return output;
	}

	// Mount GC DVD image (GCM)
	static Json::Value* MountIso(std::vector<std::string>& args)
	{
		bool result = MountFile(args[1]);

		if (result)
		{
			Report(Channel::DVD, "Mounted disk image: %s\n", args[1].c_str());
		}
		else
		{
			Report(Channel::Error, "Failed to mount disk image: %s\n", args[1].c_str());
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Bool;
		output->value.AsBool = result;

		return output;
	}

	// Simulate opening of the drive cover
	static Json::Value* OpenLid(std::vector<std::string>& args)
	{
		DDU->OpenCover();
		return nullptr;
	}

	// Simulate closing of the drive cover
	static Json::Value* CloseLid(std::vector<std::string>& args)
	{
		DDU->CloseCover();
		return nullptr;
	}

	// Show some stats
	static Json::Value* DvdStats(std::vector<std::string>& args)
	{
		Report(Channel::Norm, "DvdStats:\n");

		Report(Channel::Norm, "BytesRead: %I64u\n", DDU->stats.bytesRead);
		Report(Channel::Norm, "BytesWrite: %I64u\n", DDU->stats.bytesWrite);
		Report(Channel::Norm, "Host->DDU transfers: %i\n", DDU->stats.hostToDduTransferCount);
		Report(Channel::Norm, "DDU->Host transfers: %i\n", DDU->stats.dduToHostTransferCount);
		Report(Channel::Norm, "SampleCounter: %I64u\n", DDU->stats.sampleCounter);

		return nullptr;
	}

	// Reset stats
	static Json::Value* DvdResetStats(std::vector<std::string>& args)
	{
		DDU->ResetStats();

		return nullptr;
	}

	// Mount Dolphin SDK folder as virtual disk
	static Json::Value* MountSDK(std::vector<std::string>& args)
	{
		bool result = MountSdk(args[1]);

		if (result)
		{
			Report(Channel::DVD, "Mounted SDK: %s\n", args[1].c_str());
		}
		else
		{
			Report(Channel::Error, "Failed to mount SDK: %s\n", args[1].c_str());
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Bool;
		output->value.AsBool = result;

		return output;
	}

	// Unmount DVD (extract virtual disk)
	static Json::Value* UnmountDvd(std::vector<std::string>& args)
	{
		Unmount();
		Report( Channel::DVD, "Unmounted\n");
		return nullptr;
	}

	// Seek at DVD offset
	static Json::Value* DvdSeek(std::vector<std::string>& args)
	{
		uint32_t position = strtoul(args[1].c_str(), nullptr, 0);
		Seek(position);
		return nullptr;
	}

	// Read DVD data
	static Json::Value* DvdRead(std::vector<std::string>& args)
	{
		size_t size = strtoul(args[1].c_str(), nullptr, 0);
		
		bool silent = false;
		if (args.size() >= 3)
		{
			silent = strtoul(args[2].c_str(), nullptr, 0) != 0;
		}

		if (size > 1024 * 1024)
		{
			Report(Channel::Error, "Too big\n");
			return nullptr;
		}

		if (!IsMounted())
		{
			Report(Channel::DVD, "Not mounted!\n");
			return nullptr;
		}

		// Allocate buffer

		uint8_t* buf = new uint8_t[size];

		int seekPos = GetSeek();
		Read(buf, size);

		// Print first 32 Bytes

		if (!silent)
		{
			char hexDump[0x200] = { 0, };
			char asciiDump[0x200] = { 0, };
			char* ptr = hexDump;
			char* ptr2 = asciiDump;

			size_t bytes = my_min(32, size);
			for (size_t i = 0, breakCounter = 0; i < bytes; i++)
			{
				ptr += sprintf(ptr, "%02X ", buf[i]);
				ptr2 += sprintf(ptr2, "%c", (32 <= buf[i] && buf[i] < 128) ? buf[i] : '.');

				breakCounter++;
				if (breakCounter >= 16)
				{
					Report(Channel::Norm, "0x%08X %s %s\n", seekPos, hexDump, asciiDump);
					breakCounter = 0;
					ptr = hexDump;
					ptr2 = asciiDump;
					seekPos += 16;
				}
			}
		}

		// Return output

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		for (size_t i = 0; i < size; i++)
		{
			output->AddInt(nullptr, buf[i]);
		}

		delete[] buf;

		if (!silent)
		{
			Report(Channel::DVD, "Read %zi bytes\n", size);
		}

		return output;
	}

	// Open file on DVD filesystem
	static Json::Value* DvdOpenFile(std::vector<std::string>& args)
	{
		long position = OpenFile(args[1]);

		if (position)
		{
			Report(Channel::Norm, "File:%s, position: 0x%08X\n", args[1].c_str(), position);
		}
		else
		{
			Report(Channel::Norm, "Cannot locate file position:%s\n", args[1].c_str());
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;
		output->value.AsInt = position;

		return output;
	}

	// Dump mounted DVDBB2 struct
	static Json::Value* DumpBb2(std::vector<std::string>& args)
	{
		if (!IsMounted())
		{
			Report(Channel::DVD, "Not mounted!\n");
			return nullptr;
		}

		DVDBB2 bb2 = { 0 };

		Seek(DVD_BB2_OFFSET);
		Read(&bb2, sizeof(bb2));

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;
		for (int i = 0; i < sizeof(bb2); i++)
		{
			output->AddInt(nullptr, ((uint8_t *)&bb2)[i]);
		}

		SwapArea(&bb2, sizeof(bb2));

		Report(Channel::Norm, "DVDBB2::bootFilePosition: 0x%08X\n", bb2.bootFilePosition);
		Report(Channel::Norm, "DVDBB2::FSTPosition: 0x%08X\n", bb2.FSTPosition);
		Report(Channel::Norm, "DVDBB2::FSTLength: 0x%08X\n", bb2.FSTLength);
		Report(Channel::Norm, "DVDBB2::FSTMaxLength: 0x%08X\n", bb2.FSTMaxLength);
		Report(Channel::Norm, "DVDBB2::userPosition: 0x%08X\n", bb2.userPosition);
		Report(Channel::Norm, "DVDBB2::userLength: 0x%08X\n", bb2.userLength);

		return output;
	}

	static DVDFileEntry* DumpFstDir(DVDFileEntry* fst, DVDFileEntry* entry, Json::Value * parent, int dumpMode)
	{
		entry->nameOffsetLo = _BYTESWAP_UINT16(entry->nameOffsetLo);
		entry->fileOffset = _BYTESWAP_UINT32(entry->fileOffset);
		entry->fileLength = _BYTESWAP_UINT32(entry->fileLength);

		if (entry->isDir)
		{
			// Directory

			if (dumpMode)
			{
				Report(Channel::Norm, "Dir[%i]: name: 0x%X, parent: %i, next: %i\n",
					entry - fst,
					((uint32_t)entry->nameOffsetHi << 16) | entry->nameOffsetLo,
					entry->parentOffset, entry->nextOffset);
			}

			char dirNameAsInt[0x100] = { 0, };

			sprintf(dirNameAsInt, "%i", ((uint32_t)entry->nameOffsetHi << 16) | entry->nameOffsetLo);
			Json::Value* dir = parent->AddObject(dirNameAsInt);

			DVDFileEntry* until = &fst[entry->nextOffset];
			DVDFileEntry* next = entry + 1;
			DVDFileEntry* last = next;

			while (next < until)
			{
				next = DumpFstDir(fst, next, dir, dumpMode);
				assert(next >= last);
				last = next;
			}

			return until;
		}
		else
		{
			// File

			if (dumpMode)
			{
				Report(Channel::Norm, "File[%i]: name: 0x%X, offset: 0x%X, len: 0x%X\n",
					entry - fst,
					((uint32_t)entry->nameOffsetHi << 16) | entry->nameOffsetLo,
					entry->fileOffset, entry->fileLength);
			}

			Json::Value* files = parent->ByName("files");
			if (!files)
			{
				files = parent->AddArray("files");
			}
			assert(files);

			files->AddInt(nullptr, ((uint32_t)entry->nameOffsetHi << 16) | entry->nameOffsetLo);

			return entry + 1;
		}
	}

	static void DumpFstEntry(Json::Value* entry, std::map<uint32_t, std::string>& stringsMap, int depth)
	{
		char indent[0x200] = { 0, };
		char* indentPtr = indent;

		for (int i = 0; i < depth; i++)
		{
			*indentPtr++ = ' ';
			*indentPtr++ = ' ';
		}
		*indentPtr++ = 0;

		if (entry->type == Json::ValueType::Object)
		{
			if (entry->name)
			{
				uint32_t offset = (uint32_t)atoi(entry->name);
				
				Report(Channel::Norm, "%s%s\n", indent, depth != 0 ? stringsMap[offset].c_str() : "/");
			}
			else
			{
				Report(Channel::Norm, "%s/\n", indent);
			}
		}
		else if (entry->type == Json::ValueType::Int)
		{
			Report(Channel::Norm, "%s%s\n", indent, stringsMap[(uint32_t)entry->value.AsInt].c_str());
		}

		for (auto it = entry->children.begin(); it != entry->children.end(); ++it)
		{
			DumpFstEntry(*it, stringsMap, entry->type == Json::ValueType::Array ? depth : depth + 1);
		}
	}

	// Dump mounted DVD filesystem
	// dumpMode: 0 - simple, 1 - advanced
	static Json::Value* DumpFst(std::vector<std::string>& args)
	{
		if (!IsMounted())
		{
			Report(Channel::DVD, "Not mounted!\n");
			return nullptr;
		}

		int dumpMode = 0;
		if (args.size() >= 2)
		{
			dumpMode = atoi(args[1].c_str()) & 1;
		}

		// Get FST location

		DVDBB2 bb2 = { 0 };

		Seek (DVD_BB2_OFFSET);
		Read(&bb2, sizeof(bb2));
		SwapArea(&bb2, sizeof(bb2));

		// Load FST

		uint8_t* fst = new uint8_t[bb2.FSTLength];
		assert(fst);

		memset(fst, 0, bb2.FSTLength);

		Seek(bb2.FSTPosition);
		Read(fst, bb2.FSTLength);

		// Output FST contents

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;
		for (uint32_t i = 0; i < bb2.FSTLength; i++)
		{
			output->AddInt(nullptr, fst[i]);
		}

		// Dump entries

		Json root;

		root.root.AddObject(nullptr);

		char * stringsTable = (char*)DumpFstDir((DVDFileEntry*)fst, (DVDFileEntry*)fst, &root.root, dumpMode);
		size_t stringsTableSize = bb2.FSTLength - (stringsTable - (char*)fst);

		// Dump strings

		size_t offset = 0, savedOffset = offset;
		char name[0x200] = { 0, };
		char* namePtr = name;
		std::map<uint32_t, std::string> stringsMap;

		while (offset < stringsTableSize)
		{
			*namePtr++ = stringsTable[offset];
			if (stringsTable[offset] == 0)
			{
				if (dumpMode)
				{
					Report(Channel::Norm, "name[0x%X]: %s\n", savedOffset, name);
				}
				stringsMap[(uint32_t)savedOffset] = name;
				namePtr = name;
				savedOffset = offset + 1;
			}
			offset++;
		}

		// Dump json

		if (!dumpMode)
		{
			DumpFstEntry(root.root.children.back(), stringsMap, 0);
		}

		delete[] fst;

		return output;
	}
	
	// Disassemble DVD Firmware
	static Json::Value* MnDisa(std::vector<std::string>& args)
	{

		return nullptr;
	}

	static Json::Value* DvdRegionById(std::vector<std::string>& args)
	{
		Region region = RegionById(args[1].c_str());

		std::string name = "Unknown";

		switch (region)
		{
			case Region::EUR: name = "EUR"; break;
			case Region::NOE: name = "NOE"; break;
			case Region::FRA: name = "FRA"; break;
			case Region::ESP: name = "ESP"; break;
			case Region::ITA: name = "ITA"; break;
			case Region::FAH: name = "FAH"; break;
			case Region::HOL: name = "HOL"; break;
			case Region::AUS: name = "AUS"; break;
			case Region::JPN: name = "JPN"; break;
			case Region::USA: name = "USA"; break;
			case Region::KOR: name = "KOR"; break;
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		output->AddAnsiString(nullptr, name.c_str());

		return output;
	}

	void DvdCommandsReflector()
	{
		JDI::Hub.AddCmd("DvdInfo", DvdInfo);
		JDI::Hub.AddCmd("MountIso", MountIso);
		JDI::Hub.AddCmd("OpenLid", OpenLid);
		JDI::Hub.AddCmd("CloseLid", CloseLid);
		JDI::Hub.AddCmd("DvdStats", DvdStats);
		JDI::Hub.AddCmd("DvdResetStats", DvdResetStats);
		JDI::Hub.AddCmd("MountSDK", MountSDK);
		JDI::Hub.AddCmd("UnmountDvd", UnmountDvd);
		JDI::Hub.AddCmd("DvdSeek", DvdSeek);
		JDI::Hub.AddCmd("DvdRead", DvdRead);
		JDI::Hub.AddCmd("DvdOpenFile", DvdOpenFile);
		JDI::Hub.AddCmd("DumpBb2", DumpBb2);
		JDI::Hub.AddCmd("DumpFst", DumpFst);
		JDI::Hub.AddCmd("MnDisa", MnDisa);
		JDI::Hub.AddCmd("DvdRegionById", DvdRegionById);
	}

}
