// DVD banner helpers for file selector. 
#include "pch.h"

void* DVDLoadBanner(TCHAR* dvdFile)
{
    size_t fsize = UI::FileSize(dvdFile);
    DVDBanner2* banner = nullptr;
    uint32_t bnrofs = 0;

    banner = (DVDBanner2*)malloc(sizeof(DVDBanner2));
    assert(banner);

    // load DVD banner
    if (fsize)
    {
        if (DVD::MountFile(dvdFile))
        {
            bnrofs = DVD::OpenFile("/" DVD_BANNER_FILENAME);
        }
    }

    if (bnrofs)
    {
        DVD::Seek(bnrofs);
        DVD::Read((uint8_t*)banner, sizeof(DVDBanner2));
    }
    else
    {
        free(banner);
        banner = nullptr;
    }

    return banner;
}

uint8_t DVDBannerChecksum(void* banner)
{
    DVDBanner* bnr = (DVDBanner*)banner;
    DVDBanner2* bnr2 = (DVDBanner2*)banner;
    uint8_t* buf;
    uint32_t         sum = 0;

    // select banner type
    if (_byteswap_ulong(bnr->id) == DVD_BANNER_ID)   // US/JAP
    {
        buf = (uint8_t*)bnr;
        for (int i = 0; i < sizeof(DVDBanner); i++)
        {
            sum += buf[i];
        }
    }
    else                                    // EUR
    {
        buf = (uint8_t*)bnr2;
        for (int i = 0; i < sizeof(DVDBanner2); i++)
        {
            sum += buf[i];
        }
    }

    return (uint8_t)sum;
}
