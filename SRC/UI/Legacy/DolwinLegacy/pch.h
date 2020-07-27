
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
#include <shlobj.h>
#include <commctrl.h>
#include <intrin.h>
#include <tchar.h>
#include <fstream>
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

#include "../../../../ThirdParty/fmt/fmt/format.h"
#include "../../../../ThirdParty/fmt/fmt/printf.h"

#include "../../../Common/String.h"
#include "../../../Common/File.h"
#include "../../../Common/Json.h"

#include "../DebugConsole/DebugConsole.h"

#include "UserMain.h"           // application entrypoint
#include "UserJdiClient.h"
#include "UserConfig.h"
#include "UserFile.h"           // various file utilities
#include "UserSelector.h"       // file selector
#include "UserWindow.h"         // main window controls
#include "UserSettings.h"       // settings dialog
#include "UserFonts.h"          // bootrom fonts dialog
#include "UserMemcards.h"       // memcards dialog
#include "UserPadConfigure.h"
#include "UserAbout.h"          // about dialog
#include "DVDBanner.h"			// banner utilities for selector
#include "SjisTable.h"
#include "UserCommands.h"
