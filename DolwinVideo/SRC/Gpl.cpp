// graphics platform interface
#include "GX.h"

Renderer    *gfx;
Vertex      tri[3];             // triangle to be rendered
BOOL        frame_done=1;

// rendering complete, swap buffers, sync to vretrace
void GPFrameDone()
{
    gfx->EndFrame();
    frame_done = 1;
}

// make screenshot
void GPMakeSnapshot(char *path)
{
    gfx->MakeSnapshot(path);
}
