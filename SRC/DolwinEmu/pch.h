#pragma once

//#include <vld.h>

#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <cassert>
#include <fstream>
#include <atomic>
#include <string.h>

#ifdef _LINUX
#include <unistd.h>		// usleep
#endif

#include "../Common/Spinlock.h"
#include "../Common/Thread.h"
#include "../Common/Json.h"
#include "../Common/Jdi.h"
#include "../Common/File.h"
#include "../Common/String.h"

#include "../GekkoCore/Gekko.h"
#include "../GekkoCore/Interpreter.h"
#include "../HighLevel/HighLevel.h"
#include "../Hardware/HWConfig.h"
#include "../DSP/DSP.h"
#include "../DVD/DVD.h"
#include "../Hardware/HW.h"
#include "../Hardware/MI.h"
#include "../Hardware/VI.h"
#include "../Debugger/Debugger.h"

#include "Config.h"
#include "ExecutableFormat.h"
#include "Loader.h"
#include "Emulator.h"

#include "../../ThirdParty/fmt/fmt/format.h"
#include "../../ThirdParty/fmt/fmt/printf.h"
