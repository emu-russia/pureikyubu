#pragma once

// compiler and Windows API includes
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include <windows.h>
#include <filesystem>

#include "../Common/Thread.h"
#include "../Common/Jdi.h"
#include "../Common/File.h"

#include "../Debugger/Debugger.h"
#include "../GekkoCore/Gekko.h"

#include "DVD.h"
#include "GCM.h"            // very simple GCM reading (for .gcm files)
#include "DduCommands.h"

#include "Mn102Analyzer.h"
#include "Mn102Disasm.h"
#include "DvdAdpcmDecode.h"

#include "../../ThirdParty/fmt/fmt/format.h"
