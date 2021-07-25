// Gekko Memory interface

#include "pch.h"

namespace Gekko
{

	// Centralized hub which attracts all memory access requests from the interpreter or recompiler 
	// (as well as those who they pretend, for example HLE or Debugger).

	void GekkoCore::ReadByte(uint32_t addr, uint32_t *reg)
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
			Exception(Exception::DSI);
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
			Exception(Exception::DSI);
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

	void GekkoCore::ReadHalf(uint32_t addr, uint32_t *reg)
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
			Exception(Exception::DSI);
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
			Exception(Exception::DSI);
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

	void GekkoCore::ReadWord(uint32_t addr, uint32_t *reg)
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
			Exception(Exception::DSI);
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
			Exception(Exception::DSI);
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

	void GekkoCore::ReadDouble(uint32_t addr, uint64_t *reg)
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
			Exception(Exception::DSI);
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

	void GekkoCore::WriteDouble(uint32_t addr, uint64_t *data)
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
			Exception(Exception::DSI);
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

}
