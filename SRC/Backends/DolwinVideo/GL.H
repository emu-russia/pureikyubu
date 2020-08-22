
#pragma once

// color type
typedef union _Color
{
    struct { uint8_t     A, B, G, R; };
    uint32_t     RGBA;
} Color;

// current vertex data, to renderer
typedef struct _Vertex
{
    float       pos[3];         // x, y, z
    float       nrm[3];         // x, y, z, normalized to [0, 1]
    Color       col[2];         // 2 color / alpha (RGBA)
    float       tcoord[8][4];   // s, t for eight tex units, last two for texgen
} Vertex;

// triangle cull rules
#define GFX_CULL_NONE       0
#define GFX_CULL_FRONT      1
#define GFX_CULL_BACK       2
#define GFX_CULL_ALL        3

// platform core descriptor
// all paltform-dependent code goes here
typedef struct
{
    // platform initialization
    BOOL(*OpenSubsystem)(HWND hwnd);
    void    (*CloseSubsystem)();

    // frame begin / end
    void    (*BeginFrame)();
    void    (*EndFrame)();
    void    (*MakeSnapshot)(char* path);
    void    (*SaveBitmap)(uint8_t* buf);

    // do rendering
    void    (*RenderTriangle)(
        const Vertex* v0,
        const Vertex* v1,
        const Vertex* v2
        );
    void    (*RenderLine)(
        const Vertex* v0,
        const Vertex* v1
        );
    void    (*RenderPoint)(
        const Vertex* v0
        );

    // load matricies
    void    (*SetProjection)(float* mtx);       // 4x4
    void    (*SetViewport)(int x, int y, int w, int h, float znear, float zfar);
    void    (*SetScissor)(int x, int y, int w, int h);

    // set clearing color / z
    void    (*SetClear)(Color clr, uint32_t z);

    // culling modes
    void    (*SetCullMode)(int mode);
} Renderer;

BOOL GL_LazyOpenSubsystem(HWND hwnd);
BOOL GL_OpenSubsystem();
void GL_CloseSubsystem();
void GL_BeginFrame();
void GL_EndFrame();
void GL_SetProjection(float* mtx);
void GL_SetViewport(int x, int y, int w, int h, float znear, float zfar);
void GL_SetScissor(int x, int y, int w, int h);
void GL_SetClear(Color clr, uint32_t z);
void GL_SetCullMode(int mode);
void GL_RenderTriangle(const Vertex* v0, const Vertex* v1, const Vertex* v2);
void GL_RenderLine(const Vertex* v0, const Vertex* v1);
void GL_RenderPoint(const Vertex* v0);
void GL_DoSnapshot(BOOL sel, FILE* f, uint8_t* dst, int width, int height);
void GL_MakeSnapshot(char* path);
void GL_SaveBitmap(uint8_t* buf);

void    GPFrameDone();
extern  BOOL      frame_done;
