// DDU Debug Interface

#include "pch.h"

namespace DVD
{
	static void SwapArea(void* _addr, int count)
	{
		uint32_t* addr = (uint32_t *)_addr;
		uint32_t* until = addr + count / sizeof(uint32_t);

		while (addr != until)
		{
			*addr = _byteswap_ulong(*addr);
			addr++;
		}
	}

	// Get DDU Status Information
	static Json::Value* DvdInfo(std::vector<std::string>& args)
	{
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		if (dvd.mountedImage)
		{
			DBReport("Mounted as disk image: %s\n", Debug::Hub.TcharToString(dvd.gcm_filename).c_str());
			DBReport("GCM Size: 0x%08X bytes\n", dvd.gcm_size);
			DBReport("Current seek position: 0x%08X\n", GetSeek());

			output->AddString(nullptr, dvd.gcm_filename);
			output->AddInt(nullptr, GetSeek());
		}
		else if (dvd.mountedSdk != nullptr)
		{
			DBReport("Mounted as SDK directory: %s\n", Debug::Hub.TcharToString(dvd.mountedSdk->GetDirectory()).c_str());
			DBReport("Current seek position: 0x%08X\n", GetSeek());

			output->AddString(nullptr, dvd.mountedSdk->GetDirectory());
			output->AddInt(nullptr, GetSeek());
		}
		else
		{
			DBReport("Disk Unmounted\n");
		}

		DBReport("Lid status: %s\n", (DDU.GetCoverStatus() == CoverStatus::Open) ? "Open" : "Closed");

		output->AddBool(nullptr, DDU.GetCoverStatus() == CoverStatus::Open ? true : false);

		// Return info

		return output;
	}

	// Mount GC DVD image (GCM)
	static Json::Value* MountIso(std::vector<std::string>& args)
	{
		bool result = MountFile(args[1]);

		if (result)
		{
			DBReport2(DbgChannel::DVD, "Mounted disk image: %s\n", args[1].c_str());
		}
		else
		{
			DBReport2(DbgChannel::Error, "Failed to mount disk image: %s\n", args[1].c_str());
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Bool;
		output->value.AsBool = result;

		return output;
	}

	// Simulate opening of the drive cover
	static Json::Value* OpenLid(std::vector<std::string>& args)
	{
		DDU.OpenCover();
		return nullptr;
	}

	// Simulate closing of the drive cover
	static Json::Value* CloseLid(std::vector<std::string>& args)
	{
		DDU.CloseCover();
		return nullptr;
	}

	// Show some stats
	static Json::Value* DvdStats(std::vector<std::string>& args)
	{
		DBReport("DvdStats:\n");

		DBReport("BytesRead: %I64u\n", DDU.stats.bytesRead);
		DBReport("BytesWrite: %I64u\n", DDU.stats.bytesWrite);
		DBReport("Host->DDU transfers: %i\n", DDU.stats.hostToDduTransferCount);
		DBReport("DDU->Host transfers: %i\n", DDU.stats.dduToHostTransferCount);
		DBReport("SampleCounter: %I64u\n", DDU.stats.sampleCounter);

		return nullptr;
	}

	// Reset stats
	static Json::Value* DvdResetStats(std::vector<std::string>& args)
	{
		DDU.ResetStats();

		return nullptr;
	}

	// Mount Dolphin SDK folder as virtual disk
	static Json::Value* MountSDK(std::vector<std::string>& args)
	{
		bool result = MountSdk(args[1]);

		if (result)
		{
			DBReport2(DbgChannel::DVD, "Mounted SDK: %s\n", args[1].c_str());
		}
		else
		{
			DBReport2(DbgChannel::Error, "Failed to mount SDK: %s\n", args[1].c_str());
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
		DBReport2( DbgChannel::DVD, "Unmounted\n");
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

		if (size > 1024 * 1024)
		{
			DBReport2(DbgChannel::Error, "Too big\n");
			return nullptr;
		}

		if (!IsMounted())
		{
			DBReport2(DbgChannel::DVD, "Not mounted!\n");
			return nullptr;
		}

		// Allocate buffer

		uint8_t* buf = new uint8_t[size];
		assert(buf);

		int seekPos = GetSeek();
		Read(buf, size);

		// Print first 32 Bytes

		char hexDump[0x200] = { 0, };
		char asciiDump[0x200] = { 0, };
		char* ptr = hexDump;
		char* ptr2 = asciiDump;

		size_t bytes = min(32, size);
		for (size_t i=0, breakCounter=0; i< bytes; i++)
		{
			ptr += sprintf_s(ptr, sizeof(hexDump) - (ptr - hexDump), "%02X ", buf[i]);
			ptr2 += sprintf_s(ptr2, sizeof(asciiDump) - (ptr2 - asciiDump), "%c", (32 <= buf[i] && buf[i] < 128) ? buf[i] : '.');

			breakCounter++;
			if (breakCounter >= 16)
			{
				DBReport("0x%08X %s %s\n", seekPos, hexDump, asciiDump);
				breakCounter = 0;
				ptr = hexDump;
				ptr2 = asciiDump;
				seekPos += 16;
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

		DBReport2(DbgChannel::DVD, "Read %zi bytes\n", size);

		return output;
	}

	// Open file on DVD filesystem
	static Json::Value* DvdOpenFile(std::vector<std::string>& args)
	{
		long position = OpenFile(args[1].c_str());

		if (position)
		{
			DBReport("File:%s, position: 0x%08X\n", args[1].c_str(), position);
		}
		else
		{
			DBReport("Cannot locate file position:%s\n", args[1].c_str());
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
			DBReport2(DbgChannel::DVD, "Not mounted!\n");
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

		DBReport("DVDBB2::bootFilePosition: 0x%08X\n", bb2.bootFilePosition);
		DBReport("DVDBB2::FSTPosition: 0x%08X\n", bb2.FSTPosition);
		DBReport("DVDBB2::FSTLength: 0x%08X\n", bb2.FSTLength);
		DBReport("DVDBB2::FSTMaxLength: 0x%08X\n", bb2.FSTMaxLength);
		DBReport("DVDBB2::userPosition: 0x%08X\n", bb2.userPosition);
		DBReport("DVDBB2::userLength: 0x%08X\n", bb2.userLength);

		return output;
	}

	static DVDFileEntry* DumpFstDir(DVDFileEntry* fst, DVDFileEntry* entry, Json::Value * parent, int dumpMode)
	{
		entry->nameOffsetLo = _byteswap_ushort(entry->nameOffsetLo);
		entry->fileOffset = _byteswap_ulong(entry->fileOffset);
		entry->fileLength = _byteswap_ulong(entry->fileLength);

		if (entry->isDir)
		{
			// Directory

			if (dumpMode)
			{
				DBReport("Dir[%i]: name: 0x%X, parent: %i, next: %i\n",
					entry - fst,
					((uint32_t)entry->nameOffsetHi << 16) | entry->nameOffsetLo,
					entry->parentOffset, entry->nextOffset);
			}

			char dirNameAsInt[0x100] = { 0, };

			sprintf_s(dirNameAsInt, sizeof(dirNameAsInt) - 1, "%i", ((uint32_t)entry->nameOffsetHi << 16) | entry->nameOffsetLo);
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
				DBReport("File[%i]: name: 0x%X, offset: 0x%X, len: 0x%X\n",
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
				
				DBReport("%s%s\n", indent, depth != 0 ? stringsMap[offset].c_str() : "/");
			}
			else
			{
				DBReport("%s/\n", indent);
			}
		}
		else if (entry->type == Json::ValueType::Int)
		{
			DBReport("%s%s\n", indent, stringsMap[(uint32_t)entry->value.AsInt].c_str());
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
			DBReport2(DbgChannel::DVD, "Not mounted!\n");
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
					DBReport("name[0x%X]: %s\n", savedOffset, name);
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

	void DvdCommandsReflector()
	{
		Debug::Hub.AddCmd("DvdInfo", DvdInfo);
		Debug::Hub.AddCmd("MountIso", MountIso);
		Debug::Hub.AddCmd("OpenLid", OpenLid);
		Debug::Hub.AddCmd("CloseLid", CloseLid);
		Debug::Hub.AddCmd("DvdStats", DvdStats);
		Debug::Hub.AddCmd("DvdResetStats", DvdResetStats);
		Debug::Hub.AddCmd("MountSDK", MountSDK);
		Debug::Hub.AddCmd("UnmountDvd", UnmountDvd);
		Debug::Hub.AddCmd("DvdSeek", DvdSeek);
		Debug::Hub.AddCmd("DvdRead", DvdRead);
		Debug::Hub.AddCmd("DvdOpenFile", DvdOpenFile);
		Debug::Hub.AddCmd("DumpBb2", DumpBb2);
		Debug::Hub.AddCmd("DumpFst", DumpFst);
		Debug::Hub.AddCmd("MnDisa", MnDisa);
	}

}
