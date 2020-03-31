// DvdOpenFile.cpp

#include <iostream>
#include <intrin.h>

#include "../../SRC/DVD/DvdStructs.h"

#define NameOffset(hi, lo) (((uint32_t)(hi) << 16) | (lo))

static DVDFileEntry* fst;
static DVDFileEntry* currentDir;
static char* nameTable;

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

static int DVDConvertPathToEntrynum(char* path)
{
    return 0;
}

int main(int argc, char **argv)
{
    std::cout << "Hello DvdOpenFile!\n";

    if (argc < 3)
    {
        printf("Use: DvdOpenFile \"/path/to/file\" FST.bin\n");
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

    fst = (DVDFileEntry *)ptr;

    nameTable = fst_prepare(fst);
    if (!nameTable)
    {
        printf("fst_prepare failed!\n");
        delete[] ptr;
        return -2;
    }

    // Get entrynum

    currentDir = fst;

    int entryNum = DVDConvertPathToEntrynum(dvdPath);

    printf("Entrynum to \"%s\": %i\n", dvdPath, entryNum);

    delete[] ptr;
    return 0;
}
