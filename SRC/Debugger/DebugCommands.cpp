// Debug commands

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

    static void Tokenize(char* line, std::vector<std::string>& args)
    {
        #define endl    ( line[p] == 0 )
        #define space   ( line[p] == 0x20 )
        #define quot    ( line[p] == '\'' )
        #define dquot   ( line[p] == '\"' )

        int p, start, end;
        p = start = end = 0;

        args.clear();

        // while not end line
        while (!endl)
        {
            // skip space first, if any
            while (space) p++;
            if (!endl && (quot || dquot))
            {   // quotation, need special case
                p++;
                start = p;
                while (1)
                {
                    if (endl)
                    {
                        throw "Open quotation";
                        return;
                    }

                    if (quot || dquot)
                    {
                        end = p;
                        p++;
                        break;
                    }
                    else p++;
                }

                args.push_back(std::string(line + start, end - start));
            }
            else if (!endl)
            {
                start = p;
                while (1)
                {
                    if (endl || space || quot || dquot)
                    {
                        end = p;
                        break;
                    }

                    p++;
                }

                args.push_back(std::string(line + start, end - start));
            }
        }
        #undef space
        #undef quot
        #undef dquot
        #undef endl
    }

    static Json::Value* cmd_script(std::vector<std::string>& args)
    {
        size_t i;
        const char* file;
        std::vector<std::string> commandArgs;

        file = args[1].c_str();

        Report(Channel::Norm, "Loading script: %s\n", file); 

        auto sbuf = Util::FileLoad((std::string)file);
        if (sbuf.empty())
        {
            Report(Channel::Norm, "Cannot open script file!\n");
            return nullptr;
        }

        /* Remove all garbage, like tabs. */
        for (i = 0; i < sbuf.size(); i++)
        {
            char c = sbuf[i];
            
            if (c < ' ')
            {
                c = '\n';
            }
        }

        Report(Channel::Norm, "Executing script...\n");

        int cnt = 1;
        char* ptr = (char*)sbuf.data();
        while (*ptr)
        {
            char line[1000];
            line[i = 0] = 0;

            // Cut string
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
            Report(Channel::Norm, "%i: %s", cnt++, line);
            
            commandArgs.clear();
            Tokenize(line, commandArgs);
            line[0] = 0;

            JDI::Hub.Execute(commandArgs);
        }

        Report(Channel::Norm, "\nDone execute script.\n");
        return nullptr;
    }

    // Echo
    static Json::Value* cmd_echo(std::vector<std::string>& args)
    {
        std::string text = "";

        for (size_t i = 1; i < args.size(); i++)
        {
            text += args[i] + " ";
        }

        Report(Channel::Norm, "%s\n", text.c_str());
        return nullptr;
    }

    static SamplingProfiler* profiler = nullptr;

    static Json::Value* StartProfiler(std::vector<std::string>& args)
    {
        if (profiler)
        {
            Report(Channel::Norm, "Already started.\n");
            return nullptr;
        }

        int period = 5;
        if (args.size() > 2)
        {
            period = atoi(args[2].c_str());
            period = min(2, max(period, 50));
        }

        profiler = new SamplingProfiler(args[1].c_str(), period);

        Report(Channel::Norm, "Profiler started.\n");

        return nullptr;
    }

    static Json::Value* StopProfiler(std::vector<std::string>& args)
    {
        if (profiler == nullptr)
        {
            Report(Channel::Norm, "Not started.\n");
            return nullptr;
        }

        delete profiler;
        profiler = nullptr;

        Report(Channel::Norm, "Profiler stopped.\n");

        return nullptr;
    }

    static Json::Value* GetChannelName(std::vector<std::string>& args)
    {
        Channel chan = (Channel)atoi(args[1].c_str());

        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Array;

        output->AddString(nullptr, Util::StringToWstring(Msgs.DebugChannelToString(chan)).c_str() );

        return output;
    }

    static Json::Value* QueryDebugMessages(std::vector<std::string>& args)
    {
        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Array;

        std::list<std::pair<Channel, std::string>> queue;
        Msgs.QueryDebugMessages(queue);

        for (auto it = queue.begin(); it != queue.end(); ++it)
        {
            output->AddInt(nullptr, (int)it->first);
            output->AddString(nullptr, Util::StringToWstring(it->second).c_str());
        }

        return output;
    }

    static Json::Value* ShowHelp(std::vector<std::string>& args)
    {
        JDI::Hub.Help();
        return nullptr;
    }

    static Json::Value* IsCommandExists(std::vector<std::string>& args)
    {
        Json::Value* output = new Json::Value();
        output->type = Json::ValueType::Bool;
        output->value.AsBool = JDI::Hub.CommandExists(args[1]);
        return output;
    }

    void Reflector()
    {
        JDI::Hub.AddCmd("script", cmd_script);
        JDI::Hub.AddCmd("echo", cmd_echo);
        JDI::Hub.AddCmd("StartProfiler", StartProfiler);
        JDI::Hub.AddCmd("StopProfiler", StopProfiler);
        JDI::Hub.AddCmd("GetChannelName", GetChannelName);
        JDI::Hub.AddCmd("qd", QueryDebugMessages);
        JDI::Hub.AddCmd("help", ShowHelp);
        JDI::Hub.AddCmd("IsCommandExists", IsCommandExists);
    }
}
