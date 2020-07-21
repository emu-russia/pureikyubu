#include "pch.h"

/* ---------------------------------------------------------------------------  */
/* Basic application output                                                     */

namespace UI
{
    // fatal error
    void DolwinError(std::wstring_view title, std::wstring_view fmt, ...)
    {
        va_list arg;
        TCHAR buf[0x1000];

        va_start(arg, fmt);
        _vstprintf_s(buf, _countof(buf) - 1, fmt.data(), arg);
        va_end(arg);

        if (emu.doldebug)
        {
            char ansiText[0x1000] = { 0, }, * ansiPtr = ansiText;

            TCHAR* tcharPtr = buf;
            while (*tcharPtr)
            {
                *ansiPtr++ = (char)*tcharPtr++;
            }
            *ansiPtr++ = 0;

            DBHalt(ansiPtr);
        }
        else
        {
            MessageBox(NULL, buf, title.data(), MB_ICONHAND | MB_OK | MB_TOPMOST);
            std::vector<std::string> cmd{ "exit" };
            Debug::Hub.Execute(cmd);
        }
    }

    // fatal error, if user answers no
    // return TRUE if "yes", and FALSE if "no"
    bool DolwinQuestion(std::wstring_view title, std::wstring_view fmt, ...)
    {
        va_list arg;
        TCHAR buf[0x1000];

        va_start(arg, fmt);
        _vstprintf_s(buf, _countof(buf) - 1, fmt.data(), arg);
        va_end(arg);

        int btn = MessageBox(
            NULL,
            buf,
            title.data(),
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
    void DolwinReport(std::wstring_view fmt, ...)
    {
        va_list arg;
        TCHAR buf[0x1000];

        va_start(arg, fmt);
        _vstprintf_s(buf, _countof(buf) - 1, fmt.data(), arg);
        va_end(arg);

        auto app_name = std::wstring(APPNAME);
        MessageBox(NULL, buf, (app_name + L" Reports").data(), MB_ICONINFORMATION | MB_OK | MB_TOPMOST);
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
        UI::DolwinReport(L"We are already running %s!!", app_name.c_str());
        exit(0);    // return good
    }
    CloseHandle(dolwinsem);

    dolwinsem = CreateSemaphore(NULL, 0, 1, APPNAME);
}

static void InitFileSystem()
{
    /* Set current working directory relative to Dolwin executable */
    auto cwd = std::filesystem::current_path();
    ldat.cwd = cwd;
    
    /* Make sure, that Dolwin has data directory. */
    _chdir(cwd.string().c_str());
    std::filesystem::create_directory(".\\Data");
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
    assert((uint64_t)hInstance <= 0x400000);

    InitFileSystem();

    /* Allow only one instance of Dolwin to run at once? */
    if (GetConfigBool(USER_RUNONCE, USER_UI))
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
            UpdateProfiler();
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
    UI::DolwinError(L"ERROR", L"%s ERROR >>> SHOULD NEVER REACH HERE :)", APPNAME);
    return 1;
}
