// filesystem definitions

#include "DvdStructs.h"

// externals
BOOL    dvd_fs_init();
int     dvd_open(const char *path, DVDFileEntry *root=NULL);
