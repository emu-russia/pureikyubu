// Dolwin file utilities
#include "pch.h"

static char tempBuf[1024];      // temporary buffer for file operations

// get file size
int FileSize(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if(f == NULL) return 0;
    fseek(f, 0, SEEK_END);
    int size = ftell(f);
    fclose(f);
    return size;
}

// load data from file
void * FileLoad(const char *filename, uint32_t *size)
{
    FILE*   f;
    void*   buffer;
    uint32_t  filesize;

    if(size) *size = 0;

    f = fopen(filename, "rb");
    if(f == NULL) return NULL;

    fseek(f, 0, SEEK_END);
    filesize = ftell(f);
    fseek(f, 0, SEEK_SET);

    buffer = malloc(filesize);
    if(buffer == NULL)
    {
        fclose(f);
        return NULL;
    }

    fread(buffer, filesize, 1, f);
    fclose(f);
    if(size) *size = filesize;    
    return buffer;
}

// save data in file
BOOL FileSave(const char *filename, void *data, uint32_t size)
{
    FILE *f = fopen(filename, "wb");
    if(f == NULL) return FALSE;

    fwrite(data, size, 1, f);
    fclose(f);
    return TRUE;
}

// make path to file shorter for "lvl" levels.
char * FileShortName(const char *filename, int lvl)
{
    int c = 0;
    size_t i = 0;

    char* ptr = (char *)filename;

    tempBuf[0] = ptr[0];
    tempBuf[1] = ptr[1];
    tempBuf[2] = ptr[2];

    ptr += 3;

    for(i=strlen(ptr)-1; i; i--)
    {
        if(ptr[i] == '\\') c++;
        if(c == lvl) break;
    }

    if(c == lvl)
    {
        sprintf(&tempBuf[3], "...%s", &ptr[i]);
    }
    else return ptr - 3;

    return tempBuf;
}

// nice value of KB, MB or GB, for output
char * FileSmartSize(uint32_t size)
{
    if(size < 1024)
    {
        sprintf(tempBuf, "%i byte", size);
    }
    else if(size < 1024*1024)
    {
        sprintf(tempBuf, "%i KB", size/1024);
    }
    else if(size < 1024*1024*1024)
    {
        sprintf(tempBuf, "%i MB", size/1024/1024);
    }
    else sprintf(tempBuf, "%1.1f GB", (float)size/1024/1024/1024);
    return tempBuf;
}
