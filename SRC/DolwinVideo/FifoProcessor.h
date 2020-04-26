
#pragma once

namespace GX
{
	class FifoProcessor
	{
		static const size_t fifoSize = 64 * 1024;

		uint8_t* fifo;
		size_t readPtr;
		size_t writePtr;

	public:
		FifoProcessor();
		~FifoProcessor();

		void WriteBytes(uint8_t dataPtr[32]);

		size_t GetSize();

		bool EnoughToExecute();

		uint8_t Read8();
		uint16_t Read16();
		uint32_t Read32();
		float ReadFloat();
	};
}
