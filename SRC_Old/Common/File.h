#pragma once

namespace Util
{

    // Get the size of a file.

    size_t FileSize(const std::string& filename);
    size_t FileSize(const std::wstring& filename);
    size_t FileSize(const wchar_t* filename);

    // Check whenever the file exists

    bool FileExists(const std::string& filename);
    bool FileExists(const std::wstring& filename);
    bool FileExists(const wchar_t* filename);

    // Load data from a file

    std::vector<uint8_t> FileLoad(const std::string& filename);
    std::vector<uint8_t> FileLoad(const std::wstring& filename);
    std::vector<uint8_t> FileLoad(const wchar_t* filename);

    // Save data to file

    bool FileSave(const std::string& filename, std::vector<uint8_t> & data);
    bool FileSave(const std::wstring& filename, std::vector<uint8_t>& data);
    bool FileSave(const wchar_t* filename, std::vector<uint8_t>& data);

    void SplitPath(const char* _Path,
        char* _Drive,
        char* _Dir,
        char* _Filename,
        char* _Ext);

    // Get a list of files and directories, relative to the root directory

    void BuildFileTree(std::wstring rootDir, std::list<std::wstring>& names);

    // Check if the entity is a directory or a file.

    bool IsDirectory(std::wstring path);

}
