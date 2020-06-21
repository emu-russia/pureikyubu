// DVD banner helpers for file selector. 
#include "pch.h"

std::unique_ptr<DVDBannerPAL> DVDLoadBanner(std::wstring_view dvdFile)
{
    //size_t fsize = UI::FileSize(dvdFile);
    size_t file_size = std::filesystem::file_size(dvdFile);
    uint32_t bnrofs = 0;

    // load DVD banner
    auto banner = std::make_unique<DVDBannerPAL>();
    if (file_size > 0)
    {
        if (DVD::MountFile(dvdFile))
        {
            auto banner_name = fmt::format("/{:s}", DVD_BANNER_FILENAME);
            bnrofs = DVD::OpenFile(banner_name);
        }
    }

    if (bnrofs)
    {
        DVD::Seek(bnrofs);
        DVD::Read((uint8_t*)banner.get(), sizeof(DVDBannerPAL));
    }
    else
    {
        banner.reset(nullptr);
    }

    return std::move(banner);
}

uint8_t DVDBannerChecksum(DVDBannerBase* banner)
{
    DVDBannerNTSC*  bnr  = (DVDBannerNTSC*)banner;
    DVDBannerPAL*   bnr2 = (DVDBannerPAL*)banner;
    uint8_t*        buf;
    uint32_t        sum  = 0;

    /* Select banner type. */
    if(_byteswap_ulong(bnr->id) == DVD_BANNER_ID) /* NTSC / JP. */
    {
        buf = (uint8_t*)bnr;
        for(int i = 0; i < sizeof(DVDBannerNTSC); i++)
        {
            sum += buf[i];
        }
    }
    else /* PAL */
    {
        buf = (uint8_t*)bnr2;
        for(int i = 0; i < sizeof(DVDBannerPAL); i++)
        {
            sum += buf[i];
        }
    }

    return (uint8_t)sum;
}
