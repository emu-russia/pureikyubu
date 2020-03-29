// Opengl platform engine
// limitations : lines and points are COLOR0 only (no lit, tev)
#include "GX.h"

//
// local data
//

static HGLRC hglrc;
static HDC hdcgl;

static PAINTSTRUCT psFrame;
static int frameReady = 0;

// forward reference
void GL_EndFrame();
void GL_DoSnapshot(BOOL sel, FILE *f, uint8_t *dst, int width, int height);

// optionable
static uint32_t  scr_w = 640, scr_h = 480;

// perfomance counters
static uint32_t  frames, tris, pts, lines;

static HWND savedHwnd;
static bool opened = false;

// ---------------------------------------------------------------------------

static int GL_SetPixelFormat(HDC hdc)
{
    static PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        24,
        0, 0, 0, 0, 0, 0,
        0, 0,
        0, 0, 0, 0, 0,
        24,
        0,
        0,
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };

    int pixFmt;

    if((pixFmt = ChoosePixelFormat(hdc, &pfd)) == 0) return 0;
    if(SetPixelFormat(hdc, pixFmt, &pfd) == FALSE) return 0;
    DescribePixelFormat(hdc, pixFmt, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

    if(pfd.dwFlags & PFD_NEED_PALETTE) return 0;

    return 1;
}

BOOL GL_LazyOpenSubsystem(HWND hwnd)
{
    savedHwnd = hwnd;
    return TRUE;
}

BOOL GL_OpenSubsystem()
{
    if (opened)
        return TRUE;
    HWND hwnd = savedHwnd;

    hdcgl = GetDC(hwnd);
    
    if(hdcgl == NULL) return FALSE;

    if(GL_SetPixelFormat(hdcgl) == 0)
    {
        ReleaseDC(hwnd, hdcgl);
        return FALSE;
    }

    hglrc = wglCreateContext(hdcgl);
    if(hglrc == NULL)
    {
        ReleaseDC(hwnd, hdcgl);
        return FALSE;
    }

    wglMakeCurrent(hdcgl, hglrc);

    //
    // change some GL drawing rules
    //

    glScissor(0, 0, scr_w, scr_h);
    glViewport(0, 0, scr_w, scr_h);

    glFrontFace(GL_CW);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

    // set wireframe mode
    //glEnable(GL_CULL_FACE);
    //glCullFace(GL_BACK);
#ifdef WIREFRAME
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

    // clear performance counters
    frames = tris = pts = lines = 0;

    // prepare on-screen font texture
    PerfInit();

    opened = true;

    return TRUE;
}

void GL_CloseSubsystem()
{
    if(frameReady) GL_EndFrame();

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hglrc);

    opened = false;
}

// ---------------------------------------------------------------------------

static uint8_t      cr, cg, cb, ca;
static uint32_t     clear_z = -1;
static BOOL    set_clear = FALSE;

static BOOL    make_shot = FALSE;
static FILE *  snap_file;
static uint32_t     snap_w, snap_h;

// init rendering (call before drawing FIFO primitives)
void GL_BeginFrame()
{
    if(frameReady) return;

    BeginPaint(hwndMain, &psFrame);
    glDrawBuffer(GL_BACK);

    if(set_clear == TRUE)
    {
        glClearColor(
            (float)(cr / 255.0f),
            (float)(cg / 255.0f),
            (float)(cb / 255.0f),
            (float)(ca / 255.0f)
        );

        glClearDepth((double)(clear_z / 16777215.0));

        set_clear = FALSE;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    frameReady = 1;
}

// done rendering (call when frame is ready)
void GL_EndFrame()
{
    BOOL showPerf = FALSE;
    if(!frameReady) return;

/*/
    if(glGetError() != GL_NO_ERROR)
    {
        MessageBox(
            hwndMain, 
            "Error, during GL frame rendering.", 
            "We have big problem here!", 
            MB_OK | MB_TOPMOST
        );
        ExitProcess(0);
    }
/*/

#if SHOWPERF
    if(GetAsyncKeyState(VK_TAB) & 0x80000000) showPerf = TRUE;

    if(showPerf)
    {
        PerfPrintf(
            0, 16,
            "frame:%u\n"
            "tris:%u\n"
            "pts:%u\n"
            "lines:%u\n"
            "\n"
            "fifo:%ub\n"
            "cp:%u\nbp:%u\nxf:%u\n\n"
            "colors:%i\n"
            "texgens:%i\n"
            "tevnum:%i\n",
            frames, tris, pts, lines,
            lastFifoSize, cpLoads, bpLoads, xfLoads,
            xfRegs.numcol, xfRegs.numtex, bpRegs.genmode.ntev + 1
        );
    }
#endif

    // do snapshot
    if(make_shot)
    {
        make_shot = FALSE;
        GL_DoSnapshot(FALSE, snap_file, NULL, snap_w, snap_h);
    }

    glFinish();
    SwapBuffers(hdcgl);
    EndPaint(hwndMain, &psFrame);

    frameReady = 0;
    frames++;
    tris = pts = lines = 0;
    cpLoads = bpLoads = xfLoads = 0;
}

// ---------------------------------------------------------------------------

// load projection matrix
void GL_SetProjection(float *mtx)
{
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((GLfloat *)mtx);
    glMatrixMode(GL_MODELVIEW);
}

void GL_SetViewport(int x, int y, int w, int h, float znear, float zfar)
{
    //h += 32;
#ifndef NO_VIEWPORT
    glViewport(x, scr_h - (h + y), w, h);
    glDepthRange(znear, zfar);
#endif
}

void GL_SetScissor(int x, int y, int w, int h)
{
    //h += 32;
#ifndef NO_VIEWPORT
    glScissor(x, scr_h - (h + y), w, h);
#endif
}

// set clear rules
void GL_SetClear(Color clr, uint32_t z)
{
    cr = clr.R;
    cg = clr.G;
    cb = clr.B;
    ca = clr.A;
    clear_z = z;
    set_clear = TRUE;

/*/
    if(set_clear == TRUE)
    {
        glClearColor(
            (float)(cr / 255.0f),
            (float)(cg / 255.0f),
            (float)(cb / 255.0f),
            (float)(ca / 255.0f)
        );

        glClearDepth((double)(clear_z / 16777215.0));

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        set_clear = FALSE;
    }
/*/
}

void GL_SetCullMode(int mode)
{
/*/
    switch(mode)
    {
        case GFX_CULL_NONE:
            glDisable(GL_CULL_FACE);
            break;
        case GFX_CULL_FRONT:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            break;
        case GFX_CULL_BACK:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            break;
        case GFX_CULL_ALL:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT_AND_BACK);
            break;
    }
/*/
}

// ---------------------------------------------------------------------------

//
// platform rendering layer
//

void GL_RenderTriangle(
    const Vertex  *v0,
    const Vertex  *v1,
    const Vertex  *v2)
{
    const Vertex *vn[3] = { v0, v1, v2 };
    float mv[3][3];    // result vectors

    // position transform
    ApplyModelview(mv[0], v0->pos);
    ApplyModelview(mv[1], v1->pos);
    ApplyModelview(mv[2], v2->pos);

#ifndef WIREFRAME
    if(xfRegs.numtex && tID[0])
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tID[0]->bind);
    }
#endif

    // render triangle
    glBegin(GL_TRIANGLES);
    {
        for(int v=0; v<3; v++)
        {
#ifndef WIREFRAME
            // color hack
            if(xfRegs.numcol)
            {
                DoLights(vn[v]);
                glColor4ub(rasca[0].R, rasca[0].G, rasca[0].B, rasca[0].A);
            }

            // texture hack
            if(xfRegs.numtex && tID[0])
            {
                DoTexGen(vn[v]);
                tgout[0].out[0] *= tID[0]->ds;
                tgout[0].out[1] *= tID[0]->dt;
                glTexCoord2fv(tgout[0].out);
            }
#endif
            
#ifdef WIREFRAME
            glColor3ub(0, 255, 255);
#endif
            glVertex3fv(mv[v]);
        }
    }
    glEnd();

    tris++;
}

void GL_RenderLine(
    const Vertex *v0,
    const Vertex *v1)
{
    const Vertex *vn[2] = { v0, v1 };
    float mv[2][3];    // result vectors

    // position transform
    ApplyModelview(&mv[0][0], v0->pos);
    ApplyModelview(&mv[1][0], v1->pos);

    // render line
    //glEnable(GL_LINE_SMOOTH);
    glBegin(GL_LINES);
    {
        DoLights(vn[0]);
        glColor4ub(rasca[0].R, rasca[0].G, rasca[0].B, 0);
        glVertex3f(mv[0][0], mv[0][1], mv[0][2]);
        DoLights(vn[1]);
        glColor4ub(rasca[0].R, rasca[0].G, rasca[0].B, 0);
        glVertex3f(mv[1][0], mv[1][1], mv[1][2]);
    }
    glEnd();
    //glDisable(GL_LINE_SMOOTH);

    lines++;
}

void GL_RenderPoint(
    const Vertex *v0)
{
    float mv[3];    // result vectors

    // position transform
    ApplyModelview(&mv[0], v0->pos);

    // render triangle
    glBegin(GL_POINTS);
    {
        DoLights(v0);
        glColor3ub(rasca[0].R, rasca[0].G, rasca[0].B);
        glVertex3f(mv[0], mv[1], mv[2]);
    }
    glEnd();

    pts++;
}

// ---------------------------------------------------------------------------

// screenshots

// sel:0 - file, sel:1 - memory
void GL_DoSnapshot(BOOL sel, FILE *f, uint8_t *dst, int width, int height)
{
    uint8_t      hdr[14 + 40];   // bmp header
    uint16_t     *phdr;
    uint16_t     s, t;
    uint8_t      *buf, *ptr;
    float   ds, dt, d0, d1;
    BOOL    linear = FALSE;

    // allocate temporary buffer
    buf = (uint8_t *)malloc(scr_w * scr_h * 3);

    // calculate aspects
    ds = (float)scr_w / (float)width;
    dt = (float)scr_h / (float)height;
    if(ds != 1.0f) linear = TRUE;

    // write hardcoded header
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'B'; hdr[1] = 'M'; hdr[2] = 0x36;
    hdr[4] = 0x20; hdr[10] = 0x36;
    hdr[14] = 40; 
    phdr = (uint16_t *)(&hdr[0x12]); *phdr = (uint16_t)width;
    phdr = (uint16_t *)(&hdr[0x16]); *phdr = (uint16_t)height;
    hdr[26] = 1; hdr[28] = 24; hdr[36] = 0x20;
    if(sel)
    {
        memcpy(dst, hdr, sizeof(hdr));
        dst += sizeof(hdr);
    }
    else fwrite(hdr, 1, sizeof(hdr), f);

    // read opengl buffer
    glReadPixels(0, 0, scr_w, scr_h, GL_RGB, GL_UNSIGNED_BYTE, buf);

    // write texture image
    // fuck microsoft with their flipped bitmaps
    for(t=0,d0=0; t<height; t++,d0+=dt)
    {
        for(s=0,d1=0; s<width; s++,d1+=ds)
        {
            uint8_t  prev[3];
            uint8_t  rgb[3];     // RGB triplet
            ptr = &buf[3 * (scr_w * (int)d0 + (int)d1)];
            {
                // linear filter
                if(s && linear)
                {
                    rgb[2] = (*ptr++ + prev[2]) >> 1;
                    rgb[1] = (*ptr++ + prev[1]) >> 1;
                    rgb[0] = (*ptr++ + prev[0]) >> 1;
                }
                else
                {
                    rgb[2] = *ptr++;
                    rgb[1] = *ptr++;
                    rgb[0] = *ptr++;
                }
                
                if(linear)
                {
                    prev[2] = rgb[2];
                    prev[1] = rgb[1];
                    prev[0] = rgb[0];
                }

                if(sel) { memcpy(dst, rgb, 3); dst += 3; }
                else fwrite(rgb, 1, 3, f);
            }
        }
    }

    free(buf);
}

void GL_MakeSnapshot(char *path)
{
    if(make_shot) return;
    snap_w = scr_w, snap_h = scr_h;
    // create new file    
    if(snap_file)
    {
        fclose(snap_file);
        snap_file = NULL;
    }
    snap_file = fopen(path, "wb");
    if(snap_file) make_shot = TRUE;
}

// make small snapshot for savestate
// new size 160x120
void GL_SaveBitmap(uint8_t *buf)
{
    GL_DoSnapshot(TRUE, NULL, buf, 160, 120);
}
