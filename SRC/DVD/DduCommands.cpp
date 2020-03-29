// DDU Debug Interface

#include "pch.h"

namespace DVD
{
	// Show DDU Status Information
	static Json::Value* DvdInfo(std::vector<std::string>& args)
	{
		if (dvd.mountedImage)
		{
			DBReport("Mounted as disk image: %s\n", Debug::Hub.TcharToString(dvd.gcm_filename).c_str());
			DBReport("GCM Size: 0x%08X bytes\n", dvd.gcm_size);
			DBReport("Current seek position: 0x%08X\n", dvd.seekval);
		}
		else if (dvd.mountedSdk != nullptr)
		{
			DBReport("Mounted as SDK directory: %s\n", "TODO");
		}
		else
		{
			DBReport("Disk Unmounted\n");
		}

		DBReport("Lid status: %s\n", DIGetCoverState() == 1 ? "Open" : "Closed");

		// TODO: Return info

		return nullptr;
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
		if (DIGetCoverState() == 0)
		{
			DIOpenCover();
		}
		return nullptr;
	}

	// Simulate closing of the drive cover
	static Json::Value* CloseLid(std::vector<std::string>& args)
	{
		if (DIGetCoverState() == 1)
		{
			DICloseCover();
		}
		return nullptr;
	}

	// Show some stats
	static Json::Value* DvdStats(std::vector<std::string>& args)
	{
		DBReport2(DbgChannel::DVD, "DvdStats Bogus\n");

		// Bogus
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
			DBReport("Too big\n");
			return nullptr;
		}

		// Allocate buffer

		uint8_t* buf = new uint8_t[size];
		assert(buf);

		Read(buf, size);

		// Print first 32 Bytes

		// TODO

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

	void DvdCommandsReflector()
	{
		Debug::Hub.AddCmd("DvdInfo", DvdInfo);
		Debug::Hub.AddCmd("MountIso", MountIso);
		Debug::Hub.AddCmd("OpenLid", OpenLid);
		Debug::Hub.AddCmd("CloseLid", CloseLid);
		Debug::Hub.AddCmd("DvdStats", DvdStats);
		Debug::Hub.AddCmd("MountSDK", MountSDK);
		Debug::Hub.AddCmd("UnmountDvd", UnmountDvd);
		Debug::Hub.AddCmd("DvdSeek", DvdSeek);
		Debug::Hub.AddCmd("DvdRead", DvdRead);
		Debug::Hub.AddCmd("DvdOpenFile", DvdOpenFile);
	}

}
