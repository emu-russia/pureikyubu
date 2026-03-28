// GFX Bump Mapping Unit
#include "pch.h"

// For now it's just a placeholder

namespace GFX
{
	BumpMappingUnit::BumpMappingUnit(HWConfig* config, GFXCore* parent_gfx)
	{
		gfx = parent_gfx;
	}

	BumpMappingUnit::~BumpMappingUnit()
	{
	}

	void BumpMappingUnit::loadBUMPReg(size_t index, uint32_t value)
	{
		gfx->tx->loadTXReg(index, value);
	}
}