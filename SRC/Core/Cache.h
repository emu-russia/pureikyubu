// Gekko caches support (including Data locked cache)

#pragma once

namespace Gekko
{
	enum class CacheLogLevel
	{
		None = 0,
		Commands,
		MemOps,
	};

	class Cache
	{
		uint8_t* cacheData;
		const size_t cacheSize = 0x01800000;    // 24 MBytes

		// A sign that the cache block is dirty (does not match the value in RAM).
		bool * modifiedBlocks = nullptr;

		// Cache block invalid, must be casted-in before use
		bool * invalidBlocks = nullptr;

		bool IsDirty(uint32_t pa);
		void SetDirty(uint32_t pa, bool dirty);

		bool IsInvalid(uint32_t pa);
		void SetInvalid(uint32_t pa, bool invalid);

		bool enabled = false;
		bool frozen = false;

		CacheLogLevel log = CacheLogLevel::Commands;

		void CastIn(uint32_t pa);		// Mem -> Cache
		void CastOut(uint32_t pa);		// Cache -> Mem

		// You can disable cache emulation for debugging purposes.
		bool DisableForDebugReasons = false;

		uint8_t* LockedCache = nullptr;
		bool lcenabled = false;

	public:
		Cache();
		~Cache();

		void Reset();

		void Enable(bool enable);
		bool IsEnabled() { return enabled; }

		void Freeze(bool freeze);
		bool IsFrozen() { return frozen; }

		void LockedEnable(bool enable);
		bool IsLockedEnable() { return lcenabled; }

		// Physical addressing

		void Flush(uint32_t pa);
		void Invalidate(uint32_t pa);
		void Store(uint32_t pa);
		void Touch(uint32_t pa);
		void TouchForStore(uint32_t pa);
		void Zero(uint32_t pa);
		void ZeroLocked(uint32_t pa);

		void __fastcall ReadByte(uint32_t addr, uint32_t* reg);
		void __fastcall WriteByte(uint32_t addr, uint32_t data);
		void __fastcall ReadHalf(uint32_t addr, uint32_t* reg);
		void __fastcall WriteHalf(uint32_t addr, uint32_t data);
		void __fastcall ReadWord(uint32_t addr, uint32_t* reg);
		void __fastcall WriteWord(uint32_t addr, uint32_t data);
		void __fastcall ReadDouble(uint32_t addr, uint64_t* reg);
		void __fastcall WriteDouble(uint32_t addr, uint64_t* data);

		uint32_t LockedCacheAddr = 0;

		void LockedCacheDma(bool MemToCache, uint32_t memaddr, uint32_t lcaddr, size_t bytes);
	};
}
