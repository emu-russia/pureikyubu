// Dolwin entrypoint (WinMain) and fail-safe application messages.
// WinMain() should never return. 
// exit() should return 1, for good reason. 0, for bad.
#include "dolphin.h"

// ---------------------------------------------------------------------------
// basic application output

// fatal error
void DolwinError(const char *title, const char *fmt, ...)
{
    va_list arg;
    char buf[0x1000];

    va_start(arg, fmt);
    vsprintf(buf, fmt, arg);
    va_end(arg);

    if (emu.doldebug)
    {
        DBHalt(buf);
    }
    else
    {
        MessageBox(NULL, buf, title, MB_ICONHAND | MB_OK | MB_TOPMOST);
    }

    // send message, to stop emulation,
    // let the user decide - to close emu or not
    if(emu.running)
    {
        SendMessage(wnd.hMainWindow, WM_COMMAND, ID_FILE_UNLOAD, 0);
    }
    exit(0);    // return bad
}

// fatal error, if user answers no
// return TRUE if "yes", and FALSE if "no"
BOOL DolwinQuestion(const char *title, const char *fmt, ...)
{
    va_list arg;
    char buf[0x1000];

    va_start(arg, fmt);
    vsprintf(buf, fmt, arg);
    va_end(arg);

    int btn = MessageBox(
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
    vsprintf(buf, fmt, arg);
    va_end(arg);

    MessageBox(NULL, buf, APPNAME " Reports", MB_ICONINFORMATION | MB_OK | MB_TOPMOST);
}

// ---------------------------------------------------------------------------
// WinMain (very first run-time initialization and main loop)

// long jump buffer is used, because we cant be sure, that UpdateMainWindow
// is returned normally. example situation : emulation was already stopped by
// EMUClose, but UpdateMainWindow is already called by some hardware to update
// Dolwin main window; as result, UpdateMainWindow will return to nowhere, because
// emulation is stopped already (and hardware is closed).
static jmp_buf mainloop;

// jump to emulator's main loop
void DolwinMainLoop()
{
    longjmp(mainloop, 0);
}

// check for multiple instancies
static void LockMultipleCalls()
{
    static  HANDLE  dolwinsem;

    // mutex will fail if semephore already exists
    dolwinsem = CreateMutex(NULL, 0, APPNAME);
    if(dolwinsem == NULL)
    {
        DolwinReport("We are already running " APPNAME "!!");
        exit(1);    // return good
    }
    CloseHandle(dolwinsem);

    dolwinsem = CreateSemaphore(NULL, 0, 1, APPNAME);
}

// set proper current working directory, create missing directories
static void InitFileSystem(HINSTANCE hInst)
{
    // set current working directory relative to Dolwin binary
    GetModuleFileName(hInst, ldat.cwd, 1024);
    *(strrchr(ldat.cwd, '\\') + 1) = 0;
    SetCurrentDirectory(ldat.cwd);

    // make sure, that Dolwin has data directory.
    CreateDirectory(".\\Data", NULL);
}

// return file name without quotes
char * FixCommandLine(char *lpCmdLine)
{
    if(*lpCmdLine == '\"' || *lpCmdLine == '\'')
    {
        lpCmdLine++;
    }
    int len = strlen(lpCmdLine);
    if(lpCmdLine[len-1] == '\"' || lpCmdLine[len-1] == '\'')
    {
        lpCmdLine[len-1] = 0;
    }
    return lpCmdLine;
}

// entrypoint
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
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
        EMUOpen(
            GetConfigInt(USER_CPU_TIME, USER_CPU_TIME_DEFAULT),
            GetConfigInt(USER_CPU_DELAY, USER_CPU_DELAY_DEFAULT),
            GetConfigInt(USER_CPU_CF, USER_CPU_CF_DEFAULT) );
        // will exits after closing emulation
        // returning control back to frontend
    }

    // init emu and user interface (emulator will be initialized
    // during main window creation).
    CreateMainWindow();

    // set long jump buffer to main loop
    setjmp(mainloop);

    // roll main loop
    for(;;)
    {
        // Idle loop
        while(!emu.running)
        {
            if(emu.doldebug)
            {
                DBStart();
            }
            else
            {
                Sleep(10);
                UpdateMainWindow(emu.running);
            }
        }
    }

    // should never reach this point. Dolwin always exit()'s.
    DolwinError("ERROR", APPNAME " ERROR >>> SHOULD NEVER REACH HERE :)");
    return 0;   // return bad
}
