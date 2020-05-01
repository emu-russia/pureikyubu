// System Instructions
#include "../pch.h"
#include "interpreter.h"

namespace Gekko
{

    OP(TWI)
    {
        int32_t a = RRA, b = SIMM;
        int32_t to = RS;

        if (((a < b) && (to & 0x10)) ||
            ((a > b) && (to & 0x08)) ||
            ((a == b) && (to & 0x04)) ||
            (((uint32_t)a < (uint32_t)b) && (to & 0x02)) ||
            (((uint32_t)a > (uint32_t)b) && (to & 0x01)))
        {
            // pseudo-branch (to resume from next instruction after 'rfi')
            Gekko->regs.pc += 4;
            Gekko->PrCause = PrivilegedCause::Trap;
            Gekko->Exception(Gekko::Exception::PROGRAM);
        }
    }

    OP(TW)
    {
        int32_t a = RRA, b = RRB;
        int32_t to = RS;
        bool trap = false;

        if (((a < b) && (to & 0x10)) ||
            ((a > b) && (to & 0x08)) ||
            ((a == b) && (to & 0x04)) ||
            (((uint32_t)a < (uint32_t)b) && (to & 0x02)) ||
            (((uint32_t)a > (uint32_t)b) && (to & 0x01)))
        {
            // pseudo-branch (to resume from next instruction after 'rfi')
            Gekko->regs.pc += 4;
            Gekko->PrCause = PrivilegedCause::Trap;
            Gekko->Exception(Gekko::Exception::PROGRAM);
        }
    }

    // syscall
    OP(SC)
    {
        // pseudo-branch (to resume from next instruction after 'rfi')
        Gekko->regs.pc += 4;
        Gekko->Exception(Gekko::Exception::SYSCALL);
    }

    // return from exception
    OP(RFI)
    {
        Gekko->regs.msr &= ~(0x87C0FF73 | 0x00040000);
        Gekko->regs.msr |= Gekko->regs.spr[(int)SPR::SRR1] & 0x87C0FF73;
        Gekko->regs.pc = Gekko->regs.spr[(int)SPR::SRR0] & ~3;
        Gekko->interp->branch = true;
    }

    // ---------------------------------------------------------------------------
    // system registers

    static inline bool msr_ir() { return (Gekko->regs.msr & MSR_IR) ? true : false; }
    static inline bool msr_dr() { return (Gekko->regs.msr & MSR_DR) ? true : false; }

    // mask = (4)CRM[0] || (4)CRM[1] || ... || (4)CRM[7]
    // CR = (rs & mask) | (CR & ~mask)
    OP(MTCRF)
    {
        uint32_t m, crm = CRM, a, d = RRS;

        for (int i = 0; i < 8; i++)
        {
            if ((crm >> i) & 1)
            {
                a = (d >> (i << 2)) & 0xf;
                m = (0xf << (i << 2));
                Gekko->regs.cr = (Gekko->regs.cr & ~m) | (a << (i << 2));
            }
        }
    }

    // CR[4 * crfD .. 4 * crfd + 3] = XER[0-3]
    // XER[0..3] = 0b0000
    OP(MCRXR)
    {
        uint32_t mask = 0xf0000000 >> (4 * CRFD);
        Gekko->regs.cr &= ~mask;
        Gekko->regs.cr |= (Gekko->regs.spr[(int)SPR::XER] & 0xf0000000) >> (4 * CRFD);
        Gekko->regs.spr[(int)SPR::XER] &= ~0xf0000000;
    }

    // rd = cr
    OP(MFCR)
    {
        RRD = Gekko->regs.cr;
    }

    // msr = rs
    OP(MTMSR)
    {
        if (Gekko->regs.msr & MSR_PR)
        {
            Gekko->PrCause = PrivilegedCause::Privileged;
            Gekko->Exception(Exception::PROGRAM);
            return;
        }

        Gekko->regs.msr = RRS;
    }

    // rd = msr
    OP(MFMSR)
    {
        if (Gekko->regs.msr & MSR_PR)
        {
            Gekko->PrCause = PrivilegedCause::Privileged;
            Gekko->Exception(Exception::PROGRAM);
            return;
        }

        RRD = Gekko->regs.msr;
    }

    // spr = rs
    OP(MTSPR)
    {
        int spr = (RB << 5) | RA;

        if (spr >= 528 && spr <= 543)
        {
            static const char* bat[] = {
                "IBAT0U", "IBAT0L", "IBAT1U", "IBAT1L",
                "IBAT2U", "IBAT2L", "IBAT3U", "IBAT3L",
                "DBAT0U", "DBAT0L", "DBAT1U", "DBAT1L",
                "DBAT2U", "DBAT2L", "DBAT3U", "DBAT3L"
            };
            DBReport2(DbgChannel::CPU, "%s <- %08X (IR:%i DR:%i pc:%08X)\n",
                bat[spr - 528], RRS, msr_ir(), msr_dr(), Gekko->regs.pc);
        }
        else switch (spr)
        {
            // decrementer
            case (int)SPR::DEC:
                //DBReport2(DbgChannel::CPU, "set decrementer (OS alarm) to %08X\n", RRS);
                break;

            // page table base
            case (int)SPR::SDR1:
                DBReport2(DbgChannel::CPU, "SDR <- %08X (IR:%i DR:%i pc:%08X)\n",
                    RRS, msr_ir(), msr_dr(), Gekko->regs.pc);
                break;

            case    284:
                Gekko->regs.tb.Part.l = RRS;
                DBReport2(DbgChannel::CPU, "set TBL : %08X\n", Gekko->regs.tb.Part.l);
                break;
            case    285:
                Gekko->regs.tb.Part.u = RRS;
                DBReport2(DbgChannel::CPU, "set TBH : %08X\n", Gekko->regs.tb.Part.u);
                break;

            // write gathering buffer
            case (int)SPR::WPAR:
                //assert(RRS == 0x0C008000);
                break;

            case (int)SPR::HID0:
            {
                uint32_t bits = RRS;
                Gekko->cache.Enable((bits & HID0_DCE) ? true : false);
                Gekko->cache.Freeze((bits & HID0_DLOCK) ? true : false);
                if (bits & HID0_DCFI)
                {
                    Gekko->cache.Reset();
                }
            }
            break;

            // locked cache dma (dirty hack)
            case (int)SPR::DMAU:
                Gekko->regs.spr[spr] = RRS;
                break;
            case (int)SPR::DMAL:
            {
                Gekko->regs.spr[spr] = RRS;
                if (Gekko->regs.spr[923] & 2)
                {
                    uint32_t maddr = Gekko->regs.spr[922] & ~0x1f;
                    uint32_t lcaddr = Gekko->regs.spr[923] & ~0x1f;
                    uint32_t length = ((Gekko->regs.spr[922] & 0x1f) << 2) | ((Gekko->regs.spr[923] >> 2) & 3);
                    if (length == 0) length = 128;
                    if (Gekko->regs.spr[923] & 0x10)
                    {   // load
                        memcpy(
                            &mi.ram[maddr & RAMMASK],
                            &Gekko->lc[lcaddr & 0x3ffff],
                            length * 32
                        );
                    }
                    else
                    {   // store
                        memcpy(
                            &Gekko->lc[lcaddr & 0x3ffff],
                            &mi.ram[maddr & RAMMASK],
                            length * 32
                        );
                    }
                }

                /*/
                            DolwinQuestion(
                                "Non-predictable situation!",
                                "Locked cache is not implemented!\n"
                                "Are you sure, you want to continue?"
                            );
                /*/
            }
            break;
        }

        // default
        Gekko->regs.spr[spr] = RRS;
    }

    // rd = spr
    OP(MFSPR)
    {
        int spr = (RB << 5) | RA;
        uint32_t value;

        switch (spr)
        {
            case (int)SPR::WPAR:
                value = (Gekko->regs.spr[spr] & ~0x1f) | (Gekko->gatherBuffer.NotEmpty() ? 1 : 0);
                break;

            case (int)SPR::HID1:
                // Gekko PLL_CFG = 0b1000
                value = 0x8000'0000;
                break;

            default:
                value = Gekko->regs.spr[spr];
                break;
        }

        RRD = value;
    }

    // rd = tbr
    OP(MFTB)
    {
        int tbr = (RB << 5) | RA;

        if (tbr == 268)
        {
            RRD = Gekko->regs.tb.Part.l;
            return;
        }
        else if (tbr == 269)
        {
            RRD = Gekko->regs.tb.Part.u;
            return;
        }
    }

    // sr[a] = rs
    OP(MTSR)
    {
        if (Gekko->regs.msr & MSR_PR)
        {
            Gekko->PrCause = PrivilegedCause::Privileged;
            Gekko->Exception(Exception::PROGRAM);
            return;
        }

        Gekko->regs.sr[RA] = RRS;
    }

    // sr[rb] = rs
    OP(MTSRIN)
    {
        if (Gekko->regs.msr & MSR_PR)
        {
            Gekko->PrCause = PrivilegedCause::Privileged;
            Gekko->Exception(Exception::PROGRAM);
            return;
        }

        Gekko->regs.sr[RRB & 0xf] = RRS;
    }

    // rd = sr[a]
    OP(MFSR)
    {
        if (Gekko->regs.msr & MSR_PR)
        {
            Gekko->PrCause = PrivilegedCause::Privileged;
            Gekko->Exception(Exception::PROGRAM);
            return;
        }

        RRD = Gekko->regs.sr[RA];
    }

    // rd = sr[rb]
    OP(MFSRIN)
    {
        if (Gekko->regs.msr & MSR_PR)
        {
            Gekko->PrCause = PrivilegedCause::Privileged;
            Gekko->Exception(Exception::PROGRAM);
            return;
        }

        RRD = Gekko->regs.sr[RRB & 0xf];
    }

    // ---------------------------------------------------------------------------
    // various context synchronizing

    OP(EIEIO)
    {
    }

    OP(SYNC)
    {
    }

    // instruction synchronize. Dolwin interpreter is not super-scalar. :)
    OP(ISYNC)
    {
    }

    OP(TLBSYNC)
    {
    }

    OP(TLBIE)
    {
        Gekko->tlb.Invalidate(RRB);
    }

    // ---------------------------------------------------------------------------
    // caches

    OP(DCBT)
    {
        if (Gekko->regs.spr[(int)Gekko::SPR::HID0] & HID0_NOOPTI)
            return;

        uint32_t ea = RA ? RRA + RRB : RRB;

        uint32_t pa = Gekko->EffectiveToPhysical(ea, MmuAccess::Read);
        if (pa != Gekko::BadAddress)
        {
            Gekko->cache.Touch(pa);
        }
        else
        {
            Gekko->regs.spr[(int)Gekko::SPR::DAR] = ea;
            Gekko->Exception(Exception::DSI);
        }
    }

    OP(DCBTST)
    {
        if (Gekko->regs.spr[(int)Gekko::SPR::HID0] & HID0_NOOPTI)
            return;

        uint32_t ea = RA ? RRA + RRB : RRB;

        uint32_t pa = Gekko->EffectiveToPhysical(ea, MmuAccess::Write);
        if (pa != Gekko::BadAddress)
        {
            Gekko->cache.TouchForStore(pa);
        }
        else
        {
            Gekko->regs.spr[(int)Gekko::SPR::DAR] = ea;
            Gekko->Exception(Exception::DSI);
        }
    }

    OP(DCBZ)
    {
        uint32_t ea = RA ? RRA + RRB : RRB;

        uint32_t pa = Gekko->EffectiveToPhysical(ea, MmuAccess::Write);
        if (pa != Gekko::BadAddress)
        {
            Gekko->cache.Zero(pa);
        }
        else
        {
            Gekko->regs.spr[(int)Gekko::SPR::DAR] = ea;
            Gekko->Exception(Exception::DSI);
        }
    }

    OP(DCBZ_L)
    {
        uint32_t ea = RA ? RRA + RRB : RRB;

        uint32_t pa = Gekko->EffectiveToPhysical(ea, MmuAccess::Write);
        if (pa != Gekko::BadAddress)
        {
            Gekko->cache.ZeroLocked(pa);
        }
        else
        {
            Gekko->regs.spr[(int)Gekko::SPR::DAR] = ea;
            Gekko->Exception(Exception::DSI);
        }
    }

    OP(DCBST)
    {
        uint32_t ea = RA ? RRA + RRB : RRB;

        uint32_t pa = Gekko->EffectiveToPhysical(ea, MmuAccess::Read);
        if (pa != Gekko::BadAddress)
        {
            Gekko->cache.Store(pa);
        }
        else
        {
            Gekko->regs.spr[(int)Gekko::SPR::DAR] = ea;
            Gekko->Exception(Exception::DSI);
        }
    }

    OP(DCBF)
    {
        uint32_t ea = RA ? RRA + RRB : RRB;

        uint32_t pa = Gekko->EffectiveToPhysical(ea, MmuAccess::Read);
        if (pa != Gekko::BadAddress)
        {
            Gekko->cache.Flush(pa);
        }
        else
        {
            Gekko->regs.spr[(int)Gekko::SPR::DAR] = ea;
            Gekko->Exception(Exception::DSI);
        }
    }

    OP(DCBI)
    {
        uint32_t ea = RA ? RRA + RRB : RRB;

        if (Gekko->regs.msr & MSR_PR)
        {
            Gekko->PrCause = PrivilegedCause::Privileged;
            Gekko->Exception(Exception::PROGRAM);
            return;
        }

        uint32_t pa = Gekko->EffectiveToPhysical(ea, MmuAccess::Write);
        if (pa != Gekko::BadAddress)
        {
            Gekko->cache.Invalidate(pa);
        }
        else
        {
            Gekko->regs.spr[(int)Gekko::SPR::DAR] = ea;
            Gekko->Exception(Exception::DSI);
        }
    }
    
    OP(ICBI)
    {
        uint32_t address = RA ? RRA + RRB : RRB;
        address &= ~0x1f;

        Gekko->jitc->Invalidate(address, 32);
    }

}
