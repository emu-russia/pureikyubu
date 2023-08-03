/*

This component is used to support non-input device emulation that requires platform-specific code.

Assume you've launched the GameCube with no controllers attached when using this backend.

*/

#include "pch.h"

bool PADOpen()
{
	return true;
}

void PADClose()
{
}

bool PADReadButtons(long padnum, PADState* state)
{
	return false;
}

bool PADSetRumble(long padnum, long cmd)
{
	return false;
}
