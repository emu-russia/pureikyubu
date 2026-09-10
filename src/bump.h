// GFX Bump Mapping Unit
#pragma once

namespace GFX
{
	// Bump mapping Unit registers (gfx-bump.md 4.1). The unit owns the ids 0x06-0x1F.

	#define BUMP_MATRIX_A0_ID 0x6
	#define BUMP_MATRIX_B0_ID 0x7
	#define BUMP_MATRIX_C0_ID 0x8
	#define BUMP_MATRIX_A1_ID 0x9
	#define BUMP_MATRIX_B1_ID 0xa
	#define BUMP_MATRIX_C1_ID 0xb
	#define BUMP_MATRIX_A2_ID 0xc
	#define BUMP_MATRIX_B2_ID 0xd
	#define BUMP_MATRIX_C2_ID 0xe
	#define BUMP_IMASK_ID 0x0f
	#define BUMP_CMD_ID 0x10			// 0x10...0x1f

	// 0x06, 0x09, 0x0c - the A row of one indirect matrix: two S1.10 coefficients and a scale.
	union BumpMatrixRowAB
	{
		struct
		{
			unsigned ma : 11;
			unsigned mb : 11;
			unsigned s : 2;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0x07, 0x0a, 0x0d - the B row
	union BumpMatrixRowBC
	{
		struct
		{
			unsigned mc : 11;
			unsigned md : 11;
			unsigned s : 2;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0x08, 0x0b, 0x0e - the C row
	union BumpMatrixRowCA
	{
		struct
		{
			unsigned me : 11;
			unsigned mf : 11;
			unsigned s : 2;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	struct BumpMatrix
	{
		BumpMatrixRowAB a{};		// ma, mb
		BumpMatrixRowBC b{};		// mc, md
		BumpMatrixRowCA c{};		// me, mf
	};

	// 0x0f - the bump unit's stream-classification mask (gfx-bump.md 4.3).
	//
	// This register is *not* a mask of the indirect texel offset components, although the FDL gives it
	// no comment and the name invites that reading. The module description is explicit: the entry
	// stage latches the eight payload bits and uses them to classify incoming *words* - a texture-group
	// register word (or an indirectly marked quad word) is forwarded on the short path to the texture
	// unit when the mask bit selected by the word's tag is set, and goes through the bump unit's data
	// FIFO otherwise. The GX API fills the mask with the set of texture maps its indirect stages fetch
	// from (libogc `__GX_UpdateBPMask` computes `nres` as the OR of `1 << texmap` over the indirect
	// stages), i.e. it describes a routing/timing property of the command stream.
	//
	// The OpenGL backend has no command-stream router to feed: every BP word is decoded straight into
	// the register file at the moment it arrives, and the indirect fetch of a TEV stage already names
	// its map through the `bt` field of its command. The mask therefore has no effect on the picture
	// and none can be invented for it without modelling a stream the emulator does not have (the
	// hardware's own visible behaviour of the mask is limited to the ordering of register updates
	// against quad data). It is decoded, kept in the register file and shown by the debugger, and the
	// wiki records it as such.
	union BumpIMask
	{
		struct
		{
			unsigned imask : 8;
			unsigned unused : 16;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// Indirect texture format (gfx-bump.md 4.4 `bp_fmt`)
	enum BumpFormat : unsigned
	{
		BUMP_FMT_8 = 0,			// 8-bit indirect texel components
		BUMP_FMT_5,				// 5-bit
		BUMP_FMT_4,				// 4-bit
		BUMP_FMT_3,				// 3-bit
	};

	// Indirect bias (gfx-bump.md 4.4 `bp_bias`)
	enum BumpBias : unsigned
	{
		BUMP_BIAS_NONE = 0,		// unsigned, no bias
		BUMP_BIAS_PLUS,			// one extra bit, unsigned
		BUMP_BIAS_SIGN,			// sign extend
		BUMP_BIAS_SIGN_PLUS,	// sign extend with the bias bit
	};

	// 0x10-0x1f - the indirect command of one TEV stage
	union BumpCommand
	{
		struct
		{
			unsigned bt : 2;		// indirect texture map that provides the texel
			unsigned fmt : 2;		// BumpFormat
			unsigned bias : 3;		// BumpBias
			unsigned bs : 2;		// bump scale
			unsigned m : 4;			// matrix selection (and the 2-bit scale group)
			unsigned sw : 3;		// texcoord wrap mode, S
			unsigned tw : 3;		// texcoord wrap mode, T
			unsigned lb : 1;		// use the previous stage's texture coordinate as the base
			unsigned fb : 1;		// use the previous indirect result
			unsigned unused : 3;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	struct BUMPState
	{
		BumpMatrix matrix[3]{};		// 0x06-0x0E: three 3x2 matrices
		BumpIMask imask{};			// 0x0F
		BumpCommand cmd[16]{};		// 0x10-0x1F: one indirect command per TEV stage
	};

	class BumpMappingUnit
	{
		friend GFXCore;
		GFXCore* gfx = nullptr;

		BUMPState bump{};

	public:
		BumpMappingUnit(HWConfig* config, GFXCore* parent_gfx);
		~BumpMappingUnit();

		void loadBUMPReg(size_t index, uint32_t value);

		//! The bump/indirect register state (read-only; used by the debugger and the unit tests).
		const BUMPState& State() const { return bump; }

		//! True when any TEV stage has an indirect texture command programmed.
		bool IndirectActive() const;

		//! Put the bump unit register state back into the reset state.
		void Reset();
	};
}
