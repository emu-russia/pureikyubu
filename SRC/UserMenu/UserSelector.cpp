// file selector
#include "dolphin.h"

// Sjis->Unicode translation table for Japan DVDs
#include "SjisTable.h"

// all important data is placed here
UserSelector usel;

// list of banners
static HIMAGELIST bannerList;

// ---------------------------------------------------------------------------
// PATH management

// make sure path have ending '\\'
static void fix_path(char *path)
{
    size_t n = strlen(path);
    if(path[n-1] != '\\')
    {
        path[n]   = '\\';
        path[n+1] = 0;
    }
}

// remove all control symbols (below space)
static void fix_string(char *str)
{
    for(int i=0; i<strlen(str); i++)
    {
        if(str[i] < ' ') str[i] = ' ';
    }
}

// print all paths (DEBUG)
static void list_path()
{
    for(int i=0; i<usel.pathnum; i++)
    {
        DolwinReport("path %i : \'%s\'", i, usel.paths[i]);
    }
}

// add new path into "paths" list
static void add_path(char *path)
{
    // check path size
    size_t len = strlen(path) + 1;
    if(len >= MAX_PATH)
    {
        DolwinReport("Too long path string : %s", path);
        return;
    }

    // extend list
    if(usel.paths) usel.paths = (char **)realloc(usel.paths, sizeof(char **) * (usel.pathnum + 1));
    else usel.paths = (char **)malloc(sizeof(char **));
    VERIFY(usel.paths == NULL, "Not enough memory to extend PATH list.");

    // save new path, and increase "pathnum"
    usel.paths[usel.pathnum] = (char *)malloc(len);
    VERIFY(usel.paths[usel.pathnum] == NULL, "Not enough memory for new PATH.");
    strcpy_s(usel.paths[usel.pathnum], len, path);
    usel.pathnum++;
}

// load PATH user variable and cut it on pieces into "paths" list
static void load_path()
{
    char *var = GetConfigString(USER_PATH, USER_PATH_DEFAULT), *ptr;
    char path[MAX_PATH+1];
    int  n = 0;

    // delete current pathlist
    for(int i=0; i<usel.pathnum; i++)
    {
        if(usel.paths[i])
        {
            free(usel.paths[i]);
            usel.paths[i] = NULL;
        }
    }
    if(usel.paths)
    {
        free(usel.paths);
        usel.paths = NULL;
        usel.pathnum = 0;
    }

    // search new paths (separated by ';') until ending '\0'
    ptr = var;
    while(*ptr)
    {
        char c = *ptr++;

        // add new one
        if(c == ';')
        {
            path[n++] = 0;
            fix_path(path);
            add_path(path);
            n = 0;
        }
        else path[n++] = c;
    }

    // last path
    if(n)
    {
        path[n++] = 0;
        fix_path(path);
        add_path(path);
    }
}

// called after loading of new file (see Emulator\Loader.cpp)
bool AddSelectorPath(char *fullPath)
{
    int i;
    // spell will be checked RTL, so fullPath[0] doesnt crash when fullPath = NULL
    if( (fullPath[0] == 0) || (fullPath == NULL) )
        return false;

    fix_path(fullPath);

    for(i=0; i<usel.pathnum; i++)
    {
        if(!_stricmp(fullPath, usel.paths[i])) break;
    }

    if(i == usel.pathnum)
    {
        char temp[100000];
        char * old = GetConfigString(USER_PATH, USER_PATH_DEFAULT);
        VERIFY(strlen(old) >= (sizeof(temp) - 1000), "Argh, overflow!");
        if(!_stricmp(old, "<EMPTY>"))
            sprintf_s(temp, sizeof(temp), "%s", fullPath);
        else
            sprintf_s(temp, sizeof(temp), "%s;%s", old, fullPath);
        SetConfigString(USER_PATH, temp);
        add_path(fullPath);
        return true;
    }
    else return false;
}

// ---------------------------------------------------------------------------
// add new file (extend filelist, convert banner, put it into list)

// return color format in 16-bit videomode
static int is565()
{
    #pragma message ("Hack, until I find solution!")
    return 1;   // HACK, until I know how
}

#define PACKRGB555(r, g, b) (uint16_t)((((r)&0xf8)<<7)|(((g)&0xf8)<<2)|(((b)&0xf8)>>3))
#define PACKRGB565(r, g, b) (uint16_t)((((r)&0xf8)<<8)|(((g)&0xfc)<<3)|(((b)&0xf8)>>3))

// convert banner. return indexes of new banner icon.
// we are using two icons for single banner, to highlight.
// return FALSE, if we cant convert banner (or something bad)
static bool add_banner(uint8_t *banner, int *bA, int *bB)
{
    int pack = is565();
    int width = (usel.smallIcons) ? (DVD_BANNER_WIDTH >> 1) : (DVD_BANNER_WIDTH);
    int height = (usel.smallIcons) ? (DVD_BANNER_HEIGHT >> 1) : (DVD_BANNER_HEIGHT);
    HDC hdc = CreateDC("DISPLAY", NULL, NULL, NULL);
    int bitdepth = GetDeviceCaps(hdc, BITSPIXEL);
    int bpp = bitdepth / 8;
    int bcount = width * height * bpp;
    int tiles  = (DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT) / 16;    
    uint8_t *imageA, *imageB, *ptrA, *ptrB;
    double rgb[3], rgbh[3];
    double alpha, alphaC;
    uint16_t *tile  = (uint16_t *)banner, *ptrA16, *ptrB16;
    uint32_t ri[4], gi[4], bi[4], ai2[4], rhi[4], ghi[4], bhi[4];     // for interpolation
    uint32_t r, g, b, a, rh, gh, bh;                                 // final values
    int row = 0, col = 0, pos;

    imageA = (uint8_t *)malloc(bcount);
    if(imageA == NULL)
    {
        DeleteDC(hdc);
        return FALSE;
    }
    imageB = (uint8_t *)malloc(bcount);
    if(imageB == NULL)
    {
        free(imageA);
        DeleteDC(hdc);
        return FALSE;
    }

    DWORD backcol = GetSysColor(COLOR_WINDOW);
    rgb[0] = (double)GetRValue(backcol);
    rgb[1] = (double)GetGValue(backcol);
    rgb[2] = (double)GetBValue(backcol);

    backcol = GetSysColor(COLOR_HIGHLIGHT);
    rgbh[0] = (double)GetRValue(backcol);
    rgbh[1] = (double)GetGValue(backcol);
    rgbh[2] = (double)GetBValue(backcol);

    // convert RGB5A3 -> RGBA
    for(int i=0; i<tiles; i++, tile+=16)
    {
        for(int j=0; j<4; j+=usel.smallIcons+1)
        for(int k=0; k<4; k+=usel.smallIcons+1)
        {
            uint16_t p, ph;

            if(usel.smallIcons)     // small banner (interpolate)
            {
                for(int n=0; n<4; n++)
                {
                    p = tile[(j + (n >> 1)) * 4 + (k + (n & 1))];
                    p = (p << 8) | (p >> 8);

                    if(p >> 15)
                    {
                        ri[n] = (p & 0x7c00) >> 10;
                        gi[n] = (p & 0x03e0) >> 5;
                        bi[n] = (p & 0x001f);
                        ri[n] = (uint8_t)((ri[n] << 3) | (ri[n] >> 2));
                        gi[n] = (uint8_t)((gi[n] << 3) | (gi[n] >> 2));
                        bi[n] = (uint8_t)((bi[n] << 3) | (bi[n] >> 2));
                        rhi[n] = ri[n], ghi[n] = gi[n], bhi[n] = bi[n];
                    }
                    else
                    {
                        ri[n] = (p & 0x0f00) >> 8;
                        gi[n] = (p & 0x00f0) >> 4;
                        bi[n] = (p & 0x000f);
                        ai2[n] = (p & 0x7000) >> 12;

                        alpha = (double)ai2[n] / 7.0;
                        alphaC = 1.0 - alpha;

                        rhi[n] = (uint8_t)((double)(16*ri[n]) * alpha + rgbh[0] * alphaC);
                        ghi[n] = (uint8_t)((double)(16*gi[n]) * alpha + rgbh[1] * alphaC);
                        bhi[n] = (uint8_t)((double)(16*bi[n]) * alpha + rgbh[2] * alphaC);

                        ri[n] = (uint8_t)((double)(16*ri[n]) * alpha + rgb[0] * alphaC);
                        gi[n] = (uint8_t)((double)(16*gi[n]) * alpha + rgb[1] * alphaC);
                        bi[n] = (uint8_t)((double)(16*bi[n]) * alpha + rgb[2] * alphaC);
                    }
                }

                // bilinear interpolation
                r = ((ri[0] + ri[1] + ri[2] + ri[3]) >> 2);
                g = ((gi[0] + gi[1] + gi[2] + gi[3]) >> 2);
                b = ((bi[0] + bi[1] + bi[2] + bi[3]) >> 2);
                rh = ((rhi[0] + rhi[1] + rhi[2] + rhi[3]) >> 2);
                gh = ((ghi[0] + ghi[1] + ghi[2] + ghi[3]) >> 2);
                bh = ((bhi[0] + bhi[1] + bhi[2] + bhi[3]) >> 2);

                pos = bpp * (((row + j) >> 1) * width + ((col + k) >> 1));
            }
            else                    // large banner
            {
                p = tile[j * 4 + k];
                ph = p = (p << 8) | (p >> 8);
                if(p >> 15)
                {
                    r = (p & 0x7c00) >> 10;
                    g = (p & 0x03e0) >> 5;
                    b = (p & 0x001f);
                    r = (uint8_t)((r << 3) | (r >> 2));
                    g = (uint8_t)((g << 3) | (g >> 2));
                    b = (uint8_t)((b << 3) | (b >> 2));
                    rh = r, gh = g, bh = b;
                }
                else
                {
                    r = (p & 0x0f00) >> 8;
                    g = (p & 0x00f0) >> 4;
                    b = (p & 0x000f);
                    a = (p & 0x7000) >> 12;

                    alpha = (double)a / 7.0;
                    alphaC = 1.0 - alpha;

                    rh = (uint8_t)((double)(16*r) * alpha + rgbh[0] * alphaC);
                    gh = (uint8_t)((double)(16*g) * alpha + rgbh[1] * alphaC);
                    bh = (uint8_t)((double)(16*b) * alpha + rgbh[2] * alphaC);

                    r = (uint8_t)((double)(16*r) * alpha + rgb[0] * alphaC);
                    g = (uint8_t)((double)(16*g) * alpha + rgb[1] * alphaC);
                    b = (uint8_t)((double)(16*b) * alpha + rgb[2] * alphaC);
                }

                pos = bpp * ((row + j) * width + (col + k));
            }

            ptrA16 = (uint16_t *)&imageA[pos];
            ptrB16 = (uint16_t *)&imageB[pos];
            ptrA   = &imageA[pos];
            ptrB   = &imageB[pos];

            if(bitdepth == 8)
            {
                // you can test 8-bit in XP, running Dolwin in 256 colors
                *ptrA++ =
                *ptrB++ = (uint8_t)(r | g ^ b);
            }
            else if(bitdepth == 16)
            {
                if(pack)        // 565
                {
                    p = PACKRGB565(r, g, b);
                    ph= PACKRGB565(rh, gh, bh);
                }
                else            // 555 (on old boxes)
                {
                    p = PACKRGB555(r, g, b);
                    ph= PACKRGB555(rh, gh, bh);
                }

                *ptrA16 = p;
                *ptrB16 = ph;
            }
            else if(bitdepth == 24)
            {
                *ptrA++ = (uint8_t)b;
                *ptrA++ = (uint8_t)g;
                *ptrA++ = (uint8_t)r;

                *ptrB++ = (uint8_t)bh;
                *ptrB++ = (uint8_t)gh;
                *ptrB++ = (uint8_t)rh;
            }
            else    // 32 bpp ARGB format.
            {
                *ptrA++ = (uint8_t)b;
                *ptrA++ = (uint8_t)g;
                *ptrA++ = (uint8_t)r;
                *ptrA++ = (uint8_t)255;

                *ptrB++ = (uint8_t)bh;
                *ptrB++ = (uint8_t)gh;
                *ptrB++ = (uint8_t)rh;
                *ptrB++ = (uint8_t)255;
            }
        }

        col += 4;
        if(col == DVD_BANNER_WIDTH)
        {
            col = 0;
            row += 4;
        }
    }

    // add new icons
    HBITMAP hbm = CreateCompatibleBitmap(hdc, width, height);
    if(hbm == NULL)
    {
        DeleteDC(hdc);
        free(imageA), free(imageB);
        return FALSE;
    }

    SetBitmapBits(hbm, bcount, imageA);     // normal icon
    *bA = ImageList_Add(bannerList, hbm, NULL);

    SetBitmapBits(hbm, bcount, imageB);     // highlighted icon
    *bB = ImageList_Add(bannerList, hbm, NULL);

    // clean up
    DeleteObject(hbm);
    DeleteDC(hdc);
    free(imageA), free(imageB);
    return TRUE;
}

// add new item
static void add_item(int index)
{
    LV_ITEM lvi;
    memset(&lvi, 0, sizeof(LV_ITEM));
    lvi.mask    = LVIF_TEXT | LVIF_PARAM;
    lvi.iItem   = ListView_GetItemCount(usel.hSelectorWindow);
    lvi.lParam  = (LPARAM)index;
    lvi.pszText = LPSTR_TEXTCALLBACK;
    ListView_InsertItem(usel.hSelectorWindow, &lvi); 
}

// insert new file into filelist
static void add_file(char *file, int fsize, int type)
{
    UserFile    item;
    DVDBanner2  *bnr;

    memset(&item, 0, sizeof(UserFile));

    // check file size (for stupid user)
    if((fsize == 0) || (fsize > 0x57058000 /* 1.5 gb */ ))
    {
        return;
    }

    // try to open file
    FILE* f = nullptr;
    fopen_s(&f, file, "rb");
    if(f == NULL) return;
    fclose(f);

    // save file info
    item.type = type;
    item.size = fsize;
    strcpy_s(item.name, sizeof(item.name), file);

    if(type == SELECTOR_FILE_DVD)
    {
        // try to set current DVD
        if(DVDSetCurrent(file) == FALSE) return;

        // load DVD banner
        bnr = (DVDBanner2 *)DVDLoadBanner(file);

        // get DiskID
        char diskID[8];
        DVDSetCurrent(file);
        DVDSeek(0);
        DVDRead(diskID, 4);
        diskID[4] = 0;

        // set GameID
        sprintf_s ( item.id, sizeof(item.id), "%.4s%02X",
                 diskID, DVDBannerChecksum((void *)bnr) );

        // set game name and comment
        if(GetGameInfo(item.id, item.title, item.comment) == FALSE)
        {
            // use banner info instead (and remove line-feeds)
            strncpy_s(item.title, sizeof(item.title), (char *)bnr->comments[0].longTitle, MAX_TITLE);
            strncpy_s(item.comment, sizeof(item.comment), (char *)bnr->comments[0].comment, MAX_COMMENT);
            fix_string(item.title);
            fix_string(item.comment);
        }

        // convert banner to RGB and add into bannerlist
        int a, b;
        bool res = add_banner(bnr->image, &a, &b);
        if(res == FALSE) return;    // we cant :(
        item.icon[0] = a;
        item.icon[1] = b;
        free(bnr);
    }
    else if(type == SELECTOR_FILE_EXEC)
    {
        // rip filename
        char drive[_MAX_DRIVE + 1], dir[_MAX_DIR], name[_MAX_PATH], ext[_MAX_EXT];
        _splitpath(file, drive, dir, name, ext);
        //strlwr(name);

        // set ID
/*/
        uint32_t size, checksum = -1;
        uint8_t *buf = (uint8_t *)FileLoad(file, &size);
        if(buf)
        {
            for(uint32_t i=0; i<size; i++)
                checksum += buf[i];
            free(buf);
        }
        size &= 0xff;
        checksum &= 0xfff;
        if(!stricmp(ext, ".elf")) sprintf(item.id, "E%02X%03X", size, checksum);
        else if(!stricmp(ext, ".dol")) sprintf(item.id, "D%02X%03X", size, checksum);
        else
/*/
        strcpy_s (item.id, sizeof(item.id), "-");

        if(GetGameInfo(name, item.title, item.comment) == FALSE)
        {
            strcpy_s (item.title, sizeof(item.title), name);
            item.comment[0] = 0;
        }
    }
    else VERIFY(type, "Unknown selector file type.");

    // extend filelist
    if(usel.files)
    {
        usel.files = (UserFile *)realloc(usel.files, 
                      sizeof(UserFile)*(usel.filenum + 1) );
    }
    else
    {
        usel.files = (UserFile *)malloc(sizeof(UserFile));
    }
    VERIFY(usel.files == NULL, "Not enough memory for new selector item.");

    // add new item
    memcpy(&usel.files[usel.filenum], &item, sizeof(UserFile));
    add_item(usel.filenum++);
}

// ---------------------------------------------------------------------------
// draw

// we are using selector "width" variable to adjust last column
static void reset_columns()
{
    LV_COLUMN   lvcol;

    // delete old columns
    memset(&lvcol, 0, sizeof(lvcol));
    lvcol.mask = LVCF_FMT;
    while(ListView_GetColumn(usel.hSelectorWindow, 0, &lvcol))
    {
        ListView_DeleteColumn(usel.hSelectorWindow, 0);
    }

    lvcol.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

    // add new columns
    #define ADDCOL(align, width, text, id)                          \
    {                                                               \
        lvcol.fmt = LVCFMT_##align;                                 \
        lvcol.cx  = width;                                          \
        lvcol.pszText = SELECTOR_COLUMN_##text;                     \
        ListView_InsertColumn(usel.hSelectorWindow, id, &lvcol);    \
    }


    ADDCOL(LEFT  , (usel.smallIcons) ? (57) : (105), BANNER , 0);
    ADDCOL(LEFT  , 200, TITLE  , 1);
    ADDCOL(CENTER, 60 , SIZE   , 2);
    ADDCOL(CENTER, 60 , GAMEID , 3);

    // last one is tricky
    int commentWidth = usel.width - 
                       (((usel.smallIcons) ? (57) : (105))+200+60+60) - 
                       GetSystemMetrics(SM_CXVSCROLL) - 4;
    if(commentWidth < 190) commentWidth = 190;
    ADDCOL(LEFT  , commentWidth, COMMENT, 4);
}

void ResizeSelector(int width, int height)
{
    // opened ?
    if(!usel.opened) return;

    if(IsWindow(usel.hSelectorWindow))
    {
        // adjust
        if(IsWindow(wnd.hStatusWindow))
        {
            RECT rc;
            GetWindowRect(wnd.hStatusWindow, &rc);
            height -= (WORD)(rc.bottom - rc.top);
        }

        // move window
        MoveWindow(usel.hSelectorWindow, 0, 0, width, height, TRUE);
        usel.width = width;
        usel.height = height;

        reset_columns();
    }
}

static uint16_t * SjisToUnicode(uint8_t * sjisText, uint32_t * size, uint32_t * chars)
{
    uint16_t * unicodeText , *ptrU , uchar, schar;
    uint8_t *ptrS;

    *size = strlen((char *)sjisText) * 2;
    unicodeText = (uint16_t *)malloc(*size);
    memset(unicodeText, 0, *size);

    ptrU = unicodeText;
    ptrS = sjisText;
    *chars = 0;

    schar = *ptrS;
    while(schar != 0)
    {
        uchar = SjisTable[schar];
        if(uchar == 0xFFFF)
        {
            ptrS++;
            schar = (schar << 8) | *ptrS;
            uchar = SjisTable[schar];
        }
        *ptrU = uchar;

        ptrU++;
        ptrS++;
        (*chars)++;
        schar = *ptrS;
    }
    return unicodeText;
}

// draw single item
void DrawSelectorItem(LPDRAWITEMSTRUCT item)
{
    // opened ?
    if(!usel.opened) return;

    #define     ID item->itemID
    #define     DC item->hDC
    UserFile*   file;       // item to draw
    bool        selected;   // 1, if item is selected
    HBRUSH      hb;         // background brush
    LV_ITEM     lvi;
    LV_COLUMN   lvc;
    LVCOLUMNW   lvcw;
    RECT        rc, rc2;

    // get selected item
    memset(&lvi, 0, sizeof(lvi));
    lvi.mask    = LVIF_PARAM;
    lvi.iItem   = ID;
    if(!ListView_GetItem(usel.hSelectorWindow, &lvi)) return;
    lvi.state   = ListView_GetItemState(usel.hSelectorWindow, ID, - 1);
    selected    = (lvi.state & LVIS_SELECTED) ? (TRUE) : (FALSE);
    file        = &usel.files[lvi.lParam];

    // select background brush
    if(selected)
    {
        hb = (HBRUSH)(COLOR_HIGHLIGHT + 1);
        SetTextColor(DC, GetSysColor(COLOR_HIGHLIGHTTEXT));
    }
    else
    {
        hb = (HBRUSH)(COLOR_WINDOW + 1);
        SetTextColor(DC, GetSysColor(COLOR_WINDOWTEXT));
    }

    // show item number for debugger's 'boot {n}' command
    if(selected)
    {
        char progress[1024];
        sprintf_s(progress, sizeof(progress), "#%i: %s", (int)lvi.lParam + 1, file->name);
        SetStatusText(STATUS_PROGRESS, progress);
    }

    // fill background
    FillRect(DC, &item->rcItem, hb);
    SetBkMode(DC, TRANSPARENT);

    // draw file icon
    ListView_GetItemRect(usel.hSelectorWindow, ID, &rc, LVIR_ICON);
    switch(file->type)
    {
        case SELECTOR_FILE_DVD:
            ImageList_Draw( bannerList, file->icon[selected], DC,
                            rc.left, rc.top, ILD_NORMAL );
            break;

        case SELECTOR_FILE_EXEC:
            HICON hIcon;
            if(usel.smallIcons)
            {
                hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_GCN_SMALL_ICON));
                DrawIcon(DC, (rc.right - rc.left) / 2 - 4, rc.top, hIcon);
            }
            else
            {
                hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_GCN_ICON));
                DrawIcon(DC, (rc.right - rc.left) / 2 - 16, rc.top, hIcon);
            }
            DeleteObject(hIcon);
            break;
    }

    // other columns
    ListView_GetItemRect(usel.hSelectorWindow, ID, &rc, LVIR_LABEL);
    memset(&lvc, 0, sizeof(lvc));
    lvc.mask = LVCF_FMT | LVCF_WIDTH;

    for(int col=1; ListView_GetColumn(usel.hSelectorWindow, col, &lvc); col++)
    {
        char text[256];
        UINT fmt = DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER;
        lvcw.mask = LVCF_FMT;
        ListView_GetColumn(usel.hSelectorWindow, col, &lvcw);

        if(lvcw.fmt & LVCFMT_RIGHT)       fmt |= DT_RIGHT;
        else if(lvcw.fmt & LVCFMT_CENTER) fmt |= DT_CENTER;
        else                              fmt |= DT_LEFT;

        rc.left  = rc.right;
        rc.right+= lvc.cx;

        lvi.iSubItem   = col;
        lvi.pszText    = text;
        lvi.cchTextMax = sizeof(text);
        int len = SendMessage(usel.hSelectorWindow, LVM_GETITEMTEXT, (WPARAM)ID, (LPARAM)&lvi);

        rc2 = rc;
        FillRect(DC, &rc2, hb);
        rc2.left += 2;
        rc2.right-= 2;

        if( (file->id[3] == 'J') &&     // sick check (but no alternative)
            (col == 1 || col == 4))     // title or comment only
        {
            uint16_t *buf; uint32_t size, chars;
            buf = SjisToUnicode((uint8_t *)text, &size, &chars);
            DrawTextW(DC, (wchar_t *)buf, chars, &rc2, fmt);
            free(buf);
        } else DrawText(DC, text, len, &rc2, fmt);
    }
}

// update filelist (reload and redraw)
void UpdateSelector()
{
    char search[2 * MAX_PATH], *mask[] = { "*.dol", "*.elf", "*.gcm", "*.iso", NULL };
    char found[2 * MAX_PATH];
    int  type[] = { SELECTOR_FILE_EXEC, SELECTOR_FILE_EXEC, SELECTOR_FILE_DVD, SELECTOR_FILE_DVD };
    WIN32_FIND_DATA fd;
    HANDLE hfff;
    int dir = 0;

    // opened ?
    if(!usel.opened) return;

    // destroy old filelist and path data
    ListView_DeleteAllItems(usel.hSelectorWindow);
    if(usel.files)
    {
        free(usel.files);
        usel.files = NULL;
        usel.filenum = 0;
    }
    ImageList_Remove(bannerList, -1);

    // build search path list (even if selector closed)
    load_path();
    //list_path();

    // load file filter
    usel.filter = GetConfigInt(USER_FILTER, USER_FILTER_DEFAULT);

    // search all directories
    while(dir < usel.pathnum)
    {
        int m = 0;
        uint32_t filter = MEMSwap(usel.filter);

        while(mask[m])
        {
            uint8_t allow = (uint8_t)filter;
            filter >>= 8;
            if(!allow) { m++; continue; }

            sprintf_s(search, sizeof(search), "%s%s", usel.paths[dir], mask[m]);

            hfff = FindFirstFile(search, &fd);
            if(hfff != INVALID_HANDLE_VALUE)
            {
                do
                {
                    if(~fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        sprintf_s(found, sizeof(found), "%s%s", usel.paths[dir], fd.cFileName);
                        add_file(found, fd.nFileSizeLow, type[m]);
                    }
                }
                while(FindNextFile(hfff, &fd));
            }
            FindClose(hfff);

            // next mask
            m++;
        }

        // next directory
        dir++;
    }

    // update selector window
    UpdateWindow(usel.hSelectorWindow);

    // re-sort (we need to save old value, to avoid swap of filelist)
    int oldSort = usel.sortBy;
    usel.sortBy = 0;
    SortSelector(oldSort);
}

int SelectorGetSelected()
{
    int item = ListView_GetNextItem(usel.hSelectorWindow, -1, LVNI_SELECTED);
    if(item == -1) return -1;
    usel.selected = &usel.files[item];
    return item;
}

void SelectorSetSelected(int item)
{
    if(item >= usel.filenum) return;
    ListView_SetItemState(usel.hSelectorWindow, item, LVNI_SELECTED, LVNI_SELECTED);
    ListView_EnsureVisible(usel.hSelectorWindow, item, FALSE);
    usel.selected = &usel.files[item];
}

// if file not present, keep selection unchanged
void SelectorSetSelected(char *filename)
{
    for(int i=0; i<usel.filenum; i++)
    {
        if(!_stricmp(filename, usel.files[i].name))
        {
            SelectorSetSelected(i);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// controls

// return item string data
static void getdispinfo(LPNMHDR pnmh)
{
    LV_DISPINFO *lpdi = (LV_DISPINFO *)pnmh;
    UserFile    *file = &usel.files[lpdi->item.lParam];
    char        *len  = FileSmartSize(file->size);

    #define DISP(n, text) case n: strcpy(lpdi->item.pszText, text); break
    switch(lpdi->item.iSubItem)
    {
        DISP(0, " ");
        DISP(1, file->title);
        DISP(2, len);
        DISP(3, file->id);
        DISP(4, file->comment);
    }
}

static void columnclick(int col)
{
    switch(col)
    {
        case 0: SortSelector(SELECTOR_SORT_DEFAULT); break;
        case 1: SortSelector(SELECTOR_SORT_TITLE  ); break;
        case 2: SortSelector(SELECTOR_SORT_SIZE   ); break;
        case 3: SortSelector(SELECTOR_SORT_ID     ); break;
        case 4: SortSelector(SELECTOR_SORT_COMMENT); break;
    }
}

static void mouseclick(int rmb)
{
    int item = ListView_GetNextItem(usel.hSelectorWindow, -1, LVNI_SELECTED);
    UserFile *file = &usel.files[item];

    if(item == -1)          // empty field
    {
        SetStatusText(STATUS_PROGRESS, "Idle");
    }
    else                    // file selected
    {
        // show item number for debugger's 'boot {n}' command
        char progress[1024];
        sprintf_s(progress, sizeof(progress), "#%i: %s", item + 1, file->name);
        SetStatusText(STATUS_PROGRESS, progress);
    }

    if(rmb && item != -1)   // popup file menu
    {
        usel.hFileMenu = GetSubMenu(wnd.hPopupMenu, ID_POPUP_FILE);

        SetMenuItemText(usel.hFileMenu, ID_FILE_COMPRESS, "&Compress...");

        // hide "Compress..", if file is executable
        if(file->type == SELECTOR_FILE_EXEC)
        {
            EnableMenuItem(usel.hFileMenu, ID_FILE_COMPRESS, MF_GRAYED);
        }
        else
        {
            EnableMenuItem(usel.hFileMenu, ID_FILE_COMPRESS, MF_ENABLED);
        }

        POINT p;
        GetCursorPos(&p);

        TrackPopupMenu(
            usel.hFileMenu, 
            TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_HORIZONTAL, 
            p.x, p.y, 0,
            wnd.hMainWindow, NULL
        );
    }
}

static void doubleclick()
{
    int item = ListView_GetNextItem(usel.hSelectorWindow, -1, LVNI_SELECTED);
    if(item == -1) return;

    UserFile *file = &usel.files[item];
    char filename[256];
    strcpy_s(filename, sizeof(filename), file->name);

    // load file
    LoadFile(filename);
    AddRecentFile(filename);
    PostMessage(wnd.hMainWindow, WM_COMMAND, (WPARAM)ID_FILE_RELOAD, (LPARAM)0);
}

void NotifySelector(LPNMHDR pnmh)
{
    // opened ?
    if(!usel.opened) return;

    switch(pnmh->code)
    {
        case LVN_COLUMNCLICK: columnclick(((NM_LISTVIEW *)pnmh)->iSubItem); break;
        case LVN_GETDISPINFO: getdispinfo(pnmh); break;
        case NM_CLICK       : mouseclick(0); break;
        case NM_RCLICK      : mouseclick(1); break;
        case NM_RETURN      : doubleclick(); break;
        case NM_DBLCLK      : doubleclick(); break;
    }
}

// set selected item, by first letter key pressed
void ScrollSelector(int letter)
{
    letter = tolower(letter);
    for(int n=0; n<usel.filenum; n++)
    {
        UserFile *file = &usel.files[n];
        int c = tolower(file->title[0]);
        if(c == letter)
        {
            SelectorSetSelected(n);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// sort (using C qsort() function)

static int sort_by_type(const void *cmp1, const void *cmp2)
{
    UserFile *f1 = (UserFile *)cmp1, *f2 = (UserFile *)cmp2;
    return (f2->type - f1->type);
}

static int sort_by_filename(const void *cmp1, const void *cmp2)
{
    UserFile *f1 = (UserFile *)cmp1, *f2 = (UserFile *)cmp2;
    return _stricmp(f1->name, f2->name);
}

static int sort_by_title(const void *cmp1, const void *cmp2)
{
    UserFile *f1 = (UserFile *)cmp1, *f2 = (UserFile *)cmp2;
    return _stricmp(f1->title, f2->title);
}

static int sort_by_size(const void *cmp1, const void *cmp2)
{
    UserFile *f1 = (UserFile *)cmp1, *f2 = (UserFile *)cmp2;
    return (f1->size - f2->size);
}

static int sort_by_gameid(const void *cmp1, const void *cmp2)
{
    UserFile *f1 = (UserFile *)cmp1, *f2 = (UserFile *)cmp2;
    return strcmp(f1->id, f2->id);
}

static int sort_by_comment(const void *cmp1, const void *cmp2)
{
    UserFile *f1 = (UserFile *)cmp1, *f2 = (UserFile *)cmp2;
    return _stricmp(f1->comment, f2->comment);
}

// count DVD files in list
static int get_dvd_files()
{
    int sum = 0;
    for(int i=0; i<usel.filenum; i++)
    {
        UserFile *file = &usel.files[i];
        if(file->type == SELECTOR_FILE_DVD) sum++;
    }
    return sum;
}

void SortSelector(int sortBy)
{
    // opened ?
    if(!usel.opened) return;

    // sort
    if(usel.sortBy != sortBy)
    {
        int dvds = get_dvd_files();

        switch(sortBy)
        {
            case SELECTOR_SORT_DEFAULT:
                qsort(usel.files, usel.filenum, sizeof(UserFile), sort_by_type);
                qsort(usel.files, dvds, sizeof(UserFile), sort_by_title);
                qsort(&usel.files[dvds], usel.filenum-dvds, sizeof(UserFile), sort_by_title);
                break;
            case SELECTOR_SORT_FILENAME:
                qsort(usel.files, usel.filenum, sizeof(UserFile), sort_by_filename);
                break;
            case SELECTOR_SORT_TITLE:
                qsort(usel.files, usel.filenum, sizeof(UserFile), sort_by_title);
                break;
            case SELECTOR_SORT_SIZE:
                qsort(usel.files, usel.filenum, sizeof(UserFile), sort_by_size);
                break;
            case SELECTOR_SORT_ID:
                qsort(usel.files, usel.filenum, sizeof(UserFile), sort_by_gameid);
                break;
            case SELECTOR_SORT_COMMENT:
                qsort(usel.files, usel.filenum, sizeof(UserFile), sort_by_comment);
                break;
            default:
                return;
        }

        usel.sortBy = sortBy;
        SetConfigInt(USER_SORTVIEW, usel.sortBy);
    }
    else
    {
        // swap it, if same sort method
        for(int i=0; i<usel.filenum/2; i++)
        {
            UserFile tmp = usel.files[i];
            usel.files[i]  = usel.files[usel.filenum-i-1];
            usel.files[usel.filenum-i-1] = tmp;
        }
    }

    // rebuild filelist
    ListView_DeleteAllItems(usel.hSelectorWindow);
    for(int i=0; i<usel.filenum; i++) add_item(i);
}

// ---------------------------------------------------------------------------
// management (create/close selector)

// two flags are controlling selector view : "active" and "opened". selector
// cannot be opened, if it is not active. therefore, create/close calls
// should check for "active" flag first, then check "opened" flag. there is no
// need to check "active" flags for other calls, because if it is not "active"
// it cannot be "opened".

void CreateSelector()
{
    // allowed ?
    if(!usel.active)
    {
        load_path();
        return;
    }

    // already created ?
    if(usel.opened) return;

    HWND parent = wnd.hMainWindow;
    HINSTANCE hinst = GetModuleHandle(NULL);
    InitCommonControls();

    // check DVD accesibility. set filter to
    // *dol/*elf only, if DVD plugin is not loaded
    if(DVDSetCurrent == NULL)
    {
        SetConfigInt(USER_FILTER, 0xffff0000);
    }

    // create selector window
    usel.hSelectorWindow = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
                            WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER |
                            LVS_OWNERDRAWFIXED | LVS_SINGLESEL | LVS_REPORT,
                            0,0,0,0,
                            parent, (HMENU)ID_SELECTOR, hinst, NULL);
    if(usel.hSelectorWindow == NULL) return;

    EnableWindow(usel.hSelectorWindow, TRUE);
    ShowWindow(usel.hSelectorWindow, SW_SHOW);
    SetFocus(usel.hSelectorWindow);

    // retrieve icon size
    bool iconSize = GetConfigInt(USER_SMALLICONS, USER_SMALLICONS_DEFAULT);

    // set "opened" flag (for following calls)
    usel.opened = TRUE;

    // update selector view, by changing icon size
    SetSelectorIconSize(iconSize);

    // sort files
    usel.sortBy = 0;
    SortSelector(GetConfigInt(USER_SORTVIEW, USER_SORTVIEW_DEFAULT));

    // scroll to last loaded file
    SelectorSetSelected(GetConfigString(USER_LASTFILE, USER_LASTFILE_DEFAULT));
}

void CloseSelector()
{
    // allowed ?
    if(!usel.active) return;

    // already closed ?
    if(!usel.opened) return;

    // destroy filelist
    if(usel.files)
    {
        free(usel.files);
        usel.files = NULL;
        usel.filenum = 0;
    }

    // destroy bannerlist
    ImageList_Remove(bannerList, -1);
    ImageList_Destroy(bannerList);
    bannerList = NULL;

    // destroy window
    ListView_DeleteAllItems(usel.hSelectorWindow);
    DestroyWindow(usel.hSelectorWindow);
    usel.hSelectorWindow = NULL;
    SetFocus(wnd.hMainWindow);

    // clear "opened" flag
    usel.opened = FALSE;
}

// 0: large, 1:small
void SetSelectorIconSize(bool smallIcon)
{
    // opened ?
    if(!usel.opened) return;

    usel.smallIcons = smallIcon & 1;    // protect bool
    SetConfigInt(USER_SMALLICONS, usel.smallIcons);

    // destroy bannerlist
    if(bannerList)
    {
        ImageList_Remove(bannerList, -1);
        ImageList_Destroy(bannerList);
        bannerList = NULL;
    }

    // create banners imagelist
    if(bannerList == NULL)
    {
        int w = (usel.smallIcons) ? (DVD_BANNER_WIDTH >> 1) : (DVD_BANNER_WIDTH);
        int h = (usel.smallIcons) ? (DVD_BANNER_HEIGHT >> 1) : (DVD_BANNER_HEIGHT);
        bannerList = ImageList_Create(w, h, ILC_COLOR24, 10, 10);
        if(bannerList == NULL)
        {
            DolwinReport("Failed to create selector image list.");
            CloseSelector();
            return;
        }
        ListView_SetImageList(usel.hSelectorWindow, bannerList, LVSIL_SMALL);
    }

    // resize and update
    RECT rc;
    GetClientRect(wnd.hMainWindow, &rc);
    ResizeSelector( (WORD)(rc.right - rc.left), 
                    (WORD)(rc.bottom - rc.top) );
    UpdateSelector();
}
