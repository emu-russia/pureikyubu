// Various commands for debugging hardware (Flipper)
#include "pch.h"

namespace Debug
{
	void hw_init_handlers()
	{

	}

	void hw_help()
	{
		DBReport("--- hw debug commands --------------------------------------------------------\n");
		DBReport("    ramload              - Load binary file to main memory\n");
		DBReport("    ramsave              - Save main memory content to file\n");
		DBReport("    aramload             - Load binary file to ARAM\n");
		DBReport("    aramsave             - Save ARAM content to file\n");
		DBReport("\n");
	}


};
