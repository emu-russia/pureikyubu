// DVD banner helpers for file selector. 
#include "pch.h"

std::vector<uint8_t> DVDLoadBanner(const wchar_t* dvdFile)
{
    size_t fsize = Util::FileSize(dvdFile);
    uint32_t bnrofs = 0;

    std::vector<uint8_t> banner;
    banner.resize(sizeof(DVDBanner2));

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
        UI::Jdi->DvdRead(banner);
    }
    else
    {
        return std::vector<uint8_t>();
    }

    return banner;
}
