// Dump DolphinOS threads.

#pragma once

namespace HLE
{
	void DumpDolphinOsThreads();

	Json::Value* DumpDolphinOsContext(uint32_t effectiveAddr, bool displayOnScreen);
}
