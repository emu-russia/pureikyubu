/* Dolwin file utilities API. */
#pragma once
#include <vector>
#include <string>

namespace UI
{
    enum class FileType
    {
        All = 1,
        Dvd,
        Map,
        Patch,
        Directory,
    };
    
    size_t FileSize(std::wstring_view filename);
    bool FileExists(std::wstring_view filename);

    /* Open files with directory "filename" */
    std::vector<uint8_t> FileLoad(std::string_view filename);
    std::vector<uint8_t> FileLoad(std::wstring_view filename);

    /* Write files to direct "filename" with data. */
    bool FileSave(std::wstring_view filename, std::vector<uint8_t>& data);
    bool FileSave(std::string_view filename, std::vector<uint8_t>& data);
    
    /* Open a file dialog. */
    std::wstring FileOpen(FileType type = FileType::All);
    
    std::wstring FileShortName(std::wstring_view filename, int lvl = 3);
    std::wstring FileSmartSize(size_t size);
    std::string FileSmartSizeA(size_t size);
};
