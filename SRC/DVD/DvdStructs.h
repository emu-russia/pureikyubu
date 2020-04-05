// GAMECUBE Disk Structures

#pragma once

// size of DVD image
#define DVD_SIZE            0x57058000  // 1.4 GB

// Sector size
#define DVD_SECTOR_SIZE     2048

// DiskID

#define DVD_DISKID_MAGIC 0xC2339F3D

typedef struct _DiskID
{
    char gameName[4];
    char company[2];
    uint8_t diskNumber;
    uint8_t gameVersion;
    uint8_t streaming;
    uint8_t streamingBufSize;
    uint8_t padding[18];
    uint32_t magicNumber;       // DVD_DISKID_MAGIC
} DiskID;

//
// general DVD tables (BB2, BI2)
//

// offsets
#define DVD_ID_OFFSET       0x0000  // disk id
#define DVD_GAMENAME_OFFSET 0x0020  // Game name (0x400) bytes
#define DVD_BB2_OFFSET      0x0420  // BB2
#define DVD_BI2_OFFSET      0x0440  // BI2
#define DVD_APPLDR_OFFSET   0x2440  // apploader

typedef struct _DVDBB2
{
    uint32_t     bootFilePosition;          // Where DOL executable is 
    uint32_t     FSTPosition;
    uint32_t     FSTLength;
    uint32_t     FSTMaxLength;
    uint32_t     userPosition;          // FST location in memory. A strange architectural solution, one could do OSAlloc.
    uint32_t     userLength;            // FST size in memory
    uint8_t      padding[8];
} DVDBB2;

// BI2 is omitted here..

//
// file string table (FST):  { [FST Entry] [FST Entry] ... } { NameTable }
//

#define DVD_FST_MAX_SIZE 0x00100000 // 1 mb
#define DVD_MAXPATH      256        // Complete path length, including root /

#pragma pack(push, 1)

#pragma warning (push)
#pragma warning (disable: 4201)

typedef struct _DVDFileEntry
{
    uint8_t      isDir;                  // 1, if directory
    uint8_t      nameOffsetHi;      // Relative to Name Table start
    uint16_t     nameOffsetLo;
    union
    {
        struct                      // file
        {
            uint32_t     fileOffset;        // Relative to disk start (0)
            uint32_t     fileLength;        // In bytes
        };
        struct                      // directory
        {
            uint32_t     parentOffset;   // parent directory FST index
            uint32_t     nextOffset;     // next directory FST index
        };
    };
} DVDFileEntry;

// Additional information: FSTNotes.md

#pragma warning (pop)		// warning C4201: nonstandard extension used: nameless struct/union

#pragma pack(pop)
