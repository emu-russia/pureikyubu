// filesystem definitions

#pragma once

#include "DvdStructs.h"

// externals
bool    dvd_fs_init();
void	dvd_fs_shutdown();
int     dvd_open(const char *path);
