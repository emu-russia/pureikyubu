#pragma once

#include <string>
#include <vector>
#include <list>
#include <map>
#include <algorithm>
#include <cassert>
#include <atomic>
#include <cstdarg>
#include <unordered_map>

#ifdef _LINUX
#include <string.h>
#endif

#include "../Common/ByteSwap.h"
#include "../Common/Spinlock.h"
#include "../Common/Thread.h"
#include "../Common/Json.h"
#include "../Common/Jdi.h"
#include "../Common/File.h"
#include "../Common/String.h"

#include "../GekkoCore/GekkoCore.h"
#include "../GekkoCore/GekkoDisasm.h"
#include "../HighLevel/HighLevel.h"
#include "../Hardware/HWConfig.h"
#include "../DSP/DSP.h"
#include "../GX/GXCore.h"
#include "../DVD/DVD.h"
#include "../Hardware/HW.h"
#include "../Hardware/MI.h"
#include "../Hardware/PI.h"

#include "Debugger.h"

extern Gekko::GekkoCore* Core;
