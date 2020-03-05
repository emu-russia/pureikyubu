// perfomance monitor (shows on-screen info)
#include "GX.h"

static  UINT    perftex;
static  uint8_t *fontbuf;

void PerfInit()
{
    HINSTANCE hInst = hPlugin;
    HANDLE hRes, hResLoad;

    FILE* f = fopen("Data\\FONT.BMP", "rb");
    VERIFY(f == NULL);

    fseek(f, 0, SEEK_END);
    int fontSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    fontbuf = new uint8_t [fontSize];
    VERIFY(fontbuf == NULL);

    fread(fontbuf, 1, fontSize, f);
    fclose(f);

    glGenTextures(1, &perftex);
    glBindTexture(GL_TEXTURE_2D, perftex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 256, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, fontbuf + 40);
}

void PerfClose()
{
    if (fontbuf)
    {
        delete[] fontbuf;
    }
}

void PerfPrintf(int x, int y, char *fmt, ...)
{
    va_list arg;
    int len, x0 = x;
    char buf[0x1000];
    uint8_t *ptr = (uint8_t*)buf;

    y = (480-16) - y;

    va_start(arg, fmt);
    len = vsprintf(buf, fmt, arg);
    va_end(arg);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_TEXTURE_2D);

    gfx->SetViewport(0, 0, 640, 480, 1, 16777215.0f);
    gfx->SetScissor(0, 0, 640, 480);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 640, 0, 480, -100, 100);

    glBindTexture(GL_TEXTURE_2D, perftex);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor3ub(0, 255, 255);

    while(len--)
    {
        uint8_t c = *ptr++;
        float cx, cy;

        if(c == '\0') break;
        if(c == '\n') { y -= 16; x = x0; continue; }
        if(c < ' ') continue;

        c -= 16;
        c += 128;

        cx = float(c % 16) / 16.0f;
        cy = 1.0f - (float(c / 16) / 16.0f);

        glBegin(GL_QUADS);
        {
            glTexCoord2f(cx, cy);
            glVertex2i(x+0, y+0);
            glTexCoord2f(cx, cy+0.0625f);
            glVertex2i(x, y+16);
            glTexCoord2f(cx+0.0625f, cy+0.0625f);
            glVertex2i(x+16, y+16);
            glTexCoord2f(cx+0.0625f, cy);
            glVertex2i(x+16, y+0);
        }
        glEnd();

        x += 16;
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

    // restore z mode
    static uint32_t glzf[] = {
        GL_NEVER,
        GL_LESS,
        GL_EQUAL,
        GL_LEQUAL,
        GL_GREATER,
        GL_NOTEQUAL,
        GL_GEQUAL,
        GL_ALWAYS
    };
    if(bpRegs.zmode.enable)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(glzf[bpRegs.zmode.func]);
        glDepthMask(bpRegs.zmode.mask);
    }
    else glDisable(GL_DEPTH_TEST);
}
