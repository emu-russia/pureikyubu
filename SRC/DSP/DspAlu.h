
#pragma once

namespace DSP
{
	// Flag modify rules

	enum class CFlagRules
	{
		None = -1,
		Zero,
		C1,		// (Ds(39) & S(39)) | (~Dd(39) & (Ds(39) | S(39)))
		C2,		// (Ds(39) & ~S(39)) | (~Dd(39) & (Ds(39) | ~S(39)))
		C3,		// Ds(39) ^ S(15) != 0 ? (Ds(39) & S(15)) | (~Dd(39) & (Ds(39) | S(15))) : (Ds(39) & ~S(15)) | (~Dd(39) & (Ds(39) | ~S(15)))
		C4,		// ~Ds(39) & ~Dd(39)
		C5,		// Ds(39) ^ S(31) == 0 ? (Ds(39) & S(39)) | (~Dd(39) & (Ds(39) | S(39))) : (Ds(39) & ~S(39)) | (~Dd(39) & (Ds(39) | ~S(39)))
		C6,		// Ds(39) & ~Dd(39)
		C7,		// P(39) & ~D(39)
		C8,		// (P(39) & S(39)) | (~D(39) & (P(39) | S(39)))
	};

	enum class VFlagRules
	{
		None = -1,
		Zero,
		V1,		// (Ds(39) & S(39) & ~Dd(39)) | (~Ds(39) & ~S(39) & Dd(39))
		V2,		// (Ds(39) & ~S(39) & ~Dd(39)) | (~Ds(39) & S(39) & Dd(39))
		V3,		// Ds(39)& Dd(39)
		V4,		// ~Ds(39) & Dd(39)
		V5,		// Dd(39)
		V6,		// ~P(39) & D(39)
		V7,		// (P(39) & S(39) & ~D(39)) | (~P(39) & ~S(39) & D(39))
		V8,		// Ds(39) & ~Dd(39)
	};

	enum class ZFlagRules
	{
		None = -1,
		Z1,		// Dd == 0
		Z2,		// Dd(31 - 16) == 0
		Z3,		// Dd(39 - 0) == 0
	};

	enum class NFlagRules
	{
		None = -1,
		N1,		// Dd(39)
		N2,		// Dd(31)
	};

	enum class EFlagRules
	{
		None = -1,
		E1,		// Dd(39 - 31) != (0b0'0000'0000 || 0b1'1111'1111)
	};

	enum class UFlagRules
	{
		None = -1,
		U1,		// ~(Dd(31) ^ Dd(30))
	};

}
