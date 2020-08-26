// common include file

#pragma once

//#define WIREFRAME
#define NO_VIEWPORT
#define TEXMODE     GL_MODULATE

#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <cassert>
#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>

#include "../../Common/Json.h"
#include "../../Common/Spinlock.h"
#include "../../Common/Thread.h"

// other project includes
#include "GL.h"
#include "FifoProcessor.h"
#include "Stages.h"
#include "Fifo.h"
#include "Tex.h"
#include "GPRegs.h"
#include "XF.h"

#include "../../Debugger/Debugger.h"

#include "../../Hardware/HWConfig.h"
#include "GX.h"
#include "Backend.h"

extern  HINSTANCE   hPlugin;
extern  HWND        hwndMain;
