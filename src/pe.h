#pragma once

namespace GX
{

	// PE Registers (from CPU side). 16-bit access.

	enum class PEMappedRegister
	{
		PE_POKE_ZMODE_ID = 0,		// Cpu2Efb Z mode
		PE_POKE_CMODE0_ID,			// Cpu2Efb Color mode 0
		PE_POKE_CMODE1_ID,			// Cpu2Efb Color mode 1
		PE_POKE_AMODE0_ID,			// Cpu2Efb Alpha mode 0
		PE_POKE_AMODE1_ID,			// Cpu2Efb Alpha mode 1
		PE_SR_ID,					// Status register
		PE_UNK6_ID,
		PE_TOKEN_ID,				// Last token value
	};

	// PE status register
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
typedef union _Color
{
	struct { uint8_t     A, B, G, R; };
	uint32_t     RGBA;
} Color;
