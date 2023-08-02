
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

#include "GXCore.h"
#include "GXCommands.h"

#include "../Debugger/Debugger.h"

#include "../GekkoCore/GekkoCore.h"

extern Gekko::GekkoCore* Core;

#include "../DSP/DSP.h"

#include "../Hardware/HWConfig.h"
#include "../Hardware/HW.h"
#include "../Hardware/MI.h"
#include "../Hardware/PI.h"
