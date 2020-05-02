// Gekko caches support (including Data locked cache)

// As defined by the PowerPC architecture, they are physically indexed.

// Emulation of access to the cache is simple - a copy is created for the main memory, same size as the main RAM.
// If cached access is performed, all reads and writes are made from this buffer, otherwise from RAM.
// Invalidation causes new data to be loaded from RAM into the cache buffer.

// We do not support scattering for a locked cache and assume that it is locked as a fixed chunk.

#include "pch.h"

namespace Gekko
{
	Cache::Cache()
	{
		cacheData = new uint8_t[cacheSize];
		assert(cacheData);

		modifiedBlocks = new bool[cacheSize >> 5];
		assert(modifiedBlocks);

		invalidBlocks = new bool[cacheSize >> 5];
		assert(invalidBlocks);

		LockedCache = new uint8_t[16 * 1024];
		assert(LockedCache);

		Reset();
	}

	Cache::~Cache()
	{
		delete[] cacheData;
		delete[] modifiedBlocks;
		delete[] invalidBlocks;
	}

	void Cache::Reset()
	{
		DBReport2(DbgChannel::CPU, "Cache::Reset\n");

		for (size_t i = 0; i < (cacheSize >> 5); i++)
		{
			modifiedBlocks[i] = false;
			invalidBlocks[i] = true;
		}
	}

	void Cache::Enable(bool enable)
	{
		enabled = enable;

		if (DisableForDebugReasons)
		{
			enabled = false;
		}

		if (log >= CacheLogLevel::Commands)
		{
			DBReport2(DbgChannel::CPU, "Cache::Enable %i\n", enable ? 1 : 0);
		}
	}

	void Cache::Freeze(bool freeze)
	{
		frozen = freeze;

		if (log >= CacheLogLevel::Commands)
		{
			DBReport2(DbgChannel::CPU, "Cache::Freeze %i\n", freeze ? 1 : 0);
		}
	}

	void Cache::LockedEnable(bool enable)
	{
		lcenabled = enable;

		if (log >= CacheLogLevel::Commands)
		{
			DBReport2(DbgChannel::CPU, "Cache::LockedEnable %i\n", enable ? 1 : 0);
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
			DBReport2(DbgChannel::CPU, "Cache::SetDirty. pa: 0x%08X\n", pa & ~0x1f);
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
			DBReport2(DbgChannel::CPU, "Cache::SetInvalid. pa: 0x%08X\n", pa & ~0x1f);
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
			DBReport2(DbgChannel::CPU, "Cache::Flush 0x%08X\n", pa);
		}
	}

	void Cache::Invalidate(uint32_t pa)
	{
		if (pa >= cacheSize)
			return;

		SetInvalid(pa, true);

		if (log >= CacheLogLevel::Commands)
		{
			DBReport2(DbgChannel::CPU, "Cache::Invalidate 0x%08X\n", pa);
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
			DBReport2(DbgChannel::CPU, "Cache::Store 0x%08X\n", pa);
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
			DBReport2(DbgChannel::CPU, "Cache::Touch 0x%08X\n", pa);
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
			DBReport2(DbgChannel::CPU, "Cache::TouchForStore 0x%08X\n", pa);
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
			DBReport2(DbgChannel::CPU, "Cache::Zero 0x%08X\n", pa);
		}
	}

	void Cache::ZeroLocked(uint32_t pa)
	{
		if (log >= CacheLogLevel::Commands)
		{
			DBReport2(DbgChannel::CPU, "Cache::ZeroLocked 0x%08X\n", pa);
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
			DBReport2(DbgChannel::CPU, "Cache::CastIn: 0x%08X\n", pa & ~0x1f);
		}

		MIReadBurst(pa & ~0x1f, &cacheData[pa & ~0x1f]);
	}

	void Cache::CastOut(uint32_t pa)
	{
		assert(pa < cacheSize);

		if (frozen)
			return;

		if (log >= CacheLogLevel::MemOps)
		{
			DBReport2(DbgChannel::CPU, "Cache::CastOut: 0x%08X\n", pa & ~0x1f);
		}

		MIWriteBurst(pa & ~0x1f, &cacheData[pa & ~0x1f]);
	}

	void __fastcall Cache::ReadByte(uint32_t addr, uint32_t* reg)
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

		if (addr >= cacheSize || DisableForDebugReasons)
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
			DBReport2(DbgChannel::CPU, "Cache::ReadByte. addr: 0x%08X, *reg: 0x%08X\n", addr, *reg);
		}
	}

	void __fastcall Cache::WriteByte(uint32_t addr, uint32_t data)
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

		if (addr >= cacheSize || DisableForDebugReasons)
			return;

		if (IsInvalid(addr))
		{
			CastIn(addr);
			SetInvalid(addr, false);
		}
		cacheData[addr] = (uint8_t)data;

		if (log >= CacheLogLevel::MemOps)
		{
			DBReport2(DbgChannel::CPU, "Cache::WriteByte. addr: 0x%08X, data: 0x%08X\n", addr, data);
		}

		SetDirty(addr, true);
	}

	void __fastcall Cache::ReadHalf(uint32_t addr, uint32_t* reg)
	{
		// Locked cache
		if (IsLockedEnable())
		{
			if ((addr & ~0x3fff) == LockedCacheAddr)
			{
				uint8_t* ptr = &LockedCache[addr & 0x3fff];
				*reg = (uint32_t)_byteswap_ushort(*(uint16_t*)ptr);
				return;
			}
		}

		if (addr >= cacheSize || DisableForDebugReasons)
			return;

		if (IsInvalid(addr))
		{
			CastIn(addr);
			SetInvalid(addr, false);
			SetDirty(addr, false);
		}
		*reg = _byteswap_ushort(*(uint16_t *)&cacheData[addr]);

		if (log >= CacheLogLevel::MemOps)
		{
			DBReport2(DbgChannel::CPU, "Cache::ReadHalf. addr: 0x%08X, *reg: 0x%08X\n", addr, *reg);
		}
	}

	void __fastcall Cache::WriteHalf(uint32_t addr, uint32_t data)
	{
		// Locked cache
		if (IsLockedEnable())
		{
			if ((addr & ~0x3fff) == LockedCacheAddr)
			{
				uint8_t* ptr = &LockedCache[addr & 0x3fff];
				*(uint16_t*)ptr = _byteswap_ushort((uint16_t)data);
				return;
			}
		}

		if (addr >= cacheSize || DisableForDebugReasons)
			return;

		if (IsInvalid(addr))
		{
			CastIn(addr);
			SetInvalid(addr, false);
		}
		*(uint16_t*)&cacheData[addr] = _byteswap_ushort((uint16_t)data);

		if (log >= CacheLogLevel::MemOps)
		{
			DBReport2(DbgChannel::CPU, "Cache::WriteHalf. addr: 0x%08X, data: 0x%08X\n", addr, data);
		}

		SetDirty(addr, true);
	}

	void __fastcall Cache::ReadWord(uint32_t addr, uint32_t* reg)
	{
		// Locked cache
		if (IsLockedEnable())
		{
			if ((addr & ~0x3fff) == LockedCacheAddr)
			{
				uint8_t* ptr = &LockedCache[addr & 0x3fff];
				*reg = _byteswap_ulong(*(uint32_t*)ptr);
				return;
			}
		}

		if (addr >= cacheSize || DisableForDebugReasons)
			return;

		if (IsInvalid(addr))
		{
			CastIn(addr);
			SetInvalid(addr, false);
			SetDirty(addr, false);
		}
		*reg = _byteswap_ulong(*(uint32_t*)&cacheData[addr]);

		if (log >= CacheLogLevel::MemOps)
		{
			DBReport2(DbgChannel::CPU, "Cache::ReadWord. addr: 0x%08X, *reg: 0x%08X\n", addr, *reg);
		}
	}

	void __fastcall Cache::WriteWord(uint32_t addr, uint32_t data)
	{
		// Locked cache
		if (IsLockedEnable())
		{
			if ((addr & ~0x3fff) == LockedCacheAddr)
			{
				uint8_t* ptr = &LockedCache[addr & 0x3fff];
				*(uint32_t*)ptr = _byteswap_ulong(data);
				return;
			}
		}

		if (addr >= cacheSize || DisableForDebugReasons)
			return;

		if (IsInvalid(addr))
		{
			CastIn(addr);
			SetInvalid(addr, false);
		}
		*(uint32_t*)&cacheData[addr] = _byteswap_ulong(data);

		if (log >= CacheLogLevel::MemOps)
		{
			DBReport2(DbgChannel::CPU, "Cache::WriteWord. addr: 0x%08X, data: 0x%08X\n", addr, data);
		}

		SetDirty(addr, true);
	}

	void __fastcall Cache::ReadDouble(uint32_t addr, uint64_t* reg)
	{
		// Locked cache
		if (IsLockedEnable())
		{
			if ((addr & ~0x3fff) == LockedCacheAddr)
			{
				uint8_t* buf = &LockedCache[addr & 0x3fff];
				*reg = _byteswap_uint64(*(uint64_t*)buf);
				return;
			}
		}

		if (addr >= cacheSize || DisableForDebugReasons)
			return;

		if (IsInvalid(addr))
		{
			CastIn(addr);
			SetInvalid(addr, false);
			SetDirty(addr, false);
		}
		*reg = _byteswap_uint64(*(uint64_t*)&cacheData[addr]);

		if (log >= CacheLogLevel::MemOps)
		{
			DBReport2(DbgChannel::CPU, "Cache::ReadDouble. addr: 0x%08X, *reg: 0x%llX\n", addr, *reg);
		}
	}

	void __fastcall Cache::WriteDouble(uint32_t addr, uint64_t* data)
	{
		// Locked cache
		if (IsLockedEnable())
		{
			if ((addr & ~0x3fff) == LockedCacheAddr)
			{
				uint8_t* buf = &LockedCache[addr & 0x3fff];
				*(uint64_t*)buf = _byteswap_uint64(*data);
				return;
			}
		}

		if (addr >= cacheSize || DisableForDebugReasons)
			return;

		if (IsInvalid(addr))
		{
			CastIn(addr);
			SetInvalid(addr, false);
		}
		*(uint64_t*)&cacheData[addr] = _byteswap_uint64(*data);

		if (log >= CacheLogLevel::MemOps)
		{
			DBReport2(DbgChannel::CPU, "Cache::WriteDouble. addr: 0x%08X, data: 0x%llX\n", addr, data);
		}

		SetDirty(addr, true);
	}

	void Cache::LockedCacheDma(bool MemToCache, uint32_t memaddr, uint32_t lcaddr, size_t bursts)
	{
		if (MemToCache)
		{   // 1 load - transfer from external memory to locked cache

			if (log >= CacheLogLevel::MemOps)
			{
				DBReport2(DbgChannel::CPU, "Load Locked Cache: memadr: 0x%08X, lcaddr: 0x%08X, bursts: %i\n", memaddr, lcaddr, bursts);
			}

			for (size_t i = 0; i < bursts; i++)
			{
				MIReadBurst(memaddr, &LockedCache[lcaddr & 0x3fff]);
				memaddr += 32;
				lcaddr += 32;
			}
		}
		else
		{   // 0 store -  transfer from locked cache to external memory 

			if (log >= CacheLogLevel::MemOps)
			{
				DBReport2(DbgChannel::CPU, "Store Locked Cache: memadr: 0x%08X, lcaddr: 0x%08X, bursts: %i\n", memaddr, lcaddr, bursts);
			}

			for (size_t i = 0; i < bursts; i++)
			{
				MIWriteBurst(memaddr, &LockedCache[lcaddr & 0x3fff]);
				memaddr += 32;
				lcaddr += 32;
			}
		}
	}

}
