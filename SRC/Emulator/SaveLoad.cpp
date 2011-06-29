// Emulator save-state support
#include "dolphin.h"

// to save state, call SaveLoad after assertion of VI_INTERRUPT;
// to load state, set fileName to current, do reset, and reload all data
void SaveLoad(BOOL save, char *saveFile)
{
}
