// Setup Unit
#pragma once

namespace GX
{
	// SU BP (ByPass) address space (SU/RAS/TEV etc) Registers
	// There is no such entity as "BP" in Flipper. SU is simply used to "throw" registers further down the shop (to RAS, TEV, TX)

	enum BPRegister : size_t
	{
		// "Gen" registers are shared between all GFX blocks and have global purpose
		GEN_MODE_ID = 0x00,
		GEN_MSLOC0_ID = 0x01,
		GEN_MSLOC1_ID = 0x02,
		GEN_MSLOC2_ID = 0x03,
		GEN_MSLOC3_ID = 0x04,

		// Bump mapping Unit
		BUMP_MATRIX_A0_ID = 0x6,
		BUMP_MATRIX_B0_ID = 0x7,
		BUMP_MATRIX_C0_ID = 0x8,
		BUMP_MATRIX_A1_ID = 0x9,
		BUMP_MATRIX_B1_ID = 0xa,
		BUMP_MATRIX_C1_ID = 0xb,
		BUMP_MATRIX_A2_ID = 0xc,
		BUMP_MATRIX_B2_ID = 0xd,
		BUMP_MATRIX_C2_ID = 0xe,
		BUMP_IMASK_ID = 0x0f,
		BUMP_CMD_ID = 0x10,			// 0x10...0x1f

		// Setup Unit/Rasterizers regs
		SU_SCIS0_ID = 0x20,
		SU_SCIS1_ID = 0x21,
		SU_LPSIZE_ID = 0x22,
		SU_PERF_ID = 0x23,
		RAS1_PERF_ID = 0x24,
		RAS1_SS0_ID = 0x25,
		RAS1_SS1_ID = 0x26,
		RAS1_IREF_ID = 0x27,
		RAS1_TREF0_ID = 0x28,
		RAS1_TREF1_ID = 0x29,
		RAS1_TREF2_ID = 0x2A,
		RAS1_TREF3_ID = 0x2B,
		RAS1_TREF4_ID = 0x2C,
		RAS1_TREF5_ID = 0x2D,
		RAS1_TREF6_ID = 0x2E,
		RAS1_TREF7_ID = 0x2F,

		SU_SSIZE0_ID = 0x30,    // s/t coord scale 
		SU_TSIZE0_ID = 0x31,
		SU_SSIZE1_ID = 0x32,
		SU_TSIZE1_ID = 0x33,
		SU_SSIZE2_ID = 0x34,
		SU_TSIZE2_ID = 0x35,
		SU_SSIZE3_ID = 0x36,
		SU_TSIZE3_ID = 0x37,
		SU_SSIZE4_ID = 0x38,
		SU_TSIZE4_ID = 0x39,
		SU_SSIZE5_ID = 0x3A,
		SU_TSIZE5_ID = 0x3B,
		SU_SSIZE6_ID = 0x3C,
		SU_TSIZE6_ID = 0x3D,
		SU_SSIZE7_ID = 0x3E,
		SU_TSIZE7_ID = 0x3F,

		// Pixel Engine
		PE_ZMODE_ID = 0x40,
		PE_CMODE0_ID = 0x41,
		PE_CMODE1_ID = 0x42,
		PE_CONTROL_ID = 0x43,
		PE_FIELD_MASK_ID = 0x44,
		PE_FINISH_ID = 0x45,
		PE_REFRESH_ID = 0x46,
		PE_TOKEN_ID = 0x47,
		PE_TOKEN_INT_ID = 0x48,
		PE_COPY_SRC_ADDR_ID = 0x49,
		PE_COPY_SRC_SIZE_ID = 0x4a,
		PE_COPY_DST_BASE0_ID = 0x4b,
		PE_COPY_DST_BASE1_ID = 0x4c,
		PE_COPY_DST_STRIDE_ID = 0x4d,
		PE_COPY_SCALE_ID = 0x4e,
		PE_COPY_CLEAR_AR_ID = 0x4F,
		PE_COPY_CLEAR_GB_ID = 0x50,
		PE_COPY_CLEAR_Z_ID = 0x51,
		PE_COPY_CMD_ID = 0x52,
		PE_COPY_VFILTER0_ID = 0x53,
		PE_COPY_VFILTER1_ID = 0x54,
		PE_XBOUND_ID = 0x55,
		PE_YBOUND_ID = 0x56,
		PE_PERFMODE_ID = 0x57,
		PE_CHICKEN_ID = 0x58,
		PE_QUAD_OFFSET_ID = 0x59,

		// Texture
		TX_LOADBLOCK0_ID = 0x60,
		TX_LOADBLOCK1_ID = 0x61,
		TX_LOADBLOCK2_ID = 0x62,
		TX_LOADBLOCK3_ID = 0x63,
		TX_LOADTLUT0_ID = 0x64,    // tlut base in memory
		TX_LOADTLUT1_ID = 0x65,    // tmem ofs and size
		TX_INVTAGS_ID = 0x66,
		TX_PERFMODE_ID = 0x67,
		TX_MISC_ID = 0x68,
		TX_REFRESH_ID = 0x69,

		TX_SETMODE0_I0_ID = 0x80,    // wrap (mode)
		TX_SETMODE0_I1_ID = 0x81,
		TX_SETMODE0_I2_ID = 0x82,
		TX_SETMODE0_I3_ID = 0x83,
		TX_SETMODE1_I0_ID = 0x84,
		TX_SETMODE1_I1_ID = 0x85,
		TX_SETMODE1_I2_ID = 0x86,
		TX_SETMODE1_I3_ID = 0x87,
		TX_SETIMAGE0_I0_ID = 0x88,    // texture width, height, format
		TX_SETIMAGE0_I1_ID = 0x89,
		TX_SETIMAGE0_I2_ID = 0x8A,
		TX_SETIMAGE0_I3_ID = 0x8B,
		TX_SETIMAGE1_I0_ID = 0x8C,
		TX_SETIMAGE1_I1_ID = 0x8D,
		TX_SETIMAGE1_I2_ID = 0x8E,
		TX_SETIMAGE1_I3_ID = 0x8F,
		TX_SETIMAGE2_I0_ID = 0x90,
		TX_SETIMAGE2_I1_ID = 0x91,
		TX_SETIMAGE2_I2_ID = 0x92,
		TX_SETIMAGE2_I3_ID = 0x93,
		TX_SETIMAGE3_I0_ID = 0x94,    // texture_map >> 5, physical address
		TX_SETIMAGE3_I1_ID = 0x95,
		TX_SETIMAGE3_I2_ID = 0x96,
		TX_SETIMAGE3_I3_ID = 0x97,
		TX_SETTLUT_I0_ID = 0x98,    // bind tlut with texture
		TX_SETTLUT_I1_ID = 0x99,
		TX_SETTLUT_I2_ID = 0x9A,
		TX_SETTLUT_I3_ID = 0x9B,

		TX_SETMODE0_I4_ID = 0xA0,
		TX_SETMODE0_I5_ID = 0xA1,
		TX_SETMODE0_I6_ID = 0xA2,
		TX_SETMODE0_I7_ID = 0xA3,
		TX_SETMODE1_I4_ID = 0xA4,
		TX_SETMODE1_I5_ID = 0xA5,
		TX_SETMODE1_I6_ID = 0xA6,
		TX_SETMODE1_I7_ID = 0xA7,
		TX_SETIMAGE0_I4_ID = 0xA8,
		TX_SETIMAGE0_I5_ID = 0xA9,
		TX_SETIMAGE0_I6_ID = 0xAA,
		TX_SETIMAGE0_I7_ID = 0xAB,
		TX_SETIMAGE1_I4_ID = 0xAC,
		TX_SETIMAGE1_I5_ID = 0xAD,
		TX_SETIMAGE1_I6_ID = 0xAE,
		TX_SETIMAGE1_I7_ID = 0xAF,
		TX_SETIMAGE2_I4_ID = 0xB0,
		TX_SETIMAGE2_I5_ID = 0xB1,
		TX_SETIMAGE2_I6_ID = 0xB2,
		TX_SETIMAGE2_I7_ID = 0xB3,
		TX_SETIMAGE3_I4_ID = 0xB4,
		TX_SETIMAGE3_I5_ID = 0xB5,
		TX_SETIMAGE3_I6_ID = 0xB6,
		TX_SETIMAGE3_I7_ID = 0xB7,
		TX_SETTLUT_I4_ID = 0xB8,
		TX_SETTLUT_I5_ID = 0xB9,
		TX_SETTLUT_I6_ID = 0xBA,
		TX_SETTLUT_I7_ID = 0xBB,

		// TEV Regs
		TEV_COLOR_ENV_0_ID = 0xC0,
		TEV_ALPHA_ENV_0_ID = 0xC1,
		TEV_COLOR_ENV_1_ID = 0xC2,
		TEV_ALPHA_ENV_1_ID = 0xC3,
		TEV_COLOR_ENV_2_ID = 0xC4,
		TEV_ALPHA_ENV_2_ID = 0xC5,
		TEV_COLOR_ENV_3_ID = 0xC6,
		TEV_ALPHA_ENV_3_ID = 0xC7,
		TEV_COLOR_ENV_4_ID = 0xC8,
		TEV_ALPHA_ENV_4_ID = 0xC9,
		TEV_COLOR_ENV_5_ID = 0xCA,
		TEV_ALPHA_ENV_5_ID = 0xCB,
		TEV_COLOR_ENV_6_ID = 0xCC,
		TEV_ALPHA_ENV_6_ID = 0xCD,
		TEV_COLOR_ENV_7_ID = 0xCE,
		TEV_ALPHA_ENV_7_ID = 0xCF,
		TEV_COLOR_ENV_8_ID = 0xD0,
		TEV_ALPHA_ENV_8_ID = 0xD1,
		TEV_COLOR_ENV_9_ID = 0xD2,
		TEV_ALPHA_ENV_9_ID = 0xD3,
		TEV_COLOR_ENV_A_ID = 0xD4,
		TEV_ALPHA_ENV_A_ID = 0xD5,
		TEV_COLOR_ENV_B_ID = 0xD6,
		TEV_ALPHA_ENV_B_ID = 0xD7,
		TEV_COLOR_ENV_C_ID = 0xD8,
		TEV_ALPHA_ENV_C_ID = 0xD9,
		TEV_COLOR_ENV_D_ID = 0xDA,
		TEV_ALPHA_ENV_D_ID = 0xDB,
		TEV_COLOR_ENV_E_ID = 0xDC,
		TEV_ALPHA_ENV_E_ID = 0xDD,
		TEV_COLOR_ENV_F_ID = 0xDE,
		TEV_ALPHA_ENV_F_ID = 0xDF,

		TEV_REGISTERL_0_ID = 0xE0,
		TEV_REGISTERH_0_ID = 0xE1,
		TEV_REGISTERL_1_ID = 0xE2,
		TEV_REGISTERH_1_ID = 0xE3,
		TEV_REGISTERL_2_ID = 0xE4,
		TEV_REGISTERH_2_ID = 0xE5,
		TEV_REGISTERL_3_ID = 0xE6,
		TEV_REGISTERH_3_ID = 0xE7,
		TEV_RANGE_ADJ_C_ID = 0xE8,
		TEV_RANGE_ADJ_0_ID = 0xE9,
		TEV_RANGE_ADJ_1_ID = 0xEA,
		TEV_RANGE_ADJ_2_ID = 0xEB,
		TEV_RANGE_ADJ_3_ID = 0xEC,
		TEV_RANGE_ADJ_4_ID = 0xED,
		TEV_FOG_PARAM_0_ID = 0xEE,
		TEV_FOG_PARAM_1_ID = 0xEF,
		TEV_FOG_PARAM_2_ID = 0xF0,
		TEV_FOG_PARAM_3_ID = 0xF1,
		TEV_FOG_COLOR_ID = 0xF2,
		TEV_ALPHAFUNC_ID = 0xF3,
		TEV_Z_ENV_0_ID = 0xF4,
		TEV_Z_ENV_1_ID = 0xF5,
		TEV_KSEL_0_ID = 0xF6,
		TEV_KSEL_1_ID = 0xF7,
		TEV_KSEL_2_ID = 0xF8,
		TEV_KSEL_3_ID = 0xF9,
		TEV_KSEL_4_ID = 0xFA,
		TEV_KSEL_5_ID = 0xFB,
		TEV_KSEL_6_ID = 0xFC,
		TEV_KSEL_7_ID = 0xFD,

		SU_SSMASK_ID = 0xFE,
	};

	// Gen mode
	union GenMode
	{
		struct
		{
			unsigned ntex : 4;			// Num texcoords
			unsigned ncol : 3;			// Num colors
			unsigned unused1 : 1;
			unsigned flat_en : 1;		// Flat shading
			unsigned ms_en : 1;			// Multisampling
			unsigned ntev : 4;			// Num TEV stages
			unsigned reject_en : 2;		// Culling (front/back)
			unsigned nbmp : 3;			// Bump stages
			unsigned zfreeze : 1;
			unsigned unused2 : 4;
			unsigned rid : 8;
		};
		uint32_t     bits;
	};

	// Quad/Sample location
	union GenMsloc
	{
		struct
		{
			unsigned xs0 : 4;
			unsigned ys0 : 4;
			unsigned xs1 : 4;
			unsigned ys1 : 4;
			unsigned xs2 : 4;
			unsigned ys2 : 4;
			unsigned rid : 8;
		};
		uint32_t     bits;
	};

	// 0x20
	union SU_SCIS0
	{
		struct
		{
			unsigned    suy : 12;
			unsigned    sux : 12;
			unsigned    rid : 8;
		};
		uint32_t     bits;
	};

	// 0x21
	union SU_SCIS1
	{
		struct
		{
			unsigned    suh : 12;
			unsigned    suw : 12;
			unsigned    rid : 8;
		};
		uint32_t     bits;
	};

	// 0x22
	union SU_LinePointSize
	{
		struct
		{
			unsigned lsize : 8;
			unsigned psize : 8;
			unsigned ltoff : 3;
			unsigned ptoff : 3;
			unsigned fieldmode : 1;
			unsigned unused : 1;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0x23
	union SU_Perf
	{
		struct
		{
			unsigned selA : 3;
			unsigned selB : 3;
			unsigned ntex : 4;
			unsigned ncol : 2;
			unsigned rejf : 2;
			unsigned rejs : 2;
			unsigned cmd : 2;
			unsigned unused : 2;
			unsigned en : 2;
			unsigned pwr_en : 2;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0x3n
	union SU_TS0
	{
		struct
		{
			unsigned ssize : 16;
			unsigned bs : 1;
			unsigned ws : 1;
			unsigned lf : 1;
			unsigned pf : 1;
			unsigned unused : 4;
			unsigned rid : 8;
		};
		uint32_t     bits;
	};

	// 0x3n
	union SU_TS1
	{
		struct
		{
			unsigned tsize : 16;
			unsigned bt : 1;
			unsigned wt : 1;
			unsigned unused : 6;
			unsigned rid : 8;
		};
		uint32_t     bits;
	};

	// 0xFE
	union SU_SSMask
	{
		struct
		{
			unsigned ssmask : 24;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// SU_SSMask
	#define SSMASK_ENABLED 0xaaaaaa
	#define SSMASK_DISABLED 0x555555

	// triangle front/back rejection rules (GenMode)
	#define GEN_REJECT_NONE 0
	#define GEN_REJECT_FRONT 1
	#define GEN_REJECT_BACK 2
	#define GEN_REJECT_ALL 3
}
