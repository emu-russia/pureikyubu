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
