
#pragma once

namespace GX
{
	class GXCore;

	class FifoProcessor
	{
		size_t fifoSize = 1024 * 1024;
		uint8_t* fifo = nullptr;
		size_t readPtr = 0;
		size_t writePtr = 0;
		bool allocated = false;

		size_t GetSize();
		bool EnoughToExecute();

		size_t vertexSize[8] = { 0 };
		void RecalcVertexSize();		// Called every time the VCD / VAT settings are changed.

		uint8_t Read8();
		uint16_t Read16();
		uint32_t Read32();
		float ReadFloat();

		uint8_t Peek8(size_t offset);
		uint8_t Peek16(size_t offset);

		void ExecuteCommand();

		GXCore* gxcore = nullptr;
		SpinLock lock;

	public:
		FifoProcessor(GXCore *gx);
		FifoProcessor(GXCore* gx, uint8_t* fifoPtr, size_t size);	// Call FIFO
		~FifoProcessor();

		void WriteBytes(uint8_t dataPtr[32]);

		void Reset();
	};

}
