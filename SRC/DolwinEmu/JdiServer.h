// JDI Host Interface

#pragma once

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
Json::Value * 
#ifdef _WINDOWS
__cdecl
#endif
CallJdi(const char* request);

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
bool 
#ifdef _WINDOWS
__cdecl
#endif
CallJdiNoReturn(const char* request);

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
bool 
#ifdef _WINDOWS
__cdecl
#endif
CallJdiReturnInt(const char* request, int* valueOut);

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
bool 
#ifdef _WINDOWS
__cdecl
#endif
CallJdiReturnString(const char* request, char* valueOut, size_t valueSize);

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
bool 
#ifdef _WINDOWS
__cdecl
#endif
CallJdiReturnBool(const char* request, bool* valueOut);

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
void 
#ifdef _WINDOWS
__cdecl
#endif
JdiAddNode(const char* filename, JDI::JdiReflector reflector);

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
void 
#ifdef _WINDOWS
__cdecl
#endif
JdiRemoveNode(const char* filename);

#ifdef _WINDOWS
extern "C" __declspec(dllexport)
#endif
void 
#ifdef _WINDOWS
__cdecl
#endif
JdiAddCmd(const char* name, JDI::CmdDelegate command);
