
#pragma once

#include <cassert>
#include <cstdint>
#include <direct.h>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <windows.h>
#include <commctrl.h>
#include <intrin.h>
#include <tchar.h>
#include <fstream>
#include <filesystem>
#include <string>
#include <list>
#include <string>
#include <atomic>
#include <vector>
#include <array>
#include <codecvt>
#include <memory>
#include <locale>
#include "RES/resource.h"

#include "../Common/Spinlock.h"
#include "../Common/Json.h"
#include "../Common/Jdi.h"
#include "../Common/WinAPI.h" /* Windows API abstraction. */
#include "../Common/String.h"
#include "../GekkoCore/Gekko.h"
#include "../Hardware/Hardware.h"
#include "../HighLevel/TimeFormat.h"
#include "../Debugger/Debugger.h"
#include "../Debugger/DspDebugger.h"
#include "../Debugger/EventLog.h"
#include "../Dolwin/Emulator.h"
#include "../Dolwin/Loader.h"

#include "../../../../ThirdParty/fmt/fmt/format.h"
#include "../../../../ThirdParty/fmt/fmt/printf.h"

#include "User.h"

#include "SjisTable.h"
