#include "pch.h"

namespace Debug
{
    static JdiHub hub;      // Singletone.

    JdiHub::JdiHub()
    {

    }

    JdiHub::~JdiHub()
    {
        while (!nodes.empty())
        {
            Json* node = nodes.back();
            nodes.pop_back();
            delete node;
        }
    }

    void JdiHub::AddCmd(std::string name, CmdDelegate command)
    {
        MySpinLock::Lock(&reflexMapLock);
        hub.reflexMap[name] = command;
        MySpinLock::Unlock(&reflexMapLock);
    }

    void JdiHub::AddJson(std::wstring filename, JdiReflector reflector)
    {
        Json* json = new Json();

        size_t jsonTextSize = 0;
        void* jsonText = UI::FileLoad(filename, jsonTextSize);
        assert(jsonText);

        json->Deserialize(jsonText, jsonTextSize);

        delete[] jsonText;

        hub.nodes.push_back(json);

        reflector();
    }

    void JdiHub::Help()
    {
    }

    Json::Value* JdiHub::Execute(std::vector<std::string>& args)
    {
        return nullptr;
    }

    bool JdiHub::CommandExists(std::vector<std::string>& args)
    {
        return false;
    }

    // External API

    void AddCmd(std::string name, CmdDelegate command)
    {
        hub.AddCmd(name, command);
    }

    void AddJson(std::wstring filename, JdiReflector reflector)
    {
        hub.AddJson(filename, reflector);
    }

    void Help()
    {
        hub.Help();
    }

    Json::Value* Execute(std::vector<std::string>& args)
    {
        return hub.Execute(args);
    }

    bool CommandExists(std::vector<std::string>& args)
    {
        return hub.CommandExists(args);
    }

}
