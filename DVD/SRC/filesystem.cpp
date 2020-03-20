// DVD filesystem.
// I just cleaned hotquik's code a bit.
#include "pch.h"

#include "../SRC/Core/Memory.h"

// local data
static DVDBB2           bb2;
static DVDFileEntry*    fst;            // not above DVD_FST_MAX_SIZE!
static uint32_t         fstSize;        // size of loaded FST
static char*            files;          // string table

// ---------------------------------------------------------------------------
// file system

#define FSTOFS(lo, hi) (((uint32_t)hi << 16) | lo)

// swap bytes in FST (for x86)
// return beginning of strings table (or NULL, if bad FST)
static DVDFileEntry *fst_prepare(DVDFileEntry *root)
{
    root->nameOffsetLo = (root->nameOffsetLo << 8)| 
                         (root->nameOffsetLo >> 8);
    root->fileOffset   = MEMSwap(root->fileOffset);
    root->fileLength   = MEMSwap(root->fileLength);

    if(root->isDir)
    {
        DVDFileEntry *follow, *next, *last;

        follow = &fst[root->nextOffset];
        next = root + 1;
        last = next;

        while(next < follow)
        {
            next = fst_prepare(next);
            if(next < last) return NULL;    // damaged
            last = next;
        }
        return follow;
    }
    else return root + 1;
}

// initialize filesystem
BOOL dvd_fs_init()
{
    // Check DiskID

    char diskId[5] = { 0 };

    DVDSeek(DVD_ID_OFFSET);
    DVDRead(diskId, 4);

    for (int i = 0; i < 4; i++)
    {
        if (!isalnum(diskId[i]))
        {
            return FALSE;
        }
    }

    // Check Apploader

    char apploaderBytes[5] = { 0 };

    DVDSeek(DVD_APPLDR_OFFSET);
    DVDRead(apploaderBytes, 4);

    for (int i = 0; i < 4; i++)
    {
        if (!isdigit(apploaderBytes[i]))
        {
            return FALSE;
        }
    }

    // load tables
    DVDSeek(DVD_BB2_OFFSET);
    DVDRead(&bb2, sizeof(DVDBB2));
    MEMSwapArea((uint32_t *)&bb2, sizeof(DVDBB2));

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
        return FALSE;
    }
    fst = (DVDFileEntry *)malloc(fstSize);
    if(fst == NULL)
    {
        return FALSE;
    }
    DVDSeek(bb2.FSTPosition);
    DVDRead(fst, fstSize);
        
    // swap bytes in FST and find offset of string table
    files = (char *)fst_prepare(fst);
    if(files == NULL)
    {
        free(fst);
        fst = NULL;
        return FALSE;
    }

    // FST loaded ok
    return TRUE;
}

// convert DVD file name into file position on the disk
// 0, if file not found
int dvd_open(const char *path, DVDFileEntry *root)
{
    int  slashPos;
    char Path[DVD_MAXPATH], *PathPtr, search[DVD_MAXPATH], *slash, *p;
    DVDFileEntry *curr, *next;

    // if FST not loaded, then no files
    if(fst == NULL) return 0;

    // remove root slash
    strcpy_s(Path, sizeof(Path), path);
    PathPtr = Path;
    if((*Path == '/' || *Path == '\\') && root == NULL) PathPtr++;

    // starting from DVD root
    if(root == NULL) root = fst;
    if(!root->isDir) return 0;

    memset(search, 0, DVD_MAXPATH);

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
