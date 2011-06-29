// DVD banner helpers for file selector. 
// we are using DVD plugin for disk operations.
#include "dolphin.h"

// banner for damaged DVD images.
#include "nobanner.h"

void * DVDLoadBanner(char *dvdFile)
{
    s32 fsize = FileSize(dvdFile);
    DVDBanner2 * banner;
    u32 bnrofs;

    banner = (DVDBanner2 *)malloc(sizeof(DVDBanner2));
    ASSERT(banner == NULL, "Not enough memory for DVD banner data.");

    // load DVD banner, or copy error-banner, if DVD is damaged
    if(fsize)
    {
        DVDSetCurrent(dvdFile);
        bnrofs = DVDOpenFile("/" DVD_BANNER_FILENAME);
    } else bnrofs = 0;
    if(bnrofs)
    {
        DVDSeek(bnrofs);
        DVDRead((u8 *)banner, sizeof(DVDBanner2));
    }
    else memcpy(banner, nobanner, sizeof(DVDBanner));

    return banner;
}

u8 DVDBannerChecksum(void *banner)
{
    DVDBanner*  bnr  = (DVDBanner *)banner;
    DVDBanner2* bnr2 = (DVDBanner2*)banner;
    u8*         buf;
    u32         sum  = 0;

    // select banner type
    if(MEMSwap(bnr->id) == DVD_BANNER_ID)   // US/JAP
    {
        buf = (u8 *)bnr;
        for(int i=0; i<sizeof(DVDBanner); i++)
        {
            sum += buf[i];
        }
    }
    else                                    // EUR
    {
        buf = (u8 *)bnr2;
        for(int i=0; i<sizeof(DVDBanner2); i++)
        {
            sum += buf[i];
        }
    }

    return (u8)sum;
}
