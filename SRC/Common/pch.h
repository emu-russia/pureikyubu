#pragma once

#include <string>
#include <locale>
#include <codecvt>
#include <fstream>
#ifdef _WINDOWS
#include <windows.h>
#endif

#include "Spinlock.h"
#include "Thread.h"
#include "Json.h"
#include "Jdi.h"
#include "String.h"

#include "../Debugger/Report.h"
