#pragma once

namespace Util
{

    // Get the size of a file.

    size_t FileSize(std::string& filename);
    size_t FileSize(std::wstring& filename);
    size_t FileSize(TCHAR * filename);

    // Check whenever the file exists

    bool FileExists(std::string& filename);
    bool FileExists(std::wstring& filename);
    bool FileExists(TCHAR* filename);

    // Load data from a file

    std::vector<uint8_t> FileLoad(std::string& filename);
    std::vector<uint8_t> FileLoad(std::wstring& filename);
    std::vector<uint8_t> FileLoad(TCHAR* filename);

    // Save data to file

    bool FileSave(std::string& filename, std::vector<uint8_t> & data);
    bool FileSave(std::wstring& filename, std::vector<uint8_t>& data);
    bool FileSave(TCHAR* filename, std::vector<uint8_t>& data);

}
