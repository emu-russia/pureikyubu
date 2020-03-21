// Dolwin entrypoint (WinMain) and fail-safe application messages.
// WinMain() should never return. 
// exit() should return 0, for good reason. 1, for bad.
#include "dolphin.h"

// ---------------------------------------------------------------------------
// basic application output

// fatal error
void DolwinError(const char *title, const char *fmt, ...)
{
    va_list arg;
    char buf[0x1000];

    va_start(arg, fmt);
    vsprintf_s(buf, sizeof(buf), fmt, arg);
    va_end(arg);

    if (emu.doldebug)
    {
        DBHalt(buf);
    }
    else
    {
        MessageBoxA(NULL, buf, title, MB_ICONHAND | MB_OK | MB_TOPMOST);
        exit(1);    // return bad
    }
}

// fatal error, if user answers no
// return TRUE if "yes", and FALSE if "no"
BOOL DolwinQuestion(const char *title, const char *fmt, ...)
{
    va_list arg;
    char buf[0x1000];

    va_start(arg, fmt);
    vsprintf_s(buf, sizeof(buf), fmt, arg);
    va_end(arg);

    int btn = MessageBoxA(
        NULL,
        buf,
        title,
        MB_RETRYCANCEL | MB_ICONHAND | MB_TOPMOST
    );
    if(btn == IDCANCEL)
    {
        SendMessage(wnd.hMainWindow, WM_COMMAND, ID_FILE_UNLOAD, 0);
        return FALSE;
    }
    else return TRUE;
}

// application message
void DolwinReport(const char *fmt, ...)
{
    va_list arg;
    char buf[0x1000];

    va_start(arg, fmt);
    vsprintf_s(buf, sizeof(buf), fmt, arg);
    va_end(arg);

    MessageBoxA(NULL, buf, APPNAME " Reports", MB_ICONINFORMATION | MB_OK | MB_TOPMOST);
}

// ---------------------------------------------------------------------------
// WinMain (very first run-time initialization and main loop)

// check for multiple instancies
static void LockMultipleCalls()
{
    static  HANDLE  dolwinsem;

    // mutex will fail if semephore already exists
    dolwinsem = CreateMutexA(NULL, 0, APPNAME);
    if(dolwinsem == NULL)
    {
        DolwinReport("We are already running " APPNAME "!!");
        exit(0);    // return good
    }
    CloseHandle(dolwinsem);

    dolwinsem = CreateSemaphoreA(NULL, 0, 1, APPNAME);
}

// set proper current working directory, create missing directories
static void InitFileSystem(HINSTANCE hInst)
{
    // set current working directory relative to Dolwin executable
    GetModuleFileNameA(hInst, ldat.cwd, 1024);
    *(strrchr(ldat.cwd, '\\') + 1) = 0;
    SetCurrentDirectoryA(ldat.cwd);

    // make sure, that Dolwin has data directory.
    CreateDirectoryA(".\\Data", NULL);
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
static  HACCEL  hAccel;

// entrypoint
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nShowCmd);

    // prepare file system
    InitFileSystem(hInstance);

    // run Dolwin once ?
    if(GetConfigInt(USER_RUNONCE, USER_RUNONCE_DEFAULT) == TRUE)
    {
        LockMultipleCalls();
    }

    // check command line (used by frontends)
    if(strlen(lpCmdLine) && lpCmdLine)
    {
        ldat.cmdline = TRUE;

        CreateMainWindow();

        LoadFile(FixCommandLine(lpCmdLine));
        EMUClose();
        EMUOpen();
        // will exits after closing emulation
        // returning control back to frontend
    }

    // init emu and user interface (emulator will be initialized
    // during main window creation).
    CreateMainWindow();

    hAccel = LoadAccelerators(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_ACCELERATOR));

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

        Sleep(10);
    }

    // should never reach this point. Dolwin always exit()'s.
    DolwinError("ERROR", APPNAME " ERROR >>> SHOULD NEVER REACH HERE :)");
    return 1;   // return bad
}
