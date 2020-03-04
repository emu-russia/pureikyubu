// Dolwin entrypoint (WinMain) and fail-safe application messages.
// WinMain() should never return. 
// exit() should return 1, for good reason. 0, for bad.
#include "dolphin.h"

// ---------------------------------------------------------------------------
// basic application output

// fatal error
void DolwinError(char *title, char *fmt, ...)
{
    va_list arg;
    char buf[0x1000];

    va_start(arg, fmt);
    vsprintf(buf, fmt, arg);
    va_end(arg);

    MessageBox(NULL, buf, title, MB_ICONHAND | MB_OK | MB_TOPMOST);

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
BOOL DolwinQuestion(char *title, char *fmt, ...)
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
void DolwinReport(char *fmt, ...)
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

// execute another app
BOOL DolwinExecute(char *appName, char *cmdLine)
{
#ifndef LOADPARMS32
    typedef struct tagLOADPARMS32
    {  
        LPSTR lpEnvAddress;     // address of environment strings 
        LPSTR lpCmdLine;        // address of command line 
        LPSTR lpCmdShow;        // how to show new program 
        DWORD dwReserved;       // must be zero 
    } LOADPARMS32; 
#endif  // LOADPARMS32

    LOADPARMS32 params;
    WORD cmdShow[] = { 2, SW_NORMAL };

    memset(&params, 0, sizeof(LOADPARMS32));

    char arg[1024];
    sprintf(arg, "%c%s", strlen(cmdLine), cmdLine);
    params.lpCmdLine = arg;
    params.lpCmdShow = (LPSTR)cmdShow;

    if(LoadModule(appName, &params) > 31) return TRUE;
    else return FALSE;
}

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

    // make sure, that Dolwin has plugins directory.
    CreateDirectory(".\\Plugins", NULL);

    // today is a good day
    PlaySound( "monkeyisland.wav", NULL,
                SND_FILENAME | SND_LOOP | SND_ASYNC | SND_NOWAIT | SND_NODEFAULT);
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

static void DolwinExceptionHandler(LPEXCEPTION_POINTERS exp)
{
    char dump[1024];

    sprintf(dump,
        "Gekko CPU Registers:\n"
        "\n"
        "r0 :%08X\tr8 :%08X\tr16:%08X\tr24:%08X\n"
        "sp :%08X\tr9 :%08X\tr17:%08X\tr25:%08X\n"
        "sd2:%08X\tr10:%08X\tr18:%08X\tr26:%08X\n"
        "r3 :%08X\tr11:%08X\tr19:%08X\tr27:%08X\n"
        "r4 :%08X\tr12:%08X\tr20:%08X\tr28:%08X\n"
        "r5 :%08X\tsd1:%08X\tr21:%08X\tr29:%08X\n"
        "r6 :%08X\tr14:%08X\tr22:%08X\tr30:%08X\n"
        "r7 :%08X\tr15:%08X\tr23:%08X\tr31:%08X\n"
        "\n"
        "lr :%08X\tcr :%08X\tdec:%08X\n"
        "pc :%08X\txer:%08X\tctr:%08X\n",
        GPR[ 0], GPR[ 8], GPR[16], GPR[24],
        GPR[ 1], GPR[ 9], GPR[17], GPR[25],
        GPR[ 2], GPR[10], GPR[18], GPR[26],
        GPR[ 3], GPR[11], GPR[19], GPR[27],
        GPR[ 4], GPR[12], GPR[20], GPR[28],
        GPR[ 5], GPR[13], GPR[21], GPR[29],
        GPR[ 6], GPR[14], GPR[22], GPR[30],
        GPR[ 7], GPR[15], GPR[23], GPR[31],
        LR, CR, DEC,
        PC, XER, CTR
    );

    DolwinError( "Exception during emulation run-flow!",
                 "%s", dump );
}

// entrypoint
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    // check previous instance, maybe we are running on 3.11 =:)
    ASSERT( hPrevInstance != NULL,
            "Sorry, " APPNAME " doesnt support OS'es below Windows95.");

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

        // disable plugin warning
        SetConfigInt(USER_PLUG_WARN, FALSE);

        CreateMainWindow();

        LoadFile(FixCommandLine(lpCmdLine));
        EMUClose();
        EMUOpen();
        // will exits after closing emulation
        // returning control back to frontend
    }
    else SetConfigInt(USER_PLUG_WARN, TRUE);

    // init emu and user interface (emulator will be initialized
    // during main window creation).
    CreateMainWindow();

    // set long jump buffer to main loop
    setjmp(mainloop);

    // roll main loop
    static LPEXCEPTION_POINTERS exp;
#ifndef  _DEBUG
    __try {
#endif
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
#ifndef  _DEBUG
    }
    __except(exp = GetExceptionInformation(), EXCEPTION_EXECUTE_HANDLER)
    {
        DolwinExceptionHandler(exp);
    }
#endif

    // should never reach this point. Dolwin always exit()'s.
    DolwinError("ERROR", APPNAME " ERROR >>> SHOULD NEVER REACH HERE :)");
    return 0;   // return bad
}

const char *verId = "$Id: Dolwin 0.10 (30.11.2004)$\r\n";
