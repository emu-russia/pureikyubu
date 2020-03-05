// DVD banner helpers for file selector. 
// we are using DVD plugin for disk operations.
#include "dolphin.h"

// banner for damaged DVD images.
#include "nobanner.h"

void * DVDLoadBanner(char *dvdFile)
{
    int fsize = FileSize(dvdFile);
    DVDBanner2 * banner;
    uint32_t bnrofs;

    banner = (DVDBanner2 *)malloc(sizeof(DVDBanner2));
    VERIFY(banner == NULL, "Not enough memory for DVD banner data.");

    // load DVD banner, or copy error-banner, if DVD is damaged
    if(fsize)
    {
        DVDSetCurrent(dvdFile);
        bnrofs = DVDOpenFile("/" DVD_BANNER_FILENAME);
    } else bnrofs = 0;
    if(bnrofs)
    {
        DVDSeek(bnrofs);
        DVDRead((uint8_t *)banner, sizeof(DVDBanner2));
    }
    else memcpy(banner, nobanner, sizeof(DVDBanner));

    return banner;
}

uint8_t DVDBannerChecksum(void *banner)
{
    DVDBanner*  bnr  = (DVDBanner *)banner;
    DVDBanner2* bnr2 = (DVDBanner2*)banner;
    uint8_t*         buf;
    uint32_t         sum  = 0;

    // select banner type
    if(MEMSwap(bnr->id) == DVD_BANNER_ID)   // US/JAP
    {
        buf = (uint8_t *)bnr;
        for(int i=0; i<sizeof(DVDBanner); i++)
        {
            sum += buf[i];
        }
    }
    else                                    // EUR
    {
        buf = (uint8_t *)bnr2;
        for(int i=0; i<sizeof(DVDBanner2); i++)
        {
            sum += buf[i];
        }
    }

    return (uint8_t)sum;
}
