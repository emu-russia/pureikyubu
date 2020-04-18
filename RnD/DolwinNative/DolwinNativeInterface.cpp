// When Dolwin starts in a managed environment, the interop library calls EMUCtor at boot and EMUDtor at exit.
// All other interactions are made through JDI calls (CallJdi).

#include "pch.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
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
