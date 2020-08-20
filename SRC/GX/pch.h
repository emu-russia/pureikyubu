
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <cassert>
#include <atomic>

#include "../Common/ByteSwap.h"
#include "../Common/Spinlock.h"
#include "../Common/Thread.h"
#include "../Common/Json.h"
#include "../Common/Jdi.h"

#include "GXDefs.h"
#include "GXState.h"
#include "TexCache.h"
#include "TexConv.h"
#include "GXBackend.h"
#include "GXCore.h"
#include "GXCommands.h"
