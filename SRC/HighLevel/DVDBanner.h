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
    uint32_t     id;                         // 'BNR1'
    uint32_t     padding[7];
    uint8_t      image[2 * DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT]; // RGB5A3 texture
    uint8_t      shortTitle[32];             // game name (short, for IPL menu)
    uint8_t      shortMaker[32];             // developer
    uint8_t      longTitle[64];              // game name (long, for Dolwin =:))
    uint8_t      longMaker[64];              // developer (long description)
    uint8_t      comment[128];               // comments. may include '\n'
} DVDBanner;

// EUR version
typedef struct DVDBanner2
{
    uint32_t     id;                         // 'BNR2'
    uint32_t     padding[7];
    uint8_t      image[2 * DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT]; // RGB5A3 texture

    // comments on six european languages
    struct
    {
        uint8_t  shortTitle[32];
        uint8_t  shortMaker[32];
        uint8_t  longTitle[64];
        uint8_t  longMaker[64];
        uint8_t  comment[128];
    } comments[6];
} DVDBanner2;

// banner API
void*   DVDLoadBanner(char *dvdFile);           // free() required!
uint8_t DVDBannerChecksum(void *banner);
