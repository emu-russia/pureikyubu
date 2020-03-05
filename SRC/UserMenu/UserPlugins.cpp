// plugin select dialog
#include "dolphin.h"

static int GXSelectedPlug, AXSelectedPlug, PADSelectedPlug, DVDSelectedPlug;
static char *PlugList[256];

// ---------------------------------------------------------------------------

static void Register(HINSTANCE hPlug)
{
    // prepare plugin data structure
    memset(&plug, 0, sizeof(PluginData));
    plug.ram = RAM;
    plug.display = (void *)&wnd.hMainWindow;

    RegisterPlugin = (REGISTERPLUGIN)GetProcAddress(hPlug, "RegisterPlugin");
    if((FARPROC)RegisterPlugin != NULL)
    {
        RegisterPlugin(&plug);
    }
}

// ---------------------------------------------------------------------------

//
// configure GX-plugin
//

static void AddGXPlugin(HWND hwndDlg, char *name)
{
    char buf[256];
    HMODULE hPlug;

    hPlug = LoadLibrary(name);
    if(hPlug == NULL) return;

    //
    // check plugin type
    //

    RegisterPlugin = (REGISTERPLUGIN)GetProcAddress(hPlug, "RegisterPlugin");
    if((FARPROC)RegisterPlugin == NULL)
    {
        FreeLibrary(hPlug);
        return;
    }

    RegisterPlugin(&plug);
    sprintf(buf, "%s", plug.version);
    FreeLibrary(hPlug);
    if(!IS_DOL_PLUG_GX(plug.type)) return;

    //
    // add combo box
    //

    SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_INSERTSTRING, -1, (LPARAM)(LPSTR)buf);

    PlugList[GXSelectedPlug] = (char *)malloc(strlen(name) + 1);
    strcpy(PlugList[GXSelectedPlug++], name);
}

static void EnumGXPlugins(HWND hwndDlg)
{
    char PluginPath[256];
    WIN32_FIND_DATA fd;
    HANDLE hfff;
    int i, selected = -1;

    for(i=0; i<256; i++)
    {
        if(PlugList[i])
        {
            free(PlugList[i]);
            PlugList[i] = NULL;
        }
    }

    GXSelectedPlug = 0;

    SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_RESETCONTENT, 0, 0);

    sprintf(PluginPath, ".\\Plugins\\*.dll");
    hfff = FindFirstFile(PluginPath, &fd);

    if(hfff == INVALID_HANDLE_VALUE) return;
    else
    {
        sprintf(PluginPath, ".\\Plugins\\%s", fd.cFileName);
        AddGXPlugin(hwndDlg, PluginPath);

        if(!strcmp(
            strrchr(GetConfigString(USER_GX, USER_GX_DEFAULT), '\\') + 1,
            strrchr(PluginPath, '\\') + 1 ))
        {
            selected = GXSelectedPlug;
        }
    }

    while(FindNextFile(hfff, &fd))
    {
        sprintf(PluginPath, ".\\Plugins\\%s", fd.cFileName);
        AddGXPlugin(hwndDlg, PluginPath);

        if(!strcmp(
            strrchr(GetConfigString(USER_GX, USER_GX_DEFAULT), '\\') + 1,
            strrchr(PluginPath, '\\') + 1 ))
        {
            selected = GXSelectedPlug;
        }
    }

    if(selected >= 0)
    {
        SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_SETCURSEL, selected - 1, 0);
    }
    else
    {
        SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_SETCURSEL, 0, 0);
        GXSelectedPlug = -1;
    }
}

static INT_PTR CALLBACK ConfigureGraphicsProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    int i, selected;

    switch(uMsg)
    {
        case WM_INITDIALOG:
            SetWindowText(hwndDlg, "Select Graphics Plugin");
            GetWindowRect(wnd.hMainWindow, &rc);
            SetWindowPos(hwndDlg, HWND_TOP, rc.left + 100, rc.top  + 100, 0, 0, SWP_NOSIZE); 
            SendMessage(hwndDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_DOLWIN_ICON)));
            ShowWindow(GetDlgItem(hwndDlg, IDC_PLUGIN_PADNUM), SW_HIDE);
            EnumGXPlugins(hwndDlg);
            selected = (int)SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);
            if(selected >= 0)
            {
                SetDlgItemText(hwndDlg, IDC_PLUGIN_PATH, PlugList[selected]);
            }
            return TRUE;

        case WM_CLOSE:
            for(i=0; i<256; i++)
            {
                if(PlugList[i])
                {
                    free(PlugList[i]);
                    PlugList[i] = NULL;
                }
            }
            EndDialog(hwndDlg, 0);
            return TRUE;

        case WM_COMMAND:
            if(wParam == IDCANCEL)
            {
                for(i=0; i<256; i++)
                {
                    if(PlugList[i])
                    {
                        free(PlugList[i]);
                        PlugList[i] = NULL;
                    }
                }
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            if(wParam == IDOK)
            {
                selected = (int)SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);

                SetConfigString(USER_GX, PlugList[selected]);

                // reinit plugin system
                PSShutdown();
                PSInit();

                for(i=0; i<256; i++)
                {
                    if(PlugList[i])
                    {
                        free(PlugList[i]);
                        PlugList[i] = NULL;
                    }
                }
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            switch(LOWORD(wParam))
            {
                case IDC_PLUGIN_ABOUT:
                    {
                        HMODULE hPlug;
                        FARPROC old = (FARPROC)GXAbout;
                     
                        selected = (int)SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);

                        if(hPlug = LoadLibrary(PlugList[selected]))
                        {
                            Register(hPlug);
                            GXAbout = (GXABOUT)GetProcAddress(hPlug, "GXAbout");
                            if(GXAbout) GXAbout();
                            FreeLibrary(hPlug);
                            GXAbout = (GXABOUT)old;
                        }
                    }
                    return FALSE;

                case IDC_PLUGIN_CONFIG:
                    {
                        HMODULE hPlug;
                        FARPROC old = (FARPROC)GXConfigure;
                     
                        selected = (int)SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);

                        if(hPlug = LoadLibrary(PlugList[selected]))
                        {
                            Register(hPlug);
                            GXConfigure = (GXCONFIGURE)GetProcAddress(hPlug, "GXConfigure");
                            if(GXConfigure) GXConfigure();
                            FreeLibrary(hPlug);
                            GXConfigure = (GXCONFIGURE)old;
                        }
                    }
                    return FALSE;
            }
            switch(HIWORD(wParam))
            {
                case CBN_SELCHANGE:
                    GXSelectedPlug = (int)SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);
                    if(GXSelectedPlug >= 0)
                    {
                        SetDlgItemText(hwndDlg, IDC_PLUGIN_PATH, PlugList[GXSelectedPlug]);
                    }
                    return FALSE;
            }
            break;

        default:
            return FALSE;
    }
    return FALSE;
}

void ConfigureGraphicsDialog()
{
    DialogBox(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_SELECT_PLUGIN),
        wnd.hMainWindow,
        ConfigureGraphicsProc);
}

// ---------------------------------------------------------------------------

//
// configure audio-plugin
//

static void AddAXPlugin(HWND hwndDlg, char *name)
{
    char buf[256];
    HMODULE hPlug;

    hPlug = LoadLibrary(name);
    if(hPlug == NULL) return;

    //
    // check plugin type
    //

    RegisterPlugin = (REGISTERPLUGIN)GetProcAddress(hPlug, "RegisterPlugin");
    if((FARPROC)RegisterPlugin == NULL)
    {
        FreeLibrary(hPlug);
        return;
    }

    RegisterPlugin(&plug);
    sprintf(buf, "%s", plug.version);
    FreeLibrary(hPlug);
    if(!IS_DOL_PLUG_AX(plug.type)) return;

    //
    // add combo box
    //

    SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_INSERTSTRING, -1, (LPARAM)(LPSTR)buf);

    PlugList[AXSelectedPlug] = (char *)malloc(strlen(name) + 1);
    strcpy(PlugList[AXSelectedPlug++], name);
}

static void EnumAXPlugins(HWND hwndDlg)
{
    char PluginPath[256];
    WIN32_FIND_DATA fd;
    HANDLE hfff;
    int i, selected = -1;

    for(i=0; i<256; i++)
    {
        if(PlugList[i])
        {
            free(PlugList[i]);
            PlugList[i] = NULL;
        }
    }

    AXSelectedPlug = 0;

    SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_RESETCONTENT, 0, 0);

    sprintf(PluginPath, ".\\Plugins\\*.dll");
    hfff = FindFirstFile(PluginPath, &fd);

    if(hfff == INVALID_HANDLE_VALUE) return;
    else
    {
        sprintf(PluginPath, ".\\Plugins\\%s", fd.cFileName);
        AddAXPlugin(hwndDlg, PluginPath);

        if(!strcmp(
            strrchr(GetConfigString(USER_AX, USER_AX_DEFAULT), '\\') + 1,
            strrchr(PluginPath, '\\') + 1 ))
        {
            selected = AXSelectedPlug;
        }
    }

    while(FindNextFile(hfff, &fd))
    {
        sprintf(PluginPath, ".\\Plugins\\%s", fd.cFileName);
        AddAXPlugin(hwndDlg, PluginPath);

        if(!strcmp(
            strrchr(GetConfigString(USER_AX, USER_AX_DEFAULT), '\\') + 1,
            strrchr(PluginPath, '\\') + 1 ))
        {
            selected = AXSelectedPlug;
        }
    }

    if(selected >= 0)
    {
        SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_SETCURSEL, selected - 1, 0);
    }
    else
    {
        SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_SETCURSEL, 0, 0);
        AXSelectedPlug = -1;
    }
}

static INT_PTR CALLBACK ConfigureAudioProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    int i, selected;

    switch(uMsg)
    {
        case WM_INITDIALOG:
            SetWindowText(hwndDlg, "Select Audio Plugin");
            GetWindowRect(wnd.hMainWindow, &rc);
            SetWindowPos(hwndDlg, HWND_TOP, rc.left + 100, rc.top  + 100, 0, 0, SWP_NOSIZE); 
            SendMessage(hwndDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_DOLWIN_ICON)));
            ShowWindow(GetDlgItem(hwndDlg, IDC_PLUGIN_PADNUM), SW_HIDE);
            EnumAXPlugins(hwndDlg);
            selected = (int)SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);
            if(selected >= 0)
            {
                SetDlgItemText(hwndDlg, IDC_PLUGIN_PATH, PlugList[selected]);
            }
            return TRUE;

        case WM_CLOSE:
            for(i=0; i<256; i++)
            {
                if(PlugList[i])
                {
                    free(PlugList[i]);
                    PlugList[i] = NULL;
                }
            }
            EndDialog(hwndDlg, 0);
            return TRUE;

        case WM_COMMAND:
            if(wParam == IDCANCEL)
            {
                for(i=0; i<256; i++)
                {
                    if(PlugList[i])
                    {
                        free(PlugList[i]);
                        PlugList[i] = NULL;
                    }
                }
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            if(wParam == IDOK)
            {
                selected = (int)SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);

                SetConfigString(USER_AX, PlugList[selected]);

                // reinit plugin system
                PSShutdown();
                PSInit();

                for(i=0; i<256; i++)
                {
                    if(PlugList[i])
                    {
                        free(PlugList[i]);
                        PlugList[i] = NULL;
                    }
                }
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            switch(LOWORD(wParam))
            {
                case IDC_PLUGIN_ABOUT:
                    {
                        HMODULE hPlug;
                        FARPROC old = (FARPROC)AXAbout;
                     
                        selected = (int)SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);
                        
                        if(hPlug = LoadLibrary(PlugList[selected]))
                        {
                            Register(hPlug);
                            AXAbout = (AXABOUT)GetProcAddress(hPlug, "AXAbout");
                            if(AXAbout) AXAbout();
                            FreeLibrary(hPlug);
                            AXAbout = (AXABOUT)old;
                        }
                    }
                    return FALSE;

                case IDC_PLUGIN_CONFIG:
                    {
                        HMODULE hPlug;
                        FARPROC old = (FARPROC)AXConfigure;
                    
                        selected = (int)SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);

                        if(hPlug = LoadLibrary(PlugList[selected]))
                        {
                            Register(hPlug);
                            AXConfigure = (AXCONFIGURE)GetProcAddress(hPlug, "AXConfigure");
                            if(AXConfigure) AXConfigure();
                            FreeLibrary(hPlug);
                            AXConfigure = (AXCONFIGURE)old;
                        }
                    }
                    return FALSE;
            }
            switch(HIWORD(wParam))
            {
                case CBN_SELCHANGE:
                    AXSelectedPlug = (int)SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);
                    if(AXSelectedPlug >= 0)
                    {
                        SetDlgItemText(hwndDlg, IDC_PLUGIN_PATH, PlugList[AXSelectedPlug]);
                    }
                    return FALSE;
            }
            break;

        default:
            return FALSE;
    }
    return FALSE;
}

void ConfigureAudioDialog()
{
    DialogBox(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_SELECT_PLUGIN),
        wnd.hMainWindow,
        ConfigureAudioProc);
}

// ---------------------------------------------------------------------------

//
// configure PAD-plugin
//

static void AddPADPlugin(HWND hwndDlg, char *name)
{
    char buf[256];
    HMODULE hPlug;

    hPlug = LoadLibrary(name);
    if(hPlug == NULL) return;

    //
    // check plugin type
    //

    RegisterPlugin = (REGISTERPLUGIN)GetProcAddress(hPlug, "RegisterPlugin");
    if((FARPROC)RegisterPlugin == NULL)
    {
        FreeLibrary(hPlug);
        return;
    }

    RegisterPlugin(&plug);
    sprintf(buf, "%s", plug.version);
    FreeLibrary(hPlug);
    if(!IS_DOL_PLUG_PAD(plug.type)) return;

    //
    // add combo box
    //

    SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_INSERTSTRING, -1, (LPARAM)(LPSTR)buf);

    PlugList[PADSelectedPlug] = (char *)malloc(strlen(name) + 1);
    strcpy(PlugList[PADSelectedPlug++], name);
}

static void EnumPADPlugins(HWND hwndDlg)
{
    char PluginPath[256];
    WIN32_FIND_DATA fd;
    HANDLE hfff;
    int i, selected = -1;

    for(i=0; i<256; i++)
    {
        if(PlugList[i])
        {
            free(PlugList[i]);
            PlugList[i] = NULL;
        }
    }

    PADSelectedPlug = 0;

    SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_RESETCONTENT, 0, 0);

    sprintf(PluginPath, ".\\Plugins\\*.dll");
    hfff = FindFirstFile(PluginPath, &fd);

    if(hfff == INVALID_HANDLE_VALUE) return;
    else
    {
        sprintf(PluginPath, ".\\Plugins\\%s", fd.cFileName);
        AddPADPlugin(hwndDlg, PluginPath);

        if(!strcmp(
            strrchr(GetConfigString(USER_PAD, USER_PAD_DEFAULT), '\\') + 1,
            strrchr(PluginPath, '\\') + 1 ))
        {
            selected = PADSelectedPlug;
        }
    }

    while(FindNextFile(hfff, &fd))
    {
        sprintf(PluginPath, ".\\Plugins\\%s", fd.cFileName);
        AddPADPlugin(hwndDlg, PluginPath);

        if(!strcmp(
            strrchr(GetConfigString(USER_PAD, USER_PAD_DEFAULT), '\\') + 1,
            strrchr(PluginPath, '\\') + 1 ))
        {
            selected = PADSelectedPlug;
        }
    }

    if(selected >= 0)
    {
        SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_SETCURSEL, selected - 1, 0);
    }
    else
    {
        SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_SETCURSEL, 0, 0);
        PADSelectedPlug = -1;
    }
}

static void FillPADSelectionCombo(HWND hwndDlg)
{
    SendMessage(GetDlgItem(hwndDlg, IDC_PLUGIN_PADNUM), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"1st only");
    SendMessage(GetDlgItem(hwndDlg, IDC_PLUGIN_PADNUM), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"2nd only");
    SendMessage(GetDlgItem(hwndDlg, IDC_PLUGIN_PADNUM), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"3rd only");
    SendMessage(GetDlgItem(hwndDlg, IDC_PLUGIN_PADNUM), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"4th only");
    SendMessage(GetDlgItem(hwndDlg, IDC_PLUGIN_PADNUM), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"all pads");
    SendMessage(GetDlgItem(hwndDlg, IDC_PLUGIN_PADNUM), CB_SETCURSEL, 0, 0);
}

static INT_PTR CALLBACK ConfigureInputProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    int i, selected;

    switch(uMsg)
    {
        case WM_INITDIALOG:
            SetWindowText(hwndDlg, "Select Input Plugin");
            GetWindowRect(wnd.hMainWindow, &rc);
            SetWindowPos(hwndDlg, HWND_TOP, rc.left + 100, rc.top  + 100, 0, 0, SWP_NOSIZE); 
            SendMessage(hwndDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_DOLWIN_ICON)));
            ShowWindow(GetDlgItem(hwndDlg, IDC_PLUGIN_PADNUM), SW_SHOW);
            FillPADSelectionCombo(hwndDlg);
            EnumPADPlugins(hwndDlg);
            selected = (int)SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);
            if(selected >= 0)
            {
                SetDlgItemText(hwndDlg, IDC_PLUGIN_PATH, PlugList[selected]);
            }
            return TRUE;

        case WM_CLOSE:
            for(i=0; i<256; i++)
            {
                if(PlugList[i])
                {
                    free(PlugList[i]);
                    PlugList[i] = NULL;
                }
            }
            EndDialog(hwndDlg, 0);
            return TRUE;

        case WM_COMMAND:
            if(wParam == IDCANCEL)
            {
                for(i=0; i<256; i++)
                {
                    if(PlugList[i])
                    {
                        free(PlugList[i]);
                        PlugList[i] = NULL;
                    }
                }
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            if(wParam == IDOK)
            {
                selected = SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);

                SetConfigString(USER_PAD, PlugList[selected]);

                // reinit plugin system
                PSShutdown();
                PSInit();

                for(i=0; i<256; i++)
                {
                    if(PlugList[i])
                    {
                        free(PlugList[i]);
                        PlugList[i] = NULL;
                    }
                }
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            switch(LOWORD(wParam))
            {
                case IDC_PLUGIN_ABOUT:
                    {
                        HMODULE hPlug;
                        FARPROC old = (FARPROC)PADAbout;
                     
                        selected = SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);

                        if(hPlug = LoadLibrary(PlugList[selected]))
                        {
                            Register(hPlug);
                            PADAbout = (PADABOUT)GetProcAddress(hPlug, "PADAbout");
                            if(PADAbout) PADAbout();
                            FreeLibrary(hPlug);
                            PADAbout = (PADABOUT)old;
                        }
                    }
                    return FALSE;

                case IDC_PLUGIN_CONFIG:
                    {
                        HMODULE hPlug;
                        FARPROC old = (FARPROC)PADConfigure;
                     
                        selected = SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);
                        long padnum = SendDlgItemMessage(hwndDlg, IDC_PLUGIN_PADNUM, CB_GETCURSEL, 0, 0);

                        if(hPlug = LoadLibrary(PlugList[selected]))
                        {
                            Register(hPlug);
                            PADConfigure = (PADCONFIGURE)GetProcAddress(hPlug, "PADConfigure");
                            if(PADConfigure)
                            {
                                if(padnum == 4) // all pads
                                {
                                    PADConfigure(0);
                                    PADConfigure(1);
                                    PADConfigure(2);
                                    PADConfigure(3);
                                }
                                else if(padnum == 0) PADConfigure(0);
                                else if(padnum == 1) PADConfigure(1);
                                else if(padnum == 2) PADConfigure(2);
                                else if(padnum == 3) PADConfigure(3);
                            }
                            FreeLibrary(hPlug);
                            PADConfigure = (PADCONFIGURE)old;
                        }
                    }
                    return FALSE;
            }
            switch(HIWORD(wParam))
            {
                case CBN_SELCHANGE:
                    PADSelectedPlug = SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);
                    if(PADSelectedPlug >= 0)
                    {
                        SetDlgItemText(hwndDlg, IDC_PLUGIN_PATH, PlugList[PADSelectedPlug]);
                    }
                    return FALSE;
            }
            break;

        default:
            return FALSE;
    }
    return FALSE;
}

void ConfigureInputDialog()
{
    DialogBox(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_SELECT_PLUGIN),
        wnd.hMainWindow,
        ConfigureInputProc);
}

// ---------------------------------------------------------------------------

//
// configure DVD-plugin
//

static void AddDVDPlugin(HWND hwndDlg, char *name)
{
    char buf[256];
    HMODULE hPlug;

    hPlug = LoadLibrary(name);
    if(hPlug == NULL) return;

    //
    // check plugin type
    //

    RegisterPlugin = (REGISTERPLUGIN)GetProcAddress(hPlug, "RegisterPlugin");
    if((FARPROC)RegisterPlugin == NULL)
    {
        FreeLibrary(hPlug);
        return;
    }

    RegisterPlugin(&plug);
    sprintf(buf, "%s", plug.version);
    FreeLibrary(hPlug);
    if(!IS_DOL_PLUG_DVD(plug.type)) return;

    //
    // add combo box
    //

    SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_INSERTSTRING, -1, (LPARAM)(LPSTR)buf);

    PlugList[DVDSelectedPlug] = (char *)malloc(strlen(name) + 1);
    strcpy(PlugList[DVDSelectedPlug++], name);
}

static void EnumDVDPlugins(HWND hwndDlg)
{
    char PluginPath[256];
    WIN32_FIND_DATA fd;
    HANDLE hfff;
    int i, selected = -1;

    for(i=0; i<256; i++)
    {
        if(PlugList[i])
        {
            free(PlugList[i]);
            PlugList[i] = NULL;
        }
    }

    DVDSelectedPlug = 0;

    SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_RESETCONTENT, 0, 0);

    sprintf(PluginPath, ".\\Plugins\\*.dll");
    hfff = FindFirstFile(PluginPath, &fd);

    if(hfff == INVALID_HANDLE_VALUE) return;
    else
    {
        sprintf(PluginPath, ".\\Plugins\\%s", fd.cFileName);
        AddDVDPlugin(hwndDlg, PluginPath);

        if(!strcmp(
            strrchr(GetConfigString(USER_DVD, USER_DVD_DEFAULT), '\\') + 1,
            strrchr(PluginPath, '\\') + 1 ))
        {
            selected = DVDSelectedPlug;
        }
    }

    while(FindNextFile(hfff, &fd))
    {
        sprintf(PluginPath, ".\\Plugins\\%s", fd.cFileName);
        AddDVDPlugin(hwndDlg, PluginPath);

        if(!strcmp(
            strrchr(GetConfigString(USER_DVD, USER_DVD_DEFAULT), '\\') + 1,
            strrchr(PluginPath, '\\') + 1 ))
        {
            selected = DVDSelectedPlug;
        }
    }

    if(selected >= 0)
    {
        SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_SETCURSEL, selected - 1, 0);
    }
    else
    {
        SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_SETCURSEL, 0, 0);
        DVDSelectedPlug = -1;
    }
}

static INT_PTR CALLBACK ConfigureDVDProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    int i, selected;

    switch(uMsg)
    {
        case WM_INITDIALOG:
            SetWindowText(hwndDlg, "Select DVD Plugin");
            GetWindowRect(wnd.hMainWindow, &rc);
            SetWindowPos(hwndDlg, HWND_TOP, rc.left + 100, rc.top  + 100, 0, 0, SWP_NOSIZE); 
            SendMessage(hwndDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_DOLWIN_ICON)));
            ShowWindow(GetDlgItem(hwndDlg, IDC_PLUGIN_PADNUM), SW_HIDE);
            EnumDVDPlugins(hwndDlg);
            selected = (int)SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);
            if(selected >= 0)
            {
                SetDlgItemText(hwndDlg, IDC_PLUGIN_PATH, PlugList[selected]);
            }
            return TRUE;

        case WM_CLOSE:
            for(i=0; i<256; i++)
            {
                if(PlugList[i])
                {
                    free(PlugList[i]);
                    PlugList[i] = NULL;
                }
            }
            EndDialog(hwndDlg, 0);
            return TRUE;

        case WM_COMMAND:
            if(wParam == IDCANCEL)
            {
                for(i=0; i<256; i++)
                {
                    if(PlugList[i])
                    {
                        free(PlugList[i]);
                        PlugList[i] = NULL;
                    }
                }
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            if(wParam == IDOK)
            {
                selected = (int)SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);

                SetConfigString(USER_DVD, PlugList[selected]);

                // reinit plugin system
                PSShutdown();
                PSInit();

                for(i=0; i<256; i++)
                {
                    if(PlugList[i])
                    {
                        free(PlugList[i]);
                        PlugList[i] = NULL;
                    }
                }
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            switch(LOWORD(wParam))
            {
                case IDC_PLUGIN_ABOUT:
                    {
                        HMODULE hPlug;
                        FARPROC old = (FARPROC)DVDAbout;
                     
                        selected = (int)SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);

                        if(hPlug = LoadLibrary(PlugList[selected]))
                        {
                            Register(hPlug);
                            DVDAbout = (DVDABOUT)GetProcAddress(hPlug, "DVDAbout");
                            if(DVDAbout) DVDAbout();
                            FreeLibrary(hPlug);
                            DVDAbout = (DVDABOUT)old;
                        }
                    }
                    return FALSE;

                case IDC_PLUGIN_CONFIG:
                    {
                        HMODULE hPlug;
                        FARPROC old = (FARPROC)DVDConfigure;

                        selected = (int)SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);

                        if(hPlug = LoadLibrary(PlugList[selected]))
                        {
                            Register(hPlug);
                            DVDConfigure = (DVDCONFIGURE)GetProcAddress(hPlug, "DVDConfigure");
                            if(DVDConfigure) DVDConfigure();
                            FreeLibrary(hPlug);
                            DVDConfigure = (DVDCONFIGURE)old;
                        }
                    }
                    return FALSE;
            }
            switch(HIWORD(wParam))
            {
                case CBN_SELCHANGE:
                    DVDSelectedPlug = (int)SendDlgItemMessage(hwndDlg, IDC_PLUGIN_LIST, CB_GETCURSEL, 0, 0);
                    if(DVDSelectedPlug >= 0)
                    {
                        SetDlgItemText(hwndDlg, IDC_PLUGIN_PATH, PlugList[DVDSelectedPlug]);
                    }
                    return FALSE;
            }
            break;

        default:
            return FALSE;
    }
    return FALSE;
}

void ConfigureDVDDialog()
{
    DialogBox(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_SELECT_PLUGIN),
        wnd.hMainWindow,
        ConfigureDVDProc);
}
