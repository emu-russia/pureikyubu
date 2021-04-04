// interpreter tables setup
#include "../pch.h"
#include "InterpreterPrivate.h"

using namespace Debug;

namespace Gekko
{

	// setup extension tables
	void Interpreter::InitTables()
	{
		int i;
		uint8_t scale;

		// build rotate mask table
		for (int mb = 0; mb < 32; mb++)
		{
			for (int me = 0; me < 32; me++)
			{
				uint32_t mask = ((uint32_t)-1 >> mb) ^ ((me >= 31) ? 0 : ((uint32_t)-1) >> (me + 1));
				rotmask[mb][me] = (mb > me) ? (~mask) : (mask);
			}
		}

		// build paired-single load scale
		for (scale = 0; scale < 64; scale++)
		{
			int factor;
			if (scale & 0x20)    // -32 ... -1
			{
				factor = -32 + (scale & 0x1f);
			}
			else                // 0 ... 31
			{
				factor = 0 + (scale & 0x1f);
			}
			ldScale[scale] = powf(2, -1.0f * (float)factor);
		}

		// build paired-single store scale
		for (scale = 0; scale < 64; scale++)
		{
			int factor;
			if (scale & 0x20)    // -32 ... -1
			{
				factor = -32 + (scale & 0x1f);
			}
			else                // 0 ... 31
			{
				factor = 0 + (scale & 0x1f);
			}
			stScale[scale] = powf(2, +1.0f * (float)factor);
		}
	}

}
