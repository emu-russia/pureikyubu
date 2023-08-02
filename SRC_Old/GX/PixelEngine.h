
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
}
