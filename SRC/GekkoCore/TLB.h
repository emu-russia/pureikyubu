// This module is used to simulate Gekko TLB.

#pragma once

#include <unordered_map>

namespace Gekko
{
	struct TLBEntry
	{
		uint32_t addressTag;
		int8_t wimg;
	};

	class TLB
	{
		std::unordered_map<int, TLBEntry*> tlb;

	public:
		bool Exists(uint32_t ea, uint32_t& pa, int& WIMG);
		void Map(uint32_t ea, uint32_t pa, int WIMG);

		void Invalidate(uint32_t ea);
		void InvalidateAll();
	};
}
