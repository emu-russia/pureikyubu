// filesystem definitions

#include "DvdStructs.h"

// externals
bool    dvd_fs_init();
int     dvd_open(const char *path, DVDFileEntry *root=nullptr);
