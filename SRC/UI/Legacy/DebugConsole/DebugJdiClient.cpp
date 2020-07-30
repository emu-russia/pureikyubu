// JDI Communication.

#include "pch.h"

namespace Debug
{
	JdiClient Jdi;		// Singletone.

	JdiClient::JdiClient()
	{
#ifdef _WINDOWS
		dll = LoadLibraryA("DolwinEmu.dll");
		if (dll == nullptr)
		{
			throw "Load DolwinEmu failed!";
		}

		CallJdi = (CALL_JDI)GetProcAddress(dll, "CallJdi");
		CallJdiNoReturn = (CALL_JDI_NO_RETURN)GetProcAddress(dll, "CallJdiNoReturn");
		CallJdiReturnInt = (CALL_JDI_RETURN_INT)GetProcAddress(dll, "CallJdiReturnInt");
		CallJdiReturnString = (CALL_JDI_RETURN_STRING)GetProcAddress(dll, "CallJdiReturnString");
		CallJdiReturnBool = (CALL_JDI_RETURN_BOOL)GetProcAddress(dll, "CallJdiReturnBool");

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

	// Generic debug

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

	// Oldest messages first
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

	void JdiClient::Report(const std::string& text)
	{
		ExecuteCommand("echo " + text);
	}

	bool IsLoaded()
	{
		return false;
	}

	bool IsRunning()
	{
		return false;
	}

	// Gekko

	uint32_t JdiClient::GetGpr(size_t n)
	{
		return 0;
	}

	uint64_t JdiClient::GetPs0(size_t n)
	{
		return 0;
	}

	uint64_t JdiClient::GetPs1(size_t n)
	{
		return 0;
	}

	uint32_t JdiClient::GetPc()
	{
		return 0;
	}

	uint32_t JdiClient::GetMsr()
	{
		return 0;
	}

	uint32_t JdiClient::GetCr()
	{
		return 0;
	}

	uint32_t JdiClient::GetFpscr()
	{
		return 0;
	}

	uint32_t JdiClient::GetSpr(size_t n)
	{
		return 0;
	}

	uint32_t JdiClient::GetSr(size_t n)
	{
		return 0;
	}

	uint32_t JdiClient::GetTbu()
	{
		return 0;
	}

	uint32_t JdiClient::GetTbl()
	{
		return 0;
	}

	// DSP

	uint16_t JdiClient::DspGetReg(size_t n)
	{
		return 0;
	}

	uint16_t JdiClient::DspGetPsr()
	{
		return 0;
	}

	uint16_t JdiClient::DspGetPc()
	{
		return 0;
	}

	uint64_t JdiClient::DspPackProd()
	{
		return 0;
	}

}
