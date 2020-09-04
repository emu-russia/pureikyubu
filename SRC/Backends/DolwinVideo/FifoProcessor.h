// PHASED OUT: Will move to the shared GX component.

#pragma once

namespace GX_FromFuture
{
	class FifoProcessor
	{
		size_t fifoSize = 1024 * 1024;
		uint8_t* fifo = nullptr;
		size_t readPtr = 0;
		size_t writePtr = 0;
		bool allocated = false;

	public:
		FifoProcessor();
		FifoProcessor(uint8_t *fifoPtr, size_t size);	// Call FIFO
		~FifoProcessor();

		void WriteBytes(uint8_t dataPtr[32]);

		size_t GetSize();

		bool EnoughToExecute();

		uint8_t Read8();
		uint16_t Read16();
		uint32_t Read32();
		float ReadFloat();

		uint8_t Peek8(size_t offset);
		uint8_t Peek16(size_t offset);
	};
}
