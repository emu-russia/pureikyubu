#include "pch.h"

extern int VtxSize[8];

namespace GX
{
	FifoProcessor::FifoProcessor()
	{
		fifo = new uint8_t[fifoSize];
		assert(fifo);
		memset(fifo, 0, fifoSize);
		allocated = true;
	}

	FifoProcessor::FifoProcessor(uint8_t* fifoPtr, size_t size)
	{
		fifo = fifoPtr;
		fifoSize = size + 1;
		writePtr = fifoSize - 1;
		allocated = false;
	}

	FifoProcessor::~FifoProcessor()
	{
		if (allocated)
		{
			delete[] fifo;
		}
	}

	void FifoProcessor::WriteBytes(uint8_t dataPtr[32])
	{
		if ((writePtr + 32) < fifoSize)
		{
			memcpy(&fifo[writePtr], dataPtr, 32);
			writePtr += 32;
		}
		else
		{
			size_t part1Size = fifoSize - writePtr;
			memcpy(&fifo[writePtr], dataPtr, part1Size);
			writePtr = 32 - part1Size;
			memcpy(fifo, dataPtr + part1Size, writePtr);

			DBReport2(DbgChannel::GP, "FifoProcessor: fifo wrapped\n");
		}
	}

	size_t FifoProcessor::GetSize()
	{
		if (writePtr >= readPtr)
		{
			return writePtr - readPtr;
		}
		else
		{
			return (fifoSize - readPtr) + writePtr;
		}
	}

	bool FifoProcessor::EnoughToExecute()
	{
		if (GetSize() < 1)
			return false;

		uint8_t cmd = Peek8(0);

		switch(cmd)
		{
			case OP_CMD_NOP:
				return true;

			case OP_CMD_INV | 0:
			case OP_CMD_INV | 1:
			case OP_CMD_INV | 2:
			case OP_CMD_INV | 3:
			case OP_CMD_INV | 4:
			case OP_CMD_INV | 5:
			case OP_CMD_INV | 6:
			case OP_CMD_INV | 7:
				return true;

			case OP_CMD_CALL_DL | 0:
			case OP_CMD_CALL_DL | 1:
			case OP_CMD_CALL_DL | 2:
			case OP_CMD_CALL_DL | 3:
			case OP_CMD_CALL_DL | 4:
			case OP_CMD_CALL_DL | 5:
			case OP_CMD_CALL_DL | 6:
			case OP_CMD_CALL_DL | 7:
				return GetSize() >= 9;

			case OP_CMD_LOAD_BPREG | 0:
			case OP_CMD_LOAD_BPREG | 1:
			case OP_CMD_LOAD_BPREG | 2:
			case OP_CMD_LOAD_BPREG | 3:
			case OP_CMD_LOAD_BPREG | 4:
			case OP_CMD_LOAD_BPREG | 5:
			case OP_CMD_LOAD_BPREG | 6:
			case OP_CMD_LOAD_BPREG | 7:
			case OP_CMD_LOAD_BPREG | 8:
			case OP_CMD_LOAD_BPREG | 0xa:
			case OP_CMD_LOAD_BPREG | 0xb:
			case OP_CMD_LOAD_BPREG | 0xc:
			case OP_CMD_LOAD_BPREG | 0xd:
			case OP_CMD_LOAD_BPREG | 0xe:
			case OP_CMD_LOAD_BPREG | 0xf:
				return GetSize() >= 5;

			case OP_CMD_LOAD_CPREG | 0:
			case OP_CMD_LOAD_CPREG | 1:
			case OP_CMD_LOAD_CPREG | 2:
			case OP_CMD_LOAD_CPREG | 3:
			case OP_CMD_LOAD_CPREG | 4:
			case OP_CMD_LOAD_CPREG | 5:
			case OP_CMD_LOAD_CPREG | 6:
			case OP_CMD_LOAD_CPREG | 7:
				return GetSize() >= 6;
			
			case OP_CMD_LOAD_XFREG | 0:
			case OP_CMD_LOAD_XFREG | 1:
			case OP_CMD_LOAD_XFREG | 2:
			case OP_CMD_LOAD_XFREG | 3:
			case OP_CMD_LOAD_XFREG | 4:
			case OP_CMD_LOAD_XFREG | 5:
			case OP_CMD_LOAD_XFREG | 6:
			case OP_CMD_LOAD_XFREG | 7:
			{
				if (GetSize() < 3)
					return false;

				uint16_t len = Peek16(1) + 1;
				return GetSize() >= (len * 4 + 5);
			}

			case OP_CMD_LOAD_INDXA | 0:
			case OP_CMD_LOAD_INDXA | 1:
			case OP_CMD_LOAD_INDXA | 2:
			case OP_CMD_LOAD_INDXA | 3:
			case OP_CMD_LOAD_INDXA | 4:
			case OP_CMD_LOAD_INDXA | 5:
			case OP_CMD_LOAD_INDXA | 6:
			case OP_CMD_LOAD_INDXA | 7:
				return GetSize() >= 5;

			case OP_CMD_LOAD_INDXB | 0:
			case OP_CMD_LOAD_INDXB | 1:
			case OP_CMD_LOAD_INDXB | 2:
			case OP_CMD_LOAD_INDXB | 3:
			case OP_CMD_LOAD_INDXB | 4:
			case OP_CMD_LOAD_INDXB | 5:
			case OP_CMD_LOAD_INDXB | 6:
			case OP_CMD_LOAD_INDXB | 7:
				return GetSize() >= 5;

			case OP_CMD_LOAD_INDXC | 0:
			case OP_CMD_LOAD_INDXC | 1:
			case OP_CMD_LOAD_INDXC | 2:
			case OP_CMD_LOAD_INDXC | 3:
			case OP_CMD_LOAD_INDXC | 4:
			case OP_CMD_LOAD_INDXC | 5:
			case OP_CMD_LOAD_INDXC | 6:
			case OP_CMD_LOAD_INDXC | 7:
				return GetSize() >= 5;

			case OP_CMD_LOAD_INDXD | 0:
			case OP_CMD_LOAD_INDXD | 1:
			case OP_CMD_LOAD_INDXD | 2:
			case OP_CMD_LOAD_INDXD | 3:
			case OP_CMD_LOAD_INDXD | 4:
			case OP_CMD_LOAD_INDXD | 5:
			case OP_CMD_LOAD_INDXD | 6:
			case OP_CMD_LOAD_INDXD | 7:
				return GetSize() >= 5;

			// 0x80
			case OP_CMD_DRAW_QUAD | 0:
			case OP_CMD_DRAW_QUAD | 1:
			case OP_CMD_DRAW_QUAD | 2:
			case OP_CMD_DRAW_QUAD | 3:
			case OP_CMD_DRAW_QUAD | 4:
			case OP_CMD_DRAW_QUAD | 5:
			case OP_CMD_DRAW_QUAD | 6:
			case OP_CMD_DRAW_QUAD | 7:
			case OP_CMD_DRAW_TRIANGLE | 0:
			case OP_CMD_DRAW_TRIANGLE | 1:
			case OP_CMD_DRAW_TRIANGLE | 2:
			case OP_CMD_DRAW_TRIANGLE | 3:
			case OP_CMD_DRAW_TRIANGLE | 4:
			case OP_CMD_DRAW_TRIANGLE | 5:
			case OP_CMD_DRAW_TRIANGLE | 6:
			case OP_CMD_DRAW_TRIANGLE | 7:
			case OP_CMD_DRAW_STRIP | 0:
			case OP_CMD_DRAW_STRIP | 1:
			case OP_CMD_DRAW_STRIP | 2:
			case OP_CMD_DRAW_STRIP | 3:
			case OP_CMD_DRAW_STRIP | 4:
			case OP_CMD_DRAW_STRIP | 5:
			case OP_CMD_DRAW_STRIP | 6:
			case OP_CMD_DRAW_STRIP | 7:
			case OP_CMD_DRAW_FAN | 0:
			case OP_CMD_DRAW_FAN | 1:
			case OP_CMD_DRAW_FAN | 2:
			case OP_CMD_DRAW_FAN | 3:
			case OP_CMD_DRAW_FAN | 4:
			case OP_CMD_DRAW_FAN | 5:
			case OP_CMD_DRAW_FAN | 6:
			case OP_CMD_DRAW_FAN | 7:
			case OP_CMD_DRAW_LINE | 0:
			case OP_CMD_DRAW_LINE | 1:
			case OP_CMD_DRAW_LINE | 2:
			case OP_CMD_DRAW_LINE | 3:
			case OP_CMD_DRAW_LINE | 4:
			case OP_CMD_DRAW_LINE | 5:
			case OP_CMD_DRAW_LINE | 6:
			case OP_CMD_DRAW_LINE | 7:
			case OP_CMD_DRAW_LINESTRIP | 0:
			case OP_CMD_DRAW_LINESTRIP | 1:
			case OP_CMD_DRAW_LINESTRIP | 2:
			case OP_CMD_DRAW_LINESTRIP | 3:
			case OP_CMD_DRAW_LINESTRIP | 4:
			case OP_CMD_DRAW_LINESTRIP | 5:
			case OP_CMD_DRAW_LINESTRIP | 6:
			case OP_CMD_DRAW_LINESTRIP | 7:
			case OP_CMD_DRAW_POINT | 0:
			case OP_CMD_DRAW_POINT | 1:
			case OP_CMD_DRAW_POINT | 2:
			case OP_CMD_DRAW_POINT | 3:
			case OP_CMD_DRAW_POINT | 4:
			case OP_CMD_DRAW_POINT | 5:
			case OP_CMD_DRAW_POINT | 6:
			case OP_CMD_DRAW_POINT | 7:
			{
				if (GetSize() < 3)
					return false;

				int vtxnum = Peek16(1);
				return GetSize() >= (vtxnum * VtxSize[cmd & 7] + 3);
			}

			default:
			{
				DBHalt("GX: Unsupported opcode: 0x%02X\n", cmd);
				break;
			}
		}

		return false;
	}

	uint8_t FifoProcessor::Read8()
	{
		assert(GetSize() >= 1);

		uint8_t value = fifo[readPtr++];
		if (readPtr >= fifoSize)
		{
			readPtr = 0;
		}
		return value;
	}

	uint16_t FifoProcessor::Read16()
	{
		assert(GetSize() >= 2);
		return ((uint16_t)Read8() << 8) | Read8();
	}

	uint32_t FifoProcessor::Read32()
	{
		assert(GetSize() >= 4);
		return ((uint32_t)Read8() << 24) | ((uint32_t)Read8() << 16) | ((uint32_t)Read8() << 8) | Read8();
	}

	float FifoProcessor::ReadFloat()
	{
		assert(GetSize() >= 4);
		uint32_t value = Read32();
		return *(float*)&value;
	}

	uint8_t FifoProcessor::Peek8(size_t offset)
	{
		size_t ptr = readPtr + offset;
		if (ptr >= fifoSize)
		{
			ptr -= fifoSize;
		}
		return fifo[ptr];
	}

	uint8_t FifoProcessor::Peek16(size_t offset)
	{
		return ((uint16_t)Peek8(offset) << 8) | Peek8(offset + 1);
	}
}
