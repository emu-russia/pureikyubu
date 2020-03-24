// graphics platform interface
#include "GX.h"

Vertex      tri[3];             // triangle to be rendered
BOOL        frame_done=1;

// rendering complete, swap buffers, sync to vretrace
void GPFrameDone()
{
    GL_EndFrame();
    frame_done = 1;
}

// make screenshot
void GPMakeSnapshot(char *path)
{
    GL_MakeSnapshot(path);
}
