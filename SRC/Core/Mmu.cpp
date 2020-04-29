// Gekko Memory interface

// MMU never throws Gekko exceptions. If something went wrong, BadAddress is returned. Then the consumer decides what to do.

#include "pch.h"

namespace Gekko
{
    // Centralized hub which attracts all memory access requests from the interpreter or recompiler 
    // (as well as those who they pretend, for example HLE or Debugger).

    void __fastcall GekkoCore::ReadByte(uint32_t addr, uint32_t *reg)
    {
        // Locked cache
        if (addr >= 0xe0000000)
        {
            uint8_t * ptr = &lc[addr & 0x3ffff];
            *reg = (uint32_t)*ptr;
            return;
        }

        uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Read);
        if (pa == BadAddress)
        {
            regs.spr[(int)SPR::DAR] = addr;
            Exception(Exception::DSI);
            return;
        }

        MIReadByte(pa, reg);
    }

    void __fastcall GekkoCore::WriteByte(uint32_t addr, uint32_t data)
    {
        // Locked cache
        if (addr >= 0xe0000000)
        {
            uint8_t* ptr = &lc[addr & 0x3ffff];
            *ptr = (uint8_t)data;
            return;
        }

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
    
        MIWriteByte(pa, data);
    }

    void __fastcall GekkoCore::ReadHalf(uint32_t addr, uint32_t *reg)
    {
        // Locked cache
        if (addr >= 0xe0000000)
        {
            uint8_t* ptr = &lc[addr & 0x3ffff];
            *reg = (uint32_t)_byteswap_ushort(*(uint16_t*)ptr);
            return;
        }

        uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Read);
        if (pa == BadAddress)
        {
            regs.spr[(int)SPR::DAR] = addr;
            Exception(Exception::DSI);
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
        // Locked cache
        if (addr >= 0xe0000000)
        {
            uint8_t* ptr = &lc[addr & 0x3ffff];
            *(uint16_t*)ptr = _byteswap_ushort((uint16_t)data);
            return;
        }

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

        MIWriteHalf(pa, data);
    }

    void __fastcall GekkoCore::ReadWord(uint32_t addr, uint32_t *reg)
    {
        // Locked cache
        if (addr >= 0xe0000000)
        {
            uint8_t* ptr = &lc[addr & 0x3ffff];
            *reg = _byteswap_ulong(*(uint32_t*)ptr);
            return;
        }

        uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Read);
        if (pa == BadAddress)
        {
            regs.spr[(int)SPR::DAR] = addr;
            Exception(Exception::DSI);
            return;
        }
        MIReadWord(pa, reg);
    }

    void __fastcall GekkoCore::WriteWord(uint32_t addr, uint32_t data)
    {
        // Locked cache
        if (addr >= 0xe0000000)
        {
            uint8_t* ptr = &lc[addr & 0x3ffff];
            *(uint32_t*)ptr = _byteswap_ulong(data);
            return;
        }
   
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

        MIWriteWord(pa, data);
    }

    void __fastcall GekkoCore::ReadDouble(uint32_t addr, uint64_t *reg)
    {
        // Locked cache
        if (addr >= 0xe0000000)
        {
            uint8_t* buf = &lc[addr & 0x3ffff];
            *reg = _byteswap_uint64(*(uint64_t *)buf);
            return;
        }

        uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Read);
        if (pa == BadAddress)
        {
            regs.spr[(int)SPR::DAR] = addr;
            Exception(Exception::DSI);
            return;
        }
        MIReadDouble(pa, reg);
    }

    void __fastcall GekkoCore::WriteDouble(uint32_t addr, uint64_t *data)
    {
        // Locked cache
        if (addr >= 0xe0000000)
        {
            uint8_t* buf = &lc[addr & 0x3ffff];
            *(uint64_t *)buf = _byteswap_uint64(*data);
            return;
        }

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

        MIWriteDouble(pa, data);
    }

#pragma region "MMU"

    uint32_t __fastcall GekkoCore::EffectiveToPhysicalNoMmu(uint32_t ea, MmuAccess type)
    {
        // Required to run bootrom
        if (ea >= BOOTROM_START_ADDRESS)
        {
            return ea;
        }

        // Ignore no memory, page faults, alignment, etc errors
        return ea & RAMMASK;
    }

    // BAT fields
    #define BATBEPI(batu)   (batu >> 17)
    #define BATBL(batu)     ((batu >> 2) & 0x7ff)
    #define BATBRPN(batl)   (batl >> 17)

    bool __fastcall GekkoCore::BlockAddressTranslation(uint32_t ea, uint32_t &pa, MmuAccess type)
    {
        // Ignore BAT access rights for now (not used in embodiment system)

        if (type == MmuAccess::Execute)
        {
            if ((regs.msr & MSR_IR) == 0)
            {
                pa = ea;
                return true;
            }

            for (int n = 0; n < 4; n++)
            {
                uint32_t bepi = BATBEPI(*ibatu[n]);
                uint32_t bl = BATBL(*ibatu[n]);
                uint32_t tst = (ea >> 17) & (0x7800 | ~bl);
                if (bepi == tst)
                {
                    pa = BATBRPN(*ibatl[n]) | ((ea >> 17) & bl);
                    pa = (pa << 17) | (ea & 0x1ffff);
                    MmuLastResult = MmuResult::Ok;
                    return true;
                }
            }
        }
        else
        {
            if ((regs.msr & MSR_DR) == 0)
            {
                pa = ea;
                return true;
            }

            for (int n = 0; n < 4; n++)
            {
                uint32_t bepi = BATBEPI(*dbatu[n]);
                uint32_t bl = BATBL(*dbatu[n]);
                uint32_t tst = (ea >> 17) & (0x7800 | ~bl);
                if (bepi == tst)
                {
                    pa = BATBRPN(*dbatl[n]) | ((ea >> 17) & bl);
                    pa = (pa << 17) | (ea & 0x1ffff);
                    MmuLastResult = MmuResult::Ok;
                    return true;
                }
            }
        }

        // No BAT match, continue page table translation

        return false;
    }

    uint32_t __fastcall GekkoCore::SegmentTranslation(uint32_t ea, MmuAccess type)
    {
        int ptegUpper;

        // Direct Store (T=1) not supported.

        uint32_t sr = regs.sr[ea >> 28];

        if (sr & 0x1000'0000 && type == MmuAccess::Execute)
        {
            MmuLastResult = MmuResult::NoExecute;
            return BadAddress;
        }

        int key;
        if (regs.msr & MSR_PR)
        {
            key = sr & 0x2000'0000 ? 4 : 0;
        }
        else
        {
            key = sr & 0x4000'0000 ? 4 : 0;
        }

        // Calculate PTEG physical addresses

        uint64_t vpn = (((uint64_t)sr & 0x00ffffff) << 16) | ((ea >> 12) & 0xffff);
        uint32_t hash = ((uint32_t)(vpn >> 16) & 0x7ffff) ^ ((uint32_t)vpn & 0x1fff);

        uint32_t sdr = regs.spr[(int)SPR::SDR1];

        uint32_t primaryPteAddr = 0;
        ptegUpper = (sdr >> 16) & 0x1ff;
        ptegUpper |= (sdr & 0x1ff) & ((hash >> 10) & 0x1ff);
        primaryPteAddr |= sdr & 0xfe00'0000;
        primaryPteAddr |= ptegUpper << 16;
        primaryPteAddr |= (hash & 0x3ff) << 6;

        hash = ~hash;

        uint32_t secondaryPteAddr = 0;
        ptegUpper = (sdr >> 16) & 0x1ff;
        ptegUpper |= (sdr & 0x1ff) & ((hash >> 10) & 0x1ff);
        secondaryPteAddr |= sdr & 0xfe00'0000;
        secondaryPteAddr |= ptegUpper << 16;
        secondaryPteAddr |= (hash & 0x3ff) << 6;

        // Try Primary PTEGs

        for (int i = 0; i < 8; i++)
        {
            // Load PTE

            uint32_t pte[2];

            MIReadWord(primaryPteAddr, &pte[0]);
            MIReadWord(primaryPteAddr + 4, &pte[1]);

            // Check Hash Bit

            if ((pte[0] & 0x40) != 0)
            {
                primaryPteAddr += 8;
                continue;
            }

            // Valid and suitable? (PTE [VSID, API, V] = Seg Desc [VSID], EA[API], 1)

            if (pte[0] & 0x8000'0000 && 
                ((pte[0] >> 7) & 0xffffff) == ((vpn >> 16) & 0xffffff) &&
                (pte[0] & 0x3f) == ((vpn >> 10) & 0x3f) )
            {
                // Referenced
                pte[1] |= 0x100;

                // Check Protection
                int pp = key | (pte[1] & 3);
                bool protectViolation = false;
                if (type == MmuAccess::Read || type == MmuAccess::Execute)
                {
                    if (pp == 0b100)
                    {
                        protectViolation = true;
                    }
                }
                else if (type == MmuAccess::Write)
                {
                    if (pp == 0b011 || pp == 0b100 || pp == 0b101 || pp == 0b111)
                    {
                        protectViolation = true;
                    }
                }

                if (type == MmuAccess::Write && !protectViolation)
                {
                    pte[1] |= 0x80;     // Changed
                }
                MIWriteWord(primaryPteAddr + 4, pte[1]);

                if (protectViolation)
                {
                    MmuLastResult = MmuResult::Protected;
                    return BadAddress;
                }

                uint32_t pa = (pte[1] & ~0xfff) | (ea & 0xfff);
                MmuLastResult = MmuResult::Ok;
                return pa;
            }

            primaryPteAddr += 8;
        }

        // Try Secondary PTEGs

        for (int i = 0; i < 8; i++)
        {
            // Load PTE

            uint32_t pte[2];

            MIReadWord(secondaryPteAddr, &pte[0]);
            MIReadWord(secondaryPteAddr + 4, &pte[1]);

            // Check Hash Bit

            if ((pte[0] & 0x40) == 0)
            {
                secondaryPteAddr += 8;
                continue;
            }

            // Valid and suitable? (PTE [VSID, API, V] = Seg Desc [VSID], EA[API], 1)

            if (pte[0] & 0x8000'0000 &&
                ((pte[0] >> 7) & 0xffffff) == ((vpn >> 16) & 0xffffff) &&
                (pte[0] & 0x3f) == ((vpn >> 10) & 0x3f) )
            {
                // Referenced
                pte[1] |= 0x100;

                // Check Protection
                int pp = key | (pte[1] & 3);
                bool protectViolation = false;
                if (type == MmuAccess::Read || type == MmuAccess::Execute)
                {
                    if (pp == 0b100)
                    {
                        protectViolation = true;
                    }
                }
                else if (type == MmuAccess::Write)
                {
                    if (pp == 0b011 || pp == 0b100 || pp == 0b101 || pp == 0b111)
                    {
                        protectViolation = true;
                    }
                }

                // Changed
                if (type == MmuAccess::Write && !protectViolation)
                {
                    pte[1] |= 0x80;
                }
                MIWriteWord(secondaryPteAddr + 4, pte[1]);

                if (protectViolation)
                {
                    MmuLastResult = MmuResult::Protected;
                    return BadAddress;
                }

                uint32_t pa = (pte[1] & ~0xfff) | (ea & 0xfff);
                MmuLastResult = MmuResult::Ok;
                return pa;
            }

            secondaryPteAddr += 8;
        }

        MmuLastResult = MmuResult::PageFault;
        return BadAddress;
    }

    uint32_t __fastcall GekkoCore::EffectiveToPhysicalMmu(uint32_t ea, MmuAccess type)
    {
        uint32_t pa;
        //if (!tlb.Exists(ea, pa))
        {
            if (!BlockAddressTranslation(ea, pa, type))
            {
                pa = SegmentTranslation(ea, type);
            }
            //if (pa != BadAddress)
            //{
            //    tlb.Map(ea, pa);
            //}
        }
        return pa;
    }

#pragma endregion "MMU"

}
