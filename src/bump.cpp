// GFX Bump Mapping Unit
//
// The bump unit owns the BP register ids 0x06-0x1F (gfx-bump.md 4.1): the three 3x2 indirect
// matrices (A/B/C rows of matrix 0..2, ids 0x06-0x0E), the indirect mask (0x0F) and one indirect
// command per TEV stage (0x10-0x1F). Together they describe the indirect texture stage that
// perturbs a texture coordinate before the TEV samples the texture of that stage.
//
// The register file is emulated completely so that the state is visible (the debugger dumps it, see
// the `gx` JDI commands in gfx.cpp) and so that no register of the range is silently swallowed.
//
// What the OpenGL backend does with it: the indirect texturing itself lives in the TEV fragment
// shader (tev.cpp), which walks the same datapath for every stage that has an indirect command (the
// coordinate wrap, the texel decode, the 3x2 matrix, the scale and the offset add, gfx-bump.md 3.3).
// The one register of the group that has no backend effect is BUMP_IMASK, the stream-classification
// mask - see the note on `BumpIMask` in bump.h for why it cannot have one here.

#include "pch.h"

namespace GFX
{
	BumpMappingUnit::BumpMappingUnit(HWConfig* config, GFXCore* parent_gfx)
	{
		gfx = parent_gfx;
		Reset();
	}

	BumpMappingUnit::~BumpMappingUnit()
	{
	}

	void BumpMappingUnit::Reset()
	{
		bump = BUMPState{};

		for (int i = 0; i < 16; i++)
		{
			// The indirect mask resets to "no dot product applied to any channel"
			bump.cmd[i].bits = 0;
		}
		bump.imask.bits = 0xFFFFFF;
	}

	// One word of the bump/indirect register range (0x06-0x1F), which is where the bypass walk
	// arrives after the Pixel Engine (gfx.md 10.1).
	void BumpMappingUnit::loadBUMPReg(size_t index, uint32_t value)
	{
		//
		// The indirect matrices: 0x06-0x08 are the A/B/C rows of matrix 0, 0x09-0x0B of matrix 1 and
		// 0x0C-0x0E of matrix 2. Each row register holds two 11-bit signed S1.10 coefficients and a
		// 2-bit scale (gfx-bump.md 4.2).
		//

		if (index >= BUMP_MATRIX_A0_ID && index <= BUMP_MATRIX_C2_ID)
		{
			int row = (int)(index - BUMP_MATRIX_A0_ID) / 3;		// matrix 0..2
			int kind = (int)(index - BUMP_MATRIX_A0_ID) % 3;	// 0 = A, 1 = B, 2 = C

			BumpMatrix& m = bump.matrix[row];

			switch (kind)
			{
				case 0: m.a.bits = value; break;
				case 1: m.b.bits = value; break;
				default: m.c.bits = value; break;
			}
			return;
		}

		//
		// The indirect mask (0x0F). It classifies the incoming command words rather than the texel
		// fields: see the note on `BumpIMask` in bump.h. It is part of the register file and of the
		// debugger dump, and it has no picture-visible effect in this backend.
		//

		if (index == BUMP_IMASK_ID)
		{
			bump.imask.bits = value;
			return;
		}

		//
		// The per-stage indirect commands (0x10-0x1F)
		//

		if (index >= BUMP_CMD_ID && index < (BUMP_CMD_ID + 16))
		{
			bump.cmd[index - BUMP_CMD_ID].bits = value;
			return;
		}

		// The walk continues in the texture unit
		gfx->tx->loadTXReg(index, value);
	}

	// An indirect stage is active when its command names a texture map and a mode other than the
	// "no matrix, no texel" default of a reset register.
	bool BumpMappingUnit::IndirectActive() const
	{
		for (int i = 0; i < 16; i++)
		{
			if (bump.cmd[i].bt != 0 || bump.cmd[i].m != 0)
			{
				return true;
			}
		}

		return false;
	}
}
