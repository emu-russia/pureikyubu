// UI file utilities
#include "pch.h"

namespace UI
{
    // Open file/directory dialog
    const wchar_t* FileOpenDialog(HWND hwnd, FileType type)
    {
        static wchar_t tempBuf[0x1000] = { 0 };
        OPENFILENAME ofn;
        wchar_t szFileName[1024];
        wchar_t szFileTitle[1024];
        wchar_t prevDir[1024];
        std::string lastDir;
        BOOL result;

        GetCurrentDirectory(sizeof(prevDir), prevDir);

        switch (type)
        {
            case FileType::All:
            case FileType::Json:
                lastDir = UI::Jdi.GetConfigString(USER_LASTDIR_ALL, USER_UI);
                break;
            case FileType::Dvd:
                lastDir = UI::Jdi.GetConfigString(USER_LASTDIR_DVD, USER_UI);
                break;
            case FileType::Map:
                lastDir = UI::Jdi.GetConfigString(USER_LASTDIR_MAP, USER_UI);
                break;
        }

        memset(szFileName, 0, sizeof(szFileName));
        memset(szFileTitle, 0, sizeof(szFileTitle));

        if (type == FileType::Directory)
        {
            BROWSEINFO bi;
            LPTSTR lpBuffer = NULL;
            LPITEMIDLIST pidlRoot = NULL;     // PIDL for root folder 
            LPITEMIDLIST pidlBrowse = NULL;   // PIDL selected by user
            LPMALLOC g_pMalloc;

            // Get the shell's allocator. 
            if (!SUCCEEDED(SHGetMalloc(&g_pMalloc)))
                return L"";

            // Allocate a buffer to receive browse information.
            lpBuffer = (LPTSTR)g_pMalloc->Alloc(MAX_PATH);
            if (lpBuffer == NULL) return L"";

            // Get the PIDL for the root folder.
            if (!SUCCEEDED(SHGetSpecialFolderLocation(hwnd, CSIDL_DRIVES, &pidlRoot)))
            {
                g_pMalloc->Free(lpBuffer);
                return L"";
            }

            // Fill in the BROWSEINFO structure. 
            bi.hwndOwner = hwnd;
            bi.pidlRoot = pidlRoot;
            bi.pszDisplayName = lpBuffer;
            bi.lpszTitle = L"Choose Directory";
            bi.ulFlags = 0;
            bi.lpfn = NULL;
            bi.lParam = 0;

            // Browse for a folder and return its PIDL. 
            pidlBrowse = SHBrowseForFolder(&bi);
            result = (pidlBrowse != NULL);
            if (result)
            {
                SHGetPathFromIDList(pidlBrowse, lpBuffer);
                wcscpy_s(szFileName, _countof(szFileName) - 1, lpBuffer);

                // Free the PIDL returned by SHBrowseForFolder.
                g_pMalloc->Free(pidlBrowse);
            }

            // Clean up. 
            if (pidlRoot) g_pMalloc->Free(pidlRoot);
            if (lpBuffer) g_pMalloc->Free(lpBuffer);

            // Release the shell's allocator. 
            g_pMalloc->Release();
        }
        else
        {
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = hwnd;
            switch (type)
            {
                case FileType::All:
                    ofn.lpstrFilter =
                        L"All Supported Files (*.dol, *.elf, *.bin, *.gcm, *.iso)\0*.dol;*.elf;*.bin;*.gcm;*.iso\0"
                        L"GameCube Executable Files (*.dol, *.elf)\0*.dol;*.elf\0"
                        L"Binary Files (*.bin)\0*.bin\0"
                        L"GameCube DVD Images (*.gcm, *.iso)\0*.gcm;*.iso\0"
                        L"All Files (*.*)\0*.*\0";
                    break;
                case FileType::Dvd:
                    ofn.lpstrFilter =
                        L"GameCube DVD Images (*.gcm, *.iso)\0*.gcm;*.iso\0"
                        L"All Files (*.*)\0*.*\0";
                    break;
                case FileType::Map:
                    ofn.lpstrFilter =
                        L"Symbolic information files (*.map)\0*.map\0"
                        L"All Files (*.*)\0*.*\0";
                    break;
                case FileType::Json:
                    ofn.lpstrFilter =
                        L"Json files (*.json)\0*.json\0"
                        L"All Files (*.*)\0*.*\0";
                    break;
            }

            ofn.lpstrCustomFilter = NULL;
            ofn.nMaxCustFilter = 0;
            ofn.nFilterIndex = 1;
            ofn.lpstrFile = szFileName;
            ofn.nMaxFile = sizeof(szFileName);
            ofn.lpstrInitialDir = Util::StringToWstring(lastDir).c_str();
            ofn.lpstrFileTitle = szFileTitle;
            ofn.nMaxFileTitle = sizeof(szFileTitle);
            ofn.lpstrTitle = L"Open File\0";
            ofn.lpstrDefExt = L"";
            ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            result = GetOpenFileName(&ofn);
        }

        if (result)
        {
            wcscpy_s(tempBuf, _countof(tempBuf) - 1, szFileName);

            // save last directory

            lastDir = Util::WstringToString(tempBuf);

            while (lastDir.back() != '\\')
            {
                lastDir.pop_back();
            }

            switch (type)
            {
                case FileType::All:
                case FileType::Json:
                    UI::Jdi.SetConfigString(USER_LASTDIR_ALL, lastDir, USER_UI);
                    break;
                case FileType::Dvd:
                    UI::Jdi.SetConfigString(USER_LASTDIR_DVD, lastDir, USER_UI);
                    break;
                case FileType::Map:
                    UI::Jdi.SetConfigString(USER_LASTDIR_MAP, lastDir, USER_UI);
                    break;
            }

            SetCurrentDirectory(prevDir);
            return tempBuf;
        }
        else
        {
            SetCurrentDirectory(prevDir);
            return L"";
        }
    }

    // Save file dialog
    const wchar_t* FileSaveDialog(HWND hwnd, FileType type)
    {
        static wchar_t tempBuf[0x1000] = { 0 };
        OPENFILENAME ofn;
        wchar_t szFileName[1024];
        wchar_t szFileTitle[1024];
        wchar_t prevDir[1024];
        std::string lastDir;
        BOOL result;

        GetCurrentDirectory(sizeof(prevDir), prevDir);

        switch (type)
        {
            case FileType::All:
            case FileType::Json:
                lastDir = UI::Jdi.GetConfigString(USER_LASTDIR_ALL, USER_UI);
                break;
            case FileType::Dvd:
                lastDir = UI::Jdi.GetConfigString(USER_LASTDIR_DVD, USER_UI);
                break;
            case FileType::Map:
                lastDir = UI::Jdi.GetConfigString(USER_LASTDIR_MAP, USER_UI);
                break;
        }

        memset(szFileName, 0, sizeof(szFileName));
        memset(szFileTitle, 0, sizeof(szFileTitle));

        {
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = hwnd;
            switch (type)
            {
                case FileType::All:
                    ofn.lpstrFilter =
                        L"All Supported Files (*.dol, *.elf, *.bin, *.gcm, *.iso)\0*.dol;*.elf;*.bin;*.gcm;*.iso\0"
                        L"GameCube Executable Files (*.dol, *.elf)\0*.dol;*.elf\0"
                        L"Binary Files (*.bin)\0*.bin\0"
                        L"GameCube DVD Images (*.gcm, *.iso)\0*.gcm;*.iso\0"
                        L"All Files (*.*)\0*.*\0";
                    break;
                case FileType::Dvd:
                    ofn.lpstrFilter =
                        L"GameCube DVD Images (*.gcm, *.iso)\0*.gcm;*.iso\0"
                        L"All Files (*.*)\0*.*\0";
                    break;
                case FileType::Map:
                    ofn.lpstrFilter =
                        L"Symbolic information files (*.map)\0*.map\0"
                        L"All Files (*.*)\0*.*\0";
                    break;
                case FileType::Json:
                    ofn.lpstrFilter =
                        L"Json files (*.json)\0*.json\0"
                        L"All Files (*.*)\0*.*\0";
                    break;
            }

            ofn.lpstrCustomFilter = NULL;
            ofn.nMaxCustFilter = 0;
            ofn.nFilterIndex = 1;
            ofn.lpstrFile = szFileName;
            ofn.nMaxFile = sizeof(szFileName);
            ofn.lpstrInitialDir = Util::StringToWstring(lastDir).c_str();
            ofn.lpstrFileTitle = szFileTitle;
            ofn.nMaxFileTitle = sizeof(szFileTitle);
            ofn.lpstrTitle = L"Save File\0";
            ofn.lpstrDefExt = L"";
            ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;

            result = GetSaveFileName(&ofn);
        }

        if (result)
        {
            wcscpy_s(tempBuf, _countof(tempBuf) - 1, szFileName);

            // save last directory

            lastDir = Util::WstringToString(tempBuf);

            while (lastDir.back() != '\\')
            {
                lastDir.pop_back();
            }

            switch (type)
            {
                case FileType::All:
                case FileType::Json:
                    UI::Jdi.SetConfigString(USER_LASTDIR_ALL, lastDir, USER_UI);
                    break;
                case FileType::Dvd:
                    UI::Jdi.SetConfigString(USER_LASTDIR_DVD, lastDir, USER_UI);
                    break;
                case FileType::Map:
                    UI::Jdi.SetConfigString(USER_LASTDIR_MAP, lastDir, USER_UI);
                    break;
            }

            SetCurrentDirectory(prevDir);
            return tempBuf;
        }
        else
        {
            SetCurrentDirectory(prevDir);
            return L"";
        }
    }

    // make path to file shorter for "lvl" levels.
    wchar_t* FileShortName(const wchar_t* filename, int lvl)
    {
        static wchar_t tempBuf[1024] = { 0 };

        int c = 0;
        size_t i = 0;

        wchar_t* ptr = (wchar_t*)filename;

        tempBuf[0] = ptr[0];
        tempBuf[1] = ptr[1];
        tempBuf[2] = ptr[2];

        ptr += 3;

        for (i = wcslen(ptr) - 1; i; i--)
        {
            if (ptr[i] == L'\\') c++;
            if (c == lvl) break;
        }

        if (c == lvl)
        {
            swprintf_s(&tempBuf[3], _countof(tempBuf) - 3, L"...%s", &ptr[i]);
        }
        else return ptr - 3;

        return tempBuf;
    }

    /* Make path to file shorter for "lvl" levels. */
    std::wstring FileShortName(const std::wstring& filename, int lvl)
    {
        static wchar_t tempBuf[1024] = { 0 };

        int c = 0;
        size_t i = 0;

        wchar_t* ptr = (wchar_t*)filename.data();

        tempBuf[0] = ptr[0];
        tempBuf[1] = ptr[1];
        tempBuf[2] = ptr[2];

        ptr += 3;

        for (i = wcslen(ptr) - 1; i; i--)
        {
            if (ptr[i] == L'\\') c++;
            if (c == lvl) break;
        }

        if (c == lvl)
        {
            swprintf_s(&tempBuf[3], _countof(tempBuf) - 3, L"...%s", &ptr[i]);
        }
        else return ptr - 3;

        return tempBuf;
    }

    /* Nice value of KB, MB or GB, for output. */
    std::wstring FileSmartSize(size_t size)
    {
        static auto tempBuf = std::wstring();

        if (size < 1024)
        {
            tempBuf = fmt::sprintf(L"%zi byte", size);
        }
        else if (size < 1024 * 1024)
        {
            tempBuf = fmt::sprintf(L"%zi KB", size / 1024);
        }
        else if (size < 1024 * 1024 * 1024)
        {
            tempBuf = fmt::sprintf(L"%zi MB", size / 1024 / 1024);
        }
        else
        {
            tempBuf = fmt::sprintf(L"%1.1f GB", (float)size / 1024 / 1024 / 1024);
        }
        
        return tempBuf;
    }

    std::string FileSmartSizeA(size_t size)
    {
        static auto tempBuf = std::string(1024, 0);

        if (size < 1024)
        {
            tempBuf = fmt::format("%zi byte", size);
        }
        else if (size < 1024 * 1024)
        {
            tempBuf = fmt::format("%zi KB", size / 1024);
        }
        else if (size < 1024 * 1024 * 1024)
        {
            tempBuf = fmt::format("%zi MB", size / 1024 / 1024);
        }
        else
        {
            tempBuf = fmt::format("%1.1f GB", (float)size / 1024 / 1024 / 1024);
        }

        return tempBuf;
    }
}
