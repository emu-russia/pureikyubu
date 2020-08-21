// PI and CP FIFO processing

#include "pch.h"

namespace GX
{

    // TODO: Make a GP update when copying the frame buffer by Pixel Engine.

    void GXCore::DONE_INT()
    {
        if (state.pe.sr & PE_SR_DONEMSK)
        {
            state.pe.sr |= PE_SR_DONE;
            PIAssertInt(PI_INTERRUPT_PE_FINISH);
        }
    }

    void GXCore::TOKEN_INT()
    {
        if (state.pe.sr & PE_SR_TOKENMSK)
        {
            state.pe.sr |= PE_SR_TOKEN;
            PIAssertInt(PI_INTERRUPT_PE_TOKEN);
        }
    }

	void GXCore::CPDrawDoneCallback()
	{
        DONE_INT();
	}

	void GXCore::CPDrawTokenCallback(uint16_t tokenValue)
	{
        state.pe.token = tokenValue;
        TOKEN_INT();
	}

}
