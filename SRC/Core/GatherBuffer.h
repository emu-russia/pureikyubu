// Gekko Gather Buffer

#pragma once

namespace Gekko
{
	class GatherBuffer
	{
		uint8_t fifo[32 * 4] = { 0 };
		size_t readPtr = 0;
		size_t writePtr = 0;

		void WriteBytes(uint8_t* data, size_t size);
		size_t GatherSize();

		bool log = true;

	public:

		void Reset();

		void Write8(uint8_t value);
		void Write16(uint16_t value);
		void Write32(uint32_t value);
		void Write64(uint64_t value);

		bool NotEmpty();

	};
}
