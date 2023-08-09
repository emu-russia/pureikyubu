// CPU controls 
#include "pch.h"

using namespace Debug;

namespace Gekko
{
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
			core->TestBreakpoints();
		}

		core->interp->ExecuteOpcode();
	}

	GekkoCore::GekkoCore()
	{
		cache = new Cache(this);
		icache = new Cache(this);
		
		// DEBUG
		//cache->SetLogLevel(CacheLogLevel::MemOps);
		//icache->SetLogLevel(CacheLogLevel::MemOps);

		gatherBuffer = new GatherBuffer(this);

		interp = new Interpreter(this);

		gekkoThread = EMUCreateThread(GekkoThreadProc, false, this, "GekkoCore");

		Reset();
	}

	GekkoCore::~GekkoCore()
	{
		StopOpcodeStatsThread();
		EMUJoinThread(gekkoThread);
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
		regs.spr[SPR::HID1] = 0x8000'0000;
		regs.spr[SPR::DEC] = 0;
		regs.spr[SPR::CTR] = 0;

		gatherBuffer->Reset();

		dtlb.InvalidateAll();
		itlb.InvalidateAll();
		cache->Reset();
		cache->Enable(false);
		icache->Reset();
		icache->Enable(false);
	}

	// Modify CPU counters
	void GekkoCore::Tick()
	{
		regs.tb.uval += CounterStep;         // timer

		uint32_t old = regs.spr[SPR::DEC];
		regs.spr[SPR::DEC] -= DecrementerStep;          // decrementer
		if ((old ^ regs.spr[SPR::DEC]) & 0x80000000)
		{
			if (regs.msr & MSR_EE)
			{
				decreq = 1;
				Report(Channel::CPU, "decrementer exception (OS alarm), pc:%08X\n", regs.pc);
			}
		}
	}

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
		//Halt("Gekko Exception: #%04X\n", (uint16_t)code);

		if (exception)
		{
			Halt("CPU Double Fault!\n");
		}

		// save regs

		regs.spr[Gekko::SPR::SRR0] = regs.pc;
		regs.spr[Gekko::SPR::SRR1] = regs.msr;

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

		// disable address translation
		regs.msr &= ~(MSR_IR | MSR_DR);

		regs.msr &= ~MSR_RI;

		regs.msr &= ~MSR_EE;

		// change PC and set exception flag
		regs.pc = (uint32_t)code;
		exception = true;
	}

	uint32_t GekkoCore::EffectiveToPhysical(uint32_t ea, MmuAccess type, int& WIMG)
	{
		return EffectiveToPhysicalMmu(ea, type, WIMG);
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

		if (!cache->IsEnabled() && (addr & ~0x3fff) == DOLPHIN_OS_LOCKED_CACHE_ADDRESS)
		{
			cache->ReadByte(pa, reg);
			return;
		}

		PIReadByte(pa, reg);
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

		if (!cache->IsEnabled() && (addr & ~0x3fff) == DOLPHIN_OS_LOCKED_CACHE_ADDRESS)
		{
			cache->WriteByte(pa, data);
			return;
		}

		PIWriteByte(pa, data);
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

		if (!cache->IsEnabled() && (addr & ~0x3fff) == DOLPHIN_OS_LOCKED_CACHE_ADDRESS)
		{
			cache->ReadHalf(pa, reg);
			return;
		}

		PIReadHalf(pa, reg);
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

		if (!cache->IsEnabled() && (addr & ~0x3fff) == DOLPHIN_OS_LOCKED_CACHE_ADDRESS)
		{
			cache->WriteHalf(pa, data);
			return;
		}

		PIWriteHalf(pa, data);
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

		if (!cache->IsEnabled() && (addr & ~0x3fff) == DOLPHIN_OS_LOCKED_CACHE_ADDRESS)
		{
			cache->ReadWord(pa, reg);
			return;
		}

		PIReadWord(pa, reg);
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

		if (!cache->IsEnabled() && (addr & ~0x3fff) == DOLPHIN_OS_LOCKED_CACHE_ADDRESS)
		{
			cache->WriteWord(pa, data);
			return;
		}

		PIWriteWord(pa, data);
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

		if (!cache->IsEnabled() && (addr & ~0x3fff) == DOLPHIN_OS_LOCKED_CACHE_ADDRESS)
		{
			cache->ReadDouble(pa, reg);
			return;
		}

		// It is suspected that this type of single-beat transaction is not supported by Flipper PI.

		PIReadDouble(pa, reg);
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

		if (!cache->IsEnabled() && (addr & ~0x3fff) == DOLPHIN_OS_LOCKED_CACHE_ADDRESS)
		{
			cache->WriteDouble(pa, data);
			return;
		}

		// It is suspected that this type of single-beat transaction is not supported by Flipper PI.

		PIWriteDouble(pa, data);
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

		// You don't need to use the ICache in BS1, even if it is enabled
		if (pa >= BOOTROM_START_ADDRESS) {
			PIReadWord(pa, reg);
			return;
		}

		if (icache->IsEnabled() && (WIMG & WIMG_I) == 0)
		{
			icache->ReadWord(pa, reg);
			return;
		}

		PIReadWord(pa, reg);
	}

}


namespace Gekko
{
	bool TLB::Exists(uint32_t ea, uint32_t& pa, int& WIMG)
	{
		auto it = tlb.find(ea >> 12);
		if (it != tlb.end())
		{
			TLBEntry* entry = it->second;
			pa = (entry->addressTag << 12) | (ea & 0xfff);
			WIMG = entry->wimg;
			return true;
		}
		return false;
	}

	void TLB::Map(uint32_t ea, uint32_t pa, uint32_t pc, int WIMG)
	{
		TLBEntry* entry = new TLBEntry;
		entry->addressTag = pa >> 12;
		entry->wimg = WIMG;
		entry->pc = pc;
		tlb[ea >> 12] = entry;
	}

	void TLB::Invalidate(uint32_t ea)
	{
		auto it = tlb.find(ea >> 12);
		if (it != tlb.end())
		{
			delete it->second;
			tlb.erase(it);
		}
	}

	void TLB::InvalidateAll()
	{
		for (auto it = tlb.begin(); it != tlb.end(); ++it)
		{
			delete it->second;
		}
		tlb.clear();
	}

	void TLB::Dump()
	{
		for (auto it = tlb.begin(); it != tlb.end(); ++it)
		{
			uint32_t ea = it->first << 12;
			TLBEntry* entry = it->second;
			Report(Channel::CPU, "EA 0x%08X -> PA 0x%08X (wimg: %d, pc: 0x%08X)\n", ea, entry->addressTag << 12, entry->wimg, entry->pc);
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

	void GekkoCore::TestBreakpoints()
	{
		if (oneShotBreakpoint != BadAddress && regs.pc == oneShotBreakpoint)
		{
			oneShotBreakpoint = BadAddress;
			Halt("One shot breakpoint\n");
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
		}
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
	Cache::Cache(GekkoCore* core)
	{
		this->core = core;

		cacheData = new uint8_t[cacheSize];

		modifiedBlocks = new bool[cacheSize >> 5];

		invalidBlocks = new bool[cacheSize >> 5];

		LockedCache = new uint8_t[16 * 1024];

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
		// TODO
		// Dirty hack so far. The cache is required for bootrom to work correctly when loading an application using Apploader. If you start DVD with Bootrom HLE at once, the games work more stable without cache.
		// We need to fix the cache.
#if GEKKOCORE_CACHE_DISABLE_HACK
		if (!emu.bootrom) {
			enable = false;
		}
#endif

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

		if (IsDirty(pa))
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
		size_t blocks_num = cacheSize >> 5;
		for (size_t n = 0; n < blocks_num; n++) {
			modifiedBlocks[n] = false;
			invalidBlocks[n] = true;
		}
	}

	void Cache::Store(uint32_t pa)
	{
		if (pa >= cacheSize)
			return;

		if (IsDirty(pa))
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

		CastIn(pa);
		SetInvalid(pa, false);
		SetDirty(pa, false);			// Valid & Not Dirty

		if (log >= CacheLogLevel::Commands)
		{
			Report(Channel::CPU, "Cache::Touch 0x%08X\n", pa);
		}
	}

	void Cache::TouchForStore(uint32_t pa)
	{
		if (pa >= cacheSize)
			return;

		CastIn(pa);
		SetInvalid(pa, false);
		SetDirty(pa, true);				// Valid & Dirty

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

		if (log >= CacheLogLevel::MemOps)
		{
			Report(Channel::CPU, "Cache::CastIn: 0x%08X\n", pa & ~0x1f);
		}

		PIReadBurst(pa & ~0x1f, &cacheData[pa & ~0x1f]);
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

		PIWriteBurst(pa & ~0x1f, &cacheData[pa & ~0x1f]);
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
			Report(Channel::CPU, "Cache::ReadHalf: Unaligned cache access addr:0x%08X!\n", addr);

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
			Report(Channel::CPU, "Cache::WriteHalf: Unaligned cache access addr:0x%08X!\n", addr);

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
			Report(Channel::CPU, "Cache::ReadWord: Unaligned cache access addr:0x%08X!\n", addr);

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
			Report(Channel::CPU, "Cache::WriteWord: Unaligned cache access addr:0x%08X!\n", addr);

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
			Report(Channel::CPU, "Cache::ReadDouble: Unaligned cache access addr:0x%08X!\n", addr);

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
			Report(Channel::CPU, "Cache::WriteDouble: Unaligned cache access addr:0x%08X!\n", addr);

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
				PIReadBurst(memaddr, &LockedCache[lcaddr & 0x3fff]);
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
				PIWriteBurst(memaddr, &LockedCache[lcaddr & 0x3fff]);
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
		retireTimeout = 0;

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

			PIWriteBurst(core->regs.spr[(int)SPR::WPAR] & ~0x1f, burstData);
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

	bool GatherBuffer::NotEmpty()
	{
		// The GatherBuffer has an undocumented feature - after a certain number of cycles the data in it is destroyed and it becomes free

		retireTimeout++;
		if (retireTimeout >= GEKKOCORE_GATHER_BUFFER_RETIRE_TICKS)
		{
			Reset();
		}

		return readPtr != writePtr;
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

		// Try TLB

#if GEKKOCORE_USE_TLB
		TLB* tlb = (type == MmuAccess::Execute) ? &itlb : &dtlb;

		if (tlb->Exists(ea, pa, WIMG))
		{
			return pa;
		}
#endif

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
				PIWriteWord(secondaryPteAddr + 4, pte[1]);
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
