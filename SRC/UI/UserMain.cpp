// Dolwin entrypoint (WinMain) and fail-safe application messages.
// WinMain() should never return. 
// exit() should return 0, for good reason. 1, for bad.
#include "pch.h"

// ---------------------------------------------------------------------------
// basic application output

namespace UI
{

    // fatal error
    void DolwinError(const TCHAR* title, const TCHAR* fmt, ...)
    {
        va_list arg;
        TCHAR buf[0x1000];

        va_start(arg, fmt);
        _vstprintf_s(buf, _countof(buf) - 1, fmt, arg);
        va_end(arg);

        if (emu.doldebug)
        {
            char ansiText[0x1000] = { 0, }, * ansiPtr = ansiText;

            TCHAR* tcharPtr = buf;
            while (*tcharPtr)
            {
                *ansiPtr++ = (char)tcharPtr++;
            }
            *ansiPtr++ = 0;

            DBHalt(ansiPtr);
        }
        else
        {
            MessageBox(NULL, buf, title, MB_ICONHAND | MB_OK | MB_TOPMOST);
            exit(1);    // return bad
        }
    }

    // fatal error, if user answers no
    // return TRUE if "yes", and FALSE if "no"
    BOOL DolwinQuestion(const TCHAR* title, const TCHAR* fmt, ...)
    {
        va_list arg;
        TCHAR buf[0x1000];

        va_start(arg, fmt);
        _vstprintf_s(buf, _countof(buf) - 1, fmt, arg);
        va_end(arg);

        int btn = MessageBox(
            NULL,
            buf,
            title,
            MB_RETRYCANCEL | MB_ICONHAND | MB_TOPMOST
            );
        if (btn == IDCANCEL)
        {
            SendMessage(wnd.hMainWindow, WM_COMMAND, ID_FILE_UNLOAD, 0);
            return FALSE;
        }
        else return TRUE;
    }

    // application message
    void DolwinReport(const TCHAR* fmt, ...)
    {
        va_list arg;
        TCHAR buf[0x1000];

        va_start(arg, fmt);
        _vstprintf_s(buf, _countof(buf) - 1, fmt, arg);
        va_end(arg);

        MessageBox(NULL, buf, APPNAME _T(" Reports"), MB_ICONINFORMATION | MB_OK | MB_TOPMOST);
    }

}

// ---------------------------------------------------------------------------
// WinMain (very first run-time initialization and main loop)

// check for multiple instancies
static void LockMultipleCalls()
{
    static  HANDLE  dolwinsem;

    // mutex will fail if semephore already exists
    dolwinsem = CreateMutex(NULL, 0, APPNAME);
    if(dolwinsem == NULL)
    {
        UI::DolwinReport(_T("We are already running ") APPNAME _T("!!"));
        exit(0);    // return good
    }
    CloseHandle(dolwinsem);

    dolwinsem = CreateSemaphore(NULL, 0, 1, APPNAME);
}

// set proper current working directory, create missing directories
static void InitFileSystem(HINSTANCE hInst)
{
    // set current working directory relative to Dolwin executable
    GetModuleFileName(hInst, ldat.cwd, sizeof(ldat.cwd));
    *(_tcsrchr(ldat.cwd, _T('\\')) + 1) = 0;
    SetCurrentDirectory(ldat.cwd);

    // make sure, that Dolwin has data directory.
    CreateDirectory(_T(".\\Data"), NULL);
}

// return file name without quotes
char * FixCommandLine(char *lpCmdLine)
{
    if(*lpCmdLine == '\"' || *lpCmdLine == '\'')
    {
        lpCmdLine++;
    }
    size_t len = strlen(lpCmdLine);
    if(lpCmdLine[len-1] == '\"' || lpCmdLine[len-1] == '\'')
    {
        lpCmdLine[len-1] = 0;
    }
    return lpCmdLine;
}

// keyboard accelerators (no need to be shared)
HACCEL  hAccel;

// entrypoint
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nShowCmd);

    // prepare file system
    InitFileSystem(hInstance);

    // run Dolwin once ?
    if(GetConfigInt(USER_RUNONCE, USER_UI) != 0)
    {
        LockMultipleCalls();
    }

    hAccel = LoadAccelerators(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_ACCELERATOR));

    // init emu and user interface (emulator will be initialized
    // during main window creation).
    CreateMainWindow();

    // Idle loop
    for(;;)
    {
        MSG msg;

        // Idle loop
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (!TranslateAccelerator(wnd.hMainWindow, hAccel, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    // should never reach this point. Dolwin always exit()'s.
    UI::DolwinError(_T("ERROR"), APPNAME _T(" ERROR >>> SHOULD NEVER REACH HERE :)"));
    return 1;   // return bad
}
