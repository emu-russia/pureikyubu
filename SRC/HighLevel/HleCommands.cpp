// HLE JDI. Some commands available only after emulation has been started.
#include "pch.h"

using namespace Debug;

namespace HLE
{
    // Ask the emulator whether it is loaded or not.
    static bool IsLoaded()
    {
        Json::Value* loaded = JDI::Hub.ExecuteFast("IsLoaded");
        if (!loaded)
            return false;

        bool isLoaded = loaded->value.AsBool;
        delete loaded;

        return isLoaded;
    }

    static Json::Value* cmd_syms(std::vector<std::string>& args)
    {
        SYMList(args[1].c_str());
        return nullptr;
    }

    static Json::Value* cmd_name(std::vector<std::string>& args)
    {
        uint32_t address = strtoul(args[1].c_str(), NULL, 0);
        if (address != 0)
        {
            Report(Channel::HLE, "New symbol: %08X %s\n", address, args[2].c_str());
            SYMAddNew(address, args[2].c_str());
        }
        else Report(Channel::HLE, "Wrong address!\n");

        return nullptr;
    }

    static Json::Value* cmd_savemap(std::vector<std::string>& args)
    {
        if (!strcmp(args[1].c_str(), ".")) SaveMAP(nullptr);
        else SaveMAP(args[1].c_str());

        return nullptr;
    }

    static Json::Value* DumpThreads(std::vector<std::string>& args)
    {
        if (!IsLoaded())
            return nullptr;

        bool display = true;
        if (args.size() >= 2)
        {
            display = atoi(args[1].c_str()) != 0 ? true : false;
        }

        return DumpDolphinOsThreads(display);
    }

    static Json::Value* DumpContext(std::vector<std::string>& args)
    {
        if (!IsLoaded())
            return nullptr;

        uint32_t effectiveAddr = strtoul(args[1].c_str(), nullptr, 0);

        bool display = true;
        if (args.size() >= 3)
        {
            display = atoi(args[2].c_str()) != 0 ? true : false;
        }
        
        return DumpDolphinOsContext(effectiveAddr, display);
    }

    static Json::Value* UnloadMap(std::vector<std::string>& args)
    {
        SYMKill();
        hle.mapfile[0] = 0;
        return nullptr;
    }

    static Json::Value* LoadMap(std::vector<std::string>& args)
    {
        SYMKill();
        hle.mapfile[0] = 0;
        LoadMAP(args[1].c_str(), false);
        return nullptr;
    }

    static Json::Value* AddMap(std::vector<std::string>& args)
    {
        if (hle.mapfile[0] == 0)
        {
            LoadMAP(args[1].c_str(), false);
        }
        else
        {
            LoadMAP(args[1].c_str(), true);
        }
        return nullptr;
    }

    static Json::Value* AddressByName(std::vector<std::string>& args)
    {
        uint32_t address;
        
        if (IsLoaded())
        {
            address = SYMAddress(args[1].c_str());
        }
        else
        {
            address = 0;
        }

        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Int;

        output->value.AsInt = 0;
        output->value.AsUint32 = address;

        return output;
    }

    static Json::Value* NameByAddress(std::vector<std::string>& args)
    {
        char* name = nullptr;
        
        if (IsLoaded())
        {
            name = SYMName(strtoul(args[1].c_str(), nullptr, 0));
        }

        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Array;

        output->AddAnsiString(nullptr, name ? name : "");
        return output;
    }

    static Json::Value* OSDateTimeInternal(std::vector<std::string>& args)
    {
        wchar_t timeStr[0x100] = { 0, };

        if (args.size() > 1)
        {
            OSTimeFormat(timeStr, strtoull(args[1].c_str(), nullptr, 0), false);
        }
        else
        {
            OSTimeFormat(timeStr, Gekko::Gekko->GetTicks(), false);
        }

        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Array;

        output->AddString(nullptr, timeStr);

        return output;
    }

    static Json::Value* OSTimeInternal(std::vector<std::string>& args)
    {
        wchar_t timeStr[0x100] = { 0, };

        if (args.size() > 1)
        {
            OSTimeFormat(timeStr, strtoull(args[1].c_str(), nullptr, 0), true);
        }
        else
        {
            OSTimeFormat(timeStr, Gekko::Gekko->GetTicks(), true);
        }

        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Array;

        output->AddString(nullptr, timeStr);

        return output;
    }

    static Json::Value* GetNearestNameInternal(std::vector<std::string>& args)
    {
        uint32_t address = strtoul(args[1].c_str(), nullptr, 0);

        size_t offset = 0;
        char* nearestName = SYMGetNearestName(address, offset);

        if (!nearestName)
            return nullptr;

        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Object;

        output->AddAnsiString("name", nearestName);
        output->AddInt("offset", (int)offset);

        return output;
    }

	void JdiReflector()
	{
        JDI::Hub.AddCmd("syms", cmd_syms);
        JDI::Hub.AddCmd("name", cmd_name);
        JDI::Hub.AddCmd("savemap", cmd_savemap);
        JDI::Hub.AddCmd("DumpThreads", DumpThreads);
        JDI::Hub.AddCmd("DumpContext", DumpContext);
        JDI::Hub.AddCmd("UnloadMap", UnloadMap);
        JDI::Hub.AddCmd("LoadMap", LoadMap);
        JDI::Hub.AddCmd("AddMap", AddMap);
        JDI::Hub.AddCmd("AddressByName", AddressByName);
        JDI::Hub.AddCmd("NameByAddress", NameByAddress);
        JDI::Hub.AddCmd("OSDateTime", OSDateTimeInternal);
        JDI::Hub.AddCmd("OSTime", OSTimeInternal);
        JDI::Hub.AddCmd("GetNearestName", GetNearestNameInternal);
	}
}
