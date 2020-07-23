// common include file

#pragma once

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
#include <intrin.h>
#include <assert.h>

#include <string>

#include "../../Common/Json.h"
#include "../../Common/Spinlock.h"
#include "../../Common/Thread.h"

// other project includes
#include "Config.h"
#include "Plug.h"
#include "Perf.h"
#include "GPL.h"
#include "XF.h"
#include "GL.h"
#include "FifoProcessor.h"
#include "Stages.h"
#include "Fifo.h"
#include "Light.h"
#include "Tex.h"
#include "Texgen.h"
#include "Tev.h"
#include "GPRegs.h"

#include "../../Debugger/Debugger.h"

#include "../../Hardware/HWConfig.h"
#include "GX.h"
