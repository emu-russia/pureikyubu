#pragma once

// The GFX Engine contains three rasterizers, that run in parallel:
// - RAS0: edge rasterization
// - RAS1: texture coordinate rasterization
// - RAS2: color rasterization

namespace GX
{
	enum RAS_Primitive : size_t
	{
		RAS_QUAD = 0,
		RAS_QUAD_STRIP,
		RAS_TRIANGLE,
		RAS_TRIANGLE_STRIP,
		RAS_TRIANGLE_FAN,
		RAS_LINE,
		RAS_LINE_STRIP,
		RAS_POINT,
	};
}
