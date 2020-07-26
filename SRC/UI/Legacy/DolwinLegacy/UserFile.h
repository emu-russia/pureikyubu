/* UI file utilities API. */
#pragma once

namespace UI
{
    enum class FileType
    {
        All = 1,
        Dvd,
        Map,
        Json,
        Directory,
    };
    
    /* Open/save a file dialog. */
    const TCHAR* FileOpenDialog(HWND hwnd, FileType type);
    const TCHAR* FileSaveDialog(HWND hwnd, FileType type);
    
    std::wstring FileShortName(const std::wstring& filename, int lvl = 3);
    std::wstring FileSmartSize(size_t size);
    std::string FileSmartSizeA(size_t size);
};
