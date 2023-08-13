#include "pch.h"

// JDI Communication.

namespace UI
{
	JdiClient* Jdi;

	JdiClient::JdiClient()
	{
	}

	JdiClient::~JdiClient()
	{
	}

	// Generic

	std::string JdiClient::GetVersion()
	{
		char str[0x100] = { 0 };

		bool res = CallJdiReturnString("GetVersion", str, sizeof(str) - 1);
		if (!res)
		{
			throw "GetVersion failed!";
		}

		return std::string(str);
	}

	void JdiClient::ExecuteCommand(const std::string& cmdline)
	{
		bool res = CallJdiNoReturn(cmdline.c_str());
		if (!res)
		{
			throw "ExecuteCommand failed!";
		}
	}

	// Methods for controlling an optical drive

	bool JdiClient::DvdMount(const std::string& path)
	{
		char cmd[0x1000] = { 0 };

		sprintf(cmd, "MountIso \"%s\"", path.c_str());

		bool mountResult = false;

		bool res = CallJdiReturnBool(cmd, &mountResult);
		if (!res)
		{
			throw "MountIso failed!";
		}

		return mountResult;
	}

	bool JdiClient::DvdMountSDK(const std::string& path)
	{
		char cmd[0x1000] = { 0 };

		sprintf(cmd, "MountSDK \"%s\"", path.c_str());

		bool mountResult = false;

		bool res = CallJdiReturnBool(cmd, &mountResult);
		if (!res)
		{
			throw "MountIso failed!";
		}

		return mountResult;
	}

	void JdiClient::DvdUnmount()
	{
		ExecuteCommand("UnmountDvd");
	}

	void JdiClient::DvdSeek(int offset)
	{
		CallJdi(("DvdSeek " + std::to_string(offset)).c_str());
	}

	void JdiClient::DvdRead(std::vector<uint8_t>& data)
	{
		Json::Value* dataJson = CallJdi(("DvdRead " + std::to_string(data.size())).c_str());

		int i = 0;

		for (auto it = dataJson->children.begin(); it != dataJson->children.end(); ++it)
		{
			data[i++] = (*it)->value.AsUint8;
		}

		delete dataJson;
	}

	uint32_t JdiClient::DvdOpenFile(const std::string& filename)
	{
		Json::Value* offsetJson = CallJdi(("DvdOpenFile \"" + filename + "\"").c_str());

		uint32_t offsetValue = (uint32_t)offsetJson->value.AsInt;

		delete offsetJson;

		return offsetValue;
	}

	bool JdiClient::DvdCoverOpened()
	{
		Json::Value* dvdInfo = CallJdi("DvdInfo 1");	// silent mode

		bool lidStatusOpened = false;

		for (auto it = dvdInfo->children.begin(); it != dvdInfo->children.end(); ++it)
		{
			if ((*it)->type == Json::ValueType::Bool)
			{
				lidStatusOpened = (*it)->value.AsBool;
				break;
			}
		}

		delete dvdInfo;

		return lidStatusOpened;
	}

	void JdiClient::DvdOpenCover()
	{
		ExecuteCommand("OpenLid");
	}

	void JdiClient::DvdCloseCover()
	{
		ExecuteCommand("CloseLid");
	}

	std::string JdiClient::DvdRegionById(char* DiskId)
	{
		char regionName[0x20] = { 0, };

		char cmd[0x20];
		sprintf(cmd, "DvdRegionById %c%c%c%c", DiskId[0], DiskId[1], DiskId[2], DiskId[3]);

		CallJdiReturnString(cmd, regionName, sizeof(regionName) - 1);

		return regionName;
	}

	/// <summary>
	/// Get DVD mount status. The DvdInfo command is used
	/// </summary>
	/// <param name="path">The path to the image when mounting a file or the path to the folder when mounting as a DolphinSDK virtual disk.</param>
	/// <param name="mountedIso">Mount method (ISO / virtual disk as folder)</param>
	/// <returns>Disk mounted or not</returns>
	bool JdiClient::DvdIsMounted(std::string& path, bool& mountedIso)
	{
		Json::Value* value = CallJdi("DvdInfo");

		for (auto it = value->children.begin(); it != value->children.end(); ++it)
		{
			Json::Value* entry = *it;

			switch (entry->type)
			{
			case Json::ValueType::String:
				path = Util::WstringToString(entry->value.AsString);
				break;
			}
		}

		bool mounted = !path.empty();
		mountedIso = Util::FileExists(path);

		delete value;

		return mounted;
	}

	// Configuration access

	std::string JdiClient::GetConfigString(const std::string& var, const std::string& path)
	{
		Json::Value* value = CallJdi(("GetConfigString " + path + " " + var).c_str());
		std::string res = Util::WstringToString(value->children.front()->value.AsString);
		delete value;
		return res;
	}

	void JdiClient::SetConfigString(const std::string& var, const std::string& newVal, const std::string& path)
	{
		char cmd[0x200] = { 0, };
		sprintf(cmd, "SetConfigString %s %s \"%s\"", path.c_str(), var.c_str(), newVal.c_str());
		CallJdi(cmd);
	}

	int JdiClient::GetConfigInt(const std::string& var, const std::string& path)
	{
		Json::Value* value = CallJdi(("GetConfigInt " + path + " " + var).c_str());
		int res = (int)value->children.front()->value.AsInt;
		delete value;
		return res;
	}

	void JdiClient::SetConfigInt(const std::string& var, int newVal, const std::string& path)
	{
		CallJdi(("SetConfigInt " + path + " " + var + " " + std::to_string(newVal)).c_str());
	}

	bool JdiClient::GetConfigBool(const std::string& var, const std::string& path)
	{
		Json::Value* value = CallJdi(("GetConfigBool " + path + " " + var).c_str());
		bool res = value->children.front()->value.AsBool;
		delete value;
		return res;
	}

	void JdiClient::SetConfigBool(const std::string& var, bool newVal, const std::string& path)
	{
		CallJdi(("SetConfigBool " + path + " " + var + " " + (newVal ? "true" : "false")).c_str());
	}

	// Emulator controls

	void JdiClient::LoadFile(const std::string& filename)
	{
		char cmd[0x1000] = { 0 };

		sprintf(cmd, "load \"%s\"", filename.c_str());

		bool res = CallJdiNoReturn(cmd);
		if (!res)
		{
			throw "load failed!";
		}
	}

	void JdiClient::Unload()
	{
		ExecuteCommand("unload");
	}

	void JdiClient::Run()
	{
		ExecuteCommand("run");
	}

	void JdiClient::Stop()
	{
		ExecuteCommand("stop");
	}

	void JdiClient::Reset()
	{
		ExecuteCommand("reset");
	}

	// Performance Counters, SystemTime

	int64_t JdiClient::GetPerformanceCounter(int counter)
	{
		Json::Value* value = CallJdi(("GetPerformanceCounter " + std::to_string(counter)).c_str());
		int64_t res = value->value.AsInt;
		delete value;
		return res;
	}

	void JdiClient::ResetPerformanceCounter(int counter)
	{
		ExecuteCommand(("ResetPerformanceCounter " + std::to_string(counter)).c_str());
	}

	std::string JdiClient::GetSystemTime()
	{
		uint64_t tbr = 0;
		Json::Value* value = CallJdi("OSDateTime");
		std::string res = Util::WstringToString(value->children.front()->value.AsString);
		delete value;
		return res;
	}

	bool JdiClient::JitcEnabled()
	{
		return false;
	}

	int64_t JdiClient::GetResetGekkoMipsCounter()
	{
		int gekkoMips;
		CallJdiReturnInt("GetPerformanceCounter 0", &gekkoMips);
		CallJdiNoReturn("ResetPerformanceCounter 0");
		return gekkoMips;
	}
}
