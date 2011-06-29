// Dolwin file utilities
#include "dolphin.h"
#include <shlobj.h>

static char tempBuf[1024];      // temporary buffer for file operations

// get file size
s32 FileSize(char *filename)
{
    FILE *f = fopen(filename, "rb");
    if(f == NULL) return 0;
    fseek(f, 0, SEEK_END);
    s32 size = ftell(f);
    fclose(f);
    return size;
}

// load data from file
void * FileLoad(char *filename, u32 *size)
{
    FILE*   f;
    void*   buffer;
    u32     filesize;

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
BOOL FileSave(char *filename, void *data, u32 size)
{
    FILE *f = fopen(filename, "wb");
    if(f == NULL) return FALSE;

    fwrite(data, size, 1, f);
    fclose(f);
    return TRUE;
}

// open file dialog
char * FileOpen(HWND hwnd, int type)
{
    OPENFILENAME ofn;
    char szFileName[1024];
    char szFileTitle[1024];
    char lastDir[1024], prevDir[1024];
    BOOL result;

    _getcwd(prevDir, sizeof(prevDir));
    switch(type)
    {
        case FILE_TYPE_ALL:
            strcpy(lastDir, GetConfigString(USER_LASTDIR_ALL, USER_LASTDIR_ALL_DEFAULT));
            break;
        case FILE_TYPE_DVD:
            strcpy(lastDir, GetConfigString(USER_LASTDIR_DVD, USER_LASTDIR_DVD_DEFAULT));
            break;
        case FILE_TYPE_MAP:
            strcpy(lastDir, GetConfigString(USER_LASTDIR_MAP, USER_LASTDIR_MAP_DEFAULT));
            break;
        case FILE_TYPE_PATCH:
            strcpy(lastDir, GetConfigString(USER_LASTDIR_PATCH, USER_LASTDIR_PATCH_DEFAULT));
            break;
    }

    memset(szFileName, 0, sizeof(szFileName));
    memset(szFileTitle, 0, sizeof(szFileTitle));

    if(type == FILE_TYPE_DIR)
    {
        // Dumbiest Windows code. Hell.. I just needed "char * BrowseDirectory(hwnd)"..

        BROWSEINFO bi; 
        LPSTR lpBuffer=NULL; 
        LPITEMIDLIST pidlRoot=NULL;     // PIDL for root folder 
        LPITEMIDLIST pidlBrowse=NULL;   // PIDL selected by user
        LPMALLOC g_pMalloc;

        // Get the shell's allocator. 
        if(!SUCCEEDED(SHGetMalloc(&g_pMalloc)))
            return NULL; 

        // Allocate a buffer to receive browse information.
        lpBuffer = (LPSTR)g_pMalloc->Alloc(MAX_PATH);
        if(lpBuffer == NULL) return NULL;

        // Get the PIDL for the root folder.
        if( !SUCCEEDED(SHGetSpecialFolderLocation(hwnd, CSIDL_DRIVES, &pidlRoot)) )
        { 
            g_pMalloc->Free(lpBuffer);
            return NULL;
        }

        // Fill in the BROWSEINFO structure. 
        bi.hwndOwner        = hwnd; 
        bi.pidlRoot         = pidlRoot; 
        bi.pszDisplayName   = lpBuffer; 
        bi.lpszTitle        = "Choose Directory"; 
        bi.ulFlags          = 0; 
        bi.lpfn             = NULL; 
        bi.lParam           = 0;

        // Browse for a folder and return its PIDL. 
        pidlBrowse = SHBrowseForFolder(&bi);
        if(result = (pidlBrowse != NULL))
        {
            SHGetPathFromIDList(pidlBrowse, lpBuffer); 
            strcpy(szFileName, lpBuffer);
     
            // Free the PIDL returned by SHBrowseForFolder.
            g_pMalloc->Free(pidlBrowse);
        }

        // Clean up. 
        if(pidlRoot) g_pMalloc->Free(pidlRoot);
        if(lpBuffer) g_pMalloc->Free(lpBuffer);

        // Release the shell's allocator. 
        g_pMalloc->Release();
    }
    else
    {
        ofn.lStructSize         = sizeof(OPENFILENAME);
        ofn.hwndOwner           = hwnd;
        switch(type)
        {
            case FILE_TYPE_ALL:
                ofn.lpstrFilter = 
                    "All Supported Files (*.dol, *.elf, *.bin, *.gcm, *.gmp)\0*.dol;*.elf;*.bin;*.gcm;*.gmp\0"
                    "GameCube Executable Files (*.dol, *.elf)\0*.dol;*.elf\0"
                    "Binary Files (*.bin)\0*.bin\0"
                    "GameCube DVD Images (*.gcm)\0*.gcm\0"
                    "Compressed DVD Images (*.gmp)\0*.gmp\0"
                    "All Files (*.*)\0*.*\0";
                break;
            case FILE_TYPE_DVD:
                ofn.lpstrFilter = 
                    "All Disk Image Files (*.gcm, *.gmp)\0*.gcm;*.gmp\0"
                    "GameCube DVD Images (*.gcm)\0*.gcm\0"
                    "Compressed DVD Images (*.gmp)\0*.gmp\0"
                    "All Files (*.*)\0*.*\0";
                break;
            case FILE_TYPE_MAP:
                ofn.lpstrFilter = 
                    "Symbolic information files (*.map)\0*.map\0"
                    "All Files (*.*)\0*.*\0";
                break;
            case FILE_TYPE_PATCH:
                ofn.lpstrFilter = 
                    "Patch files (*.patch)\0*.patch\0"
                    "All Files (*.*)\0*.*\0";
                break;
        }
        ofn.lpstrCustomFilter   = NULL;
        ofn.nMaxCustFilter      = 0;
        ofn.nFilterIndex        = 1;
        ofn.lpstrFile           = szFileName;
        ofn.nMaxFile            = sizeof(szFileName);
        ofn.lpstrInitialDir     = lastDir;
        ofn.lpstrFileTitle      = szFileTitle;
        ofn.nMaxFileTitle       = sizeof(szFileTitle);
        ofn.lpstrTitle          = "Open File\0";
        ofn.lpstrDefExt         = "";
        ofn.Flags               = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        result = GetOpenFileName(&ofn);
    }

    if(result)
    {
        strcpy(tempBuf, szFileName);

        // save last directory
        strcpy(lastDir, tempBuf);
        int i = (int)strlen(lastDir) - 1;
        while(lastDir[i] != '\\') i--;
        lastDir[i+1] = '\0';
        switch(type)
        {
            case FILE_TYPE_ALL:
                SetConfigString(USER_LASTDIR_ALL, lastDir);
                break;
            case FILE_TYPE_DVD:
                SetConfigString(USER_LASTDIR_DVD, lastDir);
                break;
            case FILE_TYPE_MAP:
                SetConfigString(USER_LASTDIR_MAP, lastDir);
                break;
            case FILE_TYPE_PATCH:
                SetConfigString(USER_LASTDIR_PATCH, lastDir);
                break;
        }

        chdir(prevDir);
        return tempBuf;
    }
    else
    {
        chdir(prevDir);
        return NULL;
    }
}

// make path to file shorter for "lvl" levels.
char * FileShortName(char *filename, int lvl)
{
    int c = 0, i;

    tempBuf[0] = filename[0];
    tempBuf[1] = filename[1];
    tempBuf[2] = filename[2];

    filename += 3;

    for(i=strlen(filename)-1; i; i--)
    {
        if(filename[i] == '\\') c++;
        if(c == lvl) break;
    }

    if(c == lvl)
    {
        sprintf(&tempBuf[3], "...%s", &filename[i]);
    }
    else return filename - 3;

    return tempBuf;
}

// nice value of KB, MB or GB, for output
char * FileSmartSize(u32 size)
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
    else sprintf(tempBuf, "%1.1f GB", (f32)size/1024/1024/1024);
    return tempBuf;
}
