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
    size_t FileSize(const TCHAR * filename);
    void* FileLoad(const TCHAR * filename, size_t* size = nullptr);           // free!
    bool FileSave(const TCHAR * filename, void* data, size_t size);
    TCHAR* FileOpen(HWND hwnd, FileType type = FileType::All);        // copy away!
    TCHAR* FileShortName(const TCHAR * filename, int lvl = 3);           // copy away!
    TCHAR* FileSmartSize(size_t size);                            // copy away!

};
