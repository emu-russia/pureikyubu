// Dolwin file utilities
#include "pch.h"

namespace UI
{
    size_t FileSize(std::wstring_view filename)
    {
        if (!FileExists(filename))
            return 0;

        auto path = std::filesystem::absolute(filename);
        return std::filesystem::file_size(path);
    }

    bool FileExists(std::wstring_view filename)
    {
        auto path = std::filesystem::absolute(filename);
        return std::filesystem::exists(path);
    }

    /* Load data from a unicode file. */
    std::vector<uint8_t> FileLoad(std::wstring_view filename)
    {
        auto file = std::ifstream(filename, std::ifstream::binary);
        if (!file.is_open())
        {
            return std::vector<uint8_t>();
        }
        else
        {
            auto itr = std::istreambuf_iterator<char>(file);
            return std::vector<uint8_t>(itr, std::istreambuf_iterator<char>());
        }
    }

    /* Load data from a file. */
    std::vector<uint8_t> FileLoad(std::string_view filename)
    {
        auto file = std::ifstream(filename, std::ifstream::binary);
        if (!file.is_open())
        {
            return std::vector<uint8_t>();
        }
        else
        {
            return std::vector<uint8_t>(std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>());
        }
    }

    /* Save data in file. */
    bool FileSave(std::wstring_view filename, std::vector<uint8_t>& data)
    {
        auto file = std::ofstream(filename, std::ofstream::binary);
        if (!file.is_open())
        {
            return false;
        }
        else
        {
            auto itr = std::ostreambuf_iterator<char>(file);
            std::copy(data.begin(), data.end(), itr);

            return true;
        }
    }

    bool FileSave(std::string_view filename, std::vector<uint8_t>& data)
    {
        auto file = std::ofstream(filename, std::ofstream::binary);
        if (!file.is_open())
        {
            return false;
        }
        else
        {
            auto itr = std::ostreambuf_iterator<char>(file);
            std::copy(data.begin(), data.end(), itr);

            return true;
        }
    }

    /* Open the file/directory dialog. */
    std::wstring FileOpen(FileType type)
    {
        static auto tempStr = std::wstring(0x1000, 0);
        std::wstring szFileName(1024, 0), szFileTitle(1024, 0);

        auto prevDir = std::filesystem::current_path();
        switch (type)
        {
            case FileType::All:
            {
                auto lastDir = GetConfigString(USER_LASTDIR_ALL, USER_UI);
                break;
            }
            case FileType::Dvd:
            {
                auto lastDir = GetConfigString(USER_LASTDIR_DVD, USER_UI);
                break;
            }
            case FileType::Map:
            {
                auto lastDir = GetConfigString(USER_LASTDIR_MAP, USER_UI);
                break;
            }
            case FileType::Patch:
            {
                auto lastDir = GetConfigString(USER_LASTDIR_PATCH, USER_UI);
                break;
            }
            case FileType::Json:
            {
                auto lastDir = GetConfigString(USER_LASTDIR_PATCH, USER_UI);
                break;                
            }
            default:
            {
                break;
            }
        }

        bool result = false;
        if (type == FileType::Directory)
        {          
            auto title = L"Choose a directory";
            auto folder = Win::OpenDialog(title, L"", true);

            if (!folder.empty())
            {
                szFileName = folder;
                result = true;
            }
        }
        else
        {
            /* Select which files to allow in the dialog. */
            auto filter = std::wstring();
            switch (type)
            {
                case FileType::All:
                {
                    filter =
                        L"All Supported Files (*.dol, *.elf, *.bin, *.gcm, *.iso)\0*.dol;*.elf;*.bin;*.gcm;*.iso\0"
                        L"GameCube Executable Files (*.dol, *.elf)\0*.dol;*.elf\0"
                        L"Binary Files (*.bin)\0*.bin\0"
                        L"GameCube DVD Images (*.gcm, *.iso)\0*.gcm;*.iso\0"
                        L"All Files (*.*)\0*.*\0";
                    break;
                }
                case FileType::Dvd:
                {
                    filter =
                        L"GameCube DVD Images (*.gcm, *.iso)\0*.gcm;*.iso\0"
                        L"All Files (*.*)\0*.*\0";
                    break;
                }
                case FileType::Map:
                {
                    filter =
                        L"Symbolic information files (*.map)\0*.map\0"
                        L"All Files (*.*)\0*.*\0";
                    break;
                }
                case FileType::Patch:
                {
                    filter =
                        L"Patch files (*.patch)\0*.patch\0"
                        L"All Files (*.*)\0*.*\0";
                    break;
                }
                case FileType::Json:
                {
                    filter =
                        L"Json files (*.json)\0*.json\0"
                        L"All Files (*.*)\0*.*\0";
                    break;                
                }
                default:
                {
                    break;
                }
            }
            
            auto title = L"Open File";
            auto file = Win::OpenDialog(title, filter);

            if (!file.empty())
            {
                szFileName = file;
                result = true;
            }
        }

        if (result)
        {
            tempStr = szFileName;

            /* Save last directory. */
            auto lastDir = tempStr;
            
            /* Remove the file from the directory string. */
            int i = lastDir.size() - 1;
            while (lastDir[i] != L'\\') i--;

            lastDir[i + 1] = L'\0';
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
                case FileType::Patch:
                    SetConfigString(USER_LASTDIR_PATCH, lastDir, USER_UI);
                    break;
            }

            chdir(prevDir.string().c_str());
            return tempStr;
        }
        else
        {
            chdir(prevDir.string().c_str());
            return std::wstring();
        }
    }

    // Save file dialog
    TCHAR * FileSaveDialog(HWND hwnd, FileType type)
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
            case FileType::Patch:
                _tcscpy_s(lastDir, _countof(lastDir) - 1, GetConfigString(USER_LASTDIR_PATCH, USER_UI));
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
                case FileType::Patch:
                    ofn.lpstrFilter =
                        _T("Patch files (*.patch)\0*.patch\0")
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
                case FileType::Patch:
                    SetConfigString(USER_LASTDIR_PATCH, lastDir, USER_UI);
                    break;
                default:
                    break;
            }
            
            chdir(prevDir.string().c_str());
            return tempStr;
        }
        else
        {
            chdir(prevDir.string().c_str());
            return std::wstring();
        }
    }

    /* Make path to file shorter for "lvl" levels. */
    std::wstring FileShortName(std::wstring_view filename, int lvl)
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
