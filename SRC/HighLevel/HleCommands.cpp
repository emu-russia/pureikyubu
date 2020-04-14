// HLE JDI. Available only after emulation has been started.
#include "pch.h"

namespace HLE
{
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
        if (args.size() < 2)
        {
        }
        else
        {
            if (!strcmp(args[1].c_str(), ".")) SaveMAP(nullptr);
            else SaveMAP(args[1].c_str());
        }

        return nullptr;
    }

    static Json::Value* cmd_threads(std::vector<std::string>& args)
    {
        DumpDolphinOsThreads();
        return nullptr;
    }

    static Json::Value* DumpContext(std::vector<std::string>& args)
    {
        DBReport("Bogus!\n");
        return nullptr;
    }

	void JdiReflector()
	{
        Debug::Hub.AddCmd("syms", cmd_syms);
        Debug::Hub.AddCmd("name", cmd_name);
        Debug::Hub.AddCmd("savemap", cmd_savemap);
        Debug::Hub.AddCmd("threads", cmd_threads);
        Debug::Hub.AddCmd("DumpContext", DumpContext);
	}
}
