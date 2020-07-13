
#pragma once

#include <stdint.h>
#include <intrin.h>
#include <fstream>
#include <algorithm>

#include "../Common/Spinlock.h"
#include "../Common/Json.h"
#include "../Common/Jdi.h"

#include "../Core/Gekko.h"				// For TimeBase
#include "../Hardware/Hardware.h"
#include "../Debugger/Debugger.h"
#include "../Debugger/EventLog.h"
#include "../UI/UserFile.h"

#include "DspCore.h"
#include "DspAnalyzer.h"
#include "DspInterpreter.h"
#include "DspDisasm.h"
#include "DspCommands.h"

#include "../../ThirdParty/fmt/fmt/format.h"

#if 0
#define _TLIB_STRINGIFY1(s) _TLIB_STRINGIFY2(s)
#define _TLIB_STRINGIFY2(s) #s
#define _TB(s) if (Debug::Log) { Debug::Log->TraceBegin(DbgChannel::DSP, _TLIB_STRINGIFY2(s)); }
#define _TE() if (Debug::Log) { Debug::Log->TraceEnd(DbgChannel::DSP); }
#else
#define _TB(s)
#define _TE()
#endif
