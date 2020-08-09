// select ANSI/SJIS fonts dialog
#include "pch.h"

static HWND     parentWnd;
static wchar_t    FontsDir[] = L".\\Data";      // font placeholder
static wchar_t*   FontAnsiList[256];              // list for combo box
static wchar_t*   FontSjisList[256];              // list for combo box
static int      AnsiSelected, SjisSelected;     // current selected in combo
static wchar_t    FontAnsiFile[MAX_PATH];         // copy of user variable
static wchar_t    FontSjisFile[MAX_PATH];         // copy of user variables

#define ANSI_FONT_SIZE   0x3000

// ---------------------------------------------------------------------------

static void FontSetAnsiFile(const TCHAR *filename)
{
    wcscpy_s(FontAnsiFile, _countof(FontAnsiFile) - 1, filename);
    UI::Jdi.SetConfigString(USER_ANSI, Util::WstringToString(FontAnsiFile), USER_HW);
}

static void FontSetSjisFile(const TCHAR *filename)
{
    wcscpy_s (FontSjisFile, _countof(FontSjisFile) - 1, filename);
    UI::Jdi.SetConfigString(USER_SJIS, Util::WstringToString(FontSjisFile), USER_HW);
}

static void AddFont(HWND hwndDlg, TCHAR *file)
{
    // split path
    TCHAR drive[_MAX_DRIVE + 1], dir[_MAX_DIR], name[_MAX_PATH], ext[_MAX_EXT];
    _wsplitpath_s (file, 
        drive, _countof(drive) - 1, 
        dir, _countof(dir) - 1, 
        name, _countof(name) - 1, 
        ext, _countof(ext) - 1 );

    // check font type
    size_t size = Util::FileSize(file);
    BOOL is_ansi = (size <= ANSI_FONT_SIZE);

    if(is_ansi)
    {   // Ansi type
        size_t sizeInChars = (wcslen(file) + 1);
        FontAnsiList[AnsiSelected] = (TCHAR *)malloc(sizeInChars * sizeof(TCHAR));
        wcscpy_s(FontAnsiList[AnsiSelected++], sizeInChars, file);
        //SendDlgItemMessage(hwndDlg, IDC_FONT_ANSICOMBO, CB_INSERTSTRING, -1, (LPARAM)(LPSTR)FileShortName(file));
        SendDlgItemMessage(hwndDlg, IDC_FONT_ANSICOMBO, CB_INSERTSTRING, -1, (LPARAM)(LPSTR)name);
    }
    else
    {   // Sjis type
        size_t sizeInChars = (wcslen(file) + 1);
        FontSjisList[SjisSelected] = (TCHAR *)malloc(sizeInChars * sizeof(TCHAR));
        wcscpy_s(FontSjisList[SjisSelected++], sizeInChars, file);
        //SendDlgItemMessage(hwndDlg, IDC_FONT_SJISCOMBO, CB_INSERTSTRING, -1, (LPARAM)(LPSTR)FileShortName(file));
        SendDlgItemMessage(hwndDlg, IDC_FONT_SJISCOMBO, CB_INSERTSTRING, -1, (LPARAM)(LPSTR)name);
    }
}

static void EnumFonts(HWND hwndDlg)
{
    TCHAR FontPath[MAX_PATH] = { 0 };
    WIN32_FIND_DATA fd;
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

    swprintf_s (FontPath, _countof(FontPath) - 1, L"%s\\*.szp", FontsDir);
    hfff = FindFirstFile(FontPath, &fd);

    if(hfff == INVALID_HANDLE_VALUE) return;
    else
    {
        swprintf_s (FontPath, _countof(FontPath) - 1, L"%s\\%s", FontsDir, fd.cFileName);
        AddFont(hwndDlg, FontPath);

        if(!wcscmp(FontAnsiFile, FontPath))
            Aselected = AnsiSelected;
        if(!wcscmp(FontSjisFile, FontPath))
            Sselected = SjisSelected;
    }

    while(FindNextFile(hfff, &fd))
    {
        swprintf_s (FontPath, _countof(FontPath) - 1, L"%s\\%s", FontsDir, fd.cFileName);
        AddFont(hwndDlg, FontPath);

        if(!wcscmp(FontAnsiFile, FontPath))
            Aselected = AnsiSelected;
        if(!wcscmp(FontSjisFile, FontPath))
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

                if (wcscmp(FontAnsiList[AnsiSelected], FontAnsiFile) != 0)
                    FontSetAnsiFile(FontAnsiList[AnsiSelected]);

                if (wcscmp(FontSjisList[SjisSelected], FontSjisFile) != 0)
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
    FontSetAnsiFile( Util::StringToWstring(UI::Jdi.GetConfigString(USER_ANSI, USER_HW)).c_str() );
    FontSetSjisFile( Util::StringToWstring(UI::Jdi.GetConfigString(USER_SJIS, USER_HW)).c_str() );

    DialogBox(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_FONT_SETTINGS),
        parentWnd = hParent,
        FontSettingsProc);
}
