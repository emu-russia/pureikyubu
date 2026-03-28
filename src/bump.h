// GFX Bump Mapping Unit
#pragma once

namespace GFX
{
	// Bump mapping Unit registers
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

	class BumpMappingUnit
	{
		friend GFXCore;
		GFXCore* gfx = nullptr;

	public:
		BumpMappingUnit(HWConfig* config, GFXCore* parent_gfx);
		~BumpMappingUnit();

		void loadBUMPReg(size_t index, uint32_t value);
	};
}