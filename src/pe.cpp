// PE - pixel engine
#include "pch.h"

using namespace Debug;

// Handling access to the PE registers available to the CPU and EFB

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

	// set clear rules
	void GXCore::GL_SetClear(Color clr, uint32_t z)
	{
		cr = clr.R;
		cg = clr.G;
		cb = clr.B;
		ca = clr.A;
		clear_z = z;
		set_clear = TRUE;

		/*/
			if(set_clear == TRUE)
			{
				glClearColor(
					(float)(cr / 255.0f),
					(float)(cg / 255.0f),
					(float)(cb / 255.0f),
					(float)(ca / 255.0f)
				);

				glClearDepth((double)(clear_z / 16777215.0));

				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

				set_clear = FALSE;
			}
		/*/
	}

}

//
// Stubs
//

static void PERegRead(uint32_t addr, uint32_t* reg)
{
	*reg = Flipper::Gx->PeReadReg((GX::PEMappedRegister)((addr & 0xFF) >> 1));
}

static void PERegWrite(uint32_t addr, uint32_t data)
{
	Flipper::Gx->PeWriteReg((GX::PEMappedRegister)((addr & 0xFF) >> 1), data);
}

void PEOpen()
{
	Report(Channel::CP, "Pixel Engine (for GX)\n");

	// Pixel Engine
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_POKE_ZMODE_ID), PERegRead, PERegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_POKE_CMODE0_ID), PERegRead, PERegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_POKE_CMODE1_ID), PERegRead, PERegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_POKE_AMODE0_ID), PERegRead, PERegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_POKE_AMODE1_ID), PERegRead, PERegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_SR_ID), PERegRead, PERegWrite);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_TOKEN_ID), PERegRead, PERegWrite);
}

void PEClose()
{
}
