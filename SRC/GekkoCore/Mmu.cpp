// MMU never throws Gekko exceptions. If something went wrong, BadAddress is returned. Then the consumer decides what to do.

// The result of the MMU is placed in MmuLastResult and translated address as output.

// We also use BadAddress as a signal that the translation did not pass (a special address that is usually not used by anyone).

#include "pch.h"

namespace Gekko
{

    // A simple translation, which is configured by the Dolphin OS software environment (until it starts using MMU for ARAM mapping).

    uint32_t GekkoCore::EffectiveToPhysicalNoMmu(uint32_t ea, MmuAccess type, int& WIMG)
    {
        WIMG = WIMG_I;      // Caching inhibited

        // Locked cache
        if ((ea & ~0x3fff) == DOLPHIN_OS_LOCKED_CACHE_ADDRESS && cache.IsLockedEnable())
        {
            WIMG = 0;
            return ea;
        }

        // Required to run bootrom
        if ((ea & ~0xfffff) == BOOTROM_START_ADDRESS)
        {
            return ea;
        }

        // Ignore no memory, page faults, alignment, etc errors
        return ea & RAMMASK;
    }

    // Native address translation defined by PowerPC architecture. There are some alien moments (Hash for Page Tables), but overall its fine.

    uint32_t GekkoCore::EffectiveToPhysicalMmu(uint32_t ea, MmuAccess type, int& WIMG)
    {
        uint32_t pa;

        WIMG = 0;

        // Try TLB

        TLB* tlb = (type == MmuAccess::Execute) ? &itlb : &dtlb;

        if (tlb->Exists(ea, pa, WIMG))
        {
            return pa;
        }

        // First, try the block translation, if it doesn’t work, try the Page Table.

        if (!BlockAddressTranslation(ea, pa, type, WIMG))
        {
            pa = SegmentTranslation(ea, type, WIMG);
        }

        // Put in TLB

        if (MmuLastResult == MmuResult::Ok)
        {
            tlb->Map(ea, pa, WIMG);
        }

        return pa;
    }

    bool GekkoCore::BlockAddressTranslation(uint32_t ea, uint32_t &pa, MmuAccess type, int& WIMG)
    {
        // Ignore BAT access rights for now (not used in embodiment system)

        if (type == MmuAccess::Execute)
        {
            if ((regs.msr & MSR_IR) == 0)
            {
                WIMG = WIMG_G;
                pa = ea;
                return true;
            }

            for (int n = 0; n < 4; n++)
            {
                bool valid = (*ibatu[n] & 3) != 0;
                if (!valid)
                    continue;

                uint32_t bepi = BATBEPI(*ibatu[n]);
                uint32_t bl = BATBL(*ibatu[n]);
                uint32_t tst = (ea >> 17) & (0x7800 | ~bl);
                if (bepi == tst)
                {
                    pa = BATBRPN(*ibatl[n]) | ((ea >> 17) & bl);
                    pa = (pa << 17) | (ea & 0x1ffff);
                    WIMG = (*ibatl[n] >> 3) & 0xf;
                    MmuLastResult = MmuResult::Ok;
                    return true;
                }
            }
        }
        else
        {
            if ((regs.msr & MSR_DR) == 0)
            {
                WIMG = WIMG_M | WIMG_G;
                pa = ea;
                return true;
            }

            for (int n = 0; n < 4; n++)
            {
                bool valid = (*dbatu[n] & 3) != 0;
                if (!valid)
                    continue;

                uint32_t bepi = BATBEPI(*dbatu[n]);
                uint32_t bl = BATBL(*dbatu[n]);
                uint32_t tst = (ea >> 17) & (0x7800 | ~bl);
                if (bepi == tst)
                {
                    pa = BATBRPN(*dbatl[n]) | ((ea >> 17) & bl);
                    pa = (pa << 17) | (ea & 0x1ffff);
                    WIMG = (*dbatl[n] >> 3) & 0xf;
                    MmuLastResult = MmuResult::Ok;
                    return true;
                }
            }
        }

        // No BAT match, continue page table translation

        return false;
    }

    uint32_t GekkoCore::SegmentTranslation(uint32_t ea, MmuAccess type, int& WIMG)
    {
        int ptegUpper;

        // Direct Store (T=1) not supported (and it's not even checked, because it makes no sense).

        uint32_t sr = regs.sr[ea >> 28];

        if (sr & 0x1000'0000 && type == MmuAccess::Execute)
        {
            MmuLastResult = MmuResult::NoExecute;
            return BadAddress;
        }

        uint32_t key;
        if (regs.msr & MSR_PR)
        {
            key = sr & 0x2000'0000 ? 4 : 0;     // Kp
        }
        else
        {
            key = sr & 0x4000'0000 ? 4 : 0;     // Ks
        }

        // Calculate PTEG physical addresses

        uint64_t vpn = (((uint64_t)sr & 0x00ffffff) << 16) | ((ea >> 12) & 0xffff);
        uint32_t hash = ((uint32_t)(vpn >> 16) & 0x7ffff) ^ ((uint32_t)vpn & 0xffff);

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

        // TODO: unify these loops

        // Try Primary PTEGs

        for (int i = 0; i < 8; i++)
        {
            // Load PTE

            uint32_t pte[2];

            PIReadWord(primaryPteAddr, &pte[0]);
            PIReadWord(primaryPteAddr + 4, &pte[1]);

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
                uint32_t pp = key | (pte[1] & 3);
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
                PIWriteWord(primaryPteAddr + 4, pte[1]);

                if (protectViolation)
                {
                    switch (type)
                    {
                        case MmuAccess::Read:
                            MmuLastResult = MmuResult::ProtectedRead;
                            break;
                        case MmuAccess::Write:
                            MmuLastResult = MmuResult::ProtectedWrite;
                            break;
                        case MmuAccess::Execute:
                            MmuLastResult = MmuResult::ProtectedFetch;
                            break;
                    }

                    return BadAddress;
                }

                uint32_t pa = (pte[1] & ~0xfff) | (ea & 0xfff);
                if (type != MmuAccess::Execute)
                {
                    WIMG = (pte[1] >> 3) & 0xf;
                }
                MmuLastResult = MmuResult::Ok;
                return pa;
            }
            else
            {
                // Referenced
                pte[1] |= 0x100;
                PIWriteWord(primaryPteAddr + 4, pte[1]);
            }

            primaryPteAddr += 8;
        }

        // Try Secondary PTEGs

        for (int i = 0; i < 8; i++)
        {
            // Load PTE

            uint32_t pte[2];

            PIReadWord(secondaryPteAddr, &pte[0]);
            PIReadWord(secondaryPteAddr + 4, &pte[1]);

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
                uint32_t pp = key | (pte[1] & 3);
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
                PIWriteWord(secondaryPteAddr + 4, pte[1]);

                if (protectViolation)
                {
                    switch (type)
                    {
                        case MmuAccess::Read:
                            MmuLastResult = MmuResult::ProtectedRead;
                            break;
                        case MmuAccess::Write:
                            MmuLastResult = MmuResult::ProtectedWrite;
                            break;
                        case MmuAccess::Execute:
                            MmuLastResult = MmuResult::ProtectedFetch;
                            break;
                    }

                    return BadAddress;
                }

                uint32_t pa = (pte[1] & ~0xfff) | (ea & 0xfff);
                if (type != MmuAccess::Execute)
                {
                    WIMG = (pte[1] >> 3) & 0xf;
                }
                MmuLastResult = MmuResult::Ok;
                return pa;
            }
            else
            {
                // Referenced
                pte[1] |= 0x100;
                PIWriteWord(secondaryPteAddr + 4, pte[1]);
            }

            secondaryPteAddr += 8;
        }

        MmuLastResult = MmuResult::PageFault;
        return BadAddress;
    }

}
