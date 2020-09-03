
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <cassert>
#include <atomic>
#include <memory>

#include <windows.h>
#include <shlobj.h>
#include <commctrl.h>
#include "RES/resource.h"

#include "../../../Common/Spinlock.h"
#include "../../../Common/Thread.h"
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
#include "UserPerfMetrics.h"

#include "../../../../ThirdParty/fmt/fmt/format.h"
#include "../../../../ThirdParty/fmt/fmt/printf.h"
