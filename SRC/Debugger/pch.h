#pragma once

#include <string>
#include <vector>
#include <list>
#include <map>
#include <algorithm>
#include <cassert>
#include <atomic>
#include <cstdarg>

#ifdef _LINUX
#include <string.h>
#endif

#include "../Common/Spinlock.h"
#include "../Common/Thread.h"
#include "../Common/Json.h"
#include "../Common/Jdi.h"
#include "../Common/File.h"
#include "../Common/String.h"

#include "../GekkoCore/Gekko.h"
#include "../HighLevel/HighLevel.h"
#include "../Hardware/HWConfig.h"
#include "../DSP/DSP.h"
#include "../GX/GXCore.h"
#include "../DVD/DVD.h"
#include "../Hardware/HW.h"
#include "../Hardware/PI.h"

#include "Debugger.h"
