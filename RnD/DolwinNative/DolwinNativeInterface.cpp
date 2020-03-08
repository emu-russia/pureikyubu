// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void Test()
{
    MessageBox(NULL, _T("Test"), _T("Test"), MB_OK);
}

// set current DVD image for read/seek/open file operations
// return 1 if no errors, and 0 if cannot use file
extern "C" __declspec(dllexport) bool _N_DVDSetCurrent(char* file)
{
    return DVDSetCurrent(file);
}

// seek and read operations on current DVD
extern "C" __declspec(dllexport) void _N_DVDSeek(int position)
{
    DVDSeek(position);
}

extern "C" __declspec(dllexport) void _N_DVDRead(void* buffer, int length)
{
    DVDRead(buffer, length);
}

// open file in DVD root. return file position, or 0 if no such file.
// note : current DVD must be selected first!
// example use : s32 banner = DVDOpenFile("/opening.bnr");
extern "C" __declspec(dllexport) long _N_DVDOpenFile(char* dvdfile)
{
    return DVDOpenFile(dvdfile);
}
