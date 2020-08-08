#pragma once

#include <vector>
#include <list>
#include <map>
#include <string>
#include <cassert>
#include <atomic>

#ifdef _LINUX
#include <memory.h>
#include <string.h>
#define _stricmp strcasecmp
#endif

#include "Spinlock.h"
#include "Thread.h"
#include "Json.h"
#include "Jdi.h"
#include "String.h"
#include "File.h"

#include "../Debugger/Report.h"
