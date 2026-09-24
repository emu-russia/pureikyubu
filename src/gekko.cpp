// CPU controls 
#include "pch.h"

using namespace Debug;

namespace Gekko
{
	CpuStats stats;
	bool cycleProfile = false;

	// The main driving force behind the entire emulator. All other threads are based on changing the TBR Gekko register.
	void GekkoCore::GekkoThreadProc(void* Parameter)
	{
		GekkoCore* core = (GekkoCore*)Parameter;

		if (core->suspended)
		{
			Thread::Sleep(50);
			return;
		}

		if (core->EnableTestBreakpoints)
		{
			if (core->TestBreakpoints()) {
				return;
			}
		}

		// Breakpoints and single stepping stay on the interpreter, so that the
		// debugger sees every instruction.
		if (core->JitEnabled && core->jit != nullptr && core->jit->IsSupported() && !core->EnableTestBreakpoints)
		{
			core->jit->Run();
			return;
		}

		core->interp->ExecuteOpcode();
	}

	GekkoCore::GekkoCore()
	{
		cache = new Cache(this);
		icache = new Cache(this, true);
		
		// DEBUG
		//cache->SetLogLevel(CacheLogLevel::MemOps);
		//icache->SetLogLevel(CacheLogLevel::MemOps);

		gatherBuffer = new GatherBuffer(this);

		interp = new Interpreter(this);

		jit = new Jit(this);

		gekkoThread = EMUCreateThread(GekkoThreadProc, false, this, "GekkoCore");

		Reset();
	}

	GekkoCore::~GekkoCore()
	{
		StopOpcodeStatsThread();
		EMUJoinThread(gekkoThread);
		delete jit;
		delete interp;
		delete gatherBuffer;
	}

	// Reset processor
	void GekkoCore::Reset()
	{
		one_second = CPU_TIMER_CLOCK;
		intFlag = false;
		exception = false;
		decreq = false;
		RESERVE = false;
		ops = 0;

		// BAT registers are scattered across the SPR address space. This is not very convenient, we will make it convenient.

		dbatu[0] = &regs.spr[SPR::DBAT0U];
		dbatu[1] = &regs.spr[SPR::DBAT1U];
		dbatu[2] = &regs.spr[SPR::DBAT2U];
		dbatu[3] = &regs.spr[SPR::DBAT3U];

		dbatl[0] = &regs.spr[SPR::DBAT0L];
		dbatl[1] = &regs.spr[SPR::DBAT1L];
		dbatl[2] = &regs.spr[SPR::DBAT2L];
		dbatl[3] = &regs.spr[SPR::DBAT3L];

		ibatu[0] = &regs.spr[SPR::IBAT0U];
		ibatu[1] = &regs.spr[SPR::IBAT1U];
		ibatu[2] = &regs.spr[SPR::IBAT2U];
		ibatu[3] = &regs.spr[SPR::IBAT3U];

		ibatl[0] = &regs.spr[SPR::IBAT0L];
		ibatl[1] = &regs.spr[SPR::IBAT1L];
		ibatl[2] = &regs.spr[SPR::IBAT2L];
		ibatl[3] = &regs.spr[SPR::IBAT3L];

		// Registers

		memset(&regs, 0, sizeof(regs));

		// Disable translation for now
		regs.msr &= ~(MSR_DR | MSR_IR);

		regs.tb.uval = 0;
		flipperDeadline = 0;
		regs.spr[SPR::HID1] = 0x8000'0000;
		// The decrementer produces an exception on underflow, so it starts out negative:
		// the program has to reload it with a positive value to arm the first exception.
		regs.spr[SPR::DEC] = 0xffff'ffff;
		regs.spr[SPR::CTR] = 0;

		gatherBuffer->Reset();

		if (jit != nullptr)
		{
			jit->InvalidateAll();
		}

		dtlb.InvalidateAll();
		itlb.InvalidateAll();
		cache->Reset();
		cache->Enable(false);
		icache->Reset();
		icache->Enable(false);

		UpdateHtabRange();
	}

	// The hashed page table window (SDR1[HTABORG] and the size HTABMASK describes). The
	// translation cache has to be dropped when the guest stores inside it: see FlushTlbOnPteWrite.
	void GekkoCore::UpdateHtabRange()
	{
		uint32_t sdr1 = regs.spr[(int)SPR::SDR1];

		htabOrg = sdr1 & 0xffff0000;

		uint64_t end = (uint64_t)htabOrg + ((uint64_t)(sdr1 & 0x1ff) + 1) * 0x10000;
		htabEnd = (end > 0xffffffffULL) ? 0xffffffffu : (uint32_t)end;
	}

	// Modify CPU counters
	// (GekkoCore::Tick is defined inline in gekko.h - it is on the hot path.)

	int64_t GekkoCore::GetTicks()
	{
		return regs.tb.sval;
	}

	// 1 second of emulated CPU time.
	int64_t GekkoCore::OneSecond()
	{
		return one_second;
	}

	// Swap longs (no need in assembly, used by tools)
	void GekkoCore::SwapArea(uint32_t* addr, int count)
	{
		uint32_t* until = addr + count / sizeof(uint32_t);

		while (addr != until)
		{
			*addr = _BYTESWAP_UINT32(*addr);
			addr++;
		}
	}

	// Swap shorts (no need in assembly, used by tools)
	void GekkoCore::SwapAreaHalf(uint16_t* addr, int count)
	{
		uint16_t* until = addr + count / sizeof(uint16_t);

		while (addr != until)
		{
			*addr = _BYTESWAP_UINT16(*addr);
			addr++;
		}
	}

	void GekkoCore::Step()
	{
		interp->ExecuteOpcode();
	}

	// The Flipper-side periodic work is due: let the ASIC run it and arm the next deadline. The
	// hook lives here rather than in the header because the inline Tick/TickN cannot see Flipper.
	void GekkoCore::SyncFlipper()
	{
		flipperDeadline = regs.tb.sval + Flipper::FlipperTickStep;

		if (Flipper::HW != nullptr)
		{
			Flipper::HW->Update(regs.tb.sval);
		}
	}

	void GekkoCore::AssertInterrupt()
	{
		intFlag = true;
	}

	void GekkoCore::ClearInterrupt()
	{
		intFlag = false;
	}

	void GekkoCore::Exception(Gekko::Exception code)
	{
		if (break_on_exception)
		{
			Halt("Gekko Exception: #%04X\n", (uint16_t)code);
		}

		if (trace_exceptions)
		{
			Report(Channel::CPU, "Gekko Exception: #%04X\n", (uint16_t)code);
		}

		if (exception)
		{
			Halt("CPU Double Fault!\n");
		}

		// save regs

		regs.spr[Gekko::SPR::SRR0] = regs.pc;
		regs.spr[Gekko::SPR::SRR1] = regs.msr;

		// A page-fault handler rewrites the hashed page table (the VM library's
		// swap-in evicts one page and maps the faulting one), so any cached page
		// translation may now be stale. Real hardware walks the page table on every
		// access and has no translation cache, so the VM library does not issue tlbie
		// after those writes; flush the emulator's translation cache here instead.
		if (code == Exception::EXCEPTION_ISI || code == Exception::EXCEPTION_DSI)
		{
			dtlb.InvalidateAll();
			itlb.InvalidateAll();
		}

		// Special processing for MMU
		if (code == Exception::EXCEPTION_ISI)
		{
			regs.spr[Gekko::SPR::SRR1] &= 0x0fff'ffff;

			switch (MmuLastResult)
			{
				case MmuResult::PageFault:
					regs.spr[Gekko::SPR::SRR1] |= 0x4000'0000;
					break;

				case MmuResult::ProtectedFetch:
					regs.spr[Gekko::SPR::SRR1] |= 0x0800'0000;
					break;

				case MmuResult::NoExecute:
					regs.spr[Gekko::SPR::SRR1] |= 0x1000'0000;
					break;

				case MmuResult::DirectStore:
					regs.spr[Gekko::SPR::SRR1] |= 0x1000'0000;
					break;

				default:
					break;
			}

			if (break_on_ISI) {
				Halt("Gekko ISI Exception at %08X\n", regs.pc);
			}
		}
		else if (code == Exception::EXCEPTION_DSI)
		{
			regs.spr[Gekko::SPR::DSISR] = 0;

			switch (MmuLastResult)
			{
				case MmuResult::PageFault:
					regs.spr[Gekko::SPR::DSISR] |= 0x4000'0000;
					break;

				case MmuResult::ProtectedRead:
					regs.spr[Gekko::SPR::DSISR] |= 0x0800'0000;
					break;

				case MmuResult::ProtectedWrite:
					regs.spr[Gekko::SPR::DSISR] |= 0x0A00'0000;
					break;

				case MmuResult::DirectStore:
					regs.spr[Gekko::SPR::DSISR] |= 0x0400'0000;
					break;

				default:
					break;
			}

			if (break_on_DSI) {
				Halt("Gekko DSI Exception at %08X.%s\n", regs.pc, regs.spr[Gekko::SPR::SDR1] != 0 ? 
					"\nThe sneaky game uses the vm library, to emulate virtual memory, which is not yet well implemented." : "");
			}
		}

		// Special processing for Program
		else if (code == Exception::EXCEPTION_PROGRAM)
		{
			regs.spr[Gekko::SPR::SRR1] &= 0x0000'ffff;

			switch (PrCause)
			{
				case PrivilegedCause::FpuEnabled:
					regs.spr[Gekko::SPR::SRR1] |= 0x0010'0000;
					break;
				case PrivilegedCause::IllegalInstruction:
					regs.spr[Gekko::SPR::SRR1] |= 0x0008'0000;
					break;
				case PrivilegedCause::Privileged:
					regs.spr[Gekko::SPR::SRR1] |= 0x0004'0000;
					break;
				case PrivilegedCause::Trap:
					regs.spr[Gekko::SPR::SRR1] |= 0x0002'0000;
					break;
				default:
					break;
			}
		}

		// Disable address translation.
		//
		// This used to drop every compiled block, and it was the single largest source
		// of re-translation in a running game (an exception every 600 us in Metroid
		// Prime's audio driver, each one discarding ~16K blocks). It does not have to:
		// what a block bakes in is the *instruction stream* it was compiled from, and
		// the lookup compares the physical address the entry was compiled for with the
		// one the current MSR/BAT/TLB state translates the pc to, so a block compiled
		// under a different MSR[IR]/[DR] is simply not found when the translation
		// differs - and when it does not differ, the instructions are the same and the
		// block is still valid. Data accesses go through the helpers and translate per
		// access, exactly as the interpreter does.
		stats.invException++;
		regs.msr &= ~(MSR_IR | MSR_DR);

		regs.msr &= ~MSR_RI;

		regs.msr &= ~MSR_EE;

		// change PC and set exception flag
		regs.pc = (uint32_t)code;
		exception = true;
	}
}


// Gekko Memory interface

namespace Gekko
{

	// Centralized hub which attracts all memory access requests from the interpreter or recompiler 
	// (as well as those who they pretend, for example HLE or Debugger).

	void GekkoCore::ReadByte(uint32_t addr, uint32_t* reg)
	{
		int WIMG;
		if (EnableTestReadBreakpoints)
		{
			TestReadBreakpoints(addr);
		}

		uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Read, WIMG);
		if (pa == BadAddress)
		{
			regs.spr[(int)SPR::DAR] = addr;
			Exception(Exception::EXCEPTION_DSI);
			return;
		}

		if (cache->IsEnabled() && (WIMG & WIMG_I) == 0)
		{
			cache->ReadByte(pa, reg);
			return;
		}

		Flipper::HW->pi->PIReadByte(pa, reg);
	}

	void GekkoCore::WriteByte(uint32_t addr, uint32_t data)
	{
		int WIMG;
		if (EnableTestWriteBreakpoints)
		{
			TestWriteBreakpoints(addr);
		}

		uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Write, WIMG);
		if (pa == BadAddress)
		{
			regs.spr[(int)SPR::DAR] = addr;
			Exception(Exception::EXCEPTION_DSI);
			return;
		}

		FlushTlbOnPteWrite(pa);

		if (RESERVE && pa == RESERVE_ADDR)
		{
			RESERVE = false;
		}

		if (regs.spr[Gekko::SPR::HID2] & HID2_WPE)
		{
			if ((pa & ~0x1f) == (regs.spr[Gekko::SPR::WPAR] & ~0x1f))
			{
				gatherBuffer->Write8((uint8_t)data);
				return;
			}
		}

		if (cache->IsEnabled() && (WIMG & WIMG_I) == 0)
		{
			cache->WriteByte(pa, data);
			if ((WIMG & WIMG_W) == 0)
				return;
		}

		Flipper::HW->pi->PIWriteByte(pa, data);
	}

	void GekkoCore::ReadHalf(uint32_t addr, uint32_t* reg)
	{
		int WIMG;
		if (EnableTestReadBreakpoints)
		{
			TestReadBreakpoints(addr);
		}

		uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Read, WIMG);
		if (pa == BadAddress)
		{
			regs.spr[(int)SPR::DAR] = addr;
			Exception(Exception::EXCEPTION_DSI);
			return;
		}

		if (cache->IsEnabled() && (WIMG & WIMG_I) == 0)
		{
			cache->ReadHalf(pa, reg);
			return;
		}

		Flipper::HW->pi->PIReadHalf(pa, reg);
	}

	void GekkoCore::WriteHalf(uint32_t addr, uint32_t data)
	{
		int WIMG;
		if (EnableTestWriteBreakpoints)
		{
			TestWriteBreakpoints(addr);
		}

		uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Write, WIMG);
		if (pa == BadAddress)
		{
			regs.spr[(int)SPR::DAR] = addr;
			Exception(Exception::EXCEPTION_DSI);
			return;
		}

		FlushTlbOnPteWrite(pa);

		if (RESERVE && pa == RESERVE_ADDR)
		{
			RESERVE = false;
		}

		if (regs.spr[Gekko::SPR::HID2] & HID2_WPE)
		{
			if ((pa & ~0x1f) == (regs.spr[Gekko::SPR::WPAR] & ~0x1f))
			{
				gatherBuffer->Write16((uint16_t)data);
				return;
			}
		}

		if (cache->IsEnabled() && (WIMG & WIMG_I) == 0)
		{
			cache->WriteHalf(pa, data);
			if ((WIMG & WIMG_W) == 0)
				return;
		}

		Flipper::HW->pi->PIWriteHalf(pa, data);
	}

	void GekkoCore::ReadWord(uint32_t addr, uint32_t* reg)
	{
		int WIMG;
		if (EnableTestReadBreakpoints)
		{
			TestReadBreakpoints(addr);
		}

		uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Read, WIMG);
		if (pa == BadAddress)
		{
			regs.spr[(int)SPR::DAR] = addr;
			Exception(Exception::EXCEPTION_DSI);
			return;
		}

		if (cache->IsEnabled() && (WIMG & WIMG_I) == 0)
		{
			cache->ReadWord(pa, reg);
			return;
		}

		Flipper::HW->pi->PIReadWord(pa, reg);
	}

	void GekkoCore::WriteWord(uint32_t addr, uint32_t data)
	{
		int WIMG;
		if (EnableTestWriteBreakpoints)
		{
			TestWriteBreakpoints(addr);
		}

		uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Write, WIMG);
		if (pa == BadAddress)
		{
			regs.spr[(int)SPR::DAR] = addr;
			Exception(Exception::EXCEPTION_DSI);
			return;
		}

		FlushTlbOnPteWrite(pa);

		if (RESERVE && pa == RESERVE_ADDR)
		{
			RESERVE = false;
		}

		if (regs.spr[Gekko::SPR::HID2] & HID2_WPE)
		{
			if ((pa & ~0x1f) == (regs.spr[Gekko::SPR::WPAR] & ~0x1f))
			{
				gatherBuffer->Write32(data);
				return;
			}
		}

		if (cache->IsEnabled() && (WIMG & WIMG_I) == 0)
		{
			cache->WriteWord(pa, data);
			if ((WIMG & WIMG_W) == 0)
				return;
		}

		Flipper::HW->pi->PIWriteWord(pa, data);
	}

	void GekkoCore::ReadDouble(uint32_t addr, uint64_t* reg)
	{
		int WIMG;
		if (EnableTestReadBreakpoints)
		{
			TestReadBreakpoints(addr);
		}

		uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Read, WIMG);
		if (pa == BadAddress)
		{
			regs.spr[(int)SPR::DAR] = addr;
			Exception(Exception::EXCEPTION_DSI);
			return;
		}

		if (cache->IsEnabled() && (WIMG & WIMG_I) == 0)
		{
			cache->ReadDouble(pa, reg);
			return;
		}

		// It is suspected that this type of single-beat transaction is not supported by Flipper PI.

		Flipper::HW->pi->PIReadDouble(pa, reg);
	}

	void GekkoCore::WriteDouble(uint32_t addr, uint64_t* data)
	{
		int WIMG;
		if (EnableTestWriteBreakpoints)
		{
			TestWriteBreakpoints(addr);
		}

		uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Write, WIMG);
		if (pa == BadAddress)
		{
			regs.spr[(int)SPR::DAR] = addr;
			Exception(Exception::EXCEPTION_DSI);
			return;
		}

		FlushTlbOnPteWrite(pa);

		if (RESERVE && pa == RESERVE_ADDR)
		{
			RESERVE = false;
		}

		if (regs.spr[Gekko::SPR::HID2] & HID2_WPE)
		{
			if ((pa & ~0x1f) == (regs.spr[Gekko::SPR::WPAR] & ~0x1f))
			{
				gatherBuffer->Write64(*data);
				return;
			}
		}

		if (cache->IsEnabled() && (WIMG & WIMG_I) == 0)
		{
			cache->WriteDouble(pa, data);
			if ((WIMG & WIMG_W) == 0)
				return;
		}

		// It is suspected that this type of single-beat transaction is not supported by Flipper PI.

		Flipper::HW->pi->PIWriteDouble(pa, data);
	}

	void GekkoCore::Fetch(uint32_t addr, uint32_t* reg)
	{
		int WIMG;

		uint32_t pa = EffectiveToPhysical(addr, MmuAccess::Execute, WIMG);
		if (pa == BadAddress)
		{
			Exception(Exception::EXCEPTION_ISI);
			return;
		}

		// HACK: You don't need to use the ICache in BS1, even if it is enabled
		if (pa >= PI_MEMSPACE_BOOTROM) {
			Flipper::HW->pi->PIReadWord(pa, reg);
			return;
		}

		if (icache->IsEnabled() && (WIMG & WIMG_I) == 0)
		{
			icache->ReadWord(pa, reg);
			return;
		}

		Flipper::HW->pi->PIReadWord(pa, reg);
	}

	uint8_t* GekkoCore::GetDataCachePointer(uint32_t phys_addr)
	{
		return cache->GetCachePointer(phys_addr);
	}

}


namespace Gekko
{
	void TLB::Dump()
	{
		for (size_t n = 0; n < Size; n++)
		{
			TLBEntry* entry = &entries[n];

			if (entry->eaTag == EmptyTag)
			{
				continue;
			}

			uint32_t ea = entry->eaTag << 12;
			Report(Channel::CPU, "EA 0x%08X -> PA 0x%08X (wimg: %d, pc: 0x%08X)\n", ea, entry->addressTag << 12, entry->WIMG, entry->pc);
		}
	}
}



// Support for breakpoints.

// After switching the Gekko emulation as a separate thread, the implementation of breakpoints is trivial. 
// When a breakpoint occurs, we just do Suspend of the Gekko thread. 
// And since all the other subsystems are tied to the Gekko thread (except for the UI and Debugger, it has an independent thread)
// the whole system stops with the processor.

namespace Gekko
{
	void GekkoCore::AddBreakpoint(uint32_t addr)
	{
		breakPointsLock.Lock();
		bool exists = false;
		for (auto it = breakPointsExecute.begin(); it != breakPointsExecute.end(); ++it)
		{
			if (*it == addr)
			{
				exists = true;
				break;
			}
		}
		if (!exists)
		{
			Report(Channel::CPU, "Breakpoint added: 0x%08X\n", addr);
			breakPointsExecute.push_back(addr);
			EnableTestBreakpoints = true;
		}
		breakPointsLock.Unlock();
	}

	void GekkoCore::RemoveBreakpoint(uint32_t addr)
	{
		breakPointsLock.Lock();
		bool exists = false;
		for (auto it = breakPointsExecute.begin(); it != breakPointsExecute.end(); ++it)
		{
			if (*it == addr)
			{
				exists = true;
				break;
			}
		}
		if (exists)
		{
			Report(Channel::CPU, "Breakpoint removed: 0x%08X\n", addr);
			breakPointsExecute.remove(addr);
		}
		if (breakPointsExecute.size() == 0)
		{
			EnableTestBreakpoints = false;
		}
		breakPointsLock.Unlock();
	}

	void GekkoCore::AddReadBreak(uint32_t addr)
	{
		breakPointsLock.Lock();
		breakPointsRead.push_back(addr);
		breakPointsLock.Unlock();
		EnableTestReadBreakpoints = true;
	}

	void GekkoCore::AddWriteBreak(uint32_t addr)
	{
		breakPointsLock.Lock();
		breakPointsWrite.push_back(addr);
		breakPointsLock.Unlock();
		EnableTestWriteBreakpoints = true;
	}

	void GekkoCore::ClearBreakpoints()
	{
		breakPointsLock.Lock();
		breakPointsExecute.clear();
		breakPointsRead.clear();
		breakPointsWrite.clear();
		breakPointsLock.Unlock();
		EnableTestBreakpoints = false;
		EnableTestReadBreakpoints = false;
		EnableTestWriteBreakpoints = false;
	}

	bool GekkoCore::TestBreakpoints()
	{
		if (oneShotBreakpoint != BadAddress && regs.pc == oneShotBreakpoint)
		{
			Halt("One shot breakpoint at addr: 0x%08X\n", oneShotBreakpoint);
			oneShotBreakpoint = BadAddress;
			return true;
		}

		uint32_t addr = BadAddress;

		breakPointsLock.Lock();
		for (auto it = breakPointsExecute.begin(); it != breakPointsExecute.end(); ++it)
		{
			if (*it == regs.pc)
			{
				addr = *it;
				break;
			}
		}
		breakPointsLock.Unlock();

		if (addr != BadAddress)
		{
			Halt("Gekko suspended at addr: 0x%08X\n", addr);
			return true;
		}

		return false;
	}

	void GekkoCore::TestReadBreakpoints(uint32_t accessAddress)
	{
		uint32_t addr = BadAddress;

		breakPointsLock.Lock();
		for (auto it = breakPointsRead.begin(); it != breakPointsRead.end(); ++it)
		{
			if (*it == accessAddress)
			{
				addr = *it;
				break;
			}
		}
		breakPointsLock.Unlock();

		if (addr != BadAddress)
		{
			Halt("Gekko suspended trying to read: 0x%08X\n", addr);
		}
	}

	void GekkoCore::TestWriteBreakpoints(uint32_t accessAddress)
	{
		uint32_t addr = BadAddress;

		breakPointsLock.Lock();
		for (auto it = breakPointsWrite.begin(); it != breakPointsWrite.end(); ++it)
		{
			if (*it == accessAddress)
			{
				addr = *it;
				break;
			}
		}
		breakPointsLock.Unlock();

		if (addr != BadAddress)
		{
			Halt("Gekko suspended trying to write: 0x%08X\n", addr);
		}
	}

	void GekkoCore::AddOneShotBreakpoint(uint32_t addr)
	{
		oneShotBreakpoint = addr;
		EnableTestBreakpoints = true;
	}

	void GekkoCore::ToggleBreakpoint(uint32_t addr)
	{
		if (IsBreakpoint(addr))
			RemoveBreakpoint(addr);
		else
			AddBreakpoint(addr);
	}

	bool GekkoCore::IsBreakpoint(uint32_t addr)
	{
		bool exists = false;
		breakPointsLock.Lock();
		for (auto it = breakPointsExecute.begin(); it != breakPointsExecute.end(); ++it)
		{
			if (*it == addr)
			{
				exists = true;
				break;
			}
		}
		breakPointsLock.Unlock();
		return exists;
	}
}


// Gekko caches support (including Data locked cache)

// As defined by the PowerPC architecture, they are physically indexed.

// Emulation of access to the cache is simple - a copy is created for the main memory, same size as the main RAM.
// If cached access is performed, all reads and writes are made from this buffer, otherwise from RAM.
// Invalidation causes new data to be loaded from RAM into the cache buffer.

// We do not support scattering for a locked cache and assume that it is locked as a fixed chunk.


namespace Gekko
{
	Cache::Cache(GekkoCore* core, bool instruction)
	{
		this->core = core;
		this->instruction = instruction;

		cacheData = new uint8_t[cacheSize];

		modifiedBlocks = new bool[cacheSize >> 5];

		invalidBlocks = new bool[cacheSize >> 5];

		LockedCache = new uint8_t[LockedCacheSize];

		Reset();
	}

	Cache::~Cache()
	{
		delete[] cacheData;
		delete[] modifiedBlocks;
		delete[] invalidBlocks;
		delete[] LockedCache;
	}

	void Cache::Reset()
	{
		Report(Channel::CPU, "Cache::Reset\n");
		FlashInvalidate();
	}

	void Cache::Enable(bool enable)
	{
		enabled = enable;

		if (log >= CacheLogLevel::Commands)
		{
			Report(Channel::CPU, "Cache::Enable %i\n", enable ? 1 : 0);
		}
	}

	void Cache::Freeze(bool freeze)
	{
		frozen = freeze;

		if (log >= CacheLogLevel::Commands)
		{
			Report(Channel::CPU, "Cache::Freeze %i\n", freeze ? 1 : 0);
		}
	}

	void Cache::LockedEnable(bool enable)
	{
		lcenabled = enable;

		if (log >= CacheLogLevel::Commands)
		{
			Report(Channel::CPU, "Cache::LockedEnable %i\n", enable ? 1 : 0);
		}
	}

	bool Cache::IsDirty(uint32_t pa)
	{
		size_t blockNum = pa >> 5;
		return modifiedBlocks[blockNum];
	}

	void Cache::SetDirty(uint32_t pa, bool dirty)
	{
		size_t blockNum = pa >> 5;

		if (dirty == modifiedBlocks[blockNum])
			return;

		modifiedBlocks[blockNum] = dirty;

		if (log >= CacheLogLevel::MemOps && dirty)
		{
			Report(Channel::CPU, "Cache::SetDirty. pa: 0x%08X\n", pa & ~0x1f);
		}
	}

	bool Cache::IsInvalid(uint32_t pa)
	{
		size_t blockNum = pa >> 5;
		return invalidBlocks[blockNum];
	}

	void Cache::SetInvalid(uint32_t pa, bool invalid)
	{
		size_t blockNum = pa >> 5;

		if (invalid == invalidBlocks[blockNum])
			return;

		invalidBlocks[blockNum] = invalid;

		if (log >= CacheLogLevel::MemOps && invalid)
		{
			Report(Channel::CPU, "Cache::SetInvalid. pa: 0x%08X\n", pa & ~0x1f);
		}
	}

	void Cache::Flush(uint32_t pa)
	{
		if (pa >= cacheSize)
			return;

		if (IsDirty(pa) && !IsInvalid(pa))
		{
			CastOut(pa);
			SetDirty(pa, false);
		}
		SetInvalid(pa, true);

		if (log >= CacheLogLevel::Commands)
		{
			Report(Channel::CPU, "Cache::Flush 0x%08X, pc: 0x%08X\n", pa, core->regs.pc);
		}
	}

	void Cache::Invalidate(uint32_t pa)
	{
		if (pa >= cacheSize)
			return;

		SetInvalid(pa, true);

		if (log >= CacheLogLevel::Commands)
		{
			Report(Channel::CPU, "Cache::Invalidate 0x%08X, pc: 0x%08X\n", pa, core->regs.pc);
		}
	}

	void Cache::FlashInvalidate()
	{
		// A flash invalidate also drops every compiled block: it is used both for the
		// data cache (the 0x81300000 hack in ExecuteOpcode, which exists precisely
		// because IPL2 is DMA'ed into memory behind the CPU's back) and for the
		// instruction cache (HID0[ICFI]).
		stats.invFlash++;
		if (core->jit != nullptr)
		{
			core->jit->InvalidateAll();
		}

		size_t blocks_num = cacheSize >> 5;
		for (size_t n = 0; n < blocks_num; n++) {
			modifiedBlocks[n] = false;
			invalidBlocks[n] = true;
		}
	}

	// Write the cache back and throw it away: after this the whole of the data cache is invalid
	// (so every line is refilled from main memory on its next use) and main memory holds every
	// value the emulated cache ever held.
	//
	// This is what makes it possible for a save state to leave the 24 MB of cache data out. The
	// argument is the dirty bit: a line that is *dirty* is the only place a value written by the
	// guest can live, because a store marks its line dirty (WriteByte and the other write entries
	// end in SetDirty, and Zero does the same), so casting the dirty lines out copies every one of
	// those values back to memory. A line that is *clean* and valid is by construction a copy of
	// what main memory already holds: it was either cast in from memory (Touch/TouchForStore/
	// ReadWord), which copies memory and clears the dirty bit, or written through to memory
	// (GekkoCore::WriteWord/WriteByte/... check WIMG[W] and hand the store to the PI as well as to
	// the cache), which leaves both copies equal. Zero() is the one path that makes a line dirty
	// without reading it, and it is therefore covered by the cast-out rather than lost. Dropping a
	// clean line loses nothing, and the state does not have to carry tens of megabytes of data
	// that a guest could not tell apart from what MEM already holds.
	//
	// The flush runs through Flush() rather than CastOut() so that it cannot resurrect a block
	// that is dirty and invalid at the same time: only dirty *valid* lines are written, exactly
	// like the per-line cache management instructions do it.
	//
	// One honest caveat about a *frozen* cache (HID0[DLOCK]): CastOut refuses to write while
	// `frozen` is set, so a truly dirty line of a frozen data cache is not written back here. The
	// emulator treats a frozen cache as storage the bus cannot reach, which is what freeze means
	// (CastIn does not fill it either), and the loaded state restores the same frozen flag, so the
	// line is not lost - it simply stays where it was, and it is the emulator's own model of DLOCK
	// rather than this flush that is approximate.
	void Cache::FlushAll()
	{
		size_t blocks = cacheSize >> 5;

		for (size_t block = 0; block < blocks; block++)
		{
			uint32_t pa = (uint32_t)(block << 5);

			if (IsDirty(pa) && !IsInvalid(pa))
			{
				Flush(pa);
			}
		}

		// FlashInvalidate also drops the compiled blocks, which is right here: a recompiled block
		// was translated against the address translation and the instruction bytes the guest had
		// at the time, and after a load state both of those may be different (the JIT is
		// invalidated again by GekkoCore::LoadState, but the save side must not leave it pointing
		// at blocks it compiled for a machine that no longer exists).
		FlashInvalidate();
	}

	void Cache::Store(uint32_t pa)
	{
		if (pa >= cacheSize)
			return;

		if (IsDirty(pa) && !IsInvalid(pa))
		{
			CastOut(pa);
			SetDirty(pa, false);
		}

		if (log >= CacheLogLevel::Commands)
		{
			Report(Channel::CPU, "Cache::Store 0x%08X\n", pa);
		}
	}

	void Cache::Touch(uint32_t pa)
	{
		if (pa >= cacheSize)
			return;

		if (!IsDirty(pa))
		{
			CastIn(pa);
			SetInvalid(pa, false);
			SetDirty(pa, false);			// Valid & Not Dirty
		}

		if (log >= CacheLogLevel::Commands)
		{
			Report(Channel::CPU, "Cache::Touch 0x%08X\n", pa);
		}
	}

	void Cache::TouchForStore(uint32_t pa)
	{
		if (pa >= cacheSize)
			return;

		if (!IsDirty(pa))
		{
			CastIn(pa);
			SetInvalid(pa, false);
			SetDirty(pa, true);				// Valid & Dirty
		}

		if (log >= CacheLogLevel::Commands)
		{
			Report(Channel::CPU, "Cache::TouchForStore 0x%08X\n", pa);
		}
	}

	void Cache::Zero(uint32_t pa)
	{
		if (pa >= cacheSize)
			return;

		memset(&cacheData[pa & ~0x1f], 0, 32);
		SetDirty(pa, true);
		SetInvalid(pa, false);

		if (log >= CacheLogLevel::Commands)
		{
			Report(Channel::CPU, "Cache::Zero 0x%08X\n", pa);
		}
	}

	void Cache::ZeroLocked(uint32_t pa)
	{
		if (log >= CacheLogLevel::Commands)
		{
			Report(Channel::CPU, "Cache::ZeroLocked 0x%08X\n", pa);
		}

		LockedCacheAddr = pa & ~0x3FFF;
	}

	// The documentation says that the cache is casted by single-beat transactions, but for speed we will do casting by burts.

	void Cache::CastIn(uint32_t pa)
	{
		assert(pa < cacheSize);

		if (frozen)
			return;

		CycleScope cycles(&stats.castInCycles);

		if (instruction)
			stats.icacheFills++;
		else
			stats.dcacheFills++;

		if (log >= CacheLogLevel::MemOps)
		{
			Report(Channel::CPU, "Cache::CastIn: 0x%08X\n", pa & ~0x1f);
		}

		Flipper::HW->pi->PIReadBurst(pa & ~0x1f, &cacheData[pa & ~0x1f]);
	}

	void Cache::CastOut(uint32_t pa)
	{
		assert(pa < cacheSize);

		if (frozen)
			return;

		if (log >= CacheLogLevel::MemOps)
		{
			Report(Channel::CPU, "Cache::CastOut: 0x%08X\n", pa & ~0x1f);
		}

		Flipper::HW->pi->PIWriteBurst(pa & ~0x1f, &cacheData[pa & ~0x1f]);
	}

	void Cache::ReadByte(uint32_t addr, uint32_t* reg)
	{
		// Locked cache
		if (IsLockedEnable())
		{
			if ((addr & ~0x3fff) == LockedCacheAddr)
			{
				uint8_t* ptr = &LockedCache[addr & 0x3fff];
				*reg = (uint32_t)*ptr;
				return;
			}
		}

		if (addr >= cacheSize)
			return;

		if (IsInvalid(addr))
		{
			CastIn(addr);
			SetInvalid(addr, false);
			SetDirty(addr, false);
		}
		*reg = cacheData[addr];

		if (log >= CacheLogLevel::MemOps)
		{
			Report(Channel::CPU, "Cache::ReadByte. addr: 0x%08X, *reg: 0x%08X\n", addr, *reg);
		}
	}

	void Cache::WriteByte(uint32_t addr, uint32_t data)
	{
		// Locked cache
		if (IsLockedEnable())
		{
			if ((addr & ~0x3fff) == LockedCacheAddr)
			{
				uint8_t* ptr = &LockedCache[addr & 0x3fff];
				*ptr = (uint8_t)data;
				return;
			}
		}

		if (addr >= cacheSize)
			return;

		if (IsInvalid(addr))
		{
			CastIn(addr);
			SetInvalid(addr, false);
		}
		cacheData[addr] = (uint8_t)data;

		if (log >= CacheLogLevel::MemOps)
		{
			Report(Channel::CPU, "Cache::WriteByte. addr: 0x%08X, data: 0x%08X\n", addr, data);
		}

		SetDirty(addr, true);
	}

	void Cache::ReadHalf(uint32_t addr, uint32_t* reg)
	{
		// Locked cache
		if (IsLockedEnable())
		{
			if ((addr & ~0x3fff) == LockedCacheAddr)
			{
				uint8_t* ptr = &LockedCache[addr & 0x3fff];
				*reg = (uint32_t)_BYTESWAP_UINT16(*(uint16_t*)ptr);
				return;
			}
		}

		if (addr >= cacheSize)
			return;

		if (IsInvalid(addr))
		{
			CastIn(addr);
			SetInvalid(addr, false);
			SetDirty(addr, false);
		}

		if ((addr & 0x1f) > (32 - sizeof(uint16_t)))
		{
			if (complain_unaligned) {
				Report(Channel::CPU, "Cache::ReadHalf: Unaligned cache access addr:0x%08X!\n", addr);
			}

			uint32_t nextCacheLineAddr = addr + sizeof(uint16_t);

			if (IsInvalid(nextCacheLineAddr))
			{
				CastIn(nextCacheLineAddr);
				SetInvalid(nextCacheLineAddr, false);
				SetDirty(nextCacheLineAddr, false);
			}
		}

		*reg = _BYTESWAP_UINT16(*(uint16_t*)&cacheData[addr]);

		if (log >= CacheLogLevel::MemOps)
		{
			Report(Channel::CPU, "Cache::ReadHalf. addr: 0x%08X, *reg: 0x%08X\n", addr, *reg);
		}
	}

	void Cache::WriteHalf(uint32_t addr, uint32_t data)
	{
		// Locked cache
		if (IsLockedEnable())
		{
			if ((addr & ~0x3fff) == LockedCacheAddr)
			{
				uint8_t* ptr = &LockedCache[addr & 0x3fff];
				*(uint16_t*)ptr = _BYTESWAP_UINT16((uint16_t)data);
				return;
			}
		}

		if (addr >= cacheSize)
			return;

		if (IsInvalid(addr))
		{
			CastIn(addr);
			SetInvalid(addr, false);
		}

		if ((addr & 0x1f) > (32 - sizeof(uint16_t)))
		{
			if (complain_unaligned) {
				Report(Channel::CPU, "Cache::WriteHalf: Unaligned cache access addr:0x%08X!\n", addr);
			}

			uint32_t nextCacheLineAddr = addr + sizeof(uint16_t);

			if (IsInvalid(nextCacheLineAddr))
			{
				CastIn(nextCacheLineAddr);
				SetInvalid(nextCacheLineAddr, false);
				SetDirty(nextCacheLineAddr, false);
			}

			SetDirty(nextCacheLineAddr, true);
		}

		*(uint16_t*)&cacheData[addr] = _BYTESWAP_UINT16((uint16_t)data);

		if (log >= CacheLogLevel::MemOps)
		{
			Report(Channel::CPU, "Cache::WriteHalf. addr: 0x%08X, data: 0x%08X\n", addr, data);
		}

		SetDirty(addr, true);
	}

	void Cache::ReadWord(uint32_t addr, uint32_t* reg)
	{
		// Locked cache
		if (IsLockedEnable())
		{
			if ((addr & ~0x3fff) == LockedCacheAddr)
			{
				uint8_t* ptr = &LockedCache[addr & 0x3fff];
				*reg = _BYTESWAP_UINT32(*(uint32_t*)ptr);
				return;
			}
		}

		if (addr >= cacheSize)
			return;

		if (IsInvalid(addr))
		{
			CastIn(addr);
			SetInvalid(addr, false);
			SetDirty(addr, false);
		}

		if ((addr & 0x1f) > (32 - sizeof(uint32_t)))
		{
			if (complain_unaligned) {
				Report(Channel::CPU, "Cache::ReadWord: Unaligned cache access addr:0x%08X!\n", addr);
			}

			uint32_t nextCacheLineAddr = addr + sizeof(uint32_t);

			if (IsInvalid(nextCacheLineAddr))
			{
				CastIn(nextCacheLineAddr);
				SetInvalid(nextCacheLineAddr, false);
				SetDirty(nextCacheLineAddr, false);
			}
		}

		*reg = _BYTESWAP_UINT32(*(uint32_t*)&cacheData[addr]);

		if (log >= CacheLogLevel::MemOps)
		{
			Report(Channel::CPU, "Cache::ReadWord. addr: 0x%08X, *reg: 0x%08X\n", addr, *reg);
		}
	}

	void Cache::WriteWord(uint32_t addr, uint32_t data)
	{
		// Locked cache
		if (IsLockedEnable())
		{
			if ((addr & ~0x3fff) == LockedCacheAddr)
			{
				uint8_t* ptr = &LockedCache[addr & 0x3fff];
				*(uint32_t*)ptr = _BYTESWAP_UINT32(data);
				return;
			}
		}

		if (addr >= cacheSize)
			return;

		if (IsInvalid(addr))
		{
			CastIn(addr);
			SetInvalid(addr, false);
		}

		if ((addr & 0x1f) > (32 - sizeof(uint32_t)))
		{
			if (complain_unaligned) {
				Report(Channel::CPU, "Cache::WriteWord: Unaligned cache access addr:0x%08X!\n", addr);
			}

			uint32_t nextCacheLineAddr = addr + sizeof(uint32_t);

			if (IsInvalid(nextCacheLineAddr))
			{
				CastIn(nextCacheLineAddr);
				SetInvalid(nextCacheLineAddr, false);
				SetDirty(nextCacheLineAddr, false);
			}

			SetDirty(nextCacheLineAddr, true);
		}

		*(uint32_t*)&cacheData[addr] = _BYTESWAP_UINT32(data);

		if (log >= CacheLogLevel::MemOps)
		{
			Report(Channel::CPU, "Cache::WriteWord. addr: 0x%08X, data: 0x%08X\n", addr, data);
		}

		SetDirty(addr, true);
	}

	void Cache::ReadDouble(uint32_t addr, uint64_t* reg)
	{
		// Locked cache
		if (IsLockedEnable())
		{
			if ((addr & ~0x3fff) == LockedCacheAddr)
			{
				uint8_t* buf = &LockedCache[addr & 0x3fff];
				*reg = _BYTESWAP_UINT64(*(uint64_t*)buf);
				return;
			}
		}

		if (addr >= cacheSize)
			return;

		if (IsInvalid(addr))
		{
			CastIn(addr);
			SetInvalid(addr, false);
			SetDirty(addr, false);
		}

		if ((addr & 0x1f) > (32 - sizeof(uint64_t)))
		{
			if (complain_unaligned) {
				Report(Channel::CPU, "Cache::ReadDouble: Unaligned cache access addr:0x%08X!\n", addr);
			}

			uint32_t nextCacheLineAddr = addr + sizeof(uint64_t);

			if (IsInvalid(nextCacheLineAddr))
			{
				CastIn(nextCacheLineAddr);
				SetInvalid(nextCacheLineAddr, false);
				SetDirty(nextCacheLineAddr, false);
			}
		}

		*reg = _BYTESWAP_UINT64(*(uint64_t*)&cacheData[addr]);

		if (log >= CacheLogLevel::MemOps)
		{
			Report(Channel::CPU, "Cache::ReadDouble. addr: 0x%08X, *reg: 0x%llX\n", addr, *reg);
		}
	}

	void Cache::WriteDouble(uint32_t addr, uint64_t* data)
	{
		// Locked cache
		if (IsLockedEnable())
		{
			if ((addr & ~0x3fff) == LockedCacheAddr)
			{
				uint8_t* buf = &LockedCache[addr & 0x3fff];
				*(uint64_t*)buf = _BYTESWAP_UINT64(*data);
				return;
			}
		}

		if (addr >= cacheSize)
			return;

		if (IsInvalid(addr))
		{
			CastIn(addr);
			SetInvalid(addr, false);
		}

		if ((addr & 0x1f) > (32 - sizeof(uint64_t)))
		{
			if (complain_unaligned) {
				Report(Channel::CPU, "Cache::WriteDouble: Unaligned cache access addr:0x%08X!\n", addr);
			}

			uint32_t nextCacheLineAddr = addr + sizeof(uint64_t);

			if (IsInvalid(nextCacheLineAddr))
			{
				CastIn(nextCacheLineAddr);
				SetInvalid(nextCacheLineAddr, false);
				SetDirty(nextCacheLineAddr, false);
			}

			SetDirty(nextCacheLineAddr, true);
		}

		*(uint64_t*)&cacheData[addr] = _BYTESWAP_UINT64(*data);

		if (log >= CacheLogLevel::MemOps)
		{
			Report(Channel::CPU, "Cache::WriteDouble. addr: 0x%08X, data: 0x%llX\n", addr, data);
		}

		SetDirty(addr, true);
	}

	void Cache::LockedCacheDma(bool MemToCache, uint32_t memaddr, uint32_t lcaddr, size_t bursts)
	{
		if (MemToCache)
		{   // 1 load - transfer from external memory to locked cache

			if (log >= CacheLogLevel::MemOps)
			{
				Report(Channel::CPU, "Load Locked Cache: memadr: 0x%08X, lcaddr: 0x%08X, bursts: %i\n", memaddr, lcaddr, bursts);
			}

			for (size_t i = 0; i < bursts; i++)
			{
				Flipper::HW->pi->PIReadBurst(memaddr, &LockedCache[lcaddr & 0x3fff]);
				memaddr += 32;
				lcaddr += 32;
			}
		}
		else
		{   // 0 store -  transfer from locked cache to external memory 

			if (log >= CacheLogLevel::MemOps)
			{
				Report(Channel::CPU, "Store Locked Cache: memadr: 0x%08X, lcaddr: 0x%08X, bursts: %i\n", memaddr, lcaddr, bursts);
			}

			for (size_t i = 0; i < bursts; i++)
			{
				Flipper::HW->pi->PIWriteBurst(memaddr, &LockedCache[lcaddr & 0x3fff]);
				memaddr += 32;
				lcaddr += 32;
			}
		}
	}
}


namespace Gekko
{
	void GatherBuffer::Reset()
	{

		memset(fifo, 0, sizeof(fifo));
		readPtr = 0;
		writePtr = 0;

		if (log)
		{
			Report(Channel::CPU, "GatherBuffer::Reset");
		}
	}

	size_t GatherBuffer::GatherSize()
	{
		if (writePtr >= readPtr)
		{
			return writePtr - readPtr;
		}
		else
		{
			return (sizeof(fifo) - readPtr) + writePtr;
		}
	}

	void GatherBuffer::WriteBytes(uint8_t* data, size_t size)
	{
		// The Write Gather Buffer is one of the profiled channels (issue #394): what the CPU
		// pushes into it is the display list traffic on its way to the CP.
		Debug::HwProfile::Count(Debug::HwProfile::Counter::WriteGather, size);

		if (log)
		{
			char byteText[10];
			std::string text;

			for (int i = 0; i < size; i++)
			{
				sprintf(byteText, "%02X ", data[i]);
				text += byteText;
			}

			Report(Channel::CPU, "GatherBuffer::WriteBytes: %s", text.c_str());
		}

		if (size < 4)
		{
			for (int i = 0; i < size; i++)
			{
				fifo[writePtr] = data[i];
				writePtr++;
				if (writePtr >= sizeof(fifo))
				{
					writePtr = 0;
				}
			}
		}
		else
		{
			if ((writePtr + size) < sizeof(fifo))
			{
				memcpy(&fifo[writePtr], data, size);
				writePtr += size;
			}
			else
			{
				size_t part1Size = sizeof(fifo) - writePtr;
				memcpy(&fifo[writePtr], data, part1Size);
				writePtr = size - part1Size;
				memcpy(fifo, data + part1Size, writePtr);
			}
		}

		if (GatherSize() >= 32)
		{
			uint8_t burstData[32];

			for (int i = 0; i < sizeof(burstData); i++)
			{
				burstData[i] = fifo[readPtr];
				readPtr++;
				if (readPtr >= sizeof(fifo))
				{
					readPtr = 0;
				}
			}

			if (log)
			{
				Report(Channel::CPU, "Burst gather buffer. Bytes left: %zi\n", GatherSize());
			}

			Flipper::HW->pi->PIWriteBurst(core->regs.spr[(int)SPR::WPAR] & ~0x1f, burstData);
		}
	}

	void GatherBuffer::Write8(uint8_t value)
	{
		WriteBytes(&value, 1);
	}

	void GatherBuffer::Write16(uint16_t value)
	{
		uint8_t data[2];
		*(uint16_t*)data = _BYTESWAP_UINT16(value);
		WriteBytes(data, 2);
	}

	void GatherBuffer::Write32(uint32_t value)
	{
		uint8_t data[4];
		*(uint32_t*)data = _BYTESWAP_UINT32(value);
		WriteBytes(data, 4);
	}

	void GatherBuffer::Write64(uint64_t value)
	{
		uint8_t data[8];
		*(uint64_t*)data = _BYTESWAP_UINT64(value);
		WriteBytes(data, 8);
	}

	// WPAR[BNE]: "buffer not empty". This is a plain status bit - reading it must not disturb the
	// pipe. A partial 32-byte block really does wait: the hardware has no timeout, and the only
	// ways to get the bytes out are to accumulate a whole block, to fill the block with dummy
	// data, or to reprogram WPAR (which invalidates whatever is still buffered). Games that poll
	// this bit while data is pending are waiting for an already-requested burst to finish on the
	// bus, so clearing the buffer here would silently drop command bytes.
	// WPAR[BNE]: "buffer not empty" (manual 9.4.1). It is a plain status bit - reading it must not
	// disturb the pipe. A block that is not full yet really does wait in the buffer: the pipe has
	// no timeout, and nothing is transferred before a whole block is gathered or WPAR is
	// re-programmed (which discards it). Software that must deliver a partial block has to pad it.
	bool GatherBuffer::NotEmpty()
	{
		// EXPERIMENT (NFS Carbon black screen): report the pipe as "not empty" only while at
		// least half of the buffer is occupied, instead of on any pending byte. The SDK's
		// GXFlush waits for this bit to clear and its command stream leaves a partial 32 byte
		// block behind, so with the hardware-accurate "any byte" answer the wait never ends.
		return GatherSize() >= (sizeof(fifo) / 2);
	}

	// The write gather buffer is machine state, not a host buffer: WPAR[BNE] is a register the
	// guest polls, and the bytes the CPU has already pushed but not yet delivered to the CP are
	// the difference between a command stream that continues after the load and one that starts
	// with half a command. The FIFO is written whole (the holes in it are bytes of a block that
	// has not been completed yet, and the guest can still make them part of a delivered block by
	// padding), followed by the two cursors. The `log` flag stays out: it is a host reporting
	// option, like the cache's own log level.
	void GatherBuffer::SaveState(SaveStates::StateWriter& writer)
	{
		writer.Raw(fifo, sizeof(fifo));
		writer.U32((uint32_t)readPtr);
		writer.U32((uint32_t)writePtr);
	}

	void GatherBuffer::LoadState(SaveStates::StateReader& reader)
	{
		reader.Raw(fifo, sizeof(fifo));

		uint32_t read = reader.U32();
		uint32_t write = reader.U32();

		if (reader.Failed())
		{
			return;
		}

		// Both cursors index the FIFO, so a value that is not inside it cannot have come from this
		// machine. The failure is latched rather than clamped: the caller refuses the whole state,
		// and a wrong cursor never becomes a machine.
		if (read >= sizeof(fifo) || write >= sizeof(fifo))
		{
			reader.Fail("the write gather buffer cursors are outside the FIFO");
			return;
		}

		readPtr = read;
		writePtr = write;
	}
}


// MMU never throws Gekko exceptions. If something went wrong, BadAddress is returned. Then the consumer decides what to do.

// The result of the MMU is placed in MmuLastResult and translated address as output.

// We also use BadAddress as a signal that the translation did not pass (a special address that is usually not used by anyone).

namespace Gekko
{
	// Native address translation defined by PowerPC architecture. There are some alien moments (Hash for Page Tables), but overall its fine.

	uint32_t GekkoCore::EffectiveToPhysicalMmu(uint32_t ea, MmuAccess type, int& WIMG)
	{
		uint32_t pa;

		WIMG = 0;

		// The translation cache has already been probed by EffectiveToPhysical().

		// First, try the block translation, if it doesn’t work, try the Page Table.

		if (!BlockAddressTranslation(ea, pa, type, WIMG))
		{
			pa = SegmentTranslation(ea, type, WIMG);
		}

		return pa;
	}

	bool GekkoCore::BlockAddressTranslation(uint32_t ea, uint32_t& pa, MmuAccess type, int& WIMG)
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
					// Put in TLB
#if GEKKOCORE_USE_TLB
					itlb.Map(ea, pa, Core->regs.pc, WIMG);
#endif
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
					// Put in TLB
#if GEKKOCORE_USE_TLB
					dtlb.Map(ea, pa, Core->regs.pc, WIMG);
#endif
					return true;
				}
			}
		}

		// No BAT match, continue page table translation

		return false;
	}

	// The hash table walk (unlike DMA traffic) is coherent with the data cache.

	void GekkoCore::ReadHashPte(uint32_t pa, uint32_t* reg)
	{
		if (cache->IsEnabled())
		{
			cache->ReadWord(pa, reg);
		}
		else
		{
			Flipper::HW->pi->PIReadWord(pa, reg);
		}
	}

	void GekkoCore::WriteHashPte(uint32_t pa, uint32_t data)
	{
		if (cache->IsEnabled())
		{
			cache->WriteWord(pa, data);
		}
		else
		{
			Flipper::HW->pi->PIWriteWord(pa, data);
		}
	}

	uint32_t GekkoCore::SegmentTranslation(uint32_t ea, MmuAccess type, int& WIMG)
	{
		int ptegUpper;

		uint32_t sr = regs.sr[ea >> 28];

		// Direct Store (T=1) is used by default by Dolphin OS to throw a DSI/ISI exception
		if (sr & 0x8000'0000)
		{
			MmuLastResult = MmuResult::DirectStore;
			return BadAddress;
		}

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

			ReadHashPte(primaryPteAddr, &pte[0]);
			ReadHashPte(primaryPteAddr + 4, &pte[1]);

			// Check Hash Bit

			if ((pte[0] & 0x40) != 0)
			{
				primaryPteAddr += 8;
				continue;
			}

			// Valid and suitable? (PTE [VSID, API, V] = Seg Desc [VSID], EA[API], 1)

			if (pte[0] & 0x8000'0000 &&
				((pte[0] >> 7) & 0xffffff) == ((vpn >> 16) & 0xffffff) &&
				(pte[0] & 0x3f) == ((vpn >> 10) & 0x3f))
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
				WriteHashPte(primaryPteAddr + 4, pte[1]);

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
				// Put in TLB
#if GEKKOCORE_USE_TLB
				if (type == MmuAccess::Execute) {
					itlb.Map(ea, pa, Core->regs.pc, WIMG);
				}
				else {
					dtlb.Map(ea, pa, Core->regs.pc, WIMG);
				}
#endif
				MmuLastResult = MmuResult::Ok;
				return pa;
			}
			else
			{
				// Referenced
				pte[1] |= 0x100;
				WriteHashPte(primaryPteAddr + 4, pte[1]);
			}

			primaryPteAddr += 8;
		}

		// Try Secondary PTEGs

		for (int i = 0; i < 8; i++)
		{
			// Load PTE

			uint32_t pte[2];

			ReadHashPte(secondaryPteAddr, &pte[0]);
			ReadHashPte(secondaryPteAddr + 4, &pte[1]);

			// Check Hash Bit

			if ((pte[0] & 0x40) == 0)
			{
				secondaryPteAddr += 8;
				continue;
			}

			// Valid and suitable? (PTE [VSID, API, V] = Seg Desc [VSID], EA[API], 1)

			if (pte[0] & 0x8000'0000 &&
				((pte[0] >> 7) & 0xffffff) == ((vpn >> 16) & 0xffffff) &&
				(pte[0] & 0x3f) == ((vpn >> 10) & 0x3f))
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
				WriteHashPte(secondaryPteAddr + 4, pte[1]);

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
				// Put in TLB
#if GEKKOCORE_USE_TLB
				if (type == MmuAccess::Execute) {
					itlb.Map(ea, pa, Core->regs.pc, WIMG);
				}
				else {
					dtlb.Map(ea, pa, Core->regs.pc, WIMG);
				}
#endif
				MmuLastResult = MmuResult::Ok;
				return pa;
			}
			else
			{
				// Referenced
				pte[1] |= 0x100;
				WriteHashPte(secondaryPteAddr + 4, pte[1]);
			}

			secondaryPteAddr += 8;
		}

		MmuLastResult = MmuResult::PageFault;
		return BadAddress;
	}

	void GekkoCore::DumpDTLB()
	{
		dtlb.Dump();
	}

	void GekkoCore::DumpITLB()
	{
		itlb.Dump();
	}

	void GekkoCore::InvalidateTLBAll()
	{
		dtlb.InvalidateAll();
		itlb.InvalidateAll();
	}

}


// This module deals with the maintenance of statistics on the use of opcodes.

namespace Gekko
{
	struct OpcodeSortedEntry
	{
		Instruction instr;
		int count;
	};

	bool GekkoCore::IsOpcodeStatsEnabled()
	{
		return opcodeStatsEnabled;
	}

	void GekkoCore::EnableOpcodeStats(bool enable)
	{
		opcodeStatsEnabled = enable;
	}

	static int OpcodeStatsCompare(const void* a, const void* b)
	{
		OpcodeSortedEntry* as = (OpcodeSortedEntry*)a;
		OpcodeSortedEntry* bs = (OpcodeSortedEntry*)b;
		return bs->count - as->count;
	}

	void GekkoCore::PrintOpcodeStats(size_t maxCount)
	{
		OpcodeSortedEntry unsorted[(size_t)Instruction::Max];

		// Sort statistics

		for (size_t i = 0; i < (size_t)Instruction::Max; i++)
		{
			unsorted[i].instr = (Instruction)i;
			unsorted[i].count = opcodeStats[i];
		}

		qsort(unsorted, (size_t)Instruction::Max, sizeof(OpcodeSortedEntry), OpcodeStatsCompare);

		// Print out

		if (maxCount > (size_t)Instruction::Max)
		{
			maxCount = (size_t)Instruction::Max;
		}

		for (size_t i = 0; i < maxCount; i++)
		{
			DecoderInfo info;

			info.instr = unsorted[i].instr;
			Report(Channel::CPU, "%s: %i\n", GekkoDisasm::InstrToString(&info).c_str(), unsorted[i].count);
		}

		Report(Channel::CPU, "  \n");
	}

	void GekkoCore::ResetOpcodeStats()
	{
		memset(opcodeStats, 0, sizeof(opcodeStats));
	}

	// Thread that displays statistics on the use of opcodes once per second.

	void GekkoCore::OpcodeStatsThreadProc(void* Parameter)
	{
		GekkoCore* core = (GekkoCore*)Parameter;

		if (core->opcodeStatsEnabled && core->IsRunning())
		{
			core->PrintOpcodeStats(10);
			core->ResetOpcodeStats();
		}

		Thread::Sleep(1000);
	}

	void GekkoCore::RunOpcodeStatsThread()
	{
		if (opcodeStatsThread == nullptr)
		{
			opcodeStatsThread = EMUCreateThread(OpcodeStatsThreadProc, false, this, "OpcodeStats");
			EnableOpcodeStats(true);
		}
	}

	void GekkoCore::StopOpcodeStatsThread()
	{
		if (opcodeStatsThread != nullptr)
		{
			EMUJoinThread(opcodeStatsThread);
			opcodeStatsThread = nullptr;
			EnableOpcodeStats(false);
		}
	}

}


// Save states: the processor half (the "CPU " section).
//
// The caller opens the section, calls this pair, and closes it; the section tag and the file
// around it belong to the top-level orchestrator (see savestate.h / savestate.cpp). Both
// functions write and read the same fields in the same order at the same widths - there is one
// fixed layout and no versioning inside it, so a state written by a build whose layout differs is
// refused by the section's own length check when the reader closes it.
//
// What is in the section, and why each part of it is:
//
//   * the architectural register file - the 32 GPRs, the 64 bits of each FPR (the register is
//     physical storage and the paired-single halves are *views* of it, not separate registers, so
//     one u64 per register carries everything), the 1024 SPRs, the 16 segment registers, CR, MSR,
//     FPSCR, PC and the 64-bit time base;
//   * the three latched requests (decreq, intFlag, exception) - they are sticky between
//     instructions, so a decrementer underflow or an external interrupt line that arrived while
//     MSR[EE] was clear would be lost if it were recomputed from the registers;
//   * the lwarx/stwcx reservation (RESERVE and the address it was taken on) - a state taken in the
//     middle of a locked sequence has to keep it, or the stwcx that follows would clear a
//     reservation the guest still owns;
//   * PrCause, the reason the last PROGRAM exception was taken, for the same reason: the handler
//     has not read SRR1 yet when the state is taken;
//   * the write gather buffer - guest-visible through WPAR[BNE], and a partial 32-byte block can
//     be waiting in it;
//   * the locked L1 data cache and the address of its window - it is scratch-pad storage that is
//     deliberately NOT coherent with main memory, so unlike the ordinary caches it cannot be
//     reconstructed by flushing and refilling.
//
// What is deliberately not in the section:
//
//   * the 24 MB of ordinary data cache data and the 24 MB of instruction cache data: they are
//     copies of main memory and are written back and dropped by SaveState before anything is
//     stored (see Cache::FlushAll, which carries the argument);
//   * the `modifiedBlocks` / `invalidBlocks` arrays for the same reason - they describe those
//     copies, and after the flush every block is clean and invalid;
//   * the translation caches dtlb/itlb: they are a cache of the page-table walk, they refill on a
//     miss, and even their `changed` bit is recoverable by taking the walk again (see
//     EffectiveToPhysical, which is what makes a store to a page whose PTE Changed bit has not
//     been set through this entry take the walk);
//   * htabOrg/htabEnd (a pure function of SDR1), the BAT pointers dbatu/dbatl/ibatu/ibatl (they
//     point into regs.spr), and the cache enable flags (a pure function of HID0 and HID2): all of
//     them are derived on load instead of stored;
//   * MmuLastResult: it is read immediately after a translation, by the exception entry of the
//     instruction that caused it, and a load state is not taken between those two points;
//   * the JIT (interp/jit/gekkoThread are host objects; the compiled blocks are thrown away and
//     retranslated);
//   * everything the debugger and the profiler own - the breakpoint lists, breakPointsLock,
//     oneShotBreakpoint, the break_on_* switches, the trace_* switches, EnableTest*, opcodeStats
//     and `ops`;
//   * the host's own switches (suspended, resetInstructionCounter, JitEnabled,
//     trace_locked_dma_regs), the constants and statistics (one_second, the CpuStats counters, the
//     cycle profiling flag, the cache log levels), and the scratch registers used by instructions.

namespace Gekko
{
	namespace
	{
		// The cursors read an enum as the integer of its width, so a loaded value has to be checked
		// against the enumeration it belongs to before it becomes machine state.
		bool IsPrivilegedCause(uint32_t value)
		{
			switch ((PrivilegedCause)value)
			{
			case PrivilegedCause::None:
			case PrivilegedCause::FpuEnabled:
			case PrivilegedCause::IllegalInstruction:
			case PrivilegedCause::Privileged:
			case PrivilegedCause::Trap:
				return true;
			default:
				return false;
			}
		}

		// A floating-point register is physical 64-bit storage: fr1 (the paired-single second
		// half) is a *view* of it, not a register of its own, so one u64 per register carries all
		// of fpr/ps0/ps1. FPREG is a union with no value mapping, so the cursors cannot take it as
		// a field - a struct or a union is never written as a block of bytes - and the 64 bits go
		// out as the .uval member. These two loops are the only places the save state has to touch
		// the register file: everything else in it is a plain integer array.
		void WriteFprs(SaveStates::StateWriter& writer, const FPREG(&fpr)[32])
		{
			for (int i = 0; i < 32; i++)
			{
				writer.Fields(fpr[i].uval);
			}
		}

		void ReadFprs(SaveStates::StateReader& reader, FPREG(&fpr)[32])
		{
			for (int i = 0; i < 32; i++)
			{
				reader.Fields(fpr[i].uval);
			}
		}
	}

	void GekkoCore::SaveState(SaveStates::StateWriter& writer)
	{
		// Main memory has to become the whole truth before a single field is written: the data
		// cache holds values the guest stored that main memory does not have yet, and the MEM
		// section of this state is written from main memory. The flush writes those lines back
		// through the PI (a real bus write, not a memcpy) and then drops the whole cache, so
		// nothing that lived only in the cache can be lost.
		//
		// The instruction cache is flushed the same way. It should never hold a dirty line,
		// because the emulator only ever writes into the *data* cache - Cache::Store and the write
		// entries are called on core->cache only, and the instruction cache is only ever read
		// through Fetch and touched by Enable/Invalidate/FlashInvalidate (see the HID0 and icbi
		// paths in gekkoc.cpp). Flushing it anyway means the answer to "is the cache empty?" does
		// not depend on that argument staying true.

		cache->FlushAll();
		icache->FlushAll();

		// The register file, in the order the design fixes it. The time base goes out as the whole
		// 64-bit counter rather than as its two halves: TBL and TBU are views of it.
		writer.Array(regs.gpr);
		WriteFprs(writer, regs.fpr);
		WriteFprs(writer, regs.ps1);
		writer.Array(regs.spr);
		writer.Array(regs.sr);
		writer.Fields(regs.cr, regs.msr, regs.fpscr, regs.pc, regs.tb.uval);

		// The latches. decreq, intFlag and exception are volatile bools set from the decrementer
		// underflow in Tick, from the interrupt line and by the exception entry; they are sticky
		// between instructions, so a state taken while MSR[EE] is clear has to carry them rather
		// than recompute them. RESERVE and RESERVE_ADDR are the lwarx/stwcx reservation, which a
		// state taken in the middle of a locked sequence has to keep. PrCause is the reason the
		// last PROGRAM exception was taken: the handler has not read SRR1 yet at this point, so
		// the reason cannot be recovered from the register file.
		writer.Fields(decreq, intFlag, exception, RESERVE, RESERVE_ADDR, PrCause);

		// The tick at which the Flipper-side periodic work is next due. It looks derived - SyncFlipper
		// arms it as "now plus one Flipper tick" - but it is a latch, not a function of the clock:
		// the time base has advanced since it was armed, so recomputing it from the restored clock
		// would push the next scan-out, serial poll and device step up to a Flipper tick
		// (`FlipperTickStep`, 100 ticks) later than the run the state was taken from. The device
		// work is what raises the interrupts the guest sees, so a shifted deadline is a machine that
		// continues differently - the resumed run and the run that never stopped have to deliver
		// those interrupts on the same tick.
		writer.Fields(flipperDeadline);

		gatherBuffer->SaveState(writer);

		// What is left of the caches that is not a copy of main memory: the locked L1 data cache,
		// which is scratch-pad storage the guest fills with dcbz_l and locked-cache DMA and then
		// reads and writes without any of it reaching memory. The instruction cache has a
		// LockedCache member of the same shape, but nothing ever puts anything in it - LockedEnable
		// is called on the data cache only (mtspr HID2 in gekkoc.cpp) - so it is dead storage and
		// not part of the machine. LockedCacheAddr is the 16 KB-aligned window that storage is
		// mapped at. The locked cache's own enable flag is not written either: it follows HID2[LCE]
		// in the restored register file.
		writer.Raw(cache->LockedCache, Cache::LockedCacheSize);
		writer.Fields(cache->LockedCacheAddr);
	}

	void GekkoCore::LoadState(SaveStates::StateReader& reader)
	{
		reader.Array(regs.gpr);
		ReadFprs(reader, regs.fpr);
		ReadFprs(reader, regs.ps1);
		reader.Array(regs.spr);
		reader.Array(regs.sr);
		reader.Fields(regs.cr, regs.msr, regs.fpscr, regs.pc, regs.tb.uval);

		// An enum travels as the integer of its width, so it is read into an integer here and
		// checked before it is assigned: a bit pattern that is not one of the five causes cannot
		// have come from this machine, and latching the failure makes the caller refuse the whole
		// state rather than hand a handler a cause no exception entry understands.
		uint32_t cause = 0;
		reader.Fields(decreq, intFlag, exception, RESERVE, RESERVE_ADDR, cause);

		if (!reader.Failed() && !IsPrivilegedCause(cause))
		{
			reader.Fail("the save state names a PROGRAM exception cause this build does not know");
		}

		PrCause = (PrivilegedCause)cause;

		// The deadline of the Flipper work is read back with the latches above, for the reason the
		// save side gives: it is a latch of the run, not a function of the restored clock.
		reader.Fields(flipperDeadline);

		gatherBuffer->LoadState(reader);

		reader.Raw(cache->LockedCache, Cache::LockedCacheSize);

		uint32_t lockedCacheAddr = reader.U32();

		if (reader.Failed())
		{
			return;
		}

		// The locked cache window is 16 KB and the register that names it is masked to a 16 KB
		// boundary (GEKKO_DMAL_LC_ADDR), so an address that does not sit on that boundary cannot
		// have come from this machine. The window is put back even when HID2[LCE] is clear: the
		// flag is derived below, and the guest may enable the window later without reloading it.
		if ((lockedCacheAddr & 0x3fff) != 0)
		{
			reader.Fail("the save state has a locked cache address that is not 16 KB aligned");
			return;
		}

		cache->LockedCacheAddr = lockedCacheAddr;

		// Everything below is derived from what was just restored, in the order the machine itself
		// builds it.

		// The hashed page table window is a function of SDR1 (UpdateHtabRange is what
		// FlushTlbOnPteWrite consults), and the BAT pointers are positions inside the register
		// file - pointers are never serialized, they are re-seated here the way Reset seats them.
		UpdateHtabRange();

		dbatu[0] = &regs.spr[SPR::DBAT0U];
		dbatu[1] = &regs.spr[SPR::DBAT1U];
		dbatu[2] = &regs.spr[SPR::DBAT2U];
		dbatu[3] = &regs.spr[SPR::DBAT3U];

		dbatl[0] = &regs.spr[SPR::DBAT0L];
		dbatl[1] = &regs.spr[SPR::DBAT1L];
		dbatl[2] = &regs.spr[SPR::DBAT2L];
		dbatl[3] = &regs.spr[SPR::DBAT3L];

		ibatu[0] = &regs.spr[SPR::IBAT0U];
		ibatu[1] = &regs.spr[SPR::IBAT1U];
		ibatu[2] = &regs.spr[SPR::IBAT2U];
		ibatu[3] = &regs.spr[SPR::IBAT3U];

		ibatl[0] = &regs.spr[SPR::IBAT0L];
		ibatl[1] = &regs.spr[SPR::IBAT1L];
		ibatl[2] = &regs.spr[SPR::IBAT2L];
		ibatl[3] = &regs.spr[SPR::IBAT3L];

		// The cache enables follow HID0 (DCE, ICE, DLOCK) and HID2 (LCE) in the register file just
		// restored, exactly as the mtspr path in gekkoc.cpp derives them, so they are not stored:
		// a state that carried them as well could disagree with those registers.
		cache->Enable((regs.spr[SPR::HID0] & HID0_DCE) != 0);
		icache->Enable((regs.spr[SPR::HID0] & HID0_ICE) != 0);
		cache->Freeze((regs.spr[SPR::HID0] & HID0_DLOCK) != 0);
		cache->LockedEnable((regs.spr[SPR::HID2] & HID2_LCE) != 0);

		// The translation caches and the compiled blocks are dropped rather than restored: a
		// translation is a cache of the page-table walk and refills on its next miss - the
		// `changed` bit included, which is recoverable by taking the walk again (see
		// EffectiveToPhysical) - and a compiled block was translated against registers,
		// translations and instruction bytes that this load has just replaced.
		dtlb.InvalidateAll();
		itlb.InvalidateAll();

		if (jit != nullptr)
		{
			jit->InvalidateAll();
		}

		// The emulated caches are thrown away as well, and they are thrown away rather than
		// written back.
		//
		// A state is normally loaded into a machine that has *run on* since it was written (the
		// user pressed the save key, played for a while and pressed the load key), so the lines
		// the caches hold describe the newer run while main memory has just been rolled back to
		// the state: an instruction or a datum served out of the cache would be one the restored
		// machine never had. The instruction cache is the one that shows it first - the decoded
		// instruction is re-fetched through it, so a stale line makes the CPU execute the code of
		// the run that was thrown away - which is how this was found: the boot ROM, resumed from a
		// state, died on an unimplemented opcode.
		//
		// Writing the cache back instead would be worse than useless: its dirty lines are the
		// *newer* bytes, so casting them out would overwrite the memory this load has just
		// restored. `FlashInvalidate` drops every line without touching memory; the locked cache
		// is not affected by it and is restored by its own field, because it is scratch-pad
		// storage that main memory does not mirror.
		cache->FlashInvalidate();
		icache->FlashInvalidate();
	}
}
