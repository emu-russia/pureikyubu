// Main module with interface for Flipper (HW)

#pragma once

#include "GXDefs.h"
#include "GXState.h"
#include "TexCache.h"
#include "TexConv.h"
#include "GXBackend.h"

namespace GX
{

	class GXCore
	{
		friend GXBackend;
		GXBackend* backend = nullptr;

		TextureConverter texconv;
		TextureCache texcache;

		State state;

		void DONE_INT();
		void TOKEN_INT();

	public:
		GXCore();
		~GXCore();

		void Reset();

#pragma region "Interface to Flipper"

		// CP Registers

		// Pixel Engine
		uint16_t PeReadReg(PEMappedRegister id);
		void PeWriteReg(PEMappedRegister id, uint16_t value);
		uint32_t EfbPeek(uint32_t addr);
		void EfbPoke(uint32_t addr, uint32_t value);

		// Streaming FIFO (32-byte burst-only)

		// Refactoring hacks
		void CPDrawDoneCallback();
		void CPDrawTokenCallback(uint16_t tokenValue);

#pragma endregion "Interface to Flipper"


	};

}
