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

		// TODO: Refactoring hacks
		void DONE_INT();
		void TOKEN_INT();

		void CP_BREAK();
		void CP_OVF();
		void CP_UVF();

		static void CPThread(void* Param);
		void ProcessFifo(uint8_t data[32]);

	public:
		GXCore();
		~GXCore();

		void Open();
		void Close();

		// Debug
		void DumpPIFIFO();
		void DumpCPFIFO();

#pragma region "Interface to Flipper"

		// CP Registers
		uint16_t CpReadReg(CPMappedRegister id);
		void CpWriteReg(CPMappedRegister id, uint16_t value);

		// Pixel Engine
		uint16_t PeReadReg(PEMappedRegister id);
		void PeWriteReg(PEMappedRegister id, uint16_t value);
		uint32_t EfbPeek(uint32_t addr);
		void EfbPoke(uint32_t addr, uint32_t value);

		// PI->CP Registers
		uint32_t PiCpReadReg(PI_CPMappedRegister id);
		void PiCpWriteReg(PI_CPMappedRegister id, uint32_t value);

		// Streaming FIFO (32-byte burst-only)
		void FifoWriteBurst(uint8_t data[32]);

		// TODO: Refactoring hacks
		void CPDrawDoneCallback();
		void CPDrawTokenCallback(uint16_t tokenValue);

#pragma endregion "Interface to Flipper"


	};

}
