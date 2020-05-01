// Gekko caches support (including Data locked cache)

// As defined by the PowerPC architecture, they are physically indexed.

// Emulation of access to the cache is simple - a copy is created for the main memory, same size as the main RAM.
// If cached access is performed, all reads and writes are made from this buffer, otherwise from RAM.
// Invalidation causes new data to be loaded from RAM into the cache buffer.

// The instruction cache is not emulated because it is accessed only in one direction (Read).
// Accordingly, it makes no sense to store a copy of RAM, you can just immediately read it from memory.

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
			MIWriteBurst(pa & ~0x1f, &cacheData[pa & ~0x1f]);
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
			MIWriteBurst(pa & ~0x1f, &cacheData[pa & ~0x1f]);
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

		MIReadBurst(pa & ~0x1f, &cacheData[pa & ~0x1f]);
		SetDirty(pa, true);
		SetInvalid(pa, false);

		if (log >= CacheLogLevel::Commands)
		{
			DBReport2(DbgChannel::CPU, "Cache::Touch 0x%08X\n", pa);
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
		// TODO
	}

	void __fastcall Cache::ReadByte(uint32_t addr, uint32_t* reg)
	{
		if (addr >= cacheSize)
			return;

		if (IsInvalid(addr))
		{
			if (log >= CacheLogLevel::MemOps)
			{
				DBReport2(DbgChannel::CPU, "Cache::ReadByte busrt cache: 0x%08X\n", addr & ~0x1f);
			}

			MIReadBurst(addr & ~0x1f, &cacheData[addr & ~0x1f]);
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
		if (addr >= cacheSize)
			return;

		if (IsInvalid(addr))
		{
			if (log >= CacheLogLevel::MemOps)
			{
				DBReport2(DbgChannel::CPU, "Cache::WriteByte busrt cache: 0x%08X\n", addr & ~0x1f);
			}

			MIReadBurst(addr & ~0x1f, &cacheData[addr & ~0x1f]);
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
		if (addr >= cacheSize)
			return;

		if (IsInvalid(addr))
		{
			if (log >= CacheLogLevel::MemOps)
			{
				DBReport2(DbgChannel::CPU, "Cache::ReadHalf busrt cache: 0x%08X\n", addr & ~0x1f);
			}

			MIReadBurst(addr & ~0x1f, &cacheData[addr & ~0x1f]);
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
		if (addr >= cacheSize)
			return;

		if (IsInvalid(addr))
		{
			if (log >= CacheLogLevel::MemOps)
			{
				DBReport2(DbgChannel::CPU, "Cache::WriteHalf busrt cache: 0x%08X\n", addr & ~0x1f);
			}

			MIReadBurst(addr & ~0x1f, &cacheData[addr & ~0x1f]);
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
		if (addr >= cacheSize)
			return;

		if (IsInvalid(addr))
		{
			if (log >= CacheLogLevel::MemOps)
			{
				DBReport2(DbgChannel::CPU, "Cache::ReadWord busrt cache: 0x%08X\n", addr & ~0x1f);
			}

			MIReadBurst(addr & ~0x1f, &cacheData[addr & ~0x1f]);
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
		if (addr >= cacheSize)
			return;

		if (IsInvalid(addr))
		{
			if (log >= CacheLogLevel::MemOps)
			{
				DBReport2(DbgChannel::CPU, "Cache::WriteWord busrt cache: 0x%08X\n", addr & ~0x1f);
			}

			MIReadBurst(addr & ~0x1f, &cacheData[addr & ~0x1f]);
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
		if (addr >= cacheSize)
			return;

		if (IsInvalid(addr))
		{
			if (log >= CacheLogLevel::MemOps)
			{
				DBReport2(DbgChannel::CPU, "Cache::ReadDouble busrt cache: 0x%08X\n", addr & ~0x1f);
			}

			MIReadBurst(addr & ~0x1f, &cacheData[addr & ~0x1f]);
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
		if (addr >= cacheSize)
			return;

		if (IsInvalid(addr))
		{
			if (log >= CacheLogLevel::MemOps)
			{
				DBReport2(DbgChannel::CPU, "Cache::WriteDouble busrt cache: 0x%08X\n", addr & ~0x1f);
			}

			MIReadBurst(addr & ~0x1f, &cacheData[addr & ~0x1f]);
			SetInvalid(addr, false);
		}
		*(uint64_t*)&cacheData[addr] = _byteswap_uint64(*data);

		if (log >= CacheLogLevel::MemOps)
		{
			DBReport2(DbgChannel::CPU, "Cache::WriteDouble. addr: 0x%08X, data: 0x%llX\n", addr, data);
		}

		SetDirty(addr, true);
	}

}
