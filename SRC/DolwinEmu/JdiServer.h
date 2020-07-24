// JDI Host Interface

#pragma once

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
Json::Value * __cdecl CallJdi(const char* request);

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
bool __cdecl CallJdiNoReturn(const char* request);

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
bool __cdecl CallJdiReturnInt(const char* request, int* valueOut);

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
bool __cdecl CallJdiReturnString(const char* request, char* valueOut, size_t valueSize);

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
bool __cdecl CallJdiReturnBool(const char* request, bool* valueOut);

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
void __cdecl JdiAddNode(const char* filename, JDI::JdiReflector reflector);

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
void __cdecl JdiRemoveNode(const char* filename);

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
void __cdecl JdiAddCmd(const char* name, JDI::CmdDelegate command);
