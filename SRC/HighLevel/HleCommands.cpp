// HLE JDI. Available only after emulation has been started.
#include "pch.h"

namespace HLE
{
    // Ask the emulator whether it is loaded or not.
    static bool IsLoaded()
    {
        Json::Value* loaded = Debug::Hub.ExecuteFast("IsLoaded");
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
        uint32_t address = address = strtoul(args[1].c_str(), NULL, 0);
        if (address != 0)
        {
            DBReport2(DbgChannel::HLE, "New symbol: %08X %s\n", address, args[2].c_str());
            SYMAddNew(address, args[2].c_str());
        }
        else DBReport2(DbgChannel::HLE, "Wrong address!\n");

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

	void JdiReflector()
	{
        Debug::Hub.AddCmd("syms", cmd_syms);
        Debug::Hub.AddCmd("name", cmd_name);
        Debug::Hub.AddCmd("savemap", cmd_savemap);
        Debug::Hub.AddCmd("DumpThreads", DumpThreads);
        Debug::Hub.AddCmd("DumpContext", DumpContext);
	}
}
