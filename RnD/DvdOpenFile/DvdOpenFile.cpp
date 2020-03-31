// DvdOpenFile.cpp

#include <iostream>
#include <intrin.h>

#include "../../SRC/DVD/DvdStructs.h"

#define NameOffset(hi, lo) (((uint32_t)(hi) << 16) | (lo))

static DVDFileEntry* FstStart;
static int currentDirectory;
static char* FstStringStart;

// swap bytes in FST (little-endian)
// return beginning of strings table (or NULL, if bad FST)
static char* fst_prepare(DVDFileEntry* root)
{
    char* nameTablePtr = nullptr;

    root->nameOffsetLo = _byteswap_ushort(root->nameOffsetLo);
    root->fileOffset = _byteswap_ulong(root->fileOffset);
    root->fileLength = _byteswap_ulong(root->fileLength);

    // Check root: must have no parent, has zero nameOfset and non-zero nextOffset.
    if (!(root->isDir &&
        root->parentOffset == 0 &&
        NameOffset(root->nameOffsetHi, root->nameOffsetLo) == 0 &&
        root->nextOffset != 0))
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


// <0: Bad path
static int DVDConvertPathToEntrynum(char* path)
{
    // currentDirectory assigned by DVDChangeDir
    int entry = currentDirectory;         // running entry

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
                char *r21 = path;      // r21 -- current pathPtr to inner loop
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

int main(int argc, char **argv)
{
    std::cout << "Hello DvdOpenFile!\n";

    if (argc < 3)
    {
        printf("Use: DvdOpenFile \"/path/to/dir/or/file\" FST.bin\n");
        return 0;
    }

    char* dvdPath = argv[1];
    char* fstFile = argv[2];

    // Load FST

    FILE* f;
    fopen_s(&f, fstFile, "rb");
    if (!f)
    {
        printf("Cannot open FST: %s\n", fstFile);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long fstSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t* ptr = new uint8_t[fstSize];
    
    fread(ptr, 1, fstSize, f);
    fclose(f);

    // Swap endianess

    FstStart = (DVDFileEntry *)ptr;

    FstStringStart = fst_prepare(FstStart);
    if (!FstStringStart)
    {
        printf("fst_prepare failed!\n");
        delete[] ptr;
        return -2;
    }

    // Get entrynum

    currentDirectory = 0;

    int entryNum = DVDConvertPathToEntrynum(dvdPath);

    printf("Entrynum to \"%s\": %i\n", dvdPath, entryNum);

    delete[] ptr;
    return 0;
}
