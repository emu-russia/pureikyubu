// Main module with interface for Flipper (HW)

#include "pch.h"

namespace GX
{

	GXCore::GXCore()
	{
		Reset();
	}

	GXCore::~GXCore()
	{

	}

	void GXCore::Reset()
	{
		memset(&state, 0, sizeof(state));
	}

}
