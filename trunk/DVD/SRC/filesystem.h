// filesystem definitions

//
// general DVD tables (BB2, BI2)
//

// offsets
#define DVD_ID_OFFSET       0x0000  // disk id
#define DVD_BB2_OFFSET      0x0420  // BB2
#define DVD_BI2_OFFSET      0x0440  // BI2
#define DVD_APPLDR_OFFSET   0x2440  // apploader

typedef struct
{
    u32     bootFilePosition;
    u32     FSTPosition;
    u32     FSTLength;
    u32     FSTMaxLength;
    u32     userPosition;
    u32     userLength;
    u8      padding[8];
} DVDBB2;

// BI2 is omitted here..

//
// file string table (FST)
//

#define DVD_FST_MAX_SIZE 0x00100000 // 1 mb
#define DVD_MAXPATH      256        // path length

typedef struct
{
    u8      isDir;                  // 1, if directory
    u8      nameOffsetHi;
    u16     nameOffsetLo;
    union
    {
        struct                      // file
        {
            u32     fileOffset;
            u32     fileLength;
        };
        struct                      // directory
        {
            u32     parentOffset;   // previous
            u32     nextOffset;     // next
        };
    };
} DVDFileEntry;

// externals
BOOL    dvd_fs_init();
s32     dvd_open(char *path, DVDFileEntry *root=NULL);
