#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <cassert>
#include <atomic>
#include <string.h>
#include <cstdarg>
#include <unordered_map>
#include <math.h>
#include <limits.h>
#include <fstream>

#ifdef _WINDOWS
#include <conio.h>
#include <windows.h>
#include <shlobj.h>
#include <commctrl.h>
#include "res/resource.h"
#endif

#ifdef _LINUX
#include <memory.h>
#include <string.h>
#include <unistd.h>		// usleep
#include <pthread.h>
#include <signal.h>
#include <libgen.h>		// dirname / basename
#include <sys/types.h>
#include <sys/stat.h>	// _wstat (IsDirectory)
#include <dirent.h>		// BuildFileTree
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _wcsicmp wcscasecmp
#endif

#include "../thirdparty/fmt/fmt/format.h"
#include "../thirdparty/fmt/fmt/printf.h"

#define my_max(a,b) (((a) > (b)) ? (a) : (b))
#define my_min(a,b) (((a) < (b)) ? (a) : (b))

#include "utils.h"
#include "json.h"
#include "jdi.h"
#include "jdiserver.h"

#include "gekkodec.h"
#include "gekko.h"
#include "gekkoc.h"
#include "gekkodisasm.h"
#include "gekkodebug.h"

#include "dspdec.h"
#include "dspcore.h"
#include "dsp.h"
#include "dspdisasm.h"
#include "dspdebug.h"

#include "dvd.h"
#include "dvddebug.h"

#include "flipper.h"
#include "gfx.h"
#include "ai.h"
#include "cp.h"
#include "pe.h"
#include "vi.h"
#include "pi.h"
#include "mem.h"
#include "di.h"
#include "exi.h"
#include "memcard.h"
#include "bootrtc.h"
#include "pad.h"
#ifdef _WINDOWS
#include "audio.h"
#else
#include "audionull.h"
#endif
#include "xfb.h"
#include "si.h"
#include "flipperdebug.h"
#include "cp.h"

// TODO: Phased out
#include "../thirdparty/DolwinVideo/GX.h"
#include "../thirdparty/GX/GXCore.h"

namespace Flipper
{
	// TODO: I do not like these lonely definitions, which, moreover, have to be created far away in the emulation module (Emulator.cpp).
	// Need to make one single class for the Flipper ASIC and move them there.
	extern DSP::Dsp16* DSP;
	extern GX::GXCore* Gx;
}

#include "sym.h"
#include "os.h"
#include "osdebug.h"

#include "debug.h"
#ifdef _WINDOWS
#include "cui.h"
#include "debugui.h"
#endif

#include "config.h"
#include "main.h"
#include "ui.h"

#define _TB(s)
#define _TE()

extern Gekko::GekkoCore* Core;
