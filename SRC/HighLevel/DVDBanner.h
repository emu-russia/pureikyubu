// banner filename. must reside in DVD root.
#define DVD_BANNER_FILENAME     "opening.bnr"

#define DVD_BANNER_WIDTH        96
#define DVD_BANNER_HEIGHT       32

#define DVD_BANNER_ID           'BNR1'  // JP/US
#define DVD_BANNER_ID2          'BNR2'  // EU

// there two formats of banner file - US/Japan and European.
// the difference is that EUR version has comments on six common european
// languages : English, Deutsch, Francais, Espanol, Italiano and Nederlands.

// JP/US version
typedef struct DVDBanner
{
    u32     id;                         // 'BNR1'
    u32     padding[7];
    u8      image[2 * DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT]; // RGB5A3 texture
    u8      shortTitle[32];             // game name (short, for IPL menu)
    u8      shortMaker[32];             // developer
    u8      longTitle[64];              // game name (long, for Dolwin =:))
    u8      longMaker[64];              // developer (long description)
    u8      comment[128];               // comments. may include '\n'
} DVDBanner;

// EUR version
typedef struct DVDBanner2
{
    u32     id;                         // 'BNR2'
    u32     padding[7];
    u8      image[2 * DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT]; // RGB5A3 texture

    // comments on six european languages
    struct
    {
        u8  shortTitle[32];
        u8  shortMaker[32];
        u8  longTitle[64];
        u8  longMaker[64];
        u8  comment[128];
    } comments[6];
} DVDBanner2;

// banner API
void*   DVDLoadBanner(char *dvdFile);           // free() required!
u8      DVDBannerChecksum(void *banner);
