#pragma once

namespace GX
{

	// PE Registers (from CPU side). 16-bit access.

	enum class PEMappedRegister
	{
		PE_PI_ZMODE_ID = 0,			// Cpu2Efb Z mode
		PE_PI_CMODE0_ID,			// Cpu2Efb Color mode 0
		PE_PI_CMODE1_ID,			// Cpu2Efb Color mode 1
		PE_PI_ALPHA_THRES_ID,		// Cpu2Efb Alpha mode 0
		PE_PI_CONTROL_ID,
		PE_PI_INTRCTRL_ID,
		PE_PI_INTRSTAT_ID,
		PE_PI_TOKEN_ID,				// Last token value
		PE_PI_XBOUND0_ID,
		PE_PI_XBOUND1_ID,
		PE_PI_YBOUND0_ID,
		PE_PI_YBOUND1_ID,
		PE_PI_PERF_COUNTER_0L_ID,	// 0x18
		PE_PI_PERF_COUNTER_0H_ID,	// 0x1a
		PE_PI_PERF_COUNTER_1L_ID,	// 0x20
		PE_PI_PERF_COUNTER_1H_ID,	// 0x22
		PE_PI_PERF_COUNTER_2L_ID,	// 0x24
		PE_PI_PERF_COUNTER_2H_ID,	// 0x26
		PE_PI_PERF_COUNTER_3L_ID,	// 0x28
		PE_PI_PERF_COUNTER_3H_ID,	// 0x2a
		PE_PI_PERF_COUNTER_4L_ID,	// 0x2c
		PE_PI_PERF_COUNTER_4H_ID,	// 0x30
		PE_PI_PERF_COUNTER_5L_ID,	// 0x32
		PE_PI_PERF_COUNTER_5H_ID,	// 0x34
	};

	// PE intrctrl register
	#define PE_SR_DONE      (1 << 0)
	#define PE_SR_TOKEN     (1 << 1)
	#define PE_SR_DONEMSK   (1 << 2)
	#define PE_SR_TOKENMSK  (1 << 3)

	// The register definitions for PE PI are slightly different from command stream PE registers because PE PI registers are 16-bit

	// PE registers mapped to CPU
	struct PERegs
	{
		uint16_t     sr;         // status register
	};

    // PE register definitions that come through the command stream.

	// 0x40
	union PE_ZMODE
	{
		struct
		{
			unsigned enable : 1;
			unsigned func : 3;
			unsigned mask : 1;
			unsigned unused : 19;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0x41
	union PE_CMODE0
	{
		struct
		{
			unsigned blend_en : 1;
			unsigned logop_en : 1;
			unsigned dither_en : 1;
			unsigned col_mask : 1;
			unsigned alpha_mask : 1;
			unsigned dfactor : 3;
			unsigned sfactor : 3;
			unsigned blendop : 1;
			unsigned logop : 12;
			unsigned unused : 8;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// 0x42
	union PE_CMODE1
	{
		struct
		{
			unsigned const_alpha : 8;
			unsigned const_alpha_en : 1;
			unsigned yuv : 2;
			unsigned unused : 13;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

    // 0x43
    union PE_CONTROL
    {
        struct
        {
            unsigned pixtype : 3;
            unsigned zcmode : 3;
            unsigned ztop : 1;
            unsigned unused : 17;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x44
    union PE_FIELD_MASK
    {
        struct
        {
            unsigned even : 1;
            unsigned odd : 1;
            unsigned unused : 22;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x45
    union PE_FINISH
    {
        struct
        {
            unsigned dst : 2;
            unsigned unused : 22;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x46
    union PE_REFRESH
    {
        struct
        {
            unsigned interval : 9;
            unsigned enable : 1;
            unsigned unused : 14;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x47
    union PE_TOKEN
    {
        struct
        {
            unsigned token : 16;
            unsigned unused : 8;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x48
    union PE_TOKEN_INT
    {
        struct
        {
            unsigned token : 16;
            unsigned unused : 8;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x49
    union PE_COPY_SRC_ADDR
    {
        struct
        {
            unsigned x : 10;
            unsigned y : 10;
            unsigned unused : 4;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x4a
    union PE_COPY_SRC_SIZE
    {
        struct
        {
            unsigned x : 10;
            unsigned y : 10;
            unsigned unused : 4;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x4b, 0x4c
    union PE_COPY_DST_BASE
    {
        struct
        {
            unsigned base : 21;
            unsigned unused : 3;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x4d
    union PE_COPY_DST_STRIDE
    {
        struct
        {
            unsigned stride : 10;
            unsigned unused : 14;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x4e
    union PE_COPY_SCALE
    {
        struct
        {
            unsigned scale : 9;
            unsigned unused : 15;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x4F
    union PE_COPY_CLEAR_AR
    {
        struct
        {
            unsigned red : 8;
            unsigned alpha : 8;
            unsigned unused : 8;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x50
    union PE_COPY_CLEAR_GB
    {
        struct
        {
            unsigned blue : 8;
            unsigned green : 8;
            unsigned unused : 8;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x51
    union PE_COPY_CLEAR_Z
    {
        struct
        {
            unsigned value : 24;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x52
    union PE_COPY_CMD
    {
        struct
        {
            unsigned clamp_top : 1;
            unsigned clamp_bottom : 1;
            unsigned unused1 : 1;
            unsigned tex_format_h : 1;
            unsigned tex_format : 3;
            unsigned gamma : 2;
            unsigned mip_map_filter : 1;
            unsigned vert_scale : 1;
            unsigned clear : 1;
            unsigned interlaced : 2;
            unsigned opcode : 1;
            unsigned ccv : 2;
            unsigned unused2 : 7;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x53
    union PE_VFILTER_0
    {
        struct
        {
            unsigned coeff0 : 6;
            unsigned coeff1 : 6;
            unsigned coeff2 : 6;
            unsigned coeff3 : 6;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x54
    union PE_VFILTER_1
    {
        struct
        {
            unsigned coeff4 : 6;
            unsigned coeff5 : 6;
            unsigned coeff6 : 6;
            unsigned unused : 6;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x55
    union PE_XBOUND
    {
        struct
        {
            unsigned left : 10;
            unsigned right : 10;
            unsigned unused : 4;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x56
    union PE_YBOUND
    {
        struct
        {
            unsigned top : 10;
            unsigned bottom : 10;
            unsigned unused : 4;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x57
    union PE_PERFMODE
    {
        struct
        {
            unsigned conter0 : 2;
            unsigned conter1 : 2;
            unsigned conter2 : 2;
            unsigned conter3 : 2;
            unsigned conter4 : 2;
            unsigned conter5 : 2;
            unsigned unused : 12;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x58
    union PE_CHICKEN
    {
        struct
        {
            unsigned piwr : 1;
            unsigned tx_copy_fmt : 1;
            unsigned tx_copy_ccv : 1;
            unsigned blendop : 1;
            unsigned unused : 20;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    // 0x59
    union PE_QUAD_OFFSET
    {
        struct
        {
            unsigned x : 10;
            unsigned y : 10;
            unsigned pad : 4;
            unsigned rid : 8;
        };
        uint32_t bits;
    };

    struct PEState
    {
        PE_ZMODE zmode;		// 0x40
        PE_CMODE0 cmode0;	// 0x41
        PE_CMODE1 cmode1;	// 0x42
        PE_CONTROL control;	// 0x43
        PE_FIELD_MASK field_mask; // 0x44
        PE_FINISH finish;	// 0x45
        PE_REFRESH refresh; // 0x46
        PE_TOKEN token; // 0x47
        PE_TOKEN_INT token_int; // 0x48
        PE_COPY_SRC_ADDR copy_src_addr;	// 0x49
        PE_COPY_SRC_SIZE copy_src_size;	// 0x4a
        PE_COPY_DST_BASE copy_dst_base[2];	// 0x4b, 0x4c
        PE_COPY_DST_STRIDE copy_dst_stride;	// 0x4d
        PE_COPY_SCALE copy_scale;	// 0x4e
        PE_COPY_CLEAR_AR copy_clear_ar;	// 0x4F
        PE_COPY_CLEAR_GB copy_clear_gb;	// 0x50
        PE_COPY_CLEAR_Z copy_clear_z;	// 0x51
        PE_COPY_CMD copy_cmd;	// 0x52
        PE_VFILTER_0 vfilter_0;	// 0x53
        PE_VFILTER_1 vfilter_1;	// 0x54
        PE_XBOUND xbound;	// 0x55
        PE_YBOUND ybound;	// 0x56
        PE_PERFMODE perfmode;	// 0x57
        PE_CHICKEN chicken;  // 0x58
        PE_QUAD_OFFSET quad_offset;  // 0x59
    };
}

void PEOpen();
void PEClose();
