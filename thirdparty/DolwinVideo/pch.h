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

#include "../../src/utils.h"
#include "../../src/json.h"
#include "../../src/jdi.h"
#include "../../src/flipper.h"
#include "../../src/debug.h"

#include "GL.h"
#include "GX.h"
#include "FifoProcessor.h"
#include "GPRegs.h"
#include "Fifo.h"
#include "Tex.h"
#include "XF.h"
#include "Backend.h"
