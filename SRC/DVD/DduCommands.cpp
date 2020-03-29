// DDU Debug Interface

#include "pch.h"

namespace DVD
{
	// Mount GC DVD image (GCM)
	static Json::Value* MountIso(std::vector<std::string>& args)
	{
		bool result = MountFile(args[1]);

		if (result)
		{
			DBReport("Mounted disk image: %s\n", args[1].c_str());
		}
		else
		{
			DBReport("Failed to mount disk image: %s\n", args[1].c_str());
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
		// Bogus
		return nullptr;
	}

	// Mount Dolphin SDK folder as virtual disk
	static Json::Value* MountSDK(std::vector<std::string>& args)
	{
		bool result = MountSdk(args[1]);

		if (result)
		{
			DBReport("Mounted SDK: %s\n", args[1].c_str());
		}
		else
		{
			DBReport("Failed to mount SDK: %s\n", args[1].c_str());
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
		DBReport("DVD unmounted\n");
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

		// Allocate buffer

		uint8_t* buf = new uint8_t[size];
		assert(buf);

		Read(buf, size);

		// Return output

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		for (size_t i = 0; i < size; i++)
		{
			output->AddInt(nullptr, buf[i]);
		}

		delete[] buf;

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
		Debug::AddCmd("MountIso", MountIso);
		Debug::AddCmd("OpenLid", OpenLid);
		Debug::AddCmd("CloseLid", CloseLid);
		Debug::AddCmd("DvdStats", DvdStats);
		Debug::AddCmd("MountSDK", MountSDK);
		Debug::AddCmd("UnmountDvd", UnmountDvd);
		Debug::AddCmd("DvdSeek", DvdSeek);
		Debug::AddCmd("DvdRead", DvdRead);
		Debug::AddCmd("DvdOpenFile", DvdOpenFile);
	}
}
