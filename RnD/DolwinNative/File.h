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
int     FileSize(const char *filename);
void*   FileLoad(const char *filename, uint32_t *size=NULL);           // free!
BOOL    FileSave(const char *filename, void *data, uint32_t size);
char*   FileShortName(const char *filename, int lvl=3);           // copy away!
char*   FileSmartSize(uint32_t size);                            // copy away!
