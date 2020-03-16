// Dolwin debugger interface;
// currently console debugger, migrated from 0.08+ is in use, 
// but you may add GUI aswell. see "DOCS\EMU\debug.txt" for details of 
// debugger replacement.
#include "pch.h"

// message output
static void dummy(const char *text, ...) {}
void (*DBHalt)(const char *text, ...)   = dummy;
void (*DBReport)(const char *text, ...) = dummy;

static HANDLE consoleThreadHandle = INVALID_HANDLE_VALUE;
static DWORD consoleThreadId;

static DWORD WINAPI DBThreadProc(LPVOID lpParameter)
{
    con.update |= CON_UPDATE_ALL;
    con_refresh(1);
    con_start();
    return 0;
}

void DBOpen()           // open debugger window
{
    DBHalt = con_error;
    DBReport = con_print;
    con_open();

    // start debugger thread
    if (consoleThreadHandle == INVALID_HANDLE_VALUE)
    {
        consoleThreadHandle = CreateThread(NULL, 0, DBThreadProc, &con, 0, &consoleThreadId);
        assert(consoleThreadHandle != INVALID_HANDLE_VALUE);
    }
}

void DBClose()          // close debugger window
{
    if (consoleThreadHandle != INVALID_HANDLE_VALUE)
    {
        TerminateThread(consoleThreadHandle, 0);
        WaitForSingleObject(consoleThreadHandle, 1000);
        consoleThreadHandle = INVALID_HANDLE_VALUE;
        con_close();
    }
    DBHalt = dummy;
    DBReport = dummy;
}
