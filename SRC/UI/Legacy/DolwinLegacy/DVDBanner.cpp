// DVD banner helpers for file selector. 
#include "pch.h"

std::vector<uint8_t> DVDLoadBanner(const wchar_t* dvdFile)
{
    size_t fsize = Util::FileSize(dvdFile);
    uint32_t bnrofs = 0;

    std::vector<uint8_t> banner;
    banner.resize(sizeof(DVDBanner2));

    bool mounted = false;
    std::string path;
    bool mountedAsIso = false;

    // Keep previous mount state

    mounted = UI::Jdi->DvdIsMounted(path, mountedAsIso);

    // load DVD banner
    if (fsize)
    {
        if (UI::Jdi->DvdMount( Util::WstringToString(dvdFile)))
        {
            bnrofs = UI::Jdi->DvdOpenFile("/" DVD_BANNER_FILENAME);
        }
    }

    if (bnrofs)
    {
        UI::Jdi->DvdSeek (bnrofs);
        UI::Jdi->DvdRead (banner);
    }
    else
    {
        banner.resize(0);
    }

    // Restore previous mount state

    if (mounted)
    {
        if (mountedAsIso)
        {
            UI::Jdi->DvdMount(path);
        }
        else
        {
            UI::Jdi->DvdMountSDK(path);
        }
    }
    else
    {
        UI::Jdi->DvdUnmount();
    }

    return banner;
}
