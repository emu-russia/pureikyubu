
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <atomic>
#include <cassert>
#include <cstring>
#include <unordered_map>

#include "../Common/Spinlock.h"
#include "../Common/Json.h"
#include "../Common/Jdi.h"
#include "../Common/File.h"
#include "../Common/String.h"
#include "../Common/Thread.h"
#include "../Common/ByteSwap.h"

#include "../Hardware/HWConfig.h"

#include "../GekkoCore/Gekko.h"				// For TimeBase

#include "../Debugger/Debugger.h"
#include "../Debugger/EventLog.h"

#include "DSP.h"
#include "DspDisasm.h"
#include "DspCommands.h"

#include "../GX/GXCore.h"
#include "../Hardware/AI.h"
#include "../Hardware/AR.h"
#include "../Hardware/HW.h"
#include "../Hardware/MI.h"

#if 0
#define _TLIB_STRINGIFY1(s) _TLIB_STRINGIFY2(s)
#define _TLIB_STRINGIFY2(s) #s
#define _TB(s) if (Debug::Log) { Debug::Log->TraceBegin(DbgChannel::DSP, _TLIB_STRINGIFY2(s)); }
#define _TE() if (Debug::Log) { Debug::Log->TraceEnd(DbgChannel::DSP); }
#else
#define _TB(s)
#define _TE()
#endif

#ifdef _LINUX
#define _stricmp strcasecmp
#endif
