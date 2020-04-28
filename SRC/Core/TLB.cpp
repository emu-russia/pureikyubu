#include "pch.h"

namespace Gekko
{
	bool TLB::Exists(uint32_t ea, uint32_t& pa)
	{
		auto it = tlb.find(ea >> 12);
		if (it != tlb.end())
		{
			pa = ((uint32_t)it->second << 12) | (ea & 0xfff);
			return true;
		}
		return false;
	}

	void TLB::Map(uint32_t ea, uint32_t pa)
	{
		tlb[ea >> 12] = pa >> 12;
	}

	void TLB::Invalidate(uint32_t ea)
	{
		auto it = tlb.find(ea >> 12);
		if (it != tlb.end())
		{
			tlb.erase(it);
		}
	}

	void TLB::InvalidateAll()
	{
		tlb.clear();
	}
}
