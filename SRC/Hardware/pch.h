#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <cassert>
#include <atomic>
#include <string.h>
#include <unordered_map>

#include "../Common/ByteSwap.h"
#include "../Common/Spinlock.h"
#include "../Common/Thread.h"
#include "../Common/Json.h"
#include "../Common/Jdi.h"
#include "../Common/File.h"
#include "../Common/String.h"

#include "HWConfig.h"

#include "../DVD/DVD.h"
#include "../DSP/DSP.h"
#include "../GX/GXCore.h"
#include "HW.h"
#include "AI.h"
#include "CP_PE.h"
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

// For now Null backends for Linux builds

#ifdef _LINUX
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
