// Gekko Memory interface

#include "pch.h"

namespace Gekko
{

    // Centralized hub which attracts all memory access requests from the interpreter or recompiler 
    // (as well as those who they pretend, for example HLE or Debugger).

    void __fastcall GekkoCore::ReadByte(uint32_t addr, uint32_t *reg)
    {
        TestReadBreakpoints(addr);

        uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Read);
        if (pa == BadAddress)
        {
            regs.spr[(int)SPR::DAR] = addr;
            Exception(Exception::DSI);
            return;
        }

        if (cache.IsEnabled() && (LastWIMG & WIMG_I) == 0)
        {
            cache.ReadByte(pa, reg);
            return;
        }

        if (!cache.IsEnabled() && (addr & ~0x3fff) == DOLPHIN_OS_LOCKED_CACHE_ADDRESS)
        {
            cache.ReadByte(pa, reg);
            return;
        }

        MIReadByte(pa, reg);
    }

    void __fastcall GekkoCore::WriteByte(uint32_t addr, uint32_t data)
    {
        TestWriteBreakpoints(addr);

        uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Write);
        if (pa == BadAddress)
        {
            regs.spr[(int)SPR::DAR] = addr;
            Exception(Exception::DSI);
            return;
        }
    
        if (Gekko::Gekko->regs.spr[(int)Gekko::SPR::HID2] & HID2_WPE)
        {
            if ((pa & ~0x1f) == (Gekko::Gekko->regs.spr[(int)Gekko::SPR::WPAR] & ~0x1f))
            {
                Gekko::Gekko->gatherBuffer.Write8((uint8_t)data);
                return;
            }
        }
    
        if (cache.IsEnabled() && (LastWIMG & WIMG_I) == 0)
        {
            cache.WriteByte(pa, data);
            if ((LastWIMG & WIMG_W) == 0)
                return;
        }

        if (!cache.IsEnabled() && (addr & ~0x3fff) == DOLPHIN_OS_LOCKED_CACHE_ADDRESS)
        {
            cache.WriteByte(pa, data);
            return;
        }

        MIWriteByte(pa, data);
    }

    void __fastcall GekkoCore::ReadHalf(uint32_t addr, uint32_t *reg)
    {
        TestReadBreakpoints(addr);

        uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Read);
        if (pa == BadAddress)
        {
            regs.spr[(int)SPR::DAR] = addr;
            Exception(Exception::DSI);
            return;
        }

        if (cache.IsEnabled() && (LastWIMG & WIMG_I) == 0)
        {
            cache.ReadHalf(pa, reg);
            return;
        }

        if (!cache.IsEnabled() && (addr & ~0x3fff) == DOLPHIN_OS_LOCKED_CACHE_ADDRESS)
        {
            cache.ReadHalf(pa, reg);
            return;
        }

        MIReadHalf(pa, reg);
    }

    void __fastcall GekkoCore::ReadHalfS(uint32_t addr, uint32_t *reg)
    {
        ReadHalf(addr, reg);
        if (*reg & 0x8000) *reg |= 0xffff0000;
    }

    void __fastcall GekkoCore::WriteHalf(uint32_t addr, uint32_t data)
    {
        TestWriteBreakpoints(addr);

        uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Write);
        if (pa == BadAddress)
        {
            regs.spr[(int)SPR::DAR] = addr;
            Exception(Exception::DSI);
            return;
        }

        if (Gekko::Gekko->regs.spr[(int)Gekko::SPR::HID2] & HID2_WPE)
        {
            if ((pa & ~0x1f) == (Gekko::Gekko->regs.spr[(int)Gekko::SPR::WPAR] & ~0x1f))
            {
                Gekko::Gekko->gatherBuffer.Write16((uint16_t)data);
                return;
            }
        }

        if (cache.IsEnabled() && (LastWIMG & WIMG_I) == 0)
        {
            cache.WriteHalf(pa, data);
            if ((LastWIMG & WIMG_W) == 0)
                return;
        }

        if (!cache.IsEnabled() && (addr & ~0x3fff) == DOLPHIN_OS_LOCKED_CACHE_ADDRESS)
        {
            cache.WriteHalf(pa, data);
            return;
        }

        MIWriteHalf(pa, data);
    }

    void __fastcall GekkoCore::ReadWord(uint32_t addr, uint32_t *reg)
    {
        TestReadBreakpoints(addr);

        uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Read);
        if (pa == BadAddress)
        {
            regs.spr[(int)SPR::DAR] = addr;
            Exception(Exception::DSI);
            return;
        }

        if (cache.IsEnabled() && (LastWIMG & WIMG_I) == 0)
        {
            cache.ReadWord(pa, reg);
            return;
        }

        if (!cache.IsEnabled() && (addr & ~0x3fff) == DOLPHIN_OS_LOCKED_CACHE_ADDRESS)
        {
            cache.ReadWord(pa, reg);
            return;
        }

        MIReadWord(pa, reg);
    }

    void __fastcall GekkoCore::WriteWord(uint32_t addr, uint32_t data)
    {
        TestWriteBreakpoints(addr);

        uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Write);
        if (pa == BadAddress)
        {
            regs.spr[(int)SPR::DAR] = addr;
            Exception(Exception::DSI);
            return;
        }

        if (Gekko::Gekko->regs.spr[(int)Gekko::SPR::HID2] & HID2_WPE)
        {
            if ((pa & ~0x1f) == (Gekko::Gekko->regs.spr[(int)Gekko::SPR::WPAR] & ~0x1f))
            {
                Gekko::Gekko->gatherBuffer.Write32(data);
                return;
            }
        }

        if (cache.IsEnabled() && (LastWIMG & WIMG_I) == 0)
        {
            cache.WriteWord(pa, data);
            if ((LastWIMG & WIMG_W) == 0)
                return;
        }

        if (!cache.IsEnabled() && (addr & ~0x3fff) == DOLPHIN_OS_LOCKED_CACHE_ADDRESS)
        {
            cache.WriteWord(pa, data);
            return;
        }

        MIWriteWord(pa, data);
    }

    void __fastcall GekkoCore::ReadDouble(uint32_t addr, uint64_t *reg)
    {
        TestReadBreakpoints(addr);

        uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Read);
        if (pa == BadAddress)
        {
            regs.spr[(int)SPR::DAR] = addr;
            Exception(Exception::DSI);
            return;
        }

        if (cache.IsEnabled() && (LastWIMG & WIMG_I) == 0)
        {
            cache.ReadDouble(pa, reg);
            return;
        }

        if (!cache.IsEnabled() && (addr & ~0x3fff) == DOLPHIN_OS_LOCKED_CACHE_ADDRESS)
        {
            cache.ReadDouble(pa, reg);
            return;
        }

        // It is suspected that this type of single-beat transaction is not supported by Flipper MI.

        MIReadDouble(pa, reg);
    }

    void __fastcall GekkoCore::WriteDouble(uint32_t addr, uint64_t *data)
    {
        TestWriteBreakpoints(addr);

        uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Write);
        if (pa == BadAddress)
        {
            regs.spr[(int)SPR::DAR] = addr;
            Exception(Exception::DSI);
            return;
        }

        if (Gekko::Gekko->regs.spr[(int)Gekko::SPR::HID2] & HID2_WPE)
        {
            if ((pa & ~0x1f) == (Gekko::Gekko->regs.spr[(int)Gekko::SPR::WPAR] & ~0x1f))
            {
                Gekko::Gekko->gatherBuffer.Write64(*data);
                return;
            }
        }

        if (cache.IsEnabled() && (LastWIMG & WIMG_I) == 0)
        {
            cache.WriteDouble(pa, data);
            if ((LastWIMG & WIMG_W) == 0)
                return;
        }

        if (!cache.IsEnabled() && (addr & ~0x3fff) == DOLPHIN_OS_LOCKED_CACHE_ADDRESS)
        {
            cache.WriteDouble(pa, data);
            return;
        }

        // It is suspected that this type of single-beat transaction is not supported by Flipper MI.

        MIWriteDouble(pa, data);
    }

}
