#pragma once

// banner filename. must reside in DVD root.
constexpr auto DVD_BANNER_FILENAME = "opening.bnr";
constexpr auto DVD_BANNER_ID        = 'BNR1';   // JP/US
constexpr auto DVD_BANNER_ID2       = 'BNR2';   // EU
constexpr auto DVD_BANNER_WIDTH = 96;
constexpr auto DVD_BANNER_HEIGHT = 32;

/* There two formats of banner file - USA/Japan and European.                   */
/* the difference is that EUR version has comments on six common european       */ 
/* languages : English, Deutsch, Francais, Espanol, Italiano and Nederlands.    */

/* The base class */
struct DVDBannerBase {};

/* JP/NTSC version. */
struct DVDBannerNTSC : public DVDBannerBase
{
    uint32_t  id;                                                /* 'BNR1' */
    uint32_t  padding[7];                               
    uint8_t   image[2 * DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT];   /* RGB5A3 texture */
    uint8_t   shortTitle[32];                                    /* game name (short, for IPL menu) */
    uint8_t   shortMaker[32];                                    /* developer */
    uint8_t   longTitle[64];                                     /* game name (long, for Dolwin =:)) */
    uint8_t   longMaker[64];                                     /* developer (long description) */
    uint8_t   comment[128];                                      /* comments. may include '\n' */
};

/* PAL version. */
struct DVDBannerPAL : public DVDBannerBase
{
    uint32_t   id;                                              /* 'BNR2' */
    uint32_t   padding[7];
    uint8_t    image[2 * DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT]; /* RGB5A3 texture */

    /* Comments on six european languages. */
    struct
    {
        uint8_t  shortTitle[32];
        uint8_t  shortMaker[32];
        uint8_t  longTitle[64];
        uint8_t  longMaker[64];
        uint8_t  comment[128];
    } comments[6];
};

/* DVD Banner API. */
std::unique_ptr<DVDBannerPAL> DVDLoadBanner(std::wstring_view dvdFile);
uint8_t DVDBannerChecksum(DVDBannerBase* banner);
