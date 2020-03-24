
#pragma once

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
