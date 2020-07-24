#include "pch.h"

using namespace Debug;

namespace Gekko
{
	void GatherBuffer::Reset()
	{
		memset(fifo, 0, sizeof(fifo));
		readPtr = 0;
		writePtr = 0;

		if (log)
		{
			Report(Channel::CPU, "GatherBuffer::Reset");
		}
	}

	size_t GatherBuffer::GatherSize()
	{
		if (writePtr >= readPtr)
		{
			return writePtr - readPtr;
		}
		else
		{
			return (sizeof(fifo) - readPtr) + writePtr;
		}
	}

	void GatherBuffer::WriteBytes(uint8_t* data, size_t size)
	{
		if (log)
		{
			char byteText[10];
			std::string text;

			for (int i = 0; i < size; i++)
			{
				sprintf_s(byteText, sizeof(byteText) - 1, "%02X ", data[i]);
				text += byteText;
			}

			Report(Channel::CPU, "GatherBuffer::WriteBytes: %s", text.c_str());
		}

		if (size < 4)
		{
			for (int i = 0; i < size; i++)
			{
				fifo[writePtr] = data[i];
				writePtr++;
				if (writePtr >= sizeof(fifo))
				{
					writePtr = 0;
				}
			}
		}
		else
		{
			if ((writePtr + size) < sizeof(fifo))
			{
				memcpy(&fifo[writePtr], data, size);
				writePtr += size;
			}
			else
			{
				size_t part1Size = sizeof(fifo) - writePtr;
				memcpy(&fifo[writePtr], data, part1Size);
				writePtr = size - part1Size;
				memcpy(fifo, data + part1Size, writePtr);
			}
		}

		if (GatherSize() >= 32)
		{
			uint8_t burstData[32];

			for (int i = 0; i < sizeof(burstData); i++)
			{
				burstData[i] = fifo[readPtr];
				readPtr++;
				if (readPtr >= sizeof(fifo))
				{
					readPtr = 0;
				}
			}

			if (log)
			{
				Report(Channel::CPU, "Burst gather buffer. Bytes left: %zi\n", GatherSize());
			}

			MIWriteBurst(Gekko->regs.spr[(int)SPR::WPAR] & ~0x1f, burstData);
		}
	}

	void GatherBuffer::Write8(uint8_t value)
	{
		WriteBytes(&value, 1);
	}

	void GatherBuffer::Write16(uint16_t value)
	{
		uint8_t data[2];
		*(uint16_t *)data = _byteswap_ushort(value);
		WriteBytes(data, 2);
	}

	void GatherBuffer::Write32(uint32_t value)
	{
		uint8_t data[4];
		*(uint32_t*)data = _byteswap_ulong(value);
		WriteBytes(data, 4);
	}

	void GatherBuffer::Write64(uint64_t value)
	{
		uint8_t data[8];
		*(uint64_t*)data = _byteswap_uint64(value);
		WriteBytes(data, 8);
	}

	bool GatherBuffer::NotEmpty()
	{
		return readPtr != writePtr;
	}
}
