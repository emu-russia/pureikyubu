// Dolwin settings dialog (to configure user variables)
#include "dolphin.h"

/*/
    Settings control behaviours are : normal, sticky and locked. 
    Normal: Can be changed anytime, emulator state also modified with control
    modifying.
    Sticky: Can be changed anytime, but emulator state will be modified after
    game Reload.
    Locked: Cannot be changed while emu running. 
/*/

// all user variables (except memory cards vars) are placed in UserConfig.h

// parent window and instance
static HWND         hParentWnd, hChildDlg[4];
static HINSTANCE    hParentInst;
static BOOL         settingsLoaded[4];
static BOOL         needSelUpdate;

static char * tabs[] = 
{
    "Emulator",
    "GUI/Selector",
    "GCN Hardware",
    "GCN HLE"
};

static struct ConsoleVersion
{
    uint32_t ver;
    char*   info;
} consoleVersion[] = {
    { 0x00000001, "0x00000001: Retail 1" },
    { 0x00000002, "0x00000002: HW2 production board" },
    { 0x00000003, "0x00000003: The latest production board" },
    { 0x10000004, "0x10000004: 1st Devkit HW" },
    { 0x10000005, "0x10000005: 2nd Devkit HW" },
    { 0x10000006, "0x10000006: The latest Devkit HW" },
    { 0xffffffff, "0x%08X: User defined" }
};

static struct Tooltip
{
    int     page, id;
    char*   text;
} tooltip[] = {
    // Emulator
    {
        0, IDC_COUNTER_FACTOR,
        "Ghaghaghaghagha!\n"
        "My popup windows are working perfectly!"
    },
    {
        0, IDC_MMXFLAG,
        "Shows whenever MMX technology is present or not"
    },
    {
        0, IDC_SSEFLAG,
        "Shows whenever SSE technology is present or not"
    },
    // UserMenu
    // GCN Hardware
    // GCN High Level
    {
        3, IDC_HLE_MODE,
        "Mwhahah-ha-haaaa!"
    },
    {
        3, IDC_SIMULATE_APPLDR,
        "No-waaaaaay!"
    },

    // terminate
    { 0, NULL, NULL },
};

static char * int2str(int i)
{
    static char str[16];
    sprintf_s (str, sizeof(str), "%i", i);
    return str;
}

// ---------------------------------------------------------------------------

static void LoadSettings(int n)         // dialogs created
{
    HWND hDlg = hChildDlg[n];

    // Emulator
    if(n == 0)
    {
        CheckDlgButton(hDlg, IDC_ENSURE_WINDALL, BST_UNCHECKED);
        EnableWindow(GetDlgItem(hDlg, IDC_WINDALL), 0);

        SendDlgItemMessage(hDlg, IDC_CPU_CORE, CB_RESETCONTENT, 0, 0);
        SendDlgItemMessage(hDlg, IDC_CPU_CORE, CB_INSERTSTRING, -1, (LPARAM)(LPSTR)"Interpreter");
        SendDlgItemMessage(hDlg, IDC_CPU_CORE, CB_SETCURSEL, 0, 0);

        bool selected = GetConfigInt(USER_MMU, USER_MMU_DEFAULT);
        SendDlgItemMessage(hDlg, IDC_MEMORY_MODE, CB_RESETCONTENT, 0, 0);
        SendDlgItemMessage(hDlg, IDC_MEMORY_MODE, CB_INSERTSTRING, -1, (LPARAM)(LPSTR)"Simple translation");
        SendDlgItemMessage(hDlg, IDC_MEMORY_MODE, CB_INSERTSTRING, -1, (LPARAM)(LPSTR)"Advanced (Linux)");
        SendDlgItemMessage(hDlg, IDC_MEMORY_MODE, CB_SETCURSEL, selected, 0);

        int cf = GetConfigInt(USER_CPU_CF, USER_CPU_CF_DEFAULT);
        SetDlgItemText(hDlg, IDC_COUNTER_FACTOR, int2str(cf));
        int delay = GetConfigInt(USER_CPU_DELAY, USER_CPU_DELAY_DEFAULT);
        SetDlgItemText(hDlg, IDC_COUNTER_DELAY, int2str(delay));
        int bail = GetConfigInt(USER_CPU_TIME, USER_CPU_TIME_DEFAULT);
        SetDlgItemText(hDlg, IDC_BAILOUT, int2str(bail));

        char buf[256];
        sprintf_s(buf, sizeof(buf), "(Default: %i)", USER_CPU_CF_DEFAULT);
        SetDlgItemText(hDlg, IDC_CF_DEFAULT, buf);
        sprintf_s(buf, sizeof(buf), "(Default: %i)", USER_CPU_DELAY_DEFAULT);
        SetDlgItemText(hDlg, IDC_DELAY_DEFAULT, buf);
        sprintf_s(buf, sizeof(buf), "(Default: %i)", USER_CPU_TIME_DEFAULT);
        SetDlgItemText(hDlg, IDC_BAILOUT_DEFAULT, buf);

        settingsLoaded[0] = TRUE;
    }

    // GUI/Selector
    if(n == 1)
    {
        for(int i=0; i<usel.pathnum; i++)
        {
            SendDlgItemMessage( hDlg, IDC_PATHLIST, LB_ADDSTRING,
                                0, (LPARAM)usel.paths[i] );
        }

        needSelUpdate = FALSE;
        settingsLoaded[1] = TRUE;
    }

    // GCN Hardware
    if(n == 2)
    {
        uint32_t ver = GetConfigInt(USER_CONSOLE, USER_CONSOLE_DEFAULT);
        int i=0, selected = sizeof(consoleVersion) / 8 - 1;
        while(consoleVersion[i].ver != 0xffffffff)
        {
            if(consoleVersion[i].ver == ver)
            {
                selected = i;
                break;
            }
            i++;
        }
        if(consoleVersion[i].ver == 0xffffffff) selected = sizeof(consoleVersion)/8 - 1;
        i=0;
        SendDlgItemMessage(hDlg, IDC_CONSOLE_VER, CB_RESETCONTENT, 0, 0);
        do
        {
            SendDlgItemMessage(hDlg, IDC_CONSOLE_VER, CB_INSERTSTRING, -1, (LPARAM)(LPSTR)consoleVersion[i].info);
        } while(consoleVersion[++i].ver != 0xffffffff);
        if(selected == sizeof(consoleVersion)/8 - 1)
        {
            char buf[100];
            sprintf_s(buf, sizeof(buf), consoleVersion[selected].info, ver);
            SendDlgItemMessage(hDlg, IDC_CONSOLE_VER, CB_INSERTSTRING, -1, (LPARAM)(LPSTR)buf);
        }
        SendDlgItemMessage(hDlg, IDC_CONSOLE_VER, CB_SETCURSEL, selected, 0);

        SetDlgItemText(hDlg, IDC_BOOTROM_FILE, GetConfigString(USER_BOOTROM, USER_BOOTROM_DEFAULT));
        SetDlgItemText(hDlg, IDC_DSPDROM_FILE, GetConfigString(USER_DSP_DROM, USER_DSP_DROM_DEFAULT));
        SetDlgItemText(hDlg, IDC_DSPIROM_FILE, GetConfigString(USER_DSP_IROM, USER_DSP_IROM_DEFAULT));

        CheckDlgButton(hDlg, IDC_RTC, BST_UNCHECKED);
        BOOL flag = GetConfigInt(USER_RTC, USER_RTC_DEFAULT);
        if(flag) CheckDlgButton(hDlg, IDC_RTC, BST_CHECKED);
        CheckDlgButton(hDlg, IDC_GX_POLL, BST_UNCHECKED);
        fifo.gxpoll = GetConfigInt(USER_GX_POLL, USER_GX_POLL_DEFAULT);
        if(fifo.gxpoll) CheckDlgButton(hDlg, IDC_GX_POLL, BST_CHECKED);
        CheckDlgButton(hDlg, IDC_VI_STRETCH, BST_UNCHECKED);
        vi.stretch = GetConfigInt(USER_VI_STRETCH, USER_VI_STRETCH_DEFAULT);
        if(vi.stretch) CheckDlgButton(hDlg, IDC_VI_STRETCH, BST_CHECKED);

        settingsLoaded[2] = TRUE;
    }

    // GCN High Level
    if(n == 3)
    {
        CheckDlgButton(hDlg, IDC_MTXHLE, BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_DSP_FAKE, BST_UNCHECKED);
        BOOL flag = GetConfigInt(USER_HLE_MTX, USER_HLE_MTX_DEFAULT);
        if(flag) CheckDlgButton(hDlg, IDC_MTXHLE, BST_CHECKED);
        flag = GetConfigInt(USER_DSP_FAKE, USER_DSP_FAKE_DEFAULT);
        if(flag) CheckDlgButton(hDlg, IDC_DSP_FAKE, BST_CHECKED);

        settingsLoaded[3] = TRUE;
    }
}

static void SaveSettings()              // OK pressed
{
    int i;
    char buf[256] = { 0, };

    // Emulator
    if(settingsLoaded[0])
    {
        HWND hDlg = hChildDlg[0];
        int selected = (int)SendDlgItemMessage(hDlg, IDC_MEMORY_MODE, CB_GETCURSEL, 0, 0);
        SetConfigInt(USER_MMU, selected);

        GetDlgItemText(hDlg, IDC_COUNTER_FACTOR, buf, sizeof(buf));
        cpu.cf = atoi(buf); if(cpu.cf <= 0) cpu.cf = 1;
        SetConfigInt(USER_CPU_CF, cpu.cf);
        GetDlgItemText(hDlg, IDC_COUNTER_DELAY, buf, sizeof(buf));
        cpu.delay = atoi(buf); if(cpu.delay <= 0) cpu.delay = 1;
        SetConfigInt(USER_CPU_DELAY, cpu.delay);
        GetDlgItemText(hDlg, IDC_BAILOUT, buf, sizeof(buf));
        cpu.bailout = atoi(buf); if(cpu.bailout <= 0) cpu.bailout = 1;
        SetConfigInt(USER_CPU_TIME, cpu.bailout);

        if(emu.running)
        {
            // update view of CPU timing setup
            sprintf_s(buf, sizeof(buf), "%i - %i - %i", cpu.cf, cpu.delay, cpu.bailout);
            SetStatusText(STATUS_TIMING, buf);
        }
    }

    // GUI/Selector
    if(settingsLoaded[1])
    {
        HWND hDlg = hChildDlg[1];

        char text[1024];
        int max = SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_GETCOUNT, 0, 0);

        // delete all dirs
        for(i=0; i<usel.pathnum; i++)
        {
            if(usel.paths[i])
            {
                free(usel.paths[i]);
                usel.paths[i] = NULL;
            }
        }
        if(usel.paths)
        {
            free(usel.paths);
            usel.paths = NULL;
            usel.pathnum = 0;
        }
        SetConfigString(USER_PATH, "<EMPTY>");

        // add dirs again
        for(i=0; i<max; i++)
        {
            SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_GETTEXT, i, (LPARAM)text);
            AddSelectorPath(text);
        }

        // update selector layout, if PATH has changed
        if(needSelUpdate)
        {
            UpdateSelector();
            needSelUpdate = FALSE;
        }
    }

    // GCN Hardwre
    if(settingsLoaded[2])
    {
        HWND hDlg = hChildDlg[2];
        int selected = (int)SendDlgItemMessage(hDlg, IDC_CONSOLE_VER, CB_GETCURSEL, 0, 0);
        if(selected == sizeof(consoleVersion)/8 - 1)
        {
            SendDlgItemMessage(hDlg, IDC_CONSOLE_VER, CB_GETLBTEXT, selected, (LPARAM)buf);
            uint32_t ver = strtoul(buf, NULL, 0);
            SetConfigInt(USER_CONSOLE, ver);
        }
        else SetConfigInt(USER_CONSOLE, consoleVersion[selected].ver);

        GetDlgItemText(hDlg, IDC_BOOTROM_FILE, buf, sizeof(buf));
        SetConfigString(USER_BOOTROM, buf);
        GetDlgItemText(hDlg, IDC_DSPDROM_FILE, buf, sizeof(buf));
        SetConfigString(USER_DSP_DROM, buf);
        GetDlgItemText(hDlg, IDC_DSPIROM_FILE, buf, sizeof(buf));
        SetConfigString(USER_DSP_IROM, buf);

        BOOL flag = IsDlgButtonChecked(hDlg, IDC_RTC);
        SetConfigInt(USER_RTC, flag);
        fifo.gxpoll = IsDlgButtonChecked(hDlg, IDC_GX_POLL) & 1;
        SetConfigInt(USER_GX_POLL, fifo.gxpoll);
        vi.stretch = IsDlgButtonChecked(hDlg, IDC_VI_STRETCH) & 1;
        SetConfigInt(USER_VI_STRETCH, vi.stretch);
    }

    // GCN High Level
    if(settingsLoaded[3])
    {
        HWND hDlg = hChildDlg[3];
        BOOL flag = IsDlgButtonChecked(hDlg, IDC_MTXHLE);
        SetConfigInt(USER_HLE_MTX, flag);
        flag = IsDlgButtonChecked(hDlg, IDC_DSP_FAKE);
        SetConfigInt(USER_DSP_FAKE, flag);
    }
}

void ResetAllSettings()
{
}

// make sure path have ending '\\'
static void fix_path(char *path)
{
    size_t n = strlen(path);
    if(path[n-1] != '\\')
    {
        path[n]   = '\\';
        path[n+1] = 0;
    }
}

// remove all control symbols (below space)
static void fix_string(char *str)
{
    for(size_t i=0; i<strlen(str); i++)
    {
        if(str[i] < ' ') str[i] = ' ';
    }
}

// ---------------------------------------------------------------------------
// Emulator

static INT_PTR CALLBACK EmulatorSettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND propSheet;

    switch(message)
    {
        case WM_INITDIALOG:
            // seems propsheet callback is shit :)
            // this trick do the same job
            propSheet = GetParent(hDlg);
            CenterChildWindow(hParentWnd, propSheet);

            hChildDlg[0] = hDlg;
            LoadSettings(0);
            return TRUE;

        case WM_COMMAND:
            switch(wParam)
            {
                case IDC_WINDALL:
                {
                    ResetAllSettings();
                    DolwinReport(
                        "All Settings have been deleted.\n"
                        "Default values will be restored after first run."
                    );
                    for(int i=0; i<4; i++) LoadSettings(i);
                }
                break;
                
                case IDC_ENSURE_WINDALL:
                {
                    if(IsDlgButtonChecked(hDlg, IDC_ENSURE_WINDALL))
                        EnableWindow(GetDlgItem(hDlg, IDC_WINDALL), 1);
                    else
                        EnableWindow(GetDlgItem(hDlg, IDC_WINDALL), 0);
                }
                break;
            }
            break;

        case WM_NOTIFY:
            if(((NMHDR FAR *)lParam)->code == PSN_APPLY) SaveSettings();
            break;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// UserMenu

static INT_PTR CALLBACK UserMenuSettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    int i;
    int curSel, max;
    char * path, text[1024];

    switch(message)
    {
        case WM_INITDIALOG:
            hChildDlg[1] = hDlg;
            LoadSettings(1);
            return TRUE;

        case WM_COMMAND:
            switch(wParam)
            {
                case IDC_FILEFILTER:
                    EditFileFilter(hDlg);
                    break;
                case IDC_ADDPATH:
                    if((path = FileOpen(hDlg, FILE_TYPE_DIR)) != nullptr)
                    {
                        fix_path(path);

                        // check if already present
                        max = SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_GETCOUNT, 0, 0);
                        for(i=0; i<max; i++)
                        {
                            SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_GETTEXT, i, (LPARAM)text);
                            if(!_stricmp(path, text)) break;
                        }

                        // add new path
                        if(i == max)
                        {
                            SendDlgItemMessage( hDlg, IDC_PATHLIST, LB_ADDSTRING,
                                                0, (LPARAM)path );
                            needSelUpdate = TRUE;
                        }
                    }
                    break;
                case IDC_KILLPATH:
                    curSel = SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_GETCURSEL, 0, 0);
                    SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_DELETESTRING, (WPARAM)curSel, 0);
                    needSelUpdate = TRUE;
                    break;
            }
            break;
 
        case WM_NOTIFY:
            if(((NMHDR FAR *)lParam)->code == PSN_APPLY) SaveSettings();
            break;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// GCN Hardware

static INT_PTR CALLBACK HardwareSettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    char* file = nullptr;

    switch(message)
    {
        case WM_INITDIALOG:
            hChildDlg[2] = hDlg;
            LoadSettings(2);
            return TRUE;

        case WM_NOTIFY:
            if(((NMHDR FAR *)lParam)->code == PSN_APPLY) SaveSettings();
            break;

        case WM_COMMAND:
            switch (wParam)
            {
                case IDC_CHOOSE_BOOTROM:
                    file = FileOpen(wnd.hMainWindow, FILE_TYPE::FILE_TYPE_ALL);
                    if (file != nullptr)
                    {
                        SetDlgItemText(hDlg, IDC_BOOTROM_FILE, file);
                    }
                    else
                    {
                        SetDlgItemText(hDlg, IDC_BOOTROM_FILE, "");
                    }
                    break;
                case IDC_CHOOSE_DSPDROM:
                    file = FileOpen(wnd.hMainWindow, FILE_TYPE::FILE_TYPE_ALL);
                    if (file != nullptr)
                    {
                        SetDlgItemText(hDlg, IDC_DSPDROM_FILE, file);
                    }
                    else
                    {
                        SetDlgItemText(hDlg, IDC_DSPDROM_FILE, "");
                    }
                    break;
                case IDC_CHOOSE_DSPIROM:
                    file = FileOpen(wnd.hMainWindow, FILE_TYPE::FILE_TYPE_ALL);
                    if (file != nullptr)
                    {
                        SetDlgItemText(hDlg, IDC_DSPIROM_FILE, file);
                    }
                    else
                    {
                        SetDlgItemText(hDlg, IDC_DSPIROM_FILE, "");
                    }
                    break;
            }
            break;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// GCN High Level

static INT_PTR CALLBACK HighLevelSettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    char buf[256];

    switch(message)
    {
        case WM_INITDIALOG:
            hChildDlg[3] = hDlg;
            LoadSettings(3);
            return TRUE;

        case WM_NOTIFY:
            if(((NMHDR FAR *)lParam)->code == PSN_APPLY) SaveSettings();
            break;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------

void OpenSettingsDialog(HWND hParent, HINSTANCE hInst)
{
    hParentWnd  = hParent;
    hParentInst = hInst;

    PROPSHEETPAGE psp[4];
    PROPSHEETHEADER psh;

    // Emulator page
    psp[0].dwSize = sizeof(PROPSHEETPAGE);
    psp[0].dwFlags = PSP_USETITLE;
    psp[0].hInstance = hParentInst;
    psp[0].pszTemplate = MAKEINTRESOURCE(IDD_SETTINGS_EMU);
    psp[0].pfnDlgProc = EmulatorSettingsProc;
    psp[0].pszTitle = tabs[0];
    psp[0].lParam = 0;
    psp[0].pfnCallback = NULL;

    // UserMenu page
    psp[1].dwSize = sizeof(PROPSHEETPAGE);
    psp[1].dwFlags = PSP_USETITLE;
    psp[1].hInstance = hParentInst;
    psp[1].pszTemplate = MAKEINTRESOURCE(IDD_SETTINGS_GUI);
    psp[1].pfnDlgProc = UserMenuSettingsProc;
    psp[1].pszTitle = tabs[1];
    psp[1].lParam = 0;
    psp[1].pfnCallback = NULL;
    settingsLoaded[0] = FALSE;

    // Hardware page
    psp[2].dwSize = sizeof(PROPSHEETPAGE);
    psp[2].dwFlags = PSP_USETITLE;
    psp[2].hInstance = hParentInst;
    psp[2].pszTemplate = MAKEINTRESOURCE(IDD_SETTINGS_HW);
    psp[2].pfnDlgProc = HardwareSettingsProc;
    psp[2].pszTitle = tabs[2];
    psp[2].lParam = 0;
    psp[2].pfnCallback = NULL;
    settingsLoaded[1] = FALSE;

    // High Level page
    psp[3].dwSize = sizeof(PROPSHEETPAGE);
    psp[3].dwFlags = PSP_USETITLE;
    psp[3].hInstance = hParentInst;
    psp[3].pszTemplate = MAKEINTRESOURCE(IDD_SETTINGS_HLE);
    psp[3].pfnDlgProc = HighLevelSettingsProc;
    psp[3].pszTitle = tabs[3];
    psp[3].lParam = 0;
    psp[3].pfnCallback = NULL;
    settingsLoaded[2] = FALSE;

    // property sheet
    psh.dwSize = sizeof(PROPSHEETHEADER);
    psh.dwFlags = PSH_USEHICON | /*PSH_PROPTITLE |*/ PSH_NOAPPLYNOW | PSH_PROPSHEETPAGE;
    psh.hwndParent = hParentWnd;
    psh.hInstance = hParentInst;
    psh.hIcon = LoadIcon(hParentInst, MAKEINTRESOURCE(IDI_DOLWIN_ICON));
    psh.pszCaption = "Configure " APPNAME;
    psh.nPages = sizeof(psp) / sizeof(PROPSHEETPAGE);
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE)&psp;
    psh.pfnCallback = NULL;
    settingsLoaded[3] = FALSE;

    PropertySheet(&psh);    // blocking call
}



/* ---------------------------------------------------------------------------
    Misc config section.
--------------------------------------------------------------------------- */




// ---------------------------------------------------------------------------
// edit file information dialog

// dialog procedure
static INT_PTR CALLBACK EditFileProc(
    HWND    hwndDlg,    // handle to dialog box
    UINT    uMsg,       // message
    WPARAM  wParam,     // first message parameter
    LPARAM  lParam      // second message parameter
)
{
    switch(uMsg)
    {
        // prepare dialog
        case WM_INITDIALOG:
        {
            // set dialog appearance
            ShowWindow(hwndDlg, SW_NORMAL);
            SendMessage(hwndDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_DOLWIN_ICON)));

            // fill by default info
            char drive[_MAX_DRIVE + 1], dir[_MAX_DIR], name[_MAX_PATH], ext[_MAX_EXT];
            char path[MAX_PATH], fullname[MAX_PATH];
            _splitpath(usel.selected->name, drive, dir, name, ext);
            sprintf_s(path, sizeof(path), "%s%s", drive, dir);
            sprintf_s(fullname, sizeof(fullname), "%s%s", name, ext);
            SetDlgItemText(hwndDlg, IDC_FILE_INFO_TITLE, usel.selected->title);
            SetDlgItemText(hwndDlg, IDC_FILE_INFO_COMMENT, usel.selected->comment);
            SetDlgItemText(hwndDlg, IDC_FILE_INFO_FILENAME, fullname);
            SetDlgItemText(hwndDlg, IDC_FILE_INFO_PATH, path);
            return TRUE;
        }

        // close button
        case WM_CLOSE:
        {
            EndDialog(hwndDlg, 0);
            return TRUE;
        }

        // dialog controls
        case WM_COMMAND:
        {
            if(wParam == IDCANCEL)
            {
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            if(wParam == IDOK)
            {
                // save information and update selector
                GetDlgItemText( hwndDlg, IDC_FILE_INFO_TITLE, 
                                usel.selected->title, MAX_TITLE);
                GetDlgItemText( hwndDlg, IDC_FILE_INFO_COMMENT, 
                                usel.selected->comment, MAX_COMMENT);
                if(usel.selected->type == SELECTOR_FILE_DVD)
                {
                    SetGameInfo(
                        usel.selected->id,
                        usel.selected->title,
                        usel.selected->comment
                    );
                }
                else
                {
                    char drive[_MAX_DRIVE + 1], dir[_MAX_DIR], name[_MAX_PATH], ext[_MAX_EXT];
                    _splitpath(usel.selected->name, drive, dir, name, ext);
                    SetGameInfo(
                        name,
                        usel.selected->title,
                        usel.selected->comment
                    );
                }
                EndDialog(hwndDlg, 0);
                int sel = SelectorGetSelected();
                UpdateSelector();
                SelectorSetSelected(sel);
                return TRUE;
            }
            switch(LOWORD(wParam))
            {
                // restore default information
                case IDC_FILE_INFO_DEFAULT:
                {
                    if(usel.selected->type == SELECTOR_FILE_DVD)
                    {
                        DVDBanner2 * bnr = (DVDBanner2 *)DVDLoadBanner(usel.selected->name);
                        fix_string((char *)bnr->comments[0].longTitle);
                        fix_string((char *)bnr->comments[0].comment);
                        SetDlgItemText(hwndDlg, IDC_FILE_INFO_TITLE, (char *)bnr->comments[0].longTitle);
                        SetDlgItemText(hwndDlg, IDC_FILE_INFO_COMMENT, (char *)bnr->comments[0].comment);
                        free(bnr);
                    }
                    else
                    {
                        char drive[_MAX_DRIVE + 1], dir[_MAX_DIR], name[_MAX_PATH], ext[_MAX_EXT];
                        _splitpath(usel.selected->name, drive, dir, name, ext);
                        SetDlgItemText(hwndDlg, IDC_FILE_INFO_TITLE, name);
                        SetDlgItemText(hwndDlg, IDC_FILE_INFO_COMMENT, "");
                    }
                    return FALSE;
                }
            }
        }
    }

    return FALSE;
}

void EditFileInformation(HWND hwnd)
{
    // opened ?
    if(!usel.opened) return;

    // choose the first selected item
    int item = ListView_GetNextItem(usel.hSelectorWindow, -1, LVNI_SELECTED);
    if(item == -1) return;
    else usel.selected = &usel.files[item];

    DialogBox(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_EDIT_FILE_INFO),
        hwnd,
        EditFileProc);
}

// ---------------------------------------------------------------------------
// file filter dialog

static void filter_string(HWND hDlg, uint32_t filter)
{
    char buf[64], *ptr = buf;
    char * mask[] = { "*.dol", "*.elf", "*.gcm", "*.iso" };

    filter = MEMSwap(filter);

    for(int i=0; i<4; i++)
    {
        if(filter & 0xff) ptr += sprintf(ptr, "%s;", mask[i]);
        filter >>= 8;
    }
    ptr[-1] = 0;

    SetDlgItemText(hDlg, IDC_FILE_FILTER, buf);
}

static void check_filter(HWND hDlg, uint32_t filter)
{
    // DOL
    if(filter & 0xff000000) CheckDlgButton(hDlg, IDC_DOL_FILTER, BST_CHECKED);
    else CheckDlgButton(hDlg, IDC_DOL_FILTER, BST_UNCHECKED);
    // ELF
    if(filter & 0x00ff0000) CheckDlgButton(hDlg, IDC_ELF_FILTER, BST_CHECKED);
    else CheckDlgButton(hDlg, IDC_ELF_FILTER, BST_UNCHECKED);
    // GCM
    if(filter & 0x0000ff00) CheckDlgButton(hDlg, IDC_GCM_FILTER, BST_CHECKED);
    else CheckDlgButton(hDlg, IDC_GCM_FILTER, BST_UNCHECKED);
    // ISO
    if(filter & 0x000000ff) CheckDlgButton(hDlg, IDC_GMP_FILTER, BST_CHECKED);
    else CheckDlgButton(hDlg, IDC_GMP_FILTER, BST_UNCHECKED);
}

// dialog procedure
static INT_PTR CALLBACK FileFilterProc(
    HWND    hwndDlg,    // handle to dialog box
    UINT    uMsg,       // message
    WPARAM  wParam,     // first message parameter
    LPARAM  lParam      // second message parameter
)
{
    switch(uMsg)
    {
        // prepare dialog
        case WM_INITDIALOG:
        {
            CenterChildWindow(GetParent(hwndDlg), hwndDlg);

            // set dialog appearance
            ShowWindow(hwndDlg, SW_NORMAL);
            SendMessage(hwndDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_DOLWIN_ICON)));

            // check DVD accesibility. disable DVD image
            // extensions if DVD plugin is not loaded
            EnableWindow(GetDlgItem(hwndDlg, IDC_GCM_FILTER), DVDSetCurrent != NULL);
            EnableWindow(GetDlgItem(hwndDlg, IDC_GMP_FILTER), DVDSetCurrent != NULL);

            // fill by default info
            usel.filter = GetConfigInt(USER_FILTER, USER_FILTER_DEFAULT);
            filter_string(hwndDlg, usel.filter);
            check_filter(hwndDlg, usel.filter);
            return TRUE;
        }

        // close button
        case WM_CLOSE:
        {
            EndDialog(hwndDlg, 0);
            return TRUE;
        }

        // dialog controls
        case WM_COMMAND:
        {
            if(wParam == IDOK)
            {
                EndDialog(hwndDlg, 0);

                // save information and update selector (if filter was changed)
                if((uint32_t)GetConfigInt(USER_FILTER, USER_FILTER_DEFAULT) != usel.filter)
                {
                    SetConfigInt(USER_FILTER, usel.filter);
                    UpdateSelector();
                }
                return TRUE;
            }
            switch(LOWORD(wParam))
            {
                case IDC_DOL_FILTER:                    // DOL
                    usel.filter ^= 0xff000000;
                    filter_string(hwndDlg, usel.filter);
                    check_filter(hwndDlg, usel.filter);
                    return TRUE;
                case IDC_ELF_FILTER:                    // ELF
                    usel.filter ^= 0x00ff0000;
                    filter_string(hwndDlg, usel.filter);
                    check_filter(hwndDlg, usel.filter);
                    return TRUE;
                case IDC_GCM_FILTER:                    // GCM
                    usel.filter ^= 0x0000ff00;
                    filter_string(hwndDlg, usel.filter);
                    check_filter(hwndDlg, usel.filter);
                    return TRUE;
                case IDC_GMP_FILTER:                    // ISO
                    usel.filter ^= 0x000000ff;
                    filter_string(hwndDlg, usel.filter);
                    check_filter(hwndDlg, usel.filter);
                    return TRUE;
            }
        }
    }

    return FALSE;
}

void EditFileFilter(HWND hwnd)
{
    DialogBox(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_FILE_FILTER),
        hwnd,
        FileFilterProc);
}
