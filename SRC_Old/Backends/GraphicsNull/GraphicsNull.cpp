// GraphicsNull

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
