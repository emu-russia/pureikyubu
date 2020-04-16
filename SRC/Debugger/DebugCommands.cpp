// Legacy cmd.cpp will gradually be replaced by this module.

#include "pch.h"

namespace Debug
{
    static bool testempty(char* str)
    {
        int len = (int)strlen(str);

        for (int i = 0; i < len; i++)
        {
            if (str[i] > ' ') return false;
        }

        return true;
    }

    static Json::Value* cmd_script(std::vector<std::string>& args)
    {
        int i;
        const char* file;
        std::vector<std::string> commandArgs;

        file = args[1].c_str();

        DBReport("Loading script: %s\n", file);

        size_t size = 0;
        char* sbuf = (char*)UI::FileLoad(file, &size);
        if (!sbuf)
        {
            DBReport("Cannot open script file!\n");
            return nullptr;
        }

        // remove all garbage, like tabs
        for (i = 0; i < size; i++)
        {
            if (sbuf[i] < ' ') sbuf[i] = '\n';
        }

        DBReport("Executing script...\n");

        int cnt = 1;
        char* ptr = sbuf;
        while (*ptr)
        {
            char line[1000];
            line[i = 0] = 0;

            // cut string
            while (*ptr == '\n') ptr++;
            if (!*ptr) break;
            while (*ptr != '\n') line[i++] = *ptr++;
            line[i++] = 0;

            // remove comments
            char* p = line;
            while (*p)
            {
                if (p[0] == '/' && p[1] == '/')
                {
                    *p = 0;
                    break;
                }
                p++;
            }

            // remove spaces at the end
            p = &line[strlen(line) - 1];
            while (*p <= ' ') p--;
            if (*p) p[1] = 0;

            // remove spaces at the beginning
            p = line;
            while (*p <= ' ' && *p) p++;

            // empty string ?
            if (!*p) continue;

            // execute line
            if (testempty(line)) continue;
            DBReport("%i: %s", cnt++, line);
            con_tokenizing(line);
            line[0] = 0;
            con_update(CON_UPDATE_EDIT | CON_UPDATE_MSGS);

            commandArgs.clear();
            for (int i = 0; i < roll.tokencount; i++)
            {
                commandArgs.push_back(roll.tokens[i]);
            }

            con_command(commandArgs);
        }
        free(sbuf);

        DBReport("\nDone execute script.\n");
        con.update |= CON_UPDATE_ALL;
        return nullptr;
    }

    // Echo
    static Json::Value* cmd_echo(std::vector<std::string>& args)
    {
        DBReport("%s\n", args[1].c_str());
        return nullptr;
    }

	void Reflector()
	{
        Debug::Hub.AddCmd("script", cmd_script);
        Debug::Hub.AddCmd("echo", cmd_echo);
	}
}
