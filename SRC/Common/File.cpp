#include "pch.h"

namespace Util
{
    size_t FileSize(const std::wstring& filename)
    {
        FILE* f;
#ifdef _LINUX
        f = fopen(Util::WstringToString(filename).c_str(), "rb");
#else
        _wfopen_s(&f, filename.c_str(), L"rb");
#endif
        if (!f)
            return 0;

        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        fclose(f);

        return size;
    }

    size_t FileSize(const std::string& filename)
    {
        std::wstring wstr = StringToWstring(filename);
        return FileSize(wstr);
    }

    size_t FileSize(const wchar_t* filename)
    {
        std::wstring wstr(filename);
        return FileSize(wstr);
    }

    bool FileExists(const std::wstring& filename)
    {
        FILE* f;
#ifdef _LINUX
        f = fopen(Util::WstringToString(filename).c_str(), "rb");
#else
        _wfopen_s(&f, filename.c_str(), L"rb");
#endif
        if (!f)
            return false;
        fclose(f);
        return true;
    }

    bool FileExists(const std::string& filename)
    {
        std::wstring wstr = StringToWstring(filename);
        return FileExists(wstr);
    }

    bool FileExists(const wchar_t* filename)
    {
        std::wstring wstr(filename);
        return FileExists(wstr);
    }

    std::vector<uint8_t> FileLoad(const std::wstring& filename)
    {
        if (!FileExists(filename))
        {
            return std::vector<uint8_t>();
        }
        
        size_t size = FileSize(filename);

        uint8_t* data = new uint8_t[size];

        FILE* f; 
#ifdef _LINUX
        f = fopen(Util::WstringToString(filename).c_str(), "rb");
#else
        _wfopen_s(&f, filename.c_str(), L"rb");
#endif

        fread(data, 1, size, f);
        fclose(f);

		std::vector<uint8_t> output(data, data + size);

		delete[] data;

        return output;
    }

    std::vector<uint8_t> FileLoad(const std::string& filename)
    {
        std::wstring wstr = StringToWstring(filename);
        return FileLoad(wstr);
    }

    std::vector<uint8_t> FileLoad(const wchar_t* filename)
    {
        std::wstring wstr(filename);
        return FileLoad(wstr);
    }

    bool FileSave(const std::wstring& filename, std::vector<uint8_t>& data)
    {
        FILE* f;
#ifdef _LINUX
        f = fopen(Util::WstringToString(filename).c_str(), "rb");
#else
        _wfopen_s(&f, filename.c_str(), L"wb");
#endif
        if (!f)
            return false;

        fwrite(data.data(), 1, data.size(), f);
        fclose(f);

        return true;
    }

    bool FileSave(const std::string& filename, std::vector<uint8_t>& data)
    {
        std::wstring wstr = StringToWstring(filename);
        return FileSave(wstr, data);
    }

    bool FileSave(const wchar_t* filename, std::vector<uint8_t>& data)
    {
        std::wstring wstr(filename);
        return FileSave(wstr, data);
    }

    void SplitPath(const char* _Path,
        char* _Drive,
        char* _Dir,
        char* _Filename,
        char* _Ext)
    {

#if defined(_WINDOWS) || defined(_PLAYGROUND_WINDOWS)
        _splitpath(_Path, _Drive, _Dir, _Filename, _Ext);
#endif

#if defined (_LINUX)

        _Drive[0] = 0;

        char filename[0x1000] = { 0, };

        char* base = basename((char *)_Path);

        if (base)
        {
            strcpy(_Filename, base);
            strcpy(_Ext, base);

            char* fnamePtr = strchr(_Filename, '.');
            if (fnamePtr)
            {
                *fnamePtr = 0;
            }
            else
            {
                _Filename[0] = 0;
            }

            char * extPtr = strrchr(_Ext, '.');
            if (extPtr)
            {
                *extPtr = 0;
            }
            else
            {
                _Ext[0] = 0;
            }
        }
        else
        {
            _Filename[0] = 0;
            _Ext[0] = 0;
        }

        char* dir = dirname((char*)_Path);

        if (dir)
        {
            strcpy(_Dir, dir);
        }
        else
        {
            _Dir[0] = 0;
        }


#endif

    }

    /// <summary>
    /// Get a list of files and directories, relative to the root directory.
    /// WARNING! This method is recursive. You must understand what you are doing.
    /// </summary>
    /// <param name="rootDir">Directory relative to which the tree will be built</param>
    /// <param name="names">List of files and directories. The path includes the root directory. 
    /// If the root directory is a full path, then the path in this list to the directory/file will also be full. Otherwise, the paths are relative (but include the root directory).</param>
    void BuildFileTree(std::wstring rootDir, std::list<std::wstring>& names)
    {

        if (rootDir.back() == L'/')
        {
            rootDir.pop_back();
        }

#if defined(_WINDOWS) || defined(_PLAYGROUND_WINDOWS)

        std::wstring search_path = rootDir + L"/*.*";
        WIN32_FIND_DATAW fd = { 0 };
        HANDLE hFind = ::FindFirstFileW(search_path.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                std::wstring name = fd.cFileName;

                if (name == L"." || name == L"..")
                    continue;

                std::wstring fullPath = rootDir + L"/" + name;

                names.push_back(fullPath);

                if (Util::IsDirectory(fullPath))
                {
                    BuildFileTree(fullPath, names);
                }

            } while (::FindNextFileW(hFind, &fd));

            ::FindClose(hFind);
        }

#endif

#if defined (_LINUX)

        DIR* dir;
        struct dirent* ent;
        if ((dir = opendir(Util::WstringToString(rootDir).c_str())) != NULL)
        {
            while ((ent = readdir(dir)) != NULL)
            {
                std::wstring name = Util::StringToWstring(ent->d_name);

                if (name == L"." || name == L"..")
                    continue;

                std::wstring fullPath = rootDir + L"/" + name;

                names.push_back(fullPath);

                if (Util::IsDirectory(fullPath))
                {
                    BuildFileTree(fullPath, names);
                }
            }
            closedir(dir);
        }

#endif


    }

    /// <summary>
    /// Check if the entity is a directory or a file.
    /// </summary>
    /// <param name="fullPath">Path to directory or file (can be relative).</param>
    /// <returns>true: The specified entity is a directory.</returns>
    bool IsDirectory(std::wstring path)
    {

#if defined(_WINDOWS) || defined(_PLAYGROUND_WINDOWS)

        DWORD attr = ::GetFileAttributesW(path.c_str());

        return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;

#endif

#if defined (_LINUX)

        // Year 2020. Linux still doesn't really know how to work with wchar_t

        struct stat attr;

        stat( Util::WstringToString(path).c_str(), &attr);

        return (attr.st_mode & S_IFDIR) != 0;

#endif

        return false;
    }



#if 0
    
    void BuildTreeDemo()
    {
        std::list<std::wstring> names;
        Util::BuildFileTree(L"c:/Work/DolphinSDK_Dvddata", names);
        for (auto it = names.begin(); it != names.end(); ++it)
        {
            std::wstring name = *it;

            if (Util::IsDirectory(name))
            {
                Debug::Report(Debug::Channel::Norm, "Dir: %s\n", Util::WstringToString(name).c_str());
            }
            else
            {
                Debug::Report(Debug::Channel::Norm, "File: %s\n", Util::WstringToString(name).c_str());
            }
        }
    }

#endif

}
