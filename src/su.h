// Setup Unit
#pragma once

namespace GFX
{
	// "Gen" registers are shared between all GFX blocks and have global purpose
	#define GEN_MODE_ID 0x00
	#define GEN_MSLOC0_ID 0x01
	#define GEN_MSLOC1_ID 0x02
	#define GEN_MSLOC2_ID 0x03
	#define GEN_MSLOC3_ID 0x04

	// Setup Unit/Rasterizers regs
	#define SU_SCIS0_ID 0x20
	#define SU_SCIS1_ID 0x21
	#define SU_LPSIZE_ID 0x22
	#define SU_PERF_ID 0x23

	#define SU_SSIZE0_ID 0x30    // s/t coord scale 
	#define SU_TSIZE0_ID 0x31
	#define SU_SSIZE1_ID 0x32
	#define SU_TSIZE1_ID 0x33
	#define SU_SSIZE2_ID 0x34
	#define SU_TSIZE2_ID 0x35
	#define SU_SSIZE3_ID 0x36
	#define SU_TSIZE3_ID 0x37
	#define SU_SSIZE4_ID 0x38
	#define SU_TSIZE4_ID 0x39
	#define SU_SSIZE5_ID 0x3A
	#define SU_TSIZE5_ID 0x3B
	#define SU_SSIZE6_ID 0x3C
	#define SU_TSIZE6_ID 0x3D
	#define SU_SSIZE7_ID 0x3E
	#define SU_TSIZE7_ID 0x3F

	#define SU_SSMASK_ID 0xFE

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

	struct SUState
	{
		SU_SCIS0 scis0;		// 0x20
		SU_SCIS1 scis1;		// 0x21
		SU_TS0 ssize[8];	// 0x3n
		SU_TS1 tsize[8];	// 0x3n
	};

	class SetupUnit
	{
		friend GFXCore;
		GFXCore* gfx = nullptr;

		SUState su{};

		void GL_SetScissor(int x, int y, int w, int h);
		void GL_SetCullMode(int mode);

	public:
		SetupUnit(HWConfig* config, GFXCore* parent_gfx);
		~SetupUnit();

		void loadSUReg(size_t index, uint32_t value);
	};
}