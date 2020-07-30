// Disk region detection
#include "pch.h"

namespace DVD
{
	// Get region by DiskID (not a very reliable method)
	Region RegionById(const char* DiskId)
	{
		switch (DiskId[3])
		{
			case 'P':
			case 'Y':
				return Region::EUR;
			case 'D':
				return Region::NOE;
			case 'F':
				return Region::FRA;
			case 'S':
				return Region::ESP;
			case 'I':
				return Region::ITA;
			case 'X':
			case 'K':
				return Region::FAH;
			case 'H':
				return Region::HOL;
			case 'U':
				return Region::AUS;

			case 'J':
				return Region::JPN;

			case 'E':
				return Region::USA;
			case 'W':
				return Region::KOR;
		}

		return Region::Unknown;
	}

	// Check that the region is NTSC-like
	bool IsNtsc(Region region)
	{
		switch (region)
		{
			case Region::JPN:
			case Region::USA:
			case Region::KOR:
				return true;
		}
		return false;
	}

}
