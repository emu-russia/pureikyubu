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

			queue.push_back(std::pair<int, std::string>((int)channel->value.AsInt, Util::WstringToString(message->value.AsString)));
		}

		delete value;
	}

	void JdiClient::Report(const std::string& text)
	{
		ExecuteCommand("echo " + text);
	}

	bool JdiClient::IsLoaded()
	{
		bool loaded = false;

		bool res = CallJdiReturnBool("IsLoaded", &loaded);
		if (!res)
		{
			throw "IsLoaded failed!";
		}

		return loaded;
	}

	bool JdiClient::IsCommandExists(const std::string& cmdline)
	{
		bool exists = false;

		std::string cmd = "IsCommandExists " + cmdline;

		bool res = CallJdiReturnBool(cmd.c_str(), &exists);
		if (!res)
		{
			throw "IsCommandExists failed!";
		}

		return exists;
	}

	// Gekko

	bool JdiClient::IsRunning()
	{
		bool running = false;
		CallJdiReturnBool("IsRunning", &running);
		return running;
	}

	void JdiClient::GekkoRun()
	{
		CallJdiNoReturn("GekkoRun");
	}

	void JdiClient::GekkoSuspend()
	{
		CallJdiNoReturn("GekkoSuspend");
	}

	void JdiClient::GekkoStep()
	{
		CallJdiNoReturn("GekkoStep");
	}

	void JdiClient::GekkoSkipInstruction()
	{
		CallJdiNoReturn("GekkoSkipInstruction");
	}

	uint32_t JdiClient::GetGpr(size_t n)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GetGpr %zi", n);

		Json::Value* output = CallJdi(cmd);

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint64_t JdiClient::GetPs0(size_t n)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GetPs0 %zi", n);

		Json::Value* output = CallJdi(cmd);

		uint64_t value = output->value.AsInt;

		delete output;

		return value;
	}

	uint64_t JdiClient::GetPs1(size_t n)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GetPs1 %zi", n);

		Json::Value* output = CallJdi(cmd);

		uint64_t value = output->value.AsInt;

		delete output;

		return value;
	}

	uint32_t JdiClient::GetPc()
	{
		Json::Value* output = CallJdi("GetPc");
		
		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::GetMsr()
	{
		Json::Value* output = CallJdi("GetMsr");

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::GetCr()
	{
		Json::Value* output = CallJdi("GetCr");

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::GetFpscr()
	{
		Json::Value* output = CallJdi("GetFpscr");

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::GetSpr(size_t n)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GetSpr %zi", n);

		Json::Value* output = CallJdi(cmd);

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::GetSr(size_t n)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GetSr %zi", n);

		Json::Value* output = CallJdi(cmd);

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::GetTbu()
	{
		Json::Value* output = CallJdi("GetTbu");

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::GetTbl()
	{
		Json::Value* output = CallJdi("GetTbl");

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	void* JdiClient::TranslateDMmu(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "TranslateDMmu 0x%08X", address);

		Json::Value* output = CallJdi(cmd);

		void* ptr = (void *)output->value.AsInt;

		delete output;

		return ptr;
	}

	void* JdiClient::TranslateIMmu(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "TranslateIMmu 0x%08X", address);

		Json::Value* output = CallJdi(cmd);

		void* ptr = (void*)output->value.AsInt;

		delete output;

		return ptr;
	}

	uint32_t JdiClient::VirtualToPhysicalDMmu(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "VirtualToPhysicalDMmu 0x%08X", address);

		Json::Value* output = CallJdi(cmd);

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::VirtualToPhysicalIMmu(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "VirtualToPhysicalIMmu 0x%08X", address);

		Json::Value* output = CallJdi(cmd);

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	bool JdiClient::GekkoTestBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GekkoTestBreakpoint 0x%08X", address);

		Json::Value* output = CallJdi(cmd);

		bool value = output->value.AsBool;

		delete output;

		return value;
	}

	void JdiClient::GekkoToggleBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GekkoToggleBreakpoint 0x%08X", address);

		Json::Value* output = CallJdi(cmd);
		delete output;
	}

	void JdiClient::GekkoAddOneShotBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GekkoAddOneShotBreakpoint 0x%08X", address);

		Json::Value* output = CallJdi(cmd);
		delete output;
	}

	std::string JdiClient::GekkoDisasm(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GekkoDisasm 0x%08X", address);

		char text[0x200];

		CallJdiReturnString(cmd, text, sizeof(text) - 1);

		return text;
	}

	bool JdiClient::GekkoIsBranch(uint32_t address, uint32_t& targetAddress)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GekkoIsBranch 0x%08X", address);

		Json::Value* output = CallJdi(cmd);

		bool flowControl = false;

		for (auto it = output->children.begin(); it != output->children.end(); ++it)
		{
			Json::Value* child = *it;

			if (child->type == Json::ValueType::Bool)
			{
				flowControl = child->value.AsBool;
			}

			if (child->type == Json::ValueType::Int)
			{
				targetAddress = child->value.AsUint32;
			}
		}

		delete output;

		return flowControl;
	}

	// Actually from HLE Symbols

	uint32_t JdiClient::AddressByName(const std::string& name)
	{
		char cmd[0x100];
		sprintf_s(cmd, sizeof(cmd), "AddressByName %s", name.c_str());

		Json::Value* output = CallJdi(cmd);

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	std::string JdiClient::NameByAddress(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "NameByAddress 0x%08X", address);

		char name[0x200];

		CallJdiReturnString(cmd, name, sizeof(name) - 1);

		return name;
	}

	// DSP

	bool JdiClient::DspIsRunning()
	{
		bool running = false;
		CallJdiReturnBool("DspIsRunning", &running);
		return running;
	}

	void JdiClient::DspRun()
	{
		CallJdiNoReturn("DspRun");
	}

	void JdiClient::DspSuspend()
	{
		CallJdiNoReturn("DspSuspend");
	}

	void JdiClient::DspStep()
	{
		CallJdiNoReturn("DspStep");
	}

	uint16_t JdiClient::DspGetReg(size_t n)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "DspGetReg %zi", n);

		Json::Value* output = CallJdi(cmd);

		uint16_t value = output->value.AsUint16;

		delete output;

		return value;
	}

	uint16_t JdiClient::DspGetPsr()
	{
		Json::Value* output = CallJdi("DspGetPsr");

		uint16_t value = output->value.AsUint16;

		delete output;

		return value;
	}

	uint16_t JdiClient::DspGetPc()
	{
		Json::Value* output = CallJdi("DspGetPsr");

		uint16_t value = output->value.AsUint16;

		delete output;

		return value;
	}

	uint64_t JdiClient::DspPackProd()
	{
		Json::Value* output = CallJdi("DspPackProd");

		uint64_t value = output->value.AsInt;

		delete output;

		return value;
	}

	void* JdiClient::DspTranslateDMem(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "DspTranslateDMem 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);

		void* ptr = (void*)output->value.AsInt;

		delete output;

		return ptr;
	}

	void* JdiClient::DspTranslateIMem(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "DspTranslateIMem 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);

		void* ptr = (void*)output->value.AsInt;

		delete output;

		return ptr;
	}

	bool JdiClient::DspTestBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "DspTestBreakpoint 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);

		bool value = output->value.AsBool;

		delete output;

		return value;
	}

	void JdiClient::DspToggleBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "DspToggleBreakpoint 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);
		delete output;
	}

	void JdiClient::DspAddOneShotBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "DspAddOneShotBreakpoint 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);
		delete output;
	}

	std::string JdiClient::DspDisasm(uint32_t address, size_t& instrSizeWords, bool& flowControl)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "DspDisasm 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);

		std::string text = "";

		for (auto it = output->children.begin(); it != output->children.end(); ++it)
		{
			Json::Value* child = *it;

			if (child->type == Json::ValueType::Bool)
			{
				flowControl = child->value.AsBool;
			}

			if (child->type == Json::ValueType::Int)
			{
				instrSizeWords = child->value.AsInt;
			}

			if (child->type == Json::ValueType::String)
			{
				text = Util::WstringToString(child->value.AsString);
			}
		}

		delete output;

		return text;
	}

	bool JdiClient::DspIsCall(uint32_t address, uint32_t& targetAddress)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "DspIsCall 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);

		bool flag = false;

		for (auto it = output->children.begin(); it != output->children.end(); ++it)
		{
			Json::Value* child = *it;

			if (child->type == Json::ValueType::Bool)
			{
				flag = child->value.AsBool;
			}

			if (child->type == Json::ValueType::Int)
			{
				targetAddress = child->value.AsUint16;
			}
		}

		delete output;

		return flag;
	}

	bool JdiClient::DspIsCallOrJump(uint32_t address, uint32_t& targetAddress)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "DspIsCallOrJump 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);

		bool flag = false;

		for (auto it = output->children.begin(); it != output->children.end(); ++it)
		{
			Json::Value* child = *it;

			if (child->type == Json::ValueType::Bool)
			{
				flag = child->value.AsBool;
			}

			if (child->type == Json::ValueType::Int)
			{
				targetAddress = child->value.AsUint16;
			}
		}

		delete output;

		return flag;
	}

}
