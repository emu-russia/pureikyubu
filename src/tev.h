// Texture Environment Unit (TEV)
#pragma once

// TEV cannot be emulated by "simple" OpenGL, it requires more advanced pixel shaders.

namespace GX
{
	// 0xC0..0xDF
	union TEV_ColorEnv
	{
		struct
		{
			unsigned seld : 4;
			unsigned selc : 4;
			unsigned selb : 4;
			unsigned sela : 4;
			unsigned bias : 2;
			unsigned sub : 1;
			unsigned clamp : 1;
			unsigned shift : 2;
			unsigned dest : 2;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0xC0..0xDF
	union TEV_AlphaEnv
	{
		struct
		{
			unsigned mode : 2;
			unsigned swap : 2;
			unsigned seld : 3;
			unsigned selc : 3;
			unsigned selb : 3;
			unsigned sela : 3;
			unsigned bias : 2;
			unsigned sub : 1;
			unsigned clamp : 1;
			unsigned shift : 2;
			unsigned dest : 2;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0xE0
	union TEV_RegisterL
	{
		struct
		{
			unsigned r : 11;
			unsigned unused1 : 1;
			unsigned a : 11;
			unsigned unused2 : 1;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0xE0
	union TEV_RegisterH
	{
		struct
		{
			unsigned b : 11;
			unsigned unused1 : 1;
			unsigned g : 11;
			unsigned unused2 : 1;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	union TEV_KonstRegisterL
	{
		struct
		{
			unsigned r : 8;
			unsigned unused1 : 4;
			unsigned a : 8;
			unsigned unused2 : 4;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	union TEV_KonstRegisterH
	{
		struct
		{
			unsigned b : 8;
			unsigned unused1 : 4;
			unsigned g : 8;
			unsigned unused2 : 4;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0xE8
	union TEV_RangeAdj_Contol
	{
		struct
		{
			unsigned center : 10;	// center x
			unsigned enb : 1;
			unsigned unused : 13;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	union TEV_RangeAdj
	{
		struct
		{
			unsigned r0 : 12;
			unsigned r1 : 12;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0xEE
	union TEV_FogParam0
	{
		struct
		{
			unsigned a_mant : 11;
			unsigned a_expn : 8;
			unsigned a_sign : 1;
			unsigned unused : 4;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0xEF
	union TEV_FogParam1
	{
		struct
		{
			unsigned b_mag : 24;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0xF0
	union TEV_FogParam2
	{
		struct
		{
			unsigned b_shft : 5;
			unsigned unused : 19;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0xF1
	union TEV_FogParam3
	{
		struct
		{
			unsigned c_mant : 11;
			unsigned c_expn : 8;
			unsigned c_sign : 1;
			unsigned proj : 1;
			unsigned fsel : 3;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0xF2
	union TEV_FogColor
	{
		struct
		{
			unsigned b : 8;
			unsigned g : 8;
			unsigned r : 8;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0xF3
	union TEV_AlphaFunc
	{
		struct
		{
			unsigned a0 : 8;
			unsigned a1 : 8;
			unsigned op0 : 3;
			unsigned op1 : 3;
			unsigned logic : 2;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0xF4
	union TEV_ZEnv0
	{
		struct
		{
			unsigned zoff : 24;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0xF5
	union TEV_ZEnv1
	{
		struct
		{
			unsigned type : 2;
			unsigned op : 2;
			unsigned unused : 20;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0xF6..0xFD
	union TEV_KSel
	{
		struct
		{
			unsigned xrb : 2;
			unsigned xga : 2;
			unsigned kcsel0 : 5;
			unsigned kasel0 : 5;
			unsigned kcsel1 : 5;
			unsigned kasel1 : 5;
			unsigned rid : 8;
		};
		uint32_t bits;
	};
}
