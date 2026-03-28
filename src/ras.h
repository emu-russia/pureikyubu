#pragma once

// The GFX Engine contains three rasterizers, that run in parallel:
// - RAS0: edge rasterization
// - RAS1: texture coordinate rasterization
// - RAS2: color rasterization

namespace GFX
{
	#define RAS1_PERF_ID 0x24
	#define RAS1_SS0_ID 0x25
	#define RAS1_SS1_ID 0x26
	#define RAS1_IREF_ID 0x27
	#define RAS1_TREF0_ID 0x28
	#define RAS1_TREF1_ID 0x29
	#define RAS1_TREF2_ID 0x2A
	#define RAS1_TREF3_ID 0x2B
	#define RAS1_TREF4_ID 0x2C
	#define RAS1_TREF5_ID 0x2D
	#define RAS1_TREF6_ID 0x2E
	#define RAS1_TREF7_ID 0x2F

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
		
		void loadRASReg(size_t index, uint32_t value);
	};
}