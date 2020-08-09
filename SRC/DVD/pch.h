#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <cassert>
#include <atomic>
#include <string.h>

#include "../Common/ByteSwap.h"
#include "../Common/Spinlock.h"
#include "../Common/Thread.h"
#include "../Common/Json.h"
#include "../Common/Jdi.h"
#include "../Common/File.h"
#include "../Common/String.h"

#include "../Debugger/Debugger.h"
#include "../GekkoCore/Gekko.h"

#include "DVD.h"
#include "GCM.h"
#include "DduCommands.h"

#include "Mn102Analyzer.h"
#include "Mn102Disasm.h"
#include "DvdAdpcmDecode.h"

#include "../../ThirdParty/fmt/fmt/format.h"

#define my_max(a,b) (((a) > (b)) ? (a) : (b))
#define my_min(a,b) (((a) < (b)) ? (a) : (b))

#ifdef _LINUX
#define _stricmp strcasecmp
#endif
