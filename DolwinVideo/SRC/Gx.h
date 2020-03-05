// common include file

#define GX_VER "0.7"

//#define WIREFRAME
#define NO_VIEWPORT
#define TEXMODE     GL_MODULATE

// system includes
#include <stdint.h>
#include <direct.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>

// color type
typedef union
{
    struct { uint8_t     A, B, G, R; };
    uint32_t     RGBA;
} Color;

// other project includes
#include "Plug.h"
#include "Perf.h"
#include "GPL.h"
#include "XF.h"
#include "GL.h"
#include "Stages.h"
#include "Fifo.h"
#include "Light.h"
#include "Tex.h"
#include "Texgen.h"
#include "Tev.h"
#include "GPRegs.h"
