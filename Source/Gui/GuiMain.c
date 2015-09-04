/*++

Copyright (c)

Module Name:

    GuiMain.c

Abstract:

    Application entrypoint (WinMain) and fail-safe messages.

    Application should return 1 for the good reason. 0, for bad.

--*/

#include "dolphin.h"

VOID DolwinError ( PTCHAR Title, PTCHAR Format, ... )
{
    TCHAR Message[0x1000];
    va_list VaList;

    va_start ( VaList, Format );
    _vstprintf ( Message, Format, VaList );
    va_end ( VaList );

    MessageBox ( NULL, Message, Title, MB_ICONHAND | MB_OK | MB_TOPMOST);

    exit (0);   // Return bad
}

BOOLEAN DolwinQuestion ( PTCHAR Title, PTCHAR Format, ... )
{
    TCHAR Message[0x1000];
    va_list VaList;
    int Result;

    va_start ( VaList, Format );
    _vstprintf ( Message, Format, VaList );
    va_end ( VaList );

    Result = MessageBox ( NULL, Message, Title, MB_RETRYCANCEL | MB_ICONHAND | MB_TOPMOST);

    if (Result == IDCANCEL)
        return FALSE;
    else
        return TRUE;
}

VOID DolwinReport ( PTCHAR Format, ... )
{
    TCHAR Message[0x1000];
    va_list VaList;

    va_start ( VaList, Format );
    _vstprintf ( Message, Format, VaList );
    va_end ( VaList );

    MessageBox ( NULL, Message, APPNAME _T(" Reports"), MB_ICONINFORMATION | MB_OK | MB_TOPMOST);
}

int WINAPI WinMain (HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CmdLine, int CmdShow)
{

    ASSERTMSG ( 1 == 0, _T("Bogus Привет") );

    //
    // Should never reach this point.
    //

    DolwinError( _T("ERROR"), APPNAME _T(" ERROR >>> SHOULD NEVER REACH HERE :)") );

    return 0;       // Return bad
}
