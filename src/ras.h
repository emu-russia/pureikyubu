#pragma once

// The GFX Engine contains three rasterizers, that run in parallel:
// - RAS0: edge rasterization
// - RAS1: texture coordinate rasterization
// - RAS2: color rasterization
//
// In this emulator the rasterizers are not emulated geometrically: the primitives are handed to the
// OpenGL backend as-is. What *is* emulated here is the register state that the rasterizers own and
// that the rest of the pipeline (mainly TEV) needs - the per-stage texture bindings (RAS1_TREF).

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

	// Texture coordinate shift scale of the indirect stages (RAS1_SS0/SS1, gfx-ras1.md 4.2). The four
	// 4-bit fields are the S and T shift scales of two indirect stages: RAS1_SS0 holds the shifts of
	// the indirect stages 0 and 1 (GX_SetIndTexCoordScale of the stages 0/1) and RAS1_SS1 those of
	// the stages 2 and 3. A field value of 0..8 means "divide the coordinate by 2^value"; 9..15 are
	// unused (there is no entry for them in the RTL mask table).
	union RAS1_SS
	{
		struct
		{
			unsigned ss0 : 4;		// s shift scale of the first indirect stage of the pair
			unsigned ts0 : 4;		// t shift scale of the first indirect stage of the pair
			unsigned ss1 : 4;		// s shift scale of the second indirect stage of the pair
			unsigned ts1 : 4;		// t shift scale of the second indirect stage of the pair
			unsigned unused : 8;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// Texture coordinate / colour source reference (RAS1_TREF0..7). One register per pair of TEV stages.
	union RAS1_TREF
	{
		struct
		{
			unsigned ti0 : 3;		// Texture image id for the even stage
			unsigned tc0 : 3;		// Texture coordinate for the even stage
			unsigned te0 : 1;		// Texture enable for the even stage
			unsigned cc0 : 3;		// Colour source for the even stage (ras1_cc)
			unsigned pad0 : 2;
			unsigned ti1 : 3;		// Texture image id for the odd stage
			unsigned tc1 : 3;		// Texture coordinate for the odd stage
			unsigned te1 : 1;		// Texture enable for the odd stage
			unsigned cc1 : 3;		// Colour source for the odd stage
			unsigned pad1 : 2;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// Colour source of a TEV stage (ras1_cc)
	enum RasColorSource : unsigned
	{
		RAS1_CC_0 = 0,
		RAS1_CC_1,
		RAS1_CC_2,
		RAS1_CC_3,
		RAS2_CC_UNUSED,
		RAS1_CC_BUMP,
		RAS1_CC_BUMP_NRM,
		RAS1_CC_ZERO,
	};

	class TextureEnvironmentUnit;
	class GFXCore;

	class Rasterizer
	{
		friend GFXCore;
		friend TextureEnvironmentUnit;
		GFXCore* gfx = nullptr;

		RAS_Primitive current_prim = RAS_QUAD;
		size_t vertex_count = 0;

		RAS1_TREF tref[8]{};		// 0x28-0x2F
		RAS1_SS ss[2]{};			// 0x25, 0x26
		uint32_t iref = 0;			// 0x27

		void SetUpPipeline();
		void DrawPrimitive();

	public:
		bool ras_wireframe = false;			//!< Enable wireframe drawing of primitives (DEBUG)

		Rasterizer(HWConfig* config, GFXCore* parent_gfx);
		~Rasterizer();

		void RAS_Begin(RAS_Primitive prim, size_t vtx_num);
		void RAS_End();
		void RAS_SendVertex(const Vertex* v);
		
		void loadRASReg(size_t index, uint32_t value);

		//! Texture binding of a TEV stage (0..15), as programmed through RAS1_TREF0..7
		const RAS1_TREF* GetTref(int stage) const { return &tref[(stage >> 1) & 7]; }

		//! The raw RAS1 register state (read-only; used by the debugger and the unit tests).
		const RAS1_TREF& Tref(int pair) const { return tref[pair & 7]; }
		const RAS1_SS& SS(int pair) const { return ss[pair & 1]; }
		uint32_t Iref() const { return iref; }

		//! The multiplier the coordinate shift scale of indirect stage `stage` (0..3) stands for:
		//! `ras1_sts` 0..8 divides the fetch coordinate by 2^value (gfx-ras1.md 4.2), the unused
		//! encodings 9..15 divide by 256 like the largest defined one.
		float IndirectScale(int stage, bool t) const
		{
			stage &= 3;

			const RAS1_SS& s = ss[(stage >> 1) & 1];

			unsigned shift = ((stage & 1) == 0) ? (t ? s.ts0 : s.ss0) : (t ? s.ts1 : s.ss1);
			if (shift > 8)
				shift = 8;

			return 1.0f / (float)(1u << shift);
		}

		//! Put the RAS register state back into the reset state.
		void Reset();
	};
}
