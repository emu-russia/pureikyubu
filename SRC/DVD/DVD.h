// common include project header

#pragma once

#include <string>

// DVD interface

namespace DVD
{
    void InitSubsystem();
    void ShutdownSubsystem();

    // Mount current DVD image for read/seek/open file operations
    bool MountFile(const TCHAR* file);
    bool MountFile(std::string file);

    // Mount DolphinSDK directory
    bool MountSdk(const TCHAR* path);
    bool MountSdk(std::string path);

    // Unmount
    void Unmount();

    // Seek and read operations on mounted DVD
    void Seek(int position);
    void Read(void* buffer, size_t length);

    // Open file in DVD root. Return file position, or 0 if no such file.
    // Note: DVD must be mounted first!
    // example use : long banner = DVDOpenFile("/opening.bnr");
    long OpenFile(const char* dvdfile);
}

// other include files
#include "filesystem.h"     // DVD file system, based on hotquik's code from Dolwin 0.09
#include "MountSDK.h"

// all important data is placed here
typedef struct
{
    bool mountedImage;
    TCHAR gcm_filename[0x1000];
    int   gcm_size;       // size of mounted file
    int   seekval;        // current DVD position

    DVD::MountDolphinSdk* mountedSdk;
} DVDControl;

extern DVDControl dvd;             // share with other modules
