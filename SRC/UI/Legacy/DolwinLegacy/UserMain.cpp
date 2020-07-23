#include "pch.h"

/* ---------------------------------------------------------------------------  */
/* Basic application output                                                     */

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

        if (GekkoDebuggerOpened)
        {
            char ansiText[0x1000] = { 0, }, * ansiPtr = ansiText;

            TCHAR* tcharPtr = buf;
            while (*tcharPtr)
            {
                *ansiPtr++ = (char)*tcharPtr++;
            }
            *ansiPtr++ = 0;

            char cmd[0x1000];
            sprintf_s(cmd, sizeof(cmd), "echo %s", ansiText);
            UI::Jdi.ExecuteCommand(cmd);
            UI::Jdi.ExecuteCommand("stop");
        }
        else
        {
            MessageBox(NULL, buf, title, MB_ICONHAND | MB_OK | MB_TOPMOST);
            UI::Jdi.ExecuteCommand("exit");
        }
    }

    // fatal error, if user answers no
    // return true if "yes", and false if "no"
    bool DolwinQuestion(const TCHAR* title, const TCHAR* fmt, ...)
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

/* ---------------------------------------------------------------------------  */
/* WinMain (very first run-time initialization and main loop)                   */

/* Check for multiple instancies. */
static void LockMultipleCalls()
{
    static HANDLE dolwinsem;

    // mutex will fail if semephore already exists
    dolwinsem = CreateMutex(NULL, 0, APPNAME);
    if(dolwinsem == NULL)
    {
        auto app_name = std::wstring(APPNAME);
        UI::DolwinReport(_T("We are already running %s!!"), app_name);
        exit(0);    // return good
    }
    CloseHandle(dolwinsem);

    dolwinsem = CreateSemaphore(NULL, 0, 1, APPNAME);
}

static void InitFileSystem(HINSTANCE hInst)
{
    TCHAR cwd[0x1000];

    /* Set current working directory relative to Dolwin executable */
    GetModuleFileName(hInst, cwd, sizeof(cwd));
    *(_tcsrchr(cwd, _T('\\')) + 1) = 0;
    SetCurrentDirectory(cwd);
    
    /* Make sure, that Dolwin has data directory. */
    CreateDirectory(_T(".\\Data"), NULL);
}

// return file name without quotes
char* FixCommandLine(char *lpCmdLine)
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

/* Keyboard accelerators (no need to be shared). */
HACCEL  hAccel;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nShowCmd);

    /* Required by HLE Subsystem */
    if ((uint64_t)hInstance > 0x400000)
    {
        throw "Required by HLE Subsystem";
    }

    InitFileSystem(hInstance);

    /* Allow only one instance of Dolwin to run at once? */
    if (UI::Jdi.GetConfigBool(USER_RUNONCE, USER_UI))
    {
        LockMultipleCalls();
    }

    hAccel = LoadAccelerators(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_ACCELERATOR));

    /* Start the emulator and user interface                        */
    /* (emulator will be initialized during main window creation).  */
    CreateMainWindow(hInstance);

    /* Main loop */
    MSG msg = {};
    while (true)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) == 0)
        {
            Sleep(1);
        }

        /* Idle loop */
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (!TranslateAccelerator(wnd.hMainWindow, hAccel, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    /* Should never reach this point. Dolwin always exits. */
    throw "SHOULD NEVER REACH HERE";
}
