// BitFactory is a small class for generating bitstream. Used to generate code.

// The operator << submits Bits. The first number is the value, the second number is the size of the value in bits.
// The number is clipped to the mask so that there is no overflow.

// The added bitstream drives from one end of the BitFactory and shifts the current value to the left.
// That is, in essence, this is the Shift Register.

#pragma once

#include <cstdint>
#include <utility>

typedef std::pair<int, int> Bits;		// First - value, Second - Value size in bits

class BitFactory
{

	uint64_t dataBits = 0;

public:

	BitFactory& operator << (Bits data)
	{
		uint64_t mask = ((uint64_t)1 << data.second) - 1;

		dataBits <<= data.second;
		dataBits |= (uint64_t)(data.first & mask);

		return *this;
	}

	uint8_t GetBits8() { return (uint8_t)dataBits; }
	uint16_t GetBits16() { return (uint16_t)dataBits; }
	uint32_t GetBits32() { return (uint32_t)dataBits; }
	uint64_t GetBits64() { return dataBits; }

	void Clear() { dataBits = 0; }

};
