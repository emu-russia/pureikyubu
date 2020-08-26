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

#include "../../Debugger/Debugger.h"

#include "../../Hardware/HWConfig.h"

#include "Tex.h"
#include "XF.h"
#include "Backend.h"
