// Windows GDI for VI module, to support homedev.
#include "dolphin.h"

static HDC      hdcMainWnd, hdcWndComp;
static HBITMAP  hbmDIBSection;
static HGDIOBJ  oldSelected;

static BOOL     gdi_init;
static s32      gdi_width, gdi_height;
static s32      bm_width, bm_height;

BOOL GDIOpen(HWND hwnd, s32 width, s32 height, RGBQUAD **gfxbuf)
{
    HDC         hdc;
    BITMAPINFO* bmi;
    u8*         DIBase;

    DBReport(CYAN "GDI: Windows DIB for video interface\n");

    if(gdi_init == TRUE) return TRUE;

    bmi = (BITMAPINFO *)calloc(sizeof(BITMAPINFO) + 16*4, 1);
    if(bmi == NULL) return FALSE;

    hdcMainWnd = hdc = GetDC(hwnd);

    memset(bmi, 0, sizeof(BITMAPINFO));
    bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bm_width  = bmi->bmiHeader.biWidth = width;
    bm_height = bmi->bmiHeader.biHeight = -height;
    bmi->bmiHeader.biPlanes = 1;
    bmi->bmiHeader.biBitCount = 32;
    bmi->bmiHeader.biCompression = BI_RGB;

    hbmDIBSection = CreateDIBSection(NULL, bmi, DIB_RGB_COLORS, (void **)&DIBase, NULL, 0);
    if(hbmDIBSection == NULL) return FALSE;
    *gfxbuf = (RGBQUAD *)DIBase;

    hdcWndComp = CreateCompatibleDC(hdc);
    if(hdcWndComp == NULL) return FALSE;
    oldSelected = SelectObject(hdcWndComp, hbmDIBSection);

    free(bmi);

    return (gdi_init = TRUE);
}

void GDIClose(HWND hwnd)
{
    if(gdi_init == FALSE) return;

    if(hbmDIBSection)
    {
        SelectObject(hdcWndComp, oldSelected);
        DeleteObject(hbmDIBSection);
        hbmDIBSection = NULL;
    }

    if(hdcWndComp)
    {
        DeleteDC(hdcWndComp);
        hdcWndComp = NULL;
    }

    if(hdcMainWnd)
    {
        ReleaseDC(hwnd, hdcMainWnd);
        hdcMainWnd = NULL;
    }

    gdi_init = FALSE;
}

void GDIRefresh()
{
    if(gdi_init)
    {
        if(vi.stretch)
        {
            StretchBlt(
                hdcMainWnd,
                0, 0,
                gdi_width, gdi_height,
                hdcWndComp,
                0, 0,
                bm_width, -bm_height,
                SRCCOPY
            );
        }
        else
        {
            BitBlt(
                hdcMainWnd,
                0, 0, 
                gdi_width, gdi_height,
                hdcWndComp,
                0, 0,
                SRCCOPY
            );
        }
    }
}

void GDIResize(s32 width, s32 height)
{
    gdi_width  = width;
    gdi_height = height;
}
