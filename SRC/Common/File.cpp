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

}
