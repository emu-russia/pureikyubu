// DVD filesystem access.
#include "pch.h"

// local data
static DVDBB2           bb2;
static DVDFileEntry* FstStart;            // Loaded FST (byte-swapped as little-endian)
static uint32_t         fstSize;        // Size of loaded FST in bytes (not greater DVD_FST_MAX_SIZE)
static char* FstStringStart;          // Strings(name) table

#define FSTOFS(lo, hi) (((uint32_t)hi << 16) | lo)

// Swap longs (no need in assembly, used by tools)
static void SwapArea(uint32_t* addr, int count)
{
    uint32_t* until = addr + count / sizeof(uint32_t);

    while (addr != until)
    {
        *addr = _BYTESWAP_UINT32(*addr);
        addr++;
    }
}

// swap bytes in FST (little-endian)
// return beginning of strings table (or NULL, if bad FST)
static char *fst_prepare(DVDFileEntry *root)
{
    char* nameTablePtr = nullptr;

    root->nameOffsetLo = _BYTESWAP_UINT16(root->nameOffsetLo);
    root->fileOffset   = _BYTESWAP_UINT32(root->fileOffset);
    root->fileLength   = _BYTESWAP_UINT32(root->fileLength);

    // Check root: must have no parent, has zero nameOfset and non-zero nextOffset.
    if (! ( root->isDir && 
        root->parentOffset == 0 && 
        FSTOFS(root->nameOffsetLo, root->nameOffsetHi) == 0 && 
        root->nextOffset != 0 ) )
    {
        return nullptr;
    }

    nameTablePtr = (char*)&root[root->nextOffset];

    // Starting from next after root
    for (uint32_t i = 1; i < root->nextOffset; i++)
    {
        DVDFileEntry* entry = &root[i];

        entry->nameOffsetLo = _BYTESWAP_UINT16(entry->nameOffsetLo);
        entry->fileOffset = _BYTESWAP_UINT32(entry->fileOffset);
        entry->fileLength = _BYTESWAP_UINT32(entry->fileLength);
    }

    return nameTablePtr;
}

// initialize filesystem
bool dvd_fs_init()
{
    // Check DiskID

    char diskId[5] = { 0 };

    DVD::Seek(DVD_ID_OFFSET);
    DVD::Read(diskId, 4);

    for (int i = 0; i < 4; i++)
    {
        if (!isalnum(diskId[i]))
        {
            return false;
        }
    }

    // Check Apploader

    char apploaderBytes[5] = { 0 };

    DVD::Seek(DVD_APPLDR_OFFSET);
    DVD::Read(apploaderBytes, 4);

    for (int i = 0; i < 4; i++)
    {
        if (!isdigit(apploaderBytes[i]))
        {
            return false;
        }
    }

    // load tables
    DVD::Seek(DVD_BB2_OFFSET);
    DVD::Read(&bb2, sizeof(DVDBB2));
    SwapArea((uint32_t *)&bb2, sizeof(DVDBB2));

    // delete previous FST
    if(FstStart)
    {
        free(FstStart);
        FstStart = NULL;
        fstSize = 0;
    }

    // create new FST
    fstSize = bb2.FSTLength;
    if(fstSize > DVD_FST_MAX_SIZE)
    {
        return false;
    }
    FstStart = (DVDFileEntry *)malloc(fstSize);
    if(FstStart == NULL)
    {
        return false;
    }
    DVD::Seek(bb2.FSTPosition);
    DVD::Read(FstStart, fstSize);
        
    // swap bytes in FST and find offset of string table
    FstStringStart = fst_prepare(FstStart);
    if(!FstStringStart)
    {
        free(FstStart);
        FstStart = NULL;
        return false;
    }

    // FST loaded ok
    return true;
}

void dvd_fs_shutdown()
{
	if (FstStart)
	{
		free(FstStart);
		FstStart = NULL;
		fstSize = 0;
	}
}

// Based on reversing of original method.
// <0: Bad path
static int DVDConvertPathToEntrynum(const char* _path)
{
    char* path = (char *)_path;

    // currentDirectory assigned by DVDChangeDir
    int entry = 0;         // running entry

    // Loop1
    while (true)
    {
        if (path[0] == 0)
            return entry;

        // Current/parent directory walk

        if (path[0] == '/')
        {
            entry = 0;      // root
            path++;
            continue;   // Loop1
        }

        if (path[0] == '.')
        {
            if (path[1] == '.')
            {
                if (path[2] == '/')
                {
                    entry = FstStart[entry].parentOffset;
                    path += 3;
                    continue;   // Loop1
                }
                if (path[2] == 0)
                {
                    return FstStart[entry].parentOffset;
                }
            }
            else
            {
                if (path[1] == '/')
                {
                    path += 2;
                    continue;   // Loop1
                }
                if (path[1] == 0)
                {
                    return entry;
                }
            }
        }

        // Get a pointer to the end of a file or directory name (the end is 0 or /)
        char* endPathPtr;

        if (true)
        {
            endPathPtr = path;
            while (!(endPathPtr[0] == 0 || endPathPtr[0] == '/'))
            {
                endPathPtr++;
            }
        }

        // if-else Block 2

        bool afterNameCharNZ = endPathPtr[0] != 0;      // after-name character != 0
        int prevEntry = entry;          // Save previous entry
        size_t nameSize = endPathPtr - path;        // path element nameSize
        entry++;              // Increment entry

        // Loop2
        while (true)
        {
            if ((int)FstStart[prevEntry].nextOffset <= entry)   // Walk forward only
                return -1;      // Bad FST

            // Loop2 - Group 1  -- Compare names
            if (FstStart[entry].isDir || afterNameCharNZ == false /* after-name is 0 */)
            {
                char* r21 = path;      // r21 -- current pathPtr to inner loop
                int nameOffset = (FstStart[entry].nameOffsetHi << 16) | FstStart[entry].nameOffsetLo;
                char* r20 = &FstStringStart[nameOffset & 0xFFFFFF];     // r20 -- ptr to current entry name

                bool same;
                while (true)
                {
                    if (*r20 == 0)
                    {
                        same = (*r21 == '/' || *r21 == 0);
                        break;
                    }

                    if (_tolower(*r20++) != _tolower(*r21++))
                    {
                        same = false;
                        break;
                    }
                }

                if (same)
                {
                    if (afterNameCharNZ == false)
                        return entry;
                    path += nameSize + 1;
                    break;      // break Loop2
                }
            }

            // Walk next directory/file at same level
            entry = FstStart[entry].isDir ? FstStart[entry].nextOffset : (entry + 1);

        }   // Loop2

    }   // Loop1
}

// convert DVD file name into file position on the disk
// 0, if file not found
int dvd_open(const char *path)
{
    int entry = DVDConvertPathToEntrynum(path);
    if (entry < 0)
    {
        return 0;
    }

    return (int)FstStart[entry].fileOffset;
}
