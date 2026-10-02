/*

This component is used to support emulation without any video output requiring platform-specific code.

*/

#include "pch.h"

bool VideoOutOpen(HWConfig* config, int width, int height, RGB** gfxbuf)
{
	return true;
}

void VideoOutClose()
{
}

void VideoOutRefresh()
{
}

void VideoOutResize(int width, int height)
{
}

int64_t VideoOutTakeFrames()
{
	// Nothing is shown, so no frame is output.
	return 0;
}
