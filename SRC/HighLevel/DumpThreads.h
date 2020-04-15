// Dump DolphinOS threads.

#pragma once

namespace HLE
{
	Json::Value* DumpDolphinOsThreads(bool displayOnScreen);

	Json::Value* DumpDolphinOsContext(uint32_t effectiveAddr, bool displayOnScreen);
}
