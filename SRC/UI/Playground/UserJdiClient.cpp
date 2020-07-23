// JDI Communication.

#include "pch.h"

namespace UI
{
	JdiClient Jdi;		// Singletone.

	JdiClient::JdiClient()
	{
#ifdef _WINDOWS
		dll = LoadLibraryA("DolwinEmuForPlayground.dll");
		if (dll == nullptr)
		{
			throw "Load DolwinEmuForPlayground failed!";
		}

		CallJdi = (CALL_JDI) GetProcAddress(dll, "CallJdi");
		CallJdiNoReturn = (CALL_JDI_NO_RETURN) GetProcAddress(dll, "CallJdiNoReturn");
		CallJdiReturnInt = (CALL_JDI_RETURN_INT) GetProcAddress(dll, "CallJdiReturnInt");
		CallJdiReturnString = (CALL_JDI_RETURN_STRING) GetProcAddress(dll, "CallJdiReturnString");
		CallJdiReturnBool = (CALL_JDI_RETURN_BOOL) GetProcAddress(dll, "CallJdiReturnBool");

		JdiAddNode = (JDI_ADD_NODE)GetProcAddress(dll, "JdiAddNode");
		JdiRemoveNode = (JDI_REMOVE_NODE)GetProcAddress(dll, "JdiRemoveNode");
		JdiAddCmd = (JDI_ADD_CMD)GetProcAddress(dll, "JdiAddCmd");
#endif
	}

	JdiClient::~JdiClient()
	{
#ifdef _WINDOWS
		FreeLibrary(dll);
#endif
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

		sprintf_s(cmd, sizeof(cmd), "MountIso \"%s\"", path.c_str());

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

	}

	void JdiClient::DvdRead(std::vector<uint8_t>& data)
	{

	}

	uint32_t JdiClient::DvdOpenFile(const std::string& filename)
	{
		return 0;
	}

	bool JdiClient::DvdCoverOpened()
	{
		return false;
	}

	void JdiClient::DvdOpenCover()
	{
		ExecuteCommand("OpenLid");
	}

	void JdiClient::DvdCloseCover()
	{
		ExecuteCommand("CloseLid");
	}

	// Configuration access

	std::string JdiClient::GetConfigString(const std::string& var, const std::string& path)
	{
		return std::string();
	}

	void JdiClient::SetConfigString(const std::string& var, const std::string& newVal, const std::string& path)
	{

	}

	int JdiClient::GetConfigInt(const std::string& var, const std::string& path)
	{
		return 0;
	}

	void JdiClient::SetConfigInt(const std::string& var, int newVal, const std::string& path)
	{

	}

	bool JdiClient::GetConfigBool(const std::string& var, const std::string& path)
	{
		return false;
	}

	void JdiClient::SetConfigBool(const std::string& var, bool newVal, const std::string& path)
	{

	}

	// Emulator controls

	void JdiClient::LoadFile(const std::string& filename)
	{
		char cmd[0x1000] = { 0 };

		sprintf_s(cmd, sizeof(cmd), "load \"%s\"", filename.c_str());

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

	std::string JdiClient::DebugChannelToString(int chan)
	{
		char str[0x100] = { 0 };
		char cmd[0x30] = { 0, };

		sprintf_s(cmd, sizeof(cmd), "GetChannelName %i", chan);

		bool res = CallJdiReturnString(cmd, str, sizeof(str) - 1);
		if (!res)
		{
			throw "GetChannelName failed!";
		}

		return std::string(str);
	}

	void JdiClient::QueryDebugMessages(std::list<std::pair<int, std::string>>& queue)
	{
		Json::Value* value = CallJdi("qd");
		if (value == nullptr)
		{
			throw "QueryDebugMessages failed!";
		}

		if (value->type != Json::ValueType::Array)
		{
			throw "QueryDebugMessages invalid format!";
		}

		auto it = value->children.begin();

		while (it != value->children.end())
		{
			Json::Value* channel = *it;
			++it;

			Json::Value* message = *it;
			++it;

			if (channel->type != Json::ValueType::Int || message->type != Json::ValueType::String)
			{
				throw "QueryDebugMessages invalid format of array key-values!";
			}

			queue.push_back(std::pair<int, std::string>((int)channel->value.AsInt, Util::TcharToString(message->value.AsString)));
		}

		delete value;
	}

}
