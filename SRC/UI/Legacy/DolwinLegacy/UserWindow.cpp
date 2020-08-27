/* Main window controls and creation.                                       */
/* main window consist from 3 parts : main menu, selector and statusbar.    */
/* selector is active only when emulator is in Idle state (not running);    */
/* statusbar is used to show current emulator state and performance.        */
/* last note : DO NOT USE WINDOWS API CODE IN OTHER SUB-SYSTEMS!!           */
#include "pch.h"

/* All important data is placed here */
UserWindow wnd;

Debug::DspDebug* dspDebug;
Debug::GekkoDebug* gekkoDebug;

/* ---------------------------------------------------------------------------  */
/* Statusbar                                                                    */

/* Set default values of statusbar parts */
static void ResetStatusBar()
{
    SetStatusText(STATUS_ENUM::Progress, L"Idle");
    SetStatusText(STATUS_ENUM::Fps,      L"");
    SetStatusText(STATUS_ENUM::Timing,   L"");
    SetStatusText(STATUS_ENUM::Time,     L"");
}

/* Create status bar window */
static void CreateStatusBar()
{
    int parts[] = { 0, 360, 420, 480, -1 };

    if (wnd.hMainWindow == NULL) return;

    /* Create window */
    wnd.hStatusWindow = CreateStatusWindow(
        WS_CHILD | WS_VISIBLE,
        NULL,
        wnd.hMainWindow,
        ID_STATUS_BAR
    );
    assert(wnd.hStatusWindow);

    /* Depart statusbar */
    SendMessage( wnd.hStatusWindow, 
                 SB_SETPARTS, 
                 (WPARAM)sizeof(parts) / sizeof(int), 
                 (LPARAM)parts);

    /* Set default values */
    ResetStatusBar();
}

/* Change text in specified statusbar part */
void SetStatusText(STATUS_ENUM sbPart, const std::wstring & text, bool post)
{
    if (wnd.hStatusWindow == NULL)
    {
        return;
    }

    if(post)
    {
        PostMessage(wnd.hStatusWindow, SB_SETTEXT, (WPARAM)(sbPart), (LPARAM)text.data());
    }
    else
    {
        SendMessage(wnd.hStatusWindow, SB_SETTEXT, (WPARAM)(sbPart), (LPARAM)text.data());
    }
}

/* Get text of statusbar part */
std::wstring GetStatusText(STATUS_ENUM sbPart)
{
    static auto sbText = std::wstring(256, 0);

    if (wnd.hStatusWindow == NULL) return NULL;

    SendMessage(wnd.hStatusWindow, SB_GETTEXT, (WPARAM)(sbPart), (LPARAM)sbText.data());
    return sbText;
}

void StartProgress(int range, int delta)
{
    StopProgress();

    RECT rect;
    SendMessage(wnd.hStatusWindow, SB_GETRECT, 1, (LPARAM)&rect);
    int cyVScroll = GetSystemMetrics(SM_CYVSCROLL);

    wnd.hProgress = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE, 
        rect.left, rect.top + abs(rect.bottom - rect.top) / 2 - cyVScroll / 2, rect.right, cyVScroll, 
        wnd.hStatusWindow, NULL, GetModuleHandle(NULL), 0);
    if(wnd.hProgress == NULL) return;
    
    SendMessage(wnd.hProgress, PBM_SETPOS, 0, 0);
    SendMessage(wnd.hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, range));
    SendMessage(wnd.hProgress, PBM_SETSTEP, delta, 0);
}

void StepProgress()
{
    if(wnd.hProgress)
    {
        SendMessage(wnd.hProgress, PBM_STEPIT, 0, 0);
    }
}

void StopProgress()
{
    if(wnd.hProgress)
    {
        DestroyWindow(wnd.hProgress);
        wnd.hProgress = NULL;
    }
}

#pragma region "Recent files list"

#define MAX_RECENT  5   // if you want to increase, you must also add new ID_FILE_RECENT_*

/* Returns -1 if not found */
static int GetMenuItemIndex(HMENU hMenu, const std::wstring & item)
{
    int index = 0;
    wchar_t buf[MAX_PATH];

    while (index < GetMenuItemCount(hMenu))
    {
        if (GetMenuString(hMenu, index, buf, sizeof(buf) - 1, MF_BYPOSITION))
        {
            if (!wcscmp(item.c_str(), buf)) return index;
        }
        index++;
    }
    return -1;
}

static void SetRecentEntry(int index, const wchar_t* str)
{
    char var[256] = { 0, };
    sprintf_s (var, sizeof(var), USER_RECENT, index);
    UI::Jdi.SetConfigString(var, Util::WstringToString(str), USER_UI);
}

static std::wstring GetRecentEntry(int index)
{
    char var[256];
    sprintf_s (var, sizeof(var), USER_RECENT, index);
    return Util::StringToWstring(UI::Jdi.GetConfigString(var, USER_UI));
}

void UpdateRecentMenu(HWND hwnd)
{
    HMENU   hMainMenu;
    HMENU   hFileMenu;
    HMENU   hReloadMenu;
    UINT    idx;

    // search for required menu sub-item
    hMainMenu = GetMenu(hwnd);
    if(hMainMenu == NULL) return;

    idx = GetMenuItemIndex(hMainMenu, L"&File");             // take care about it
    if(idx < 0) return;
    hFileMenu = GetSubMenu(hMainMenu, idx);
    if(hFileMenu == NULL) return;

    idx = GetMenuItemIndex(hFileMenu, L"&Reopen\tCtrl+R");   // take care about it
    if(idx < 0) return;
    hReloadMenu = GetSubMenu(hFileMenu, idx);
    if(hReloadMenu == NULL) return;

    // clear recent list
    while(GetMenuItemCount(hReloadMenu))
    {
        DeleteMenu(hReloadMenu, 0, MF_BYPOSITION);
    }

    // if no recent, add empty
    if(UI::Jdi.GetConfigInt(USER_RECENT_NUM, USER_UI) == 0)
    {
        AppendMenu(hReloadMenu, MF_GRAYED | MF_STRING, ID_FILE_RECENT_1, L"None");
    }
    else
    {
        auto buffer = std::wstring();
        int RecentNum = UI::Jdi.GetConfigInt(USER_RECENT_NUM, USER_UI);

        for(int i = 0, n = RecentNum; i < RecentNum; i++, n--)
        {
            buffer = fmt::format(L"{:s}", UI::FileShortName(GetRecentEntry(n), 3));
            AppendMenu(hReloadMenu, MF_STRING, ID_FILE_RECENT_1 + i, buffer.data());
        }
    }

    DrawMenuBar(hwnd);
}

void AddRecentFile(const std::wstring& path)
{
    int n;
    int RecentNum = UI::Jdi.GetConfigInt(USER_RECENT_NUM, USER_UI);

    // check if item already present in list
    for (n = 1; n <= RecentNum; n++)
    {
        if (!_wcsicmp(path.c_str(), GetRecentEntry(n).c_str()))
        {
            // place old recent to the top
            // and move upper recents down
            wchar_t old[MAX_PATH] = { 0, };
            swprintf_s(old, _countof(old) - 1, L"%s", GetRecentEntry(n).c_str());
            for (n = n + 1; n <= RecentNum; n++)
            {
                SetRecentEntry(n - 1, GetRecentEntry(n).c_str());
            }
            SetRecentEntry(RecentNum, old);
            UpdateRecentMenu(wnd.hMainWindow);
            return;
        }
    }

    // increase amount of recent files
    RecentNum++;
    if (RecentNum > MAX_RECENT)
    {
        // move list up
        for (n = 1; n < MAX_RECENT; n++)
        {
            SetRecentEntry(n, GetRecentEntry(n + 1).c_str());
        }
        RecentNum = 5;
    }
    UI::Jdi.SetConfigInt(USER_RECENT_NUM, RecentNum, USER_UI);

    // add new entry
    SetRecentEntry(RecentNum, path.c_str());
    UpdateRecentMenu(wnd.hMainWindow);
}

// index = 1..max
void LoadRecentFile(int index)
{
    int RecentNum = UI::Jdi.GetConfigInt(USER_RECENT_NUM, USER_UI);
    std::wstring path = GetRecentEntry((RecentNum+1) - index);
    UI::Jdi.Unload();
    UI::Jdi.LoadFile(Util::WstringToString(path));
    if (gekkoDebug)
    {
        gekkoDebug->InvalidateAll();
    }
    OnMainWindowOpened(path.c_str());
    UI::Jdi.Run();
}

#pragma endregion "Recent files list"

// ---------------------------------------------------------------------------
// window actions, on emu start/stop

// select sort method (Options->View->Sort by)
static void SelectSort()
{
    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_1, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_2, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_3, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_4, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_5, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_6, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_7, MF_BYCOMMAND | MF_UNCHECKED);
    
    switch((SELECTOR_SORT)UI::Jdi.GetConfigInt(USER_SORTVIEW, USER_UI))
    {
        case SELECTOR_SORT::Default:
            CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_1, MF_BYCOMMAND | MF_CHECKED);
            break;
        case SELECTOR_SORT::Filename:
            CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_2, MF_BYCOMMAND | MF_CHECKED);
            break;
        case SELECTOR_SORT::Title:
            CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_3, MF_BYCOMMAND | MF_CHECKED);
            break;
        case SELECTOR_SORT::Size:
            CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_4, MF_BYCOMMAND | MF_CHECKED);
            break;
        case SELECTOR_SORT::ID:
            CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_5, MF_BYCOMMAND | MF_CHECKED);
            break;
        case SELECTOR_SORT::Comment:
            CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_6, MF_BYCOMMAND | MF_CHECKED);
            break;
        default:
            CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_7, MF_BYCOMMAND | MF_CHECKED);
    }
}

// change Swap Controls
void ModifySwapControls(bool stateOpened)
{
    if(stateOpened)       // opened
    {
        SetMenuItemText(wnd.hMainMenu, ID_FILE_COVER, L"&Close Cover");
        EnableMenuItem(wnd.hMainMenu, ID_FILE_CHANGEDVD, MF_BYCOMMAND | MF_ENABLED);
    }
    else            // closed
    {
        SetMenuItemText(wnd.hMainMenu, ID_FILE_COVER, L"&Open Cover");
        EnableMenuItem(wnd.hMainMenu, ID_FILE_CHANGEDVD, MF_BYCOMMAND | MF_GRAYED);
    }
}

// set menu selector-related controls state
void ModifySelectorControls(bool active)
{
    if(active)
    {
        SetMenuItemText(wnd.hMainMenu, ID_OPTIONS_VIEW_DISABLE, L"&Disable Selector");
        EnableMenuItem(wnd.hMainMenu, ID_FILE_EDITINFO, MF_BYCOMMAND | MF_ENABLED);
        EnableMenuItem(wnd.hMainMenu, ID_FILE_REFRESH, MF_BYCOMMAND | MF_ENABLED);
        EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_ENABLED);
        EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_ENABLED);
        EnableMenuItem( GetSubMenu(GetSubMenu(wnd.hMainMenu, 2), 1),    // Sort By..
                        5, MF_BYPOSITION | MF_ENABLED);
        EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_FILEFILTER, MF_BYCOMMAND | MF_ENABLED);
    }
    else
    {
        SetMenuItemText(wnd.hMainMenu, ID_OPTIONS_VIEW_DISABLE, L"&Enable Selector");
        EnableMenuItem(wnd.hMainMenu, ID_FILE_EDITINFO, MF_BYCOMMAND | MF_GRAYED);
        EnableMenuItem(wnd.hMainMenu, ID_FILE_REFRESH, MF_BYCOMMAND | MF_GRAYED);
        EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_GRAYED);
        EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_GRAYED);
        EnableMenuItem( GetSubMenu(GetSubMenu(wnd.hMainMenu, 2), 1),    // Sort By..
                        5, MF_BYPOSITION | MF_GRAYED);
        EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_FILEFILTER, MF_BYCOMMAND | MF_GRAYED);
    }
}

// called once, during main window creation
static void OnMainWindowCreate(HWND hwnd)
{
    // save handlers
    wnd.hMainWindow = hwnd;
    wnd.hMainMenu = GetMenu(wnd.hMainWindow);

    // run once ?
    if (UI::Jdi.GetConfigBool(USER_RUNONCE, USER_UI))
    {
        CheckMenuItem(wnd.hMainMenu, ID_RUN_ONCE, MF_BYCOMMAND | MF_CHECKED);
    }
    else
    {
        CheckMenuItem(wnd.hMainMenu, ID_RUN_ONCE, MF_BYCOMMAND | MF_UNCHECKED);
    }

    // debugger enabled ?
    CheckMenuItem(wnd.hMainMenu, ID_DEBUG_CONSOLE, MF_BYCOMMAND | MF_UNCHECKED);
    if (UI::Jdi.GetConfigBool(USER_DOLDEBUG, USER_UI))
    {
        gekkoDebug = new Debug::GekkoDebug();
        CheckMenuItem(wnd.hMainMenu, ID_DEBUG_CONSOLE, MF_BYCOMMAND | MF_CHECKED);
    }

    // load accelerators
    InitCommonControls();

    // always on top (not in debug)
    wnd.ontop = UI::Jdi.GetConfigBool(USER_ONTOP, USER_UI);
    if(wnd.ontop) CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_ALWAYSONTOP, MF_CHECKED);
    else CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_ALWAYSONTOP, MF_UNCHECKED);
    SetAlwaysOnTop(wnd.hMainWindow, wnd.ontop);

    // recent menu
    UpdateRecentMenu(wnd.hMainWindow);

    // dvd swap controls
    ModifySwapControls(false);

    // child windows
    CreateStatusBar();
    ResizeMainWindow(640, 480-32);  // 32 pixels overscan

    // selector disabled ?
    usel.active = UI::Jdi.GetConfigBool(USER_SELECTOR, USER_UI);
    ModifySelectorControls(usel.active);

    // icon size
    bool smallSize = UI::Jdi.GetConfigBool(USER_SMALLICONS, USER_UI);
    if(smallSize)
    {
        CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_UNCHECKED);
        CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_CHECKED);
    }
    else
    {
        CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_UNCHECKED);
        CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_CHECKED);
    }

    // select sort method
    SelectSort();

    // enable drop operation
    DragAcceptFiles(wnd.hMainWindow, TRUE);

    // Add UI methods
    UI::Jdi.JdiAddNode(UI_JDI_JSON, UIReflector);

    // simulate close operation, like we just stopped emu
    OnMainWindowClosed();
}

// called once, when Dolwin exits to OS
static void OnMainWindowDestroy()
{
    UI::Jdi.Unload();

    UI::Jdi.JdiRemoveNode(UI_JDI_JSON);

    // disable drop operation
    DragAcceptFiles(wnd.hMainWindow, FALSE);

    if (gekkoDebug)
    {
        delete gekkoDebug;
    }

    if (dspDebug)
    {
        delete dspDebug;
    }

    UI::Jdi.ExecuteCommand("exit");
}

// emulation has started - do proper actions
void OnMainWindowOpened(const wchar_t* currentFileName)
{
    // disable selector
    CloseSelector();
    ModifySelectorControls(false);
    EnableMenuItem( GetSubMenu(wnd.hMainMenu, GetMenuItemIndex(     // View
                    wnd.hMainMenu, L"&Options") ), 1, MF_BYPOSITION | MF_GRAYED );

    std::wstring newTitle, gameTitle;
    wchar_t drive[_MAX_DRIVE + 1] = { 0, }, dir[_MAX_DIR] = { 0, }, name[_MAX_PATH] = { 0, }, ext[_MAX_EXT] = { 0, };
    bool dvd = false;
    bool bootrom = !wcscmp(currentFileName, L"Bootrom");

    if (!bootrom)
    {
        wchar_t* extension = wcsrchr((wchar_t*)currentFileName, L'.');

        if (!_wcsicmp(extension, L".dol"))
        {
            dvd = false;
        }
        else if (!_wcsicmp(extension, L".elf"))
        {
            dvd = false;
        }
        else if (!_wcsicmp(extension, L".bin"))
        {
            dvd = false;
        }
        else if (!_wcsicmp(extension, L".iso"))
        {
            dvd = true;
        }
        else if (!_wcsicmp(extension, L".gcm"))
        {
            dvd = true;
        }

        _wsplitpath_s(currentFileName,
            drive, _countof(drive) - 1,
            dir, _countof(dir) - 1,
            name, _countof(name) - 1,
            ext, _countof(ext) - 1);
    }

    // set new title for main window

    if (dvd)
    {
        UI::Jdi.DvdMount(Util::WstringToString(currentFileName));

        // get DiskID
        std::vector<uint8_t> diskID;
        diskID.resize(4);
        UI::Jdi.DvdSeek(0);
        UI::Jdi.DvdRead(diskID);

        // Get title from banner

        std::vector<uint8_t> bnrRaw = DVDLoadBanner(currentFileName);

        DVDBanner2* bnr = (DVDBanner2*)bnrRaw.data();

        wchar_t longTitle[0x200];

        char* ansiPtr = (char*)bnr->comments[0].longTitle;
        wchar_t* wcharPtr = longTitle;

        while (*ansiPtr)
        {
            *wcharPtr++ = (uint8_t)*ansiPtr++;
        }
        *wcharPtr++ = 0;

        // Convert SJIS Title to Unicode

        if ( UI::Jdi.DvdRegionById((char *)diskID.data()) == "JPN")
        {
            size_t size, chars;
            uint16_t* widePtr = SjisToUnicode(longTitle, &size, &chars);
            uint16_t* unicodePtr;

            if (widePtr)
            {
                wcharPtr = longTitle;
                unicodePtr = widePtr;

                while (*unicodePtr)
                {
                    *wcharPtr++ = *unicodePtr++;
                }
                *wcharPtr++ = 0;

                free(widePtr);
            }
        }

        // Update recent files list and add selector path

        wchar_t fullPath[MAX_PATH];

        swprintf_s(fullPath, _countof(fullPath) - 1, L"%s%s", drive, dir);

        // add new recent entry
        //AddRecentFile(currentFileName);

        // add new path to selector
        AddSelectorPath(fullPath);      // all checks are there

        gameTitle = longTitle;
        newTitle = fmt::format(L"{:s} Running {:s}", APPNAME, gameTitle);
    }
    else
    {
        if (bootrom)
        {
            gameTitle = currentFileName;
        }
        else
        {
            gameTitle = fmt::format(L"{:s} demo", name);
        }
        
        newTitle = fmt::format(L"{:s} Running {:s}", APPNAME, gameTitle);
    }
    
    SetWindowText(wnd.hMainWindow, newTitle.c_str());
}

// emulation stop in progress
void OnMainWindowClosed()
{
    // restore current working directory
    SetCurrentDirectory(wnd.cwd.c_str());

    // enable selector
    CreateSelector();
    ModifySelectorControls(usel.active);
    EnableMenuItem(GetSubMenu(wnd.hMainMenu, GetMenuItemIndex(     // View
        wnd.hMainMenu, L"&Options")), 1, MF_BYPOSITION | MF_ENABLED);

    // set to Idle
    auto win_name = fmt::format(L"{:s} - {:s} ({:s})", APPNAME, APPDESC, Util::StringToWstring(UI::Jdi.GetVersion()));
    SetWindowText(wnd.hMainWindow, win_name.c_str());
    ResetStatusBar();
}

// ---------------------------------------------------------------------------
// window controls

// resize client area to fit in given width and height
void ResizeMainWindow(int width, int height)
{
    RECT rc;
    int x, y, w, h;

    GetWindowRect(wnd.hMainWindow, &rc);

    // left-upper corner
    x = rc.left;
    y = rc.top;

    // calculate adjustment
    rc.left = 0;
    rc.top = 0;
    rc.right = width;
    rc.bottom = height;
    AdjustWindowRect(&rc, WIN_STYLE, 1);

    // width and height
    w = rc.right - rc.left;
    h = rc.bottom - rc.top + GetSystemMetrics(SM_CYCAPTION) + 9;

    // adjust by statusbar height
    if(IsWindow(wnd.hStatusWindow))
    {
        GetWindowRect(wnd.hStatusWindow, &rc);
        h += (WORD)(rc.bottom - rc.top);
    }

    // move window
    MoveWindow(wnd.hMainWindow, x, y, w, h, TRUE);
    SendMessage(wnd.hMainWindow, WM_SIZE, 0, 0);
}

/* Main window procedure : "return 0" to leave, "break" to continue DefWindowProc() */
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto name = std::wstring();
    int recent;

    switch(msg)
    {
        /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */ 
        /* Create/destroy messages                                                      */

        case WM_CREATE:
        {
            OnMainWindowCreate(hwnd);
            return 0;
        }
        case WM_CLOSE:
        {
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY:
        {
            OnMainWindowDestroy();
            return 0;
        }
        
        /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
        /* Window controls                                                              */

        case WM_COMMAND:
        {
            /* Load recent file */
            if (LOWORD(wParam) >= ID_FILE_RECENT_1 &&
                LOWORD(wParam) <= ID_FILE_RECENT_5)
            {
                LoadRecentFile(LOWORD(wParam) - ID_FILE_RECENT_1 + 1);
            }
            else switch (LOWORD(wParam))
            {
                /* Load DVD/executable (START) */
                case ID_FILE_LOAD:
                {
                    name = UI::FileOpenDialog(wnd.hMainWindow, UI::FileType::All);
                    if (!name.empty())
                    {
                    loadFile:
                    
                        UI::Jdi.LoadFile(Util::WstringToString(name));
                        if (gekkoDebug)
                        {
                            gekkoDebug->InvalidateAll();
                        }
                        OnMainWindowOpened(name.c_str());
                        UI::Jdi.Run();
                    }

                    return 0;
                }
                /* Reload last opened file (RESET) */
                case ID_FILE_RELOAD:
                {
                    recent = UI::Jdi.GetConfigInt(USER_RECENT_NUM, USER_UI);
                    if (recent > 0)
                    {
                        LoadRecentFile(1);
                    }

                    return 0;
                }
                /* Unload file (STOP) */
                case ID_FILE_UNLOAD:
                {
                    UI::Jdi.Stop();
                    Sleep(100);
                    UI::Jdi.Unload();
                    OnMainWindowClosed();

                    return 0;
                }
                /* Load bootrom */
                case ID_FILE_IPLMENU:
                {
                    UI::Jdi.LoadFile("Bootrom");
                    OnMainWindowOpened(L"Bootrom");
                    if (gekkoDebug == nullptr)
                    {
                        UI::Jdi.Run();
                    }
                    else
                    {
                        gekkoDebug->SetDisasmCursor(0xfff0'0100);
                    }
                    return 0;
                }
                /* Open/close DVD lid */
                case ID_FILE_COVER:
                {
                    if ( UI::Jdi.DvdCoverOpened() )   /* Close lid */
                    {
                        UI::Jdi.DvdCloseCover();
                        ModifySwapControls(false);
                    }
                    else /* Open lid */
                    {
                        UI::Jdi.DvdOpenCover();
                        ModifySwapControls(true);
                    }

                    return 0;
                }
                /* Set new current DVD image. */
                case ID_FILE_CHANGEDVD:
                {
                    name = UI::FileOpenDialog(wnd.hMainWindow, UI::FileType::Dvd);
                    if (!name.empty() && UI::Jdi.DvdCoverOpened() )
                    {
                        /* Bad */
                        if (! UI::Jdi.DvdMount ( Util::WstringToString(name)) )
                        {
                            return 0;
                        }

                        /* Close lid */
                        UI::Jdi.DvdCloseCover();
                        ModifySwapControls(false);
                    }

                    return 0;
                }
                /* Exit to OS */
                case ID_FILE_EXIT:
                {
                    DestroyWindow(hwnd);
                    return 0;
                }
                /* Settings dialog */
                case ID_OPTIONS_SETTINGS:
                {
                    OpenSettingsDialog(hwnd, GetModuleHandle(NULL));
                    return 0;
                }
                /* Always on top */
                case ID_OPTIONS_ALWAYSONTOP:
                {
                    wnd.ontop = wnd.ontop ? false : true;
                    UI::Jdi.SetConfigBool(USER_ONTOP, wnd.ontop, USER_UI);

                    auto flags = (wnd.ontop ? MF_BYCOMMAND | MF_CHECKED : MF_BYCOMMAND | MF_UNCHECKED);
                    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_ALWAYSONTOP, flags);

                    SetAlwaysOnTop(hwnd, wnd.ontop);
                    return 0;
                }
                /* Refresh view */
                case ID_FILE_REFRESH:
                {
                    UpdateSelector();
                    return 0;
                }
                /* Disable the view */
                case ID_OPTIONS_VIEW_DISABLE:
                {
                    if (usel.active)
                    {
                        CloseSelector();
                        ResetStatusBar();
                        ModifySelectorControls(false);
                        usel.active = false;
                        UI::Jdi.SetConfigBool(USER_SELECTOR, false, USER_UI);
                    }
                    else
                    {
                        ModifySelectorControls(true);
                        usel.active = true;
                        UI::Jdi.SetConfigBool(USER_SELECTOR, true, USER_UI);
                        CreateSelector();
                    }

                    return 0;
                }
                /* Change icon size */
                case ID_OPTIONS_VIEW_SMALLICONS:
                {
                    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_UNCHECKED);
                    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_CHECKED);
                    SetSelectorIconSize(TRUE);
                    return 0;
                }
                case ID_OPTIONS_VIEW_LARGEICONS:
                {
                    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_UNCHECKED);
                    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_CHECKED);
                    SetSelectorIconSize(FALSE);
                    return 0;
                }
                /* Sort files */
                case ID_OPTIONS_VIEW_SORTBY_1:
                {
                    SortSelector(SELECTOR_SORT::Default);
                    SelectSort();

                    return 0;
                }
                case ID_OPTIONS_VIEW_SORTBY_2:
                {
                    SortSelector(SELECTOR_SORT::Filename);
                    SelectSort();

                    return 0;
                }
                case ID_OPTIONS_VIEW_SORTBY_3:
                {
                    SortSelector(SELECTOR_SORT::Title);
                    SelectSort();

                    return 0;
                }
                case ID_OPTIONS_VIEW_SORTBY_4:
                {
                    SortSelector(SELECTOR_SORT::Size);
                    SelectSort();

                    return 0;
                }
                case ID_OPTIONS_VIEW_SORTBY_5:
                {
                    SortSelector(SELECTOR_SORT::ID);
                    SelectSort();

                    return 0;
                }
                case ID_OPTIONS_VIEW_SORTBY_6:
                {
                    SortSelector(SELECTOR_SORT::Comment);
                    SelectSort();

                    return 0;
                }
                case ID_OPTIONS_VIEW_SORTBY_7:
                {
                    SortSelector(SELECTOR_SORT::Unsorted);
                    SelectSort();

                    return 0;
                }

                /* Edit file filter */
                case ID_OPTIONS_VIEW_FILEFILTER:
                {
                    EditFileFilter(hwnd);
                    return 0;
                }

                /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
                /* debug controls                                                               */

                // multiple instancies on/off
                case ID_RUN_ONCE:
                {
                    if (UI::Jdi.GetConfigBool(USER_RUNONCE, USER_UI))
                    {   /* Off */
                        CheckMenuItem(wnd.hMainMenu, ID_RUN_ONCE, MF_BYCOMMAND | MF_UNCHECKED);
                        UI::Jdi.SetConfigBool(USER_RUNONCE, false, USER_UI);
                    }
                    else
                    {   /* On */
                        CheckMenuItem(wnd.hMainMenu, ID_RUN_ONCE, MF_BYCOMMAND | MF_CHECKED);
                        UI::Jdi.SetConfigBool(USER_RUNONCE, true, USER_UI);
                    }

                    return 0;
                }
                // Open/close system-wide debugger
                case ID_DEBUG_CONSOLE:
                {
                    if (dspDebug)
                    {
                        CheckMenuItem(wnd.hMainMenu, ID_DSP_DEBUG, MF_BYCOMMAND | MF_UNCHECKED);
                        delete dspDebug;
                        dspDebug = nullptr;
                    }

                    if (gekkoDebug == nullptr)
                    {   // open
                        CheckMenuItem(wnd.hMainMenu, ID_DEBUG_CONSOLE, MF_BYCOMMAND | MF_CHECKED);
                        gekkoDebug = new Debug::GekkoDebug();
                        UI::Jdi.SetConfigBool(USER_DOLDEBUG, true, USER_UI);
                        SetStatusText(STATUS_ENUM::Progress, L"Debugger opened");
                    }
                    else
                    {   // close
                        CheckMenuItem(wnd.hMainMenu, ID_DEBUG_CONSOLE, MF_BYCOMMAND | MF_UNCHECKED);
                        delete gekkoDebug;
                        gekkoDebug = nullptr;
                        UI::Jdi.SetConfigBool(USER_DOLDEBUG, false, USER_UI);
                        SetStatusText(STATUS_ENUM::Progress, L"Debugger closed");
                    }
                    return 0;
                }
                // Open/close DSP Debug
                case ID_DSP_DEBUG:
                {
                    if (gekkoDebug)
                    {
                        CheckMenuItem(wnd.hMainMenu, ID_DEBUG_CONSOLE, MF_BYCOMMAND | MF_UNCHECKED);
                        delete gekkoDebug;
                        gekkoDebug = nullptr;
                    }

                    if (dspDebug == nullptr)
                    {
                        CheckMenuItem(wnd.hMainMenu, ID_DSP_DEBUG, MF_BYCOMMAND | MF_CHECKED);
                        dspDebug = new Debug::DspDebug();
                        SetStatusText(STATUS_ENUM::Progress, L"DSP Debugger opened");
                    }
                    else
                    {
                        CheckMenuItem(wnd.hMainMenu, ID_DSP_DEBUG, MF_BYCOMMAND | MF_UNCHECKED);
                        delete dspDebug;
                        dspDebug = nullptr;
                        SetStatusText(STATUS_ENUM::Progress, L"DSP Debugger closed");
                    }
                    return 0;
                }
                // Mount Dolphin SDK as DVD
                case ID_DEVELOPMENT_MOUNTSDK:
                {
                    std::wstring dolphinSdkDir = UI::FileOpenDialog(wnd.hMainWindow, UI::FileType::Directory);
                    if (!dolphinSdkDir.empty())
                    {
                        char cmd[0x200];
                        sprintf_s(cmd, sizeof(cmd), "MountSDK \"%s\"", Util::WstringToString(dolphinSdkDir).c_str());
                        UI::Jdi.ExecuteCommand(cmd);
                    }

                    return 0;
                }

                // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
                // options dialogs

                case ID_OPTIONS_CONTROLLERS_PORT1:
                    PADConfigure(0, wnd.hMainWindow);
                    return 0;
                case ID_OPTIONS_CONTROLLERS_PORT2:
                    PADConfigure(1, wnd.hMainWindow);
                    return 0;
                case ID_OPTIONS_CONTROLLERS_PORT3:
                    PADConfigure(2, wnd.hMainWindow);
                    return 0;
                case ID_OPTIONS_CONTROLLERS_PORT4:
                    PADConfigure(3, wnd.hMainWindow);
                    return 0;

                // configure memcard in slot A
                case ID_OPTIONS_MEMCARDS_SLOTA:
                    MemcardConfigure(0, hwnd);
                    return 0;

                // configure memcard in slot B
                case ID_OPTIONS_MEMCARDS_SLOTB:
                    MemcardConfigure(1, hwnd);
                    return 0;

                // configure fonts
                case ID_OPTIONS_BOOTROMFONT:
                    FontConfigure(hwnd);
                    break;

                case ID_HELP_ABOUT:
                    AboutDialog(hwnd);
                    return 0;
                }
                break;
        }

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        // file drag & drop operation

        case WM_DROPFILES:
        {
            wchar_t fileName[MAX_PATH] = { 0 };
            DragQueryFile((HDROP)wParam, 0, fileName, sizeof(fileName));
            DragFinish((HDROP)wParam);

            // extension filter
            if(_wcsicmp(L".dol", wcsrchr(fileName, L'.')) &&
               _wcsicmp(L".elf", wcsrchr(fileName, L'.')) &&
               _wcsicmp(L".bin", wcsrchr(fileName, L'.')) &&
               _wcsicmp(L".iso", wcsrchr(fileName, L'.')) &&
               _wcsicmp(L".gcm", wcsrchr(fileName, L'.')) ) break;

            name = fileName;
            goto loadFile;
        }
        return 0;

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        // resizing of main window

        case WM_SIZE:
        {
            // resize status bar window
            if(IsWindow(wnd.hStatusWindow))
            {
                RECT rm, rs;
                GetWindowRect(hwnd, &rm);
                GetWindowRect(wnd.hStatusWindow, &rs); 
                long sbh = (WORD)(rs.bottom - rs.top);
                MoveWindow(wnd.hStatusWindow, rm.left, rm.top, rm.right - rm.left, rm.bottom - sbh, TRUE);
            }

            // resize selector
            {
                RECT rc;
                GetClientRect(hwnd, &rc);
                ResizeSelector(
                    (WORD)(rc.right - rc.left),
                    (WORD)(rc.bottom - rc.top)
                );
            }

            return 0;
        }

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        // selector notification messages

        case WM_NOTIFY:
        {
            if(wParam == ID_SELECTOR)
            {
                NotifySelector((LPNMHDR)lParam);

                // sort rules may change, by clicking on column
                SelectSort();
            }
            return 0;
        }

        case WM_DRAWITEM:
        {
            if(wParam == ID_SELECTOR)
            {
                DrawSelectorItem((LPDRAWITEMSTRUCT)lParam);
            }
            return 0;
        }        
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// self-explanatory.. creates main window and all child windows.
// window size will be set to default 400x300.
HWND CreateMainWindow(HINSTANCE hInstance)
{
    WNDCLASS wc = { 0 };
    const wchar_t CLASS_NAME[] = L"GAMECUBECLASS";

    assert(wnd.hMainWindow == nullptr);

    wc.cbClsExtra    = wc.cbWndExtra = 0;
    wc.hbrBackground = (HBRUSH)GetStockObject(DKGRAY_BRUSH);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DOLWIN_ICON));
    wc.hInstance     = hInstance;
    wc.lpfnWndProc   = WindowProc;
    wc.lpszClassName = CLASS_NAME;
    wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MAIN_MENU);
    wc.style         = 0;

    ATOM classAtom = RegisterClass(&wc);
    assert(classAtom != 0);

    auto win_name = fmt::format(L"{:s} - {:s} ({:s})", APPNAME, APPDESC, Util::StringToWstring(UI::Jdi.GetVersion()));
    wnd.hMainWindow = CreateWindowEx(
        0,
        CLASS_NAME,
        win_name.c_str(),
        WIN_STYLE, 
        CW_USEDEFAULT, CW_USEDEFAULT,
        400, 300,
        NULL, NULL,
        hInstance, NULL);

    assert(wnd.hMainWindow);

    ShowWindow(wnd.hMainWindow, SW_NORMAL);
    UpdateWindow(wnd.hMainWindow);

    return wnd.hMainWindow;
}

// ---------------------------------------------------------------------------
// utilities

// change main window on-top state
void SetAlwaysOnTop(HWND hwnd, BOOL state)
{
    RECT rect;
    HWND ontop[] = { HWND_NOTOPMOST, HWND_TOPMOST };
    GetWindowRect(hwnd, &rect);
    SetWindowPos(
        hwnd, 
        ontop[state], 
        rect.left, rect.top, 
        rect.right - rect.left, rect.bottom - rect.top, 
        SWP_SHOWWINDOW
    );
    UpdateWindow(hwnd);
}

// center the child window into the parent window
void CenterChildWindow(HWND hParent, HWND hChild)
{
   if (IsWindow(hParent) && IsWindow(hChild))
   {
        RECT rp, rc;

        GetWindowRect(hParent, &rp);
        GetWindowRect(hChild, &rc);

        MoveWindow(hChild, 
            rp.left + (rp.right - rp.left - rc.right + rc.left) / 2,
            rp.top + (rp.bottom - rp.top - rc.bottom + rc.top) / 2,
            rc.right - rc.left, rc.bottom - rc.top, TRUE
        );
    }
}

void SetMenuItemText(HMENU hmenu, UINT id, const std::wstring & text)
{
    MENUITEMINFO info = { 0 };
       
    info.cbSize = sizeof(MENUITEMINFO);
    info.fMask  = MIIM_TYPE;
    info.fType  = MFT_STRING;
    info.dwTypeData = (LPTSTR)text.data();

    SetMenuItemInfo(hmenu, id, FALSE, &info);
}
