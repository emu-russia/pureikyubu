#pragma once

// The GFX Engine contains three rasterizers, that run in parallel:
// - RAS0: edge rasterization
// - RAS1: texture coordinate rasterization
// - RAS2: color rasterization

namespace GFX
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

	class Rasterizer
	{
		friend GFXCore;
		GFXCore* gfx = nullptr;

		bool ras_wireframe = false;			//!< Enable wireframe drawing of primitives (DEBUG)
		bool ras_use_texture = false;

	public:
		Rasterizer(HWConfig* config, GFXCore* parent_gfx);
		~Rasterizer();

		void RAS_Begin(RAS_Primitive prim, size_t vtx_num);
		void RAS_End();
		void RAS_SendVertex(const Vertex* v);
	};
}