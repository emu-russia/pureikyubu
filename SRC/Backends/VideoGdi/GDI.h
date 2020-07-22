
#pragma once

#include <Windows.h>

bool    GDIOpen(HWND hwnd, int width, int height, RGBQUAD **gfxbuf);
void    GDIClose(HWND hwnd);
void    GDIRefresh();
void    GDIResize(int width, int height);
