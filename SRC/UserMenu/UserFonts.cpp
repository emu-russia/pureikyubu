// select ANSI/SJIS fonts dialog
#include "dolphin.h"

static HWND     parentWnd;
static TCHAR    FontsDir[] = _T(".\\Data");     // font placeholder
static TCHAR*   FontAnsiList[256];              // list for combo box
static TCHAR*   FontSjisList[256];              // list for combo box
static int      AnsiSelected, SjisSelected;     // current selected in combo
static TCHAR    FontAnsiFile[MAX_PATH];         // copy of user variable
static TCHAR    FontSjisFile[MAX_PATH];         // copy of user variables

// ---------------------------------------------------------------------------

static void FontSetAnsiFile(TCHAR *filename)
{
    strcpy_s(FontAnsiFile, sizeof(FontAnsiFile), filename);
    SetConfigString(USER_ANSI, FontAnsiFile, USER_HW);
}

static void FontSetSjisFile(TCHAR *filename)
{
    strcpy_s(FontSjisFile, sizeof(FontSjisFile), filename);
    SetConfigString(USER_SJIS, FontSjisFile, USER_HW);
}

static void AddFont(HWND hwndDlg, TCHAR *file)
{
    // split path
    TCHAR drive[_MAX_DRIVE + 1], dir[_MAX_DIR], name[_MAX_PATH], ext[_MAX_EXT];
    _splitpath(file, drive, dir, name, ext);

    // check font type
    size_t size = UI::FileSize(file);
    BOOL is_ansi = (size <= ANSI_SIZE);

    if(is_ansi)
    {   // Ansi type
        FontAnsiList[AnsiSelected] = (char *)malloc(strlen(file) + 1);
        strcpy(FontAnsiList[AnsiSelected++], file);
        //SendDlgItemMessage(hwndDlg, IDC_FONT_ANSICOMBO, CB_INSERTSTRING, -1, (LPARAM)(LPSTR)FileShortName(file));
        SendDlgItemMessage(hwndDlg, IDC_FONT_ANSICOMBO, CB_INSERTSTRING, -1, (LPARAM)(LPSTR)name);
    }
    else
    {   // Sjis type
        FontSjisList[SjisSelected] = (char *)malloc(strlen(file) + 1);
        strcpy(FontSjisList[SjisSelected++], file);
        //SendDlgItemMessage(hwndDlg, IDC_FONT_SJISCOMBO, CB_INSERTSTRING, -1, (LPARAM)(LPSTR)FileShortName(file));
        SendDlgItemMessage(hwndDlg, IDC_FONT_SJISCOMBO, CB_INSERTSTRING, -1, (LPARAM)(LPSTR)name);
    }
}

static void EnumFonts(HWND hwndDlg)
{
    char FontPath[256];
    WIN32_FIND_DATAA fd;
    HANDLE hfff;
    int i, Aselected = -1, Sselected = -1;

    for(i=0; i<256; i++)
    {
        if(FontAnsiList[i])
        {
            free(FontAnsiList[i]);
            FontAnsiList[i] = NULL;
        }
        if(FontSjisList[i])
        {
            free(FontSjisList[i]);
            FontSjisList[i] = NULL;
        }
    }

    AnsiSelected = 0;
    SjisSelected = 0;

    SendDlgItemMessage(hwndDlg, IDC_FONT_ANSICOMBO, CB_RESETCONTENT, 0, 0);
    SendDlgItemMessage(hwndDlg, IDC_FONT_SJISCOMBO, CB_RESETCONTENT, 0, 0);

    sprintf_s(FontPath, sizeof(FontPath), "%s\\*.szp", FontsDir);
    hfff = FindFirstFileA(FontPath, &fd);

    if(hfff == INVALID_HANDLE_VALUE) return;
    else
    {
        sprintf_s(FontPath, sizeof(FontPath), "%s\\%s", FontsDir, fd.cFileName);
        AddFont(hwndDlg, FontPath);

        if(!strcmp(FontAnsiFile, FontPath))
            Aselected = AnsiSelected;
        if(!strcmp(FontSjisFile, FontPath))
            Sselected = SjisSelected;
    }

    while(FindNextFileA(hfff, &fd))
    {
        sprintf_s(FontPath, sizeof(FontPath), "%s\\%s", FontsDir, fd.cFileName);
        AddFont(hwndDlg, FontPath);

        if(!strcmp(FontAnsiFile, FontPath))
            Aselected = AnsiSelected;
        if(!strcmp(FontSjisFile, FontPath))
            Sselected = SjisSelected;
    }

    if(Aselected >= 0)
    {
        SendDlgItemMessage(hwndDlg, IDC_FONT_ANSICOMBO, CB_SETCURSEL, Aselected - 1, 0);
    }
    else
    {
        SendDlgItemMessage(hwndDlg, IDC_FONT_ANSICOMBO, CB_SETCURSEL, 0, 0);
        AnsiSelected = -1;
    }

    if(Sselected >= 0)
    {
        SendDlgItemMessage(hwndDlg, IDC_FONT_SJISCOMBO, CB_SETCURSEL, Sselected - 1, 0);
    }
    else
    {
        SendDlgItemMessage(hwndDlg, IDC_FONT_SJISCOMBO, CB_SETCURSEL, 0, 0);
        SjisSelected = -1;
    }
}

static INT_PTR CALLBACK FontSettingsProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    int i;
    switch(uMsg)
    {
        case WM_INITDIALOG:
            CenterChildWindow(parentWnd, hwndDlg);
            SendMessage(hwndDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_DOLWIN_ICON)));
            EnumFonts(hwndDlg);
            return TRUE;

        case WM_CLOSE:
            for(i=0; i<256; i++)
            {
                if(FontAnsiList[i])
                {
                    free(FontAnsiList[i]);
                    FontAnsiList[i] = NULL;
                }
                if(FontSjisList[i])
                {
                    free(FontSjisList[i]);
                    FontSjisList[i] = NULL;
                }
            }
            EndDialog(hwndDlg, 0);
            return TRUE;

        case WM_COMMAND:
            if(wParam == IDCANCEL)
            {
                for(i=0; i<256; i++)
                {
                    if(FontAnsiList[i])
                    {
                        free(FontAnsiList[i]);
                        FontAnsiList[i] = NULL;
                    }
                    if(FontSjisList[i])
                    {
                        free(FontSjisList[i]);
                        FontSjisList[i] = NULL;
                    }
                }
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            if(wParam == IDOK)
            {

                AnsiSelected = (int)SendDlgItemMessage(hwndDlg, IDC_FONT_ANSICOMBO, CB_GETCURSEL, 0, 0);
                SjisSelected = (int)SendDlgItemMessage(hwndDlg, IDC_FONT_SJISCOMBO, CB_GETCURSEL, 0, 0);

                if (strcmp(FontAnsiList[AnsiSelected], FontAnsiFile) != 0)
                    FontSetAnsiFile(FontAnsiList[AnsiSelected]);

                if (strcmp(FontSjisList[SjisSelected], FontSjisFile) != 0)
                    FontSetSjisFile(FontSjisList[SjisSelected]);

                for(i=0; i<256; i++)
                {
                    if(FontAnsiList[i])
                    {
                        free(FontAnsiList[i]);
                        FontAnsiList[i] = NULL;
                    }
                    if(FontSjisList[i])
                    {
                        free(FontSjisList[i]);
                        FontSjisList[i] = NULL;
                    }
                }
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            break;

        default:
            return FALSE;
    }
    return FALSE;
}

void FontConfigure(HWND hParent)
{
    FontSetAnsiFile(GetConfigString(USER_ANSI, USER_HW));
    FontSetSjisFile(GetConfigString(USER_SJIS, USER_HW));

    DialogBox(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_FONT_SETTINGS),
        parentWnd = hParent,
        FontSettingsProc);
}
