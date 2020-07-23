// Dolwin file utilities
#include "pch.h"

namespace UI
{
    bool FileExists(const TCHAR* filename)
    {
        FILE* f = nullptr;
        _tfopen_s(&f, filename, _T("rb"));
        if (f == NULL) return false;
        fclose(f);
        return true;
    }

    // Get file size
    size_t FileSize(const TCHAR* filename)
    {
        FILE* f = nullptr;
        _tfopen_s(&f, filename, _T("rb"));
        if (f == NULL) return 0;
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        fclose(f);
        return size;
    }

    // Open file/directory dialog
    TCHAR* FileOpenDialog(HWND hwnd, FileType type)
    {
        static TCHAR tempBuf[0x1000] = { 0 };
        OPENFILENAME ofn;
        TCHAR szFileName[1024];
        TCHAR szFileTitle[1024];
        TCHAR lastDir[1024], prevDir[1024];
        BOOL result;

        GetCurrentDirectory(sizeof(prevDir), prevDir);

        switch (type)
        {
            case FileType::All:
            case FileType::Json:
                _tcscpy_s(lastDir, _countof(lastDir) - 1, GetConfigString(USER_LASTDIR_ALL, USER_UI));
                break;
            case FileType::Dvd:
                _tcscpy_s(lastDir, _countof(lastDir) - 1, GetConfigString(USER_LASTDIR_DVD, USER_UI));
                break;
            case FileType::Map:
                _tcscpy_s(lastDir, _countof(lastDir) - 1, GetConfigString(USER_LASTDIR_MAP, USER_UI));
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
                return NULL;

            // Allocate a buffer to receive browse information.
            lpBuffer = (LPTSTR)g_pMalloc->Alloc(MAX_PATH);
            if (lpBuffer == NULL) return NULL;

            // Get the PIDL for the root folder.
            if (!SUCCEEDED(SHGetSpecialFolderLocation(hwnd, CSIDL_DRIVES, &pidlRoot)))
            {
                g_pMalloc->Free(lpBuffer);
                return NULL;
            }

            // Fill in the BROWSEINFO structure. 
            bi.hwndOwner = hwnd;
            bi.pidlRoot = pidlRoot;
            bi.pszDisplayName = lpBuffer;
            bi.lpszTitle = _T("Choose Directory");
            bi.ulFlags = 0;
            bi.lpfn = NULL;
            bi.lParam = 0;

            // Browse for a folder and return its PIDL. 
            pidlBrowse = SHBrowseForFolder(&bi);
            result = (pidlBrowse != NULL);
            if (result)
            {
                SHGetPathFromIDList(pidlBrowse, lpBuffer);
                _tcscpy_s(szFileName, _countof(szFileName) - 1, lpBuffer);

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
                        _T("All Supported Files (*.dol, *.elf, *.bin, *.gcm, *.iso)\0*.dol;*.elf;*.bin;*.gcm;*.iso\0")
                        _T("GameCube Executable Files (*.dol, *.elf)\0*.dol;*.elf\0")
                        _T("Binary Files (*.bin)\0*.bin\0")
                        _T("GameCube DVD Images (*.gcm, *.iso)\0*.gcm;*.iso\0")
                        _T("All Files (*.*)\0*.*\0");
                    break;
                case FileType::Dvd:
                    ofn.lpstrFilter =
                        _T("GameCube DVD Images (*.gcm, *.iso)\0*.gcm;*.iso\0")
                        _T("All Files (*.*)\0*.*\0");
                    break;
                case FileType::Map:
                    ofn.lpstrFilter =
                        _T("Symbolic information files (*.map)\0*.map\0")
                        _T("All Files (*.*)\0*.*\0");
                    break;
                case FileType::Json:
                    ofn.lpstrFilter =
                        _T("Json files (*.json)\0*.json\0")
                        _T("All Files (*.*)\0*.*\0");
                    break;
            }

            ofn.lpstrCustomFilter = NULL;
            ofn.nMaxCustFilter = 0;
            ofn.nFilterIndex = 1;
            ofn.lpstrFile = szFileName;
            ofn.nMaxFile = sizeof(szFileName);
            ofn.lpstrInitialDir = lastDir;
            ofn.lpstrFileTitle = szFileTitle;
            ofn.nMaxFileTitle = sizeof(szFileTitle);
            ofn.lpstrTitle = _T("Open File\0");
            ofn.lpstrDefExt = _T("");
            ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            result = GetOpenFileName(&ofn);
        }

        if (result)
        {
            _tcscpy_s(tempBuf, _countof(tempBuf) - 1, szFileName);

            // save last directory
            _tcscpy_s(lastDir, _countof(lastDir) - 1, tempBuf);
            int i = (int)_tcslen(lastDir) - 1;
            while (lastDir[i] != _T('\\')) i--;
            lastDir[i + 1] = _T('\0');
            switch (type)
            {
                case FileType::All:
                case FileType::Json:
                    SetConfigString(USER_LASTDIR_ALL, lastDir, USER_UI);
                    break;
                case FileType::Dvd:
                    SetConfigString(USER_LASTDIR_DVD, lastDir, USER_UI);
                    break;
                case FileType::Map:
                    SetConfigString(USER_LASTDIR_MAP, lastDir, USER_UI);
                    break;
            }

            SetCurrentDirectory(prevDir);
            return tempBuf;
        }
        else
        {
            SetCurrentDirectory(prevDir);
            return NULL;
        }
    }

    // Save file dialog
    TCHAR* FileSaveDialog(HWND hwnd, FileType type)
    {
        static TCHAR tempBuf[0x1000] = { 0 };
        OPENFILENAME ofn;
        TCHAR szFileName[1024];
        TCHAR szFileTitle[1024];
        TCHAR lastDir[1024], prevDir[1024];
        BOOL result;

        GetCurrentDirectory(sizeof(prevDir), prevDir);

        switch (type)
        {
            case FileType::All:
            case FileType::Json:
                _tcscpy_s(lastDir, _countof(lastDir) - 1, GetConfigString(USER_LASTDIR_ALL, USER_UI));
                break;
            case FileType::Dvd:
                _tcscpy_s(lastDir, _countof(lastDir) - 1, GetConfigString(USER_LASTDIR_DVD, USER_UI));
                break;
            case FileType::Map:
                _tcscpy_s(lastDir, _countof(lastDir) - 1, GetConfigString(USER_LASTDIR_MAP, USER_UI));
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
                    _T("All Supported Files (*.dol, *.elf, *.bin, *.gcm, *.iso)\0*.dol;*.elf;*.bin;*.gcm;*.iso\0")
                    _T("GameCube Executable Files (*.dol, *.elf)\0*.dol;*.elf\0")
                    _T("Binary Files (*.bin)\0*.bin\0")
                    _T("GameCube DVD Images (*.gcm, *.iso)\0*.gcm;*.iso\0")
                    _T("All Files (*.*)\0*.*\0");
                break;
            case FileType::Dvd:
                ofn.lpstrFilter =
                    _T("GameCube DVD Images (*.gcm, *.iso)\0*.gcm;*.iso\0")
                    _T("All Files (*.*)\0*.*\0");
                break;
            case FileType::Map:
                ofn.lpstrFilter =
                    _T("Symbolic information files (*.map)\0*.map\0")
                    _T("All Files (*.*)\0*.*\0");
                break;
            case FileType::Json:
                ofn.lpstrFilter =
                    _T("Json files (*.json)\0*.json\0")
                    _T("All Files (*.*)\0*.*\0");
                break;
            }

            ofn.lpstrCustomFilter = NULL;
            ofn.nMaxCustFilter = 0;
            ofn.nFilterIndex = 1;
            ofn.lpstrFile = szFileName;
            ofn.nMaxFile = sizeof(szFileName);
            ofn.lpstrInitialDir = lastDir;
            ofn.lpstrFileTitle = szFileTitle;
            ofn.nMaxFileTitle = sizeof(szFileTitle);
            ofn.lpstrTitle = _T("Save File\0");
            ofn.lpstrDefExt = _T("");
            ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;

            result = GetSaveFileName(&ofn);
        }

        if (result)
        {
            _tcscpy_s(tempBuf, _countof(tempBuf) - 1, szFileName);

            // save last directory
            _tcscpy_s(lastDir, _countof(lastDir) - 1, tempBuf);
            int i = (int)_tcslen(lastDir) - 1;
            while (lastDir[i] != _T('\\')) i--;
            lastDir[i + 1] = _T('\0');
            switch (type)
            {
                case FileType::All:
                case FileType::Json:
                    SetConfigString(USER_LASTDIR_ALL, lastDir, USER_UI);
                    break;
                case FileType::Dvd:
                    SetConfigString(USER_LASTDIR_DVD, lastDir, USER_UI);
                    break;
                case FileType::Map:
                    SetConfigString(USER_LASTDIR_MAP, lastDir, USER_UI);
                    break;
            }

            SetCurrentDirectory(prevDir);
            return tempBuf;
        }
        else
        {
            SetCurrentDirectory(prevDir);
            return NULL;
        }
    }

    // make path to file shorter for "lvl" levels.
    TCHAR* FileShortName(const TCHAR* filename, int lvl)
    {
        static TCHAR tempBuf[1024] = { 0 };

        int c = 0;
        size_t i = 0;

        TCHAR* ptr = (TCHAR*)filename;

        tempBuf[0] = ptr[0];
        tempBuf[1] = ptr[1];
        tempBuf[2] = ptr[2];

        ptr += 3;

        for (i = _tcslen(ptr) - 1; i; i--)
        {
            if (ptr[i] == _T('\\')) c++;
            if (c == lvl) break;
        }

        if (c == lvl)
        {
            _stprintf_s(&tempBuf[3], _countof(tempBuf) - 3, _T("...%s"), &ptr[i]);
        }
        else return ptr - 3;

        return tempBuf;
    }

    /* Make path to file shorter for "lvl" levels. */
    std::wstring FileShortName(std::wstring& filename, int lvl)
    {
        static TCHAR tempBuf[1024] = { 0 };

        int c = 0;
        size_t i = 0;

        TCHAR* ptr = (TCHAR*)filename.data();

        tempBuf[0] = ptr[0];
        tempBuf[1] = ptr[1];
        tempBuf[2] = ptr[2];

        ptr += 3;

        for (i = _tcslen(ptr) - 1; i; i--)
        {
            if (ptr[i] == _T('\\')) c++;
            if (c == lvl) break;
        }

        if (c == lvl)
        {
            _stprintf_s(&tempBuf[3], _countof(tempBuf) - 3, _T("...%s"), &ptr[i]);
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
