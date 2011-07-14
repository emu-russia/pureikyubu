// common include file

#define GX_VER "0.7"

//#define WIREFRAME
#define NO_VIEWPORT
#define TEXMODE     GL_MODULATE

// system includes
#include <direct.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include "resource.h"

// these data types are commonly used for GCDEV
typedef signed   char       s8;
typedef signed   short      s16;
typedef signed   long       s32;
typedef signed   __int64    s64;
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned long       u32;
typedef unsigned __int64    u64;

// color type
typedef union
{
    struct { u8     A, B, G, R; };
    u32     RGBA;
} Color;

// other project includes
#include "PlugSpec.h"
#include "Plug.h"
#include "Perf.h"
#include "GPL.h"
#include "XF.h"
#include "GL.h"
#include "Fifo.h"
#include "Light.h"
#include "Tex.h"
#include "Texgen.h"
#include "Tev.h"
#include "GPRegs.h"
