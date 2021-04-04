// System Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

using namespace Debug;

namespace Gekko
{

	void Interpreter::eieio(AnalyzeInfo& info)
	{
		core->regs.pc += 4;
	}

	// instruction synchronize.
	void Interpreter::isync(AnalyzeInfo& info)
	{
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// RESERVE = 1
	// RESERVE_ADDR = physical(ea)
	// rd = MEM(ea, 4)
	void Interpreter::lwarx(AnalyzeInfo& info)
	{
		int WIMG;
		uint32_t ea = core->regs.gpr[info.paramBits[2]];
		if (info.paramBits[1]) ea += core->regs.gpr[info.paramBits[1]];
		RESERVE = true;
		RESERVE_ADDR = core->EffectiveToPhysical(ea, Gekko::MmuAccess::Read, WIMG);
		core->ReadWord(ea, &core->regs.gpr[info.paramBits[0]]);
		if (core->exception) return;
		core->regs.pc += 4;
	}

	// ea = (ra | 0) + rb
	// if RESERVE
	//      then
	//          MEM(ea, 4) = rs
	//          CR0 = 0b00 || 0b1 || XER[SO]
	//          RESERVE = 0
	//      else
	//          CR0 = 0b00 || 0b0 || XER[SO]
	void Interpreter::stwcx_d(AnalyzeInfo& info)
	{
		uint32_t ea = core->regs.gpr[info.paramBits[2]];
		if (info.paramBits[1]) ea += core->regs.gpr[info.paramBits[1]];

		core->regs.cr &= 0x0fffffff;

		if (RESERVE)
		{
			core->WriteWord(ea, core->regs.gpr[info.paramBits[0]]);
			if (core->exception) return;
			SET_CR0_EQ;
			RESERVE = false;
		}

		if (IS_XER_SO) SET_CR0_SO;
		core->regs.pc += 4;
	}

	void Interpreter::sync(AnalyzeInfo& info)
	{
		core->regs.pc += 4;
	}

	// return from exception
	void Interpreter::rfi(AnalyzeInfo& info)
	{
		core->regs.msr &= ~(0x87C0FF73 | 0x00040000);
		core->regs.msr |= core->regs.spr[(int)SPR::SRR1] & 0x87C0FF73;
		core->regs.pc = core->regs.spr[(int)SPR::SRR0] & ~3;
	}

	// syscall
	void Interpreter::sc(AnalyzeInfo& info)
	{
		// pseudo-branch (to resume from next instruction after 'rfi')
		core->regs.pc += 4;
		core->Exception(Gekko::Exception::SYSCALL);
	}

	void Interpreter::tw(AnalyzeInfo& info)
	{
		int32_t a = core->regs.gpr[info.paramBits[1]], b = core->regs.gpr[info.paramBits[2]];
		int32_t to = info.paramBits[0];

		if (((a < b) && (to & 0x10)) ||
			((a > b) && (to & 0x08)) ||
			((a == b) && (to & 0x04)) ||
			(((uint32_t)a < (uint32_t)b) && (to & 0x02)) ||
			(((uint32_t)a > (uint32_t)b) && (to & 0x01)))
		{
			// pseudo-branch (to resume from next instruction after 'rfi')
			core->regs.pc += 4;
			core->PrCause = PrivilegedCause::Trap;
			core->Exception(Gekko::Exception::PROGRAM);
		}
		else
		{
			core->regs.pc += 4;
		}
	}

	void Interpreter::twi(AnalyzeInfo& info)
	{
		int32_t a = core->regs.gpr[info.paramBits[1]], b = (int32_t)info.Imm.Signed;
		int32_t to = info.paramBits[0];

		if (((a < b) && (to & 0x10)) ||
			((a > b) && (to & 0x08)) ||
			((a == b) && (to & 0x04)) ||
			(((uint32_t)a < (uint32_t)b) && (to & 0x02)) ||
			(((uint32_t)a > (uint32_t)b) && (to & 0x01)))
		{
			// pseudo-branch (to resume from next instruction after 'rfi')
			core->regs.pc += 4;
			core->PrCause = PrivilegedCause::Trap;
			core->Exception(Gekko::Exception::PROGRAM);
		}
		else
		{
			core->regs.pc += 4;
		}
	}

	// CR[4 * crfD .. 4 * crfd + 3] = XER[0-3]
	// XER[0..3] = 0b0000
	void Interpreter::mcrxr(AnalyzeInfo& info)
	{
		uint32_t mask = 0xf0000000 >> (4 * info.paramBits[0]);
		core->regs.cr &= ~mask;
		core->regs.cr |= (core->regs.spr[(int)SPR::XER] & 0xf0000000) >> (4 * info.paramBits[0]);
		core->regs.spr[(int)SPR::XER] &= ~0xf0000000;
		core->regs.pc += 4;
	}

	// rd = cr
	void Interpreter::mfcr(AnalyzeInfo& info)
	{
		core->regs.gpr[info.paramBits[0]] = core->regs.cr;
		core->regs.pc += 4;
	}

	// rd = msr
	void Interpreter::mfmsr(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_PR)
		{
			core->PrCause = PrivilegedCause::Privileged;
			core->Exception(Exception::PROGRAM);
			return;
		}

		core->regs.gpr[info.paramBits[0]] = core->regs.msr;
		core->regs.pc += 4;
	}

	// We do not support access rights to SPRs, since all applications on the emulated system are executed with OEA rights.
	// A detailed study of all SPRs in all modes is in Docs\HW\SPR.txt. If necessary, it will be possible to wind the rights properly.

	// rd = spr
	void Interpreter::mfspr(AnalyzeInfo& info)
	{
		int spr = info.paramBits[1];
		uint32_t value;

		switch (spr)
		{
			case (int)SPR::WPAR:
				value = (core->regs.spr[spr] & ~0x1f) | (core->gatherBuffer.NotEmpty() ? 1 : 0);
				break;

			case (int)SPR::HID1:
				// Gekko PLL_CFG = 0b1000
				value = 0x8000'0000;
				break;

			default:
				value = core->regs.spr[spr];
				break;
		}

		core->regs.gpr[info.paramBits[0]] = value;
		core->regs.pc += 4;
	}

	// rd = tbr
	void Interpreter::mftb(AnalyzeInfo& info)
	{
		int tbr = info.paramBits[1];

		if (tbr == 268)
		{
			core->regs.gpr[info.paramBits[0]] = core->regs.tb.Part.l;
		}
		else if (tbr == 269)
		{
			core->regs.gpr[info.paramBits[0]] = core->regs.tb.Part.u;
		}

		core->regs.pc += 4;
	}

	// mask = (4)CRM[0] || (4)CRM[1] || ... || (4)CRM[7]
	// CR = (rs & mask) | (CR & ~mask)
	void Interpreter::mtcrf(AnalyzeInfo& info)
	{
		uint32_t m, crm = info.paramBits[0], a, d = core->regs.gpr[info.paramBits[1]];

		for (int i = 0; i < 8; i++)
		{
			if ((crm >> i) & 1)
			{
				a = (d >> (i << 2)) & 0xf;
				m = (0xf << (i << 2));
				core->regs.cr = (core->regs.cr & ~m) | (a << (i << 2));
			}
		}
		core->regs.pc += 4;
	}

	// msr = rs
	void Interpreter::mtmsr(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_PR)
		{
			core->PrCause = PrivilegedCause::Privileged;
			core->Exception(Exception::PROGRAM);
			return;
		}

		uint32_t oldMsr = core->regs.msr;
		core->regs.msr = core->regs.gpr[info.paramBits[0]];

		if ((oldMsr & MSR_IR) != (core->regs.msr & MSR_IR))
		{
			core->itlb.InvalidateAll();
		}

		if ((oldMsr & MSR_DR) != (core->regs.msr & MSR_DR))
		{
			core->dtlb.InvalidateAll();
		}

		core->regs.pc += 4;
	}

	// spr = rs
	void Interpreter::mtspr(AnalyzeInfo& info)
	{
		int spr = info.paramBits[0];

		if (spr >= 528 && spr <= 543)
		{
			static const char* bat[] = {
				"IBAT0U", "IBAT0L", "IBAT1U", "IBAT1L",
				"IBAT2U", "IBAT2L", "IBAT3U", "IBAT3L",
				"DBAT0U", "DBAT0L", "DBAT1U", "DBAT1L",
				"DBAT2U", "DBAT2L", "DBAT3U", "DBAT3L"
			};

			bool msr_ir = (core->regs.msr & MSR_IR) ? true : false;
			bool msr_dr = (core->regs.msr & MSR_DR) ? true : false;

			Report(Channel::CPU, "%s <- %08X (IR:%i DR:%i pc:%08X)\n",
				bat[spr - 528], core->regs.gpr[info.paramBits[1]], msr_ir, msr_dr, core->regs.pc);
		}
		else switch (spr)
		{
			// decrementer
			case (int)SPR::DEC:
				//DBReport2(DbgChannel::CPU, "set decrementer (OS alarm) to %08X\n", RRS);
				break;

			// page table base
			case (int)SPR::SDR1:
			{
				bool msr_ir = (core->regs.msr & MSR_IR) ? true : false;
				bool msr_dr = (core->regs.msr & MSR_DR) ? true : false;

				Report(Channel::CPU, "SDR <- %08X (IR:%i DR:%i pc:%08X)\n",
					core->regs.gpr[info.paramBits[1]], msr_ir, msr_dr, core->regs.pc);

				core->dtlb.InvalidateAll();
				core->itlb.InvalidateAll();
			}
			break;

			case (int)SPR::TBL:
				core->regs.tb.Part.l = core->regs.gpr[info.paramBits[1]];
				Report(Channel::CPU, "Set TBL: 0x%08X\n", core->regs.tb.Part.l);
				break;
			case (int)SPR::TBU:
				core->regs.tb.Part.u = core->regs.gpr[info.paramBits[1]];
				Report(Channel::CPU, "Set TBU: 0x%08X\n", core->regs.tb.Part.u);
				break;

			// write gathering buffer
			case (int)SPR::WPAR:
				// A mtspr to WPAR invalidates the data.
				core->gatherBuffer.Reset();
				break;

			case (int)SPR::HID0:
			{
				uint32_t bits = core->regs.gpr[info.paramBits[1]];
				core->cache.Enable((bits & HID0_DCE) ? true : false);
				core->cache.Freeze((bits & HID0_DLOCK) ? true : false);
				if (bits & HID0_DCFI)
				{
					bits &= ~HID0_DCFI;

					// On a real system, after a global cache invalidation, the data still remains in the L2 cache.
					// We cannot afford global invalidation, as the L2 cache is not supported.

					Report(Channel::CPU, "Data Cache Flash Invalidate\n");
				}
				if (bits & HID0_ICFI)
				{
					bits &= ~HID0_ICFI;
					core->jitc->Reset();

					Report(Channel::CPU, "Instruction Cache Flash Invalidate\n");
				}

				core->regs.spr[spr] = bits;
				core->regs.pc += 4;
				return;
			}
			break;

			case (int)SPR::HID1:
				// Read only
				core->regs.pc += 4;
				return;

			case (int)SPR::HID2:
			{
				uint32_t bits = core->regs.gpr[info.paramBits[1]];
				core->cache.LockedEnable((bits & HID2_LCE) ? true : false);
			}
			break;

			// Locked cache DMA

			case (int)SPR::DMAU:
				//DBReport2(DbgChannel::CPU, "DMAU: 0x%08X\n", RRS);
				break;
			case (int)SPR::DMAL:
			{
				core->regs.spr[spr] = core->regs.gpr[info.paramBits[1]];
				//DBReport2(DbgChannel::CPU, "DMAL: 0x%08X\n", RRS);
				if (core->regs.spr[(int)SPR::DMAL] & GEKKO_DMAL_DMA_T)
				{
					uint32_t maddr = core->regs.spr[(int)SPR::DMAU] & GEKKO_DMAU_MEM_ADDR;
					uint32_t lcaddr = core->regs.spr[(int)SPR::DMAL] & GEKKO_DMAL_LC_ADDR;
					size_t length = ((core->regs.spr[(int)SPR::DMAU] & GEKKO_DMAU_DMA_LEN_U) << GEKKO_DMA_LEN_SHIFT) |
						((core->regs.spr[(int)SPR::DMAL] >> GEKKO_DMA_LEN_SHIFT) & GEKKO_DMAL_DMA_LEN_L);
					if (length == 0) length = 128;
					if (core->cache.IsLockedEnable())
					{
						core->cache.LockedCacheDma(
							(core->regs.spr[(int)SPR::DMAL] & GEKKO_DMAL_DMA_LD) ? true : false,
							maddr,
							lcaddr,
							length);
					}
				}

				// It makes no sense to implement such a small Queue. We make all transactions instant.

				core->regs.spr[spr] &= ~(GEKKO_DMAL_DMA_T | GEKKO_DMAL_DMA_F);
				core->regs.pc += 4;
				return;
			}
			break;

			// When the GQR values change, the recompiler is invalidated.
			// This rarely happens.

			case (int)SPR::GQR0:
			case (int)SPR::GQR1:
			case (int)SPR::GQR2:
			case (int)SPR::GQR3:
			case (int)SPR::GQR4:
			case (int)SPR::GQR5:
			case (int)SPR::GQR6:
			case (int)SPR::GQR7:
			{
				if (core->regs.spr[spr] != core->regs.gpr[info.paramBits[1]])
				{
					core->jitc->Reset();
				}
			}
			break;

			case (int)SPR::IBAT0U:
			case (int)SPR::IBAT0L:
			case (int)SPR::IBAT1U:
			case (int)SPR::IBAT1L:
			case (int)SPR::IBAT2U:
			case (int)SPR::IBAT2L:
			case (int)SPR::IBAT3U:
			case (int)SPR::IBAT3L:
				core->itlb.InvalidateAll();
				break;

			case (int)SPR::DBAT0U:
			case (int)SPR::DBAT0L:
			case (int)SPR::DBAT1U:
			case (int)SPR::DBAT1L:
			case (int)SPR::DBAT2U:
			case (int)SPR::DBAT2L:
			case (int)SPR::DBAT3U:
			case (int)SPR::DBAT3L:
				core->dtlb.InvalidateAll();
				break;
		}

		// default
		core->regs.spr[spr] = core->regs.gpr[info.paramBits[1]];
		core->regs.pc += 4;
	}

	void Interpreter::dcbf(AnalyzeInfo& info)
	{
		int WIMG;
		uint32_t ea = info.paramBits[0] ? core->regs.gpr[info.paramBits[0]] + core->regs.gpr[info.paramBits[1]] : core->regs.gpr[info.paramBits[1]];

		uint32_t pa = core->EffectiveToPhysical(ea, MmuAccess::Read, WIMG);
		if (pa != Gekko::BadAddress)
		{
			core->cache.Flush(pa);
		}
		else
		{
			core->regs.spr[(int)Gekko::SPR::DAR] = ea;
			core->Exception(Exception::DSI);
			return;
		}
		core->regs.pc += 4;
	}

	void Interpreter::dcbi(AnalyzeInfo& info)
	{
		int WIMG;
		uint32_t ea = info.paramBits[0] ? core->regs.gpr[info.paramBits[0]] + core->regs.gpr[info.paramBits[1]] : core->regs.gpr[info.paramBits[1]];

		if (core->regs.msr & MSR_PR)
		{
			core->PrCause = PrivilegedCause::Privileged;
			core->Exception(Exception::PROGRAM);
			return;
		}

		uint32_t pa = core->EffectiveToPhysical(ea, MmuAccess::Write, WIMG);
		if (pa != Gekko::BadAddress)
		{
			core->cache.Invalidate(pa);
		}
		else
		{
			core->regs.spr[(int)Gekko::SPR::DAR] = ea;
			core->Exception(Exception::DSI);
			return;
		}
		core->regs.pc += 4;
	}

	void Interpreter::dcbst(AnalyzeInfo& info)
	{
		int WIMG;
		uint32_t ea = info.paramBits[0] ? core->regs.gpr[info.paramBits[0]] + core->regs.gpr[info.paramBits[1]] : core->regs.gpr[info.paramBits[1]];

		uint32_t pa = core->EffectiveToPhysical(ea, MmuAccess::Read, WIMG);
		if (pa != Gekko::BadAddress)
		{
			core->cache.Store(pa);
		}
		else
		{
			core->regs.spr[(int)Gekko::SPR::DAR] = ea;
			core->Exception(Exception::DSI);
			return;
		}
		core->regs.pc += 4;
	}

	void Interpreter::dcbt(AnalyzeInfo& info)
	{
		int WIMG;

		if (core->regs.spr[(int)Gekko::SPR::HID0] & HID0_NOOPTI)
			return;

		uint32_t ea = info.paramBits[0] ? core->regs.gpr[info.paramBits[0]] + core->regs.gpr[info.paramBits[1]] : core->regs.gpr[info.paramBits[1]];

		uint32_t pa = core->EffectiveToPhysical(ea, MmuAccess::Read, WIMG);
		if (pa != Gekko::BadAddress)
		{
			core->cache.Touch(pa);
		}
		core->regs.pc += 4;
	}

	void Interpreter::dcbtst(AnalyzeInfo& info)
	{
		int WIMG;

		if (core->regs.spr[(int)Gekko::SPR::HID0] & HID0_NOOPTI)
			return;

		uint32_t ea = info.paramBits[0] ? core->regs.gpr[info.paramBits[0]] + core->regs.gpr[info.paramBits[1]] : core->regs.gpr[info.paramBits[1]];

		// TouchForStore is also made architecturally as a Read operation so that the MMU does not set the "Changed" bit for PTE.

		uint32_t pa = core->EffectiveToPhysical(ea, MmuAccess::Read, WIMG);
		if (pa != Gekko::BadAddress)
		{
			core->cache.TouchForStore(pa);
		}
		core->regs.pc += 4;
	}

	void Interpreter::dcbz(AnalyzeInfo& info)
	{
		int WIMG;
		uint32_t ea = info.paramBits[0] ? core->regs.gpr[info.paramBits[0]] + core->regs.gpr[info.paramBits[1]] : core->regs.gpr[info.paramBits[1]];

		uint32_t pa = core->EffectiveToPhysical(ea, MmuAccess::Write, WIMG);
		if (pa != Gekko::BadAddress)
		{
			core->cache.Zero(pa);
		}
		else
		{
			core->regs.spr[(int)Gekko::SPR::DAR] = ea;
			core->Exception(Exception::DSI);
			return;
		}
		core->regs.pc += 4;
	}

	// DCBZ_L is used for the alien Locked Cache address mapping mechanism.
	// For example, calling dcbz_l 0xE0000000 will make this address be associated with Locked Cache for subsequent Load/Store operations.
	// Locked Cache is saved in RAM by another alien mechanism (DMA).

	void Interpreter::dcbz_l(AnalyzeInfo& info)
	{
		int WIMG;

		if (!core->cache.IsLockedEnable())
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::PROGRAM);
			return;
		}

		uint32_t ea = info.paramBits[0] ? core->regs.gpr[info.paramBits[0]] + core->regs.gpr[info.paramBits[1]] : core->regs.gpr[info.paramBits[1]];

		uint32_t pa = core->EffectiveToPhysical(ea, MmuAccess::Write, WIMG);
		if (pa != Gekko::BadAddress)
		{
			core->cache.ZeroLocked(pa);
		}
		else
		{
			core->regs.spr[(int)Gekko::SPR::DAR] = ea;
			core->Exception(Exception::DSI);
			return;
		}
		core->regs.pc += 4;
	}

	// Used as a hint to JITC so that it can invalidate the compiled code at this address.

	void Interpreter::icbi(AnalyzeInfo& info)
	{
		uint32_t address = info.paramBits[0] ? core->regs.gpr[info.paramBits[0]] + core->regs.gpr[info.paramBits[1]] : core->regs.gpr[info.paramBits[0]];
		address &= ~0x1f;

		core->jitc->Invalidate(address, 32);
		core->regs.pc += 4;
	}

	// rd = sr[a]
	void Interpreter::mfsr(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_PR)
		{
			core->PrCause = PrivilegedCause::Privileged;
			core->Exception(Exception::PROGRAM);
			return;
		}

		core->regs.gpr[info.paramBits[0]] = core->regs.sr[info.paramBits[1]];
		core->regs.pc += 4;
	}

	// rd = sr[rb]
	void Interpreter::mfsrin(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_PR)
		{
			core->PrCause = PrivilegedCause::Privileged;
			core->Exception(Exception::PROGRAM);
			return;
		}

		core->regs.gpr[info.paramBits[0]] = core->regs.sr[core->regs.gpr[info.paramBits[1]] >> 28];
		core->regs.pc += 4;
	}

	// sr[a] = rs
	void Interpreter::mtsr(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_PR)
		{
			core->PrCause = PrivilegedCause::Privileged;
			core->Exception(Exception::PROGRAM);
			return;
		}

		core->regs.sr[info.paramBits[0]] = core->regs.gpr[info.paramBits[1]];
		core->regs.pc += 4;
	}

	// sr[rb] = rs
	void Interpreter::mtsrin(AnalyzeInfo& info)
	{
		if (core->regs.msr & MSR_PR)
		{
			core->PrCause = PrivilegedCause::Privileged;
			core->Exception(Exception::PROGRAM);
			return;
		}

		core->regs.sr[core->regs.gpr[info.paramBits[1]] >> 28] = core->regs.gpr[info.paramBits[0]];
		core->regs.pc += 4;
	}

	void Interpreter::tlbie(AnalyzeInfo& info)
	{
		core->dtlb.Invalidate(core->regs.gpr[info.paramBits[0]]);
		core->itlb.Invalidate(core->regs.gpr[info.paramBits[0]]);
		core->regs.pc += 4;
	}

	void Interpreter::tlbsync(AnalyzeInfo& info)
	{
		core->regs.pc += 4;
	}

	void Interpreter::eciwx(AnalyzeInfo& info)
	{
		Halt("eciwx\n");
	}

	void Interpreter::ecowx(AnalyzeInfo& info)
	{
		Halt("ecowx\n");
	}

}
