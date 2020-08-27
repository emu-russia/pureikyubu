// CP - command processor, PE - pixel engine.
// This module contains stubs for accessing the CP and PE registers, which are now located in the common GX component.
#include "pch.h"

using namespace Debug;

// TODO: Refactoring hacks

size_t done_num;   // number of drawdone (PE_FINISH) events

static void CPDrawDoneCallback()
{
    done_num++;
    vi.frames++;
    if (done_num == 1)
    {
        vi.xfb = 0;     // disable VI output
    }

    Flipper::Gx->CPDrawDoneCallback();
}

static void CPDrawTokenCallback(uint16_t tokenValue)
{
    vi.frames++;
    vi.xfb = 0;     // disable VI output

    Flipper::Gx->CPDrawTokenCallback(tokenValue);
}

//
// Stubs
//

static void CPRegRead(uint32_t addr, uint32_t* reg)
{
    *reg = Flipper::Gx->CpReadReg((GX::CPMappedRegister)((addr & 0xFF) >> 1));
}

static void CPRegWrite(uint32_t addr, uint32_t data)
{
    Flipper::Gx->CpWriteReg((GX::CPMappedRegister)((addr & 0xFF) >> 1), data);
}

static void PERegRead(uint32_t addr, uint32_t* reg)
{
    *reg = Flipper::Gx->PeReadReg(  (GX::PEMappedRegister)((addr & 0xFF) >> 1) );
}

static void PERegWrite(uint32_t addr, uint32_t data)
{
    Flipper::Gx->PeWriteReg((GX::PEMappedRegister)((addr & 0xFF) >> 1), data);
}

// init

void CP_PEOpen()
{
    Report(Channel::CP, "Command processor and Pixel Engine (for GX)\n");

    // Command Processor
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_STATUS_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_ENABLE_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_CLR_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_MEMPERF_SEL_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_STM_LOW_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_BASEL_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_BASEH_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_TOPL_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_TOPH_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_HICNTL_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_HICNTH_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_LOCNTL_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_LOCNTH_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_COUNTL_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_COUNTH_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_WPTRL_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_WPTRH_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_RPTRL_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_RPTRH_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_BRKL_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FIFO_BRKH_ID), CPRegRead, CPRegWrite);

    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_COUNTER0L_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_COUNTER0H_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_COUNTER1L_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_COUNTER1H_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_COUNTER2L_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_COUNTER2H_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_COUNTER3L_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_COUNTER3H_ID), CPRegRead, CPRegWrite);

    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_VC_CHKCNTL_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_VC_CHKCNTH_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_VC_MISSL_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_VC_MISSH_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_VC_STALLL_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_VC_STALLH_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FRCLK_CNTL_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_FRCLK_CNTH_ID), CPRegRead, CPRegWrite);

    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_XF_ADDR_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_XF_DATAL_ID), CPRegRead, CPRegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_CP, GX::CPMappedRegister::CP_XF_DATAH_ID), CPRegRead, CPRegWrite);

    // Pixel Engine
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_POKE_ZMODE_ID), PERegRead, PERegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_POKE_CMODE0_ID), PERegRead, PERegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_POKE_CMODE1_ID), PERegRead, PERegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_POKE_AMODE0_ID), PERegRead, PERegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_POKE_AMODE1_ID), PERegRead, PERegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_SR_ID), PERegRead, PERegWrite);
    MISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_PE, GX::PEMappedRegister::PE_TOKEN_ID), PERegRead, PERegWrite);

    // TODO: Refactoring hacks
    GXSetDrawCallbacks(CPDrawDoneCallback, CPDrawTokenCallback);
}

void CP_PEClose()
{
}
