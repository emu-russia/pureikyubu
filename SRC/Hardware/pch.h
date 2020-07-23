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
#include <codecvt>
#include <sys/stat.h>
#include <algorithm>
#include <atomic>
#include <Windows.h>

#include "../Common/Thread.h"
#include "../Common/Spinlock.h"
#include "../Common/Jdi.h"
#include "../Common/File.h"
#include "../Common/String.h"

#include "HWConfig.h"

#include "../DVD/DVD.h"
#include "../DSP/DspCore.h"
#include "HW.h"
#include "EFB.h"
#include "AI.h"
#include "CP.h"
#include "VI.h"
#include "PI.h"
#include "MI.h"
#include "AR.h"
#include "DI.h"
#include "EXI.h"
#include "MC.h"
#include "IPLDescrambler.h"

#pragma region "Backends"

#ifdef _WINDOWS
#include "../Backends/DolwinVideo/GX.h"
#include "../Backends/PadSimpleWin32/PAD.h"
#include "../Backends/VideoGdi/VideoOut.h"
#include "../Backends/AudioDirectSound/AX.h"
#endif

#ifdef _PLAYGROUND_WINDOWS
#include "../Backends/GraphicsNull/GX.h"
#include "../Backends/PadNull/PAD.h"
#include "../Backends/VideoNull/VideoOut.h"
#include "../Backends/AudioNull/AX.h"
#endif

#pragma endregion "Backends"

#include "SI.h"

#include "HwCommands.h"

#include "../Debugger/Debugger.h"

#include "../GekkoCore/Gekko.h"

#include "../HighLevel/TimeFormat.h"

#include "FIFO.h"
