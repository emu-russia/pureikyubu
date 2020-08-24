
#pragma once

namespace GX
{

	// XF Registers

	enum class XFRegister
	{
		// 0x0000...0x03FF - ModelView/Texture Matrix memory. This block is formed by the matrix memory. Its address range is 0 to 1k, but only 256 entries are used. This memory is organized in a 64 entry by four 32b words.

		XF_MATRIX_MEMORY_ID = 0x0000,
		XF_MATRIX_MEMORY_SIZE = (64 * 4),

		// 0x0400...0x04FF - Normal matrix memory. This block of memory is the normal matrix memory. It is organized as 32 rows of 3 words.

		XF_NORMAL_MATRIX_MEMORY_ID = 0x0400,
		XF_NORMAL_MATRIX_MEMORY_SIZE = (32 * 3),

		// 0x0500...0x05FF - Texture post-transform matrix memory. This block of memory holds the dual texture transform matrices. The format is identical to the first block of matrix memory. 
		// There are also 64 rows of 4 words for these matrices

		XF_DUALTEX_MATRIX_MEMORY_ID = 0x0500,
		XF_DUALTEX_MATRIX_MEMORY_SIZE = (64 * 4),

		// Lighting parameters memory

		XF_LIGHT_MEMORY_ID = 0x0600,
		XF_LIGHT_DATA_SIZE = 0x10,
		XF_LIGHT_MEMORY_SIZE = (XF_LIGHT_DATA_SIZE * 8),

		// Light0 parameters

		XF_LIGHT0_ID = 0x0600,			// reserved
		XF_LIGHT0_Reserved1 = 0x0601,	// reserved
		XF_LIGHT0_Reserved2 = 0x0602,	// reserved
		XF_LIGHT0_RGBA_ID = 0x0603,  // RGBA (8b/comp)
		XF_LIGHT0_A0_ID = 0x0604,  // cos atten a0
		XF_LIGHT0_A1_ID = 0x0605,  // cos atten a1
		XF_LIGHT0_A2_ID = 0x0606,  // cos atten a2
		XF_LIGHT0_K0_ID = 0x0607,  // dist atten k0
		XF_LIGHT0_K1_ID = 0x0608,  // dist atten k1
		XF_LIGHT0_K2_ID = 0x0609,  // dist atten k2
		XF_LIGHT0_LPX_ID = 0x060A,  // x light pos, or inf ldir x
		XF_LIGHT0_LPY_ID = 0x060B,  // y light pos, or inf ldir y
		XF_LIGHT0_LPZ_ID = 0x060C,  // z light pos, or inf ldir z
		XF_LIGHT0_DHX_ID = 0x060D,  // light dir x, or 1/2 angle x
		XF_LIGHT0_DHY_ID = 0x060E,  // light dir y, or 1/2 angle y
		XF_LIGHT0_DHZ_ID = 0x060F,  // light dir z, or 1/2 angle z

		// 0x0610-0x067f: Parameters for Light1-Light7. See Light0 data.

		XF_LIGHT1_ID = 0x0610,
		XF_LIGHT2_ID = 0x0620,
		XF_LIGHT3_ID = 0x0630,
		XF_LIGHT4_ID = 0x0640,
		XF_LIGHT5_ID = 0x0650,
		XF_LIGHT6_ID = 0x0660,
		XF_LIGHT7_ID = 0x0670,

		// XF 0x0680...0x07FF are reserved

		// Other general XF parameters (>= 0x1000)

		XF_ERROR_ID = 0x1000,
		XF_DIAGNOSTICS_ID = 0x1001,
		XF_STATE0_ID = 0x1002,
		XF_STATE1_ID = 0x1003,
		XF_CLOCK_ID = 0x1004,
		XF_CLIP_DISABLE_ID = 0x1005,
		XF_PERF0_ID = 0x1006,
		XF_PERF1_ID = 0x1007,

		XF_INVTXSPEC_ID = 0x1008,
		XF_NUMCOLS_ID = 0x1009,  // Selects the number of output colors
		XF_AMBIENT0_ID = 0x100A,  // RGBA (8b/comp) ambient color 0
		XF_AMBIENT1_ID = 0x100B,  // RGBA (8b/comp) ambient color 1
		XF_MATERIAL0_ID = 0x100C,  // RGBA (8b/comp) material color 0
		XF_MATERIAL1_ID = 0x100D,  // RGBA (8b/comp) material color 1
		XF_COLOR0CNTL_ID = 0x100E,  // COLOR0 channel control
		XF_COLOR1CNTL_ID = 0x100F,  // COLOR1 channel control
		XF_ALPHA0CNTL_ID = 0x1010,  // ALPHA0 channel control
		XF_ALPHA1CNTL_ID = 0x1011,  // ALPHA1 channel control

		XF_DUALTEX_ID = 0x1012,  // enable tex post-transform
		XF_MATINDEX_A_ID = 0x1018,  // Position / Tex coord 0-3 mat index
		XF_MATINDEX_B_ID = 0x1019,  // Tex coord 4-7 mat index
		XF_VIEWPORT_SCALE_X_ID = 0x101A,
		XF_VIEWPORT_SCALE_Y_ID = 0x101B,
		XF_VIEWPORT_SCALE_Z_ID = 0x101C,
		XF_VIEWPORT_OFFSET_X_ID = 0x101D,
		XF_VIEWPORT_OFFSET_Y_ID = 0x101E,
		XF_VIEWPORT_OFFSET_Z_ID = 0x101F,

		// Projection matrix parameters

		XF_PROJECTION_A_ID = 0x1020,
		XF_PROJECTION_B_ID = 0x1021,
		XF_PROJECTION_C_ID = 0x1022,
		XF_PROJECTION_D_ID = 0x1023,
		XF_PROJECTION_E_ID = 0x1024,
		XF_PROJECTION_F_ID = 0x1025,
		XF_PROJECT_ORTHO_ID = 0x1026,

		XF_NUMTEX_ID = 0x103F,  // active texgens
		XF_TEXGEN0_ID = 0x1040,
		XF_TEXGEN1_ID = 0x1041,
		XF_TEXGEN2_ID = 0x1042,
		XF_TEXGEN3_ID = 0x1043,
		XF_TEXGEN4_ID = 0x1044,
		XF_TEXGEN5_ID = 0x1045,
		XF_TEXGEN6_ID = 0x1046,
		XF_TEXGEN7_ID = 0x1047,

		// Dual texgen setup

		XF_DUALGEN0_ID = 0x1050,
		XF_DUALGEN1_ID = 0x1051,
		XF_DUALGEN2_ID = 0x1052,
		XF_DUALGEN3_ID = 0x1053,
		XF_DUALGEN4_ID = 0x1054,
		XF_DUALGEN5_ID = 0x1055,
		XF_DUALGEN6_ID = 0x1056,
		XF_DUALGEN7_ID = 0x1057,
	};

	// Texture generator types

	// Texgen Source

	// Lighting diffuse function

	// Lighting attenuation function

	// Lighting spotlight function

	// Lighting distance attenuation function

}
