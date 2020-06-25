#pragma once

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <intrin.h>
#include <time.h>
#include <tchar.h>
#include <Windows.h>

#include "../Common/Spinlock.h"
#include "../Common/Jdi.h"

#include "../Core/Gekko.h"
#include "../Core/GekkoDisasm.h"
#include "../Core/GekkoDisasmOld.h"			// PHASED OUT

#include "../Hardware/Hardware.h"
#include "../HighLevel/HighLevel.h"

#include "../Dolwin/Emulator.h"
#include "../UI/UserFile.h"

#include "Debugger.h"
#include "Cui.h"
#include "console.h"
#include "SamplingProfiler.h"
#include "DebugCommands.h"
#include "DspDebugger.h"
#include "EventLog.h"
