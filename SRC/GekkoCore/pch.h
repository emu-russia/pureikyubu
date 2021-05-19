#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <cassert>
#include <atomic>
#include <math.h>
#include <string.h>

#include "../Common/ByteSwap.h"
#include "../Common/Spinlock.h"
#include "../Common/Thread.h"
#include "../Common/Json.h"
#include "../Common/Jdi.h"

#include "../IntelCore/IntelCore.h"

#include "Config.h"
#include "Gekko.h"
#include "GekkoAnalyzer.h"
#include "Interpreter.h"
#include "GekkoCommands.h"
#include "GekkoDisasm.h"
#include "GekkoAssembler.h"
#include "Jitc.h"
#include "TLB.h"
#include "Cache.h"

#include "../Hardware/HWConfig.h"
#include "../Hardware/MI.h"
#include "../Hardware/PI.h"
#include "../Debugger/Debugger.h"

#ifdef _LINUX
#define _stricmp strcasecmp
#endif

using namespace IntelCore;
