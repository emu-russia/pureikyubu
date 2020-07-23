// Local JDI Host

#include "pch.h"

#ifdef _WINDOWS
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:

            // TODO: Check ImageBase.

            /* Required by HLE Subsystem */
            //if ((uint64_t)hInstance > 0x400000)
            //{
            //    UI::DolwinError(_T("Error"), _T("Image base must be below or equal 0x400`000. Required by HLE Subsystem for artifical CPU `CallVM` opcode."));
            //    return -1;
            //}

            EMUCtor();
            break;
        case DLL_PROCESS_DETACH:
            EMUDtor();
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}
#endif // _WINDOWS

#define endl    ( line[p] == 0 )
#define space   ( line[p] == 0x20 )
#define quot    ( line[p] == '\'' )
#define dquot   ( line[p] == '\"' )

static void Tokenize(const char* line, std::vector<std::string>& args)
{
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
}

#undef space
#undef quot
#undef dquot
#undef endl

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
Json::Value* __cdecl CallJdi(const char* request)
{
    std::vector<std::string> args;

    Tokenize(request, args);

    return JDI::Hub.Execute(args);
}

#ifdef _WINDOWS
extern "C" __declspec(dllexport) 
#endif
bool __cdecl CallJdiNoReturn(const char* request)
{
    CallJdi(request);
    return true;
}

#ifdef _WINDOWS
extern "C" __declspec(dllexport) 
#endif
bool __cdecl CallJdiReturnInt(const char* request, int* valueOut)
{
    if (!valueOut)
    {
        return false;
    }

    Json::Value* value = CallJdi(request);
    if (!value)
    {
        return false;
    }
    if (value->type != Json::ValueType::Int)
    {
        delete value;
        return false;
    }

    *valueOut = value->value.AsInt;
    delete value;

    return true;
}

#ifdef _WINDOWS
extern "C" __declspec(dllexport) 
#endif
bool __cdecl CallJdiReturnString(const char* request, char* valueOut, size_t valueSize)
{
    if (!valueOut)
    {
        return false;
    }

    Json::Value* value = CallJdi(request);
    if (!value)
    {
        return false;
    }

    // String are returned in form of: [ "string" ]

    if (value->type != Json::ValueType::Array)
    {
        delete value;
        return false;
    }

    if (value->children.size() < 1)
    {
        delete value;
        return false;
    }

    Json::Value* child = value->children.front();

    if (child->type != Json::ValueType::String )
    {
        delete value;
        return false;
    }

    // Check string size

    size_t sizeInChars = _tcslen(child->value.AsString);
    size_t sizeInBytes = sizeInChars * sizeof(TCHAR);
    if (sizeInBytes >= valueSize)
    {
        delete value;
        return false;
    }
    
    // Copy out

    TCHAR* tstrPtr = child->value.AsString;
    char* valuePtr = valueOut;

    for (size_t i = 0; i < sizeInChars; i++)
    {
        *valuePtr++ = (char)*tstrPtr++;
    }
    *valuePtr++ = 0;

    delete value;

    return true;
}

#ifdef _WINDOWS
extern "C" __declspec(dllexport) 
#endif
bool __cdecl CallJdiReturnBool(const char* request, bool* valueOut)
{
    if (!valueOut)
    {
        return false;
    }

    Json::Value* value = CallJdi(request);
    if (!value)
    {
        return false;
    }
    if ( ! (value->type == Json::ValueType::Int || value->type == Json::ValueType::Bool) )
    {
        delete value;
        return false;
    }

    if (value->type == Json::ValueType::Bool)
    {
        *valueOut = value->value.AsBool;
    }
    else
    {
        *valueOut = value->value.AsInt != 0;
    }

    delete value;

    return true;
}

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
void __cdecl JdiAddNode(const char* filename, JDI::JdiReflector reflector)
{
    JDI::Hub.AddNode(Util::StringToWstring(filename), reflector);
}

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
void __cdecl JdiRemoveNode(const char* filename)
{
    JDI::Hub.RemoveNode(Util::StringToWstring(filename));
}

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
void __cdecl JdiAddCmd(const char* name, JDI::CmdDelegate command)
{
    JDI::Hub.AddCmd(name, command);
}
