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
#include <Windows.h>

#include "../Common/Spinlock.h"
#include "../Common/Jdi.h"
#include "../Common/File.h"
#include "../Common/String.h"

#include "Hardware.h"
#include "HwCommands.h"

#include "../Debugger/Debugger.h"

#include "../GekkoCore/Gekko.h"

#include "../HighLevel/TimeFormat.h"

#include "FIFO.h"

// Backends

#include "../Backends/AudioDirectSound/AX.h"
