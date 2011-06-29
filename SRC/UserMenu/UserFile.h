// Dolwin file utilities API

enum FILE_TYPE
{
    FILE_TYPE_ALL = 1,
    FILE_TYPE_DVD,
    FILE_TYPE_MAP,
    FILE_TYPE_PATCH,
    FILE_TYPE_DIR,
};
                                                            // dont forget to :
s32     FileSize(char *filename);
void*   FileLoad(char *filename, u32 *size=NULL);           // free!
BOOL    FileSave(char *filename, void *data, u32 size);
char*   FileOpen(HWND hwnd, int type=FILE_TYPE_ALL);        // copy away!
char*   FileShortName(char *filename, int lvl=3);           // copy away!
char*   FileSmartSize(u32 size);                            // copy away!
