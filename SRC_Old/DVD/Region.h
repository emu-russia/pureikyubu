// Obtaining a region by DiskID(first 4 bytes of a DVD).
// Region identification by DiskID is not very reliable.

#pragma once

// Regions according to: http://redump.org/discs/system/gc/

namespace DVD
{
	enum class Region
	{
		Unknown = -1,

		// PAL (Europe, Australia)

		EUR,			// English or mix of other languages. GW7P, GU4Y
		NOE,			// Deutschland: GW7D
		FRA,			// France: GLZF
		ESP,			// Spain: GBDS
		ITA,			// Italy: GQCI
		FAH,			// France and Holland: GFSX, GNEK
		HOL,			// Holland: GKJH
		AUS,			// Australia: D95U

		// NTSC-J (Japan)

		JPN,			// Japan: GENJ

		// NTSC (North America, Korea)

		USA,			// GW7E
		KOR,			// Korea: GTEW
	};

	Region RegionById(const char* DiskId);

	bool IsNtsc(Region region);
}
