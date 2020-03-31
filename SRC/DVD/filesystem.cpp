// DVD filesystem.
// I just cleaned hotquik's code a bit.
#include "pch.h"

// local data
static DVDBB2           bb2;
static DVDFileEntry*    fst;            // Loaded FST (byte-swapped as little-endian)
static uint32_t         fstSize;        // Size of loaded FST in bytes (not greater DVD_FST_MAX_SIZE)
static char*            files;          // Strings(name) table

#define FSTOFS(lo, hi) (((uint32_t)hi << 16) | lo)

// Swap longs (no need in assembly, used by tools)
static void SwapArea(uint32_t* addr, int count)
{
    uint32_t* until = addr + count / sizeof(uint32_t);

    while (addr != until)
    {
        *addr = _byteswap_ulong(*addr);
        addr++;
    }
}

// swap bytes in FST (little-endian)
// return beginning of strings table (or NULL, if bad FST)
static char *fst_prepare(DVDFileEntry *root)
{
    char* nameTablePtr = nullptr;

    root->nameOffsetLo = _byteswap_ushort(root->nameOffsetLo);
    root->fileOffset   = _byteswap_ulong(root->fileOffset);
    root->fileLength   = _byteswap_ulong(root->fileLength);

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

        entry->nameOffsetLo = _byteswap_ushort(entry->nameOffsetLo);
        entry->fileOffset = _byteswap_ulong(entry->fileOffset);
        entry->fileLength = _byteswap_ulong(entry->fileLength);
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
    if(fst)
    {
        free(fst);
        fst = NULL;
        fstSize = 0;
    }

    // create new FST
    fstSize = bb2.FSTLength;
    if(fstSize > DVD_FST_MAX_SIZE)
    {
        return false;
    }
    fst = (DVDFileEntry *)malloc(fstSize);
    if(fst == NULL)
    {
        return false;
    }
    DVD::Seek(bb2.FSTPosition);
    DVD::Read(fst, fstSize);
        
    // swap bytes in FST and find offset of string table
    files = fst_prepare(fst);
    if(!files)
    {
        free(fst);
        fst = NULL;
        return false;
    }

    // FST loaded ok
    return true;
}

// convert DVD file name into file position on the disk
// 0, if file not found
int dvd_open(const char *path, DVDFileEntry *root)
{
    int  slashPos;
    char Path[DVD_MAXPATH], * PathPtr, search[DVD_MAXPATH] = { 0 }, * slash, * p;
    DVDFileEntry *curr, *next;

    // if FST not loaded, then no files
    if(!fst) return 0;

    // remove root slash
    strcpy_s(Path, sizeof(Path), path);
    PathPtr = Path;
    if((*Path == '/' || *Path == '\\') && root == NULL) PathPtr++;

    // starting from DVD root
    if(root == NULL) root = fst;
    if(!root->isDir) return 0;

    // replace all '\' by '/'
    p = PathPtr;
    while(*p)
    {
        if(*p == '\\') *p++ = '/';
        else p++;
    }

    // find delimiter
    slash = strchr(PathPtr, '/');
    if(slash == NULL) slashPos = -1;
    else slashPos = (int)(slash - PathPtr);

    if(slashPos == -1)  // search in current dir
    {
        strcpy_s(search, sizeof(search), PathPtr);
    }
    else                // search in another dir
    {
        strncpy_s(search, sizeof(search), PathPtr, slashPos);
    }

    // search file
    next = &fst[root->nextOffset];
    curr = root + 1;
    while(curr < next)
    {
        if(!_stricmp(search, &files[FSTOFS(curr->nameOffsetLo, curr->nameOffsetHi)]))
            break;
        if(curr->isDir) curr = &fst[curr->nextOffset];
        else            curr = curr + 1;
    }

    // file found ?
    if(curr < (DVDFileEntry *)files)
    {
        if(slashPos == -1) return curr->fileOffset;
        else
        {
            if(curr->isDir) return dvd_open(&search[slashPos+1], curr);
            else return 0;
        }
    }
    else return 0;
}
