// Dolwin file utilities
#include "pch.h"
#include <shlobj.h>

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
    size_t FileSize(std::wstring_view filename)
    {
        auto f = std::ifstream(filename, std::ifstream::ate | std::ifstream::binary);
        
        if (!f.is_open())
            return 0;
        else
            return f.tellg();
    }

    // Load data from file
    void* FileLoad(const TCHAR * filename, size_t * size)
    {
        FILE* f;
        uint8_t* buffer;
        size_t  filesize;

        if (size) *size = 0;

        f = nullptr;
        _tfopen_s(&f, filename, _T("rb"));
        if (f == NULL) return NULL;

        fseek(f, 0, SEEK_END);
        filesize = ftell(f);
        fseek(f, 0, SEEK_SET);

        buffer = (uint8_t *)malloc(filesize + 1);
        if (buffer == NULL)
        {
            fclose(f);
            return NULL;
        }

        fread(buffer, filesize, 1, f);
        fclose(f);

        buffer[filesize] = 0;

        if (size) *size = filesize;
        return buffer;
    }

    // Load data from file
    void* FileLoad(const char* filename, size_t* size)
    {
        FILE* f;
        uint8_t* buffer;
        size_t  filesize;

        if (size) *size = 0;

        f = nullptr;
        fopen_s(&f, filename, "rb");
        if (f == NULL) return NULL;

        fseek(f, 0, SEEK_END);
        filesize = ftell(f);
        fseek(f, 0, SEEK_SET);

        buffer = (uint8_t*)malloc(filesize + 1);
        if (buffer == NULL)
        {
            fclose(f);
            return NULL;
        }

        fread(buffer, filesize, 1, f);
        fclose(f);

        buffer[filesize] = 0;

        if (size) *size = filesize;
        return buffer;
    }

    // Save data in file
    bool FileSave(const TCHAR * filename, void* data, size_t size)
    {
        FILE* f = nullptr;
        _tfopen_s (&f, filename, _T("wb"));
        if (f == NULL) return FALSE;

        fwrite(data, size, 1, f);
        fclose(f);
        return TRUE;
    }

    bool FileSave(const char* filename, void* data, size_t size)
    {
        FILE* f = nullptr;
        fopen_s(&f, filename, "wb");
        if (f == NULL) return FALSE;

        fwrite(data, size, 1, f);
        fclose(f);
        return TRUE;
    }

    // Open file/directory dialog
    TCHAR* FileOpen(HWND hwnd, FileType type)
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
                _tcscpy_s(lastDir, _countof(lastDir) - 1, GetConfigString(USER_LASTDIR_ALL, USER_UI).c_str());
                break;
            case FileType::Dvd:
                _tcscpy_s(lastDir, _countof(lastDir) - 1, GetConfigString(USER_LASTDIR_DVD, USER_UI).c_str());
                break;
            case FileType::Map:
                _tcscpy_s(lastDir, _countof(lastDir) - 1, GetConfigString(USER_LASTDIR_MAP, USER_UI).c_str());
                break;
            case FileType::Patch:
                _tcscpy_s(lastDir, _countof(lastDir) - 1, GetConfigString(USER_LASTDIR_PATCH, USER_UI).c_str());
                break;
            default:
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
                case FileType::Patch:
                    ofn.lpstrFilter =
                        _T("Patch files (*.patch)\0*.patch\0")
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
                    SetConfigString(USER_LASTDIR_ALL, lastDir, USER_UI);
                    break;
                case FileType::Dvd:
                    SetConfigString(USER_LASTDIR_DVD, lastDir, USER_UI);
                    break;
                case FileType::Map:
                    SetConfigString(USER_LASTDIR_MAP, lastDir, USER_UI);
                    break;
                case FileType::Patch:
                    SetConfigString(USER_LASTDIR_PATCH, lastDir, USER_UI);
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
    TCHAR * FileShortName(const TCHAR * filename, int lvl)
    {
        static TCHAR tempBuf[1024] = { 0 };

        int c = 0;
        size_t i = 0;

        TCHAR * ptr = (TCHAR *)filename;

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

    // nice value of KB, MB or GB, for output
    TCHAR * FileSmartSize(size_t size)
    {
        static TCHAR tempBuf[1024] = { 0 };

        if (size < 1024)
        {
            _stprintf_s(tempBuf, sizeof(tempBuf), _T("%zi byte"), size);
        }
        else if (size < 1024 * 1024)
        {
            _stprintf_s(tempBuf, sizeof(tempBuf), _T("%zi KB"), size / 1024);
        }
        else if (size < 1024 * 1024 * 1024)
        {
            _stprintf_s(tempBuf, sizeof(tempBuf), _T("%zi MB"), size / 1024 / 1024);
        }
        else _stprintf_s(tempBuf, sizeof(tempBuf), _T("%1.1f GB"), (float)size / 1024 / 1024 / 1024);
        return tempBuf;
    }

    char* FileSmartSizeA(size_t size)
    {
        static char tempBuf[1024] = { 0 };

        if (size < 1024)
        {
            sprintf_s(tempBuf, sizeof(tempBuf), "%zi byte", size);
        }
        else if (size < 1024 * 1024)
        {
            sprintf_s(tempBuf, sizeof(tempBuf), "%zi KB", size / 1024);
        }
        else if (size < 1024 * 1024 * 1024)
        {
            sprintf_s(tempBuf, sizeof(tempBuf), "%zi MB", size / 1024 / 1024);
        }
        else sprintf_s(tempBuf, sizeof(tempBuf), "%1.1f GB", (float)size / 1024 / 1024 / 1024);
        return tempBuf;
    }

    void* FileLoad(std::wstring filename, size_t& size)
    {
        FILE* f;
        uint8_t* buffer;
        size_t  filesize;

        size = 0;

        f = nullptr;
        _wfopen_s(&f, filename.c_str(), L"rb");
        if (f == NULL) return NULL;

        fseek(f, 0, SEEK_END);
        filesize = ftell(f);
        fseek(f, 0, SEEK_SET);

        buffer = new uint8_t[filesize + 1];
        if (buffer == NULL)
        {
            fclose(f);
            return NULL;
        }

        fread(buffer, filesize, 1, f);
        fclose(f);

        buffer[filesize] = 0;

        size = filesize;
        return buffer;
    }

    bool FileSave(std::wstring filename, void* data, size_t size)
    {
        FILE* f = nullptr;
        _wfopen_s(&f, filename.c_str(), L"wb");
        if (f == NULL) return FALSE;

        fwrite(data, size, 1, f);
        fclose(f);
        return TRUE;
    }

}
