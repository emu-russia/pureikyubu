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

	// PE registers mapped to CPU
	struct PERegs
	{
		uint16_t     sr;         // status register
		uint16_t     token;      // last token
	};

	// Fog type

	// Blend mode

	// Blend factor

	// Compare

	// Logic Op

	// Pixel format

	// Z format

	// Projection type

	// Tex Op

	// Alpha read mode

	// 0x40
	union PE_ZMODE
	{
		struct
		{
			unsigned    enable : 1;
			unsigned    func : 3;
			unsigned    mask : 1;
		};
		uint32_t     bits;
	};

	// 0x41
	union ColMode0
	{
		struct
		{
			unsigned    blend_en : 1;
			unsigned    logop_en : 1;
			unsigned    dither_en : 1;
			unsigned    col_mask : 1;
			unsigned    alpha_mask : 1;
			unsigned    dfactor : 3;
			unsigned    sfactor : 3;
			unsigned    blebdop : 1;
			unsigned    logop : 12;
		};
		uint32_t     bits;
	};

	// 0x42
	union ColMode1
	{
		struct
		{
			unsigned    const_alpha_en : 1;
			unsigned    rsrv : 7;
			unsigned    const_alpha : 8;
			unsigned    rid : 16;
		};
		uint32_t     bits;
	};

}

void PEOpen();
void PEClose();



// color type
union Color
{
	struct { uint8_t     A, B, G, R; };
	uint32_t     RGBA;
};
