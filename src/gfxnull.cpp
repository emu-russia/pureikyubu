/*

This component is used to support emulation without any graphics output requiring platform-specific code.

*/

#include "pch.h"

long GXOpen(HWConfig* config, uint8_t* ramPtr)
{
	return 1;
}

void GXClose()
{
}

void GXWriteFifo(uint8_t dataPtr[32])
{
}

void GXSetDrawCallbacks(GXDrawDoneCallback drawDoneCb, GXDrawTokenCallback drawTokenCb)
{
}
