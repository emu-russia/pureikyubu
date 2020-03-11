// Dolwin file utilities API

typedef enum _FILE_TYPE
{
    FILE_TYPE_ALL = 1,
    FILE_TYPE_DVD,
    FILE_TYPE_MAP,
    FILE_TYPE_PATCH,
    FILE_TYPE_DIR,
} FILE_TYPE;
                                                            // dont forget to :
int     FileSize(const char *filename);
void*   FileLoad(const char *filename, uint32_t *size=NULL);           // free!
BOOL    FileSave(const char *filename, void *data, uint32_t size);
char*   FileOpen(HWND hwnd, FILE_TYPE type=FILE_TYPE_ALL);        // copy away!
char*   FileShortName(const char *filename, int lvl=3);           // copy away!
char*   FileSmartSize(uint32_t size);                            // copy away!
