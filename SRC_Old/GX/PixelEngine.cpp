// Handling access to the PE registers available to the CPU and EFB

#include "pch.h"

using namespace Debug;

namespace GX
{

	uint16_t GXCore::PeReadReg(PEMappedRegister id)
	{
		switch (id)
		{
			case PEMappedRegister::PE_SR_ID:
				return state.peregs.sr;

			case PEMappedRegister::PE_TOKEN_ID:
				return state.peregs.token;

			default:
				return 0;
		}
	}

	void GXCore::PeWriteReg(PEMappedRegister id, uint16_t value)
	{
		switch (id)
		{
			case PEMappedRegister::PE_SR_ID:

				// clear interrupts
				if (state.peregs.sr & PE_SR_DONE)
				{
					state.peregs.sr &= ~PE_SR_DONE;
					PIClearInt(PI_INTERRUPT_PE_FINISH);
				}
				if (state.peregs.sr & PE_SR_TOKEN)
				{
					state.peregs.sr &= ~PE_SR_TOKEN;
					PIClearInt(PI_INTERRUPT_PE_TOKEN);
				}

				// set mask bits
				if (value & PE_SR_DONEMSK) state.peregs.sr |= PE_SR_DONEMSK;
				else state.peregs.sr &= ~PE_SR_DONEMSK;
				if (value & PE_SR_TOKENMSK) state.peregs.sr |= PE_SR_TOKENMSK;
				else state.peregs.sr &= ~PE_SR_TOKENMSK;

				break;
		}
	}

	// Currently, Cpu2Efb emulation is not supported. It is planned to be forwarded to the graphic Backend.

	uint32_t GXCore::EfbPeek(uint32_t addr)
	{
		Report(Channel::GP, "EfbPeek, address: 0x%08X\n", addr);
		return 0;
	}

	void GXCore::EfbPoke(uint32_t addr, uint32_t value)
	{
		Report(Channel::GP, "EfbPoke, address: 0x%08X, value: 0x%08X\n", addr, value);
	}

}
