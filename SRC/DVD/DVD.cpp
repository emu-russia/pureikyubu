// DVD API for emulator
#include "pch.h"

DVDControl dvd;

namespace DVD
{
    // Mount current dvd 
    bool MountFile(const TCHAR * file)
    {
        // try to open file
        if (!Util::FileExists(file))
        {
            return false;
        }

        Unmount();

        // select current DVD
        bool res = GCMMountFile(file);
        if (!res)
            return res;

        // init filesystem
        res = dvd_fs_init();
        if (!res)
        {
            GCMMountFile(nullptr);
            return res;
        }

        Seek(0);

        return true;
    }

    bool MountFile(const std::string& file)
    {
        TCHAR path[0x1000] = { 0, };
        TCHAR* tcharPtr = path;
        char* ansiPtr = (char *)file.c_str();
        while (*ansiPtr)
        {
            *tcharPtr++ = *ansiPtr++;
        }
        *tcharPtr++ = 0;
        return MountFile(path);
    }

    bool MountFile(const std::wstring& file)
    {
        TCHAR path[0x1000] = { 0, };
        TCHAR* tcharPtr = path;
        wchar_t* widePtr = (wchar_t*)file.c_str();
        while (*widePtr)
        {
            *tcharPtr++ = *widePtr++;
        }
        *tcharPtr++ = 0;
        return MountFile(path);
    }

    bool MountSdk(const TCHAR* path)
    {
        Unmount();

        dvd.mountedSdk = new MountDolphinSdk(path);

        if (!dvd.mountedSdk->Mounted())
        {
            delete dvd.mountedSdk;
            dvd.mountedSdk = nullptr;
            return false;
        }

        // init filesystem
        if (!dvd_fs_init())
        {
            delete dvd.mountedSdk;
            dvd.mountedSdk = nullptr;
            return false;
        }

        dvd.mountedSdk->Seek(0);

        return true;
    }

    bool MountSdk(std::string path)
    {
        TCHAR tcharStr[0x1000] = { 0, };
        TCHAR* tcharPtr = tcharStr;
        char* ansiPtr = (char*)path.c_str();
        while (*ansiPtr)
        {
            *tcharPtr++ = *ansiPtr++;
        }
        *tcharPtr++ = 0;
        return MountSdk(tcharStr);
    }

    // Unmount
    void Unmount()
    {
        GCMMountFile(nullptr);

        if (dvd.mountedSdk)
        {
            delete dvd.mountedSdk;
            dvd.mountedSdk = nullptr;
        }
    }

    bool IsMounted()
    {
        return (dvd.mountedImage || dvd.mountedSdk != nullptr);
    }

    // dvd operations on current mounted dvd

    void Seek(int position)
    {
        if (dvd.mountedImage)
        {
            GCMSeek(position);
        }
        else if (dvd.mountedSdk)
        {
            dvd.mountedSdk->Seek(position);
        }
    }

    int GetSeek()
    {
        if (dvd.mountedImage)
        {
            return dvd.seekval;
        }
        else if (dvd.mountedSdk)
        {
            return dvd.mountedSdk->GetSeek();
        }
        else return 0;
    }

    bool Read(void *buffer, size_t length)
    {
        if (length == 0)
            return true;

        if (dvd.mountedImage)
        {
            return GCMRead((uint8_t*)buffer, length);
        }
        else if (dvd.mountedSdk)
        {
            return dvd.mountedSdk->Read((uint8_t*)buffer, length);
        }
        else
        {
            memset(buffer, 0, length);        // fill by zeroes
        }

        return true;
    }

    long OpenFile(std::string& dvdfile)
    {
        if (dvd.mountedImage || dvd.mountedSdk)
        {
            // call DVD filesystem open
            return dvd_open(dvdfile.data());
        }

        // Not mounted
        return 0;
    }

    // Call somewhere

    void InitSubsystem()
    {
        JDI::Hub.AddNode(DDU_JDI_JSON, DvdCommandsReflector);

        DDU = new DduCore;
    }

    void ShutdownSubsystem()
    {
        Unmount();
        JDI::Hub.RemoveNode(DDU_JDI_JSON);
        delete DDU;
    }

}
