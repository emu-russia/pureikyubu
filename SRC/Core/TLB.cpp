#include "pch.h"

namespace Gekko
{
	bool TLB::Exists(uint32_t ea, uint32_t& pa, int &WIMG)
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

	void TLB::Map(uint32_t ea, uint32_t pa, int WIMG)
	{
		TLBEntry* entry = new TLBEntry;
		entry->addressTag = pa >> 12;
		entry->wimg = WIMG;
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
}
