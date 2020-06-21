// Dolwin file utilities API

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

    // dont forget to :

    bool FileExists(const TCHAR* filename);
    size_t FileSize(std::wstring_view filename);
    void* FileLoad(const TCHAR * filename, size_t* size = nullptr);           // free!
    void* FileLoad(const char* filename, size_t* size = nullptr);
    bool FileSave(const TCHAR * filename, void* data, size_t size);
    bool FileSave(const char* filename, void* data, size_t size);
    TCHAR* FileOpen(HWND hwnd, FileType type = FileType::All);        // copy away!
    TCHAR* FileShortName(const TCHAR * filename, int lvl = 3);           // copy away!
    TCHAR* FileSmartSize(size_t size);                            // copy away!
    char* FileSmartSizeA(size_t size);                            // copy away!

    void* FileLoad(std::wstring filename, size_t& size);           // delete []
    bool FileSave(std::wstring filename, void* data, size_t size);

};
