// JDI Host Interface

#pragma once

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
