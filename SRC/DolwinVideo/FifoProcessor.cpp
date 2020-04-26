#include "pch.h"

extern int VtxSize[8];

namespace GX
{
	FifoProcessor::FifoProcessor()
	{
		fifo = new uint8_t[fifoSize];
		assert(fifo);
		memset(fifo, 0, fifoSize);
		readPtr = 0;
		writePtr = 0;
	}

	FifoProcessor::~FifoProcessor()
	{
		delete[] fifo;
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

        uint8_t cmd = fifo[readPtr];

        switch(cmd)
        {
            case OP_CMD_NOP:
            case OP_CMD_INV:
                return true;

            case OP_CMD_CALL_DL:
                return GetSize() >= 9;

            case OP_CMD_LOAD_BPREG:
                return GetSize() >= 5;

            case OP_CMD_LOAD_CPREG:
                return GetSize() >= 6;
            
            case OP_CMD_LOAD_XFREG:
            {
                if (GetSize() < 3)
                    return false;

                uint16_t len = _byteswap_ushort(*(uint16_t*)(&fifo[readPtr + 1])) + 1;
                return GetSize() >= (len * 4 + 5);
            }

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

                int vtxnum = _byteswap_ushort(*(uint16_t *)(&fifo[readPtr + 1]));
                return GetSize() >= (vtxnum * VtxSize[cmd & 7] + 3);
            }
        }

        return false;
	}

	uint8_t FifoProcessor::Read8()
	{
		assert(GetSize() >= 1);
		return fifo[readPtr++];
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
}
