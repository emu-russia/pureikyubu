// DVD filesystem.
// I just cleaned hotquik's code a bit.
#include "DVD.h"

// local data
static DVDBB2           bb2;
static DVDFileEntry*    fst;            // not above DVD_FST_MAX_SIZE!
static u32              fstSize;        // size of loaded FST
static char*            files;          // string table

// ---------------------------------------------------------------------------
// swap endianess

static __declspec(naked) inline u32 __fastcall MEMSwap(u32 data)
{
    __asm   bswap   ecx
    __asm   mov     eax, ecx
    __asm   ret
}

static void __fastcall MEMSwapArea(u32 *addr, long count)
{
    u32 *until = addr + count / 4;

    while(addr != until)
    {
        *addr = MEMSwap(*addr);
        addr++;
    }
}

// ---------------------------------------------------------------------------
// file system

#define FSTOFS(lo, hi) (((u32)hi << 16) | lo)

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
    // load tables
    DVDSeek(DVD_BB2_OFFSET);
    DVDRead(&bb2, sizeof(DVDBB2));
    MEMSwapArea((u32 *)&bb2, sizeof(DVDBB2));

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
s32 dvd_open(char *path, DVDFileEntry *root)
{
    int  slashPos;
    char search[DVD_MAXPATH], *slash, *p;
    DVDFileEntry *curr, *next;

    // if FST not loaded, then no files
    if(fst == NULL) return 0;

    // remove root slash
    if((*path == '/' || *path == '\\') && root == NULL) path++;

    // starting from DVD root
    if(root == NULL) root = fst;
    if(!root->isDir) return 0;

    memset(search, 0, DVD_MAXPATH);

    // replace all '\' by '/'
    p = path;
    while(*p)
    {
        if(*p == '\\') *p++ = '/';
        else p++;
    }

    // find delimiter
    slash = strchr(path, '/');
    if(slash == NULL) slashPos = -1;
    else slashPos = (int)(slash - path);

    if(slashPos == -1)  // search in current dir
    {
        strcpy(search, path);
    }
    else                // search in another dir
    {
        strncpy(search, path, slashPos);
    }

    // search file
    next = &fst[root->nextOffset];
    curr = root + 1;
    while(curr < next)
    {
        if(!stricmp(search, &files[FSTOFS(curr->nameOffsetLo, curr->nameOffsetHi)]))
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
