// This module is used to simulate Gekko TLB.

#pragma once

#include <unordered_map>

namespace Gekko
{
	class TLB
	{
		std::unordered_map<int, int> tlb;

	public:
		bool Exists(uint32_t ea, uint32_t& pa);
		void Map(uint32_t ea, uint32_t pa);

		void Invalidate(uint32_t ea);
		void InvalidateAll();
	};
}
