#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <cassert>
#include <fstream>
#include <atomic>
#include <string.h>
#include <math.h>
#include <limits.h>

#include "../Common/ByteSwap.h"
#include "../Common/Spinlock.h"
#include "../Common/Thread.h"
#include "../Common/Json.h"
#include "../Common/Jdi.h"
#include "../Common/File.h"
#include "../Common/String.h"

#include "../GekkoCore/Gekko.h"

#include "HighLevel.h"

#include "../Hardware/HWConfig.h"
#include "../DVD/DVD.h"
#include "../Hardware/EXI.h"
#include "../Hardware/MI.h"

#include "../Debugger/Debugger.h"

#include "HleCommands.h"
#include "TimeFormat.h"
#include "DumpThreads.h"

#ifdef _LINUX
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#endif
