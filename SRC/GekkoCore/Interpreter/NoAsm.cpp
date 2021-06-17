// Asm replacement

#include "../pch.h"

extern "C"
{
	uint32_t CarryBit;
	uint32_t OverflowBit;
}

/// <summary>
/// Result = a + b + CarryBit. Return carry flag in CarryBit. Return overflow flag in OverflowBit.
/// </summary>
/// <param name="a"></param>
/// <param name="b"></param>
/// <returns></returns>
extern "C" uint32_t __FASTCALL FullAdder(uint32_t a, uint32_t b)
{
	uint64_t res = (uint64_t)a + (uint64_t)b + (uint64_t)(CarryBit != 0 ? 1 : 0);

	bool msb = (res & 0x8000'0000) != 0 ? true : false;
	bool aMsb = (a & 0x8000'0000) != 0 ? true : false;
	bool bMsb = (b & 0x8000'0000) != 0 ? true : false;
	OverflowBit = 0;
	if (aMsb == bMsb)
	{
		OverflowBit = (aMsb != msb) ? 1 : 0;
	}

	CarryBit = ((res & 0xffffffff'00000000) != 0) ? 1 : 0;

	return (uint32_t)res;
}

/// <summary>
/// Rotate 32 bit left
/// </summary>
/// <param name="sa">Rotate bits amount</param>
/// <param name="data">Source</param>
/// <returns>Result</returns>
extern "C" uint32_t __FASTCALL Rotl32(int sa, uint32_t data)
{
	return (data << sa) | (data >> ((32 - sa) & 31));
}
