
#pragma once

#include <vector>
#include <list>
#include <map>
#include <string>
#include <cstdio>
#include <atomic>

#ifdef _WINDOWS
#include <conio.h>
#include <Windows.h>
#endif

#include "../../Common/Spinlock.h"
#include "../../Common/Thread.h"
#include "../../Common/Json.h"
#include "../../Common/String.h"

#ifdef _LINUX
#include "../../Common/Json.h"
#include "../../Common/Jdi.h"
#include "../../DolwinEmu/JdiServer.h"
#endif

#include "UserJdiClient.h"
#include "UiCommands.h"
#include "DummyDebugger.h"
